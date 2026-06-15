#!/usr/bin/env bash
#
# Build / upload / monitor do Claude Usage Stick (Guition JC4832W535, ESP32-S3).
#
# Uso:
#   ./build.sh                 # compila
#   ./build.sh upload          # compila + grava (porta padrão abaixo)
#   ./build.sh upload <porta>  # compila + grava na porta indicada
#   ./build.sh monitor <porta> # abre o serial monitor (115200)
#
# Pré-requisitos (ver firmware/REFERENCIA-HARDWARE-LVGL.md):
#   - arduino-cli 1.4.x, core esp32:esp32 3.3.8
#   - libs: GFX Library for Arduino 1.6.5, lvgl 9.2.2
#
# O -DLV_CONF_INCLUDE_SIMPLE + -I<sketch> faz o LVGL achar o nosso lv_conf.h.
# Fallback, se der "lv_conf.h not found": copie lv_conf.h para a pasta de
# libraries do Arduino (um nível acima da pasta `lvgl`).
set -euo pipefail

SKETCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FQBN="esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=custom,CDCOnBoot=cdc,USBMode=hwcdc,FlashMode=qio"
PORT_DEFAULT="/dev/cu.usbmodem101"

LVFLAGS="-DLV_CONF_INCLUDE_SIMPLE -I${SKETCH_DIR}"

cmd="${1:-build}"
port="${2:-$PORT_DEFAULT}"

case "$cmd" in
  monitor)
    exec arduino-cli monitor -p "$port" -c baudrate=115200
    ;;
  build)
    echo "==> compilando ($FQBN)"
    arduino-cli compile \
      --fqbn "$FQBN" \
      --build-property "compiler.cpp.extra_flags=$LVFLAGS" \
      --build-property "compiler.c.extra_flags=$LVFLAGS" \
      "$SKETCH_DIR"
    ;;
  upload)
    # `compile --upload` compila e grava num passo só (upload puro não aceita --build-property)
    echo "==> compilando + gravando em $port ($FQBN)"
    arduino-cli compile \
      --fqbn "$FQBN" \
      --build-property "compiler.cpp.extra_flags=$LVFLAGS" \
      --build-property "compiler.c.extra_flags=$LVFLAGS" \
      --upload -p "$port" \
      "$SKETCH_DIR"
    ;;
  *)
    echo "comando desconhecido: $cmd (use: build | upload | monitor)" >&2
    exit 1
    ;;
esac
