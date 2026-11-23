# roundDash 1.28 — round CAN gauge for rusEFI

[![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue)](https://www.espressif.com/en/products/socs/esp32-s3)
[![Framework](https://img.shields.io/badge/framework-ESP--IDF%20v6.0-red)](https://github.com/espressif/esp-idf)
[![GUI](https://img.shields.io/badge/GUI-LVGL%209-orange)](https://lvgl.io)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

A small round digital gauge that reads the **rusEFI CAN broadcast** and shows RPM, coolant, AFR, speed
and more on a 1.28" touch screen. Which screens are shown, and how they look, is set from a web page on
your phone over the gauge's own Wi-Fi. It works with the rusEFI preset out of the box: wire it to the
CAN bus, turn on the broadcast in TunerStudio, and it runs.

This is a port of [Light-r4y/roundDash_2.1](https://github.com/Light-r4y/roundDash_2.1)
(MIT, written for the larger 2.1" Waveshare board) to the cheaper, smaller 1.28" board.

## The gauge

**Waveshare ESP32-S3-Touch-LCD-1.28** — Waveshare SKU **25098**
([wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-1.28)).

| | |
|---|---|
| MCU | ESP32-S3R2, 16 MB flash, 2 MB PSRAM, Wi-Fi |
| Display | 1.28" round IPS, 240×240, GC9A01A (SPI) |
| Touch | CST816S capacitive |
| IMU | QMI8658 (used for the g-meter screen) |
| Size | Φ38.5 mm glass |

Be sure to get the **ESP32-S3** version with touch. Waveshare also sells an RP2040 board with the same
screen, and versions without touch; this firmware does not run on those.

You also need:
- a **3.3 V CAN transceiver**, such as an SN65HVD230 module or a TJA1051T/3 (the board has none);
- a **12 V → 5 V automotive DC-DC** with surge protection, to power the gauge in the car.

## What it does

- Reads the rusEFI CAN broadcast (frames 0x200–0x207, 500 kbps). By default the controller is
  **listen-only**: it never transmits on your bus.
- Five built-in screens: **RPM** (dial), **COOLANT** (ring), **AFR** (dial), **SPEED** (number) and a
  **g-meter** (IMU).
- Up to 8 screens of four types (dial, ring fill, number, g-meter), each with up to 4 extra widgets
  (number, text, image, conditional indicator, bar, mini arc), colour zones and configurable ticks.
- **Swipe** left/right on the screen to change pages.
- **Web interface** over the gauge's own Wi-Fi access point: screen editor with live preview and drag &
  drop, CAN signal mapping (DBC-style), CAN sniffer, demo mode, brightness, accelerometer calibration,
  password, and firmware updates over the air (OTA, with rollback).
- About 40–50 frames per second on the dial screens.

### Signals in the rusEFI preset

| Signal | Frame | Signal | Frame |
|---|---|---|---|
| RPM | 0x201 | Oil pressure | 0x204 |
| Vehicle speed | 0x201 | Oil temp | 0x204 |
| TPS | 0x202 | Battery voltage | 0x204 |
| MAP | 0x203 | Lambda / AFR | 0x207 |
| Coolant temp | 0x203 | Fuel level | 0x203 |
| Intake air temp | 0x203 | | |

Anything else on the bus can be mapped on the web page's **CAN** tab.

## Setting up rusEFI

In TunerStudio, under the CAN settings:
1. Turn on **rusEFI CAN broadcast** (the dash / "Enable rusEFI CAN broadcast" option).
2. Leave the base ID at **0x200 (512)**, which is the default.
3. Set the CAN bus to **500 kbps**.

Burn the settings and restart the ECU. A wideband controller on the same bus does not interfere.

## Wiring

Everything goes through the board's **P2 connector** (JST SH 1.0 mm, 12-pin; a lead comes with the
board). **Check the wire colours against the pins with a meter before you solder**: they are not the same
on every lead.

| P2 pin | Signal | Connect to |
|---|---|---|
| 1 | GND | ground (DC-DC and transceiver) |
| 2 | VSYS | **5 V** from the DC-DC |
| 4 | BOOT (GPIO0) | optional: a button to GND, same as the on-board BOOT key |
| 6 | 3V3 | transceiver 3.3 V supply |
| 9 | GPIO17 | transceiver **TX** input (TXD, "CTX" on SN65HVD230 modules) |
| 10 | GPIO18 | transceiver **RX** output (RXD, "CRX") |

CAN-H and CAN-L go to the transceiver, not to the gauge. Use twisted pair.

> ⚠️ **Never put 12 V on any P2 pin.** VSYS takes 5 V only. 12 V on the wrong wire kills the board's
> 3.3 V regulator, and the board with it.

**Termination:** the bus needs 120 Ω at each end. Measure between CAN-H and CAN-L with everything
powered off: about **60 Ω** means the bus is already terminated at both ends, so the gauge must not add
one. About **120 Ω** means one end is missing, and if the gauge is at that end, add 120 Ω at the gauge.

## Controls

| Action | Result |
|---|---|
| Swipe left / right | next / previous screen |
| BOOT key, short press | next screen |
| BOOT key, hold 2 s | start the Wi-Fi access point |
| BOOT key, hold 10 s | clear a forgotten settings password |

Holding BOOT while the board powers up puts it into the USB flashing mode. That is harmless: power it
up again.

### First run

1. **Hold BOOT for 2 seconds.** The screen shows the network name (`roundGauge-XXXX`), the password
   (`roundgauge`) and the address.
2. Connect from a phone or laptop and open `http://192.168.4.1/`.
3. No car handy? **CAN** → turn on **Demo** → Apply, and the values start moving.
4. **Screens** sets up the pages; "Apply to gauge" sends them straight to the board.

Chrome on Android sometimes refuses to load the page from the gauge's access point
(`ERR_TOO_MANY_RETRIES`). Samsung Internet or a laptop browser work.

## Building and flashing

Requires [ESP-IDF v6.0](https://docs.espressif.com/projects/esp-idf/en/v6.0/esp32s3/get-started/).
Dependencies (LVGL, display and touch drivers) download on the first build.

```bash
cd fw
idf.py build
idf.py -p COM5 flash monitor     # or /dev/ttyACM0 etc.
```

The ESP-IDF project is in `fw/`, not the repository root. In VS Code with the Espressif extension, open
`roundGauge.code-workspace` or the `fw` folder. Flash over the board's USB-C with the 12 V supply
unplugged. After editing `fw/sdkconfig.defaults` or `fw/sdkconfig.board.*`, delete `fw/sdkconfig` so the
new values are picked up.

The 1.28 is the default target. The original 2.1 board is still supported:
`idf.py -D RG_BOARD=2_1 build` (start from a clean build directory).

> 💡 **Windows:** if ESP-IDF was installed with EIM and the system Python is newer than 3.11,
> `export.ps1` won't find its environment. Put the IDF Python first in `PATH`:
> ```powershell
> $env:PATH = "$env:USERPROFILE\.espressif\tools\idf-python\3.11.2;$env:PATH"; . $env:USERPROFILE\esp\v6.0\esp-idf\export.ps1
> ```

### Release kit

```bash
tools/build/build.sh        # Linux/macOS
tools\build\build.bat       # Windows
```

This builds into `releases/roundGauge_v<version>_<time>/` (and a `.zip`) with everything needed to flash
on any PC without installing ESP-IDF: `flash_all.bat` (full flash) and `update_firmware.bat` (firmware
and web interface only, keeps uploaded images), plus `.sh` versions. Screen layout, CAN table and
settings live in NVS and survive both.

### Web interface without hardware

```bash
python tools/webtest/mock_server.py
```

Open `http://localhost:8088/`. It serves `fw/www` and fakes the board's API, including CAN frames.

## Differences from the 2.1 original

| | 2.1 (upstream) | 1.28 (this port) |
|---|---|---|
| Screen | 480×480 RGB | 240×240 SPI |
| Fonts and backgrounds | image set in `fw/media` | built-in fonts, plain backgrounds (the media set is drawn for 480 px) |
| microSD logging | yes | no (no card slot) |
| Buzzer alerts | yes | no (no buzzer) |
| Buttons | two on J9 | BOOT key (plus optional external button on P2.4) |
| CAN pins | GPIO19 / GPIO20 | GPIO17 / GPIO18 |

All pin numbers live in `fw/common/board/board.h`. The 1.28 board code is
`fw/common/board/board_1_28.c`. Firmware design notes from upstream: [docs/fw-design.md](docs/fw-design.md).

## Repository layout

```
├── fw/                 firmware (ESP-IDF project)
│   ├── main/           initialization and task startup
│   ├── tasks/          one component per task: can, ui, buttons, touch, imu, sound, logger, webcfg
│   ├── common/         board (pins, screen, touch), config (settings, layout, CAN table), signals
│   ├── www/            web interface → www partition
│   └── media/          base image set → media partition
├── docs/fw-design.md   firmware design notes (upstream)
├── hw/                 2.1 board schematic and datasheets (upstream)
└── tools/              release build, flashing kit, web mock server
```

## Status

Working on the bench and with demo data: display, touch, swipes, CAN reception, web interface. In-car
testing on a rusEFI uaEFI Pro is in progress. Issues and pull requests are welcome.

## Credits and third-party code

Original firmware: **[Light-r4y/roundDash_2.1](https://github.com/Light-r4y/roundDash_2.1)**,
© 2026 Light-r4y, MIT. Its README (English and Russian) covers the 2.1 board.

| What | Where | License |
|---|---|---|
| ST7701 init sequence by Waveshare from [ESP32_Display_Panel](https://github.com/esp-arduino-libs/ESP32_Display_Panel) (2.1 build) | `fw/common/board/st7701_init.h` | Apache-2.0 |
| [Alpine.js](https://alpinejs.dev) 3.14.1 | `fw/www/alpine.min.js` | MIT |
| [Bootstrap](https://getbootstrap.com) 5.3.0 | `fw/www/bootstrap.min.css` | MIT |
| Segment7 (Cedders, 2014) as LVGL fonts | `fw/media/seg7_*.fnt` | SIL OFL 1.1 |
| [Carbon Fiber](https://cc0-textures.com/t/st-carbon-fiber) texture (ShareTextures, M. Tolga Arslan) | `fw/media/bg_carbon.bin` | CC0 |
| Orbitron, Michroma, Russo One, Jura ([Google Fonts](https://fonts.google.com)) as LVGL fonts | `fw/media/*.fnt` | SIL OFL 1.1 |
| Black Ops One (Black-Ops Project Authors), modified | `fw/media/blackops_*.fnt` | SIL OFL 1.1 |
| [Font Awesome Free](https://fontawesome.com) 6.4.0 | `fw/www/fontawesome/` | icons CC BY 4.0, font SIL OFL 1.1, code MIT |

LVGL (MIT) and Espressif components (Apache-2.0) are downloaded at build time.

## License

[MIT](LICENSE). Keep the copyright notice when you redistribute.
