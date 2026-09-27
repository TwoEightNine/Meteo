#ifndef TOUCH_CALIBRATION_H
#define TOUCH_CALIBRATION_H

#include "meteo_config.h"

#if !METEO_DISPLAY_ONLY && METEO_TOUCH_CALIBRATION

#include "meteo_display.h"
#include <XPT2046_Touchscreen.h>

class TouchCalibration {
public:
    TouchCalibration(MeteoDisplay &display, XPT2046_Touchscreen &controller, int16_t minimumPressure);

    void begin();
    void loop();

private:
    static const uint8_t POINT_COUNT = 9;
    static const uint8_t MAX_SAMPLES = 32;

    struct Target {
        uint16_t x;
        uint16_t y;
        int16_t rawX;
        int16_t rawY;
        int16_t pressure;
    };

    struct AxisFit {
        float atZero;
        float atEnd;
    };

    MeteoDisplay &tft;
    XPT2046_Touchscreen &touch;
    int16_t pressureMinimum;
    Target targets[POINT_COUNT];
    uint8_t currentPoint = 0;
    uint8_t sampleCount = 0;
    int32_t rawXSum = 0;
    int32_t rawYSum = 0;
    uint32_t pressureSum = 0;
    uint32_t lastSampleAt = 0;
    uint32_t lastValidTouchAt = 0;
    uint32_t lastResultPrintAt = 0;

    void drawTarget();
    void finishPoint();
    AxisFit fitAxis(bool rawX, bool screenX) const;
    void printResults() const;
};

#endif
#endif
