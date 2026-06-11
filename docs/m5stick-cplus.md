# M5StickC Plus

Part of [Claude Usage Stick](../README.md). The original board this project was built for — a finger-sized ESP32 stick with a built-in display, battery, and two buttons. Zero soldering required.

## Specs

| | |
| --- | --- |
| MCU | ESP32-PICO-D4 (dual-core, WiFi) |
| Display | 1.14" ST7789 LCD, 240×135 |
| Battery | 120 mAh internal |
| Buttons | Button A (front, GPIO 37) · Button B (side, GPIO 39) |
| Firmware | 🥭 **Mango (v2)** — display tier **S** (reference board) |
| PlatformIO env | `m5stick-cplus` |
| Buy | [aliexpress.com](https://s.click.aliexpress.com/e/_c3w3hHWl) |

## Flash

```bash
pio run -e m5stick-cplus -t upload     # firmware
pio run -e m5stick-cplus -t uploadfs   # web setup UI (SPIFFS)
```

## Controls

| Context | Button A | Button B |
| ------- | -------- | -------- |
| PIN entry | Cycle the current digit (0–9) | Confirm digit, move to the next |
| Dashboard | Flip screen 180° | Cycle brightness |
| On boot | Hold **A+B** = factory reset (wipes all stored data) | |

Refresh happens automatically on the poll interval — Mango has no manual-refresh button.

## Notes

- As the **tier S** reference board, the dashboard's MODELS section shows one overall-health Clawd mascot plus a 2×2 `NAME UP/DOWN` text grid — see [Display tiers](../README.md#display-tiers).
- During setup, the WiFi AP password is shown on the device screen.
