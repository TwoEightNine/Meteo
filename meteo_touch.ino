#include "meteo_config.h"
#include "meteo_display.h"

#if !METEO_DISPLAY_ONLY && !METEO_TOUCH_CALIBRATION
#include "main_screen.h"
#endif

#if !METEO_DISPLAY_ONLY
#include <XPT2046_Touchscreen.h>
#endif

#if !METEO_DISPLAY_ONLY && METEO_TOUCH_CALIBRATION
#include "touch_calibration.h"
#endif

#define PIN_TFT_SCK 8
#define PIN_TFT_MOSI 19
#define PIN_TFT_MISO 15
#define PIN_TFT_CS 14
#define PIN_TFT_DC 20
#define PIN_TFT_RST 18
#define PIN_TOUCH_CS 9

// XPT2046 calibration for the 480x320 landscape display. Adjust these if the
// panel is mirrored, rotated, or offset on a particular unit.
#define TOUCH_MIN_X 1724
#define TOUCH_MAX_X 3890
#define TOUCH_MIN_Y 968
#define TOUCH_MAX_Y 3057
#define TOUCH_SWAP_XY false
#define TOUCH_INVERT_X true
#define TOUCH_INVERT_Y false
#define TOUCH_PRESSURE_MIN 1200
#define TOUCH_POLL_INTERVAL_MS 5
#define TOUCH_RELEASE_DEBOUNCE_MS 20

MeteoDisplay tft(PIN_TFT_CS, PIN_TFT_DC, PIN_TFT_RST, PIN_TFT_SCK, PIN_TFT_MOSI, PIN_TFT_MISO);

#if METEO_DISPLAY_ONLY
void drawDisplayOnlyScreen() {
    tft.fillScreen(SCREEN_BLACK);

    tft.drawRect(0, 0, tft.width(), tft.height(), SCREEN_WHITE);
    tft.drawFastHLine(0, 64, tft.width(), SCREEN_WHITE);
    tft.drawFastHLine(0, 256, tft.width(), SCREEN_WHITE);

    tft.fillRect(1, 1, 159, 63, 0x001f);
    tft.fillRect(160, 1, 160, 63, 0x07e0);
    tft.fillRect(320, 1, 159, 63, 0xf800);
    tft.fillRect(1, 257, 119, 62, 0x07ff);
    tft.fillRect(120, 257, 120, 62, 0xf81f);
    tft.fillRect(240, 257, 120, 62, 0xffe0);
    tft.fillRect(398, 274, 44, 28, 0x07e0);

    tft.setTextColor(SCREEN_WHITE);
    tft.setTextSize(2);
    tft.setCursor(16, 24);
    tft.print(F("BLUE L"));
    tft.setCursor(184, 24);
    tft.print(F("GREEN"));
    tft.setCursor(348, 24);
    tft.print(F("RED R"));

    tft.setTextSize(4);
    tft.setCursor(72, 130);
    tft.print(F("ILI9486"));
    tft.setCursor(72, 178);
    tft.print(F("DISPLAY OK"));

    tft.setTextSize(2);
    tft.setTextColor(0x8c71);
    tft.setCursor(18, 280);
    tft.print(F("480x320"));
    tft.setCursor(142, 280);
    tft.print(F("SPI"));
    tft.setCursor(260, 280);
    tft.print(F("C6"));
}
#endif

#if !METEO_DISPLAY_ONLY
#if !METEO_TOUCH_CALIBRATION
SensorsProvider *sensorsProvider;
MainScreen *mainScreen;
#endif
XPT2046_Touchscreen touch(PIN_TOUCH_CS);

#if METEO_TOUCH_CALIBRATION
TouchCalibration calibration(tft, touch, TOUCH_PRESSURE_MIN);
#else

uint16_t mapTouchAxis(int16_t value, int16_t minValue, int16_t maxValue, uint16_t size, bool invert) {
    value = constrain(value, minValue, maxValue);
    long mapped = map(value, minValue, maxValue, 0, size - 1);
    if (invert) {
        mapped = size - 1 - mapped;
    }
    return (uint16_t) constrain(mapped, 0, size - 1);
}

bool readTouchPoint(uint16_t *x, uint16_t *y, int16_t *rawXResult, int16_t *rawYResult, int16_t *pressure) {
    if (!touch.touched()) {
        return false;
    }

    TS_Point point = touch.getPoint();
    if (point.z < TOUCH_PRESSURE_MIN) {
        return false;
    }

    int16_t rawX = point.x;
    int16_t rawY = point.y;

    *rawXResult = rawX;
    *rawYResult = rawY;
    *pressure = point.z;

    if (TOUCH_SWAP_XY) {
        int16_t tmp = rawX;
        rawX = rawY;
        rawY = tmp;
    }

    *x = mapTouchAxis(rawX, TOUCH_MIN_X, TOUCH_MAX_X, tft.width(), TOUCH_INVERT_X);
    *y = mapTouchAxis(rawY, TOUCH_MIN_Y, TOUCH_MAX_Y, tft.height(), TOUCH_INVERT_Y);
    return true;
}

struct PendingTouch {
    bool available = false;
    uint16_t x = 0;
    uint16_t y = 0;
};

PendingTouch pendingTouch;
bool touchDown = false;
uint32_t lastTouchPollAt = 0;
uint32_t lastValidTouchAt = 0;

void pollTouchInput() {
    uint32_t now = millis();
    if ((uint32_t) (now - lastTouchPollAt) < TOUCH_POLL_INTERVAL_MS) {
        return;
    }
    lastTouchPollAt = now;

    uint16_t x;
    uint16_t y;
    int16_t rawX;
    int16_t rawY;
    int16_t pressure;
    if (readTouchPoint(&x, &y, &rawX, &rawY, &pressure)) {
        lastValidTouchAt = now;
        if (!touchDown) {
            touchDown = true;
            if (!pendingTouch.available) {
                pendingTouch.x = x;
                pendingTouch.y = y;
                pendingTouch.available = true;
            }

            Serial.print(F("touch: raw=("));
            Serial.print(rawX);
            Serial.print(F(","));
            Serial.print(rawY);
            Serial.print(F(") z="));
            Serial.print(pressure);
            Serial.print(F(" screen=("));
            Serial.print(x);
            Serial.print(F(","));
            Serial.print(y);
            Serial.println(F(")"));
        }
        return;
    }

    if (touchDown && (uint32_t) (now - lastValidTouchAt) >= TOUCH_RELEASE_DEBOUNCE_MS) {
        touchDown = false;
        Serial.println(F("touch: released"));
    }
}

void dispatchPendingTouch() {
    if (!pendingTouch.available) {
        return;
    }

    uint16_t x = pendingTouch.x;
    uint16_t y = pendingTouch.y;
    pendingTouch.available = false;
    mainScreen->onTouch(x, y);
}
#endif
#endif

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println();
    Serial.println(F("meteo_touch: boot"));
    Serial.println(F("meteo_touch: pins sck=8 mosi=19 miso=15 lcd_cs=14 dc=20 rst=18 tp_cs=9"));
    Serial.println(F("meteo_touch: display transfer mode 1, RPi 16-bit 00,data"));

    tft.setTransferMode(1);
    tft.begin();
    tft.setRotation(1);
    Serial.print(F("meteo_touch: display size "));
    Serial.print(tft.width());
    Serial.print(F("x"));
    Serial.println(tft.height());

#if METEO_DISPLAY_ONLY
    drawDisplayOnlyScreen();
    Serial.println(F("meteo_touch: test screen drawn"));
#else
#if !METEO_TOUCH_CALIBRATION
    Serial.println(F("meteo_touch: init sensors"));
    sensorsProvider = new SensorsProvider();
    Serial.println(F("meteo_touch: init main screen"));
    mainScreen = new MainScreen(sensorsProvider, &tft);
#endif
    Serial.println(F("meteo_touch: init touch"));
    touch.begin(SPI);
    touch.setRotation(1);
#if METEO_TOUCH_CALIBRATION
    calibration.begin();
    Serial.println(F("meteo_touch: touch calibration ready"));
#else
    tft.setServiceCallback(pollTouchInput);
    sensorsProvider->setServiceCallback(pollTouchInput);
#endif
#endif
}

void loop() {
#if METEO_DISPLAY_ONLY
    static uint32_t lastPrint = 0;
    if (millis() - lastPrint >= 5000) {
        Serial.println(F("meteo_touch: running"));
        lastPrint = millis();
    }
    delay(1000);
#else
#if METEO_TOUCH_CALIBRATION
    calibration.loop();
#else
    pollTouchInput();
    dispatchPendingTouch();
    mainScreen->loop();
    pollTouchInput();
    dispatchPendingTouch();
#endif
#endif
}
