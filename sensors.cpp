#include "meteo_config.h"

#if !METEO_DISPLAY_ONLY && !METEO_TOUCH_CALIBRATION

#include "Arduino.h"
#include "sensors.h"
#include <math.h>

#ifdef ARDUINO_ARCH_ESP32
#include "driver/gpio.h"
#endif

// Values sampled every 0.5 minutes after the first 3 minutes.
// f(t) = A * (1 - e ^ (-(t - t0) / tau))
// measured at 25 degrees
// first 3 minutes skipped
const float approxDeviation[] = {
    0.00, 0.05, 0.10, 0.14, 0.18, 0.22, 0.26, 0.30, 0.33, 0.36, 0.39, 
    0.42, 0.45, 0.48, 0.50, 0.53, 0.55, 0.57, 0.59, 0.61, 0.63, 0.65, 0.67, 0.68, 0.70, 0.71, 0.73, 0.74, 0.75, 
    0.77, 0.78, 0.79, 0.80, 0.81, 0.82, 0.83, 0.83, 0.84, 0.85, 0.86, 0.86, 0.87, 0.88, 0.88, 0.89, 0.89
};
const size_t approxDeviationCount =
    sizeof(approxDeviation) / sizeof(approxDeviation[0]);
const float PASCALS_PER_MMHG = 133.322387415f;

template <typename T>
bool isWithinInclusive(T value, T minimum, T maximum) {
    return value >= minimum && value <= maximum;
}

SensorsProvider::SensorsProvider() {
    // The DHT11 data pin is idle high.  INPUT_PULLUP also makes an absent
    // sensor deterministic rather than leaving GPIO3 floating.
    pinMode(PIN_DHT, INPUT_PULLUP);

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    bmp = new Adafruit_BMP280();
    bmpReady = bmp->begin(0x76);
    if (!bmpReady) {
        bmpReady = bmp->begin(0x77);
    }
    if (bmpReady) {
        bmp->setSampling(
            Adafruit_BMP280::MODE_NORMAL, // operating mode
            Adafruit_BMP280::SAMPLING_X2, // temperature oversampling
            Adafruit_BMP280::SAMPLING_X16, // pressure oversampling
            Adafruit_BMP280::FILTER_X16, // filtering
            Adafruit_BMP280::STANDBY_MS_500
        );
        Serial.println(F("sensor bmp280: ready"));
    } else {
        Serial.println(F("sensor bmp280: not found"));
    }

    Serial.print(F("sensor ds18b20: pin "));
    Serial.println(PIN_D18B20);
    ds18b20Release();
    delay(2);
    if (ds18b20ReadLevel() == LOW) {
        Serial.println(F("sensor ds18b20: bus low at startup"));
    }

    mhz19 = new MHZ19_uart();
    mhz19->begin(PIN_MHZ_RX, PIN_MHZ_TX);
    mhz19->setAutoCalibration(false);

#ifdef ARDUINO_ARCH_ESP32
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);
#endif
}

void SensorsProvider::setServiceCallback(void (*callback)()) {
    serviceCallback = callback;
}

void SensorsProvider::service() {
    if (serviceCallback != nullptr) {
        serviceCallback();
    }
}

void SensorsProvider::cooperativeDelay(uint32_t durationMs) {
    uint32_t startedAt = millis();
    while ((uint32_t) (millis() - startedAt) < durationMs) {
        service();
        delay(1);
    }
    service();
}

void SensorsProvider::readDht(SensorValue<uint8_t>& humidityResult,
                              SensorValue<int8_t>& internalTemperature) {
    humidityResult.valid = false;
    internalTemperature.valid = false;

    float humidity;
    float temperature;
    if (readDht11(&humidity, &temperature)) {
        float correctedHumidity = getCorrectedHumidity(humidity);
        if (isfinite(correctedHumidity) &&
            isWithinInclusive(
                correctedHumidity,
                SensorOperatingRanges::DHT11_HUMIDITY_MIN_PERCENT,
                SensorOperatingRanges::DHT11_HUMIDITY_MAX_PERCENT)) {
            humidityResult.value = (uint8_t) correctedHumidity;
            humidityResult.valid = true;
        }
    }

    internalTemperature = getCachedTempInternal();
}

void SensorsProvider::readBmp(SensorValue<uint16_t>& pressureMmHgResult,
                              SensorValue<int8_t>& internalTemperature) {
    pressureMmHgResult.valid = false;
    internalTemperature.valid = false;

    bmpLastTemperatureValid = false;
    if (!bmpReady) {
        internalTemperature = getCachedTempInternal();
        return;
    }

    float temperature = bmp->readTemperature();
    if (isfinite(temperature) &&
        isWithinInclusive(
            temperature,
            SensorOperatingRanges::BMP280_TEMPERATURE_MIN_C,
            SensorOperatingRanges::BMP280_TEMPERATURE_MAX_C)) {
        bmpLastTemperature = temperature;
        bmpLastTemperatureValid = true;
    }

    float pressurePa = bmp->readPressure();
    if (isfinite(pressurePa) &&
        isWithinInclusive(
            pressurePa,
            SensorOperatingRanges::BMP280_PRESSURE_MIN_PA,
            SensorOperatingRanges::BMP280_PRESSURE_MAX_PA)) {
        pressureMmHgResult.value =
            (uint16_t) round(pressurePa / PASCALS_PER_MMHG);
        pressureMmHgResult.valid = true;
    }

    internalTemperature = getCachedTempInternal();
}

SensorValue<int8_t> SensorsProvider::getCachedTempInternal() {
    SensorValue<int8_t> result;
    float correctedTemperatureSum = 0;
    uint8_t validTemperatureCount = 0;

    if (dhtLastReadValid) {
        float correctedDhtTemperature =
            getCorrectedTempInternal(dhtLastTemperature);
        if (isfinite(correctedDhtTemperature) &&
            isWithinInclusive(
                correctedDhtTemperature,
                SensorOperatingRanges::DHT11_TEMPERATURE_MIN_C,
                SensorOperatingRanges::DHT11_TEMPERATURE_MAX_C)) {
            correctedTemperatureSum += correctedDhtTemperature;
            validTemperatureCount++;
        }
    }

    if (bmpLastTemperatureValid) {
        float correctedBmpTemperature =
            getCorrectedTempInternal(bmpLastTemperature);
        if (isfinite(correctedBmpTemperature) &&
            isWithinInclusive(
                correctedBmpTemperature,
                SensorOperatingRanges::BMP280_TEMPERATURE_MIN_C,
                SensorOperatingRanges::BMP280_TEMPERATURE_MAX_C)) {
            correctedTemperatureSum += correctedBmpTemperature;
            validTemperatureCount++;
        }
    }

    if (validTemperatureCount == 0) {
        return result;
    }

    result.value =
        (int8_t) (correctedTemperatureSum / validTemperatureCount);
    result.valid = true;
    return result;
}

bool SensorsProvider::waitForDhtLevel(uint8_t level, uint32_t timeoutUs) {
    uint32_t startedAt = micros();
    while (digitalRead(PIN_DHT) != level) {
        if ((uint32_t) (micros() - startedAt) >= timeoutUs) {
            return false;
        }
    }
    return true;
}

bool SensorsProvider::readDht11(float *humidity, float *temperature) {
    const uint32_t DHT_MIN_INTERVAL_MS = 2000;
    const uint32_t DHT_RESPONSE_TIMEOUT_US = 150;
    const uint32_t DHT_BIT_TIMEOUT_US = 120;
    const uint32_t DHT_ONE_PULSE_US = 50;

    uint32_t now = millis();
    if (now - dhtLastAttemptAt < DHT_MIN_INTERVAL_MS) {
        if (dhtLastReadValid) {
            *humidity = dhtLastHumidity;
            *temperature = dhtLastTemperature;
        }
        return dhtLastReadValid;
    }

    // Record the attempt before touching the wire so a missing sensor is not
    // retried immediately if the scheduler calls again too soon.
    dhtLastAttemptAt = now;
    dhtLastReadValid = false;

    // DHT11 start signal: low for at least 18 ms, then release the bus.
    pinMode(PIN_DHT, OUTPUT);
    digitalWrite(PIN_DHT, LOW);
    cooperativeDelay(20);
    digitalWrite(PIN_DHT, HIGH);
    delayMicroseconds(40);
    pinMode(PIN_DHT, INPUT_PULLUP);

    // Sensor response: 80 us low, 80 us high, then the first data low pulse.
    // Unlike the Adafruit DHT implementation, no interrupts are disabled here.
    // An unplugged DHT11 therefore exits after the first bounded timeout.
    if (!waitForDhtLevel(LOW, DHT_RESPONSE_TIMEOUT_US) ||
        !waitForDhtLevel(HIGH, DHT_RESPONSE_TIMEOUT_US) ||
        !waitForDhtLevel(LOW, DHT_RESPONSE_TIMEOUT_US)) {
        return false;
    }

    uint8_t data[5] = {};
    for (uint8_t bitIndex = 0; bitIndex < 40; bitIndex++) {
        if (!waitForDhtLevel(HIGH, DHT_BIT_TIMEOUT_US)) {
            return false;
        }

        uint32_t highStartedAt = micros();
        if (!waitForDhtLevel(LOW, DHT_BIT_TIMEOUT_US)) {
            return false;
        }

        if ((uint32_t) (micros() - highStartedAt) > DHT_ONE_PULSE_US) {
            data[bitIndex / 8] |= 1 << (7 - (bitIndex % 8));
        }
    }

    if (((uint8_t) (data[0] + data[1] + data[2] + data[3])) != data[4]) {
        return false;
    }

    // The second and fourth bytes contain decimal tenths.  The high bit of
    // the temperature decimal byte is the sign bit.
    uint8_t humidityTenths = data[1];
    uint8_t temperatureTenths = data[3] & 0x7f;
    if (humidityTenths > 9 || temperatureTenths > 9) {
        return false;
    }

    float decodedHumidity = data[0] + humidityTenths / 10.0f;
    float decodedTemperature = data[2] + temperatureTenths / 10.0f;
    if (data[3] & 0x80) {
        decodedTemperature = -decodedTemperature;
    }

    if (!isWithinInclusive(
            decodedHumidity,
            SensorOperatingRanges::DHT11_HUMIDITY_MIN_PERCENT,
            SensorOperatingRanges::DHT11_HUMIDITY_MAX_PERCENT) ||
        !isWithinInclusive(
            decodedTemperature,
            SensorOperatingRanges::DHT11_TEMPERATURE_MIN_C,
            SensorOperatingRanges::DHT11_TEMPERATURE_MAX_C)) {
        return false;
    }

    dhtLastHumidity = decodedHumidity;
    dhtLastTemperature = decodedTemperature;
    dhtLastReadValid = true;
    *humidity = dhtLastHumidity;
    *temperature = dhtLastTemperature;
    return true;
}

bool SensorsProvider::startExternalTemperatureRead() {
    if (ds18b20ReadState != Ds18b20ReadState::Idle) {
        return false;
    }

    if (!ds18b20RomLogged) {
        ds18b20LogRom();
        ds18b20RomLogged = true;
    }

    ds18b20ReadAttempt = 1;
    if (!ds18b20StartConversion()) {
        ds18b20ReadState = Ds18b20ReadState::Failure;
    }
    return true;
}

AsyncReadStatus SensorsProvider::pollExternalTemperature(
    SensorValue<int8_t>& temperature) {
    temperature.valid = false;

    if (ds18b20ReadState == Ds18b20ReadState::Idle) {
        return AsyncReadStatus::Idle;
    }
    if (ds18b20ReadState == Ds18b20ReadState::Failure) {
        ds18b20ReadState = Ds18b20ReadState::Idle;
        return AsyncReadStatus::Failure;
    }

    uint32_t now = millis();
    if (!ds18b20DeadlineReached(now)) {
        return AsyncReadStatus::Pending;
    }

    if (ds18b20ReadState == Ds18b20ReadState::WaitingToRetry) {
        ds18b20ReadAttempt++;
        if (!ds18b20StartConversion()) {
            ds18b20ReadState = Ds18b20ReadState::Idle;
            return AsyncReadStatus::Failure;
        }
        return AsyncReadStatus::Pending;
    }

    uint8_t scratchpad[9];
    if (!ds18b20Reset()) {
        Serial.println(F("sensor ds18b20: lost after convert"));
        ds18b20ReadState = Ds18b20ReadState::Idle;
        return AsyncReadStatus::Failure;
    }
    ds18b20WriteByte(0xcc);
    ds18b20WriteByte(0xbe); // Read scratchpad.

    for (uint8_t i = 0; i < 9; i++) {
        scratchpad[i] = ds18b20ReadByte();
    }

    if (ds18b20Crc8(scratchpad, 8) != scratchpad[8]) {
        bool allHigh = true;
        bool allLow = true;
        for (uint8_t i = 0; i < 9; i++) {
            if (scratchpad[i] != 0xff) {
                allHigh = false;
            }
            if (scratchpad[i] != 0x00) {
                allLow = false;
            }
        }

        Serial.print(F("sensor ds18b20: crc error attempt "));
        Serial.print(ds18b20ReadAttempt);
        Serial.print(F(" data"));
        for (uint8_t i = 0; i < 9; i++) {
            Serial.print(' ');
            if (scratchpad[i] < 16) {
                Serial.print('0');
            }
            Serial.print(scratchpad[i], HEX);
        }
        Serial.println();
        if (allHigh) {
            Serial.println(F("sensor ds18b20: bus stayed high while reading scratchpad"));
        } else if (allLow) {
            Serial.println(F("sensor ds18b20: bus stayed low while reading scratchpad"));
        }

        if (ds18b20ReadAttempt < DS18B20_READ_ATTEMPTS) {
            ds18b20Deadline = millis() + DS18B20_RETRY_DELAY_MS;
            ds18b20ReadState = Ds18b20ReadState::WaitingToRetry;
            return AsyncReadStatus::Pending;
        }

        ds18b20ReadState = Ds18b20ReadState::Idle;
        return AsyncReadStatus::Failure;
    }

    int16_t raw = (scratchpad[1] << 8) | scratchpad[0];
    float c = raw / 16.0;

    if (!isWithinInclusive(
            c,
            SensorOperatingRanges::DS18B20_TEMPERATURE_MIN_C,
            SensorOperatingRanges::DS18B20_TEMPERATURE_MAX_C)) {
        Serial.print(F("sensor ds18b20: out of range "));
        Serial.println(c);
        ds18b20ReadState = Ds18b20ReadState::Idle;
        return AsyncReadStatus::Failure;
    }

    float correctedTemperature = getCorrectedTempExternal(c);
    if (!isfinite(correctedTemperature) ||
        !isWithinInclusive(
            correctedTemperature,
            SensorOperatingRanges::DS18B20_TEMPERATURE_MIN_C,
            SensorOperatingRanges::DS18B20_TEMPERATURE_MAX_C)) {
        ds18b20ReadState = Ds18b20ReadState::Idle;
        return AsyncReadStatus::Failure;
    }

    temperature.value = (int8_t) correctedTemperature;
    temperature.valid = true;
    ds18b20ReadState = Ds18b20ReadState::Idle;
    return AsyncReadStatus::Success;
}

bool SensorsProvider::startCo2Read() {
    return mhz19->startCO2Read();
}

AsyncReadStatus SensorsProvider::pollCo2(SensorValue<uint16_t>& ppm) {
    ppm.valid = false;

    int result = 0;
    AsyncReadStatus status = mhz19->pollCO2(result);
    if (status == AsyncReadStatus::Success) {
        if (result < SensorOperatingRanges::MHZ19B_CO2_MIN_PPM ||
            result > SensorOperatingRanges::MHZ19B_CO2_MAX_PPM) {
            return AsyncReadStatus::Failure;
        }

        if (!co2Initialized && result == 500) {
            return AsyncReadStatus::Failure;
        }

        ppm.value = (uint16_t) result;
        ppm.valid = true;
        co2Initialized = true;
    }
    return status;
}

SensorValue<uint16_t> SensorsProvider::readBatteryMilliVolts() {
    SensorValue<uint16_t> result;
    uint32_t sum = 0;
    for (uint8_t i = 0; i < BATTERY_SAMPLES; i++) {
#ifdef ARDUINO_ARCH_ESP32
        sum += analogReadMilliVolts(PIN_BATTERY_ADC);
#else
        sum += (uint32_t) round(analogRead(PIN_BATTERY_ADC) * 3300.0 / 1023.0);
#endif
        cooperativeDelay(2);
    }

    float pinMilliVolts = ((float) sum) / BATTERY_SAMPLES;
    // The ADC reads the midpoint of the 100k / 100k divider, i.e. half the battery voltage.
    uint16_t batteryMilliVolts =
        (uint16_t) round(pinMilliVolts * BATTERY_DIVIDER_RATIO);
    if (batteryMilliVolts > 0) {
        result.value = batteryMilliVolts;
        result.valid = true;
    }
    return result;
}

bool SensorsProvider::ds18b20StartConversion() {
    if (!ds18b20Reset()) {
        Serial.println(F("sensor ds18b20: not found"));
        return false;
    }

    ds18b20WriteByte(0xcc); // Skip ROM: this firmware supports one DS18B20 on the bus.
    ds18b20WriteByte(0x44); // Convert temperature.
    ds18b20Deadline = millis() + DS18B20_CONVERSION_TIME_MS;
    ds18b20ReadState = Ds18b20ReadState::WaitingForConversion;
    return true;
}

bool SensorsProvider::ds18b20DeadlineReached(uint32_t now) const {
    return (int32_t) (now - ds18b20Deadline) >= 0;
}

void SensorsProvider::ds18b20Release() {
#ifdef ARDUINO_ARCH_ESP32
    gpio_set_level((gpio_num_t) PIN_D18B20, 1);
    gpio_set_direction((gpio_num_t) PIN_D18B20, GPIO_MODE_INPUT);
#else
    pinMode(PIN_D18B20, INPUT);
#endif
}

void SensorsProvider::ds18b20DriveLow() {
#ifdef ARDUINO_ARCH_ESP32
    gpio_set_level((gpio_num_t) PIN_D18B20, 0);
    gpio_set_direction((gpio_num_t) PIN_D18B20, GPIO_MODE_OUTPUT_OD);
#else
    digitalWrite(PIN_D18B20, LOW);
    pinMode(PIN_D18B20, OUTPUT);
#endif
}

uint8_t SensorsProvider::ds18b20ReadLevel() {
#ifdef ARDUINO_ARCH_ESP32
    return gpio_get_level((gpio_num_t) PIN_D18B20) ? HIGH : LOW;
#else
    return digitalRead(PIN_D18B20);
#endif
}

void SensorsProvider::ds18b20LogRom() {
    uint8_t rom[8];

    if (!ds18b20Reset()) {
        Serial.println(F("sensor ds18b20 rom: not found"));
        return;
    }

    ds18b20WriteByte(0x33); // Read ROM, valid when only one device is connected.
    for (uint8_t i = 0; i < 8; i++) {
        rom[i] = ds18b20ReadByte();
    }

    Serial.print(F("sensor ds18b20 rom:"));
    for (uint8_t i = 0; i < 8; i++) {
        Serial.print(' ');
        if (rom[i] < 16) {
            Serial.print('0');
        }
        Serial.print(rom[i], HEX);
    }

    if (rom[0] == 0x28 && ds18b20Crc8(rom, 7) == rom[7]) {
        Serial.println(F(" ok"));
    } else {
        Serial.println(F(" invalid"));
    }
}

bool SensorsProvider::ds18b20Reset() {
    ds18b20Release();
    delayMicroseconds(10);
    if (ds18b20ReadLevel() == LOW) {
        ds18b20BusWasPresent = false;
        return false;
    }

    ds18b20DriveLow();
    delayMicroseconds(480);
    ds18b20Release();
    delayMicroseconds(70);
    bool present = ds18b20ReadLevel() == LOW;
    delayMicroseconds(240);
    bool recovered = ds18b20ReadLevel() == HIGH;
    delayMicroseconds(170);
    ds18b20BusWasPresent = present && recovered;
    return ds18b20BusWasPresent;
}

void SensorsProvider::ds18b20WriteBit(uint8_t bit) {
    noInterrupts();
    ds18b20DriveLow();
    if (bit) {
        delayMicroseconds(10);
        ds18b20Release();
        interrupts();
        delayMicroseconds(55);
    } else {
        delayMicroseconds(65);
        ds18b20Release();
        interrupts();
        delayMicroseconds(5);
    }
}

uint8_t SensorsProvider::ds18b20ReadBit() {
    noInterrupts();
    ds18b20DriveLow();
    delayMicroseconds(3);
    ds18b20Release();
    delayMicroseconds(10);
    uint8_t bit = ds18b20ReadLevel();
    interrupts();
    delayMicroseconds(53);
    return bit;
}

void SensorsProvider::ds18b20WriteByte(uint8_t value) {
    for (uint8_t i = 0; i < 8; i++) {
        ds18b20WriteBit(value & 0x01);
        value >>= 1;
    }
}

uint8_t SensorsProvider::ds18b20ReadByte() {
    uint8_t value = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (ds18b20ReadBit()) {
            value |= (1 << i);
        }
    }
    return value;
}

uint8_t SensorsProvider::ds18b20Crc8(uint8_t *data, uint8_t len) {
    uint8_t crc = 0;

    while (len--) {
        uint8_t inByte = *data++;
        for (uint8_t i = 0; i < 8; i++) {
            uint8_t mix = (crc ^ inByte) & 0x01;
            crc >>= 1;
            if (mix) {
                crc ^= 0x8c;
            }
            inByte >>= 1;
        }
    }

    return crc;
}

float SensorsProvider::getCurrentCorrectionBase() {
    uint32_t halfMinutes = millis() / 30000UL;
    if (halfMinutes >= 6 + approxDeviationCount) {
        return 1;
    } else if (halfMinutes >= 6) { // first 3 minutes skipped on purpose
        return approxDeviation[halfMinutes - 6];
    } else {
        return 0;
    }
}

float SensorsProvider::getCorrectedHumidity(float value) {
    // humidity decreases from time, so we add
    return value + getCurrentCorrectionBase() * 3.5;
}

float SensorsProvider::getCorrectedTempInternal(float value) {
    // temperature increases from time, so we subtract
    return value - getCurrentCorrectionBase() * 2.7;
}

float SensorsProvider::getCorrectedTempExternal(float value) {
    uint32_t minutes = millis() / 60000UL;
    float correction = 1;
    if (minutes < 1) {
        correction = 2;
    } else if (minutes < 2) {
        correction = 1.7;
    } else if (minutes < 3) {
        correction = 1.3;
    }
    return value + correction; // here is plus because ds18b20 show lower values first 3 minutes
}

#endif
