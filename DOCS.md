# Firmware Documentation

This document describes the implementation and runtime behavior of the ESP32-C6 Meteo Touch Station. For hardware requirements, wiring, dependencies, and first-time installation, start with the [README](README.md).

## Architecture

The sketch uses one cooperative main loop. It does not create background tasks: sensor state machines, battery acquisition, display updates, and touch input all advance in short steps.

| File | Responsibility |
|---|---|
| `meteo_touch.ino` | Startup, mode selection, display/touch pins, touch mapping, and main-loop dispatch |
| `meteo_config.h` | Display-only, calibration, and demo compile-time settings |
| `main_screen.*` | Dashboard rendering, touch behavior, polling scheduler, thresholds, and aggregate status |
| `sensors.*` | Sensor drivers, validation, corrections, and battery ADC acquisition |
| `MHZ19_uart.*` | Incremental MH-Z19 request and response handling |
| `meteo_display.*` | Project-local ILI9486 SPI driver |
| `touch_calibration.*` | Interactive nine-point calibration and fitted mapping output |
| `smooth_digits.h`, `smooth_text.h` | Generated flash-resident antialiased glyphs |
| `tools/generate_smooth_digits.py` | Font generation utility |

## Operating Modes

The three modes are selected at compile time in `meteo_config.h`.

### Normal mode

When all mode flags are `0`, the firmware initializes the display, touch controller, all configured sensors, and the battery ADC. Measurements start as invalid and appear as placeholder lines until a valid sample is available.

### Display-only mode

`METEO_DISPLAY_ONLY=1` initializes only the display and draws a color/test screen. Touch and sensors are not initialized. Serial Monitor prints a heartbeat every five seconds.

This mode has the highest precedence.

### Touch-calibration mode

`METEO_TOUCH_CALIBRATION=1` initializes the display and touch controller but not the dashboard or sensors. It runs the calibration workflow described below.

Display-only mode overrides this mode.

### Demo mode

`METEO_DEMO_MODE=1` shows these defaults immediately:

- Internal temperature: `23°C`
- Humidity: `45%`
- CO2: `600 ppm`
- Pressure: `720 mmHg`
- Battery: `85%`

DHT11, BMP280, MH-Z19, and battery acquisition are disabled. The DS18B20 remains live. External temperature displays the fixed internal temperature while the DS18B20 is unavailable or differs from it by less than `10°C`; a difference of exactly `10°C` or more displays the live external reading.

Display-only and touch-calibration modes both override demo mode.

## Dashboard and Touch Behavior

The 480x320 dashboard contains:

- A large focus panel on the left, initially showing internal temperature.
- Four right-hand tiles, initially ordered CO2, humidity, pressure, and external temperature.
- An aggregate status line across the top.
- A non-clickable battery indicator in the upper-right corner.

Tapping a CO2, humidity, or pressure tile displays that metric in the focus panel without changing the right-hand order. Tapping the temperature tile moves that temperature into focus and replaces the tile with the other temperature source. The focus panel and header are not interactive. A held press produces only one selection; a new event is accepted after release.

Values are rendered as whole-degree temperatures, whole-percent humidity, integer CO2 ppm, and integer mmHg pressure. Invalid measurements without a previous valid sample render as a placeholder line.

### Colors and thresholds

The focus panel is always pink. Right-hand tiles are cyan in their normal range. Humidity, pressure, and CO2 tiles become yellow at warning levels and red at urgent levels.

| Metric | Cyan | Yellow | Red |
|---|---:|---:|---:|
| Humidity | `41–59%` | `31–40%`, `60–69%` | `≤30%`, `≥70%` |
| Pressure | `691–749 mmHg` | `671–690`, `750–769 mmHg` | `≤670`, `≥770 mmHg` |
| CO2 | `400–999 ppm` | `1000–1999 ppm` | `≥2000 ppm` |

A metric with no stored valid value keeps its tile cyan. Later acquisition failures retain the previous value and color. The focus panel does not change to a warning color even when it shows a warning metric.

The aggregate status is calculated only after humidity, pressure, and CO2 all have valid values. Each input is converted to a quality score using the same warning/urgent thresholds, and the three scores are averaged. The resulting bands are:

| Average score | Internal quality | Status color |
|---:|---|---|
| `>85` | Best | Green |
| `>70` | Good | Green |
| `>50` | Average | Yellow |
| `>25` | Bad | Red |
| `≤25` | Worst | Red |

UI colors, geometry, and warning constants are defined in `main_screen.cpp` and `main_screen.h`.

## Sensor Scheduling

The scheduler permits only one sensor transaction at a time. After a transaction completes, another sensor cannot start for one second. Due tasks are selected round-robin so simultaneous deadlines do not repeatedly favor one sensor.

| Source | Nominal interval | Notes |
|---|---:|---|
| DHT11 | 15 seconds | Humidity and cached internal-temperature update |
| BMP280 | 15 seconds | Pressure and cached internal-temperature update |
| DS18B20 | 15 seconds | Used while temperatures are unavailable or differ by less than `5°C` |
| DS18B20 | 1 second | Used while valid internal/external temperatures differ by at least `5°C` |
| MH-Z19 | 60 seconds | Incremental UART request/response |
| Battery ADC | At least 15 seconds | Also requires the quiet windows described below |

Intervals are measured from task completion. Actual reads can occur later when another transaction is active or a guard interval has not expired.

DS18B20 conversion is asynchronous: the scheduler waits 750 ms without blocking the rest of the loop, then reads and CRC-checks the scratchpad. It makes up to three attempts with 20 ms between retries. MH-Z19 UART bytes are also framed and validated incrementally rather than waiting synchronously for a complete response.

Touch is serviced between sensor steps and between bounded display transfers. Large display fills are split into chunks of at most 2048 pixels so a short press can be latched during a redraw.

### Battery quiet windows

Battery measurement is deliberately separated from sensor activity to reduce load-related ADC error:

1. A due battery read waits until no sensor has completed for at least two seconds.
2. The ADC takes eight millivolt samples and averages them.
3. Sensor polling remains paused for another two seconds.

The first battery reading follows the same rule, with sensor initialization treated as recent activity. A reading can therefore occur later than its nominal 15-second interval.

## Measurement Validation and Retention

Each measurement contains a value and an explicit validity flag. Out-of-range, malformed, unavailable, or failed readings are invalid.

| Sensor | Metric | Inclusive accepted range | Display form |
|---|---|---:|---|
| DHT11 | Relative humidity | `20–90% RH` | Whole percent after correction |
| DHT11 | Temperature | `0–50°C` | Whole degrees after correction |
| BMP280 | Temperature | `−40–85°C` | Whole degrees after correction |
| BMP280 | Pressure | `30000–110000 Pa` (`300–1100 hPa`) | Whole mmHg after conversion |
| DS18B20 | Temperature | `−55–125°C` | Whole degrees after correction |
| MH-Z19B | CO2 | `400–5000 ppm` | Integer ppm |

The pressure conversion uses `133.322387415 Pa` per mmHg. A new valid sample replaces the stored value. An invalid sample leaves the last valid value on screen; if no valid value has ever been received, the placeholder remains.

The operating ranges follow the selected variants' documentation: [Aosong DHT11](https://aosong.com/userfiles/files/media/DHT11%E6%95%B0%E5%AD%97%E6%B8%A9%E6%B9%BF%E5%BA%A6%E4%BC%A0%E6%84%9F%E5%99%A8%E6%A8%A1%E5%9D%97.pdf), [Bosch BMP280](https://www.bosch-sensortec.com/products/environmental-sensors/pressure-sensors/bmp280/), [Analog Devices DS18B20](https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf), and [Winsen MH-Z19B](https://www.winsen-sensor.com/d/files/manual/mh-z19b.pdf).

### Internal temperature and corrections

Internal temperature is derived from the currently valid cached DHT11 and BMP280 temperatures. If both are available, their corrected values are averaged; if only one is available, that source is used.

The firmware applies time-based compensation for local sensor heating:

- For the first three minutes, DHT11 humidity and internal temperature have no compensation.
- After three minutes, a half-minute lookup curve gradually approaches full compensation.
- Full compensation adds `3.5% RH` to humidity and subtracts `2.7°C` from internal temperature.
- DS18B20 compensation adds `2°C` during minute 0, `1.7°C` during minute 1, `1.3°C` during minute 2, and `1°C` afterward.

Validation occurs both around decoding and after correction where applicable.

### MH-Z19 startup handling

The firmware disables MH-Z19 automatic calibration. Before the sensor has produced its first accepted reading, results of exactly `400 ppm` or `500 ppm` are ignored as startup values. After any other valid in-range reading initializes the stream, both `400 ppm` and `500 ppm` are accepted normally.

## Battery Calculation

The default `100k / 100k` divider has a ratio of 2. The firmware averages eight ADC readings, converts the divider output to millivolts, and multiplies it by the configured divider ratio.

Battery percentage is estimated linearly:

- `≤3.35 V`: `0%`
- `≥3.90 V`: `100%`
- Between those points: linear interpolation, rounded to the nearest percent

The result is clamped to `0–100%`. This is a display estimate, not a battery fuel-gauge model.

## Touch Calibration

Touch uses the XPT2046 controller on the shared SPI bus. The normal input path polls every 5 ms, requires pressure of at least `1200`, and applies a 20 ms release debounce.

The checked-in mapping in `meteo_touch.ino` is:

```cpp
#define TOUCH_MIN_X 1724
#define TOUCH_MAX_X 3890
#define TOUCH_MIN_Y 968
#define TOUCH_MAX_Y 3057
#define TOUCH_SWAP_XY false
#define TOUCH_INVERT_X true
#define TOUCH_INVERT_Y false
```

These values are panel-specific. To calibrate another unit:

1. Open Serial Monitor at `115200` baud.
2. Set `METEO_TOUCH_CALIBRATION` to `1` in `meteo_config.h`, then build and upload.
3. Tap the targets in this order: top-left, top-right, bottom-right, bottom-left, top edge, right edge, bottom edge, left edge, center.
4. Hold each target briefly so at least three samples are captured, then release before the next target.
5. After the ninth release, find the `TOUCH CALIBRATION RESULTS - 9 POINTS` report in Serial Monitor.
6. Copy the seven definitions between the marked begin/end lines into `meteo_touch.ino`.
7. Keep both the display and touch controller at rotation `1`.
8. Set `METEO_TOUCH_CALIBRATION` back to `0`, rebuild, and upload.

The report repeats every 15 seconds after completion. Send `P` or `p` in Serial Monitor to print it immediately. Reset the board to restart the sequence.

A reported raw span below 200 indicates failed calibration. A large fit-error value can indicate an inaccurate tap, noisy measurements, incorrect wiring, or a panel that needs a more detailed mapping. Lower `TOUCH_PRESSURE_MIN` only when intentional presses do not reach the threshold; reducing it too far admits idle noise.

## Display Driver and Fonts

The LCDWiki RPi-style ILI9486 adapter uses padded 16-bit SPI command transfers. `meteo_touch.ino` selects transfer mode `1`, which sends command and data bytes as `00,data`. The project-local driver uses 8 MHz SPI and RGB565 pixels.

Dashboard digits and labels use four-bit coverage bitmaps stored in flash. They are generated from `font.otf`, so no font library is required at runtime.

To regenerate them, install Pillow and run:

```sh
python3 tools/generate_smooth_digits.py
```

Pass a TTF or OTF path as the first argument to use another font. Use `--size 166` to set the focus-digit source size. The script rewrites `smooth_digits.h` and `smooth_text.h`. Check the source font's license before distributing generated bitmap data.

## Serial Diagnostics and Troubleshooting

Use Serial Monitor at `115200` baud. Startup output reports display initialization, selected pins, sensor discovery, scheduler activity, touch coordinates, and acquisition failures.

### Blank or incorrect display

- Verify SCK, MOSI, LCD_CS, DC, and RST against the README wiring table.
- Confirm the module is the LCDWiki RPi-style ILI9486 adapter expected by the padded transfer mode.
- Use `METEO_DISPLAY_ONLY=1` to isolate display initialization from touch and sensor code.
- Confirm Serial Monitor reports a `480x320` display after rotation.

### Touch does not respond correctly

- Verify TP_CS on GPIO `9` and TP_SO on GPIO `15`.
- Watch raw coordinates and pressure in Serial Monitor.
- Run the nine-point calibration for the specific panel.
- If presses are missing, compare their pressure with `TOUCH_PRESSURE_MIN`; if idle events appear, raise the threshold.

### DS18B20 CRC errors

If Serial Monitor shows `sensor ds18b20: crc error` with `FF FF FF FF FF FF FF FF FF`, the device was detected but did not drive the scratchpad read.

- Check power, ground, GPIO `6`, and the pull-up to `3V3`.
- Avoid parasite-power mode.
- Keep the data pull-up at 3.3 V, not 5 V.
- For a long cable, try a stronger pull-up such as `2.2k` to `3V3`.
- The implementation supports one DS18B20 on the bus.

### Missing or invalid sensor readings

- Placeholders at startup are expected until each sensor returns a valid sample.
- A later error deliberately retains the previous valid value.
- BMP280 discovery tries addresses `0x76` and `0x77`.
- DHT11 timing waits are bounded, and reads respect its minimum two-second interval.
- Initial MH-Z19 readings of exactly 400 or 500 ppm are intentionally filtered until initialization completes.
- Scheduler and quiet-window messages explain why a due read may be delayed.
