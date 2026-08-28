/**
 * Bring-up — ESP32-2432S028 ("Cheap Yellow Display" / CYD)
 *
 * Objetivo: validar no HARDWARE REAL que as cores saem corretas, a
 * orientação está certa e o touch (XPT2046, barramento SPI separado do
 * display) está lendo e mapeando pra tela — ANTES de investir na UI.
 * Mesmo espírito do firmware/bringup/bringup.ino (placa S3), adaptado
 * para esta placa: ILI9341 + TFT_eSPI (sem LVGL ainda), touch resistivo.
 *
 * Pinos e flags de driver da TFT_eSPI vêm do firmware/bringup_cyd/build.sh
 * (via -DUSER_SETUP_LOADED=1 + defines), NÃO do User_Setup.h da lib
 * instalada globalmente — outros projetos desta máquina também usam
 * TFT_eSPI com pinagens diferentes, não podemos mexer no arquivo da lib.
 */
#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>
#include "config.h"

TFT_eSPI tft = TFT_eSPI();

SPIClass touchSPI(HSPI);
XPT2046_Touchscreen ts(TOUCH_CS_PIN, TOUCH_IRQ_PIN);

// Calibração medida nesta unidade (4 cantos + 2 pontos-âncora via Serial).
#define TS_MINX 300
#define TS_MAXX 3500
#define TS_MINY 430
#define TS_MAXY 3550

static uint16_t hex565(uint32_t hex) {
  return tft.color565((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

static void make_swatch(int x, int y, int w, int h, uint32_t bg, const char *name, uint32_t fg) {
  tft.fillRoundRect(x, y, w, h, 6, hex565(bg));
  tft.setTextColor(hex565(fg), hex565(bg));
  tft.setTextDatum(MC_DATUM);
  tft.drawString(name, x + w / 2, y + h / 2, 2);
}

// Mapeia o ponto cru do XPT2046 direto pro espaço de tela já rotacionado
// (TFT_ROTATION=1). Medido nesta unidade: raw_x cresce esquerda->direita e
// raw_y cresce cima->baixo, já alinhado com a orientação atual da tela —
// sem precisar de troca/inversão de eixo (diferente do driver AXS15231B da
// outra placa, que exigia rotação manual; esse XPT2046 já vem "reto" aqui).
static void map_touch(uint16_t raw_x, uint16_t raw_y, uint16_t *sx, uint16_t *sy) {
  *sx = constrain(map(raw_x, TS_MINX, TS_MAXX, 0, SCREEN_WIDTH - 1), 0, SCREEN_WIDTH - 1);
  *sy = constrain(map(raw_y, TS_MINY, TS_MAXY, 0, SCREEN_HEIGHT - 1), 0, SCREEN_HEIGHT - 1);
}

void setup() {
  Serial.begin(115200);
  delay(1200);
  Serial.println("\n=== ESP32-2432S028 (CYD) — BRING-UP (cor + touch) ===");

  tft.init();
  tft.setRotation(TFT_ROTATION);
  tft.fillScreen(TFT_BLACK);
  Serial.println("Display: OK");

  touchSPI.begin(TOUCH_CLK_PIN, TOUCH_MISO_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN);
  ts.begin(touchSPI);
  Serial.println("Touch: iniciado (toque a tela p/ testar)");

  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("CYD BRING-UP", SCREEN_WIDTH / 2, 6, 4);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.drawString("cores certas? toque nos cantos e olhe o Serial", SCREEN_WIDTH / 2, 34, 2);

  // Grade de cores 3x2 (mesma paleta do bring-up da placa S3)
  int w = 96, h = 60, gap = 8;
  int x0 = (SCREEN_WIDTH - (w * 3 + gap * 2)) / 2;
  int y0 = 56;
  make_swatch(x0 + 0 * (w + gap), y0, w, h, 0xFF0000, "VERMELHO", 0xFFFFFF);
  make_swatch(x0 + 1 * (w + gap), y0, w, h, 0x00FF00, "VERDE",    0x000000);
  make_swatch(x0 + 2 * (w + gap), y0, w, h, 0x0000FF, "AZUL",     0xFFFFFF);
  make_swatch(x0 + 0 * (w + gap), y0 + h + gap, w, h, 0x7C3AED, "VIOLETA", 0xFFFFFF);
  make_swatch(x0 + 1 * (w + gap), y0 + h + gap, w, h, 0xF43F5E, "ROSA",    0xFFFFFF);
  make_swatch(x0 + 2 * (w + gap), y0 + h + gap, w, h, 0xFFFFFF, "BRANCO",  0x000000);

  tft.setTextColor(TFT_SKYBLUE, TFT_BLACK);
  tft.setTextDatum(BC_DATUM);
  tft.drawString("TOQUE: aguardando...", SCREEN_WIDTH / 2, SCREEN_HEIGHT - 6, 2);

  Serial.println("=== Bring-up pronto. Olhe a tela. ===");
}

static uint16_t lastSx = 0xFFFF, lastSy = 0xFFFF;

void loop() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    uint16_t sx, sy;
    map_touch(p.x, p.y, &sx, &sy);

    Serial.printf("touch cru: x=%d y=%d z=%d  ->  mapeado: x=%d y=%d\n", p.x, p.y, p.z, sx, sy);

    // apaga o ponto anterior (redesenha fundo preto) e desenha o novo
    if (lastSx != 0xFFFF) tft.fillCircle(lastSx, lastSy, 12, TFT_BLACK);
    tft.fillCircle(sx, sy, 10, TFT_RED);
    tft.drawCircle(sx, sy, 10, TFT_WHITE);
    lastSx = sx; lastSy = sy;

    tft.setTextColor(TFT_SKYBLUE, TFT_BLACK);
    tft.setTextDatum(BC_DATUM);
    char buf[48];
    snprintf(buf, sizeof(buf), "TOQUE: x=%d y=%d (cru %d,%d)   ", sx, sy, p.x, p.y);
    tft.drawString(buf, SCREEN_WIDTH / 2, SCREEN_HEIGHT - 6, 2);
  }
  delay(15);
}
