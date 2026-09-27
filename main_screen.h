#include "meteo_config.h"

#if !METEO_DISPLAY_ONLY && !METEO_TOUCH_CALIBRATION

#include "screen.h"
#include "sensors.h"

#include "meteo_display.h"


#define MODE_TEMP_INT 0
#define MODE_HUMIDITY 1
#define MODE_CO2      2
#define MODE_TEMP_EXT 3
#define MODE_PRESSURE 4
#define MODES_COUNT   5

#define SENSOR_POLL_INTERVAL_MS 5000
#define CO2_POLL_INTERVAL_MS 15000
#define BATTERY_POLL_INTERVAL_MS 1000

#define HUM_WARN_MIN 40
#define HUM_WARN_MAX 60
#define HUM_URGENT_MIN 30
#define HUM_URGENT_MAX 70
// normal 750 samara
// normal 720 tbilisi
#define PRES_WARN_MIN 90 // normal - 600 - 30
#define PRES_URGENT_MIN 70 // normal - 600 - 50
#define PRES_WARN_MAX 150 // normal - 600 + 30
#define PRES_URGENT_MAX 170 // normal - 600 + 50
#define CO2_WARN_MIN 0
#define CO2_URGENT_MIN 0
#define CO2_WARN_MAX 1000
#define CO2_URGENT_MAX 2000

#define QUALITY_BEST 4
#define QUALITY_GOOD 3
#define QUALITY_AVG 2
#define QUALITY_BAD 1
#define QUALITY_WORST 0

struct Sensors {
    uint8_t humidity;
    int8_t temperatureInternal;
    uint8_t pressureMinus600; // extra over 600
    int8_t temperatureExternal;
    uint16_t co2ppm;
    uint16_t batteryMilliVolts;
};

struct TextRect {
    int16_t x = 0;
    int16_t y = 0;
    uint16_t w = 0;
    uint16_t h = 0;
};

class MainScreen : public Screen {
private:
    SensorsProvider *sensorsProvider;

    MeteoDisplay *tft;

    uint8_t mode = MODE_TEMP_INT;
    const uint8_t sideModes[4] = {MODE_TEMP_EXT, MODE_PRESSURE, MODE_CO2, MODE_HUMIDITY};
    uint8_t quality = QUALITY_WORST;
    uint8_t renderedQuality = 255;
    int16_t renderedBatteryPercent = -1;
    bool isFirstLaunch = true;

    TextRect valueBounds[5] = {};
    TextRect unitBounds[5] = {};

    unsigned long lastSensorsPoll = 0;
    unsigned long lastCo2Poll = 0;
    unsigned long lastBatteryPoll = 0;

    Sensors actualSensors = {};

    void drawFrame();
    void drawFocusPanel(bool fullPanel);
    void drawSidePanel(uint8_t row, bool fullPanel);
    void drawValue(uint8_t slot, uint8_t sensorMode, bool focus);
    TextRect drawSmoothValue(const char *number, uint16_t color);
    void drawLabel(uint8_t sensorMode, int16_t x, int16_t y, bool focus);
    void drawBattery();
    void clearText(TextRect& bounds);
    uint16_t unitWidth(uint8_t sensorMode, bool focus);
    TextRect drawUnit(uint8_t sensorMode, int16_t x, int16_t y, bool focus);
    void formatValue(uint8_t sensorMode, char *buffer, size_t bufferSize);
    bool sensorChanged(uint8_t sensorMode, const Sensors& previous) const;
    void readSensors(Sensors& result, bool readStandardSensors, bool readCo2, bool readBattery);
    void updateQuality();
    uint8_t calculateQuality(Sensors& sensors);
    uint8_t getWarningRank(uint16_t warnMin, uint16_t urgentMin, uint16_t warnMax, uint16_t urgentMax, uint16_t value);
    uint8_t getWarningRank(uint16_t warn, uint16_t urgent, uint16_t value);

public:
    MainScreen(SensorsProvider *sensorsProvider, MeteoDisplay *tft);

    void loop(); 
    void onTouch(uint16_t x, uint16_t y);
};

#endif
