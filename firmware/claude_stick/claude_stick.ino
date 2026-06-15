/**
 * Claude Usage Stick — tela touch LVGL
 * Placa: Guition JC4832W535 (ESP32-S3, AXS15231B QSPI), 480x320 paisagem.
 *
 * Mostra o uso de rate-limit do Claude Code (janelas 5h e 7d) lido dos headers
 * da api.anthropic.com, mais a saúde dos modelos (status.claude.com) numa
 * fileira de mascotes. Token OAuth digitado na tela e guardado cifrado
 * (AES-256-GCM, chave derivada de um PIN de 4 dígitos).
 *
 * Sem botão físico: navegação 100% touch.
 * Init de display/touch validado no bring-up (ver REFERENCIA-HARDWARE-LVGL.md).
 */
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <time.h>
#include <math.h>
#include "config.h"
#include "touch.h"
#include "wifi_manager.h"
#include "api.h"
#include "status.h"
#include "crypto.h"

// ---- Paleta (tema escuro, acento coral do Claude) ----
#define C_BG       0x0E0E10
#define C_SURFACE  0x1A1A1E
#define C_SURFACE2 0x24242A
#define C_BORDER   0x33333A
#define C_TEXT     0xECECEC
#define C_MUTED    0x9A9AA2
#define C_ACCENT   0xD97757   // coral Claude
#define C_OK       0x22C55E
#define C_WARN     0xF59E0B
#define C_BAD      0xEF4444

// ---- Hardware ----
Arduino_Canvas *gfx = nullptr;
static uint16_t *canvas_fb = nullptr;
AXS15231B_Touch touch_dev(TOUCH_SCL, TOUCH_SDA, TOUCH_INT, TOUCH_ADDR, TOUCH_ROTATION);
WiFiManager g_wifi;
Preferences g_prefs;

// ---- Estado da aplicação ----
enum State {
  ST_BOOT, ST_PIN, ST_SETUP_PIN, ST_WIFI, ST_TOKEN,
  ST_LOADING, ST_MAIN, ST_SETTINGS, ST_ERROR
};
static State g_state = ST_BOOT;
static State g_pending = ST_BOOT;
static bool  g_dirty = false;
static void request_state(State s) { g_pending = s; g_dirty = true; }

// ---- Dados ----
static UsageData   g_usage = {0, 0, 0, 0, false, ""};
static ModelStatus g_status = {true, true, true, true, false};

// ---- Token / segurança ----
static EncryptedBlob g_blob;
static bool g_hasToken = false;              // existe blob salvo no NVS
static bool g_onboarding = false;            // primeiro setup em andamento
static char g_token[200] = {0};              // token decifrado (só em RAM)
static char g_pendingToken[200] = {0};       // token digitado, aguardando PIN
static char g_pinEntry[PIN_LEN + 1] = {0};   // dígitos sendo digitados
static char g_pinFirst[PIN_LEN + 1] = {0};   // 1ª entrada no setup de PIN
static bool g_pinConfirming = false;         // setup: confirmando 2ª vez
static int  g_pinAttempts = 0;               // tentativas erradas (persistido)
static uint32_t g_lockoutUntil = 0;          // millis até liberar nova tentativa
static bool g_timeInit = false;

// ---- Refresh em background ----
static bool g_wantRefresh = false;        // botão de refresh pediu atualização
static bool g_refreshing = false;         // busca em andamento
static bool g_lastFetchOk = true;         // último fetch deu certo?
static uint32_t g_lastOkMs = 0;           // millis do último sucesso (p/ "atualizado há Xs")
static lv_obj_t *g_hdrStatus = nullptr;   // texto de status no cabeçalho do dashboard

// ---- Brilho ----
static const uint8_t BRI_LEVELS[3] = {60, 160, 255};
static int g_briIdx = 1;

static uint32_t g_lastPollMs = 0;         // millis do último poll (p/ barra de refresh)
static int g_pollSec = DEFAULT_POLL_SEC;  // intervalo de atualização (config, NVS)
static int g_tzOffset = -3;               // fuso GMT (horas), config NVS

// ---- Histórico (ring buffer; persistido em LittleFS) ----
#define HIST_MAX 120
struct Sample { uint32_t t; uint8_t h5; uint8_t d7; };   // t = epoch (0 = relógio não sincronizado)
static Sample g_hist[HIST_MAX];
static int g_histN = 0;
static int g_histHead = 0;
static float g_hourBurn[24] = {0};   // consumo por hora do dia (heatmap)
static float g_lastH5 = -1.0f;       // última utilização 5h (delta do heatmap)

// ---- Mascotes Clawd (corpo + braços/pernas; animados) ----
struct Mascot { lv_obj_t *cont, *eye[2]; int baseY, eyeH; bool up; };
static Mascot g_masc[8];
static int g_mascN = 0;

// ---- Ponteiros de UI do dashboard (zerados a cada build de ST_MAIN) ----
#define NTILES 6
struct DashUI {
  lv_obj_t *tv, *tile[NTILES], *dots[NTILES];
  lv_obj_t *refBar, *hdrChip;
  // overview
  lv_obj_t *ovChip, *ovPct5, *ovBar5, *ovEta5, *ovPct7, *ovBar7, *ovEta7;
  // reset clocks
  lv_obj_t *rcBig5, *rcAt5, *rcChip5, *rcBig7, *rcAt7, *rcChip7;
  // models
  lv_obj_t *incident;
  // tendência
  lv_obj_t *chart, *burn; lv_chart_series_t *ser5, *ser7;
  // heatmap por hora
  lv_obj_t *heat[24];
  // detalhes (minimalista)
  lv_obj_t *dvClaim, *dvOverage;
};
static DashUI g_ui;
static lv_obj_t *g_pinDots = nullptr, *g_pinMsg = nullptr;

// ---- Forward declarations ----
static void render_state();
static void refresh_ui_values();
static void dash_tick();
static void set_hdr_status();
static void apply_tz();
static void ui_pin();
static void ui_wifi();
static void ui_token();
static void ui_loading(const char *sub);
static void ui_main();
static void ui_settings();
static void ui_message(const char *title, const char *sub, uint32_t color);
static void nav_cb(lv_event_t *e);

// ============================================================
// Pipeline de display/touch (validado no bring-up)
// ============================================================
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint16_t *src = (uint16_t *)px_map;
  for (int ly = 0; ly < SCREEN_HEIGHT; ly++) {
    uint16_t *src_row = src + ly * SCREEN_WIDTH;
    for (int lx = 0; lx < SCREEN_WIDTH; lx++)
      canvas_fb[(479 - lx) * 320 + ly] = src_row[lx];
  }
  gfx->flush();
  lv_disp_flush_ready(disp);
}
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
  uint16_t x, y;
  if (touch_dev.touched()) {
    touch_dev.readData(&x, &y);
    data->point.x = x; data->point.y = y;
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// ============================================================
// Helpers de UI
// ============================================================
static lv_obj_t *mklabel(lv_obj_t *p, const char *txt, const lv_font_t *font, uint32_t color) {
  lv_obj_t *l = lv_label_create(p);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
  return l;
}
static void no_box(lv_obj_t *o) {
  lv_obj_set_style_bg_opa(o, 0, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}
static lv_obj_t *mkcard(lv_obj_t *p, int w, int h) {
  lv_obj_t *c = lv_obj_create(p);
  lv_obj_set_size(c, w, h);
  lv_obj_set_style_bg_color(c, lv_color_hex(C_SURFACE), 0);
  lv_obj_set_style_border_color(c, lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_radius(c, 12, 0);
  lv_obj_set_style_pad_all(c, 12, 0);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  return c;
}
// Botão pílula com label centralizado; user_data leva o State alvo (nav_cb).
static lv_obj_t *mkbtn(lv_obj_t *p, const char *txt, const lv_font_t *font,
                       uint32_t bg, uint32_t fg) {
  lv_obj_t *b = lv_button_create(p);
  lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
  lv_obj_set_style_radius(b, 10, 0);
  lv_obj_center(mklabel(b, txt, font, fg));
  return b;
}
static uint32_t pct_color(float p) {
  if (p < 70.0f) return C_OK;
  if (p < 90.0f) return C_WARN;
  return C_BAD;
}
// "reseta em 1h 23m" / "2d 4h" / "agora" / "--" (relógio não sincronizado)
static void fmt_eta(uint32_t epoch, char *out, int sz) {
  time_t now = time(nullptr);
  if (now < 1000000000L || epoch == 0) { snprintf(out, sz, "--"); return; }
  long d = (long)epoch - (long)now;
  if (d <= 0) { snprintf(out, sz, "agora"); return; }
  int days = d / 86400; d %= 86400;
  int hrs  = d / 3600;  d %= 3600;
  int mins = d / 60;
  if (days > 0)      snprintf(out, sz, "%dd %dh", days, hrs);
  else if (hrs > 0)  snprintf(out, sz, "%dh %02dm", hrs, mins);
  else               snprintf(out, sz, "%dm", mins);
}

// ============================================================
// NVS / persistência
// ============================================================
static void load_persisted() {
  g_prefs.begin(NVS_NAMESPACE, false);
  size_t n = g_prefs.getBytesLength("blob");
  if (n == sizeof(EncryptedBlob)) {
    g_prefs.getBytes("blob", &g_blob, sizeof(EncryptedBlob));
    g_hasToken = true;
  }
  g_pinAttempts = g_prefs.getInt("pinatt", 0);
  g_briIdx = g_prefs.getInt("bri", 1);
  if (g_briIdx < 0 || g_briIdx > 2) g_briIdx = 1;
  g_pollSec = g_prefs.getInt("poll", DEFAULT_POLL_SEC);
  if (g_pollSec < MIN_POLL_SEC || g_pollSec > MAX_POLL_SEC) g_pollSec = DEFAULT_POLL_SEC;
  g_tzOffset = g_prefs.getInt("tz", -3);
  if (g_tzOffset < -12 || g_tzOffset > 14) g_tzOffset = -3;
}
static void save_blob() { g_prefs.putBytes("blob", &g_blob, sizeof(EncryptedBlob)); }
static void save_attempts() { g_prefs.putInt("pinatt", g_pinAttempts); }
static void apply_brightness() { ledcWrite(TFT_BL, BRI_LEVELS[g_briIdx]); }

static void factory_reset() {
  g_prefs.clear();              // apaga blob, pinatt, bri do namespace claude
  g_wifi.forgetAll();
  g_hasToken = false;
  g_token[0] = 0; g_pendingToken[0] = 0;
  g_pinAttempts = 0;
  g_onboarding = true;
  Serial.println("[RESET] tudo apagado");
}

// ============================================================
// Tela: PIN (keypad touch) — entra PIN p/ decifrar OU define novo no setup
// ============================================================
static const char *pin_map[] = {
  "1", "2", "3", "\n",
  "4", "5", "6", "\n",
  "7", "8", "9", "\n",
  LV_SYMBOL_LEFT, "0", LV_SYMBOL_OK, ""
};

static void pin_update_dots() {
  if (!g_pinDots) return;
  char dots[24] = {0};
  int len = strlen(g_pinEntry);
  for (int i = 0; i < PIN_LEN; i++) {
    strcat(dots, i < len ? "*" : "_");
    if (i < PIN_LEN - 1) strcat(dots, " ");
  }
  lv_label_set_text(g_pinDots, dots);
}

static void pin_submit() {
  if (g_state == ST_SETUP_PIN) {
    if (!g_pinConfirming) {
      strlcpy(g_pinFirst, g_pinEntry, sizeof(g_pinFirst));
      g_pinConfirming = true;
      g_pinEntry[0] = 0;
      pin_update_dots();
      if (g_pinMsg) lv_label_set_text(g_pinMsg, "Confirme o PIN");
      return;
    }
    // confirmando
    if (strcmp(g_pinFirst, g_pinEntry) != 0) {
      g_pinConfirming = false;
      g_pinFirst[0] = 0; g_pinEntry[0] = 0;
      pin_update_dots();
      if (g_pinMsg) lv_label_set_text(g_pinMsg, "Nao bateu. Defina de novo.");
      return;
    }
    // PIN definido -> cifra o token pendente e salva
    if (!encryptToken(g_pendingToken, g_pinEntry, g_blob)) {
      if (g_pinMsg) lv_label_set_text(g_pinMsg, "Falha ao cifrar. Tente de novo.");
      g_pinConfirming = false; g_pinFirst[0] = 0; g_pinEntry[0] = 0; pin_update_dots();
      return;
    }
    save_blob();
    strlcpy(g_token, g_pendingToken, sizeof(g_token));
    memset(g_pendingToken, 0, sizeof(g_pendingToken));
    g_hasToken = true; g_onboarding = false;
    g_pinAttempts = 0; save_attempts();
    g_pinConfirming = false; g_pinFirst[0] = 0; g_pinEntry[0] = 0;
    Serial.println("[PIN] token cifrado e salvo");
    request_state(g_wifi.isConnected() ? ST_LOADING : ST_WIFI);
    return;
  }

  // ST_PIN: tenta decifrar
  if (decryptToken(g_blob, g_pinEntry, g_token, sizeof(g_token))) {
    g_pinAttempts = 0; save_attempts();
    g_pinEntry[0] = 0;
    Serial.printf("[PIN] ok, token %d chars\n", (int)strlen(g_token));
    if (!g_wifi.isConnected()) g_wifi.autoConnect(WIFI_CONNECT_TIMEOUT_MS);
    request_state(g_wifi.isConnected() ? ST_LOADING : ST_WIFI);
  } else {
    g_pinAttempts++; save_attempts();
    g_pinEntry[0] = 0; pin_update_dots();
    if (g_pinAttempts >= MAX_PIN_ATTEMPTS) {
      Serial.println("[PIN] limite estourado -> wipe");
      factory_reset();
      request_state(ST_WIFI);
      return;
    }
    int wait = LOCKOUT_BASE_SEC * (1 << (g_pinAttempts - 1));
    if (wait > 3600) wait = 3600;
    g_lockoutUntil = millis() + (uint32_t)wait * 1000;
    if (g_pinMsg) {
      char m[64];
      snprintf(m, sizeof(m), "PIN errado (%d/%d). Aguarde %ds",
               g_pinAttempts, MAX_PIN_ATTEMPTS, wait);
      lv_label_set_text(g_pinMsg, m);
    }
  }
}

static void pin_kb_cb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  if (millis() < g_lockoutUntil) return;     // travado
  lv_obj_t *bm = (lv_obj_t *)lv_event_get_target(e);
  uint32_t id = lv_buttonmatrix_get_selected_button(bm);
  const char *txt = lv_buttonmatrix_get_button_text(bm, id);
  if (!txt) return;
  int len = strlen(g_pinEntry);
  if (strcmp(txt, LV_SYMBOL_LEFT) == 0) {
    if (len > 0) g_pinEntry[len - 1] = 0;
    pin_update_dots();
  } else if (strcmp(txt, LV_SYMBOL_OK) == 0) {
    if (len == PIN_LEN) pin_submit();
  } else if (len < PIN_LEN) {
    g_pinEntry[len] = txt[0];
    g_pinEntry[len + 1] = 0;
    pin_update_dots();
    if (len + 1 == PIN_LEN) pin_submit();     // auto-submit ao completar
  }
}

static void ui_pin() {
  lv_obj_t *scr = lv_screen_active();
  const char *title = (g_state == ST_SETUP_PIN)
    ? (g_pinConfirming ? "Confirme o PIN" : "Defina um PIN")
    : "Digite o PIN";
  lv_obj_t *t = mklabel(scr, title, &lv_font_montserrat_22, C_TEXT);
  lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 14);

  g_pinDots = mklabel(scr, "", &lv_font_montserrat_28, C_ACCENT);
  lv_obj_align(g_pinDots, LV_ALIGN_TOP_MID, 0, 48);
  pin_update_dots();

  const char *sub = (g_state == ST_SETUP_PIN)
    ? "Voce vai digita-lo a cada boot."
    : "Necessario para desbloquear o token.";
  g_pinMsg = mklabel(scr, sub, &lv_font_montserrat_14, C_MUTED);
  lv_obj_align(g_pinMsg, LV_ALIGN_TOP_MID, 0, 86);

  lv_obj_t *bm = lv_buttonmatrix_create(scr);
  lv_buttonmatrix_set_map(bm, pin_map);
  lv_obj_set_size(bm, 280, 180);
  lv_obj_align(bm, LV_ALIGN_BOTTOM_MID, 0, -10);
  lv_obj_set_style_bg_color(bm, lv_color_hex(C_BG), 0);
  lv_obj_set_style_border_width(bm, 0, 0);
  lv_obj_set_style_text_font(bm, &lv_font_montserrat_24, 0);
  lv_obj_set_style_bg_color(bm, lv_color_hex(C_SURFACE2), LV_PART_ITEMS);
  lv_obj_set_style_text_color(bm, lv_color_hex(C_TEXT), LV_PART_ITEMS);
  lv_obj_add_event_cb(bm, pin_kb_cb, LV_EVENT_VALUE_CHANGED, NULL);

  if (millis() < g_lockoutUntil && g_pinMsg) {
    int rem = (g_lockoutUntil - millis()) / 1000;
    char m[48]; snprintf(m, sizeof(m), "Aguarde %ds", rem);
    lv_label_set_text(g_pinMsg, m);
  }
}

// ============================================================
// Tela: WiFi (scan + teclado)
// ============================================================
static lv_obj_t *wifi_list = nullptr, *wifi_ta = nullptr, *wifi_kb = nullptr, *wifi_status = nullptr;
static char sel_ssid[33] = {0};

static void wifi_populate() {
  lv_obj_clean(wifi_list);
  lv_label_set_text(wifi_status, "Escaneando redes...");
  lv_refr_now(NULL);
  WiFiManager::NetworkInfo nets[12];
  int n = g_wifi.scanNetworks(nets, 12);
  for (int i = 0; i < n; i++) {
    lv_obj_t *b = lv_list_add_button(wifi_list, LV_SYMBOL_WIFI, nets[i].ssid);
    lv_obj_set_style_bg_color(b, lv_color_hex(C_SURFACE), 0);
    lv_obj_set_style_text_color(b, lv_color_hex(C_TEXT), 0);
    lv_obj_add_event_cb(b, wifi_item_cb, LV_EVENT_CLICKED, NULL);  // clique direto no botão
  }
  lv_label_set_text(wifi_status, n > 0 ? "Toque na sua rede" : "Nenhuma rede. Toque em Reescanear.");
}
static void wifi_item_cb(lv_event_t *e) {
  lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
  const char *txt = lv_list_get_button_text(wifi_list, btn);
  if (!txt) return;
  strlcpy(sel_ssid, txt, sizeof(sel_ssid));
  lv_label_set_text_fmt(wifi_status, "Senha de \"%s\":", sel_ssid);
  lv_obj_add_flag(wifi_list, LV_OBJ_FLAG_HIDDEN);
  lv_textarea_set_text(wifi_ta, "");
  lv_obj_clear_flag(wifi_ta, LV_OBJ_FLAG_HIDDEN);
  lv_obj_clear_flag(wifi_kb, LV_OBJ_FLAG_HIDDEN);
  lv_keyboard_set_textarea(wifi_kb, wifi_ta);
}
static void wifi_kb_cb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_READY) {
    const char *pass = lv_textarea_get_text(wifi_ta);
    lv_label_set_text(wifi_status, "Conectando...");
    lv_obj_add_flag(wifi_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(wifi_ta, LV_OBJ_FLAG_HIDDEN);
    lv_refr_now(NULL);
    bool ok = g_wifi.connectTo(sel_ssid, pass, 15000);
    if (ok) request_state(g_onboarding ? ST_TOKEN : ST_LOADING);
    else { lv_label_set_text(wifi_status, "Falhou. Toque numa rede de novo."); lv_obj_clear_flag(wifi_list, LV_OBJ_FLAG_HIDDEN); }
  } else if (code == LV_EVENT_CANCEL) {
    lv_obj_add_flag(wifi_kb, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(wifi_ta, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(wifi_list, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(wifi_status, "Toque na sua rede");
  }
}
static void wifi_rescan_cb(lv_event_t *e) { (void)e; wifi_populate(); }

static void ui_wifi() {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_t *title = mklabel(scr, "Configurar WiFi", &lv_font_montserrat_20, C_TEXT);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 12);

  lv_obj_t *rb = mkbtn(scr, "Reescanear", &lv_font_montserrat_14, C_SURFACE2, C_ACCENT);
  lv_obj_align(rb, LV_ALIGN_TOP_RIGHT, -12, 8);
  lv_obj_add_event_cb(rb, wifi_rescan_cb, LV_EVENT_CLICKED, NULL);

  if (!g_onboarding && g_hasToken) {
    lv_obj_t *bk = mkbtn(scr, LV_SYMBOL_LEFT " Voltar", &lv_font_montserrat_14, C_SURFACE2, C_MUTED);
    lv_obj_align(bk, LV_ALIGN_TOP_RIGHT, -150, 8);
    lv_obj_add_event_cb(bk, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(g_usage.ok ? ST_MAIN : ST_SETTINGS));
  }

  wifi_status = mklabel(scr, "...", &lv_font_montserrat_14, C_MUTED);
  lv_obj_align(wifi_status, LV_ALIGN_TOP_LEFT, 14, 44);

  wifi_list = lv_list_create(scr);
  lv_obj_set_size(wifi_list, 452, 246);
  lv_obj_align(wifi_list, LV_ALIGN_TOP_MID, 0, 68);
  lv_obj_set_style_bg_color(wifi_list, lv_color_hex(C_BG), 0);
  lv_obj_set_style_border_color(wifi_list, lv_color_hex(C_BORDER), 0);
  // clique é anexado por botão em wifi_populate()

  wifi_ta = lv_textarea_create(scr);
  lv_textarea_set_one_line(wifi_ta, true);
  lv_textarea_set_password_mode(wifi_ta, true);
  lv_textarea_set_placeholder_text(wifi_ta, "senha do WiFi");
  lv_obj_set_size(wifi_ta, 452, 44);
  lv_obj_align(wifi_ta, LV_ALIGN_TOP_MID, 0, 66);
  lv_obj_add_flag(wifi_ta, LV_OBJ_FLAG_HIDDEN);

  wifi_kb = lv_keyboard_create(scr);
  lv_obj_add_flag(wifi_kb, LV_OBJ_FLAG_HIDDEN);
  lv_obj_add_event_cb(wifi_kb, wifi_kb_cb, LV_EVENT_ALL, NULL);

  wifi_populate();
}

// ============================================================
// Tela: token OAuth via WEB (cola do navegador na rede local)
// Digitar 100+ chars no touch é inviável -> sobe um WebServer e o usuário
// cola o token pelo PC/celular. Ao salvar, o device VALIDA com fetchUsage.
// ============================================================
static WebServer *g_web = nullptr;
static volatile bool g_tokenGot = false;
static lv_obj_t *g_tokMsg = nullptr;            // status na tela do device
static lv_point_precise_t g_markPts[6][2];      // pontos do "sol" do Claude (persistem)

static void stop_web() { if (g_web) { g_web->stop(); delete g_web; g_web = nullptr; } }

static void anim_opa_cb(void *o, int32_t v) { lv_obj_set_style_opa((lv_obj_t *)o, (lv_opa_t)v, 0); }

// Ícone do Claude: "sol" de 12 raios (6 diâmetros) em coral, com respiração.
static lv_obj_t *build_claude_mark(lv_obj_t *parent, int size) {
  lv_obj_t *c = lv_obj_create(parent);
  lv_obj_set_size(c, size, size);
  no_box(c);
  int cx = size / 2, cy = size / 2, r = size / 2 - 3;
  static const float ang[6] = {0, 30, 60, 90, 120, 150};
  for (int k = 0; k < 6; k++) {
    float a = ang[k] * 3.14159265f / 180.0f;
    int dx = (int)(r * cosf(a)), dy = (int)(r * sinf(a));
    g_markPts[k][0].x = cx - dx; g_markPts[k][0].y = cy - dy;
    g_markPts[k][1].x = cx + dx; g_markPts[k][1].y = cy + dy;
    lv_obj_t *ln = lv_line_create(c);
    lv_line_set_points(ln, g_markPts[k], 2);
    lv_obj_set_style_line_width(ln, size / 8, 0);
    lv_obj_set_style_line_color(ln, lv_color_hex(C_ACCENT), 0);
    lv_obj_set_style_line_rounded(ln, true, 0);
  }
  lv_anim_t a; lv_anim_init(&a);
  lv_anim_set_var(&a, c);
  lv_anim_set_exec_cb(&a, anim_opa_cb);
  lv_anim_set_values(&a, 110, 255);
  lv_anim_set_duration(&a, 900);
  lv_anim_set_playback_duration(&a, 900);
  lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
  lv_anim_start(&a);
  return c;
}

// ---- páginas HTML ----
#define WEB_CSS \
  ":root{--bg:#0E0E10;--card:#1A1A1E;--bd:#33333A;--tx:#ECECEC;--mut:#9A9AA2;--cor:#D97757}" \
  "*{box-sizing:border-box}" \
  "body{margin:0;background:var(--bg);color:var(--tx);font-family:-apple-system,Segoe UI,Roboto,sans-serif;" \
  "display:flex;min-height:100vh;align-items:center;justify-content:center}" \
  ".card{background:var(--card);border:1px solid var(--bd);border-radius:16px;padding:26px;max-width:520px;width:92%}" \
  "h1{font-size:19px;margin:0 0 6px;display:flex;align-items:center;gap:10px}" \
  "p{color:var(--mut);font-size:14px;line-height:1.5;margin:6px 0 14px}" \
  "textarea{width:100%;background:var(--bg);color:var(--tx);border:1px solid var(--bd);border-radius:10px;" \
  "padding:12px;font-family:ui-monospace,monospace;font-size:13px;min-height:96px;resize:vertical}" \
  "button{margin-top:14px;width:100%;background:var(--cor);color:#1A1A1E;border:0;border-radius:10px;" \
  "padding:14px;font-size:16px;font-weight:700;cursor:pointer}" \
  ".spark{width:26px;height:26px;flex:0 0 auto}code,a{color:var(--cor)}"

#define WEB_SPARK \
  "<svg class=spark viewBox='0 0 100 100'><g stroke='#D97757' stroke-width='12' stroke-linecap='round'>" \
  "<line x1=50 y1=9 x2=50 y2=91/><line x1=9 y1=50 x2=91 y2=50/>" \
  "<line x1=21 y1=21 x2=79 y2=79/><line x1=79 y1=21 x2=21 y2=79/>" \
  "<line x1=34 y1=11 x2=66 y2=89/><line x1=66 y1=11 x2=34 y2=89/></g></svg>"

static String web_form() {
  String h = F("<!doctype html><html lang=pt><head><meta charset=utf-8>"
               "<meta name=viewport content='width=device-width,initial-scale=1'>"
               "<title>Claude Usage Stick</title><style>" WEB_CSS "</style></head><body><div class=card>"
               "<h1>" WEB_SPARK " Claude Usage Stick</h1>"
               "<p>Cole o seu token OAuth do Claude (<code>sk-ant-oat01-...</code>) e toque em <b>Salvar</b>. "
               "O gadget vai <b>validar</b> o token e pedir um PIN na tela.</p>"
               "<form method=POST action='/token'>"
               "<textarea name=token placeholder='sk-ant-oat01-...' autocomplete=off autofocus></textarea>"
               "<button type=submit>Salvar e validar</button></form></div></body></html>");
  return h;
}
static String web_result(bool ok, const String &msg) {
  String h = F("<!doctype html><html lang=pt><head><meta charset=utf-8>"
               "<meta name=viewport content='width=device-width,initial-scale=1'>"
               "<title>Claude Usage Stick</title><style>" WEB_CSS "</style></head><body><div class=card>");
  if (ok) {
    h += F("<h1>" WEB_SPARK " Token validado</h1>"
           "<p>Token aceito pela API. Agora <b>defina um PIN de 4 dígitos</b> na tela do gadget para finalizar. "
           "Pode fechar esta página.</p>");
  } else {
    h += F("<h1>" WEB_SPARK " Token recusado</h1><p>");
    h += msg;
    h += F("</p><p><a href='/'>Voltar e tentar de novo</a></p>");
  }
  h += F("</div></body></html>");
  return h;
}

static void handleRoot()     { g_web->send(200, "text/html; charset=utf-8", web_form()); }
static void handleNotFound() { g_web->sendHeader("Location", "/"); g_web->send(302, "text/plain", ""); }

static void handleTokenPost() {
  String t = g_web->arg("token");
  t.trim();
  if (t.length() < 8) {
    if (g_tokMsg) lv_label_set_text(g_tokMsg, "token vazio");
    g_web->send(200, "text/html; charset=utf-8", web_result(false, "Token vazio ou muito curto."));
    return;
  }
  // feedback no device antes da chamada bloqueante
  if (g_tokMsg) { lv_label_set_text(g_tokMsg, "validando token..."); lv_refr_now(NULL); }

  UsageData tmp = {0, 0, 0, 0, false, ""};
  bool ok = fetchUsage(t.c_str(), tmp);
  if (ok) {
    strlcpy(g_pendingToken, t.c_str(), sizeof(g_pendingToken));
    g_usage = tmp;                              // já temos dados p/ o dashboard
    g_pinConfirming = false; g_pinFirst[0] = 0; g_pinEntry[0] = 0;
    g_tokenGot = true;                          // loop -> ST_SETUP_PIN
    if (g_tokMsg) lv_label_set_text(g_tokMsg, "token OK! defina o PIN");
    g_web->send(200, "text/html; charset=utf-8", web_result(true, ""));
  } else {
    String m = String("A API recusou o token (") + tmp.error + "). Confira e cole de novo.";
    if (g_tokMsg) lv_label_set_text(g_tokMsg, "token recusado, tente de novo");
    g_web->send(200, "text/html; charset=utf-8", web_result(false, m));
  }
}

static void ui_token() {
  stop_web();
  lv_obj_t *scr = lv_screen_active();

  if (!g_onboarding && g_hasToken) {
    lv_obj_t *bk = mkbtn(scr, LV_SYMBOL_LEFT " Voltar", &lv_font_montserrat_14, C_SURFACE2, C_MUTED);
    lv_obj_align(bk, LV_ALIGN_TOP_RIGHT, -12, 8);
    lv_obj_add_event_cb(bk, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(g_usage.ok ? ST_MAIN : ST_SETTINGS));
  }

  lv_obj_t *mark = build_claude_mark(scr, 88);
  lv_obj_align(mark, LV_ALIGN_TOP_MID, 0, 10);

  lv_obj_t *cap = mklabel(scr, "Cole o token pelo navegador, em:", &lv_font_montserrat_16, C_MUTED);
  lv_obj_align(cap, LV_ALIGN_TOP_MID, 0, 108);

  String url = String("http://") + WiFi.localIP().toString();
  lv_obj_t *ip = mklabel(scr, url.c_str(), &lv_font_montserrat_28, C_ACCENT);
  lv_obj_align(ip, LV_ALIGN_TOP_MID, 0, 132);

  lv_obj_t *hint = mklabel(scr, "abra esse endereco no PC/celular na MESMA rede WiFi",
                           &lv_font_montserrat_12, C_MUTED);
  lv_obj_align(hint, LV_ALIGN_TOP_MID, 0, 174);

  lv_obj_t *sp = lv_spinner_create(scr);
  lv_spinner_set_anim_params(sp, 1200, 70);
  lv_obj_set_size(sp, 36, 36);
  lv_obj_align(sp, LV_ALIGN_BOTTOM_MID, 0, -46);
  lv_obj_set_style_arc_color(sp, lv_color_hex(C_SURFACE2), LV_PART_MAIN);
  lv_obj_set_style_arc_color(sp, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(sp, 5, LV_PART_MAIN);
  lv_obj_set_style_arc_width(sp, 5, LV_PART_INDICATOR);

  g_tokMsg = mklabel(scr, "aguardando o token...", &lv_font_montserrat_14, C_MUTED);
  lv_obj_align(g_tokMsg, LV_ALIGN_BOTTOM_MID, 0, -14);

  // sobe o servidor web
  g_web = new WebServer(80);
  g_web->on("/", HTTP_GET, handleRoot);
  g_web->on("/token", HTTP_POST, handleTokenPost);
  g_web->onNotFound(handleNotFound);
  g_web->begin();
  Serial.printf("[WEB] servidor em %s\n", url.c_str());
}

// ============================================================
// Tela: loading / mensagem
// ============================================================
static void ui_message(const char *title, const char *sub, uint32_t color) {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_t *t = mklabel(scr, title, &lv_font_montserrat_24, color);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, -16);
  if (sub && sub[0]) {
    lv_obj_t *s = mklabel(scr, sub, &lv_font_montserrat_16, C_MUTED);
    lv_obj_align(s, LV_ALIGN_CENTER, 0, 20);
  }
}
static void ui_loading(const char *sub) {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_t *mark = build_claude_mark(scr, 80);
  lv_obj_align(mark, LV_ALIGN_CENTER, 0, -40);
  lv_obj_t *t = mklabel(scr, "Carregando seu uso...", &lv_font_montserrat_18, C_TEXT);
  lv_obj_align(t, LV_ALIGN_CENTER, 0, 28);
  if (sub && sub[0]) {
    lv_obj_t *s = mklabel(scr, sub, &lv_font_montserrat_12, C_MUTED);
    lv_obj_align(s, LV_ALIGN_CENTER, 0, 52);
  }
  lv_obj_t *spn = lv_spinner_create(scr);
  lv_spinner_set_anim_params(spn, 1200, 70);
  lv_obj_set_size(spn, 34, 34);
  lv_obj_align(spn, LV_ALIGN_CENTER, 0, 90);
  lv_obj_set_style_arc_color(spn, lv_color_hex(C_SURFACE2), LV_PART_MAIN);
  lv_obj_set_style_arc_color(spn, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(spn, 4, LV_PART_MAIN);
  lv_obj_set_style_arc_width(spn, 4, LV_PART_INDICATOR);
}

// ============================================================
// Tela: dashboard (uso 5h/7d + mascotes)
// ============================================================
// ---- Helpers: card, chip, mascote, histórico ----
static uint32_t status_color(const char *s) {
  if (!s || !s[0]) return C_MUTED;
  if (!strcmp(s, "rejected") || !strcmp(s, "rate_limited") || !strcmp(s, "exceeded")) return C_BAD;
  if (strstr(s, "warning")) return C_WARN;
  return C_OK;
}
static void hist_push(float h5, float d7) {
  time_t now = time(nullptr);
  g_hist[g_histHead].t  = (now > 1000000000L) ? (uint32_t)now : 0;
  g_hist[g_histHead].h5 = (uint8_t)(h5 + 0.5f);
  g_hist[g_histHead].d7 = (uint8_t)(d7 + 0.5f);
  g_histHead = (g_histHead + 1) % HIST_MAX;
  if (g_histN < HIST_MAX) g_histN++;
}
static int hist_idx(int i) { return (g_histHead - g_histN + i + HIST_MAX * 2) % HIST_MAX; }

// Heatmap: atribui o consumo (Δ utilização 5h) à hora do dia local.
static void accumulate_heat(float h5) {
  time_t now = time(nullptr);
  if (g_lastH5 >= 0 && now > 1000000000L) {
    float d = h5 - g_lastH5;
    if (d > 0 && d < 100) { struct tm tmv; localtime_r(&now, &tmv); g_hourBurn[tmv.tm_hour] += d; }
  }
  g_lastH5 = h5;
}

// Persistência do histórico/heatmap em LittleFS (sobrevive reboot).
struct HistFile { uint32_t magic; int n, head; Sample hist[HIST_MAX]; float hourBurn[24]; float lastH5; };
static void save_history() {
  File f = LittleFS.open("/hist.bin", "w");
  if (!f) return;
  HistFile hf;
  hf.magic = 0xC1A0DE01; hf.n = g_histN; hf.head = g_histHead;
  memcpy(hf.hist, g_hist, sizeof(g_hist));
  memcpy(hf.hourBurn, g_hourBurn, sizeof(g_hourBurn));
  hf.lastH5 = g_lastH5;
  f.write((uint8_t *)&hf, sizeof(hf));
  f.close();
}
static void load_history() {
  File f = LittleFS.open("/hist.bin", "r");
  if (!f) return;
  HistFile hf;
  if (f.read((uint8_t *)&hf, sizeof(hf)) == (int)sizeof(hf) && hf.magic == 0xC1A0DE01) {
    g_histN = hf.n; g_histHead = hf.head;
    memcpy(g_hist, hf.hist, sizeof(g_hist));
    memcpy(g_hourBurn, hf.hourBurn, sizeof(g_hourBurn));
    g_lastH5 = hf.lastH5;
  }
  f.close();
}

static void burn_text(char *out, size_t sz) {
  if (g_histN < 2) { strlcpy(out, "Coletando dados de uso... (~alguns minutos)", sz); return; }
  Sample a = g_hist[hist_idx(0)], b = g_hist[hist_idx(g_histN - 1)];
  if (a.t == 0 || b.t == 0 || b.t <= a.t) { strlcpy(out, "Coletando dados de uso...", sz); return; }
  float dt = (b.t - a.t) / 60.0f;                 // minutos
  if (dt < 0.5f) { strlcpy(out, "Coletando dados de uso...", sz); return; }
  float rate = (b.h5 - a.h5) / dt;                // %/min na janela 5h
  if (rate <= 0.02f) { strlcpy(out, "Uso estavel no momento.", sz); return; }
  float mins = (100.0f - b.h5) / rate;
  snprintf(out, sz, "No ritmo atual, a janela 5h enche em ~%dh%02dm.", (int)mins / 60, (int)mins % 60);
}
static void fmt_clock(uint32_t epoch, char *out, int sz) {
  if (epoch == 0 || time(nullptr) < 1000000000L) { strlcpy(out, "--:--", sz); return; }
  time_t t = (time_t)epoch; struct tm tmv;
  localtime_r(&t, &tmv);
  strftime(out, sz, "%a %H:%M", &tmv);
}

// label posicionado vazio (preenchido em refresh_ui_values/dash_tick)
static lv_obj_t *tlabel(lv_obj_t *p, const lv_font_t *f, uint32_t c, int x, int y) {
  lv_obj_t *l = lv_label_create(p);
  lv_obj_set_style_text_font(l, f, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(c), 0);
  lv_label_set_text(l, "");
  lv_obj_set_pos(l, x, y);
  return l;
}
static lv_obj_t *tstatic(lv_obj_t *p, const char *txt, const lv_font_t *f, uint32_t c, int x, int y) {
  lv_obj_t *l = mklabel(p, txt, f, c);
  lv_obj_set_pos(l, x, y);
  return l;
}
static void tile_setup(lv_obj_t *t) {
  lv_obj_set_style_bg_opa(t, 0, 0);
  lv_obj_set_style_border_width(t, 0, 0);
  lv_obj_set_style_pad_all(t, 0, 0);
  lv_obj_clear_flag(t, LV_OBJ_FLAG_SCROLLABLE);
}
// card moderno (superfície arredondada)
static lv_obj_t *card(lv_obj_t *p, int x, int y, int w, int h) {
  lv_obj_t *c = lv_obj_create(p);
  lv_obj_set_pos(c, x, y); lv_obj_set_size(c, w, h);
  lv_obj_set_style_bg_color(c, lv_color_hex(C_SURFACE), 0);
  lv_obj_set_style_border_color(c, lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_radius(c, 16, 0);
  lv_obj_set_style_pad_all(c, 12, 0);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  return c;
}
// pílula de status (chip)
static lv_obj_t *mkchip(lv_obj_t *p, int x, int y) {
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, LV_SIZE_CONTENT, 26);
  lv_obj_set_style_radius(o, 13, 0);
  lv_obj_set_style_pad_hor(o, 12, 0);
  lv_obj_set_style_pad_ver(o, 0, 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_t *l = lv_label_create(o);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_label_set_text(l, ""); lv_obj_center(l);
  return o;
}
static void set_chip(lv_obj_t *o, const char *txt, uint32_t col) {
  if (!o) return;
  lv_obj_set_style_bg_color(o, lv_color_hex(col), 0);
  lv_obj_t *l = lv_obj_get_child(o, 0);
  if (l) { lv_label_set_text(l, txt[0] ? txt : "--"); lv_obj_set_style_text_color(l, lv_color_hex(C_BG), 0); }
}
// peça retangular arredondada do mascote
static lv_obj_t *rrect(lv_obj_t *p, int x, int y, int w, int h, int r, uint32_t col) {
  lv_obj_t *o = lv_obj_create(p);
  lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, r, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(col), 0);
  lv_obj_set_style_border_width(o, 0, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
  return o;
}
// Mascote Clawd: corpo arredondado + olhos + braços/pernas retangulares.
// Online = coral; offline = cinza/translúcido + olhos fechados. Anima no loop (bob + pisca).
static void build_mascot(lv_obj_t *parent, int cx, int cy, bool big, const char *name, bool up) {
  if (g_mascN >= 8) return;
  float s = big ? 1.0f : 0.74f;
  #define SC(v) ((int)((v) * s))
  int CW = SC(64), CH = SC(74);
  lv_obj_t *c = lv_obj_create(parent);
  lv_obj_set_pos(c, cx, cy); lv_obj_set_size(c, CW, CH);
  lv_obj_set_style_bg_opa(c, 0, 0); lv_obj_set_style_border_width(c, 0, 0);
  lv_obj_set_style_pad_all(c, 0, 0); lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  if (!up) lv_obj_set_style_opa(c, 150, 0);
  uint32_t body = up ? C_ACCENT : C_BORDER, dark = up ? 0x141414 : C_MUTED;
  rrect(c, SC(1),  SC(28), SC(9),  SC(20), SC(4),  body);   // braço esq
  rrect(c, SC(54), SC(28), SC(9),  SC(20), SC(4),  body);   // braço dir
  rrect(c, SC(18), SC(50), SC(9),  SC(22), SC(4),  body);   // perna esq
  rrect(c, SC(37), SC(50), SC(9),  SC(22), SC(4),  body);   // perna dir
  rrect(c, SC(8),  SC(6),  SC(48), SC(46), SC(15), body);   // corpo/cabeça
  g_masc[g_mascN].eye[0] = rrect(c, SC(22), SC(22), SC(6), up ? SC(9) : SC(3), SC(3), dark);
  g_masc[g_mascN].eye[1] = rrect(c, SC(36), SC(22), SC(6), up ? SC(9) : SC(3), SC(3), dark);
  rrect(c, SC(26), SC(38), SC(12), SC(3), SC(2), dark);     // boca
  lv_obj_t *l = mklabel(parent, name, big ? &lv_font_montserrat_16 : &lv_font_montserrat_14, up ? C_TEXT : C_MUTED);
  lv_obj_set_width(l, CW + 24);
  lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(l, cx + CW / 2 - (CW + 24) / 2, cy + CH + 2);
  g_masc[g_mascN].cont = c; g_masc[g_mascN].baseY = cy; g_masc[g_mascN].up = up;
  g_masc[g_mascN].eyeH = up ? SC(9) : SC(3);
  g_mascN++;
  #undef SC
}

// ---- Builders de cada tile (cards; valores via refresh_ui_values) ----
static void mkbar(lv_obj_t *parent, lv_obj_t **slot, int y) {
  lv_obj_t *bar = lv_bar_create(parent);
  lv_obj_set_pos(bar, 0, y); lv_obj_set_size(bar, 196, 10);
  lv_bar_set_range(bar, 0, 100);
  lv_obj_set_style_bg_color(bar, lv_color_hex(C_SURFACE2), LV_PART_MAIN);
  lv_obj_set_style_radius(bar, 5, LV_PART_MAIN);
  lv_obj_set_style_radius(bar, 5, LV_PART_INDICATOR);
  *slot = bar;
}
static void build_tile_overview(lv_obj_t *t) {
  g_ui.ovChip = mkchip(t, 12, 2);
  lv_obj_t *c5 = card(t, 8, 34, 224, 104);
  tstatic(c5, "5 horas", &lv_font_montserrat_14, C_MUTED, 0, 0);
  g_ui.ovPct5 = tlabel(c5, &lv_font_montserrat_28, C_OK, 0, 14);
  mkbar(c5, &g_ui.ovBar5, 50);
  g_ui.ovEta5 = tlabel(c5, &lv_font_montserrat_12, C_MUTED, 0, 66);
  lv_obj_t *c7 = card(t, 248, 34, 224, 104);
  tstatic(c7, "7 dias", &lv_font_montserrat_14, C_MUTED, 0, 0);
  g_ui.ovPct7 = tlabel(c7, &lv_font_montserrat_28, C_OK, 0, 14);
  mkbar(c7, &g_ui.ovBar7, 50);
  g_ui.ovEta7 = tlabel(c7, &lv_font_montserrat_12, C_MUTED, 0, 66);
  build_mascot(t, 28,  150, false, "Haiku",  g_status.haikuUp);
  build_mascot(t, 142, 150, false, "Sonnet", g_status.sonnetUp);
  build_mascot(t, 256, 150, false, "Opus",   g_status.opusUp);
  build_mascot(t, 370, 150, false, "Fable",  g_status.fableUp);
}
static void build_tile_reset(lv_obj_t *t) {
  lv_obj_t *c5 = card(t, 8, 6, 464, 104);
  tstatic(c5, "Janela de 5 horas", &lv_font_montserrat_14, C_MUTED, 0, 0);
  g_ui.rcBig5 = tlabel(c5, &lv_font_montserrat_28, C_TEXT, 0, 18);
  g_ui.rcAt5  = tlabel(c5, &lv_font_montserrat_14, C_MUTED, 0, 56);
  g_ui.rcChip5 = mkchip(c5, 330, 4);
  lv_obj_t *c7 = card(t, 8, 120, 464, 104);
  tstatic(c7, "Janela de 7 dias", &lv_font_montserrat_14, C_MUTED, 0, 0);
  g_ui.rcBig7 = tlabel(c7, &lv_font_montserrat_28, C_TEXT, 0, 18);
  g_ui.rcAt7  = tlabel(c7, &lv_font_montserrat_14, C_MUTED, 0, 56);
  g_ui.rcChip7 = mkchip(c7, 330, 4);
}
static void build_tile_models(lv_obj_t *t) {
  tstatic(t, "Status dos modelos", &lv_font_montserrat_16, C_TEXT, 14, 4);
  build_mascot(t, 28,  34, true, "Haiku",  g_status.haikuUp);
  build_mascot(t, 142, 34, true, "Sonnet", g_status.sonnetUp);
  build_mascot(t, 256, 34, true, "Opus",   g_status.opusUp);
  build_mascot(t, 370, 34, true, "Fable",  g_status.fableUp);
  g_ui.incident = tlabel(t, &lv_font_montserrat_14, C_MUTED, 14, 200);
  lv_obj_set_width(g_ui.incident, 452);
  lv_label_set_long_mode(g_ui.incident, LV_LABEL_LONG_WRAP);
}
static void build_tile_trend(lv_obj_t *t) {
  tstatic(t, "Tendencia de uso", &lv_font_montserrat_16, C_TEXT, 14, 4);
  g_ui.chart = lv_chart_create(t);
  lv_obj_set_pos(g_ui.chart, 14, 30); lv_obj_set_size(g_ui.chart, 452, 150);
  lv_chart_set_type(g_ui.chart, LV_CHART_TYPE_LINE);
  lv_chart_set_range(g_ui.chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
  lv_chart_set_point_count(g_ui.chart, 40);
  lv_chart_set_div_line_count(g_ui.chart, 3, 0);
  lv_obj_set_style_bg_color(g_ui.chart, lv_color_hex(C_SURFACE), 0);
  lv_obj_set_style_border_color(g_ui.chart, lv_color_hex(C_BORDER), 0);
  lv_obj_set_style_border_width(g_ui.chart, 1, 0);
  lv_obj_set_style_radius(g_ui.chart, 12, 0);
  lv_obj_set_style_line_color(g_ui.chart, lv_color_hex(C_BORDER), LV_PART_MAIN);
  lv_obj_set_style_width(g_ui.chart, 0, LV_PART_INDICATOR);   // sem pontos
  lv_obj_set_style_height(g_ui.chart, 0, LV_PART_INDICATOR);
  lv_obj_set_style_line_width(g_ui.chart, 3, LV_PART_ITEMS);
  g_ui.ser5 = lv_chart_add_series(g_ui.chart, lv_color_hex(C_ACCENT), LV_CHART_AXIS_PRIMARY_Y);
  g_ui.ser7 = lv_chart_add_series(g_ui.chart, lv_color_hex(C_OK), LV_CHART_AXIS_PRIMARY_Y);
  tstatic(t, "5h", &lv_font_montserrat_14, C_ACCENT, 14, 186);
  tstatic(t, "7d", &lv_font_montserrat_14, C_OK, 56, 186);
  g_ui.burn = tlabel(t, &lv_font_montserrat_14, C_WARN, 14, 212);
  lv_obj_set_width(g_ui.burn, 452);
  lv_label_set_long_mode(g_ui.burn, LV_LABEL_LONG_WRAP);
}
static void build_tile_heat(lv_obj_t *t) {
  tstatic(t, "Uso por hora do dia", &lv_font_montserrat_16, C_TEXT, 14, 4);
  tstatic(t, "quanto voce queima de quota em cada hora", &lv_font_montserrat_12, C_MUTED, 14, 26);
  for (int h = 0; h < 24; h++) {
    lv_obj_t *bar = lv_obj_create(t);
    lv_obj_set_size(bar, 13, 4);
    lv_obj_set_pos(bar, 18 + h * 18, 176);
    lv_obj_set_style_radius(bar, 3, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(C_ACCENT), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    g_ui.heat[h] = bar;
  }
  int ticks[5] = {0, 6, 12, 18, 23};
  for (int i = 0; i < 5; i++) {
    int h = ticks[i]; char s[4]; snprintf(s, sizeof(s), "%dh", h);
    lv_obj_t *l = mklabel(t, s, &lv_font_montserrat_12, C_MUTED);
    lv_obj_set_pos(l, 14 + h * 18, 186);
  }
}
// Detalhes minimalista: só "quem te limita primeiro" + estado de overage.
static void build_tile_details(lv_obj_t *t) {
  lv_obj_t *cap = mklabel(t, "Limite que vai te bloquear primeiro", &lv_font_montserrat_14, C_MUTED);
  lv_obj_align(cap, LV_ALIGN_TOP_MID, 0, 46);
  g_ui.dvClaim = mklabel(t, "", &lv_font_montserrat_40, C_ACCENT);
  lv_obj_align(g_ui.dvClaim, LV_ALIGN_TOP_MID, 0, 84);
  g_ui.dvOverage = mklabel(t, "", &lv_font_montserrat_14, C_MUTED);
  lv_obj_align(g_ui.dvOverage, LV_ALIGN_TOP_MID, 0, 156);
}

static void on_tile_changed(lv_event_t *e) {
  (void)e;
  if (!g_ui.tv) return;
  lv_obj_t *act = lv_tileview_get_tile_active(g_ui.tv);
  for (int i = 0; i < NTILES; i++) {
    if (!g_ui.dots[i]) continue;
    bool on = (g_ui.tile[i] == act);
    lv_obj_set_style_bg_color(g_ui.dots[i], lv_color_hex(on ? C_ACCENT : C_BORDER), 0);
    lv_obj_set_width(g_ui.dots[i], on ? 18 : 8);
  }
}

// Atualiza contadores/relógios (1s) — separado dos valores de fetch.
static void dash_tick() {
  if (g_state != ST_MAIN || !g_ui.ovEta5) return;
  char e[32], b[64], c[24];
  fmt_eta(g_usage.h5ResetEpoch, e, sizeof(e)); snprintf(b, sizeof(b), "reseta em %s", e);
  lv_label_set_text(g_ui.ovEta5, b);
  fmt_eta(g_usage.d7ResetEpoch, e, sizeof(e)); snprintf(b, sizeof(b), "reseta em %s", e);
  lv_label_set_text(g_ui.ovEta7, b);
  if (g_ui.rcBig5) { fmt_eta(g_usage.h5ResetEpoch, e, sizeof(e)); lv_label_set_text(g_ui.rcBig5, e); }
  if (g_ui.rcBig7) { fmt_eta(g_usage.d7ResetEpoch, e, sizeof(e)); lv_label_set_text(g_ui.rcBig7, e); }
  if (g_ui.rcAt5)  { fmt_clock(g_usage.h5ResetEpoch, c, sizeof(c)); snprintf(b, sizeof(b), "reseta %s", c); lv_label_set_text(g_ui.rcAt5, b); }
  if (g_ui.rcAt7)  { fmt_clock(g_usage.d7ResetEpoch, c, sizeof(c)); snprintf(b, sizeof(b), "reseta %s", c); lv_label_set_text(g_ui.rcAt7, b); }
  set_hdr_status();
}

// Preenche todos os valores vindos do fetch (sem rebuild de tela).
static void refresh_ui_values() {
  if (g_state != ST_MAIN || !g_ui.ovChip) return;
  char b[96];

  // Overview
  set_chip(g_ui.ovChip, g_usage.statusOverall, status_color(g_usage.statusOverall));
  snprintf(b, sizeof(b), "%d%%", (int)(g_usage.h5 + 0.5f)); lv_label_set_text(g_ui.ovPct5, b);
  lv_obj_set_style_text_color(g_ui.ovPct5, lv_color_hex(pct_color(g_usage.h5)), 0);
  lv_bar_set_value(g_ui.ovBar5, (int)(g_usage.h5 + 0.5f), LV_ANIM_OFF);
  lv_obj_set_style_bg_color(g_ui.ovBar5, lv_color_hex(pct_color(g_usage.h5)), LV_PART_INDICATOR);
  snprintf(b, sizeof(b), "%d%%", (int)(g_usage.d7 + 0.5f)); lv_label_set_text(g_ui.ovPct7, b);
  lv_obj_set_style_text_color(g_ui.ovPct7, lv_color_hex(pct_color(g_usage.d7)), 0);
  lv_bar_set_value(g_ui.ovBar7, (int)(g_usage.d7 + 0.5f), LV_ANIM_OFF);
  lv_obj_set_style_bg_color(g_ui.ovBar7, lv_color_hex(pct_color(g_usage.d7)), LV_PART_INDICATOR);

  // Reset (chips de status; countdowns no dash_tick)
  if (g_ui.rcChip5) set_chip(g_ui.rcChip5, g_usage.status5h, status_color(g_usage.status5h));
  if (g_ui.rcChip7) set_chip(g_ui.rcChip7, g_usage.status7d, status_color(g_usage.status7d));

  // Modelos / incidente
  if (g_ui.incident) {
    bool any = !(g_status.haikuUp && g_status.sonnetUp && g_status.opusUp && g_status.fableUp);
    lv_label_set_text(g_ui.incident, !g_status.ok ? "Status: sem dados"
                      : (any ? "Incidente ativo - veja status.claude.com" : "Todos os modelos OK - sem incidentes"));
    lv_obj_set_style_text_color(g_ui.incident, lv_color_hex(any ? C_WARN : C_MUTED), 0);
  }

  // Tendência (chart + burn-rate)
  if (g_ui.chart) {
    int n = 40;
    for (int i = 0; i < n; i++) {
      int hi = g_histN - n + i;
      if (hi < 0) {
        lv_chart_set_value_by_id(g_ui.chart, g_ui.ser5, i, LV_CHART_POINT_NONE);
        lv_chart_set_value_by_id(g_ui.chart, g_ui.ser7, i, LV_CHART_POINT_NONE);
      } else {
        lv_chart_set_value_by_id(g_ui.chart, g_ui.ser5, i, g_hist[hist_idx(hi)].h5);
        lv_chart_set_value_by_id(g_ui.chart, g_ui.ser7, i, g_hist[hist_idx(hi)].d7);
      }
    }
    lv_chart_refresh(g_ui.chart);
  }
  if (g_ui.burn) { char s[96]; burn_text(s, sizeof(s)); lv_label_set_text(g_ui.burn, s); }

  // Detalhes minimalista: quem te limita primeiro + overage
  if (g_ui.dvClaim) {
    bool c5 = (strcmp(g_usage.repClaim, "five_hour") == 0);
    lv_label_set_text(g_ui.dvClaim, g_usage.repClaim[0] ? (c5 ? "5 HORAS" : "7 DIAS") : "--");
  }
  if (g_ui.dvOverage) {
    bool rej = (strcmp(g_usage.overageStatus, "rejected") == 0);
    if (rej) snprintf(b, sizeof(b), "Overage indisponivel%s%s", g_usage.overageReason[0] ? ": " : "", g_usage.overageReason);
    else     snprintf(b, sizeof(b), "Overage: %s", g_usage.overageStatus[0] ? g_usage.overageStatus : "--");
    lv_label_set_text(g_ui.dvOverage, b);
  }

  // Heatmap por hora
  if (g_ui.heat[0]) {
    float mx = 1.0f;
    for (int h = 0; h < 24; h++) if (g_hourBurn[h] > mx) mx = g_hourBurn[h];
    int curHour = -1; time_t now = time(nullptr);
    if (now > 1000000000L) { struct tm tv; localtime_r(&now, &tv); curHour = tv.tm_hour; }
    for (int h = 0; h < 24; h++) {
      if (!g_ui.heat[h]) continue;
      float r = g_hourBurn[h] / mx; if (r < 0) r = 0; if (r > 1) r = 1;
      int hgt = 4 + (int)(r * 118);
      lv_obj_set_size(g_ui.heat[h], 13, hgt);
      lv_obj_set_y(g_ui.heat[h], 174 - hgt);
      lv_obj_set_style_bg_color(g_ui.heat[h], lv_color_hex(h == curHour ? C_TEXT : C_ACCENT), 0);
      lv_obj_set_style_bg_opa(g_ui.heat[h], (lv_opa_t)(70 + (int)(r * 185)), 0);
    }
  }

  dash_tick();
}

// Atualiza o texto de status do cabeçalho (sem trocar de tela)
static void set_hdr_status() {
  if (!g_hdrStatus) return;
  char buf[40]; uint32_t color;
  if (g_refreshing)        { strcpy(buf, "atualizando...");     color = C_ACCENT; }
  else if (!g_lastFetchOk) { strcpy(buf, "falha ao atualizar"); color = C_BAD; }
  else {
    uint32_t s = (millis() - g_lastOkMs) / 1000;
    if (s < 60) snprintf(buf, sizeof(buf), "atualizado ha %us", (unsigned)s);
    else        snprintf(buf, sizeof(buf), "atualizado ha %umin", (unsigned)(s / 60));
    color = C_MUTED;
  }
  lv_label_set_text(g_hdrStatus, buf);
  lv_obj_set_style_text_color(g_hdrStatus, lv_color_hex(color), 0);
}
// Botão de refresh: só pede; a busca acontece em background no loop()
static void refresh_cb(lv_event_t *e) { (void)e; g_wantRefresh = true; }

static void ui_main() {
  lv_obj_t *scr = lv_screen_active();
  lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);

  // Header moderno
  lv_obj_t *title = mklabel(scr, "Claude", &lv_font_montserrat_22, C_TEXT);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 7);
  g_hdrStatus = mklabel(scr, "", &lv_font_montserrat_12, C_MUTED);
  lv_obj_align(g_hdrStatus, LV_ALIGN_TOP_RIGHT, -52, 12);

  lv_obj_t *gear = mkbtn(scr, LV_SYMBOL_SETTINGS, &lv_font_montserrat_18, C_SURFACE2, C_TEXT);
  lv_obj_set_size(gear, 40, 30);
  lv_obj_align(gear, LV_ALIGN_TOP_RIGHT, -8, 5);
  lv_obj_add_event_cb(gear, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)ST_SETTINGS);

  // Barra fina decrescente do próximo refresh (toque = atualizar agora)
  g_ui.refBar = lv_bar_create(scr);
  lv_obj_set_size(g_ui.refBar, 480, 3);
  lv_obj_set_pos(g_ui.refBar, 0, 38);
  lv_bar_set_range(g_ui.refBar, 0, 1000);
  lv_bar_set_value(g_ui.refBar, 1000, LV_ANIM_OFF);
  lv_obj_set_style_bg_color(g_ui.refBar, lv_color_hex(C_SURFACE), LV_PART_MAIN);
  lv_obj_set_style_bg_color(g_ui.refBar, lv_color_hex(C_ACCENT), LV_PART_INDICATOR);
  lv_obj_set_style_radius(g_ui.refBar, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(g_ui.refBar, 0, LV_PART_INDICATOR);
  lv_obj_add_flag(g_ui.refBar, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_ui.refBar, refresh_cb, LV_EVENT_CLICKED, NULL);

  // Telas (swipe horizontal)
  g_ui.tv = lv_tileview_create(scr);
  lv_obj_set_pos(g_ui.tv, 0, 44);
  lv_obj_set_size(g_ui.tv, 480, 250);
  lv_obj_set_style_bg_opa(g_ui.tv, 0, 0);
  lv_obj_set_style_border_width(g_ui.tv, 0, 0);
  lv_obj_set_scrollbar_mode(g_ui.tv, LV_SCROLLBAR_MODE_OFF);
  for (int i = 0; i < NTILES; i++) {
    g_ui.tile[i] = lv_tileview_add_tile(g_ui.tv, i, 0, LV_DIR_HOR);
    tile_setup(g_ui.tile[i]);
  }
  build_tile_overview(g_ui.tile[0]);
  build_tile_reset(g_ui.tile[1]);
  build_tile_models(g_ui.tile[2]);
  build_tile_trend(g_ui.tile[3]);
  build_tile_heat(g_ui.tile[4]);
  build_tile_details(g_ui.tile[5]);
  lv_obj_add_event_cb(g_ui.tv, on_tile_changed, LV_EVENT_VALUE_CHANGED, NULL);

  // Dots (objetos; o ativo vira pílula)
  for (int i = 0; i < NTILES; i++) {
    g_ui.dots[i] = lv_obj_create(scr);
    lv_obj_set_size(g_ui.dots[i], 8, 8);
    lv_obj_set_style_radius(g_ui.dots[i], 4, 0);
    lv_obj_set_style_bg_color(g_ui.dots[i], lv_color_hex(C_BORDER), 0);
    lv_obj_set_style_border_width(g_ui.dots[i], 0, 0);
    lv_obj_clear_flag(g_ui.dots[i], LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(g_ui.dots[i], LV_ALIGN_BOTTOM_MID, (int)((i - (NTILES - 1) / 2.0f) * 16), -3);
  }

  refresh_ui_values();
  on_tile_changed(NULL);
}

// ============================================================
// Tela: settings (navegação touch substitui o botão físico)
// ============================================================
static bool g_wipeArmed = false;
static lv_obj_t *g_briLbl = nullptr, *g_wipeLbl = nullptr, *g_pollLbl = nullptr, *g_tzLbl = nullptr;
static const int POLL_OPTS[4] = {30, 60, 120, 300};
static const int TZ_OPTS[] = {-3, -4, -5, -6, -7, -8, -2, -1, 0, 1, 2, 3};
#define NTZ ((int)(sizeof(TZ_OPTS) / sizeof(TZ_OPTS[0])))

static void settings_action_cb(lv_event_t *e) {
  int act = (int)(intptr_t)lv_event_get_user_data(e);
  switch (act) {
    case 0: request_state(ST_LOADING); break;          // atualizar
    case 1: g_onboarding = false; request_state(ST_WIFI); break;
    case 2: request_state(ST_TOKEN); break;            // trocar token
    case 3:                                            // brilho
      g_briIdx = (g_briIdx + 1) % 3; g_prefs.putInt("bri", g_briIdx); apply_brightness();
      if (g_briLbl) {
        const char *n[3] = {"baixo", "medio", "alto"};
        char m[24]; snprintf(m, sizeof(m), "Brilho: %s", n[g_briIdx]);
        lv_label_set_text(g_briLbl, m);
      }
      break;
    case 4:                                            // apagar tudo (2 toques)
      if (!g_wipeArmed) {
        g_wipeArmed = true;
        if (g_wipeLbl) lv_label_set_text(g_wipeLbl, "Toque de novo p/ confirmar");
      } else {
        g_wipeArmed = false;
        factory_reset();
        request_state(ST_WIFI);
      }
      break;
    case 5: request_state(ST_MAIN); break;             // voltar
    case 6: {                                          // intervalo de atualização
      int idx = 0;
      for (int i = 0; i < 4; i++) if (POLL_OPTS[i] == g_pollSec) idx = i;
      g_pollSec = POLL_OPTS[(idx + 1) % 4];
      g_prefs.putInt("poll", g_pollSec);
      if (g_pollLbl) {
        char m[40];
        if (g_pollSec < 60) snprintf(m, sizeof(m), LV_SYMBOL_LOOP "  Atualizar: %ds", g_pollSec);
        else snprintf(m, sizeof(m), LV_SYMBOL_LOOP "  Atualizar: %dmin", g_pollSec / 60);
        lv_label_set_text(g_pollLbl, m);
      }
      break;
    }
    case 7: {                                          // fuso horário (GMT)
      int idx = 0;
      for (int i = 0; i < NTZ; i++) if (TZ_OPTS[i] == g_tzOffset) idx = i;
      g_tzOffset = TZ_OPTS[(idx + 1) % NTZ];
      g_prefs.putInt("tz", g_tzOffset);
      apply_tz();
      if (g_tzLbl) { char m[32]; snprintf(m, sizeof(m), LV_SYMBOL_GPS "  Fuso: GMT%+d", g_tzOffset); lv_label_set_text(g_tzLbl, m); }
      break;
    }
  }
}
static void add_setting_row(lv_obj_t *p, int y, const char *txt, int act, uint32_t fg, lv_obj_t **out) {
  lv_obj_t *b = lv_button_create(p);
  lv_obj_set_size(b, 452, 36);
  lv_obj_align(b, LV_ALIGN_TOP_MID, 0, y);
  lv_obj_set_style_bg_color(b, lv_color_hex(C_SURFACE), 0);
  lv_obj_set_style_radius(b, 10, 0);
  lv_obj_t *l = mklabel(b, txt, &lv_font_montserrat_14, fg);
  lv_obj_align(l, LV_ALIGN_LEFT_MID, 6, 0);
  lv_obj_add_event_cb(b, settings_action_cb, LV_EVENT_CLICKED, (void *)(intptr_t)act);
  if (out) *out = l;
}
static void ui_settings() {
  lv_obj_t *scr = lv_screen_active();
  g_wipeArmed = false;
  lv_obj_t *title = mklabel(scr, "Ajustes", &lv_font_montserrat_20, C_TEXT);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 14, 10);

  lv_obj_t *bk = mkbtn(scr, LV_SYMBOL_LEFT " Voltar", &lv_font_montserrat_14, C_SURFACE2, C_MUTED);
  lv_obj_align(bk, LV_ALIGN_TOP_RIGHT, -12, 8);
  lv_obj_add_event_cb(bk, nav_cb, LV_EVENT_CLICKED, (void *)(intptr_t)(g_usage.ok ? ST_MAIN : ST_SETTINGS));

  const char *n[3] = {"baixo", "medio", "alto"};
  char bri[28]; snprintf(bri, sizeof(bri), LV_SYMBOL_EYE_OPEN "  Brilho: %s", n[g_briIdx]);
  char pollTxt[40];
  if (g_pollSec < 60) snprintf(pollTxt, sizeof(pollTxt), LV_SYMBOL_LOOP "  Atualizar: %ds", g_pollSec);
  else                snprintf(pollTxt, sizeof(pollTxt), LV_SYMBOL_LOOP "  Atualizar: %dmin", g_pollSec / 60);
  char tzTxt[32]; snprintf(tzTxt, sizeof(tzTxt), LV_SYMBOL_GPS "  Fuso: GMT%+d", g_tzOffset);

  add_setting_row(scr, 38,  LV_SYMBOL_REFRESH "  Atualizar agora", 0, C_TEXT, nullptr);
  add_setting_row(scr, 78,  pollTxt,                               6, C_TEXT, &g_pollLbl);
  add_setting_row(scr, 118, tzTxt,                                 7, C_TEXT, &g_tzLbl);
  add_setting_row(scr, 158, LV_SYMBOL_WIFI    "  Configurar WiFi",  1, C_TEXT, nullptr);
  add_setting_row(scr, 198, LV_SYMBOL_KEYBOARD"  Trocar token",     2, C_TEXT, nullptr);
  add_setting_row(scr, 238, bri,                                    3, C_TEXT, &g_briLbl);
  add_setting_row(scr, 278, LV_SYMBOL_TRASH   "  Apagar tudo",      4, C_BAD,  &g_wipeLbl);
}

// ============================================================
// Navegação genérica
// ============================================================
static void nav_cb(lv_event_t *e) {
  State s = (State)(intptr_t)lv_event_get_user_data(e);
  request_state(s);
}

// ============================================================
// Render do estado atual
// ============================================================
static void render_state() {
  g_state = g_pending;
  if (g_state != ST_TOKEN) stop_web();        // derruba o WebServer ao sair da tela de token
  // invalida ponteiros vivos antes de destruir a tela antiga
  memset(&g_ui, 0, sizeof(g_ui));
  g_mascN = 0;
  g_pinDots = g_pinMsg = nullptr;
  g_tokMsg = nullptr;
  g_hdrStatus = nullptr;

  lv_obj_clean(lv_screen_active());
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(C_BG), 0);
  lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, 0);

  switch (g_state) {
    case ST_PIN:
    case ST_SETUP_PIN: ui_pin(); break;
    case ST_WIFI:      ui_wifi(); break;
    case ST_TOKEN:     ui_token(); break;
    case ST_LOADING:   ui_loading(g_wifi.isConnected() ? g_wifi.getSSID().c_str() : "conectando WiFi"); break;
    case ST_MAIN:      ui_main(); break;
    case ST_SETTINGS:  ui_settings(); break;
    case ST_ERROR:     ui_message("Falha", g_usage.error[0] ? g_usage.error : "sem dados", C_BAD); break;
    default: break;
  }
}

// ============================================================
// Tempo (NTP) e ciclo de dados
// ============================================================
static void apply_tz() { configTime(g_tzOffset * 3600, 0, NTP_SERVER_1, NTP_SERVER_2); }
static void ensure_time() {
  if (g_timeInit || !g_wifi.isConnected()) return;
  apply_tz();
  g_timeInit = true;
  Serial.println("[NTP] sync iniciado");
}

// Primeiro load (mostra a tela de carregamento). Vai p/ ST_MAIN ou ST_ERROR.
static void do_refresh() {
  ensure_time();
  bool ok = fetchUsage(g_token, g_usage);
  if (ok) {
    fetchModelStatus(g_status); g_lastOkMs = millis(); g_lastFetchOk = true;
    hist_push(g_usage.h5, g_usage.d7); accumulate_heat(g_usage.h5); save_history();
  } else g_lastFetchOk = false;
  g_lastPollMs = millis();
  request_state(ok ? ST_MAIN : ST_ERROR);
}

// Atualização EM BACKGROUND: não troca de tela; mantém o dashboard e os dados
// antigos se falhar. A chamada à API é bloqueante (~1-2s), então mostra
// "atualizando..." no cabeçalho durante a busca.
static void bg_refresh() {
  if (!g_wifi.isConnected()) g_wifi.autoConnect(WIFI_CONNECT_TIMEOUT_MS);
  ensure_time();
  g_refreshing = true; set_hdr_status(); lv_refr_now(NULL);
  UsageData u = {};
  bool ok = fetchUsage(g_token, u);
  bool stChanged = false;
  if (ok) {
    g_usage = u; g_lastOkMs = millis(); g_lastFetchOk = true;
    hist_push(u.h5, u.d7); accumulate_heat(u.h5); save_history();
    ModelStatus old = g_status;
    fetchModelStatus(g_status);
    stChanged = old.haikuUp != g_status.haikuUp || old.sonnetUp != g_status.sonnetUp ||
                old.opusUp  != g_status.opusUp  || old.fableUp  != g_status.fableUp;
  } else g_lastFetchOk = false;
  g_refreshing = false;
  g_lastPollMs = millis();
  if (stChanged) request_state(ST_MAIN);  // mascotes mudaram -> rebuild
  else refresh_ui_values();               // resto: in-place (preserva o tile atual)
}

// ============================================================
// setup / loop
// ============================================================
void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== Claude Usage Stick (touch) ===");

  // Display
  Arduino_DataBus *bus = new Arduino_ESP32QSPI(TFT_CS, TFT_SCK, TFT_SDA0, TFT_SDA1, TFT_SDA2, TFT_SDA3);
  Arduino_GFX *g = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, 320, 480);
  gfx = new Arduino_Canvas(320, 480, g, 0, 0, 0);
  if (!gfx->begin(QSPI_FREQ)) { Serial.println("FATAL display"); while (1) delay(1000); }
  gfx->fillScreen(0x0000); gfx->flush();
  canvas_fb = gfx->getFramebuffer();

  // Backlight via PWM (brilho ajustável)
  ledcAttach(TFT_BL, 5000, 8);
  touch_dev.begin();

  // LVGL
  lv_init();
  lv_tick_set_cb([]() -> uint32_t { return millis(); });
  uint32_t bufSize = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color_t);
  lv_color_t *buf = (lv_color_t *)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) { Serial.println("FATAL PSRAM"); while (1) delay(1000); }
  lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_flush_cb(disp, disp_flush_cb);
  lv_display_set_buffers(disp, buf, NULL, bufSize, LV_DISPLAY_RENDER_MODE_FULL);
  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, touch_read_cb);

  load_persisted();
  apply_brightness();

  if (!LittleFS.begin(true)) Serial.println("LittleFS: falhou");
  else load_history();

  g_wifi.begin();

  if (g_hasToken) {
    // Tenta WiFi cedo (em paralelo o usuário digita o PIN)
    g_wifi.autoConnect(WIFI_CONNECT_TIMEOUT_MS);
    request_state(ST_PIN);
  } else {
    g_onboarding = true;
    // Se já há WiFi salvo (reboot no meio do onboarding), pula direto p/ o token
    request_state(g_wifi.autoConnect(WIFI_CONNECT_TIMEOUT_MS) ? ST_TOKEN : ST_WIFI);
  }
}

void loop() {
  lv_task_handler();

  // Servidor web da tela de token
  if (g_state == ST_TOKEN && g_web) {
    g_web->handleClient();
    if (g_tokenGot) { g_tokenGot = false; request_state(ST_SETUP_PIN); }
  }

  if (g_dirty) {
    g_dirty = false;
    render_state();
    if (g_state == ST_LOADING) {
      lv_task_handler();
      lv_refr_now(NULL);
      do_refresh();
    }
  }

  // Poll automático EM BACKGROUND (sem trocar de tela) + refresh manual
  if (g_state == ST_MAIN &&
      (g_wantRefresh || millis() - g_lastPollMs > (uint32_t)g_pollSec * 1000)) {
    g_wantRefresh = false;
    bg_refresh();           // seta g_lastPollMs no fim
  }

  // Atualização viva: contadores (1s), barra de refresh (250ms), mascotes (bob+pisca)
  if (g_state == ST_MAIN) {
    uint32_t now = millis();
    static uint32_t lastTick = 0, lastBar = 0, lastBob = 0, blinkAt = 0;
    static bool blinkClosed = false;
    if (now - lastTick > 1000) { lastTick = now; dash_tick(); }
    if (now - lastBar > 250 && g_ui.refBar) {
      lastBar = now;
      int v;
      if (g_refreshing) v = 1000;
      else {
        uint32_t el = now - g_lastPollMs, per = (uint32_t)g_pollSec * 1000;
        v = el >= per ? 0 : (int)(1000 - (uint64_t)el * 1000 / per);
      }
      lv_bar_set_value(g_ui.refBar, v, LV_ANIM_OFF);
    }
    if (now - lastBob > 80) {                       // bob suave dos mascotes online
      lastBob = now;
      float ph = now / 600.0f;
      for (int i = 0; i < g_mascN; i++) {
        if (!g_masc[i].cont || !g_masc[i].up) continue;
        int off = (int)(2.0f * sinf(ph + i * 0.9f) - 1.0f);   // -3..+1
        lv_obj_set_y(g_masc[i].cont, g_masc[i].baseY + off);
      }
    }
    uint32_t bp = blinkClosed ? 150 : 3000;
    if (now - blinkAt > bp) {                        // piscar
      blinkAt = now; blinkClosed = !blinkClosed;
      for (int i = 0; i < g_mascN; i++) {
        if (!g_masc[i].up) continue;
        for (int k = 0; k < 2; k++)
          if (g_masc[i].eye[k]) lv_obj_set_height(g_masc[i].eye[k], blinkClosed ? 2 : g_masc[i].eyeH);
      }
    }
  }

  delay(5);
}
