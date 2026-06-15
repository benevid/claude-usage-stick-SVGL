#ifndef CONFIG_H
#define CONFIG_H

// ============================================
// Guition JC4832W535 — pinos (ver REFERENCIA-HARDWARE-LVGL.md)
// ============================================

// Display QSPI (AXS15231B)
#define TFT_CS    45
#define TFT_SCK   47
#define TFT_SDA0  21
#define TFT_SDA1  48
#define TFT_SDA2  40
#define TFT_SDA3  39
#define TFT_BL    1
#define TFT_TE    38

// Touch I2C (AXS15231B)
#define TOUCH_SDA  4
#define TOUCH_SCL  8
#define TOUCH_INT  3
#define TOUCH_ADDR 0x3B

// Display
#define SCREEN_WIDTH   480
#define SCREEN_HEIGHT  320
#define QSPI_FREQ      40000000UL

#endif // CONFIG_H
