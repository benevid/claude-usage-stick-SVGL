#!/usr/bin/env bash
#
# Build / upload / monitor do Claude Usage Stick — ESP32-2432S028 (CYD).
#
# Uso:
#   ./build.sh                 # compila
#   ./build.sh upload          # compila + grava (porta padrão COM10)
#   ./build.sh upload <porta>  # compila + grava na porta indicada
#   ./build.sh monitor <porta> # abre o serial monitor (115200)
#
# Pré-requisitos: core esp32:esp32 (qualquer 3.3.x), libs TFT_eSPI e
# XPT2046_Touchscreen (arduino-cli lib install "XPT2046_Touchscreen"),
# além das já usadas no app original (ArduinoJson, lvgl).
#
# Os pinos/driver da TFT_eSPI são passados via -D (USER_SETUP_LOADED=1),
# NÃO pelo User_Setup.h da lib instalada globalmente — essa máquina tem
# outros projetos que também usam TFT_eSPI com pinagens diferentes.
# Os valores abaixo têm que bater com firmware/claude_stick_cyd/config.h.
# O backlight (TFT_BL_PIN) NÃO é passado pra TFT_eSPI — o app controla o
# brilho sozinho via PWM (ledcAttach/ledcWrite), igual ao firmware S3.
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

if command -v arduino-cli >/dev/null 2>&1; then
  CLI="arduino-cli"
else
  CLI="/c/Program Files/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe"
  if [ ! -f "$CLI" ]; then
    echo "erro: arduino-cli nao encontrado (nem no PATH nem no Arduino IDE padrao)" >&2
    exit 1
  fi
fi

SKETCH_DIR="$(pwd)"
# huge_app: 3MB app (sem OTA) / 1MB spiffs — o app completo (LVGL + TLS +
# imagens embutidas + todas as telas) passou de 1.9MB (min_spiffs) por
# pouco. Sem OTA nao e perda: essa placa so recebe firmware via cabo mesmo.
# Historico usa poucas dezenas de KB, 1MB de LittleFS sobra bastante.
FQBN="esp32:esp32:esp32:PSRAM=disabled,PartitionScheme=huge_app,FlashSize=4M"
PORT_DEFAULT="COM10"

TFTFLAGS="-DUSER_SETUP_LOADED=1 -DILI9341_DRIVER=1 -DTFT_WIDTH=240 -DTFT_HEIGHT=320 \
-DTFT_MISO=12 -DTFT_MOSI=13 -DTFT_SCLK=14 -DTFT_CS=15 -DTFT_DC=2 -DTFT_RST=-1 \
-DLOAD_GLCD=1 -DLOAD_FONT2=1 -DLOAD_FONT4=1 \
-DSPI_FREQUENCY=40000000 -DSPI_READ_FREQUENCY=20000000"
LVFLAGS="-DLV_CONF_INCLUDE_SIMPLE -I${SKETCH_DIR}"
EXTRAFLAGS="$TFTFLAGS $LVFLAGS"

cmd="${1:-build}"
port="${2:-$PORT_DEFAULT}"

case "$cmd" in
  monitor)
    exec "$CLI" monitor -p "$port" -c baudrate=115200
    ;;
  build)
    echo "==> compilando ($FQBN)"
    "$CLI" compile \
      --fqbn "$FQBN" \
      --build-property "compiler.cpp.extra_flags=$EXTRAFLAGS" \
      --build-property "compiler.c.extra_flags=$EXTRAFLAGS" \
      "$SKETCH_DIR"
    ;;
  upload)
    echo "==> compilando + gravando em $port ($FQBN)"
    "$CLI" compile \
      --fqbn "$FQBN" \
      --build-property "compiler.cpp.extra_flags=$EXTRAFLAGS" \
      --build-property "compiler.c.extra_flags=$EXTRAFLAGS" \
      --upload -p "$port" \
      "$SKETCH_DIR"
    ;;
  *)
    echo "comando desconhecido: $cmd (use: build | upload | monitor)" >&2
    exit 1
    ;;
esac
