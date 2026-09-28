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

#define DHT_POLL_INTERVAL_MS 15000
#define BMP_POLL_INTERVAL_MS 15000
#define EXTERNAL_TEMP_NEAR_POLL_INTERVAL_MS 15000
#define EXTERNAL_TEMP_FAR_POLL_INTERVAL_MS 1000
#define EXTERNAL_TEMP_DIFFERENCE_THRESHOLD_C 5
#define CO2_POLL_INTERVAL_MS 60000
#define BATTERY_POLL_INTERVAL_MS 15000
#define SENSOR_POLL_GAP_MS 1000
#define BATTERY_SENSOR_GUARD_MS 2000

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

enum class SensorTask : uint8_t {
    Dht = 0,
    Bmp = 1,
    ExternalTemperature = 2,
    Co2 = 3,
    None = 255
};

#define SENSOR_TASK_COUNT 4

class MainScreen : public Screen {
private:
    SensorsProvider *sensorsProvider;

    MeteoDisplay *tft;

    uint8_t mode = MODE_TEMP_INT;
    const uint8_t sideModes[4] = {MODE_TEMP_EXT, MODE_PRESSURE, MODE_CO2, MODE_HUMIDITY};
    uint8_t quality = QUALITY_WORST;
    uint8_t renderedQuality = 255;
    int16_t renderedBatteryPercent = -2;
    SensorTask activeSensorTask = SensorTask::None;
    uint8_t nextSensorTaskIndex = 0;
    bool sensorPollCompleted[SENSOR_TASK_COUNT] = {};
    uint32_t lastSensorPollAt[SENSOR_TASK_COUNT] = {};
    uint32_t lastSensorActivityAt = 0;
    uint32_t sensorStartAllowedAt = 0;
    uint32_t lastBatteryPollAt = 0;
    bool batteryPollCompleted = false;
    bool batteryQuietWaitLogged = false;

    TextRect valueBounds[5] = {};
    TextRect unitBounds[5] = {};

    Sensors actualSensors = {
        HUMID_NONE,
        TEMP_NONE,
        0,
        TEMP_EXTERNAL_NONE,
        CO2_NONE,
        0
    };

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
    bool deadlineReached(uint32_t now, uint32_t deadline) const;
    bool taskDue(SensorTask task, uint32_t now) const;
    uint32_t taskInterval(SensorTask task) const;
    const __FlashStringHelper *taskName(SensorTask task) const;
    void logTaskEvent(SensorTask task, const __FlashStringHelper *event) const;
    bool qualityInputsReady() const;
    void renderCompletedTask(SensorTask task, const Sensors& previous);
    void completeSensorTask(SensorTask task, const Sensors& previous);
    void startSensorTask(SensorTask task);
    bool pollActiveSensorTask();
    bool pollBatteryIfDue(uint32_t now);
    SensorTask nextDueSensorTask(uint32_t now);
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
