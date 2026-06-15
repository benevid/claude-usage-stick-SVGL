/**
 * lv_conf.h — config do LVGL 9.2 para o Claude Usage Stick (JC4832W535).
 *
 * Compile com -DLV_CONF_INCLUDE_SIMPLE (ver build.sh) para que o LVGL ache
 * este arquivo pelo include path do sketch. Alternativa: copiar este arquivo
 * para a pasta de libraries do Arduino (um nível acima da pasta `lvgl`).
 *
 * Arquivo parcial: o que não estiver definido aqui usa o default do
 * lv_conf_internal.h. As opções abaixo são as que importam para esta placa.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR
 *====================*/
#define LV_COLOR_DEPTH 16
/* Pipeline validado no bring-up usa cópia direta RGB565 (sem swap).
   Se vermelho/azul saírem trocados, mude para 1. */
#define LV_COLOR_16_SWAP 0

/*=========================
   MEMÓRIA
   Pool interno do LVGL (objetos/estilos). O buffer de render full-screen
   (480x320x2) é alocado à parte na PSRAM, dentro do sketch.
 *=========================*/
#define LV_USE_STDLIB_MALLOC   LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING   LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_SPRINTF  LV_STDLIB_BUILTIN
#define LV_MEM_SIZE            (96 * 1024U)

/*====================
   HAL / SISTEMA
 *====================*/
#define LV_USE_OS LV_OS_NONE
/* tick vem de lv_tick_set_cb(millis) no sketch */

/*====================
   RENDER
 *====================*/
#define LV_USE_DRAW_SW 1

/*====================
   FONTES (Montserrat usadas na UI)
 *====================*/
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_MONTSERRAT_40 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

/*====================
   WIDGETS usados
 *====================*/
#define LV_USE_LABEL        1
#define LV_USE_BUTTON       1
#define LV_USE_BUTTONMATRIX 1
#define LV_USE_BAR          1
#define LV_USE_LIST         1
#define LV_USE_TEXTAREA     1
#define LV_USE_KEYBOARD     1
#define LV_USE_CANVAS       1
#define LV_USE_IMAGE        1
#define LV_USE_LINE         1
#define LV_USE_ARC          1
#define LV_USE_SPINNER      1
#define LV_USE_TILEVIEW     1
#define LV_USE_CHART        1

#endif /*LV_CONF_H*/
