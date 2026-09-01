# ESP32-C6 Meteo Touch Station

Arduino meteo station firmware for an ESP32-C6, a 3.5 inch ILI9486 SPI TFT with XPT2046 resistive touch, and indoor climate sensors.

The UI keeps the original station structure: a compact metric strip, one large highlighted metric, a secondary metric strip, an air-quality indicator, and an always-visible battery level. There are no physical buttons and no firmware brightness control.

## Features

- ESP32-C6 target board.
- 480x320 landscape UI for the LCDWiki 3.5 inch RPi display.
- Incremental display-only startup mode for bring-up.
- ILI9486 TFT over SPI through the project-local display driver.
- XPT2046 resistive touch over the shared SPI bus, currently disabled for incremental bring-up.
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

The default wiring is split by board side. Display and touch use only the right-side breadboard pins, while sensors and battery measurement use the left-side breadboard pins.

| Side | Function | ESP32-C6 pin |
|---|---|---:|
| Right | TFT SCK / Touch SCK | `8` |
| Right | TFT MOSI / Touch MOSI | `19` |
| Right | TFT MISO / Touch MISO | `20` |
| Right | TFT LCD_CS | `18` |
| Right | TFT DC / RS | `14` |
| Right | TFT RST | `15` |
| Right | Touch TP_CS | `9` |
| Left | BMP280 SDA | `4` |
| Left | BMP280 SCL | `5` |
| Left | ESP32-C6 RX from MH-Z19 TX | `RX` |
| Left | ESP32-C6 TX to MH-Z19 RX | `TX` |
| Left | DHT11 data | `1` |
| Left | DS18B20 data | `2` |
| Left | Battery ADC | `0` |

Only these physical pins are used by default: left side `TX`, `RX`, `0`-`7`; right side `8`, `9`, `14`, `15`, `18`-`20`. Left-side pins `3`, `6`, and `7` remain spare.

If the selected Arduino ESP32-C6 board package does not define `TX` and `RX` symbols, the firmware falls back to GPIO `16` for the `TX` header pin and GPIO `17` for the `RX` header pin.

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

- DHT sensor library / DHT Unified
- Adafruit BMP280 Library
- Adafruit Unified Sensor

DS18B20 support is implemented in the sketch, so the external OneWire and DallasTemperature libraries are not required.

The current incremental display-only build only requires Adafruit GFX. Sensor libraries are needed again after `METEO_DISPLAY_ONLY` is set to `0` in `meteo_config.h`.

Open Serial Monitor at `115200` baud during display bring-up. The sketch prints display init steps and a heartbeat every 5 seconds.

The LCDWiki RPi-style ILI9486 adapter uses padded 16-bit SPI command transfers. The project-local display driver uses transfer mode `1`, which sends command/data bytes as `00,data`.

Use an Arduino ESP32 core version that supports ESP32-C6.

## Build And Upload

1. Open `meteo_touch.ino` in Arduino IDE.
2. Select an ESP32-C6 board, such as ESP32-C6-DevKitC-1.
3. Install the libraries listed above.
4. Confirm the display wiring in `meteo_touch.ino` and the sensor wiring in `sensors.h`.
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
