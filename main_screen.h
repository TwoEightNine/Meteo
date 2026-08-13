#include "screen.h"
#include "sensors.h"

#include <Arduino_GFX_Library.h>

#define SCREEN_BLACK 0x0000
#define SCREEN_WHITE 0xffff

#define MODE_TEMP_INT 0
#define MODE_HUMIDITY 1
#define MODE_CO2      2
#define MODE_TEMP_EXT 3
#define MODE_PRESSURE 4
#define MODES_COUNT   5

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
#define CO2_WARN_MAX 10
#define CO2_URGENT_MAX 20

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
    uint8_t co2hppm; // hecto, 10^2
    uint8_t batteryPercent;
    uint16_t batteryMilliVolts;
};

class MainScreen : public Screen {
private:
    SensorsProvider *sensorsProvider;

    Arduino_GFX *tft;

    uint8_t mode = MODE_TEMP_INT;
    uint8_t quality = 100;
    uint8_t isFirstLaunch = true;
    uint8_t modeChanged = true;

    unsigned long lupdSensors = 0;

    Sensors lastSensors = {};
    Sensors actualSensors = {};

    void drawFrame();
    void updateMainInfo(Sensors& sensors, Sensors& prevSensors, uint8_t forceRender);
    void updateSideInfo(Sensors& sensors, Sensors& prevSensors, uint8_t forceRender);
    void readSensors(Sensors& result);
    void updateQuality();
    void drawTile(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t tileMode, uint8_t forceRender);
    void drawBatteryTile(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t forceRender);
    uint8_t calculateQuality(Sensors& sensors);
    uint8_t getWarningRank(uint16_t warnMin, uint16_t urgentMin, uint16_t warnMax, uint16_t urgentMax, uint16_t value);
    uint8_t getWarningRank(uint16_t warn, uint16_t urgent, uint16_t value);
    void printMm();
    void printDashes();
    void printTemp(int8_t temp, uint16_t mainColor, bool isExternal);
    void printHumidity(uint8_t humidity, uint16_t mainColor);
    void printCo2Hppm(uint8_t co2Hppm, uint16_t mainColor);
    void printPressure(uint8_t pressureMinus600, uint16_t mainColor);
    void printBatteryPercent(uint8_t batteryPercent, uint16_t mainColor);

public:
    MainScreen(SensorsProvider *sensorsProvider, Arduino_GFX *tft);

    void loop(); 
    void onTouch(uint16_t x, uint16_t y);
};
