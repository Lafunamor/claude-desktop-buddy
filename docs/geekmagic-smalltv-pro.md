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

## Building & flashing

```sh
# enter the bootloader: bridge GPIO0 -> GND while powering on via the
# UART pads, then release. (Pads: GND, TXD0, RXD0, 3V3, GPIO0, RST.)
pio run -e geekmagic-smalltv-pro -t upload          # firmware
pio run -e geekmagic-smalltv-pro -t uploadfs        # /characters GIFs (optional)
pio device monitor -e geekmagic-smalltv-pro         # serial log
```

You'll need a USB-to-serial (3.3 V) adapter on the SmallTV Pro's UART pads —
the board's USB port is power-only.

## If the display looks wrong

ST7789 panels vary in color order and inversion. Adjust these flags in the
`geekmagic-smalltv-pro` env:

- Colors inverted (negative image): toggle `-DTFT_INVERSION_ON=1` ↔
  `-DTFT_INVERSION_OFF=1`.
- Red/blue swapped: change `-DTFT_RGB_ORDER=TFT_BGR` ↔ `TFT_RGB`.
- Mirrored or rotated: the firmware calls `setRotation(0)`; if needed, shift
  the panel with `-DCGRAM_OFFSET` or adjust `TFT_WIDTH/HEIGHT` offsets.
