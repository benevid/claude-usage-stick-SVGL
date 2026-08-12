# Claude Usage Stick — touch screen (ESP32-S3 + LVGL)

A desk gadget that shows your **Claude Code rate-limit usage** in real time on a 3.5" touch
screen. No computer, no app, no cloud: the device queries Anthropic's API directly, reads usage
straight from the response headers, and renders it all on a friendly dashboard — with animated
**Clawd** mascots, a usage trend chart, an hour-of-day heatmap and reset clocks.

<p align="center">
  <img src="assets/mock-agora.png" width="520" alt="Claude Usage Stick — Now screen (mockup)">
</p>

> 100% touch navigation (swipe ← → between screens, no physical button). Adapted from the original
> **Claude Usage Stick** project (a multi-board firmware with physical buttons) to run **on this
> screen only** — see [What came from the original project](#what-came-from-the-original-project).

> The on-screen UI is in Portuguese (the author's language). This README documents it in English;
> screenshot labels are referenced where useful.

---

## Screens

> The images below are **pixel-accurate mockups** of the v2.1 layout (real device photos coming soon). Regenerate them with `python3 tools/gen_mockups.py`.

Navigate by **swiping** (the dots at the bottom show your position; the active one becomes a
pill). The **gear** opens Settings. The thin **coral bar** below the header counts down to the
next refresh — tapping it refreshes immediately.

### 1. Now (*Agora*)
<img src="assets/mock-agora.png" width="400" align="right" alt="Now screen">

- Two big cards: **5-hour window** and **week (7-day) window**.
- Each card: large percentage and an **18-segment meter** whose lit segments (and the number)
  slide continuously from **green through amber to red** as the window fills, plus a **large live
  countdown** to the reset and the **local reset time**.
- Bottom strip: overall **status chip** (`OK` / `ATENCAO` / `BLOQUEADO`) and, when the
  [token bridge](#tokens-per-session-optional-bridge) is running, the **real token counts** for
  the current 5 h window.

<br clear="right">

### 2. Models (*Modelos*)
<img src="assets/mock-modelos.png" width="400" align="right" alt="Models screen">

- The 4 Clawd mascots (Haiku / Sonnet / Opus / Fable) with a **live status pill** under each one,
  fed by a **real probe against the API** (one model per refresh cycle, rotating):
  `OK 0.9s` (green, with latency) · `LIMITADO` (amber, HTTP 429) · `ERRO` (red, 5xx/network) ·
  `N/D` / `--` (gray). The mascot goes gray when the model is unreachable or under incident.
- An **incident line** from `status.claude.com` (is the problem you or Anthropic?).

<br clear="right">

### 3. 5-hour window (*Janela de 5h*)
<img src="assets/mock-janela5h.png" width="400" align="right" alt="5-hour window screen">

- Custom chart with the **X axis spanning exactly the current 5 h window** (start → reset).
- Solid coral line = real usage history; **dotted line = projection** at the current burn rate.
- Plain-language verdict, color-coded: *"At the current rate, runs out at 16:40 (in 1h32m)"*
  (amber/red) or *"Does NOT run out before the reset (~62%)"* (green).

<br clear="right">

### 4. Hourly rhythm (*Ritmo por hora*)
<img src="assets/mock-ritmo.png" width="400" align="right" alt="Hourly rhythm screen">

- **Usage by hour of day**: 24 bars whose height/brightness show which hours burn the most quota;
  the current hour is highlighted.
- **Period selector** at the top: **Hoje / 7d / 30d / Tudo** (today, last 7 days, last 30 days,
  all time). Per-day history is **persisted to flash** (31 days on the device).

<br clear="right">

### Threshold moments (animations)
<img src="assets/mock-momento.png" width="400" align="right" alt="Threshold moment overlay">

Whenever a window crosses **25 % / 50 % / 70 % / 100 %**, a full-screen animated "moment" pops
up (8 combinations: 4 thresholds × 2 windows): the official pixel-art **Clawd** drops in and
reacts to the level — relaxed at 25 %, focused with a sweat drop at 50 %, wide-eyed and shaking
at 70 %, grayed-out with X eyes and a blinking red ring at 100 % — while the percentage counts
up and a segment meter lights up. Tap to dismiss (auto-closes after ~4.5 s).

> **Double-tap the Clawd icon or the CLAUDE CODE wordmark** to preview the 8 animations in sequence. The **refresh button** sits at the center of the header (the thin coral bar below it is just the countdown indicator).

The header and the token/loading screens use the **official Claude Code pixel logo** (SVGs in
`assets/brand/`, converted to embedded LVGL images by `tools/gen_logo_assets.py`).

<br clear="right">

### Settings (*Ajustes*)

Opened from the gear (scrollable list, 44 px touch rows):

- **Refresh now** — forces a refresh.
- **Refresh interval** — 30 s / 1 min / 2 min / 5 min (tap to cycle; saved to NVS).
- **Slideshow** — auto-advances the screens; tap to cycle **off / 5 s / 10 s / 15 s / 30 s**
  (pauses for 10 s after any touch).
- **Timezone: GMT±N** — adjusts the timezone (tap to cycle; fixes the reset clocks).
- **Brightness** — low / medium / high (backlight PWM).
- **Configure WiFi** — re-scan + password on screen.
- **Change token** — reopens the web token entry.
- **Language** — Portuguese / English, applied to the whole UI (saved to NVS).
- **About** — device info, display model and developer credits.
- **Erase everything** — factory reset (2 taps to confirm).

---

## Hardware

| | |
|---|---|
| Screen | **Mini ESP32-S3 3.5" Capacitive Touch IPS · 480×320 · 8 MB PSRAM · 16 MB Flash** ([AliExpress](https://pt.aliexpress.com/item/1005007641039070.html)) |
| Chip | ESP32-S3 (native USB) |
| Display | **AXS15231B**, QSPI interface |
| Touch | **AXS15231B** capacitive, I²C `0x3B` |

> **OPI PSRAM is mandatory** — the 480×320 LVGL buffer doesn't fit in internal RAM.

Pins and the validated display/color/touch configuration are in
[`firmware/REFERENCIA-HARDWARE-LVGL.md`](firmware/REFERENCIA-HARDWARE-LVGL.md) and the reference
bring-up sketch in [`firmware/bringup/`](firmware/bringup/).

### 3D-printable case

A ready-to-print case for this display board is included:
[`3D Case/Case_JC3248W535C.stl`](3D%20Case/Case_JC3248W535C.stl) — print it, slide the board in
and the Usage Stick is desk-ready.

---

## How it works (and the token)

The gadget makes a **minimal** `POST` (`max_tokens: 1`) to
`https://api.anthropic.com/v1/messages` and **doesn't use the response body** — it reads usage
straight from the headers:

```
anthropic-ratelimit-unified-status                allowed | allowed_warning | rejected
anthropic-ratelimit-unified-5h-utilization        0–1   (becomes the 5-hour window %)
anthropic-ratelimit-unified-5h-reset              epoch
anthropic-ratelimit-unified-7d-utilization        0–1   (7-day window)
anthropic-ratelimit-unified-7d-reset              epoch
anthropic-ratelimit-unified-representative-claim  five_hour | seven_day  (what limits you first)
anthropic-ratelimit-unified-fallback-percentage
anthropic-ratelimit-unified-overage-status / -overage-disabled-reason
```

Model health combines `status.claude.com/api/v2/incidents/unresolved.json` (incidents) with a
**per-model probe**: each refresh cycle the device sends one `max_tokens: 1` request to the next
model in the rotation (Haiku → Sonnet → Opus → Fable) and records the HTTP code + latency. That's
what feeds the colored status pills on the Models screen.

### Tokens per session (optional bridge)

The API does **not** expose token counts for subscription accounts — the `unified-*` headers only
carry utilization percentages, and `/api/oauth/usage` requires the `user:profile` scope (the
`setup-token` only has `user:inference`) and still returns percentages. The real numbers live in
the **local Claude Code transcripts** (`~/.claude/projects/**/*.jsonl`).

[`tools/token_bridge.py`](tools/token_bridge.py) (stdlib only) closes that gap: it asks the device
for the current window (`GET http://claude-stick.local/window`), sums the transcript `usage`
entries since the window start (deduped by message id) and pushes them back
(`POST /tokens`). The "Now" screen then shows *"tokens na janela: 1.2M entrada • 88k saida"*.

```bash
python3 tools/token_bridge.py               # one shot
python3 tools/token_bridge.py --loop 120    # keep pushing every 2 min
```

The device advertises itself via mDNS as **`claude-stick.local`** while the dashboard is open. If
the row disappears, the data just went stale (> 15 min without a push).

### Generating the token (`claude setup-token`)

In a terminal, with **Claude Code** installed and logged into your subscription (**Pro** or
**Max**):

```bash
claude setup-token
```

This opens an **OAuth** flow in the browser; you authenticate with your Anthropic account and
receive a **long-lived token** in the form `sk-ant-oat01-…`.

It was designed for environments **without interactive login** (CI/CD, GitHub Actions, headless
scripts) — the typical use is as an environment variable:

```bash
export CLAUDE_CODE_OAUTH_TOKEN="sk-ant-oat01-..."
```

**⚠️ Important caveat:** this is a **Claude Code** token. A "raw" call to the Messages API
(`/v1/messages`) with it is usually **rejected**.

**How this gadget works around that:** it sends exactly the headers Claude Code sends —
`anthropic-beta: oauth-2025-04-20` plus the Claude Code `User-Agent` — in a `max_tokens: 1`
request. The API then responds **200** and returns the rate-limit headers (validated against a
real account). Since the body is discarded and it's just 1 token, **quota consumption is
negligible**.

> The token is typed **once** (via the web, see below) and stored **encrypted** on the device.

---

## Build & flash

Prerequisites (tested versions):

- `arduino-cli` 1.4.x · core `esp32:esp32` **3.3.11**
- libraries: **GFX Library for Arduino** 1.6.5 · **lvgl** 9.2.2

```bash
cd firmware/claude_stick
./build.sh                 # compile
./build.sh upload          # compile + flash (default port /dev/cu.usbmodem101)
./build.sh upload /dev/cu.usbmodemXXXX
./build.sh monitor /dev/cu.usbmodemXXXX
```

FQBN: `esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=custom,CDCOnBoot=cdc,USBMode=hwcdc,FlashMode=qio`

`build.sh` passes `-DLV_CONF_INCLUDE_SIMPLE -I<sketch>` so LVGL finds the sketch's `lv_conf.h`. If
you get `lv_conf.h not found`, copy `firmware/claude_stick/lv_conf.h` into your Arduino libraries
folder (one level above the `lvgl` folder) — copy **this** file, not an older one: the
`#include <stdint.h>` in it is wrapped in an `#ifndef __ASSEMBLY__` guard, without which the build
dies while assembling lvgl's `lv_blend_helium.S` / `lv_blend_neon.S`:

```
xtensa-esp-elf/include/stdint.h:21: Error: unknown opcode or format name 'typedef'
```

That guard is what lvgl's own `lv_conf_template.h` prescribes, and it is required because
`lv_conf_internal.h` pulls `lv_conf.h` in *even while assembling* those `.S` files.

One subtlety worth knowing if you build other sketches on this board: the `-I` above rides on
`compiler.c/cpp.extra_flags`, and the ESP32 core assembles `.S` files with `compiler.S.extra_flags`
instead. A sketch that carries its own `lv_conf.h` is fine (the sketch folder is on the include path
either way), but a sketch **without** one falls back to lvgl's `../../lv_conf.h` — that loose file in
`libraries/`, which may well belong to some other project. If you hit the error above from a sketch
that has no `lv_conf.h`, pass the `-I` in `compiler.S.extra_flags` too; `firmware/bringup/build.sh`
does exactly that.

To build the bring-up sketch, use its own script — `firmware/bringup/build.sh`. That folder has no
`partitions.csv` and no `lv_conf.h`, so the firmware's FQBN does not apply to it.

> If colors come out with red/blue swapped, flip `LV_COLOR_16_SWAP` to `1` in `lv_conf.h`.

---

## First-time setup (onboarding)

Everything via the screen / network — no recompiling needed:

1. **WiFi** — tap your network and type the password (on-screen keyboard). Stores up to 3 networks
   in NVS.
2. **Token** — the screen shows the **gadget's IP** (e.g. `http://192.168.0.42`) with an animated
   Claude icon. Open that address **on your PC/phone on the same network** and **paste the token**
   into the form. The device **validates** the token on the spot (a real API call) before
   accepting it.
3. **PIN** — set a 4-digit PIN (entered twice to confirm). The token is encrypted with it.

On every subsequent boot, the device only asks for the **PIN** to decrypt the token.

---

## Security

- The token is stored **encrypted** (AES-256-GCM; key derived from the PIN via SHA-256). The PIN
  is **never** stored — a wrong PIN means the GCM tag fails to verify.
- After 10 wrong attempts, the credentials are **wiped** and the device returns to onboarding
  (each failure doubles the lockout time).
- The history/heatmap lives in a **LittleFS** file (it does not contain the token).
- `.env` and `.mcp.json` are in `.gitignore` — **no secrets go to git**.

---

## What came from the original project

This is a fork of the **Claude Usage Stick** (a multi-board firmware with physical buttons). The
**data mechanics were reused** and the entire **hardware/UI layer was rewritten** for this screen.

**Reused from the original (adapted):**

- The core idea of **reading usage from the** `anthropic-ratelimit-unified-*` **headers** with a
  minimal `POST` (`firmware/claude_stick/api.cpp`).
- The **model-health** fetch from `status.claude.com` (`status.cpp`).
- The **token encryption** AES-256-GCM + PIN-derived key (`crypto.cpp`).
- The **CA bundle** for HTTPS (`certs.cpp`).
- The product concept and the **Clawd mascots** / model-status row.

**Rewritten / new in this version:**

- **LVGL 9 UI** for the touch screen (tileview with swipe + dots, cards, mascots with arms/legs,
  chart, heatmap) — replacing the multi-board TFT_eSPI/U8g2.
- **arduino-cli build** for the ESP32-S3 (replacing the multi-board PlatformIO setup).
- **Touch navigation** instead of physical buttons.
- **On-screen onboarding + web token entry** (local IP) instead of a captive portal.
- **Full** header parsing (status, `representative-claim`, overage, fallback).
- **Background refresh**, **persisted history/heatmap** (LittleFS), **configurable timezone**.

---

## Repository layout

```
firmware/
  claude_stick/                 # the firmware (arduino-cli sketch)
    claude_stick.ino            # setup/loop, state machine, dashboard, screens
    api.cpp/.h                  # fetchUsage() — usage via API headers
    status.cpp/.h               # fetchModelStatus() — model health
    crypto.cpp/.h               # AES-256-GCM + PIN-derived key
    certs.cpp/.h                # CA bundle for HTTPS
    wifi_manager.h              # networks saved in NVS (up to 3)
    touch.h                     # AXS15231B driver
    config.h                    # pins + endpoints + constants
    lv_conf.h                   # LVGL 9.2 config
    partitions.csv              # 16 MB partition (app + nvs + LittleFS)
    build.sh                    # compile / flash / monitor
  bringup/                      # validated bring-up (hardware reference)
  REFERENCIA-HARDWARE-LVGL.md   # display/colors/touch that work
assets/                         # mockups das telas + assets de marca (brand/)
3D Case/                        # case imprimível (STL) para a placa
```

## Where to tweak

- **Poll interval, endpoints, PIN, timezone:** via the screen (Settings) or in `config.h`.
- **Theme colors / layout:** top of `claude_stick.ino` (palette) and the `build_tile_*` builders.
- **Mascots:** `build_mascot()` in `claude_stick.ino`.

---

## Credits

Fork of the original **Claude Usage Stick**. This version's firmware was rewritten for the
ESP32-S3 480×320 LVGL screen. Not an official Anthropic product.
