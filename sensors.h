#include "meteo_config.h"

#if !METEO_DISPLAY_ONLY && !METEO_TOUCH_CALIBRATION

#include "Arduino.h"
#include <Wire.h>

// bmp280
#include <Adafruit_BMP280.h>

// mh-z19b
#include "MHZ19_uart.h"

// Sensor wiring uses the left-side breadboard pins.
#define PIN_I2C_SCL 1
#define PIN_I2C_SDA 2
#define PIN_DHT 3
#define PIN_MHZ_RX 4
#define PIN_MHZ_TX 5
#define PIN_D18B20 6
#define PIN_BATTERY_ADC 0

// sensors configs
#define BATTERY_R_TOP 100000.0
#define BATTERY_R_BOTTOM 100000.0
#define BATTERY_DIVIDER_RATIO ((BATTERY_R_TOP + BATTERY_R_BOTTOM) / BATTERY_R_BOTTOM)
#define BATTERY_SAMPLES 8
#define DS18B20_READ_ATTEMPTS 3
#define DS18B20_CONVERSION_TIME_MS 750
#define DS18B20_RETRY_DELAY_MS 20

// Inclusive measurement limits from the installed sensor variants' datasheets.
// Names include units so acquisition code cannot silently mix representations.
namespace SensorOperatingRanges {
constexpr float DHT11_HUMIDITY_MIN_PERCENT = 20.0f;
constexpr float DHT11_HUMIDITY_MAX_PERCENT = 90.0f;
constexpr float DHT11_TEMPERATURE_MIN_C = 0.0f;
constexpr float DHT11_TEMPERATURE_MAX_C = 50.0f;

constexpr float BMP280_TEMPERATURE_MIN_C = -40.0f;
constexpr float BMP280_TEMPERATURE_MAX_C = 85.0f;
constexpr float BMP280_PRESSURE_MIN_PA = 30000.0f;
constexpr float BMP280_PRESSURE_MAX_PA = 110000.0f;

constexpr float DS18B20_TEMPERATURE_MIN_C = -55.0f;
constexpr float DS18B20_TEMPERATURE_MAX_C = 125.0f;

constexpr uint16_t MHZ19B_CO2_MIN_PPM = 400;
constexpr uint16_t MHZ19B_CO2_MAX_PPM = 5000;
}

template <typename T>
struct SensorValue {
    T value = {};
    bool valid = false;
};

class SensorsProvider {

private:
    enum class Ds18b20ReadState : uint8_t {
        Idle,
        WaitingForConversion,
        WaitingToRetry,
        Failure
    };

    Adafruit_BMP280 *bmp;
    MHZ19_uart *mhz19;
    bool bmpReady = false;
    bool co2Initialized = false;
    bool ds18b20BusWasPresent = false;
    bool ds18b20RomLogged = false;
    uint32_t dhtLastAttemptAt = 0;
    bool dhtLastReadValid = false;
    float dhtLastHumidity = 0;
    float dhtLastTemperature = 0;
    bool bmpLastTemperatureValid = false;
    float bmpLastTemperature = 0;
    Ds18b20ReadState ds18b20ReadState = Ds18b20ReadState::Idle;
    uint8_t ds18b20ReadAttempt = 0;
    uint32_t ds18b20Deadline = 0;
    // Called only from waits where sensor timing permits interruption.
    void (*serviceCallback)() = nullptr;

    bool readDht11(float *humidity, float *temperature);
    bool waitForDhtLevel(uint8_t level, uint32_t timeoutUs);
    void service();
    void cooperativeDelay(uint32_t durationMs);
    void ds18b20DriveLow();
    void ds18b20Release();
    uint8_t ds18b20ReadLevel();
    void ds18b20LogRom();
    bool ds18b20Reset();
    void ds18b20WriteBit(uint8_t bit);
    uint8_t ds18b20ReadBit();
    void ds18b20WriteByte(uint8_t value);
    uint8_t ds18b20ReadByte();
    uint8_t ds18b20Crc8(uint8_t *data, uint8_t len);
    bool ds18b20StartConversion();
    bool ds18b20DeadlineReached(uint32_t now) const;
    float getCorrectedHumidity(float value);
    float getCurrentCorrectionBase();
    float getCorrectedTempInternal(float value);
    float getCorrectedTempExternal(float value);
    SensorValue<int8_t> getCachedTempInternal();

public:
    SensorsProvider();
    void setServiceCallback(void (*callback)());
    void readDht(SensorValue<uint8_t>& humidity,
                 SensorValue<int8_t>& internalTemperature);
    void readBmp(SensorValue<uint16_t>& pressureMmHg,
                 SensorValue<int8_t>& internalTemperature);
    bool startExternalTemperatureRead();
    AsyncReadStatus pollExternalTemperature(SensorValue<int8_t>& temperature);
    bool startCo2Read();
    AsyncReadStatus pollCo2(SensorValue<uint16_t>& ppm);
    SensorValue<uint16_t> readBatteryMilliVolts();
};

#endif
