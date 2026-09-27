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
#define PIN_BATTERY_ADC 7

// sensors configs
#define BATTERY_R_TOP 100000.0
#define BATTERY_R_BOTTOM 100000.0
#define BATTERY_DIVIDER_RATIO ((BATTERY_R_TOP + BATTERY_R_BOTTOM) / BATTERY_R_BOTTOM)
#define BATTERY_SAMPLES 8
#define DS18B20_READ_ATTEMPTS 3

#define CO2_NONE 0
#define TEMP_EXTERNAL_NONE -60
#define HUMID_NONE 0
#define TEMP_NONE 0

class SensorsProvider {

private:
    Adafruit_BMP280 *bmp;
    MHZ19_uart *mhz19;
    bool bmpReady = false;
    bool ds18b20BusWasPresent = false;
    bool ds18b20RomLogged = false;
    uint32_t dhtLastAttemptAt = 0;
    bool dhtLastReadValid = false;
    float dhtLastHumidity = 0;
    float dhtLastTemperature = 0;

    bool readDht11(float *humidity, float *temperature);
    bool waitForDhtLevel(uint8_t level, uint32_t timeoutUs);
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
    float getCorrectedHumidity(float value);
    float getCurrentCorrectionBase(float value);
    float getCorrectedTempInternal(float value);
    float getCorrectedTempExternal(float value);

public:
    SensorsProvider();
    uint8_t readHumidity();
    int8_t readTempInternal();
    uint8_t readPressureMinus600();
    int8_t readTempExternal();
    uint16_t readCo2ppm();
    uint16_t readBatteryMilliVolts();
};

#endif
