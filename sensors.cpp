#include "meteo_config.h"

#if !METEO_DISPLAY_ONLY

#include "Arduino.h"
#include "sensors.h"

// 54 values, 0, 0.5, 1, ... minutes
// f(t) = A * (1 - e ^ (-(t - t0) / tau))
// measured at 25 degrees
// first 3 minutes skipped
float approxDeviation[] = {
    0.00, 0.05, 0.10, 0.14, 0.18, 0.22, 0.26, 0.30, 0.33, 0.36, 0.39, 
    0.42, 0.45, 0.48, 0.50, 0.53, 0.55, 0.57, 0.59, 0.61, 0.63, 0.65, 0.67, 0.68, 0.70, 0.71, 0.73, 0.74, 0.75, 
    0.77, 0.78, 0.79, 0.80, 0.81, 0.82, 0.83, 0.83, 0.84, 0.85, 0.86, 0.86, 0.87, 0.88, 0.88, 0.89, 0.89
};

SensorsProvider::SensorsProvider() {
    dht = new DHT_Unified(PIN_DHT, DHTTYPE);
    dht->begin();

    Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);

    bmp = new Adafruit_BMP280();
    bmp->begin(0x76);
    bmp->setSampling(
        Adafruit_BMP280::MODE_NORMAL, // Режим работы
        Adafruit_BMP280::SAMPLING_X2, // Точность изм. температуры
        Adafruit_BMP280::SAMPLING_X16, // Точность изм. давления
        Adafruit_BMP280::FILTER_X16, // Уровень фильтрации
        Adafruit_BMP280::STANDBY_MS_500
    );

    pinMode(PIN_D18B20, INPUT_PULLUP);

    mhz19 = new MHZ19_uart();
    mhz19->begin(PIN_MHZ_RX, PIN_MHZ_TX);
    mhz19->setAutoCalibration(false);

#ifdef ARDUINO_ARCH_ESP32
    analogReadResolution(12);
    analogSetPinAttenuation(PIN_BATTERY_ADC, ADC_11db);
#endif
}

uint8_t SensorsProvider::readHumidity() {
    sensors_event_t event;
    dht->humidity().getEvent(&event);
    return (uint8_t) getCorrectedHumidity(event.relative_humidity);
}

uint8_t SensorsProvider::readPressureMinus600() {
    return (uint8_t) (round(bmp->readPressure() / 133.3) - 600);
}

int8_t SensorsProvider::readTempInternal() {
    sensors_event_t event;
    dht->temperature().getEvent(&event);
    float temp_dht = event.temperature;
    float temp_bmp = bmp->readTemperature();

    float temp;
    if (temp_dht != 0 && temp_bmp != 0) {
        temp = (temp_dht + temp_bmp) / 2.0;
    } else if (temp_bmp != 0) {
        temp = temp_bmp;
    } else {
        temp = temp_dht;
    }
    return (int8_t) getCorrectedTempInternal(temp);
}

int8_t SensorsProvider::readTempExternal() {
    uint8_t scratchpad[9];

    if (!ds18b20Reset()) {
        return TEMP_EXTERNAL_NONE;
    }
    ds18b20WriteByte(0xcc); // Skip ROM: this firmware supports one DS18B20 on the bus.
    ds18b20WriteByte(0x44); // Convert temperature.
    delay(750);

    if (!ds18b20Reset()) {
        return TEMP_EXTERNAL_NONE;
    }
    ds18b20WriteByte(0xcc);
    ds18b20WriteByte(0xbe); // Read scratchpad.

    for (uint8_t i = 0; i < 9; i++) {
        scratchpad[i] = ds18b20ReadByte();
    }

    if (ds18b20Crc8(scratchpad, 8) != scratchpad[8]) {
        return TEMP_EXTERNAL_NONE;
    }

    int16_t raw = (scratchpad[1] << 8) | scratchpad[0];
    float c = raw / 16.0;

    if (c < -55 || c > 125) {
        return TEMP_EXTERNAL_NONE;
    }

    return (int8_t) getCorrectedTempExternal(c);
}

uint8_t SensorsProvider::readCo2hppm() {
    int ppm = mhz19->getCO2PPM();
    if (ppm > CO2_NONE) {
        return (uint8_t) round(ppm / 100); 
    } else {
        return CO2_NONE;
    }
}

uint16_t SensorsProvider::readBatteryMilliVolts() {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < BATTERY_SAMPLES; i++) {
#ifdef ARDUINO_ARCH_ESP32
        sum += analogReadMilliVolts(PIN_BATTERY_ADC);
#else
        sum += (uint32_t) round(analogRead(PIN_BATTERY_ADC) * 3300.0 / 1023.0);
#endif
        delay(2);
    }

    float pinMilliVolts = ((float) sum) / BATTERY_SAMPLES;
    float dividerRatio = (BATTERY_R_TOP + BATTERY_R_BOTTOM) / BATTERY_R_BOTTOM;
    return (uint16_t) round(pinMilliVolts * dividerRatio);
}

uint8_t SensorsProvider::readBatteryPercent(uint16_t milliVolts) {
    if (milliVolts <= BATTERY_EMPTY_MV) {
        return 0;
    }
    if (milliVolts >= BATTERY_FULL_MV) {
        return 100;
    }

    return (uint8_t) round((milliVolts - BATTERY_EMPTY_MV) * 100.0 / (BATTERY_FULL_MV - BATTERY_EMPTY_MV));
}

bool SensorsProvider::ds18b20Reset() {
    pinMode(PIN_D18B20, OUTPUT);
    digitalWrite(PIN_D18B20, LOW);
    delayMicroseconds(480);
    pinMode(PIN_D18B20, INPUT_PULLUP);
    delayMicroseconds(70);
    bool present = digitalRead(PIN_D18B20) == LOW;
    delayMicroseconds(410);
    return present;
}

void SensorsProvider::ds18b20WriteBit(uint8_t bit) {
    pinMode(PIN_D18B20, OUTPUT);
    digitalWrite(PIN_D18B20, LOW);
    if (bit) {
        delayMicroseconds(6);
        pinMode(PIN_D18B20, INPUT_PULLUP);
        delayMicroseconds(64);
    } else {
        delayMicroseconds(60);
        pinMode(PIN_D18B20, INPUT_PULLUP);
        delayMicroseconds(10);
    }
}

uint8_t SensorsProvider::ds18b20ReadBit() {
    pinMode(PIN_D18B20, OUTPUT);
    digitalWrite(PIN_D18B20, LOW);
    delayMicroseconds(6);
    pinMode(PIN_D18B20, INPUT_PULLUP);
    delayMicroseconds(9);
    uint8_t bit = digitalRead(PIN_D18B20);
    delayMicroseconds(55);
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
        value >>= 1;
        if (ds18b20ReadBit()) {
            value |= 0x80;
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

float SensorsProvider::getCurrentCorrectionBase(float value) {
    uint8_t halfMinutes = (uint8_t) (millis() / 1000 / 30);
    if (halfMinutes >= 54) {
        return 1;
    } else if (halfMinutes >= 6) { // first 3 minutes skipped on purpose
        return approxDeviation[halfMinutes - 6];
    } else {
        return 0;
    }
}

float SensorsProvider::getCorrectedHumidity(float value) {
    // humidity decreases from time, so we add
    return value + getCurrentCorrectionBase(value) * 3.5;
}

float SensorsProvider::getCorrectedTempInternal(float value) {
    // temperature increases from time, so we subtract
    return value - getCurrentCorrectionBase(value) * 2.7;;
}

float SensorsProvider::getCorrectedTempExternal(float value) {
    uint8_t minutes = (uint8_t) (millis() / 1000 / 60);
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
