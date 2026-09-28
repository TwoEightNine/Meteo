#include "meteo_config.h"

#if !METEO_DISPLAY_ONLY && !METEO_TOUCH_CALIBRATION

#include "main_screen.h"
#include "smooth_digits.h"
#include "smooth_text.h"

#include <stdio.h>
#include <string.h>

namespace {
constexpr int16_t SCREEN_W = 480;
constexpr int16_t SCREEN_H = 320;
constexpr int16_t CONTENT_Y = 32;
constexpr int16_t CONTENT_BOTTOM = 317;
constexpr int16_t FOCUS_X = 7;
constexpr int16_t FOCUS_RIGHT = 278;
constexpr int16_t DIVIDER_X = 282;
constexpr int16_t SIDE_X = 288;
constexpr int16_t SIDE_RIGHT = 474;
constexpr int16_t STATUS_X = 7;
constexpr int16_t STATUS_Y = 16;
constexpr int16_t STATUS_W = 394;
constexpr int16_t STATUS_H = 4;
constexpr int16_t FOCUS_VALUE_X = 22;
constexpr int16_t FOCUS_VALUE_Y = 101;
constexpr int16_t FOCUS_VALUE_W = 244;
constexpr int16_t FOCUS_VALUE_H = 132;
constexpr int16_t SIDE_Y[4] = {32, 104, 176, 248};
constexpr int16_t SIDE_H[4] = {69, 69, 69, 69};

constexpr uint16_t BACKGROUND = 0x0000;
constexpr uint16_t SEPARATOR = 0x39CB;
constexpr uint16_t DIVIDER = 0x05B5;
constexpr uint16_t MUTED = 0x8C92;
constexpr uint16_t UNIT_COLOR = 0xC638;
constexpr uint16_t CYAN = 0x07FF;
constexpr uint16_t MAGENTA = 0xF81F;
constexpr uint16_t YELLOW = 0xFFE0;
constexpr uint16_t PURPLE = 0xA81F;
constexpr uint16_t TEAL = 0x2FF6;
constexpr uint16_t GREEN = 0x07E0;
constexpr uint16_t RED = 0xF800;

struct SmoothFont {
    const char *chars;
    const SmoothGlyph *glyphs;
    const uint8_t *pixels;
    uint8_t spaceAdvance;
};

const SmoothFont FOCUS_FONT = {SMOOTH_DIGIT_CHARS, SMOOTH_DIGIT_GLYPHS,
                               SMOOTH_DIGIT_PIXELS, 0};
const SmoothFont SIDE_FONT = {SMOOTH_SIDE_CHARS, SMOOTH_SIDE_GLYPHS,
                              SMOOTH_SIDE_PIXELS, 0};
const SmoothFont SIDE_COMPACT_FONT = {SMOOTH_SIDE_COMPACT_CHARS,
                                      SMOOTH_SIDE_COMPACT_GLYPHS,
                                      SMOOTH_SIDE_COMPACT_PIXELS, 0};
const SmoothFont SIDE_TINY_FONT = {SMOOTH_SIDE_TINY_CHARS,
                                   SMOOTH_SIDE_TINY_GLYPHS,
                                   SMOOTH_SIDE_TINY_PIXELS, 0};
const SmoothFont LABEL_FONT = {SMOOTH_LABEL_CHARS, SMOOTH_LABEL_GLYPHS,
                               SMOOTH_LABEL_PIXELS, 7};
const SmoothFont TITLE_FONT = {SMOOTH_TITLE_CHARS, SMOOTH_TITLE_GLYPHS,
                               SMOOTH_TITLE_PIXELS, 11};
const SmoothFont UNIT_FONT = {SMOOTH_UNIT_CHARS, SMOOTH_UNIT_GLYPHS,
                              SMOOTH_UNIT_PIXELS, 7};
const SmoothFont FOCUS_UNIT_FONT = {SMOOTH_FOCUS_UNIT_CHARS,
                                    SMOOTH_FOCUS_UNIT_GLYPHS,
                                    SMOOTH_FOCUS_UNIT_PIXELS, 12};
const SmoothFont SUBSCRIPT_FONT = {SMOOTH_SUBSCRIPT_CHARS,
                                   SMOOTH_SUBSCRIPT_GLYPHS,
                                   SMOOTH_SUBSCRIPT_PIXELS, 0};

uint16_t sensorColor(uint8_t sensorMode) {
    switch (sensorMode) {
        case MODE_TEMP_INT: return CYAN;
        case MODE_TEMP_EXT: return MAGENTA;
        case MODE_PRESSURE: return YELLOW;
        case MODE_CO2: return PURPLE;
        case MODE_HUMIDITY: return TEAL;
        default: return UNIT_COLOR;
    }
}

const __FlashStringHelper *modeLabel(uint8_t value) {
    switch (value) {
        case MODE_TEMP_INT: return F("temp-int");
        case MODE_HUMIDITY: return F("humidity");
        case MODE_CO2: return F("co2");
        case MODE_TEMP_EXT: return F("temp-ext");
        case MODE_PRESSURE: return F("pressure");
        default: return F("unknown");
    }
}

uint16_t dim565(uint16_t color, uint8_t coverage) {
    uint16_t red = ((color >> 11) * coverage + 127) / 255;
    uint16_t green = (((color >> 5) & 0x3F) * coverage + 127) / 255;
    uint16_t blue = ((color & 0x1F) * coverage + 127) / 255;
    return (red << 11) | (green << 5) | blue;
}

void drawSoftHLine(MeteoDisplay *tft, int16_t x, int16_t y,
                   int16_t length, uint16_t color) {
    uint16_t fringe = dim565(color, 56);
    tft->drawFastHLine(x, y - 1, length, fringe);
    tft->drawFastHLine(x, y, length, color);
    tft->drawFastHLine(x, y + 1, length, fringe);
}

void drawSoftVLine(MeteoDisplay *tft, int16_t x, int16_t y,
                   int16_t length, uint16_t color) {
    uint16_t fringe = dim565(color, 56);
    tft->drawFastVLine(x - 1, y, length, fringe);
    tft->drawFastVLine(x, y, length, color);
    tft->drawFastVLine(x + 1, y, length, fringe);
}

void drawAALine(MeteoDisplay *tft, int16_t x0, int16_t y0,
                int16_t x1, int16_t y1, uint16_t color) {
    if (x0 > x1) {
        int16_t temp = x0; x0 = x1; x1 = temp;
        temp = y0; y0 = y1; y1 = temp;
    }
    if (x0 == x1) {
        drawSoftVLine(tft, x0, y0, y1 - y0 + 1, color);
        return;
    }

    int32_t yFixed = (int32_t) y0 * 256;
    int32_t step = ((int32_t) (y1 - y0) * 256) / (x1 - x0);
    for (int16_t x = x0; x <= x1; ++x, yFixed += step) {
        int16_t y = yFixed >> 8;
        uint8_t fraction = yFixed & 0xFF;
        uint16_t upper = dim565(color, 255 - fraction);
        uint16_t lower = dim565(color, fraction);
        if (upper) tft->drawPixel(x, y, upper);
        if (lower) tft->drawPixel(x, y + 1, lower);
    }
}

void drawAASegment(MeteoDisplay *tft, int16_t x0, int16_t y0,
                   int16_t x1, int16_t y1, uint16_t color) {
    if (y0 == y1) {
        if (x1 < x0) { int16_t swap = x0; x0 = x1; x1 = swap; }
        drawSoftHLine(tft, x0, y0, x1 - x0 + 1, color);
    } else if (x0 == x1) {
        if (y1 < y0) { int16_t swap = y0; y0 = y1; y1 = swap; }
        drawSoftVLine(tft, x0, y0, y1 - y0 + 1, color);
    } else {
        drawAALine(tft, x0, y0, x1, y1, color);
    }
}

void drawFocusBorder(MeteoDisplay *tft, uint16_t color) {
    drawAASegment(tft, FOCUS_X + 22, CONTENT_Y, FOCUS_RIGHT - 11, CONTENT_Y, color);
    drawAASegment(tft, FOCUS_X + 22, CONTENT_Y, FOCUS_X, CONTENT_Y + 22, color);
    drawAASegment(tft, FOCUS_X, CONTENT_Y + 22, FOCUS_X, CONTENT_BOTTOM - 19, color);
    drawAASegment(tft, FOCUS_X, CONTENT_BOTTOM - 19,
                  FOCUS_X + 18, CONTENT_BOTTOM, color);
    drawAASegment(tft, FOCUS_X + 18, CONTENT_BOTTOM,
                  FOCUS_RIGHT - 13, CONTENT_BOTTOM, color);
    drawAASegment(tft, FOCUS_RIGHT - 13, CONTENT_BOTTOM,
                  FOCUS_RIGHT, CONTENT_BOTTOM - 13, color);
    drawAASegment(tft, FOCUS_RIGHT, CONTENT_BOTTOM - 13,
                  FOCUS_RIGHT, CONTENT_BOTTOM - 25, color);
    drawAASegment(tft, FOCUS_RIGHT - 11, CONTENT_Y, FOCUS_RIGHT, CONTENT_Y + 11, color);
    drawAASegment(tft, FOCUS_RIGHT, CONTENT_Y + 11, FOCUS_RIGHT, CONTENT_Y + 31, color);

    tft->fillTriangle(FOCUS_X, CONTENT_Y, FOCUS_X + 15, CONTENT_Y,
                      FOCUS_X, CONTENT_Y + 15, color);
    drawAALine(tft, FOCUS_X, CONTENT_Y + 15, FOCUS_X + 15, CONTENT_Y, color);
    tft->fillTriangle(FOCUS_X, CONTENT_BOTTOM - 15, FOCUS_X, CONTENT_BOTTOM,
                      FOCUS_X + 15, CONTENT_BOTTOM, color);
    drawAALine(tft, FOCUS_X, CONTENT_BOTTOM - 15,
               FOCUS_X + 15, CONTENT_BOTTOM, color);
}

void drawSideBorder(MeteoDisplay *tft, uint8_t row, uint16_t color) {
    int16_t top = SIDE_Y[row];
    int16_t bottom = top + SIDE_H[row] - 1;
    constexpr int16_t chamfer = 11;
    drawAASegment(tft, SIDE_X + chamfer, top, SIDE_RIGHT - chamfer, top, color);
    drawAASegment(tft, SIDE_RIGHT - chamfer, top, SIDE_RIGHT, top + chamfer, color);
    drawAASegment(tft, SIDE_RIGHT, top + chamfer, SIDE_RIGHT, bottom - chamfer, color);
    drawAASegment(tft, SIDE_RIGHT, bottom - chamfer, SIDE_RIGHT - chamfer, bottom, color);
    drawAASegment(tft, SIDE_RIGHT - chamfer, bottom, SIDE_X + chamfer, bottom, color);
    drawAASegment(tft, SIDE_X + chamfer, bottom, SIDE_X, bottom - chamfer, color);
    drawAASegment(tft, SIDE_X, bottom - chamfer, SIDE_X, top + chamfer, color);
    drawAASegment(tft, SIDE_X, top + chamfer, SIDE_X + chamfer, top, color);

    tft->fillTriangle(SIDE_X, top, SIDE_X + chamfer, top,
                      SIDE_X, top + chamfer, color);
    tft->fillTriangle(SIDE_RIGHT, top, SIDE_RIGHT - chamfer, top,
                      SIDE_RIGHT, top + chamfer, color);
    tft->fillTriangle(SIDE_X, bottom, SIDE_X + chamfer, bottom,
                      SIDE_X, bottom - chamfer, color);
    tft->fillTriangle(SIDE_RIGHT, bottom, SIDE_RIGHT - chamfer, bottom,
                      SIDE_RIGHT, bottom - chamfer, color);
    drawAALine(tft, SIDE_X, top + chamfer, SIDE_X + chamfer, top, color);
    drawAALine(tft, SIDE_RIGHT - chamfer, top, SIDE_RIGHT, top + chamfer, color);
    drawAALine(tft, SIDE_X, bottom - chamfer, SIDE_X + chamfer, bottom, color);
    drawAALine(tft, SIDE_RIGHT - chamfer, bottom, SIDE_RIGHT, bottom - chamfer, color);
}

struct SmoothRun {
    SmoothGlyph glyph;
    int16_t penX;
};

uint8_t smoothAlphaAt(const SmoothRun *runs, uint8_t count,
                      const uint8_t *pixels,
                      int16_t sourceX, int16_t sourceY) {
    if (sourceX < 0) return 0;
    for (uint8_t i = 0; i < count; ++i) {
        const SmoothGlyph& glyph = runs[i].glyph;
        if (sourceX >= runs[i].penX + glyph.advance) continue;
        int16_t x = sourceX - runs[i].penX - glyph.xOffset;
        int16_t y = sourceY - glyph.yOffset;
        if (x < 0 || y < 0 || x >= glyph.width || y >= glyph.height) return 0;
        uint32_t pixel = (uint32_t) y * glyph.width + x;
        uint8_t packed = pgm_read_byte(pixels + glyph.offset + pixel / 2);
        return (pixel & 1) ? (packed & 0x0F) : (packed >> 4);
    }
    return 0;
}

struct SmoothLayout {
    SmoothRun runs[16];
    uint8_t count = 0;
    int16_t width = 0;
    int16_t top = 0;
    int16_t bottom = 0;
};

SmoothLayout layoutText(const char *text, const SmoothFont& font) {
    SmoothLayout layout;
    for (const char *c = text; *c && layout.count < 16; ++c) {
        if (*c == ' ') {
            layout.width += font.spaceAdvance;
            continue;
        }
        const char *found = strchr(font.chars, *c);
        if (!found) continue;
        SmoothGlyph glyph = font.glyphs[found - font.chars];
        layout.runs[layout.count++] = {glyph, layout.width};
        layout.width += glyph.advance;
        if (glyph.yOffset < layout.top) layout.top = glyph.yOffset;
        int16_t bottom = glyph.yOffset + glyph.height;
        if (bottom > layout.bottom) layout.bottom = bottom;
    }
    return layout;
}

TextRect measureSmoothText(const char *text, const SmoothFont& font) {
    SmoothLayout layout = layoutText(text, font);
    return {0, 0, (uint16_t) layout.width,
            (uint16_t) (layout.bottom - layout.top)};
}

TextRect drawSmoothText(MeteoDisplay *tft, const char *text, int16_t x, int16_t y,
                        const SmoothFont& font, uint16_t color) {
    SmoothLayout layout = layoutText(text, font);
    int16_t height = layout.bottom - layout.top;
    if (!layout.count || layout.width <= 0 || height <= 0) return {};

    uint16_t palette[16];
    for (uint8_t a = 0; a < 16; ++a) palette[a] = dim565(color, a * 17);
    uint16_t rowPixels[256];
    if (layout.width > 256) return {};
    for (int16_t row = 0; row < height; ++row) {
        for (int16_t col = 0; col < layout.width; ++col) {
            rowPixels[col] = palette[smoothAlphaAt(layout.runs, layout.count,
                                                   font.pixels, col,
                                                   layout.top + row)];
        }
        tft->drawRGB565Row(x, y + row, rowPixels, layout.width);
    }
    return {x, y, (uint16_t) layout.width, (uint16_t) height};
}
}

MainScreen::MainScreen(SensorsProvider *sensorsProvider, MeteoDisplay *tft)
    : sensorsProvider(sensorsProvider), tft(tft) {
    tft->setTextWrap(false);
    drawFrame();
}

void MainScreen::drawFrame() {
    tft->fillScreen(BACKGROUND);
    tft->drawFastHLine(STATUS_X, STATUS_Y - 1, STATUS_W, dim565(SEPARATOR, 64));
    tft->fillRect(STATUS_X, STATUS_Y, STATUS_W, STATUS_H, SEPARATOR);
    tft->drawFastHLine(STATUS_X, STATUS_Y + STATUS_H, STATUS_W, dim565(SEPARATOR, 64));
    drawSoftVLine(tft, DIVIDER_X, CONTENT_Y,
                  CONTENT_BOTTOM - CONTENT_Y + 1, CYAN);
}

void MainScreen::clearText(TextRect& bounds) {
    if (bounds.w && bounds.h) {
        tft->fillRect(bounds.x - 1, bounds.y - 1, bounds.w + 2, bounds.h + 2, BACKGROUND);
        bounds = {};
    }
}

void MainScreen::drawLabel(uint8_t sensorMode, int16_t x, int16_t y, bool focus) {
    uint16_t color = sensorColor(sensorMode);
    const SmoothFont& font = focus ? TITLE_FONT : LABEL_FONT;
    if (sensorMode == MODE_CO2) {
        TextRect co = drawSmoothText(tft, "CO", x, y, font, color);
        drawSmoothText(tft, "2", x + co.w + 1,
                       y + (focus ? 12 : 6), SUBSCRIPT_FONT, color);
        return;
    }

    const char *label = "";
    switch (sensorMode) {
        case MODE_TEMP_INT: label = "TEMP"; break;
        case MODE_TEMP_EXT: label = "TEMP EXT"; break;
        case MODE_PRESSURE: label = "PRESSURE"; break;
        case MODE_HUMIDITY: label = "HUMIDITY"; break;
    }
    drawSmoothText(tft, label, x, y, font, color);
}

void MainScreen::formatValue(uint8_t sensorMode, char *buffer, size_t bufferSize) {
    switch (sensorMode) {
        case MODE_TEMP_INT:
            if (actualSensors.temperatureInternal <= 0 ||
                actualSensors.temperatureInternal >= 100) {
                snprintf(buffer, bufferSize, "--");
            } else {
                snprintf(buffer, bufferSize, "%d", (int) actualSensors.temperatureInternal);
            }
            break;
        case MODE_TEMP_EXT:
            if (actualSensors.temperatureExternal == TEMP_EXTERNAL_NONE) {
                snprintf(buffer, bufferSize, "--");
            } else {
                snprintf(buffer, bufferSize, "%d", (int) actualSensors.temperatureExternal);
            }
            break;
        case MODE_PRESSURE:
            if (actualSensors.pressureMinus600 == 0) {
                snprintf(buffer, bufferSize, "--");
            } else {
                uint32_t mmHg = (uint32_t) actualSensors.pressureMinus600 + 600;
                snprintf(buffer, bufferSize, "%lu", (unsigned long) mmHg);
            }
            break;
        case MODE_CO2:
            if (actualSensors.co2ppm == CO2_NONE) {
                snprintf(buffer, bufferSize, "--");
            } else {
                snprintf(buffer, bufferSize, "%u", (unsigned int) actualSensors.co2ppm);
            }
            break;
        case MODE_HUMIDITY:
            if (actualSensors.humidity == HUMID_NONE) {
                snprintf(buffer, bufferSize, "--");
            } else {
                snprintf(buffer, bufferSize, "%u", (unsigned int) actualSensors.humidity);
            }
            break;
        default:
            snprintf(buffer, bufferSize, "--");
    }
}

uint16_t MainScreen::unitWidth(uint8_t sensorMode, bool focus) {
    if (sensorMode == MODE_TEMP_INT || sensorMode == MODE_TEMP_EXT) {
        const SmoothFont& degreeFont = focus ? LABEL_FONT : SUBSCRIPT_FONT;
        const SmoothFont& letterFont = focus ? FOCUS_UNIT_FONT : UNIT_FONT;
        return measureSmoothText("O", degreeFont).w + 2 +
               measureSmoothText("C", letterFont).w;
    }
    const char *unit = sensorMode == MODE_PRESSURE ? "mmHg" :
                       sensorMode == MODE_CO2 ? "ppm" : "%";
    return measureSmoothText(unit, focus ? FOCUS_UNIT_FONT : UNIT_FONT).w;
}

TextRect MainScreen::drawUnit(uint8_t sensorMode, int16_t x, int16_t y, bool focus) {
    uint16_t color = sensorColor(sensorMode);
    const SmoothFont& unitFont = focus ? FOCUS_UNIT_FONT : UNIT_FONT;
    if (sensorMode == MODE_TEMP_INT || sensorMode == MODE_TEMP_EXT) {
        const SmoothFont& degreeFont = focus ? LABEL_FONT : SUBSCRIPT_FONT;
        TextRect degree = drawSmoothText(tft, "O", x, y, degreeFont, color);
        TextRect letter = drawSmoothText(tft, "C", x + degree.w + 2, y,
                                         unitFont, color);
        uint16_t height = degree.h > letter.h ? degree.h : letter.h;
        return {x, y, (uint16_t) (degree.w + 2 + letter.w), height};
    }
    const char *unit = sensorMode == MODE_PRESSURE ? "mmHg" :
                       sensorMode == MODE_CO2 ? "ppm" : "%";
    return drawSmoothText(tft, unit, x, y, unitFont, color);
}

TextRect MainScreen::drawSmoothValue(const char *number, uint16_t color) {
    SmoothLayout layout = layoutText(number, FOCUS_FONT);
    if (!layout.count || layout.width <= 0) return {};

    int16_t sourceWidth = layout.width;
    int16_t sourceHeight = layout.bottom - layout.top;
    int16_t displayW = sourceWidth < FOCUS_VALUE_W ? sourceWidth : FOCUS_VALUE_W;
    int16_t displayH = ((int32_t) sourceHeight * displayW + sourceWidth / 2) / sourceWidth;
    // Valorax is intentionally wide; a small vertical stretch matches the reference display.
    displayH = ((int32_t) displayH * 11 + 5) / 10;
    if (displayH > FOCUS_VALUE_H) displayH = FOCUS_VALUE_H;
    int16_t textX = FOCUS_VALUE_X + (FOCUS_VALUE_W - displayW) / 2;
    int16_t textY = FOCUS_VALUE_Y + (FOCUS_VALUE_H - displayH) / 2;

    int16_t sourceXs[FOCUS_VALUE_W];
    uint8_t fractionsX[FOCUS_VALUE_W];
    for (int16_t x = 0; x < displayW; ++x) {
        int32_t mapped = ((int32_t) (2 * x + 1) * sourceWidth * 128) / displayW - 128;
        sourceXs[x] = mapped >> 8;
        fractionsX[x] = mapped & 0xFF;
    }

    uint16_t palette[16];
    for (uint8_t a = 0; a < 16; ++a) palette[a] = dim565(color, a * 17);
    uint16_t rowPixels[FOCUS_VALUE_W];
    for (int16_t screenY = FOCUS_VALUE_Y;
         screenY < FOCUS_VALUE_Y + FOCUS_VALUE_H; ++screenY) {
        memset(rowPixels, 0, sizeof(rowPixels));
        if (screenY >= textY && screenY < textY + displayH) {
            int16_t outputY = screenY - textY;
            int32_t mappedY = ((int32_t) (2 * outputY + 1) * sourceHeight * 128) / displayH - 128;
            int16_t sourceY = layout.top + (mappedY >> 8);
            uint8_t fractionY = mappedY & 0xFF;
            for (int16_t x = 0; x < displayW; ++x) {
                int16_t sourceX = sourceXs[x];
                uint8_t fractionX = fractionsX[x];
                uint32_t upper =
                    smoothAlphaAt(layout.runs, layout.count, FOCUS_FONT.pixels,
                                  sourceX, sourceY) * (256 - fractionX) +
                    smoothAlphaAt(layout.runs, layout.count, FOCUS_FONT.pixels,
                                  sourceX + 1, sourceY) * fractionX;
                uint32_t lower =
                    smoothAlphaAt(layout.runs, layout.count, FOCUS_FONT.pixels,
                                  sourceX, sourceY + 1) * (256 - fractionX) +
                    smoothAlphaAt(layout.runs, layout.count, FOCUS_FONT.pixels,
                                  sourceX + 1, sourceY + 1) * fractionX;
                uint8_t alpha = (upper * (256 - fractionY) +
                                 lower * fractionY + 32768) >> 16;
                rowPixels[textX - FOCUS_VALUE_X + x] = palette[alpha];
            }
        }
        tft->drawRGB565Row(FOCUS_VALUE_X, screenY, rowPixels, FOCUS_VALUE_W);
    }
    return {textX, textY, (uint16_t) displayW, (uint16_t) displayH};
}

void MainScreen::drawValue(uint8_t slot, uint8_t sensorMode, bool focus) {
    clearText(unitBounds[slot]);

    char number[12];
    formatValue(sensorMode, number, sizeof(number));
    uint16_t color = strcmp(number, "--") == 0 ? MUTED : sensorColor(sensorMode);

    if (focus) {
        drawSmoothValue(number, color);
        int16_t unitX = FOCUS_RIGHT - 18 - unitWidth(sensorMode, true);
        unitBounds[slot] = drawUnit(sensorMode, unitX, 261, true);
        return;
    }

    clearText(valueBounds[slot]);

    uint16_t unitW = unitWidth(sensorMode, false);
    const SmoothFont *valueFont = &SIDE_FONT;
    TextRect measured = measureSmoothText(number, *valueFont);
    constexpr uint16_t maxGroupWidth = SIDE_RIGHT - (SIDE_X + 18) - 8;
    if (measured.w + 5 + unitW > maxGroupWidth) {
        valueFont = &SIDE_COMPACT_FONT;
        measured = measureSmoothText(number, *valueFont);
    }
    if (measured.w + 5 + unitW > maxGroupWidth) {
        valueFont = &SIDE_TINY_FONT;
        measured = measureSmoothText(number, *valueFont);
    }

    uint8_t row = slot - 1;
    int16_t rowY = SIDE_Y[row];
    int16_t numberX = SIDE_X + 18;
    int16_t numberY = rowY + 29 + (31 - measured.h) / 2;
    valueBounds[slot] = drawSmoothText(tft, number, numberX, numberY,
                                       *valueFont, color);
    TextRect unitMetrics = measureSmoothText(sensorMode == MODE_PRESSURE ? "mmHg" :
                                              sensorMode == MODE_CO2 ? "ppm" :
                                              sensorMode == MODE_HUMIDITY ? "%" : "C",
                                              UNIT_FONT);
    int16_t unitY = numberY + measured.h - unitMetrics.h;
    unitBounds[slot] = drawUnit(sensorMode, numberX + measured.w + 5,
                                unitY, false);
}

void MainScreen::drawFocusPanel(bool fullPanel) {
    if (fullPanel) {
        tft->fillRect(0, CONTENT_Y - 2, DIVIDER_X - 2, SCREEN_H - CONTENT_Y + 2,
                      BACKGROUND);
        valueBounds[0] = {};
        unitBounds[0] = {};
        uint16_t color = sensorColor(mode);
        drawFocusBorder(tft, color);
        drawSoftVLine(tft, DIVIDER_X, CONTENT_Y,
                      CONTENT_BOTTOM - CONTENT_Y + 1, CYAN);
        drawLabel(mode, 20, 56, true);
    }
    drawValue(0, mode, true);
}

void MainScreen::drawSidePanel(uint8_t row, bool fullPanel) {
    int16_t rowY = SIDE_Y[row];
    uint8_t sensorMode = sideModes[row];
    if (fullPanel) {
        tft->fillRect(SIDE_X - 2, rowY - 2, SIDE_RIGHT - SIDE_X + 5,
                      SIDE_H[row] + 4, BACKGROUND);
        valueBounds[row + 1] = {};
        unitBounds[row + 1] = {};
        uint16_t color = sensorColor(sensorMode);
        drawSideBorder(tft, row, color);
        drawLabel(sensorMode, SIDE_X + 16, rowY + 7, false);
    }
    drawValue(row + 1, sensorMode, false);
}

bool MainScreen::sensorChanged(uint8_t sensorMode, const Sensors& previous) const {
    switch (sensorMode) {
        case MODE_TEMP_INT:
            return actualSensors.temperatureInternal != previous.temperatureInternal;
        case MODE_TEMP_EXT:
            return actualSensors.temperatureExternal != previous.temperatureExternal;
        case MODE_PRESSURE:
            return actualSensors.pressureMinus600 != previous.pressureMinus600;
        case MODE_CO2:
            return actualSensors.co2ppm != previous.co2ppm;
        case MODE_HUMIDITY:
            return actualSensors.humidity != previous.humidity;
        default:
            return false;
    }
}

void MainScreen::drawBattery() {
    const uint16_t millivolts = actualSensors.batteryMilliVolts;
    int16_t percent = millivolts <= 3350 ? 0 :
                      millivolts >= 3900 ? 100 :
                      ((uint32_t) (millivolts - 3350) * 100 + 275) / 550;
    if (percent == renderedBatteryPercent) {
        return;
    }
    renderedBatteryPercent = percent;

    tft->fillRect(404, 3, 76, 27, BACKGROUND);
    drawSoftHLine(tft, 408, 10, 22, GREEN);
    drawSoftHLine(tft, 408, 24, 22, GREEN);
    drawSoftVLine(tft, 408, 11, 13, GREEN);
    drawSoftVLine(tft, 429, 11, 13, GREEN);
    tft->fillRect(430, 14, 3, 7, GREEN);
    tft->fillRect(411, 13, (16 * percent + 50) / 100, 9, GREEN);

    char label[5];
    snprintf(label, sizeof(label), "%d%%", (int) percent);
    TextRect size = measureSmoothText(label, LABEL_FONT);
    drawSmoothText(tft, label, 477 - size.w, 10, LABEL_FONT, GREEN);
}

void MainScreen::readStandardSensors(Sensors& result) {
    result.humidity = sensorsProvider->readHumidity();
    result.temperatureInternal = sensorsProvider->readTempInternal();
    result.pressureMinus600 = sensorsProvider->readPressureMinus600();

    Serial.print(F("sensor standard complete: hum="));
    Serial.print(result.humidity);
    Serial.print(F(" ti="));
    Serial.print(result.temperatureInternal);
    Serial.print(F(" p="));
    if (result.pressureMinus600 == 0) {
        Serial.print(F("---"));
    } else {
        Serial.print(result.pressureMinus600 + 600);
    }
    Serial.println();
}

void MainScreen::loop() {
    unsigned long now = millis();
    Sensors previous = actualSensors;

    bool readStandardSensorsNow = !initialImmediateReadDone ||
        now - lastStandardSensorsPoll >= STANDARD_SENSORS_POLL_INTERVAL_MS;
    bool readBatteryNow = !initialImmediateReadDone ||
        now - lastBatteryPoll >= BATTERY_POLL_INTERVAL_MS;

    if (readStandardSensorsNow) {
        readStandardSensors(actualSensors);
        lastStandardSensorsPoll = millis();
    }
    if (readBatteryNow) {
        actualSensors.batteryMilliVolts = sensorsProvider->readBatteryMilliVolts();
        lastBatteryPoll = millis();
        Serial.print(F("sensor battery complete: "));
        Serial.print(actualSensors.batteryMilliVolts);
        Serial.println(F("mV"));
    }
    initialImmediateReadDone = true;

    bool temperaturesValid = actualSensors.temperatureInternal > 0 &&
                             actualSensors.temperatureInternal < 100 &&
                             actualSensors.temperatureExternal != TEMP_EXTERNAL_NONE;
    int16_t temperatureDifference = (int16_t) actualSensors.temperatureInternal -
                                    (int16_t) actualSensors.temperatureExternal;
    bool temperaturesFarApart = temperaturesValid &&
                                (temperatureDifference <= -EXTERNAL_TEMP_DIFFERENCE_THRESHOLD_C ||
                                 temperatureDifference >= EXTERNAL_TEMP_DIFFERENCE_THRESHOLD_C);
    unsigned long externalTempPollInterval = temperaturesFarApart
        ? EXTERNAL_TEMP_FAR_POLL_INTERVAL_MS
        : EXTERNAL_TEMP_NEAR_POLL_INTERVAL_MS;

    bool externalTempDue = !externalTempPollCompleted ||
        now - lastExternalTempPoll >= externalTempPollInterval;
    if (!externalTempReadPending && externalTempDue &&
        sensorsProvider->startExternalTemperatureRead()) {
        externalTempReadPending = true;
        Serial.println(F("sensor external-temp request started"));
    }

    bool co2Due = !co2PollCompleted || now - lastCo2Poll >= CO2_POLL_INTERVAL_MS;
    if (!co2ReadPending && co2Due && sensorsProvider->startCo2Read()) {
        co2ReadPending = true;
        Serial.println(F("sensor co2 request started"));
    }

    bool externalTempSucceeded = false;
    if (externalTempReadPending) {
        int8_t temperature;
        AsyncReadStatus status = sensorsProvider->pollExternalTemperature(temperature);
        if (status == AsyncReadStatus::Success || status == AsyncReadStatus::Failure) {
            externalTempReadPending = false;
            externalTempPollCompleted = true;
            lastExternalTempPoll = millis();
            if (status == AsyncReadStatus::Success) {
                actualSensors.temperatureExternal = temperature;
                externalTempSucceeded = true;
                Serial.print(F("sensor external-temp complete: "));
                Serial.println(temperature);
            } else if (actualSensors.temperatureExternal == TEMP_EXTERNAL_NONE) {
                Serial.println(F("sensor external-temp failed; no valid value"));
            } else {
                Serial.println(F("sensor external-temp failed; keeping last value"));
            }
        }
    }

    bool co2Succeeded = false;
    if (co2ReadPending) {
        uint16_t ppm;
        AsyncReadStatus status = sensorsProvider->pollCo2(ppm);
        if (status == AsyncReadStatus::Success || status == AsyncReadStatus::Failure) {
            co2ReadPending = false;
            co2PollCompleted = true;
            lastCo2Poll = millis();
            if (status == AsyncReadStatus::Success) {
                actualSensors.co2ppm = ppm;
                co2Succeeded = true;
                Serial.print(F("sensor co2 complete: "));
                Serial.print(ppm);
                Serial.println(F("ppm"));
            } else if (actualSensors.co2ppm == CO2_NONE) {
                Serial.println(F("sensor co2 failed; no valid value"));
            } else {
                Serial.println(F("sensor co2 failed; keeping last value"));
            }
        }
    }

    if (!dashboardDrawn) {
        if (externalTempPollCompleted && co2PollCompleted) {
            quality = calculateQuality(actualSensors);
            updateQuality();
            drawBattery();
            drawFocusPanel(true);
            for (uint8_t row = 0; row < 4; ++row) {
                drawSidePanel(row, true);
            }
            dashboardDrawn = true;
        }
    } else {
        if (readStandardSensorsNow || co2Succeeded) {
            quality = calculateQuality(actualSensors);
            updateQuality();
        }
        if (readBatteryNow) {
            drawBattery();
        }

        if (readStandardSensorsNow || externalTempSucceeded || co2Succeeded) {
            if (sensorChanged(mode, previous)) {
                drawFocusPanel(false);
            }
            for (uint8_t row = 0; row < 4; ++row) {
                if (sensorChanged(sideModes[row], previous)) {
                    drawSidePanel(row, false);
                }
            }
        }
    }

    delay(50);
}

void MainScreen::onTouch(uint16_t x, uint16_t y) {
    if (x >= SCREEN_W || y < CONTENT_Y || y >= SCREEN_H) {
        Serial.println(F("touch: event=none"));
        return;
    }

    uint8_t selected;
    if (x <= FOCUS_RIGHT) {
        selected = MODE_TEMP_INT;
    } else if (x >= SIDE_X && x <= SIDE_RIGHT) {
        if (y > CONTENT_BOTTOM) {
            Serial.println(F("touch: event=none"));
            return;
        }
        uint8_t row = y < 102 ? 0 : y < 174 ? 1 : y < 246 ? 2 : 3;
        selected = sideModes[row];
    } else {
        Serial.println(F("touch: event=none"));
        return;
    }

    if (selected == mode) {
        Serial.println(F("touch: event=already-selected"));
        return;
    }

    mode = selected;
    if (dashboardDrawn) {
        drawFocusPanel(true);
    }

    Serial.print(F("touch: event=select mode="));
    Serial.println(modeLabel(mode));
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
        sensors.co2ppm
    );

    uint8_t summary = (presWarnState + humWarnState + co2WarnState) / 3;
    if (summary > 85) return QUALITY_BEST;
    if (summary > 70) return QUALITY_GOOD;
    if (summary > 50) return QUALITY_AVG;
    if (summary > 25) return QUALITY_BAD;
    return QUALITY_WORST;
}

void MainScreen::updateQuality() {
    if (quality == renderedQuality) {
        return;
    }
    renderedQuality = quality;
    uint16_t color = quality >= QUALITY_GOOD ? GREEN :
                     quality == QUALITY_AVG ? YELLOW : RED;
    tft->drawFastHLine(STATUS_X, STATUS_Y - 1, STATUS_W, dim565(color, 64));
    tft->fillRect(STATUS_X, STATUS_Y, STATUS_W, STATUS_H, color);
    tft->drawFastHLine(STATUS_X, STATUS_Y + STATUS_H, STATUS_W, dim565(color, 64));
}

uint8_t MainScreen::getWarningRank(uint16_t warnMin, uint16_t urgentMin,
                                    uint16_t warnMax, uint16_t urgentMax,
                                    uint16_t value) {
    if (value <= urgentMin || value >= urgentMax) return 100;
    if (value > warnMin && value < warnMax) return 0;
    if (value <= warnMin) {
        return getWarningRank(warnMin, urgentMin, value);
    }
    return getWarningRank(warnMax, urgentMax, value);
}

uint8_t MainScreen::getWarningRank(uint16_t warn, uint16_t urgent, uint16_t value) {
    if (value >= urgent) return 100;
    if (value <= warn) return 0;
    float f = ((float) (value - warn)) / (urgent - warn);
    return (uint8_t) (100 * f);
}

#endif
