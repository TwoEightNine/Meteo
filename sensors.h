#include "meteo_config.h"

#if !METEO_DISPLAY_ONLY

#include "Arduino.h"
#include <Wire.h>

// dht11
#include <DHT_U.h>

// bmp280
#include <Adafruit_BMP280.h>

// mh-z19b
#include "MHZ19_uart.h"

// Sensor wiring uses the left-side breadboard pins.
#define PIN_DHT 1

#ifndef RX
#define PIN_MHZ_RX 17
#else
#define PIN_MHZ_RX RX
#endif

#ifndef TX
#define PIN_MHZ_TX 16
#else
#define PIN_MHZ_TX TX
#endif

#define PIN_D18B20 2
#define PIN_I2C_SDA 4
#define PIN_I2C_SCL 5
#define PIN_BATTERY_ADC 0

// sensors configs
#define DHTTYPE DHT11
#define BATTERY_R_TOP 100000.0
#define BATTERY_R_BOTTOM 100000.0
#define BATTERY_EMPTY_MV 3300
#define BATTERY_FULL_MV 4200
#define BATTERY_SAMPLES 8

#define CO2_NONE 0
#define TEMP_EXTERNAL_NONE -60
#define HUMID_NONE 0
#define TEMP_NONE 0

class SensorsProvider {

private:
    DHT_Unified *dht;
    Adafruit_BMP280 *bmp;
    MHZ19_uart *mhz19;

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
    uint8_t readCo2hppm();
    uint16_t readBatteryMilliVolts();
    uint8_t readBatteryPercent(uint16_t milliVolts);
};

#endif
