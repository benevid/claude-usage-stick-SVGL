/**
 * Gadget Valorant — Fase 1: BRING-UP (cor + touch)
 * Placa: Guition JC4832W535 (ESP32-S3, AXS15231B QSPI), 480x320 paisagem.
 *
 * Objetivo: validar no HARDWARE REAL que as cores saem corretas e que o toque
 * está na orientação certa, ANTES de investir na UI.
 *
 * Pipeline de tela copiado da referência que funciona (esp32_controller):
 *   Arduino_Canvas rotation=0  +  rotação manual 270° CW no flush  +  gfx->flush()
 * Ver firmware/REFERENCIA-HARDWARE-LVGL.md
 */
#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <lvgl.h>
#include <Wire.h>
#include "config.h"
#include "touch.h"

Arduino_Canvas *gfx = nullptr;
static uint16_t *canvas_fb = nullptr;
AXS15231B_Touch touch_dev(TOUCH_SCL, TOUCH_SDA, TOUCH_INT, TOUCH_ADDR, 3); // 3 = USB à esquerda

static lv_obj_t *g_dot = nullptr;
static lv_obj_t *g_touch_lbl = nullptr;

// ---- Flush: LVGL 480x320 -> rotaciona 270° CW -> Canvas 320x480 -> QSPI ----
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
    uint16_t *src = (uint16_t *)px_map;
    for (int ly = 0; ly < SCREEN_HEIGHT; ly++) {
        uint16_t *src_row = src + ly * SCREEN_WIDTH;
        for (int lx = 0; lx < SCREEN_WIDTH; lx++) {
            canvas_fb[(479 - lx) * 320 + ly] = src_row[lx];
        }
    }
    gfx->flush();
    lv_disp_flush_ready(disp);
}

static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    uint16_t x, y;
    if (touch_dev.touched()) {
        touch_dev.readData(&x, &y);
        data->point.x = x;
        data->point.y = y;
        data->state = LV_INDEV_STATE_PRESSED;
        if (g_dot) lv_obj_set_pos(g_dot, (int)x - 11, (int)y - 11);
        if (g_touch_lbl) lv_label_set_text_fmt(g_touch_lbl, "TOQUE: x=%d  y=%d", x, y);
        Serial.printf("touch: x=%d y=%d\n", x, y);
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void make_swatch(lv_obj_t *parent, uint32_t hex, const char *name, uint32_t textHex) {
    lv_obj_t *sw = lv_obj_create(parent);
    lv_obj_set_size(sw, 142, 62);
    lv_obj_set_style_bg_color(sw, lv_color_hex(hex), 0);
    lv_obj_set_style_bg_opa(sw, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(sw, 0, 0);
    lv_obj_set_style_radius(sw, 8, 0);
    lv_obj_set_style_pad_all(sw, 0, 0);
    lv_obj_t *l = lv_label_create(sw);
    lv_label_set_text(l, name);
    lv_obj_set_style_text_color(l, lv_color_hex(textHex), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_center(l);
}

void setup() {
    Serial.begin(115200);
    delay(1200);
    Serial.println("\n=== Valorant Gadget — BRING-UP (cor + touch) ===");

    // --- Display (Canvas rotation=0 => cores corretas) ---
    Arduino_DataBus *bus = new Arduino_ESP32QSPI(
        TFT_CS, TFT_SCK, TFT_SDA0, TFT_SDA1, TFT_SDA2, TFT_SDA3);
    Arduino_GFX *g = new Arduino_AXS15231B(bus, GFX_NOT_DEFINED, 0, false, 320, 480);
    gfx = new Arduino_Canvas(320, 480, g, 0, 0, 0);
    if (!gfx->begin(QSPI_FREQ)) {
        Serial.println("FATAL: display init falhou!");
        while (1) delay(1000);
    }
    gfx->fillScreen(0x0000);
    gfx->flush();
    canvas_fb = gfx->getFramebuffer();
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
    Serial.println("Display: OK");

    // --- Touch ---
    bool t_ok = touch_dev.begin();
    Serial.printf("Touch: %s\n", t_ok ? "OK" : "FALHOU (verificar I2C)");

    // --- LVGL ---
    lv_init();
    lv_tick_set_cb([]() -> uint32_t { return millis(); });

    uint32_t bufSize = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(lv_color_t);
    lv_color_t *buf = (lv_color_t *)heap_caps_malloc(bufSize, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        Serial.println("FATAL: alloc PSRAM falhou (habilitar PSRAM=opi!)");
        while (1) delay(1000);
    }
    lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_flush_cb(disp, disp_flush_cb);
    lv_display_set_buffers(disp, buf, NULL, bufSize, LV_DISPLAY_RENDER_MODE_FULL);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    Serial.println("LVGL: OK");

    // --- UI de teste ---
    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0b0b18), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *title = lv_label_create(scr);
    lv_label_set_text(title, "VALO GADGET  -  BRING-UP");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_18, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *sub = lv_label_create(scr);
    lv_label_set_text(sub, "Cores corretas? Toque a tela (canto sup. esq. = x,y baixos).");
    lv_obj_set_style_text_color(sub, lv_color_hex(0x94a3b8), 0);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_12, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 36);

    // Grade de cores (3 por linha, com wrap)
    lv_obj_t *grid = lv_obj_create(scr);
    lv_obj_set_size(grid, 462, 150);
    lv_obj_align(grid, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 4, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);

    make_swatch(grid, 0xFF0000, "VERMELHO", 0xFFFFFF);
    make_swatch(grid, 0x00FF00, "VERDE", 0x000000);
    make_swatch(grid, 0x0000FF, "AZUL", 0xFFFFFF);
    make_swatch(grid, 0x7C3AED, "VIOLETA", 0xFFFFFF); // brand
    make_swatch(grid, 0xF43F5E, "ROSA/CTA", 0xFFFFFF); // brand
    make_swatch(grid, 0xFFFFFF, "BRANCO", 0x000000);

    // Label de toque
    g_touch_lbl = lv_label_create(scr);
    lv_label_set_text(g_touch_lbl, "TOQUE: aguardando...");
    lv_obj_set_style_text_color(g_touch_lbl, lv_color_hex(0xa78bfa), 0);
    lv_obj_align(g_touch_lbl, LV_ALIGN_BOTTOM_MID, 0, -12);

    // Ponto que segue o dedo
    g_dot = lv_obj_create(scr);
    lv_obj_set_size(g_dot, 22, 22);
    lv_obj_set_style_radius(g_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(g_dot, lv_color_hex(0xF43F5E), 0);
    lv_obj_set_style_border_width(g_dot, 2, 0);
    lv_obj_set_style_border_color(g_dot, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_pos(g_dot, 229, 149);

    Serial.println("=== Bring-up pronto. Olhe a tela. ===");
}

void loop() {
    lv_task_handler();
    delay(5);
}
