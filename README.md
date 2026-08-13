# ESP32-C6 Meteo Touch Station

Arduino meteo station firmware for an ESP32-C6, a 3.5 inch ILI9486 SPI TFT with XPT2046 resistive touch, and indoor climate sensors.

The UI keeps the original station structure: a compact metric strip, one large highlighted metric, a secondary metric strip, an air-quality indicator, and an always-visible battery level. There are no physical buttons and no firmware brightness control.

## Features

- ESP32-C6 target board.
- 480x320 landscape UI for the LCDWiki 3.5 inch RPi display.
- ILI9486 TFT over SPI.
- XPT2046 resistive touch over the shared SPI bus.
- Touch-selectable metrics:
  - internal temperature
  - humidity
  - CO2
  - external temperature
  - pressure
- Always-visible, non-clickable battery percentage.
- Sensors:
  - MH-Z19 CO2 sensor
  - DHT11 temperature and humidity sensor
  - BMP280 pressure and temperature sensor
  - optional DS18B20 external temperature sensor
- Air-quality indicator based on humidity, pressure, and CO2 thresholds.

## Hardware

### Controller

- ESP32-C6 development board compatible with ESP32-C6-DevKitC-1 style GPIO availability.

### Display

- LCDWiki 3.5 inch RPi Display
- Resolution: 320x480 native, used as 480x320 landscape
- LCD driver: ILI9486
- Touch controller: XPT2046
- Interface: SPI

### Default Wiring

| Function | ESP32-C6 GPIO |
|---|---:|
| TFT SCK / Touch SCK | `21` |
| TFT MOSI / Touch MOSI | `19` |
| TFT MISO / Touch MISO | `20` |
| TFT LCD_CS | `18` |
| TFT DC / RS | `10` |
| TFT RST | `11` |
| Touch TP_CS | `3` |
| BMP280 SDA | `23` |
| BMP280 SCL | `22` |
| ESP32-C6 RX from MH-Z19 TX | `16` |
| ESP32-C6 TX to MH-Z19 RX | `17` |
| DHT11 data | `1` |
| DS18B20 data | `2` |
| Battery ADC | `0` |

The LCD backlight is expected to be wired permanently on. Brightness control is intentionally removed.

## Battery Measurement

The default firmware assumes a single-cell Li-ion or LiPo battery measured through a resistor divider:

- Top resistor: `100k`
- Bottom resistor: `100k`
- Empty voltage: `3300 mV`
- Full voltage: `4200 mV`

The divider output must never exceed the ESP32-C6 ADC input range. With the default `100k / 100k` divider, a `4.2 V` cell is presented to the ADC as about `2.1 V`.

Battery percentage is a simple clamped linear estimate from `3300 mV` to `4200 mV`.

## Software Dependencies

Install these Arduino libraries before compiling:

- GFX Library for Arduino
- XPT2046_Touchscreen
- DHT sensor library / DHT Unified
- Adafruit BMP280 Library
- Adafruit Unified Sensor
- OneWire
- DallasTemperature

Use an Arduino ESP32 core version that supports ESP32-C6.

## Build And Upload

1. Open `meteo_touch.ino` in Arduino IDE.
2. Select an ESP32-C6 board, such as ESP32-C6-DevKitC-1.
3. Install the libraries listed above.
4. Confirm the wiring constants at the top of `meteo_touch.ino` and in `sensors.h`.
5. Build and upload.

## Touch Calibration

Touch calibration constants are defined near the top of `meteo_touch.ino`:

```cpp
#define TOUCH_MIN_X 300
#define TOUCH_MAX_X 3900
#define TOUCH_MIN_Y 300
#define TOUCH_MAX_Y 3900
#define TOUCH_SWAP_XY true
#define TOUCH_INVERT_X false
#define TOUCH_INVERT_Y true
```

If touch is mirrored, rotated, or offset on your hardware, adjust only these constants first. The UI hit boxes are already defined for the 480x320 landscape layout.

## License

This project is licensed under the GNU General Public License version 3.

GPL-3.0 is a copyleft license. If you distribute modified versions of this firmware, you must provide the corresponding source code under the same license terms.

See the GNU GPL v3 text at:

https://www.gnu.org/licenses/gpl-3.0.en.html
