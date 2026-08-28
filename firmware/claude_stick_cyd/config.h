#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// Claude Usage Stick — ESP32-2432S028 (CYD, ESP32 + ILI9341 + XPT2046)
// Pinos e calibração: ver firmware/bringup_cyd/config.h
// ============================================================

// ── Firmware ─────────────────────────────────────────────
#define FW_VERSION              "2.3-cyd"

// ── Display ILI9341 (SPI) ────────────────────────────────
// Também usados via -D no build.sh p/ configurar a TFT_eSPI (User_Setup
// "solto", sem tocar no arquivo global da lib — ver build.sh).
#define TFT_MISO_PIN   12
#define TFT_MOSI_PIN   13
#define TFT_SCLK_PIN   14
#define TFT_CS_PIN     15
#define TFT_DC_PIN      2
#define TFT_RST_PIN    -1   // não usado nesta placa
#define TFT_BL_PIN     21   // backlight, controlado por PWM (brilho ajustável)

#define SCREEN_WIDTH   320  // paisagem (nativo 240x320 retrato)
#define SCREEN_HEIGHT  240
#define TFT_ROTATION    1

// ── Touch XPT2046 (SPI separado do display) ──────────────
#define TOUCH_MOSI_PIN 32
#define TOUCH_MISO_PIN 39
#define TOUCH_CLK_PIN  25
#define TOUCH_CS_PIN   33
#define TOUCH_IRQ_PIN  36
// Calibração desta unidade (varia por placa — recalibrar se o touch desalinhar)
#define TS_MINX 300
#define TS_MAXX 3500
#define TS_MINY 430
#define TS_MAXY 3550

// ── Polling ──────────────────────────────────────────────
#define DEFAULT_POLL_SEC        120
#define MIN_POLL_SEC            30
#define MAX_POLL_SEC            300
#define STATUS_POLL_SEC         300      // status.claude.com a cada 5 min

// ── Segurança (PIN + AES-256-GCM) ────────────────────────
#define PIN_LEN                 4
#define MAX_PIN_ATTEMPTS        10
#define LOCKOUT_BASE_SEC        60       // dobra a cada falha
#define KDF_ROUNDS              10000

// ── Rede / API Claude ────────────────────────────────────
#define WIFI_CONNECT_TIMEOUT_MS 8000
#define API_TIMEOUT_MS          15000
#define MESSAGES_ENDPOINT       "https://api.anthropic.com/v1/messages"
#define ANTHROPIC_VERSION       "2023-06-01"
#define PROBE_MODEL             "claude-haiku-4-5-20251001"
// status.anthropic.com redireciona para cá — consultar o host canônico direto
#define STATUS_ENDPOINT         "https://status.claude.com/api/v2/incidents/unresolved.json"

// NTP (necessário para os contadores de reset)
#define NTP_SERVER_1            "pool.ntp.org"
#define NTP_SERVER_2            "time.cloudflare.com"

// ── NVS ──────────────────────────────────────────────────
#define NVS_NAMESPACE           "claude"

#endif // CONFIG_H
