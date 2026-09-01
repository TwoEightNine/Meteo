#ifndef METEO_DISPLAY_H
#define METEO_DISPLAY_H

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <SPI.h>

#define SCREEN_BLACK 0x0000
#define SCREEN_WHITE 0xffff

class MeteoDisplay : public Adafruit_GFX {
public:
    MeteoDisplay(int8_t cs, int8_t dc, int8_t rst, int8_t sck, int8_t mosi, int8_t miso);

    void begin();
    void setTransferMode(uint8_t mode);
    void drawPixel(int16_t x, int16_t y, uint16_t color);
    void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color);
    void drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color);
    void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color);
    void fillScreen(uint16_t color);
    void setRotation(uint8_t rotation);

private:
    int8_t cs;
    int8_t dc;
    int8_t rst;
    int8_t sck;
    int8_t mosi;
    int8_t miso;
    uint8_t rotationValue;
    uint8_t transferMode;

    void select();
    void deselect();
    void write8(uint8_t data);
    void write16(uint16_t data);
    void writeCommand(uint8_t command);
    void writeData(uint8_t data);
    void writeData16(uint16_t data);
    void setAddressWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
    void writeColor(uint16_t color, uint32_t len);
};

#endif
