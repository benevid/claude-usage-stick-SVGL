#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// Claude Usage Stick — Guition JC4832W535 (ESP32-S3, AXS15231B)
// Pinos: ver firmware/REFERENCIA-HARDWARE-LVGL.md (bring-up validado)
// ============================================================

// ── Firmware ─────────────────────────────────────────────
#define FW_VERSION              "1.0-touch"

// ── Display QSPI (AXS15231B) ─────────────────────────────
#define TFT_CS    45
#define TFT_SCK   47
#define TFT_SDA0  21
#define TFT_SDA1  48
#define TFT_SDA2  40
#define TFT_SDA3  39
#define TFT_BL    1
#define TFT_TE    38

#define SCREEN_WIDTH   480
#define SCREEN_HEIGHT  320
#define QSPI_FREQ      40000000UL

// ── Touch I2C (AXS15231B) ────────────────────────────────
#define TOUCH_SDA  4
#define TOUCH_SCL  8
#define TOUCH_INT  3
#define TOUCH_ADDR 0x3B
// rotation=3 = USB à esquerda (casa com o flush 270° CW)
#define TOUCH_ROTATION 3

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
