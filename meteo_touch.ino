#include "meteo_config.h"
#include "meteo_display.h"

#if !METEO_DISPLAY_ONLY
#include "main_screen.h"
#endif

#define PIN_TFT_SCK 8
#define PIN_TFT_MOSI 19
#define PIN_TFT_MISO 20
#define PIN_TFT_CS 18
#define PIN_TFT_DC 14
#define PIN_TFT_RST 15

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
#else
SensorsProvider *sensorsProvider;
MainScreen *mainScreen;
#endif

void setup() {
    Serial.begin(115200);
    delay(1500);
    Serial.println();
    Serial.println(F("meteo_touch: boot"));
    Serial.println(F("meteo_touch: pins sck=8 mosi=19 miso=20 cs=18 dc=14 rst=15"));
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
    Serial.println(F("meteo_touch: init sensors"));
    sensorsProvider = new SensorsProvider();
    Serial.println(F("meteo_touch: init main screen"));
    mainScreen = new MainScreen(sensorsProvider, &tft);
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
    mainScreen->loop();
#endif
}
