#include "meteo_config.h"

#if !METEO_DISPLAY_ONLY

#include "main_screen.h"

#define GRAY 0x8c71
#define LIGHT_BLUE 0x975f
#define SELECTED_BG 0x0841

#define SCREEN_W 480
#define SCREEN_H 320
#define TOP_BAR_H 64
#define BOTTOM_BAR_Y 256
#define BOTTOM_BAR_H 64
#define TOP_TILE_W 160
#define BOTTOM_TILE_W 120

static const __FlashStringHelper *modeLabel(uint8_t value) {
    switch (value) {
        case MODE_TEMP_INT: return F("temp-int");
        case MODE_HUMIDITY: return F("humidity");
        case MODE_CO2: return F("co2");
        case MODE_TEMP_EXT: return F("temp-ext");
        case MODE_PRESSURE: return F("pressure");
        default: return F("unknown");
    }
}

MainScreen::MainScreen(SensorsProvider *sensorsProvider, MeteoDisplay *tft) {
    this->sensorsProvider = sensorsProvider;
    this->tft = tft;
    drawFrame();
}

void MainScreen::drawFrame() {
    tft->fillScreen(SCREEN_BLACK);
    tft->drawFastHLine(0, 0, SCREEN_W, SCREEN_WHITE);
    tft->drawFastHLine(0, TOP_BAR_H, SCREEN_W, SCREEN_WHITE);
    tft->drawFastHLine(0, BOTTOM_BAR_Y, SCREEN_W, SCREEN_WHITE);
    tft->drawFastHLine(0, SCREEN_H - 1, SCREEN_W, SCREEN_WHITE);
    tft->drawFastVLine(0, 0, SCREEN_H, SCREEN_WHITE);
    tft->drawFastVLine(SCREEN_W - 1, 0, SCREEN_H, SCREEN_WHITE);

    tft->drawFastVLine(TOP_TILE_W, 1, TOP_BAR_H - 1, GRAY);
    tft->drawFastVLine(TOP_TILE_W * 2, 1, TOP_BAR_H - 1, GRAY);
    tft->drawFastVLine(BOTTOM_TILE_W, BOTTOM_BAR_Y + 1, BOTTOM_BAR_H - 2, GRAY);
    tft->drawFastVLine(BOTTOM_TILE_W * 2, BOTTOM_BAR_Y + 1, BOTTOM_BAR_H - 2, GRAY);
    tft->drawFastVLine(BOTTOM_TILE_W * 3, BOTTOM_BAR_Y + 1, BOTTOM_BAR_H - 2, GRAY);
}

void MainScreen::readSensors(Sensors& result) {
    Serial.println(F("sensors read"));
    Serial.print(F("secs: "));
    Serial.println(millis() / 1000);
 
    result.humidity = this->sensorsProvider->readHumidity();
    result.temperatureInternal = this->sensorsProvider->readTempInternal();
    result.pressureMinus600 = this->sensorsProvider->readPressureMinus600();
    result.temperatureExternal = this->sensorsProvider->readTempExternal();
    result.co2hppm = this->sensorsProvider->readCo2hppm();
    result.batteryMilliVolts = this->sensorsProvider->readBatteryMilliVolts();

    Serial.print(F("hum: "));
    Serial.print(result.humidity);
    Serial.print(F(" ti: "));
    Serial.print(result.temperatureInternal);
    Serial.print(F(" p: "));
    if (result.pressureMinus600 == 0) {
        Serial.print(F("---"));
    } else {
        Serial.print(result.pressureMinus600 + 600);
    }
    Serial.print(F(" te: "));
    Serial.print(result.temperatureExternal);
    Serial.print(F(" co2hppm: "));
    Serial.print(result.co2hppm);
    Serial.print(F(" bat: "));
    Serial.print(result.batteryMilliVolts);
    Serial.println(F("mV"));
}

void MainScreen::loop() {
    bool sensorsUpdated = false;

    if (millis() - lupdSensors >= 5000 || isFirstLaunch) {
        
        lastSensors.humidity = actualSensors.humidity;
        lastSensors.temperatureInternal = actualSensors.temperatureInternal;
        lastSensors.pressureMinus600 = actualSensors.pressureMinus600;
        lastSensors.temperatureExternal = actualSensors.temperatureExternal;
        lastSensors.co2hppm = actualSensors.co2hppm;
        lastSensors.batteryMilliVolts = actualSensors.batteryMilliVolts;

        readSensors(actualSensors);
        lupdSensors = millis();

        updateSideInfo(actualSensors, lastSensors, isFirstLaunch || modeChanged);
        quality = calculateQuality(actualSensors);
        updateQuality();
        sensorsUpdated = true;
    }

    if (modeChanged || sensorsUpdated) {
        updateMainInfo(actualSensors, lastSensors, modeChanged);
    }

    if (modeChanged) {
        updateSideInfo(actualSensors, lastSensors, true);
        modeChanged = false;
    }
    isFirstLaunch = false;
    delay(50);
}

void MainScreen::onTouch(uint16_t x, uint16_t y) {
    uint8_t nextMode = mode;
    bool metricTapped = false;

    if (y < TOP_BAR_H) {
        if (x < TOP_TILE_W) {
            nextMode = MODE_TEMP_INT;
            metricTapped = true;
        } else if (x < TOP_TILE_W * 2) {
            nextMode = MODE_HUMIDITY;
            metricTapped = true;
        } else {
            nextMode = MODE_CO2;
            metricTapped = true;
        }
    } else if (y >= BOTTOM_BAR_Y) {
        if (x < BOTTOM_TILE_W) {
            nextMode = MODE_TEMP_EXT;
            metricTapped = true;
        } else if (x < BOTTOM_TILE_W * 2) {
            nextMode = MODE_PRESSURE;
            metricTapped = true;
        }
    }

    if (nextMode != mode) {
        mode = nextMode;
        modeChanged = true;
        Serial.print(F("touch: event=select mode="));
        Serial.println(modeLabel(mode));
    } else if (metricTapped) {
        Serial.println(F("touch: event=already-selected"));
    } else {
        Serial.println(F("touch: event=none"));
    }
}

void MainScreen::updateMainInfo(Sensors& sensors, Sensors& prevSensors, uint8_t forceRender) {
    bool isChanged = false;

    if (!forceRender) {
        if (mode == MODE_TEMP_INT) {
            isChanged = sensors.temperatureInternal != prevSensors.temperatureInternal;
        } else if (mode == MODE_HUMIDITY) {
            isChanged = sensors.humidity != prevSensors.humidity;
        } else if (mode == MODE_PRESSURE) {
            isChanged = sensors.pressureMinus600 != prevSensors.pressureMinus600;
        } else if (mode == MODE_TEMP_EXT) {
            isChanged = sensors.temperatureExternal != prevSensors.temperatureExternal;
        } else if (mode == MODE_CO2) {
            isChanged = sensors.co2hppm != prevSensors.co2hppm;
        }
    }

    if (isChanged || forceRender) {
        tft->fillRect(1, TOP_BAR_H + 1, SCREEN_W - 2, BOTTOM_BAR_Y - TOP_BAR_H - 1, SCREEN_BLACK);
        tft->setTextSize(12);
        
        if (mode == MODE_TEMP_INT) {
            tft->setCursor(120, 120);
            printTemp(sensors.temperatureInternal, SCREEN_WHITE, false);
        } else if (mode == MODE_HUMIDITY) {
            tft->setCursor(145, 120);
            printHumidity(sensors.humidity, SCREEN_WHITE);
        } else if (mode == MODE_PRESSURE) {
            tft->setCursor(95, 120);
            printPressure(sensors.pressureMinus600, SCREEN_WHITE);
        } else if (mode == MODE_CO2) {
            tft->setCursor(145, 120);
            printCo2Hppm(sensors.co2hppm, SCREEN_WHITE);
        } else if (mode == MODE_TEMP_EXT) {
            tft->setCursor(110, 120);
            printTemp(sensors.temperatureExternal, SCREEN_WHITE, true);
        }

        tft->setTextSize(4);
        tft->setCursor(330, 175);
        tft->setTextColor(GRAY);
        if (mode == MODE_TEMP_INT || mode == MODE_TEMP_EXT) {
            tft->print('C');
        } else if (mode == MODE_HUMIDITY) {
            tft->print('%');
        } else if (mode == MODE_PRESSURE) {
            printMm();
        } else if (mode == MODE_CO2) {
            tft->print('h');
            tft->print('p');
        }
    }
}

void MainScreen::updateSideInfo(Sensors& sensors, Sensors& prevSensors, uint8_t forceRender) {
    if (prevSensors.temperatureInternal != sensors.temperatureInternal || forceRender) {
        drawTile(1, 1, TOP_TILE_W - 1, TOP_BAR_H - 1, MODE_TEMP_INT, forceRender);
    }
    if (prevSensors.humidity != sensors.humidity || forceRender) {
        drawTile(TOP_TILE_W + 1, 1, TOP_TILE_W - 1, TOP_BAR_H - 1, MODE_HUMIDITY, forceRender);
    }
    if (prevSensors.co2hppm != sensors.co2hppm || forceRender) {
        drawTile(TOP_TILE_W * 2 + 1, 1, TOP_TILE_W - 2, TOP_BAR_H - 1, MODE_CO2, forceRender);
    }
    if (prevSensors.temperatureExternal != sensors.temperatureExternal || forceRender) {
        drawTile(1, BOTTOM_BAR_Y + 1, BOTTOM_TILE_W - 1, BOTTOM_BAR_H - 2, MODE_TEMP_EXT, forceRender);
    }
    if (prevSensors.pressureMinus600 != sensors.pressureMinus600 || forceRender) {
        drawTile(BOTTOM_TILE_W + 1, BOTTOM_BAR_Y + 1, BOTTOM_TILE_W - 1, BOTTOM_BAR_H - 2, MODE_PRESSURE, forceRender);
    }
    if (prevSensors.batteryMilliVolts != sensors.batteryMilliVolts || forceRender) {
        drawBatteryTile(BOTTOM_TILE_W * 2 + 1, BOTTOM_BAR_Y + 1, BOTTOM_TILE_W - 1, BOTTOM_BAR_H - 2, forceRender);
    }
}

void MainScreen::drawTile(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t tileMode, uint8_t forceRender) {
    uint16_t mainColor = tileMode == mode ? LIGHT_BLUE : SCREEN_WHITE;
    uint16_t bgColor = tileMode == mode ? SELECTED_BG : SCREEN_BLACK;

    tft->fillRect(x, y, w, h, bgColor);
    tft->setTextSize(3);
    tft->setCursor(x + 14, y + 21);

    if (tileMode == MODE_TEMP_INT) {
        printTemp(actualSensors.temperatureInternal, mainColor, false);
        tft->setTextColor(GRAY);
        tft->print('C');
    } else if (tileMode == MODE_HUMIDITY) {
        printHumidity(actualSensors.humidity, mainColor);
        tft->setTextColor(GRAY);
        tft->print('%');
    } else if (tileMode == MODE_CO2) {
        printCo2Hppm(actualSensors.co2hppm, mainColor);
        tft->setTextColor(GRAY);
        tft->print('h');
        tft->print('p');
    } else if (tileMode == MODE_TEMP_EXT) {
        printTemp(actualSensors.temperatureExternal, mainColor, true);
        tft->setTextColor(GRAY);
        tft->print('C');
    } else if (tileMode == MODE_PRESSURE) {
        tft->setCursor(x + 4, y + 21);
        printPressure(actualSensors.pressureMinus600, mainColor);
        tft->setTextColor(GRAY);
        printMm();
    }
}

void MainScreen::drawBatteryTile(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint8_t forceRender) {
    tft->fillRect(x, y, w, h, SCREEN_BLACK);
    tft->setTextColor(GRAY);
    tft->setTextSize(2);
    tft->setCursor(x + 10, y + 9);
    tft->print(F("BAT"));
    tft->setTextSize(3);
    tft->setCursor(x + 10, y + 31);
    printBatteryVoltage(actualSensors.batteryMilliVolts, SCREEN_WHITE);
}

void MainScreen::printTemp(int8_t temp, uint16_t mainColor, bool isExternal) {
    bool isInvalid = isExternal && temp == TEMP_EXTERNAL_NONE || !isExternal && (temp <= 0 || temp >= 100);
    if (isInvalid) {
        tft->setTextColor(GRAY);
        if (isExternal) {
            tft->print('<');
        }
        printDashes();
        return;
    }

    if (isExternal && temp >= -9 && temp < 100) {
        tft->setTextColor(GRAY);
        tft->print('<');
    }

    tft->setTextColor(mainColor);
    if (temp < 0) {
        tft->print('-');
    } else if (temp < 10) {
        tft->print(' ');
    }
    tft->print(temp);
}

void MainScreen::printHumidity(uint8_t humidity, uint16_t mainColor) {
    if (humidity != HUMID_NONE) { // can't be less than 20
        tft->setTextColor(mainColor);
        tft->print(humidity);
    } else {
        tft->setTextColor(GRAY);
        printDashes();
    }
}

void MainScreen::printCo2Hppm(uint8_t co2Hppm, uint16_t mainColor) {
    if (co2Hppm == CO2_NONE) {
        tft->setTextColor(GRAY);
        printDashes();
    } else {
        tft->setTextColor(mainColor);
        if (co2Hppm < 10) {
            tft->print(' ');
        }
        tft->print(co2Hppm);
    }
}

void MainScreen::printPressure(uint8_t pressureMinus600, uint16_t mainColor) {
    if (pressureMinus600 != 0) {
        tft->setTextColor(mainColor);
        tft->print(pressureMinus600 + 600);
    } else {
        tft->setTextColor(GRAY);
        printDashes();
        tft->print('-');
    }
}

void MainScreen::printBatteryVoltage(uint16_t milliVolts, uint16_t mainColor) {
    tft->setTextColor(mainColor);
    uint16_t centiVolts = (milliVolts + 5) / 10;
    tft->print(centiVolts / 100);
    tft->print('.');
    if (centiVolts % 100 < 10) {
        tft->print('0');
    }
    tft->print(centiVolts % 100);
    tft->print('V');
}

uint8_t MainScreen::calculateQuality(Sensors& sensors) {
    uint8_t humWarnState = 100 - getWarningRank(
        HUM_WARN_MIN, HUM_URGENT_MIN,
        HUM_WARN_MAX, HUM_URGENT_MAX,
        sensors.humidity
    );

    uint8_t presWarnState = 100 - getWarningRank(
        PRES_WARN_MIN, PRES_URGENT_MIN,
        PRES_WARN_MAX, PRES_URGENT_MAX,
        sensors.pressureMinus600
    );

    uint8_t co2WarnState = 100 - getWarningRank(
        CO2_WARN_MIN, CO2_URGENT_MIN,
        CO2_WARN_MAX, CO2_URGENT_MAX,
        sensors.co2hppm
    );

    // DEBUG_SERIAL.print("Humidity quality: ");
    // DEBUG_SERIAL.println(humWarnState);
    // DEBUG_SERIAL.print("Pressure quality: ");
    // DEBUG_SERIAL.println(presWarnState);
    // DEBUG_SERIAL.print("CO2 quality: ");
    // DEBUG_SERIAL.println(co2WarnState);

    uint8_t summary = ((presWarnState + humWarnState + co2WarnState) / 3);
    if (summary > 85) {
        return QUALITY_BEST;
    } else if (summary > 70) {
        return QUALITY_GOOD;
    } else if (summary > 50) {
        return QUALITY_AVG;
    } else if (summary > 25) {
        return QUALITY_BAD;
    } else {
        return QUALITY_WORST;
    }

}

void MainScreen::updateQuality() {
    int16_t color = 0;
    if (quality == QUALITY_BEST) {
        color = 0x07E0; // green
    } else if (quality == QUALITY_GOOD) {
        color = 0x9fe0; // green-yellow
    } else if (quality == QUALITY_AVG) {
        color = 0xf7E0; // yellow
    } else if (quality == QUALITY_BAD) {
        color = 0xfcc0; // orange
    } else { // WORST
        color = 0xf800; // red
    }
    tft->fillRect(BOTTOM_TILE_W * 3 + 38, BOTTOM_BAR_Y + 18, 44, 28, color);
}

uint8_t MainScreen::getWarningRank(uint16_t warnMin, uint16_t urgentMin, uint16_t warnMax, uint16_t urgentMax, uint16_t value) {
    if (value <= urgentMin || value >= urgentMax) return 100;
    if (value > warnMin && value < warnMax) return 0;

    if (value <= warnMin) {
        return getWarningRank(warnMin, urgentMin, value);
    } else {
        return getWarningRank(warnMax, urgentMax, value);
    }
}

uint8_t MainScreen::getWarningRank(uint16_t warn, uint16_t urgent, uint16_t value) {
    if (value >= urgent) return 100;
    if (value <= warn) return 0;

    float f = ((float) (value - warn)) / (urgent - warn);
    return (uint8_t) (100 * f);
}

void MainScreen::printMm() {
    tft->print('m');
    tft->print('m');
}

void MainScreen::printDashes() {
    tft->print('-');
    tft->print('-');
}

#endif
