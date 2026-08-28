#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// Bring-up — ESP32-2432S028 ("Cheap Yellow Display" / CYD)
// ESP32 comum (não S3), ILI9341 por SPI, touch XPT2046 resistivo
// num barramento SPI PRÓPRIO (pinos diferentes do display).
//
// VALIDADO no hardware real (2026-08-27): cores corretas (sem swap/
// inversão), orientação paisagem correta em TFT_ROTATION=1, e touch
// com a calibração abaixo rastreia certinho embaixo do dedo nos 4
// cantos. Esta é a referência conhecida-boa p/ o port completo do app
// (equivalente ao firmware/REFERENCIA-HARDWARE-LVGL.md da placa S3).
// ============================================================

// ── Display ILI9341 (SPI) ───────────────────────────────
// Também usados via -D no build.sh para configurar a TFT_eSPI
// (User_Setup "solto", sem tocar no arquivo global da lib).
#define TFT_MISO_PIN   12
#define TFT_MOSI_PIN   13
#define TFT_SCLK_PIN   14
#define TFT_CS_PIN     15
#define TFT_DC_PIN      2
#define TFT_RST_PIN    -1   // não usado nesta placa
#define TFT_BL_PIN     21   // backlight, HIGH = ligado

#define SCREEN_WIDTH   320  // paisagem (nativo 240x320 retrato)
#define SCREEN_HEIGHT  240
#define TFT_ROTATION    1   // confirmado: orientação correta, nada cortado

// ── Touch XPT2046 (SPI separado do display) ─────────────
// Confirmado: raw_x cresce esquerda->direita e raw_y cresce cima->baixo
// já alinhado com TFT_ROTATION=1 (sem swap/inversão de eixo). Calibração
// medida (TS_MINX/MAXX/MINY/MAXY) está em bringup_cyd.ino.
#define TOUCH_MOSI_PIN 32
#define TOUCH_MISO_PIN 39
#define TOUCH_CLK_PIN  25
#define TOUCH_CS_PIN   33
#define TOUCH_IRQ_PIN  36

#endif // CONFIG_H
