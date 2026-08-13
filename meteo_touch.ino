#include "main_screen.h"

#include <Arduino_GFX_Library.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

// ESP32-C6 / LCDWiki 3.5" RPi Display wiring
#define PIN_TFT_SCK 21
#define PIN_TFT_MOSI 19
#define PIN_TFT_MISO 20
#define PIN_TFT_CS 18
#define PIN_TFT_DC 10
#define PIN_TFT_RST 11
#define PIN_TOUCH_CS 3

// XPT2046 calibration defaults. Adjust these after first hardware touch test.
#define TOUCH_MIN_X 300
#define TOUCH_MAX_X 3900
#define TOUCH_MIN_Y 300
#define TOUCH_MAX_Y 3900
#define TOUCH_SWAP_XY true
#define TOUCH_INVERT_X false
#define TOUCH_INVERT_Y true

Arduino_DataBus *bus;
Arduino_GFX *tft;
XPT2046_Touchscreen *touch;

SensorsProvider *sensorsProvider;
MainScreen *mainScreen;

uint16_t mapTouchAxis(int16_t value, int16_t minValue, int16_t maxValue, uint16_t size, bool invert) {
    value = constrain(value, minValue, maxValue);
    long mapped = map(value, minValue, maxValue, 0, size - 1);
    if (invert) {
        mapped = size - 1 - mapped;
    }
    return (uint16_t) constrain(mapped, 0, size - 1);
}

bool readTouchPoint(uint16_t *x, uint16_t *y) {
    if (!touch->touched()) {
        return false;
    }

    TS_Point point = touch->getPoint();
    int16_t rawX = point.x;
    int16_t rawY = point.y;

    if (TOUCH_SWAP_XY) {
        int16_t tmp = rawX;
        rawX = rawY;
        rawY = tmp;
    }

    *x = mapTouchAxis(rawX, TOUCH_MIN_X, TOUCH_MAX_X, tft->width(), TOUCH_INVERT_X);
    *y = mapTouchAxis(rawY, TOUCH_MIN_Y, TOUCH_MAX_Y, tft->height(), TOUCH_INVERT_Y);
    return true;
}

void setup() {
    Serial.begin(115200);
    SPI.begin(PIN_TFT_SCK, PIN_TFT_MISO, PIN_TFT_MOSI);

    bus = new Arduino_ESP32SPI(PIN_TFT_DC, PIN_TFT_CS, PIN_TFT_SCK, PIN_TFT_MOSI, PIN_TFT_MISO);
    tft = new Arduino_ILI9486_18bit(bus, PIN_TFT_RST, 1, false);
    tft->begin();
    tft->setRotation(1);
    tft->fillScreen(SCREEN_BLACK);

    touch = new XPT2046_Touchscreen(PIN_TOUCH_CS);
    touch->begin();

    sensorsProvider = new SensorsProvider();

    mainScreen = new MainScreen(sensorsProvider, tft);
}

void loop() {
    mainScreen->loop();

    static bool wasTouched = false;
    uint16_t x;
    uint16_t y;

    if (readTouchPoint(&x, &y)) {
        if (!wasTouched) {
            mainScreen->onTouch(x, y);
        }
        wasTouched = true;
    } else {
        wasTouched = false;
    }
}
