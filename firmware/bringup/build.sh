#!/usr/bin/env bash
#
# Build / upload / monitor do bring-up (Guition JC4832W535, ESP32-S3).
#
# Uso:
#   ./build.sh                 # compila
#   ./build.sh upload          # compila + grava (porta padrão abaixo)
#   ./build.sh upload <porta>  # compila + grava na porta indicada
#   ./build.sh monitor <porta> # abre o serial monitor (115200)
#
# Duas diferenças em relação ao build.sh do claude_stick, ambas necessárias:
#
#   1. PartitionScheme=huge_app (não `custom`): esta pasta não tem partitions.csv
#      próprio — `custom` faria o core procurar um .csv aqui e abortar com
#      "cp: .../partitions/.csv: No such file or directory". O bring-up não usa
#      LittleFS nem NVS, então o esquema padrão basta.
#
#   2. O -I aponta para ../claude_stick e vai TAMBEM em compiler.S.extra_flags:
#      o bring-up nao tem lv_conf.h proprio e reaproveita o do firmware (que ja
#      traz a guarda __ASSEMBLY__). O core monta os .S da LVGL com
#      compiler.S.extra_flags — c/cpp.extra_flags nao alcancam essa etapa — e sem
#      o -I ali a LVGL cai no fallback "../../lv_conf.h", um arquivo solto na
#      pasta de libraries do Arduino que pode ser de qualquer outro projeto; um
#      #include <stdint.h> desprotegido nele quebra a montagem com
#      "unknown opcode or format name 'typedef'".
set -euo pipefail

SKETCH_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LV_CONF_DIR="$(cd "${SKETCH_DIR}/../claude_stick" && pwd)"
FQBN="esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=huge_app,CDCOnBoot=cdc,USBMode=hwcdc,FlashMode=qio"
PORT_DEFAULT="/dev/cu.usbmodem101"

LVFLAGS="-DLV_CONF_INCLUDE_SIMPLE -I${LV_CONF_DIR}"

cmd="${1:-build}"
port="${2:-$PORT_DEFAULT}"

case "$cmd" in
  monitor)
    exec arduino-cli monitor -p "$port" -c baudrate=115200
    ;;
  build)
    echo "==> compilando bring-up ($FQBN)"
    arduino-cli compile \
      --fqbn "$FQBN" \
      --build-property "compiler.cpp.extra_flags=$LVFLAGS" \
      --build-property "compiler.c.extra_flags=$LVFLAGS" \
      --build-property "compiler.S.extra_flags=$LVFLAGS" \
      "$SKETCH_DIR"
    ;;
  upload)
    echo "==> compilando + gravando bring-up em $port ($FQBN)"
    arduino-cli compile \
      --fqbn "$FQBN" \
      --build-property "compiler.cpp.extra_flags=$LVFLAGS" \
      --build-property "compiler.c.extra_flags=$LVFLAGS" \
      --build-property "compiler.S.extra_flags=$LVFLAGS" \
      --upload -p "$port" \
      "$SKETCH_DIR"
    ;;
  *)
    echo "comando desconhecido: $cmd (use: build | upload | monitor)" >&2
    exit 1
    ;;
esac
