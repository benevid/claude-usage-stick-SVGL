# LilyGo T-Display S3

Part of [Claude Usage Stick](../README.md). The biggest small-format screen in the lineup — a 1.9" LCD driven over a fast 8-bit parallel bus, with two front buttons and a battery connector.

## Specs

| | |
| --- | --- |
| MCU | ESP32-S3 (dual-core, WiFi, 8 MB PSRAM) |
| Display | 1.9" ST7789 LCD, 320×170, 8-bit parallel |
| Battery | External via JST connector (not included) |
| Buttons | Button A (BOOT, GPIO 0) · Button B (KEY, GPIO 14) |
| Firmware | 🥭 **Mango (v2.0.0)** — display tier **L** (reference board) |
| PlatformIO env | `tdisplay-s3` |
| Buy | [aliexpress.com](https://s.click.aliexpress.com/e/_c4rvB1Mv) |

## Flash

```bash
pio run -e tdisplay-s3 -t upload     # firmware
pio run -e tdisplay-s3 -t uploadfs   # web setup UI (SPIFFS)
```

> This env is for the **regular LCD variant**. For the 1.91" AMOLED version, use [`tdisplay-s3-amoled`](tdisplay-s3-amoled.md).

## Controls

| Context | Button A | Button B |
| ------- | -------- | -------- |
| PIN entry | Cycle the current digit (0–9) | Confirm digit, move to the next |
| Dashboard | Flip screen 180° | Cycle brightness |
| Dashboard | Press **A+B** together = force refresh | |
| On boot | Hold **A+B** = factory reset (wipes all stored data) | |

Refresh happens automatically on the poll interval; press **A+B** together to force one now.

## Notes

- As the **tier L** reference board, the dashboard shows the 5H/7D reset countdowns in large type on their own row below the bars, and the MODELS section shows a row of four labelled Clawd mascots (Haiku / Sonnet / Opus / Fable), each blinking while healthy — see [Display tiers](../README.md#display-tiers).
- During setup, the WiFi AP password is shown on the device screen.
