<div align="center">

<img src="assets/brand/claudecode-color.svg" width="76" alt="Clawd">

# Claude Usage Stick

**Your Claude Code rate limits, live on a 3.5" touch screen.**<br>
No computer. No app. No cloud.

**English** · [Português](README.pt-BR.md)

<img src="https://img.shields.io/badge/firmware-v2.2-D97757?style=for-the-badge" alt="firmware v2.2">
<img src="https://img.shields.io/badge/ESP32--S3-AXS15231B%20480×320-1A1A20?style=for-the-badge" alt="ESP32-S3 AXS15231B">
<img src="https://img.shields.io/badge/LVGL-9.2.2-4ADE80?style=for-the-badge" alt="LVGL 9.2.2">
<img src="https://img.shields.io/badge/accounts-up%20to%204-8C8C98?style=for-the-badge" alt="up to 4 accounts">

<img src="assets/mock-agora.png" width="560" alt="Claude Usage Stick — Now screen (mockup)">

</div>

The device queries Anthropic's API directly, reads your usage straight from the response headers,
and renders it on a dashboard — animated **Clawd** mascots, a burn-rate projection, an hour-of-day
heatmap, reset clocks, and **up to 4 accounts** you can switch between on-screen.

> 100% touch navigation (swipe ← → between screens, no physical button). Adapted from the original
> **Claude Usage Stick** project (a multi-board firmware with physical buttons) to run **on this
> screen only** — see [What came from the original project](#what-came-from-the-original-project).

> The on-screen UI is in Portuguese (the author's language). This README documents it in English;
> screenshot labels are referenced where useful.

---

## Getting one running

<img src="assets/banner-steps.png" width="100%" alt="Three steps: buy the board, plug it into USB, flash and set up">

Everything you need is in this repository: the firmware source, the exact board, the pin map and
the build commands. Flashing it yourself with `arduino-cli` is documented in
[Build & flash](#build--flash) and **costs nothing**.

### Don't want to install a toolchain?

<a href="https://usagestick.autom.my"><img src="assets/banner-web.png" width="100%" alt="Flash it from your browser at usagestick.autom.my"></a>

If you're short on time — or simply don't want to deal with `arduino-cli`, board packages and
libraries — there's a hosted flasher at **[usagestick.autom.my](https://usagestick.autom.my)**.
Create an account, plug the board into USB, and Chrome writes the firmware straight to it over Web
Serial. It takes about a minute and installs nothing on your machine.

That service charges a **small one-off fee per board**, which pays for hosting and for building the
convenience. To be explicit about what is being sold:

- **You are not paying for the firmware.** It is open source, it is right here, and you can build
  and flash it for free, forever, without an account.
- The fee covers **the convenience** of doing it from a browser. It is entirely optional.
- One payment covers **one board, for good** — including future firmware versions on that board.

If you're comfortable with a terminal, skip it and use [Build & flash](#build--flash).

---

## Screens

> The images below are **pixel-accurate mockups** rendered from the firmware's own layout and
> palette (real device photos coming soon) — regenerate with `python3 tools/gen_mockups.py`. They
> match v2.2, except that the four swipe screens do not yet show the `@label` account badge that
> appears in the header once you add a second account.

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
- **Accounts** — up to 4 Claude accounts (e.g. personal + work); tap a slot to switch which one
  the device monitors, add via the web form (with a custom label), rename on-screen (pencil),
  remove with 2 taps.
- **Change token** — reopens the web token entry (replaces the active account's token).
- **Language** — Portuguese / English, applied to the whole UI (saved to NVS).
- **About** — device info, display model and developer credits.
- **Erase everything** — factory reset (2 taps to confirm).

### Accounts (*Contas*)
<img src="assets/mock-contas.png" width="400" align="right" alt="Accounts screen">

Got a personal **and** a work Claude subscription? The device holds **up to 4**, each with its own
label and its own encrypted slot, all behind the same PIN.

- **Tap a row to switch.** The active one is coral and marked *ativa*. Switching swaps the token
  and that account's history/heatmap, then refreshes immediately — no PIN prompt, the session
  already unlocked it.
- **Only the active account is polled.** The others are completely dormant: **zero API requests**.
  That matters for a corporate account, where every poll is visible to your org's admins.
- **Pencil renames** on-screen; duplicate names get a " 2" suffix automatically, because the
  [token bridge](#tokens-per-session-optional-bridge) identifies accounts by label.
- **Trash removes** (2 taps to confirm), and it will not let you delete the last one.
- **+ Add account** reuses the web form, which now takes a label along with the token.

When more than one account exists, the dashboard header shows an `@label` badge so you always know
which one you are looking at.

<br clear="right">

---

## Hardware

| | |
|---|---|
| Screen | **Mini ESP32-S3 3.5" Capacitive Touch IPS · 480×320 · 8 MB PSRAM · 16 MB Flash** ([AliExpress](https://s.click.aliexpress.com/e/_c4T3hoZp)) |
| Chip | ESP32-S3 (native USB) |
| Display | **AXS15231B**, QSPI interface |
| Touch | **AXS15231B** capacitive, I²C `0x3B` |

> **Heads-up:** the AliExpress link above is an affiliate link. It costs you nothing extra, and the
> small commission goes straight back into keeping this project alive and updated. Buying the board
> through it is an easy way to support the work — thank you!

> **In Brazil?** Local sellers ship in days instead of weeks. [Fikra](https://fikra.com.br/esp32/) stocks this board —
> stock is limited, so it may show up as unavailable — and if you need it fast there is also this
> [Mercado Livre listing](https://www.mercadolivre.com.br/esp32s3-bluetooth-wifi-display-35--usbc/up/MLBU4794234738).

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

With multiple accounts, run one bridge per machine with `--account <label>` (the label configured
on the gadget). `GET /window` reports which account is active; a push for another account gets a
`409` and is simply skipped until that account becomes active again.

```bash
python3 tools/token_bridge.py --account Trabalho --loop 120   # work laptop
python3 tools/token_bridge.py --account Pessoal --loop 120    # personal machine
```

### Multiple accounts

The device stores up to **4 accounts** (label + token), each encrypted in its own NVS slot with
the same PIN. Only the **active** account is polled — the others stay dormant, generating **zero
API traffic**. Switching (Settings → Accounts) takes two taps: the device decrypts the slot,
swaps the per-account history/heatmap (`/hist0.bin`..`/hist3.bin` in LittleFS) and refreshes
immediately. The dashboard header shows an `@label` badge whenever more than one account exists.

Heads-up for corporate accounts: every poll is a real (minimal) API request, visible to your org's
admins like any Claude Code usage.

**Upgrading a device that already runs v2.1?** Nothing is lost and nothing is asked of you. The
first boot moves the single stored token into slot 0 (labelled "Conta 1") and gives it a copy of
the existing history. Both originals are deliberately **kept**, so flashing v2.1 again finds them
where it left them — the migration only ever adds. Flashing does not touch the `nvs` or `spiffs`
partitions in the first place; only the app partition is rewritten.

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

### 🚨 Read this before using a subscription token

**Anthropic does not permit subscription OAuth tokens in third-party tools.** In a policy
formalised on **4 April 2026**, Anthropic stated that Free/Pro/Max OAuth — the credential
`claude setup-token` produces — is intended **only** for Claude Code and claude.ai, and that using
it in any other product, tool or service violates the Consumer Terms. Server-side enforcement was
reported from January 2026.

This firmware is a third-party tool, and the workaround described just above — impersonating the
Claude Code client through its headers and User-Agent — is precisely the pattern that policy
addresses.

**What that means in practice:**

- It **works today**. A board was flashed in August 2026 and shows real data. But *working* is not
  *permitted*.
- The exposure is on **your account**, not the project's. Public reports of enforcement include
  authentication failures and account disruption.
- Anthropic may change the API or block the pattern at any time, and the device would simply stop
  showing numbers.

This project is not affiliated with Anthropic and cannot speak for them. It is published so people
can build it, read the code, and decide for themselves. **If you are not willing to accept that
risk on your own account, do not use a subscription token with this firmware.**

Sources, gathered 13 Aug 2026:
[The Register](https://www.theregister.com/2026/02/20/anthropic_clarifies_ban_third_party_claude_access/) ·
[WinBuzzer](https://winbuzzer.com/2026/02/19/anthropic-bans-claude-subscription-oauth-in-third-party-apps-xcxwbn/)

---

## Build & flash

> Rather not set any of this up? [usagestick.autom.my](https://usagestick.autom.my) flashes the
> board from Chrome, with nothing to install — see
> [Don't want to install a toolchain?](#dont-want-to-install-a-toolchain). The route below is the
> free one and always will be.

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
   Claude icon. Open that address **on your PC/phone on the same network**, optionally give the
   account a **label** (e.g. *Pessoal*, *Trabalho*) and **paste the token** into the form. The
   device **validates** the token on the spot (a real API call) before accepting it.
3. **PIN** — set a 4-digit PIN (entered twice to confirm). The token is encrypted with it.

On every subsequent boot, the device only asks for the **PIN** to decrypt the token. Extra
accounts added later (Settings → Accounts) reuse the same form and are encrypted with the same
PIN — no PIN prompt again during the session.

---

## Security

- Tokens are stored **encrypted** (AES-256-GCM; key derived from the PIN via SHA-256), one NVS
  slot per account, all under the **same PIN**. The PIN is **never** stored — a wrong PIN means
  the GCM tag fails to verify.
- After 10 wrong attempts, the credentials are **wiped** and the device returns to onboarding
  (each failure doubles the lockout time).
- The history/heatmap lives in a **LittleFS** file, one per account (it does not contain the token).
- The PIN stays in RAM for the session so switching accounts does not prompt again. This does not
  weaken the model: the active token is already held decrypted in RAM either way. Nothing is
  written to NVS, and a factory reset clears it.
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
    accounts.cpp/.h             # account slots in NVS (multi-account)
    certs.cpp/.h                # CA bundle for HTTPS
    wifi_manager.h              # networks saved in NVS (up to 3)
    touch.h                     # AXS15231B driver
    config.h                    # pins + endpoints + constants
    lv_conf.h                   # LVGL 9.2 config
    partitions.csv              # 16 MB partition (app + nvs + LittleFS)
    build.sh                    # compile / flash / monitor
  bringup/                      # validated bring-up (hardware reference)
    build.sh                    # its own FQBN — see Build & flash
  REFERENCIA-HARDWARE-LVGL.md   # display/colors/touch that work
tools/
  token_bridge.py               # pushes local token counts to the device
  gen_logo_assets.py            # brand SVGs -> logo_assets.h
  gen_mockups.py                # regenerates assets/mock-*.png (the screens)
  gen_banners.py                # regenerates assets/banner-*.png (README banners)
  gen_social.py                 # regenerates assets/social-preview*.png (repo social card)
assets/                         # screen mockups, README banners + brand assets (brand/)
3D Case/                        # printable case (STL) for the board
```

## Where to tweak

- **Poll interval, endpoints, PIN, timezone:** via the screen (Settings) or in `config.h`.
- **Theme colors / layout:** top of `claude_stick.ino` (palette) and the `build_tile_*` builders.
- **Mascots:** `build_mascot()` in `claude_stick.ino`.

---

## Credits

Fork of the original **Claude Usage Stick**. This version's firmware was rewritten for the
ESP32-S3 480×320 LVGL screen. Not an official Anthropic product.

### Contributors

For a while this project compiled on exactly one machine in the world — mine. Thanks to the people
who found that and fixed it, and to the one who taught it a new trick:

<table>
  <tr>
    <td align="center" valign="top" width="20%"><a href="https://github.com/oauramos"><img src="https://github.com/oauramos.png?size=100" width="72" height="72" alt="oauramos"><br><sub><b>@oauramos</b></sub></a><br><sub><i>original project</i></sub></td>
    <td align="center" valign="top" width="20%"><a href="https://github.com/jzimath-lab"><img src="https://github.com/jzimath-lab.png?size=100" width="72" height="72" alt="jzimath-lab"><br><sub><b>@jzimath-lab</b></sub></a><br><sub><a href="https://github.com/benevid/claude-usage-stick-SVGL/pull/1">#1</a></sub></td>
    <td align="center" valign="top" width="20%"><a href="https://github.com/renanravelli"><img src="https://github.com/renanravelli.png?size=100" width="72" height="72" alt="renanravelli"><br><sub><b>@renanravelli</b></sub></a><br><sub><a href="https://github.com/benevid/claude-usage-stick-SVGL/pull/2">#2</a></sub></td>
    <td align="center" valign="top" width="20%"><a href="https://github.com/mpsd18"><img src="https://github.com/mpsd18.png?size=100" width="72" height="72" alt="mpsd18"><br><sub><b>@mpsd18</b></sub></a><br><sub><a href="https://github.com/benevid/claude-usage-stick-SVGL/pull/3">#3</a></sub></td>
    <td align="center" valign="top" width="20%"><a href="https://github.com/ViniciusLoureiro67"><img src="https://github.com/ViniciusLoureiro67.png?size=100" width="72" height="72" alt="ViniciusLoureiro67"><br><sub><b>@ViniciusLoureiro67</b></sub></a><br><sub><a href="https://github.com/benevid/claude-usage-stick-SVGL/pull/4">#4</a></sub></td>
  </tr>
</table>

- **[@jzimath-lab](https://github.com/jzimath-lab)** — [#1](https://github.com/benevid/claude-usage-stick-SVGL/pull/1),
  merged. Tracked down why the firmware would not build anywhere else: `lv_conf.h`'s
  `#include <stdint.h>` needs an `#ifndef __ASSEMBLY__` guard, because `lv_conf_internal.h` pulls
  that header in *while assembling* lvgl's `.S` files. Also spotted that
  `AXS15231B_Touch::_instance` is a definition sitting in a header, which only got away with it
  while the sketch was a single translation unit.
- **[@mpsd18](https://github.com/mpsd18)** — [#3](https://github.com/benevid/claude-usage-stick-SVGL/pull/3).
  Reached the same root cause independently and explained the piece nobody else did: the include
  path rides on `compiler.c/cpp.extra_flags`, while the ESP32 core assembles `.S` files with
  `compiler.S.extra_flags`. That analysis is why `firmware/bringup/build.sh` passes it in all
  three, and it is the reason the Windows fallback in *Build & flash* is documented the way it is.
- **[@ViniciusLoureiro67](https://github.com/ViniciusLoureiro67)** — [#4](https://github.com/benevid/claude-usage-stick-SVGL/pull/4).
  A third independent confirmation, on a JC3248W535C, and closed the duplicate voluntarily,
  pointing at #1. Three people arriving at the same cause from three setups is what made it obvious
  the problem was here, not there.
- **[@renanravelli](https://github.com/renanravelli)** — [#2](https://github.com/benevid/claude-usage-stick-SVGL/pull/2),
  merged. Multi-account support: up to 4 accounts, only the active one polled, per-account history,
  on-screen rename, and the `--account` flag in the token bridge.
