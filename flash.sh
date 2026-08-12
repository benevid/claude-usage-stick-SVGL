#!/usr/bin/env bash
#
# flash.sh — grava a versão atual do firmware num Claude Usage Stick.
#
# Uso: conecte a tela (Guition JC4832W535) no USB e rode:
#     ./flash.sh
#
# O script encontra a porta sozinho (espera até 30s se ainda não conectou),
# compila e grava. Pré-requisitos: arduino-cli + core esp32 3.3.11 + libs
# (ver firmware/REFERENCIA-HARDWARE-LVGL.md).
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")"

if ! command -v arduino-cli >/dev/null; then
  echo "erro: arduino-cli nao encontrado (brew install arduino-cli)" >&2
  exit 1
fi

find_port() { ls /dev/cu.usbmodem* 2>/dev/null | head -1; }

PORT="$(find_port || true)"
if [ -z "$PORT" ]; then
  echo "==> nenhuma tela na USB — conecte o device (aguardando ate 30s)..."
  for _ in $(seq 1 30); do
    sleep 1
    PORT="$(find_port || true)"
    [ -n "$PORT" ] && break
  done
fi
if [ -z "$PORT" ]; then
  echo "erro: nenhum /dev/cu.usbmodem* apareceu. A tela esta conectada e ligada?" >&2
  exit 1
fi

echo "==> tela encontrada em $PORT"
echo "==> compilando e gravando a versao atual..."
firmware/claude_stick/build.sh upload "$PORT"
echo
echo "==> pronto! O device reinicia sozinho (vai pedir o PIN na tela)."
