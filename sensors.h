#include "Arduino.h"
#include <Wire.h>

// dht11
#include <DHT_U.h>

// bmp280
#include <Adafruit_BMP280.h>

// d18b20
#include <OneWire.h>
#include <DallasTemperature.h>

// mh-z19b
#include "MHZ19_uart.h"

// pins
#define PIN_DHT 1
#define PIN_MHZ_RX 16
#define PIN_MHZ_TX 17
#define PIN_D18B20 2
#define PIN_I2C_SDA 23
#define PIN_I2C_SCL 22
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
    OneWire *oneWire;
    DallasTemperature *dallasTemp;
    MHZ19_uart *mhz19;

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
