# M5StickC Plus2

Part of [Claude Usage Stick](../README.md). The successor to the M5StickC Plus — same form factor, newer ESP32-PICO-V3-02 module, bigger battery.

## Specs

| | |
| --- | --- |
| MCU | ESP32-PICO-V3-02 (dual-core, WiFi, 2 MB PSRAM) |
| Display | 1.14" ST7789 LCD, 240×135 |
| Battery | 200 mAh internal |
| Buttons | Button A (front, GPIO 37) · Button B (side, GPIO 39) |
| Firmware | Clarity (v1) — target display tier **S** (not yet migrated) |
| PlatformIO env | `m5stick-cplus2` |
| Buy | [aliexpress.com](https://s.click.aliexpress.com/e/_c3jkKlNj) |

## Flash

```bash
pio run -e m5stick-cplus2 -t upload     # firmware
pio run -e m5stick-cplus2 -t uploadfs   # web setup UI (SPIFFS)
```

## Controls

| Context | Button A | Button B |
| ------- | -------- | -------- |
| PIN entry | Cycle the current digit (0–9) | Confirm digit, move to the next |
| Dashboard | Cycle brightness (off → dim → normal → bright) | Force an immediate refresh |
| On boot | Hold **A+B** = factory reset (wipes all stored data) | |

## Notes

- Runs the original **Clarity** dashboard until it's migrated to the Mango tier S layout.
- During setup, the WiFi AP password is shown on the device screen.
