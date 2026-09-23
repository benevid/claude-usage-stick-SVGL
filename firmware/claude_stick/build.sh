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
# Opcao (em qualquer posicao): --logo <arquivo.png|svg>
#   Compila o firmware com o logo de um parceiro no lugar do wordmark
#   "CLAUDE CODE" no header. O tools/gen_partner_logo.py gera partner_logo.h
#   num diretorio temporario que entra por -I + -DPARTNER_LOGO — nada fica
#   dentro do sketch, e um build sem --logo volta ao firmware padrao sozinho.
#
# Pré-requisitos (ver firmware/REFERENCIA-HARDWARE-LVGL.md):
#   - arduino-cli 1.4.x, core esp32:esp32 3.3.11
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

# --logo <arquivo> pode vir antes ou depois de cmd/porta
logo=""; args=()
while [ $# -gt 0 ]; do
  case "$1" in
    --logo) [ $# -ge 2 ] || { echo "erro: --logo precisa de um arquivo" >&2; exit 1; }
            logo="$2"; shift 2 ;;
    *)      args+=("$1"); shift ;;
  esac
done
cmd="${args[0]:-build}"
port="${args[1]:-$PORT_DEFAULT}"

if [ -n "$logo" ]; then
  GEN_DIR="$(mktemp -d)"
  echo "==> logo do parceiro: $logo"
  python3 "$SKETCH_DIR/../../tools/gen_partner_logo.py" "$logo" -o "$GEN_DIR/partner_logo.h"
  LVFLAGS="$LVFLAGS -DPARTNER_LOGO -I$GEN_DIR"
fi

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
