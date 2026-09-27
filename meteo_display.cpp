#include "meteo_display.h"

#define ILI9486_SWRESET 0x01
#define ILI9486_SLPOUT  0x11
#define ILI9486_DISPON  0x29
#define ILI9486_CASET   0x2a
#define ILI9486_PASET   0x2b
#define ILI9486_RAMWR   0x2c
#define ILI9486_MADCTL  0x36
#define ILI9486_PIXFMT  0x3a
#define ILI9486_INVON   0x21
#define ILI9486_INVOFF  0x20

#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_BGR 0x08

#define DISPLAY_SPI_FREQUENCY 8000000

MeteoDisplay::MeteoDisplay(int8_t cs, int8_t dc, int8_t rst, int8_t sck, int8_t mosi, int8_t miso)
    : Adafruit_GFX(320, 480) {
    this->cs = cs;
    this->dc = dc;
    this->rst = rst;
    this->sck = sck;
    this->mosi = mosi;
    this->miso = miso;
    this->rotationValue = 0;
    this->transferMode = 1;
}

void MeteoDisplay::begin() {
    Serial.println(F("display: configure pins"));
    pinMode(cs, OUTPUT);
    pinMode(dc, OUTPUT);
    pinMode(rst, OUTPUT);
    digitalWrite(cs, HIGH);
    digitalWrite(dc, HIGH);

    Serial.println(F("display: spi begin"));
    SPI.begin(sck, miso, mosi, cs);

    Serial.println(F("display: reset"));
    digitalWrite(rst, HIGH);
    delay(10);
    digitalWrite(rst, LOW);
    delay(20);
    digitalWrite(rst, HIGH);
    delay(150);

    Serial.println(F("display: init controller"));
    writeCommand(ILI9486_SWRESET);
    delay(120);
    writeCommand(ILI9486_SLPOUT);
    delay(120);

    writeCommand(ILI9486_PIXFMT);
    writeData(0x55); // RPi-style ILI9486 accepts 16-bit RGB565 pixels.

    writeCommand(0xc0);
    writeData(0x0e);
    writeData(0x0e);

    writeCommand(0xc1);
    writeData(0x41);
    writeData(0x00);

    writeCommand(0xc2);
    writeData(0x55);

    writeCommand(0xc5);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);
    writeData(0x00);

    writeCommand(0xe0);
    writeData(0x0f);
    writeData(0x1f);
    writeData(0x1c);
    writeData(0x0c);
    writeData(0x0f);
    writeData(0x08);
    writeData(0x48);
    writeData(0x98);
    writeData(0x37);
    writeData(0x0a);
    writeData(0x13);
    writeData(0x04);
    writeData(0x11);
    writeData(0x0d);
    writeData(0x00);

    writeCommand(0xe1);
    writeData(0x0f);
    writeData(0x32);
    writeData(0x2e);
    writeData(0x0b);
    writeData(0x0d);
    writeData(0x05);
    writeData(0x47);
    writeData(0x75);
    writeData(0x37);
    writeData(0x06);
    writeData(0x10);
    writeData(0x03);
    writeData(0x24);
    writeData(0x20);
    writeData(0x00);

    writeCommand(ILI9486_INVOFF);

    setRotation(1);

    writeCommand(ILI9486_DISPON);
    delay(150);
    Serial.println(F("display: init done"));
}

void MeteoDisplay::setTransferMode(uint8_t mode) {
    transferMode = mode;
}

void MeteoDisplay::setRotation(uint8_t rotation) {
    rotationValue = rotation & 3;

    writeCommand(ILI9486_MADCTL);
    switch (rotationValue) {
        case 0:
            writeData(MADCTL_BGR);
            _width = 320;
            _height = 480;
            break;
        case 1:
            writeData(MADCTL_MV | MADCTL_BGR);
            _width = 480;
            _height = 320;
            break;
        case 2:
            writeData(MADCTL_MX | MADCTL_MY | MADCTL_BGR);
            _width = 320;
            _height = 480;
            break;
        default:
            writeData(MADCTL_MV | MADCTL_MX | MADCTL_BGR);
            _width = 480;
            _height = 320;
            break;
    }
}

void MeteoDisplay::drawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || y < 0 || x >= width() || y >= height()) {
        return;
    }

    setAddressWindow(x, y, 1, 1);
    select();
    writeColor(color, 1);
    deselect();
}

void MeteoDisplay::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color) {
    fillRect(x, y, w, 1, color);
}

void MeteoDisplay::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color) {
    fillRect(x, y, 1, h, color);
}

void MeteoDisplay::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color) {
    if (w <= 0 || h <= 0 || x >= width() || y >= height()) {
        return;
    }
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > width()) {
        w = width() - x;
    }
    if (y + h > height()) {
        h = height() - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }

    setAddressWindow(x, y, w, h);
    select();
    writeColor(color, (uint32_t) w * h);
    deselect();
}

void MeteoDisplay::fillScreen(uint16_t color) {
    fillRect(0, 0, width(), height(), color);
}

void MeteoDisplay::drawRGB565Row(int16_t x, int16_t y, const uint16_t *pixels, int16_t w) {
    if (w <= 0 || x < 0 || y < 0 || x + w > width() || y >= height()) {
        return;
    }

    setAddressWindow(x, y, w, 1);
    select();
    for (int16_t i = 0; i < w; ++i) {
        write16(pixels[i]);
    }
    deselect();
}

void MeteoDisplay::select() {
    SPI.beginTransaction(SPISettings(DISPLAY_SPI_FREQUENCY, MSBFIRST, SPI_MODE0));
    digitalWrite(cs, LOW);
}

void MeteoDisplay::deselect() {
    digitalWrite(cs, HIGH);
    SPI.endTransaction();
}

void MeteoDisplay::writeCommand(uint8_t command) {
    select();
    digitalWrite(dc, LOW);
    write8(command);
    digitalWrite(dc, HIGH);
    deselect();
}

void MeteoDisplay::writeData(uint8_t data) {
    select();
    digitalWrite(dc, HIGH);
    write8(data);
    deselect();
}

void MeteoDisplay::writeData16(uint16_t data) {
    write8(data >> 8);
    write8(data & 0xff);
}

void MeteoDisplay::write8(uint8_t data) {
    if (transferMode == 1) {
        SPI.transfer(0x00);
        SPI.transfer(data);
    } else if (transferMode == 2) {
        SPI.transfer(data);
        SPI.transfer(0x00);
    } else if (transferMode == 3) {
        SPI.transfer(data);
        SPI.transfer(data);
    } else {
        SPI.transfer(data);
    }
}

void MeteoDisplay::write16(uint16_t data) {
    if (transferMode == 1 || transferMode == 2 || transferMode == 3) {
        SPI.transfer(data >> 8);
        SPI.transfer(data & 0xff);
    } else {
        SPI.transfer(data >> 8);
        SPI.transfer(data & 0xff);
    }
}

void MeteoDisplay::setAddressWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t x2 = x + w - 1;
    uint16_t y2 = y + h - 1;

    writeCommand(ILI9486_CASET);
    select();
    digitalWrite(dc, HIGH);
    writeData16(x);
    writeData16(x2);
    deselect();

    writeCommand(ILI9486_PASET);
    select();
    digitalWrite(dc, HIGH);
    writeData16(y);
    writeData16(y2);
    deselect();

    writeCommand(ILI9486_RAMWR);
}

void MeteoDisplay::writeColor(uint16_t color, uint32_t len) {
    while (len--) {
        write16(color);
    }
}
