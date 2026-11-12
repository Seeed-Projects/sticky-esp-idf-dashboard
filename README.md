# reTerminal Sticky Dashboard Demo

[中文说明](README-ZH.md)

An ESP-IDF demo for developing with the onboard hardware of the reTerminal
Sticky. It combines the e-paper display, buttons, touch panel, environmental
sensor, RTC, battery gauge, MicroSD card, IMU, microphone, buzzer, and deep
sleep in one working application.

The project can be flashed as a complete dashboard or used as a reference when
building a custom application for the Sticky. Hardware access, application
state, and page rendering are kept separate so individual modules can be reused
without copying the whole UI.

One firmware automatically detects **SSD1677 or SSD2677** and selects the
appropriate display driver. Pages use the same Canvas and refresh APIs on
both variants; no manual controller selection is required.

The three ESP-IDF tutorials are [Basics](https://www.seeedstudio.com/sticky/docs/en/device-guide/esp-basics/),
[Pages and peripherals](https://www.seeedstudio.com/sticky/docs/en/device-guide/esp-pages/),
and [Refresh and sleep](https://www.seeedstudio.com/sticky/docs/en/device-guide/esp-refresh/).

## Features

The dashboard contains six pages:

| Page | Function |
| --- | --- |
| Home | Text, lines, rectangles, and four-level grayscale |
| Sensor | SHT40 temperature and humidity |
| Battery | BQ27220 state of charge and external-power status |
| Note | Text read from `/TEST.TXT` on a MicroSD card |
| IMU | Gravity arrow driven by the LSM6DS3 accelerometer |
| Microphone | One-second PDM sample with RMS, peak, and level bar |

The PCF8563 time is shown in the shared status bar on the Sensor, Battery,
Note, IMU, and Microphone pages. It is refreshed once per minute with a
black-and-white partial update. The Demo also supports button and touch
navigation, buzzer feedback, and deep sleep with page-state restoration.

## Requirements

- reTerminal Sticky
- USB-C cable
- ESP-IDF v5.4
- A FAT-formatted MicroSD card for the Note page (optional)

Project configuration:

- Target: ESP32-S3
- Flash: 32 MB
- PSRAM: 80 MHz Octal PSRAM
- Logical display resolution: 800 x 480

If ESP-IDF is not installed, follow the official
[ESP-IDF Get Started guide](https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32s3/get-started/).

## Build and Run

Open an ESP-IDF v5.4 terminal in the project directory, then run:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p PORT flash monitor
```

Replace `PORT` with the serial port of the device, for example `COM5` on
Windows. Press `Ctrl+]` to exit the serial monitor.

If generated build files become inconsistent, remove only the generated
`build/` directory and build again.

### Prepare the MicroSD card

Format the card with a FAT filesystem and create this file in its root:

```text
TEST.TXT
```

Example content:

```text
Hello Sticky! Happy coding!
```

The Demo reads up to **256 bytes** and displays up to six lines, with 52
single-byte characters per line. Printable ASCII is recommended; unsupported
bytes are replaced with `?`. Longer content is only partially displayed and
file truncation is recorded in the log. Successful reads show the text without
a footer. Missing cards, mount failures, missing files, empty/blank files, and
read failures have separate messages. The Demo never formats a card automatically.

## Controls

| Input | Action |
| --- | --- |
| `UP` button | Previous page |
| `DOWN` button | Next page |
| Swipe right | Previous page |
| Swipe left | Next page |
| Short press `AI` | Refresh the current page |
| Long press `AI` for about 2 seconds | Beep and enter deep sleep |
| Press `AI` while sleeping | Wake and restore the previous page |

Touch accepts horizontal swipes only. Taps, short movements, and mostly
vertical gestures are ignored to reduce accidental page changes.

Navigation requests are coalesced until the accepted page change finishes;
rapid swipes do not accumulate a long sequence of delayed page changes. E-paper
waveforms still take time to complete. A held AI button used to wake the device
does not also request sleep: release it before starting another press. Wake-up
has a beep; it does not require a one-second hold.

## Project Layout

```text
sticky-esp-idf-dashboard/
|-- components/                 Third-party and panel components
|   |-- bq27220/
|   |-- button/
|   |-- gt911/
|   `-- seeed_epaper/
`-- main/
    |-- main.cpp                Startup and main event loop
    |-- pin_config.h            Sticky pin and I2C address definitions
    |-- board/                  Shared power, GPIO, and sensor I2C resources
    |-- devices/                Hardware-facing device modules
    |-- app/                    State, data updates, events, and navigation
    |-- pages/                  Page rendering functions
    `-- ui/                     Canvas, font, and shared status bar
```

The main data flow is:

```text
Hardware -> devices -> app_data -> AppState -> pages -> Canvas -> display
```

- `board/` owns resources shared by multiple devices.
- `devices/` initializes hardware and returns readings.
- `app/app_data.cpp` copies device readings into `AppState`.
- `pages/` reads `AppState` and draws without accessing hardware.
- `app/app.cpp` handles events, navigation, rendering, and display refresh.

`main.cpp` shows the required initialization order and is the best starting
point for understanding the complete application.

## Device APIs

Each onboard peripheral has a small API under `main/devices/`:

| Module | Main API | Purpose |
| --- | --- | --- |
| `sticky_display` | `init`, `canvas`, `refresh`, `refresh_monochrome`, `refresh_partial`, `standby`, `sleep` | E-paper framebuffer and three refresh modes |
| `sticky_buttons` | `init` | UP, DOWN, and AI button events |
| `sticky_touch` | `init`, `stop` | GT911 swipe events |
| `sticky_buzzer` | `init`, `beep` | Short operation feedback |
| `sticky_sht40` | `init`, `read` | Temperature and humidity |
| `sticky_rtc` | `init`, `initialize_time`, `read` | Preserve valid time; initialize invalid time; read date/time |
| `sticky_battery` | `init`, `read` | Battery state of charge |
| `sticky_charger` | `init`, `read` | External-power status |
| `sticky_sdcard` | `init`, `read_text` | Read a text file from MicroSD |
| `sticky_imu` | `init`, `start_monitoring`, `stop_monitoring`, `get_state` | Stable device orientation and sleep preparation |
| `sticky_microphone` | `init`, `capture` | One-shot PDM audio measurement |
| `sticky_power` | `enter_deep_sleep` | Deep-sleep entry and AI wake-up |

Refer to the matching `.h` file for the exact types and return values.
Runtime functions return `esp_err_t`; callers should check the result before
using a reading.

### Reading an I2C sensor

The battery gauge, RTC, SHT40, and IMU use the shared sensor bus created by the
board layer. Pass that existing bus handle to the device initializer:

```cpp
ESP_ERROR_CHECK(board_init());
ESP_ERROR_CHECK(sticky_sht40_init(board_sensor_i2c_bus()));

Sht40Reading reading = {};
if (sticky_sht40_read(reading) == ESP_OK) {
    printf("Temperature: %.1f C\n", reading.temperature_c);
    printf("Humidity: %.1f %%\n", reading.humidity_percent);
}
```

Do not create another I2C bus on GPIO0/GPIO1 for an individual sensor.

### Drawing on the e-paper display

The display module owns the framebuffer and exposes it through `Canvas`:

```cpp
ESP_ERROR_CHECK(sticky_display_init());

Canvas *canvas = sticky_display_canvas();
canvas->clear(GrayLevel::White);
canvas->draw_text(60, 80, "Hello Sticky", 3, GrayLevel::Black);
canvas->draw_line(60, 130, 740, 130, GrayLevel::DarkGray);
canvas->draw_rect(60, 170, 260, 120, GrayLevel::Black);

ESP_ERROR_CHECK(sticky_display_refresh());
```

All UI code uses an 800 x 480 top-left-origin coordinate system. The required
180-degree physical panel rotation is already handled by `sticky_display`.

The available canvas colors are:

```cpp
GrayLevel::Black
GrayLevel::DarkGray
GrayLevel::LightGray
GrayLevel::White
```

One logical Canvas area can be updated without refreshing the complete screen:

```cpp
// Change only the intended black-and-white Canvas pixels first.
ESP_ERROR_CHECK(sticky_display_refresh_partial());
```

The partial-refresh API performs the physical 180-degree conversion internally
and submits a full-screen monochrome comparison frame. Use it on a Canvas that
contains only `Black` and `White`, with only the intended area changed. A logical
partial update does not imply a small SPI transfer, especially on SSD2677.
The Dashboard uses this API only for RTC time on its black-and-white pages.

### Rendering application data

The complete Demo does not let pages read hardware directly. A device reading
is copied into `AppState`, then rendered by a page:

```text
sticky_sht40_read()
-> update_environment(state)
-> state.environment
-> sensor_page_render(canvas, state)
-> sticky_display_refresh_monochrome()
```

This separation keeps pages predictable and makes hardware modules reusable in
applications with a different UI.

To add another page, add a `PageId`, create a `pages/*_page.cpp/.h` renderer,
connect it in `app/app.cpp`, and add its source file to `main/CMakeLists.txt`.
Use `app_data` when the page needs hardware data.

## Important Hardware Notes

### Shared SPI bus

The e-paper display and MicroSD card share SPI2 signal lines and use different
chip-select pins. The display initializes SPI2 first. SD access temporarily
mounts the card on the existing bus, reads and closes the file, then unmounts
the card before the display is refreshed.

The SD module must not free SPI2 because the display remains attached to it.
When changing the SPI bus configuration, keep every unused data pin set to
`-1`; a zero-initialized unused pin can claim GPIO0 and break the sensor I2C
clock.

### I2C buses

- I2C1 on GPIO0/GPIO1: BQ27220, PCF8563, SHT40, and LSM6DS3.
- I2C0 on GPIO2/GPIO3: GT911 touch controller.

Keep the GT911 on its dedicated bus unless its reset, address selection, and
coordinate conversion are also reviewed.

### Display refresh

The Home page demonstrates a four-level grayscale full refresh:

```cpp
sticky_display_refresh();
```

The Sensor, Battery, Note, IMU, Microphone, and deep-sleep pages use a
black-and-white full refresh:

```cpp
sticky_display_refresh_monochrome();
```

The RTC time area on the black-and-white pages is updated once per minute with
`sticky_display_refresh_partial()`. The Home page does not display the status
bar time. Sensor, Battery, and Note data reload on entry or AI refresh; IMU
orientation changes update its page, and microphone capture requires AI.
Page changes
and AI-button refreshes use the full-refresh mode assigned to the current page.

`app/app_clock.cpp` aligns the first timer expiration to the next RTC minute
and posts `AppEvent::ClockMinuteTick`. The callback never accesses the RTC or
display directly; the normal app event loop performs the read and partial
refresh.

Repeated partial updates can gradually leave light ghosting on an e-paper
panel. After 20 successful partial refreshes, the next partial request becomes
a full monochrome refresh; refresh failures also require full-frame recovery.
Short-press AI to perform a normal full refresh when needed. Brief
flashing or old-image visibility during a full refresh is normal for e-paper.

After one second without app events, the app requests panel standby. SSD1677
Gray4 skips this extra step because its refresh sequence already shuts down
analog power and clock. The official component resumes or reinitializes on
the next refresh. The one-second interval is a Demo policy, not panel timing.

### RTC time

At startup, valid RTC time is retained. If readable clock data is invalid
(VL flag, invalid BCD or date), the Demo writes **2026-01-01 00:00:00** once
and checks it by reading back. Missing hardware and I2C errors do not trigger a
write. The example epoch demonstrates a running clock; it is not the real time.
Existing RTC values are displayed directly without timezone conversion or
network synchronization. Keeping the RTC powered preserves its running time
across resets and reflashing.

Battery Page's **External power** field reports the external-power input,
not measured battery charging current or full-charge state.

### MicroSD reload behavior

The Note page reads `TEST.TXT` whenever the page is entered or refreshed with
the AI button. The filesystem is mounted only for the read operation, so the
card can be removed or its file can be changed between reads.
If unmount cleanup fails, the page asks for a restart before further SD access;
the Demo does not reuse a potentially freed card object or release shared SPI2.

### Deep sleep

Before sleeping, the application saves versioned/checksummed `AppState` in RTC
memory, renders a monochrome sleep page, stops the minute timer, pauses IMU and
touch polling, and sends the panel sleep command. **EPD_EN stays enabled**;
controller sleep, high-voltage power-off, and removing the module supply are
different actions. GPIO4 wakes the ESP32-S3 and the saved page is restored.
An incompatible/invalid saved state falls back to Home. Preparation failures
are reported and the app attempts to resume normal operation. This Demo does
not claim a specified sleep-current target.

### Serial output and errors

INFO logs show controller detection, page changes, readings, refresh results
with the actual mode and measured refresh duration, and sleep/wake events.
Routine RTC reads, standby and intermediate sensor steps use DEBUG. Refresh
duration excludes preceding sensor reads and page rendering. GPIO configuration
and SD protocol-probe INFO logs are suppressed; warnings and errors remain.
Optional peripheral failures are shown on the corresponding page while other
functions continue; core board/display/input initialization failures stop startup.

## Hardware Check

For a quick hardware check, confirm controller detection, cycle through all six
pages, return to Home and inspect all four shades, reload the SD text, capture
audio with AI, watch a minute update on a monochrome page, then sleep and wake.
Holding AI through wake should not send the device back to sleep. Use the same
firmware for SSD1677 and SSD2677; verify the image on the device as well as logs.

## Pin Definitions

All verified pin assignments and I2C addresses are defined in
[`main/pin_config.h`](main/pin_config.h). Use that file as the source of truth
instead of duplicating pin numbers in application code.

Key shared connections:

| Function | Pins |
| --- | --- |
| Sensor I2C1 | SCL GPIO0, SDA GPIO1 |
| Touch I2C0 | SCL GPIO2, SDA GPIO3 |
| UP / DOWN / AI | GPIO5 / GPIO6 / GPIO4 |
| E-paper SPI | MOSI GPIO14, MISO GPIO12, CLK GPIO13, CS GPIO15 |
| MicroSD | CS GPIO8, detect GPIO11, enable GPIO10; shares e-paper SPI |
| PDM microphone | CLK GPIO19, data GPIO20, enable GPIO38 |

## Troubleshooting

- **Sensor I2C transactions fail:** confirm that SPI unused pins remain `-1`
  and that no second bus was created on GPIO0/GPIO1.
- **Touch direction is reversed:** do not rotate coordinates in page code;
  keep the verified conversion in `sticky_touch`.
- **MicroSD changes are not visible:** enter the Note page again or short-press
  AI to reload `TEST.TXT`.
- **The display flashes while changing pages:** this is expected during the
  current full-screen e-paper refresh.
- **The build directory reports missing generated files:** remove only
  `build/` and run `idf.py build` again.

## Extending the Demo

Keep the existing layer boundaries when adding hardware or UI features:

- Put pin definitions in `pin_config.h`.
- Put shared buses and board power sequencing in `board/`.
- Put hardware access in `devices/`.
- Convert readings to displayable state in `app_data`.
- Let pages read state and draw only through `Canvas`.
- Route input and asynchronous actions through `AppEvent`.
- Keep orientation conversion inside the display and touch modules.

Following these rules allows a new feature to be added without changing the
behavior of the existing pages.

## License

Original project code is licensed under the [MIT License](LICENSE), copyright
2026 Seeed Studio. The bundled Espressif button component retains its
[Apache-2.0 license](components/button/license.txt) and source copyright notices.
