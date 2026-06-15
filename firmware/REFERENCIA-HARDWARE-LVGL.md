# Referência de Hardware & LVGL — Gadget Valorant

Extraído do projeto que já funciona (`../esp32_controller/`). **Esta é a configuração de tela testada e validada** — usar como base do firmware do gadget para não repetir a dor de acertar as cores.

---

## 0. ⭐ TL;DR — o que faz as cores funcionarem

A placa tem display **AXS15231B por QSPI**. O que dá **cores corretas** (depois de muita tentativa) é:

1. `Arduino_Canvas` (framebuffer em RAM/PSRAM) criado com **`rotation = 0`** — comentário no código original: *"Canvas rotation=0 (perfect colors)"*.
2. A **rotação para paisagem é feita manualmente no flush** (gira 270° CW, USB à esquerda), copiando os pixels do buffer do LVGL para o framebuffer do Canvas.
3. O push pro painel é `gfx->flush()` (empurra o Canvas inteiro via QSPI).

> ❌ **NÃO** usar a abordagem do `display.h.bak` (Canvas `rotation=1` + `draw16bitRGBBitmap` no flush): foi a que dava **cores erradas**. A versão correta é a do `esp32_controller.ino`.

Por que funciona: o `Arduino_AXS15231B` espera o framebuffer no formato nativo (retrato 320×480) sem rotação interna; rotacionar via Canvas embaralhava a ordem de bytes/cor. Mantendo o Canvas em `rotation=0` e rotacionando "na mão" no flush, os pixels RGB565 chegam na ordem certa.

---

## 1. Placa & display

| | |
|---|---|
| Placa | **Guition JC4832W535** (ESP32‑S3) |
| Flash / PSRAM | 16 MB flash · **8 MB PSRAM OPI** (obrigatória) |
| Display | **AXS15231B**, interface **QSPI**, **480×320** paisagem (nativo 320×480 retrato) |
| Touch | **AXS15231B capacitivo**, I²C, addr `0x3B` |
| USB | CDC nativo (Serial pela própria USB) |

## 2. Pinos (`config.h`)

```
// Display QSPI (AXS15231B)
TFT_CS=45  TFT_SCK=47  TFT_SDA0=21  TFT_SDA1=48  TFT_SDA2=40  TFT_SDA3=39
TFT_BL=1 (backlight, HIGH=on)   TFT_TE=38
QSPI_FREQ = 40 MHz

// Touch I2C (AXS15231B)
TOUCH_SDA=4  TOUCH_SCL=8  TOUCH_INT=3  TOUCH_ADDR=0x3B

// SD Card SPI (provavelmente não usaremos — temos LittleFS)
SD_CS=10  SD_MOSI=11  SD_SCLK=12  SD_MISO=13

SCREEN_WIDTH=480  SCREEN_HEIGHT=320
```

## 3. Bibliotecas (versões testadas)

| Lib | Versão | Uso |
|---|---|---|
| `esp32` (core) | **3.3.8** | toolchain ESP32‑S3 |
| `GFX Library for Arduino` (Arduino_GFX) | **1.6.5** | driver QSPI + Canvas (`Arduino_ESP32QSPI`, `Arduino_AXS15231B`, `Arduino_Canvas`) |
| `lvgl` | **9.2.2** | UI (API v9: `lv_display_create`, etc.) |
| `ArduinoJson` | **7.2.0** | parse do payload do backend |
| `LittleFS`, `WiFi`, `Preferences` | builtin (core) | assets / rede / config NVS |

> Tem `LovyanGFX` instalada também, mas **não é usada** nesta placa — é Arduino_GFX.

---

## 4. ⭐ Inicialização do display (código que funciona)

```cpp
#include <Arduino_GFX_Library.h>
#include <lvgl.h>

Arduino_Canvas *gfx = nullptr;
static uint16_t *canvas_fb = nullptr;   // framebuffer do Canvas (320x480)

// --- setup() ---
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    TFT_CS, TFT_SCK, TFT_SDA0, TFT_SDA1, TFT_SDA2, TFT_SDA3);
Arduino_GFX *g = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, 320, 480);
gfx = new Arduino_Canvas(320, 480, g, 0, 0, 0);   // ⭐ rotation = 0

if (!gfx->begin(40000000UL)) { /* FATAL */ }
gfx->fillScreen(0x0000);
gfx->flush();
canvas_fb = gfx->getFramebuffer();                 // ⭐ acesso direto ao FB

pinMode(TFT_BL, OUTPUT);
digitalWrite(TFT_BL, HIGH);                         // backlight ON
```

### Flush callback (rotação manual 270° CW — USB à esquerda)

```cpp
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint16_t *src = (uint16_t *)px_map;
    for (int ly = 0; ly < SCREEN_HEIGHT; ly++) {        // 320
        uint16_t *src_row = src + ly * SCREEN_WIDTH;    // 480
        for (int lx = 0; lx < SCREEN_WIDTH; lx++) {
            canvas_fb[(479 - lx) * 320 + ly] = src_row[lx];  // gira 270° CW
        }
    }
    gfx->flush();
    lv_disp_flush_ready(disp);
}
```

> Esse flush usa **render mode FULL** (o LVGL entrega a tela inteira de uma vez). Por isso o buffer do LVGL é do tamanho da tela toda (480×320) e mora na **PSRAM**.

---

## 5. LVGL 9 — setup

```cpp
lv_init();
lv_tick_set_cb([]() -> uint32_t { return millis(); });   // tick por millis()

uint32_t bufSize = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color_t); // 480*320*2
lv_color_t *buf = (lv_color_t *)heap_caps_malloc(
    bufSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);       // ⭐ PSRAM

lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT); // 480x320
lv_display_set_flush_cb(disp, disp_flush_cb);
lv_display_set_buffers(disp, buf, NULL, bufSize, LV_DISPLAY_RENDER_MODE_FULL);

lv_indev_t *indev = lv_indev_create();
lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
lv_indev_set_read_cb(indev, touch_read_cb);
```

No **loop**: `lv_task_handler();` + `delay(16);` (~60 fps). Atualizar telas com dados ao vivo em intervalo (ex. a cada 1 s).

### `lv_conf.h` (pontos críticos)
- `LV_COLOR_DEPTH 16` (RGB565).
- Habilitar as fontes Montserrat usadas: 10, 12, 14, 18, 24, 28.
- `LV_USE_OS 0` (loop cooperativo) e tick via `lv_tick_set_cb` (acima).
- PSRAM: o buffer é alocado manualmente, então `LV_MEM_*` pode ficar no default (RAM interna) para o resto.

---

## 6. Touch (AXS15231B, I²C) — `touch.h`

- Driver custom (classe `AXS15231B_Touch`), I²C 400 kHz, INT em `FALLING`.
- Comando de leitura: `{0xB5,0xAB,0xA5,0x5A,0x00,0x00,0x00,0x08}` → lê 8 bytes; X/Y nos bytes 2–5.
- Instanciar com **rotation = 3** para a orientação "USB à esquerda":
  ```cpp
  AXS15231B_Touch touch_dev(TOUCH_SCL, TOUCH_SDA, TOUCH_INT, TOUCH_ADDR, 3);
  ```
- Mapeamento (nativo retrato 320×480):
  ```
  rot 1 (paisagem):  x = raw_y;        y = 319 - raw_x
  rot 3 (USB-esq.):  x = 479 - raw_y;  y = raw_x
  ```

## 7. ⚠️ Orientação — display e touch TÊM que casar

USB à esquerda → **display gira 270° CW no flush** (`479 - lx`) **e** **touch rotation = 3**. Se inverter um sem o outro, o toque fica espelhado/trocado. Mantê-los em par.

---

## 8. WiFi — reaproveitar `wifi_manager.h` (quase como está)

Classe `WiFiManager` pronta, guarda até **3 redes na NVS** (`Preferences` namespace `"wifi"`):

```cpp
g_wifi.begin();                       // carrega redes salvas (NVS)
bool ok = g_wifi.autoConnect(5000);   // tenta as salvas, 5 s cada
// se falhar → tela de config:
int n = g_wifi.scanNetworks(results, max);          // lista redes
g_wifi.connectTo(ssid, pass, 15000);  // conecta E salva na NVS (promove p/ topo)
g_wifi.getIP(); g_wifi.getSSID(); g_wifi.getSavedSSID();
```

**Fluxo no firmware do gadget:** no boot, `begin()` → `autoConnect()`. Se não conectar, abrir tela de WiFi (scan + teclado LVGL para a senha) → `connectTo()`. Reaproveitar `ui_wifi.cpp` como base da tela.

## 9. Cliente HTTP — adaptar `api_client.h`

Padrão da referência (manter): `HTTPClient` + `WiFiClientSecure` com `setInsecure()` para **HTTPS**, parse com `ArduinoJson`.

**Diferença para o nosso gadget:** não há login email/senha+JWT. O gadget só faz:
```cpp
GET https://<backend>/api/v1/gadget/dashboard
Header: Authorization: Bearer <DEVICE_TOKEN>   // o código curto tipo "SV76-7KX2"
```
e dá parse no payload enxuto (ver `../docs/03-CONTRATO-API.md`). O device token é digitado uma vez no gadget e salvo na **NVS** (mesmo padrão `Preferences` do WiFi).

---

## 10. Build & upload (arduino-cli)

- **arduino-cli 1.4.1**, core **esp32:esp32 3.3.8** já instalados.
- **Placa conectada:** `/dev/cu.usbmodem101`.
- `partitions.csv` (16 MB): app0 2 MB + spiffs ~14 MB (LittleFS monta a partição "spiffs").

FQBN recomendado (ESP32‑S3 + **PSRAM OPI** + 16 MB + partição custom + **USB CDC**):
```bash
FQBN="esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=custom,CDCOnBoot=cdc,USBMode=hwcdc,FlashMode=qio"

arduino-cli compile --fqbn "$FQBN" .
arduino-cli upload  --fqbn "$FQBN" -p /dev/cu.usbmodem101 .
arduino-cli monitor -p /dev/cu.usbmodem101 -c baudrate=115200
```
> A `PartitionScheme=custom` usa o `partitions.csv` da pasta do sketch. **PSRAM=opi é obrigatório** (o framebuffer 480×320×2 ≈ 300 KB + buffer LVGL não cabem na RAM interna). Confirmar os nomes exatos das opções do FQBN com `arduino-cli board details --fqbn esp32:esp32:esp32s3` na hora de compilar.

---

## 11. Sequência sugerida de implementação do firmware (Valorant)

> Ordem pensada para validar o difícil (tela/cor/touch) cedo e ir somando.

- **Fase 0 — Esqueleto:** estrutura do sketch em `firmware/`, copiar `config.h` (pinos), `touch.h`, `wifi_manager.h` da referência; `lv_conf.h` com depth 16 + fontes.
- **Fase 1 — Bring-up (CRÍTICO):** ✅ **FEITO e validado no hardware** — sketch em `firmware/bringup/` (cores, orientação USB-esquerda e touch 100% OK). É a base conhecida-boa pra copiar a config de display/touch.
- **Fase 2 — Base LVGL + tema Valorant:** estilos (cores `#0b0b18`/`#7c3aed`/`#f43f5e`), helpers de label/painel/botão (espelhar `ui_theme.h`).
- **Fase 3 — WiFi:** `autoConnect` + tela de config (scan + teclado) reusando `wifi_manager.h`/`ui_wifi.cpp`.
- **Fase 4 — Device token:** tela de onboarding no gadget (teclado LVGL) → salva o código curto na NVS.
- **Fase 5 — API client:** `GET /gadget/dashboard` com o token → `ArduinoJson` parse do payload.
- **Fase 6 — Tela Dashboard:** elo (imagem por `rank.id`), `lv_bar` de RR, tendência. Com dados reais.
- **Fase 7 — Navegação + telas 2–4:** `lv_tabview` por swipe → Histórico, Gráfico (`lv_chart`), Status.
- **Fase 8 — Assets + estados:** ícones de elo/agente na LittleFS (por id); loading / erro / "desatualizado" (stale).
- **Fase 9 — Polling + extras:** atualizar a cada `poll` do payload; LED WS2812B opcional (cor do elo / pós-jogo).

## 12. Adaptações vs. a referência (roaster)

| Referência (esp32_controller) | Gadget Valorant |
|---|---|
| BLE (`ble_roaster.h`) | ❌ remover (não usa) |
| SD Card (`sd_storage.h`) | usar **LittleFS** (assets) + **NVS** (config) |
| Auth: email/senha → JWT | **device token** (Bearer), sem login |
| Tema laranja industrial | **violeta/vermelho** (paleta Valorant) |
| Telas de roast/perfis | Dashboard / Histórico / Gráfico / Status |

## 13. Armadilhas (lembrar)

- 🔴 **PSRAM OPI obrigatória** — sem ela, o `heap_caps_malloc` do framebuffer falha e a placa trava no boot.
- 🔴 **Usar o esquema do `.ino`** (Canvas rot=0 + rotação manual no flush), **não** o do `.bak` — senão cores erradas.
- 🔴 **USB CDC On Boot** habilitado, senão o `Serial` não aparece na USB nativa.
- 🟡 Display e touch precisam ter a **mesma orientação** (270°/rot=3 para USB à esquerda).
- 🟡 LVGL **9.2.x** — API nova (`lv_display_create`, `lv_tick_set_cb`); exemplos antigos (v8) não compilam.
