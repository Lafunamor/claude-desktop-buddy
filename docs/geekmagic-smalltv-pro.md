# GeekMagic SmallTV Pro port

This is a port of the buddy firmware to the **GeekMagic SmallTV Pro**
(ESP32-WROOM-32 / -32E with a 240×240 ST7789 panel and a single capacitive
touch pad). It builds from the same source tree as the M5StickC Plus
firmware — a build-time flag (`-DTARGET_GEEKMAGIC`) swaps the M5 hardware
library for a compatibility shim in [`src/geekmagic/`](../src/geekmagic).

## What's different from the M5StickC Plus

The SmallTV Pro has far less hardware than the M5StickC Plus, so several
features are stubbed or remapped:

| M5StickC Plus | SmallTV Pro | Behaviour |
|---|---|---|
| 135×240 ST7789 | 240×240 ST7789 | Sprite is drawn at 135 wide and **centered** (52 px letterbox each side). Layout is unchanged. |
| Two buttons + power button | One capacitive touch pad | Gestures drive two virtual buttons — see below. |
| MPU6886 IMU | none | Reports flat/still: shake, face-down nap, and clock auto-rotate never fire. |
| AXP192 PMIC + battery | USB power only | Battery page shows USB/100 %. Brightness drives the PWM backlight. |
| RTC chip | none | Software clock, set from the desktop bridge's BLE time push. |
| Buzzer | none | Beeps are silent. |

## Touch gesture scheme

There is only one input, so the two-button UI maps onto gestures:

| Gesture | Acts as | Does |
|---|---|---|
| **Single tap** | Button A (short) | Approve a prompt · cycle screens · "next" in menus |
| **Double tap** | Button B | Deny a prompt · next page · "change/select" in menus |
| **Long press (≥0.6 s)** | Button A (held) | Open/close the menu; back out of settings |

A single tap resolves after a ~280 ms double-tap window, so primary taps have
a small intentional delay. When the screen is off, the first gesture only
wakes it.

## Wiring / pin map

These are baked into `platformio.ini` build flags for the
`geekmagic-smalltv-pro` env and match the known SmallTV Pro layout:

| Signal | GPIO | Notes |
|---|---|---|
| Display SCLK | 14 | |
| Display MOSI | 13 | |
| Display DC | 0 | also the bootstrap pin |
| Display RST | 2 | |
| Display CS | — | tied low on the board (`TFT_CS = -1`) |
| Backlight | 5 | PWM, **active-low** |
| Touch pad | **32 (assumed)** | **confirm for your unit — see below** |

### ⚠️ The touch pin is not officially documented

GeekMagic doesn't publish the touch pad's GPIO, and it varies by revision.
The build defaults to capacitive touch on **GPIO32**. If tapping does nothing:

1. Try other touch-capable pins (`-DBUDDY_TOUCH_PIN=`): 2, 4, 12, 13, 14, 15,
   27, 32, 33 — but avoid the ones already used by the display (0, 2, 13, 14).
   Good candidates: **32, 33, 27, 15, 12, 4**.
2. Tune the threshold (`-DBUDDY_TOUCH_THRESHOLD=`). `touchRead()` returns a
   low number when touched; print it over serial to find a value between the
   touched and untouched readings.
3. If your pad is wired as a plain digital button to GND, build with
   `-DBUDDY_TOUCH_DIGITAL=1` (uses `INPUT_PULLUP`, active-low).

## WiFi + over-the-air updates

The ESP32-WROOM-32 has **no USB hardware** and the board has **no USB-serial
chip**, so the USB-C port is power-only — the device can never be flashed
over USB. To avoid needing a serial adapter for every update, the firmware
brings up WiFi and an **ArduinoOTA** listener (enabled by `-DBUDDY_WIFI`).
After it's running once, every later flash is wireless.

1. Copy `src/geekmagic/wifi_secrets.example.h` to
   `src/geekmagic/wifi_secrets.h` (gitignored) and fill in your SSID /
   password. Without it the firmware still builds and runs, just BLE-only.
2. WiFi and BLE share the one 2.4 GHz radio (modem-sleep coexistence), so
   the link to Claude Desktop keeps working.

## Flashing

### First time — you need to get the firmware on once

Pick whichever you can do:

- **Serial** (cleanest, reversible): 3.3 V USB-serial adapter on the UART
  pads. Bridge `GPIO0 -> GND` while powering on to enter the bootloader, then:
  ```sh
  pio run -e geekmagic-smalltv-pro -t upload
  pio run -e geekmagic-smalltv-pro -t uploadfs   # /characters GIFs (optional)
  ```
- **OTA from existing ESPHome firmware** (no serial): if the board currently
  runs ESPHome, push `firmware.bin` once via ESPHome's OTA (port 3232, using
  your ESPHome `ota:` password). Note: ESPHome's partition table has no
  LittleFS partition, so GIF characters won't load until a serial `uploadfs`
  — the ASCII pet works regardless.

### Every time after — wireless

Once this firmware is running and on WiFi:
```sh
pio run -e geekmagic-smalltv-pro-ota -t upload   # flashes over the air
```
Set `upload_port` in the `geekmagic-smalltv-pro-ota` env to the device's IP
or `<hostname>.local`. No serial adapter, ever again.

## Status / known issues (as of first hardware bring-up)

The firmware was flashed to a real SmallTV Pro **over the air**, by pushing
`firmware.bin` from the board's existing ESPHome via ESPHome OTA (port 3232).
That part worked: ESPHome was replaced, the image booted, and the backlight
(GPIO5 PWM) lit. Two issues remain, both needing a serial console to debug:

1. **Display blank** — backlight on, nothing rendered. The panel likely needs
   a non-standard ST7789 init (the ESPHome config used a *custom* driver with
   `spi_mode3`). Things to try with serial logs in hand: SPI mode, toggling
   `TFT_INVERSION_ON/OFF`, a CGRAM offset, and confirming `Lcd.init()` output.
2. **WiFi not associating** — credentials are correct (same 2.4 GHz network),
   so this was almost certainly the BLE+WiFi coexistence ordering: modem-sleep
   was enabled before association. **Fixed in `net.cpp`** (associate with sleep
   off, enable it only after the link is up) but not yet re-flashed.

Because the running image can't get on WiFi, OTA is unavailable, so recovery
needs a **USB-to-serial adapter** (ordered). With serial we get the boot log,
can flash freely, and can iterate the display config. Consider also adding a
SoftAP/captive-portal fallback so a future WiFi miss can't lock the device out.

## If the display looks wrong

ST7789 panels vary in color order and inversion. Adjust these flags in the
`geekmagic-smalltv-pro` env:

- Colors inverted (negative image): toggle `-DTFT_INVERSION_ON=1` ↔
  `-DTFT_INVERSION_OFF=1`.
- Red/blue swapped: change `-DTFT_RGB_ORDER=TFT_BGR` ↔ `TFT_RGB`.
- Mirrored or rotated: the firmware calls `setRotation(0)`; if needed, shift
  the panel with `-DCGRAM_OFFSET` or adjust `TFT_WIDTH/HEIGHT` offsets.
