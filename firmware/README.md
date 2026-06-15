# firmware/ — Claude Usage Stick (ESP32-S3 + LVGL)

Firmware para a tela touch **Guition JC4832W535** (AXS15231B QSPI, 480×320).

- **`claude_stick/`** — o projeto. Sketch arduino-cli completo: busca o uso do
  Claude (headers da `api.anthropic.com`), saúde dos modelos (`status.claude.com`),
  e renderiza tudo em LVGL 9 com navegação touch. Token OAuth guardado cifrado
  (AES-256-GCM + PIN). Veja o build em [`claude_stick/build.sh`](claude_stick/build.sh).
- **`bringup/`** — bring-up validado no hardware (cores certas, orientação
  USB-à-esquerda e touch alinhado). Referência conhecida-boa da config de
  display/touch; não é o app.
- **`REFERENCIA-HARDWARE-LVGL.md`** — pinos, libs testadas, pipeline de flush
  (rotação 270° CW na mão) e armadilhas (PSRAM OPI obrigatória, etc.).

Comece por [`../README.md`](../README.md) para a visão geral e o passo a passo de build.
