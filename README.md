# ESP32-C6 Meteo Touch Station

Arduino meteo station firmware for an ESP32-C6, a 3.5 inch ILI9486 SPI TFT with XPT2046 resistive touch, and indoor climate sensors.

The sensor UI uses a dark 480x320 instrument layout: a large focused metric on the left, four touch-selectable metrics on the right, a thin color-coded status line, and one battery indicator in the top-right corner. There are no physical buttons and no firmware brightness control.

## Features

- ESP32-C6 target board.
- 480x320 landscape UI for the LCDWiki 3.5 inch RPi display.
- Incremental display-only startup mode is available with `METEO_DISPLAY_ONLY` in `meteo_config.h`.
- Nine-point touch calibration is available with `METEO_TOUCH_CALIBRATION` in `meteo_config.h`; normal sensor mode is currently enabled.
- ILI9486 TFT over SPI through the project-local display driver.
- XPT2046 resistive touch over the shared SPI bus supports calibration, or selects the displayed metric in sensor mode.
- Touch-selectable metrics, all visible at once:
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
- DS18B20 conversions and MH-Z19 UART responses are acquired asynchronously, so their wait times do not pause touch handling or display updates.
- Physical sensors are polled by a cooperative round-robin scheduler. Only one sensor transaction runs at a time, and completed transactions are separated by one second to avoid coincident current spikes.
- Battery voltage is sampled every 15 seconds or later, with no sensor activity for at least two seconds before and after the ADC reading.
- Touch input is sampled cooperatively during long display transfers and sensor waits. Large screen fills are sent in bounded SPI chunks so short taps can be latched while the UI redraws.
- Green/yellow/red status line based on the existing humidity, pressure, and CO2 quality thresholds.

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
| Right | Touch TP_SO (SPI MISO) | `15` |
| Right | TFT LCD_CS | `14` |
| Right | TFT DC / RS | `20` |
| Right | TFT RST | `18` |
| Right | Touch TP_CS | `9` |
| Left | BMP280 SDA | `2` |
| Left | BMP280 SCL | `1` |
| Left | DHT11 data | `3` |
| Left | ESP32-C6 RX from MH-Z19 TX | `4` |
| Left | ESP32-C6 TX to MH-Z19 RX | `5` |
| Left | DS18B20 data | `6` |
| Left | Battery ADC | `0` |

The default wiring uses left-side GPIO `0`-`6` and right-side GPIO `8`, `9`, `14`, `15`, `18`-`20`. Left-side GPIO `7` and the `TX` and `RX` header pins remain spare. The MH-Z19 UART connections are crossed: sensor `TX` goes to ESP32-C6 GPIO `4` (RX), and sensor `RX` goes to ESP32-C6 GPIO `5` (TX).

These sensor GPIO assignments are fixed by the firmware and remain unchanged by asynchronous acquisition.

The LCD backlight is expected to be wired permanently on. Brightness control is intentionally removed.

### DS18B20 Wiring

For DS18B20, connect:

- DS18B20 `GND` to board `GND`
- DS18B20 `VDD` to `3V3`
- DS18B20 `DQ` / data to GPIO `6`
- `4.7k` pull-up resistor from `DQ` / data to `3V3`

Do not pull the data line up to `5V`. ESP32-C6 GPIO pins are 3.3 V logic inputs, so a `4.7k` resistor from data to `5V` can make the bus fail and can damage the ESP32-C6 pin.

For the most reliable simple wiring, power the DS18B20 from `3V3` and pull `DQ` up to `3V3`. If the DS18B20 is powered from `5V`, use a level shifter for `DQ`; a `3.3V` pull-up can be marginal for a sensor running from `5V`.

If Serial Monitor shows `sensor ds18b20: crc error` with `FF FF FF FF FF FF FF FF FF`, the sensor was detected but did not drive the scratchpad read. Check DS18B20 power, ground, data pin, pull-up to `3V3`, and avoid parasite-power mode. For long cables, try a stronger pull-up such as `2.2k` to `3V3`.

## Battery Measurement

The default firmware assumes a single-cell Li-ion or LiPo battery measured on GPIO `0` through a resistor divider:

- Top resistor: `100k`
- Bottom resistor: `100k`

The divider output must never exceed the ESP32-C6 ADC input range. With the default `100k / 100k` divider, a `4.2 V` cell is presented to the ADC as about `2.1 V`.
The firmware converts the measured ADC voltage back to battery voltage by multiplying it by `2`.

The UI estimates battery percentage linearly from the measured voltage: `3.35 V` is `0%`, `3.90 V` is `100%`, and values outside that range are clamped. The battery icon and percentage appear only in the header.
At startup the dashboard initially shows placeholders. The first battery reading waits for a two-second quiet period after sensor initialization, and sensor polling remains paused for another two seconds afterward. Later battery readings use the same quiet-window rule and may therefore occur later than their nominal 15-second interval when a sensor transaction is still active.

## Software Dependencies

Install these Arduino libraries before compiling:

- Adafruit BMP280 Library
- Adafruit Unified Sensor
- XPT2046_Touchscreen

The DHT11 and DS18B20 readers are implemented in the sketch, so the external DHT, OneWire, and DallasTemperature libraries are not required. The DHT11 reader uses bounded timing checks and safely reports no reading when the sensor is disconnected. DS18B20 conversion waits and CRC retries run as a cooperative state machine, while MH-Z19 response bytes are framed and validated incrementally from the UART.

The current build starts in sensor mode and requires Adafruit GFX, XPT2046_Touchscreen, and the sensor libraries listed above. Set `METEO_TOUCH_CALIBRATION` to `1` in `meteo_config.h` for touch calibration, or `METEO_DISPLAY_ONLY` to `1` for the display-only test screen; display-only mode takes precedence.

Open Serial Monitor at `115200` baud during display bring-up. The sketch prints display init steps and touch coordinates; the display-only test mode also prints a heartbeat every 5 seconds.

The LCDWiki RPi-style ILI9486 adapter uses padded 16-bit SPI command transfers. The project-local display driver uses transfer mode `1`, which sends command/data bytes as `00,data`.

Use an Arduino ESP32 core version that supports ESP32-C6.

## Build And Upload

1. Open `meteo_touch.ino` in Arduino IDE.
2. Select an ESP32-C6 board, such as ESP32-C6-DevKitC-1.
3. Install the libraries listed above.
4. Confirm the display wiring in `meteo_touch.ino` and the sensor wiring in `sensors.h`.
5. Build and upload.

## Touch

Touch input uses the XPT2046 controller on the display's shared SPI bus, with `TP_CS` on GPIO `9` and `TP_SO` on GPIO `15`. Open Serial Monitor at `115200` baud before calibrating. Calibration mode shows nine crosshairs in this order: top-left, top-right, bottom-right, bottom-left, top, right, bottom, left, and center. Tap each crosshair, hold briefly, then release before tapping the next one. After the ninth release, look for the `TOUCH CALIBRATION RESULTS - 9 POINTS` banner in Serial Monitor. The report contains all raw coordinates and pressures, a clearly marked block of seven `TOUCH_*` definitions to copy, and the largest fit error. It repeats every 15 seconds; you can also send `P` in Serial Monitor to print it immediately. Reset the board to repeat the calibration sequence. Keep the same `touch.setRotation(1)` setting when using those definitions.

In sensor mode, the left half initially focuses inside temperature. The four right-hand rows initially show CO2, humidity, pressure, and outside temperature in that order. Tapping CO2, humidity, or pressure shows that metric in the large left panel without changing the right-hand rows. Tapping the temperature row moves that temperature to the large panel and replaces the row with the other temperature source, so inside and outside temperature alternate there. Tapping the large left panel does nothing. Holding a panel does not repeat the selection; the header is display-only.

The display uses whole-degree temperatures, integer humidity, raw CO2 ppm, and integer mmHg pressure. The black instrument layout has a wide chamfered focus panel, four narrower independently bordered metric cards, and a compact battery header. The focused metric's unit is aligned against the right side of the large panel. Every displayed number, label, unit, border, and decorative line uses RGB565 antialiasing. The top status line is green, yellow, or red, with no status text. Colors and panel coordinates are near the top of `main_screen.cpp`.

The four-bit coverage fonts in `smooth_digits.h` and `smooth_text.h` are generated from the repository's `font.otf` and stored in flash, so no font library is needed at runtime. The generator accepts either TTF or OTF input. With Pillow installed, run `python3 tools/generate_smooth_digits.py` to rebuild from `font.otf`, or pass another font path as the first argument. Use `--size 166` to change the focus digit source size. Check the source font's license before distributing its generated bitmap data.

Touch calibration constants are defined near the top of `meteo_touch.ino`:

```cpp
#define TOUCH_MIN_X 300
#define TOUCH_MAX_X 3900
#define TOUCH_MIN_Y 300
#define TOUCH_MAX_Y 3900
#define TOUCH_SWAP_XY false
#define TOUCH_INVERT_X true
#define TOUCH_INVERT_Y false
```

Replace these definitions with the calibration output, then set `METEO_TOUCH_CALIBRATION` to `0` and upload again. The UI hit boxes are already defined for the 480x320 landscape layout. A large reported fit error can indicate an inaccurate tap, noisy readings, or a panel that needs a more detailed mapping.

`TOUCH_PRESSURE_MIN` filters idle noise from the touch controller. The default `1200` is set above the observed idle pressure (about `1020`); lower it only if intentional presses do not reach that threshold.

## License

This project is licensed under the GNU General Public License version 3.

GPL-3.0 is a copyleft license. If you distribute modified versions of this firmware, you must provide the corresponding source code under the same license terms.

See the GNU GPL v3 text at:

https://www.gnu.org/licenses/gpl-3.0.en.html
