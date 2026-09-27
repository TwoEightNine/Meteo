#include "touch_calibration.h"

#if !METEO_DISPLAY_ONLY && METEO_TOUCH_CALIBRATION

#include <math.h>

static const char *const POINT_NAMES[] = {
    "TOP LEFT", "TOP RIGHT", "BOTTOM RIGHT", "BOTTOM LEFT",
    "TOP EDGE", "RIGHT EDGE", "BOTTOM EDGE", "LEFT EDGE", "CENTER"
};

TouchCalibration::TouchCalibration(MeteoDisplay &display, XPT2046_Touchscreen &controller,
                                   int16_t minimumPressure)
    : tft(display), touch(controller), pressureMinimum(minimumPressure) {
}

void TouchCalibration::begin() {
    const uint16_t left = 30;
    const uint16_t right = tft.width() - 31;
    const uint16_t top = 30;
    const uint16_t bottom = tft.height() - 31;
    const uint16_t middleX = (tft.width() - 1) / 2;
    const uint16_t middleY = (tft.height() - 1) / 2;

    targets[0] = {left, top, 0, 0, 0};
    targets[1] = {right, top, 0, 0, 0};
    targets[2] = {right, bottom, 0, 0, 0};
    targets[3] = {left, bottom, 0, 0, 0};
    targets[4] = {middleX, top, 0, 0, 0};
    targets[5] = {right, middleY, 0, 0, 0};
    targets[6] = {middleX, bottom, 0, 0, 0};
    targets[7] = {left, middleY, 0, 0, 0};
    targets[8] = {middleX, middleY, 0, 0, 0};

    Serial.println(F("calibration: touch.setRotation(1), display rotation 1"));
    Serial.println(F("calibration: tap each crosshair and release; hold briefly for samples"));
    drawTarget();
}

void TouchCalibration::drawTarget() {
    const Target &target = targets[currentPoint];
    tft.fillScreen(SCREEN_BLACK);
    tft.setTextColor(SCREEN_WHITE);
    tft.setTextSize(2);
    tft.setCursor(8, 86);
    tft.print(F("CALIBRATION "));
    tft.print(currentPoint + 1);
    tft.print(F("/9: "));
    tft.print(POINT_NAMES[currentPoint]);
    tft.setCursor(8, 113);
    tft.print(F("Tap crosshair, then release"));

    tft.drawCircle(target.x, target.y, 14, 0x07ff);
    tft.drawFastHLine(target.x - 23, target.y, 47, 0x07ff);
    tft.drawFastVLine(target.x, target.y - 23, 47, 0x07ff);
    tft.fillCircle(target.x, target.y, 3, SCREEN_WHITE);

    Serial.print(F("calibration: point "));
    Serial.print(currentPoint + 1);
    Serial.print(F("/9 "));
    Serial.print(POINT_NAMES[currentPoint]);
    Serial.print(F(" target=("));
    Serial.print(target.x);
    Serial.print(',');
    Serial.print(target.y);
    Serial.println(F(")"));
}

void TouchCalibration::loop() {
    if (currentPoint >= POINT_COUNT) {
        if (Serial.available()) {
            const char command = Serial.read();
            if (command == 'p' || command == 'P') {
                printResults();
                lastResultPrintAt = millis();
            }
        }
        if (millis() - lastResultPrintAt >= 15000) {
            printResults();
            lastResultPrintAt = millis();
        }
        return;
    }

    const uint32_t now = millis();
    if (touch.touched()) {
        const TS_Point point = touch.getPoint();
        if (point.z >= pressureMinimum) {
            lastValidTouchAt = now;
            if (sampleCount < MAX_SAMPLES &&
                (sampleCount == 0 || now - lastSampleAt >= 10)) {
                rawXSum += point.x;
                rawYSum += point.y;
                pressureSum += point.z;
                ++sampleCount;
                lastSampleAt = now;
            }
            return;
        }
    }

    // A short gap absorbs noisy release readings from the resistive panel.
    if (sampleCount > 0 && now - lastValidTouchAt >= 80) {
        if (sampleCount >= 3) {
            finishPoint();
        } else {
            Serial.println(F("calibration: touch too short; retry this point"));
            drawTarget();
        }
        sampleCount = 0;
        rawXSum = 0;
        rawYSum = 0;
        pressureSum = 0;
    }
}

void TouchCalibration::finishPoint() {
    Target &target = targets[currentPoint];
    target.rawX = (rawXSum + sampleCount / 2) / sampleCount;
    target.rawY = (rawYSum + sampleCount / 2) / sampleCount;
    target.pressure = (pressureSum + sampleCount / 2) / sampleCount;

    Serial.print(F("calibration: captured "));
    Serial.print(POINT_NAMES[currentPoint]);
    Serial.print(F(" screen=("));
    Serial.print(target.x);
    Serial.print(',');
    Serial.print(target.y);
    Serial.print(F(") raw=("));
    Serial.print(target.rawX);
    Serial.print(',');
    Serial.print(target.rawY);
    Serial.print(F(") z="));
    Serial.print(target.pressure);
    Serial.print(F(" samples="));
    Serial.println(sampleCount);

    ++currentPoint;
    if (currentPoint < POINT_COUNT) {
        drawTarget();
        return;
    }

    tft.fillScreen(SCREEN_BLACK);
    tft.setTextColor(SCREEN_WHITE);
    tft.setTextSize(2);
    tft.setCursor(34, 110);
    tft.println(F("CALIBRATION COMPLETE"));
    tft.setCursor(34, 145);
    tft.println(F("See Serial Monitor"));
    tft.setCursor(34, 180);
    tft.println(F("Send P to print again"));
    printResults();
    lastResultPrintAt = millis();
}

TouchCalibration::AxisFit TouchCalibration::fitAxis(bool rawX, bool screenX) const {
    float screenSum = 0;
    float screenSquareSum = 0;
    float rawSum = 0;
    float productSum = 0;

    for (uint8_t i = 0; i < POINT_COUNT; ++i) {
        const float screen = screenX ? targets[i].x : targets[i].y;
        const float raw = rawX ? targets[i].rawX : targets[i].rawY;
        screenSum += screen;
        screenSquareSum += screen * screen;
        rawSum += raw;
        productSum += screen * raw;
    }

    const float n = POINT_COUNT;
    const float slope = (n * productSum - screenSum * rawSum) /
                        (n * screenSquareSum - screenSum * screenSum);
    const float intercept = (rawSum - slope * screenSum) / n;
    const float screenEnd = (screenX ? tft.width() : tft.height()) - 1;
    return {intercept, intercept + slope * screenEnd};
}

void TouchCalibration::printResults() const {
    const AxisFit rawXOverX = fitAxis(true, true);
    const AxisFit rawXOverY = fitAxis(true, false);
    const AxisFit rawYOverX = fitAxis(false, true);
    const AxisFit rawYOverY = fitAxis(false, false);

    const float normalSpan = fabsf(rawXOverX.atEnd - rawXOverX.atZero) +
                             fabsf(rawYOverY.atEnd - rawYOverY.atZero);
    const float swappedSpan = fabsf(rawYOverX.atEnd - rawYOverX.atZero) +
                              fabsf(rawXOverY.atEnd - rawXOverY.atZero);
    const bool swap = swappedSpan > normalSpan;
    const AxisFit xFit = swap ? rawYOverX : rawXOverX;
    const AxisFit yFit = swap ? rawXOverY : rawYOverY;
    const float xSpan = fabsf(xFit.atEnd - xFit.atZero);
    const float ySpan = fabsf(yFit.atEnd - yFit.atZero);

    Serial.println();
    Serial.println(F("=================================================="));
    Serial.println(F("       TOUCH CALIBRATION RESULTS - 9 POINTS"));
    Serial.println(F("=================================================="));
    Serial.println(F("Target, screen X, screen Y, raw X, raw Y, pressure"));
    for (uint8_t i = 0; i < POINT_COUNT; ++i) {
        Serial.print(POINT_NAMES[i]); Serial.print(',');
        Serial.print(targets[i].x); Serial.print(',');
        Serial.print(targets[i].y); Serial.print(',');
        Serial.print(targets[i].rawX); Serial.print(',');
        Serial.print(targets[i].rawY); Serial.print(',');
        Serial.println(targets[i].pressure);
    }
    Serial.println(F("--------------------------------------------------"));
    if (xSpan < 200 || ySpan < 200) {
        Serial.println(F("CALIBRATION FAILED: raw range too small."));
        Serial.println(F("Check touch wiring and pressure threshold, then reset and retry."));
        Serial.println(F("=========== END TOUCH CALIBRATION RESULTS ========"));
        Serial.flush();
        return;
    }

    const int16_t xLow = constrain((int32_t)lroundf(fminf(xFit.atZero, xFit.atEnd)), 0, 4095);
    const int16_t xHigh = constrain((int32_t)lroundf(fmaxf(xFit.atZero, xFit.atEnd)), 0, 4095);
    const int16_t yLow = constrain((int32_t)lroundf(fminf(yFit.atZero, yFit.atEnd)), 0, 4095);
    const int16_t yHigh = constrain((int32_t)lroundf(fmaxf(yFit.atZero, yFit.atEnd)), 0, 4095);

    Serial.println(F("COPY THE FOLLOWING 7 LINES INTO meteo_touch.ino:"));
    Serial.println(F("========== BEGIN TOUCH SETTINGS TO COPY =========="));
    Serial.print(F("#define TOUCH_MIN_X ")); Serial.println(xLow);
    Serial.print(F("#define TOUCH_MAX_X ")); Serial.println(xHigh);
    Serial.print(F("#define TOUCH_MIN_Y ")); Serial.println(yLow);
    Serial.print(F("#define TOUCH_MAX_Y ")); Serial.println(yHigh);
    Serial.print(F("#define TOUCH_SWAP_XY ")); Serial.println(swap ? F("true") : F("false"));
    Serial.print(F("#define TOUCH_INVERT_X ")); Serial.println(xFit.atEnd < xFit.atZero ? F("true") : F("false"));
    Serial.print(F("#define TOUCH_INVERT_Y ")); Serial.println(yFit.atEnd < yFit.atZero ? F("true") : F("false"));
    Serial.println(F("=========== END TOUCH SETTINGS TO COPY ==========="));

    float worstError = 0;
    for (uint8_t i = 0; i < POINT_COUNT; ++i) {
        const float rawForX = swap ? targets[i].rawY : targets[i].rawX;
        const float rawForY = swap ? targets[i].rawX : targets[i].rawY;
        const float mappedX = (rawForX - xFit.atZero) * (tft.width() - 1) /
                              (xFit.atEnd - xFit.atZero);
        const float mappedY = (rawForY - yFit.atZero) * (tft.height() - 1) /
                              (yFit.atEnd - yFit.atZero);
        worstError = fmaxf(worstError, fabsf(mappedX - targets[i].x));
        worstError = fmaxf(worstError, fabsf(mappedY - targets[i].y));
    }
    Serial.print(F("Largest fit error across 9 points: "));
    Serial.print(worstError, 1);
    Serial.println(F(" pixels"));
    Serial.println(F("After copying, set METEO_TOUCH_CALIBRATION to 0 for the sensor UI."));
    Serial.println(F("Send P in Serial Monitor to print this report again."));
    Serial.println(F("=========== END TOUCH CALIBRATION RESULTS ========"));
    Serial.flush();
}

#endif
