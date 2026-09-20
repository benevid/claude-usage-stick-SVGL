# Plano de porte — Claude Usage Stick para a placa LCDWIKI ES3C28P

**Placa de destino:** LCDWIKI **ES3C28P** — 2.8" IPS 240×320, ESP32-S3, driver **ILI9341V** (SPI 4 fios),
touch capacitivo **FT6336G** (I²C 0x38).
**Placa de origem (já suportada):** Guition **JC4832W535** — 3.5" 480×320, AXS15231B (QSPI + touch I²C 0x3B).

**Documento escrito sem acesso à placa.** Tudo que está marcado como *(a confirmar no bring-up)* veio do
manual técnico do fabricante ou de inferência sobre a biblioteca, não de medição. Quem tem a placa é a
autoridade final: se o hardware contradisser este documento, o hardware está certo.

---

## 0. Como usar este documento

Ele foi escrito para ser executado com **Claude Code** dentro do repositório clonado, por alguém que tem a
placa fisicamente na mesa. As fases são sequenciais e cada uma tem um **critério de aceite verificável no
hardware**. Não pule a Fase 1 — ela existe justamente porque o resto do plano assume que o pipeline de
vídeo e toque já está resolvido.

Prompt inicial sugerido para o Claude Code:

> Leia `PLANO-PORTE-ES3C28P.md` na raiz do repositório. Vamos executar a Fase 0 e a Fase 1. Eu tenho a
> placa ES3C28P conectada na USB. Não altere nada dentro de `firmware/claude_stick/` — essa é a placa
> antiga e precisa continuar funcionando.

Trabalhe **uma fase por sessão**. O `.ino` principal tem ~2.700 linhas; tentar fazer tudo de uma vez
estoura o contexto e produz layout que ninguém revisou.

---

## 1. O que o projeto é (contexto mínimo)

Firmware de um gadget de mesa que mostra o consumo de rate limit do Claude Code, lido direto dos headers
de resposta de `api.anthropic.com`. Sem backend. Sketch único de arduino-cli, UI em LVGL 9.2.

- `firmware/claude_stick/` — o firmware da placa antiga. **Não tocar.**
- `firmware/bringup/` — sketch de bring-up da placa antiga (referência, não é o app).
- `firmware/REFERENCIA-HARDWARE-LVGL.md` — pinagem e versões validadas da placa antiga.
- `tools/` — scripts Python auxiliares (bridge de tokens, geração de assets e mockups).

Comentários de código e strings de tela são em **português**; o texto de UI é bilíngue pela macro
`TRS(pt, en)`.

**Não existe suíte de testes.** Verificação = `build.sh` compila limpo, grava e confere na tela.
O serial desta família de placas costuma ser instável — **prefira desenhar o estado de depuração na tela
a confiar em `Serial.printf`**.

---

## 2. Regras do projeto que NÃO estão no repositório

O arquivo `CLAUDE.md` do projeto é **gitignored** — quem clona o repo não o recebe. O que está nesta seção
é o conteúdo dele que importa para este porte. Trate como restrição de projeto, não como sugestão.

### 2.1 Toolchain (fixado por tentativa e erro — não atualize)

| item | versão |
|---|---|
| arduino-cli | 1.4.x |
| core `esp32:esp32` | **3.3.11** |
| GFX Library for Arduino | **1.6.5** |
| lvgl | **9.2.2** |

Versões mais novas de qualquer um dos três já quebraram o build ou a renderização neste projeto.
`tools/gen_*.py` precisam de `rsvg-convert` (`brew install librsvg`) e Pillow.

### 2.2 A configuração da LVGL chega em TRÊS receitas de compilação, não duas

Os `build.sh` injetam `-DLV_CONF_INCLUDE_SIMPLE -I<sketch>` via `compiler.c.extra_flags` e
`compiler.cpp.extra_flags`. Mas o `lv_conf_internal.h` também puxa o `lv_conf.h` ao **montar** os
`lv_blend_helium.S` / `lv_blend_neon.S` da LVGL, e o core ESP32 monta esses arquivos com
`compiler.S.extra_flags`. Consequências, ambas já observadas neste projeto:

- O `#include <stdint.h>` do `lv_conf.h` **tem que ficar dentro da guarda `#ifndef __ASSEMBLY__`**, senão
  o assembler engasga com typedefs de C (`unknown opcode or format name 'typedef'`).
- Um sketch **sem `lv_conf.h` próprio** cai no fallback `../../lv_conf.h` da LVGL — um arquivo solto na
  pasta `libraries/` do Arduino que pode ser de qualquer outro projeto. Um sketch assim precisa do `-I`
  **também** em `compiler.S.extra_flags` (é o que `firmware/bringup/build.sh` faz nas três).

### 2.3 Arquitetura do firmware

**Toda a UI vive no `.ino`** (~2.700 linhas, com comentários de banner separando seções). Os `.cpp/.h`
irmãos são dados/IO puros: `api.cpp` (uso + sonda por modelo), `status.cpp` (incidentes do
status.claude.com), `crypto.cpp` (AES-256-GCM), `certs.cpp` (bundle de CAs), `wifi_manager.h` (3 SSIDs
em NVS), `touch.h` (driver do toque), `logo_assets.h` (**gerado — não editar à mão**).

**Loop cooperativo, uma thread só.** Sem tasks de RTOS, sem `lv_timer` para lógica de aplicação.
`loop()` roda `lv_task_handler()`, atende o `WebServer`, aplica a mudança de estado pendente e então
dispara cada trabalho periódico por delta de `millis()`: contadores de 1 s, barra de refresh de 250 ms,
bobbing do mascote de 80 ms, piscada, slideshow, overlay de limiar. As buscas HTTP são **bloqueantes**
(~1–2 s) e rodam inline a partir do `loop()`.

**Máquina de estados.** `enum State` + `request_state(s)` marcam `g_pending`/`g_dirty`; o `loop()` chama
`render_state()`, que derruba a tela (`lv_obj_clean`), limpa o `lv_layer_top()`, **zera todo ponteiro
`lv_obj_t*` cacheado em `g_ui`/`g_masc`** e reconstrói. *Qualquer ponteiro novo com escopo de tela precisa
ser zerado ali, senão fica pendurado.*

**Dois caminhos de atualização — mantenha-os distintos:**
- `render_state()` → reconstrução total, reseta o tileview para o tile 0. Só quando o layout precisa mudar.
- `refresh_ui_values()` → atualiza labels/medidores/gráficos no lugar, preserva o tile atual. É o padrão
  depois de um poll de fundo.

**Ciclo de dados.** `do_refresh()` é a primeira carga (mostra `ST_LOADING`, termina em `ST_MAIN`/`ST_ERROR`);
`bg_refresh()` é todo poll posterior (nunca troca de tela, mantém dado velho em caso de falha, mostra
"atualizando..." no header). Cada ciclo: `fetchUsage()` → `hist_push` + `accumulate_heat` +
`save_history()` → `check_thresholds()` → `fetchModelStatus()` → `probe_next_model()`.

**Contrato da API.** O device faz POST de `max_tokens:1` em `/v1/messages` e **descarta o corpo** — todo o
dado vem dos headers `anthropic-ratelimit-unified-*`, coletados via `https.collectHeaders()`. Adicionar um
campo significa adicionar o nome do header em `RL_HEADERS` **e** incrementar `RL_HEADER_COUNT` em
`api.cpp`. O token OAuth (`sk-ant-oat01-…`, vindo de `claude setup-token`) só funciona porque a requisição
imita o Claude Code: `anthropic-beta: oauth-2025-04-20` + User-Agent `claude-code/x.y.z`.
A API **não expõe contagem de tokens** para contas de assinatura — é para isso que existem
`tools/token_bridge.py` + `POST /tokens`.

**Persistência.** NVS (`Preferences`, namespace `claude`) guarda o blob cifrado do token, o contador de
tentativas de PIN e os ajustes (`poll`, `tz`, `bri`, `slide`, `heatm`, `lang`); toda leitura é validada por
faixa. O histórico vive em LittleFS `/hist<slot>.bin` como dump de struct com magic
(`HIST_MAGIC_V2`, 160 amostras + 24 h de burn + 31 dias). **Mudar o layout exige magic novo + ramo de
migração em `load_history()`.**

**Contas (v2.2, `accounts.cpp/.h`).** Até 4 slots: NVS `blob0..blob3` + `lbl0..lbl3` + `acct` (índice ativo),
um arquivo de histórico por slot. `g_sessionPin` guarda o PIN em RAM depois do unlock para trocar de slot
sem repedir; nunca é persistido. **A migração para v2.2 é aditiva de propósito** — o primeiro boot copia
`blob`→`blob0` e `/hist.bin`→`/hist0.bin` e **preserva os originais** (caminho de rollback). A cópia do
histórico passa por `/hist0.tmp` antes do rename: escrever direto no nome final deixaria um `/hist0.bin`
truncado que a guarda `exists()` leria como "já migrado". **Não "limpe" as chaves antigas.**

**HTTP no device.** O onboarding serve um formulário de token em `/`; o dashboard serve `GET /window`,
`POST /tokens` (409 em conflito de conta), `GET /` (info) e mDNS `claude-stick.local`. `render_state()`
chama `stop_web()` primeiro — cada tela sobe o servidor de que precisa.

**Fluxo de segurança.** Token digitado uma vez no navegador → validado por chamada real à API → cifrado em
AES-256-GCM com chave derivada de um PIN de 4 dígitos; o PIN nunca é armazenado (PIN errado = tag GCM não
bate). 10 falhas apagam as credenciais, o lockout dobra a cada vez.

### 2.4 Pegadinhas da LVGL 9.2 neste código (valem integralmente para a placa nova)

- **As fontes Montserrat embutidas trazem só ASCII + `°` + `•`.** Sem acentos, sem travessão. Escreva as
  strings de UI sem acento e use `"\xE2\x80\xA2"` como separador.
- **Os protótipos auto-gerados do `.ino` quebram com tipos próprios**: uma função que retorna `MyStruct*`
  ganha um protótipo emitido *acima* da definição da struct (`does not name a type`). Retorne um índice —
  veja `day_slot()`.
- **Overlays têm que viver em `lv_layer_top()`** e ser limpos explicitamente em `render_state()`, senão
  vazam entre telas.
- **Anime proceduralmente a partir do `loop()`, não com `lv_anim`**, para qualquer coisa dispensável: um
  `exec_cb` de `lv_anim` apontando para objeto deletado causa crash. Padrão: ponteiros numa struct +
  temporização por `millis()` + deletar tudo de uma vez (`show_moment`/`moment_tick`/`moment_close`).
- Alvos de toque pequenos ganham `lv_obj_set_ext_click_area()` **além** de um tamanho generoso.
- **Linhas de lista não propagam `CLICKED`** — anexe o handler por botão.
- **i18n**: `#define TRS(pt, en)` inline em cada string, sem tabela paralela. Trocar idioma salva no NVS e
  chama `request_state()` para reconstruir.
- `lv_image_dsc_t` usa inicialização posicional e bytes ARGB8888 na ordem **B,G,R,A** (ver `logo_assets.h`);
  `lv_obj_set_style_image_recolor()` acinzenta o sprite do Clawd em runtime em vez de embarcar um segundo
  asset.

### 2.5 Convenções do repositório

- `.env`, `.mcp.json`, `.claude/` e `CLAUDE.md` são gitignored — **nada de segredo e nada de config local
  de ferramenta no git.**
- Os screenshots do README são mockups gerados, não fotos: re-rode `tools/gen_mockups.py` depois de
  mudanças de layout.
- **O README é bilíngue: `README.md` (inglês, o principal) + `README.pt-BR.md`.** Uma mudança técnica
  precisa entrar nos dois — tradução desatualizada é pior que nenhuma, porque alguém segue instrução que
  não vale mais.
- `.github/PULL_REQUEST_TEMPLATE.md` exige **foto do device rodando o firmware** em PRs de hardware.
  Este porte é exatamente esse caso: tire a foto.

---

## 3. A placa nova: especificações e pinagem

Fonte: `ES3C28P_Manual_Tecnico.pdf` (LCDWIKI, rev. V1.0, 2025-06-14). O SKU **ES3N28P** é a mesma placa
**sem** touch — este porte é do **ES3C28P** (com touch).

### 3.1 Núcleo

| item | valor |
|---|---|
| MCU | ESP32-S3, Xtensa LX7 dual-core, 240 MHz |
| PSRAM | **8 MB OPI interna** |
| Flash | **16 MB QSPI externa** |
| Display | ILI9341V, 240×320, IPS, **SPI 4 fios**, RGB565/RGB666 |
| Touch | FT6336G capacitivo, **I²C, endereço 0x38** |
| Alimentação | USB Type-C (ligado ao USB nativo do S3) ou LiPo 3,7 V |
| Extras | slot microSD (SDIO), microfone MEMS, codec de áudio I²S, LED RGB, ADC de bateria |

**O FQBN não muda em relação à placa antiga** — mesmo S3, mesma PSRAM OPI, mesma flash de 16 MB:

```
esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=custom,CDCOnBoot=cdc,USBMode=hwcdc,FlashMode=qio
```

### 3.2 Pinagem completa (do manual, seção 4.2)

**LCD (SPI):**

| sinal | GPIO | observação |
|---|---|---|
| CS | **10** | ativo baixo |
| DC (comando/dado) | **46** | alto = dado, baixo = comando |
| SCK | **12** | |
| MOSI (SDA) | **11** | "write data signal" |
| MISO (SDO) | **13** | "data signal" — não usado para escrita |
| RESET | — | **compartilhado com o CHIP_PU do ESP32-S3** → use `GFX_NOT_DEFINED` |
| Backlight | **45** | alto acende. **GPIO45 é strapping pin (VDD_SPI)** — ver §8 |

**Touch (FT6336G):**

| sinal | GPIO |
|---|---|
| SDA | **16** |
| SCL | **15** |
| RST | **18** (ativo baixo) |
| INT | **17** (baixo quando há toque) |

O barramento I²C é **compartilhado** com o codec de áudio e com o conector de expansão I²C.

**Outros (não usados pelo firmware, mas bom saber para não pisar):**

| função | GPIOs |
|---|---|
| microSD (SDIO) | CLK 38 · CMD 40 · D0 39 · D1 41 · D2 48 · D3 47 |
| LED RGB (IC embutido, 1 fio tipo WS2812) | **42** |
| ADC de bateria | **9** |
| Áudio | EN 1 · MCLK 4 · BCLK 5 · DOUT 6 · WS 7 · DIN 8 |
| USB nativo | D− 19 · D+ 20 |
| UART0 | TX 43 · RX 44 |
| Pinos livres no conector de expansão | **2, 3, 14, 21** |

**Não use** 26–37 (PSRAM/flash) nem 27–32 (compartilhados flash/PSRAM).

### 3.3 Orientação

O painel é nativamente **retrato 240×320**. A recomendação é rodar em **paisagem 320×240**, porque preserva
a arquitetura da UI atual (header no topo + 4 tiles com swipe horizontal + dots embaixo). Retrato exigiria
repensar a navegação, não só as coordenadas.

Diferente da placa antiga — onde a rotação é feita **à mão** dentro do `disp_flush_cb` porque o
`Arduino_Canvas` produzia cores erradas — o **ILI9341 tem rotação nativa por MADCTL**. Passe a rotação no
construtor (`r = 1` ou `r = 3`) e **não replique o hack de rotação manual da placa antiga**.

`r=1` e `r=3` diferem em 180°: um deixa o Type-C à esquerda, o outro à direita. Escolha no bring-up.
A regra que vale de ambas as placas: **display e touch precisam girar juntos.** Mudar um sem o outro deixa
o toque espelhado, e o sintoma engana (parece calibração, é rotação).

---

## 4. O que muda e o que não muda

### Reaproveitado sem uma linha alterada

`api.cpp/.h` · `status.cpp/.h` · `crypto.cpp/.h` · `certs.cpp/.h` · `accounts.cpp/.h` ·
`wifi_manager.h` · `logo_assets.h` · `partitions.csv` · `lv_conf.h`

E, dentro do `.ino`, as seções de: NVS/persistência, WebServer (onboarding + bridge), histórico/heatmap,
navegação, `render_state()`, NTP e ciclo de dados. São ~1.070 linhas que **devem ser copiadas literalmente**.

### Reescrito

| o quê | por quê |
|---|---|
| `config.h` | pinos, resolução, versão |
| `touch.h` → `touch_ft6336.h` | chip e protocolo totalmente diferentes |
| `disp_flush_cb` + init de display no `setup()` | QSPI+canvas rotacionado à mão → SPI + render parcial |
| **todo o layout** | 480×320 → 320×240 é **metade da área** |

### O custo real é o layout

Só há 10 literais `480`/`320` no `.ino` — mas **123 chamadas de posicionamento absoluto** calibradas para
480×320. Exemplos que quebram imediatamente:

- tile AGORA: dois cards de `228 px` lado a lado = 456 px → não cabe em 320
- tile MODELOS: 4 mascotes em centros `60/180/300/420` → o quarto fica fora da tela
- `refBar`, overlay de momento e tileview são criados com largura `480` fixa

Distribuição das 2.731 linhas do `.ino`:

| bloco | linhas aprox. | trabalho |
|---|---|---|
| lógica pura | ~1.070 | copiar |
| ajustes leves (update de valores, about, loading) | ~330 | mecânico |
| **relayout pesado** (PIN, WiFi, 4 tiles, momentos, settings, header) | **~1.330** | refazer coordenadas |
| pipeline display/touch + `setup()` | ~190 | reescrever |

**Estimativa: 10–14 h de trabalho efetivo, 3–5 sessões.**

---

## 5. Estrutura a criar

```
firmware/
  es3c28p-bringup/          # Fase 1 — sketch mínimo de validação de hardware
    bringup.ino
    config.h
    touch_ft6336.h
    build.sh
  es3c28p/                  # Fase 2+ — o firmware
    es3c28p.ino
    config.h
    touch_ft6336.h
    lv_conf.h               # cópia de claude_stick/lv_conf.h
    partitions.csv          # cópia de claude_stick/partitions.csv
    api.cpp api.h           # cópias literais
    status.cpp status.h
    crypto.cpp crypto.h
    certs.cpp certs.h
    accounts.cpp accounts.h
    wifi_manager.h
    logo_assets.h
    build.sh
flash-es3c28p.sh            # na raiz, irmão do flash.sh existente
```

**Sobre a duplicação:** os ~570 linhas de lógica ficam copiadas em dois lugares. É intencional — a
prioridade é não arriscar o firmware que já está em produção. A contrapartida é que **todo bugfix de API,
crypto ou contas precisa entrar nos dois sketches**. Registre isso no README do firmware novo e considere,
mais adiante, um `tools/sync_shared.py` que copie de `claude_stick/` para `es3c28p/` e avise quando
divergirem. Não extraia os arquivos para uma biblioteca comum agora: isso mexeria em
`firmware/claude_stick/`, que está fora do escopo deste trabalho.

---

## 6. Fases

### Fase 0 — Ambiente (30 min)

1. Instalar `arduino-cli` 1.4.x, o core `esp32:esp32` **3.3.11** e as libs **GFX 1.6.5** e **lvgl 9.2.2**
   (versões exatas — §2.1).
2. Clonar o repositório.
3. **Sanity check:** `firmware/claude_stick/build.sh` deve compilar limpo, mesmo sem a placa antiga. Se
   não compilar, o problema é toolchain e precisa ser resolvido antes de qualquer outra coisa.
4. Conectar a ES3C28P na USB e confirmar que aparece uma porta (`ls /dev/cu.usbmodem*` no macOS/Linux).
   Se não aparecer: segure BOOT, aperte e solte RESET, solte BOOT — isso força o modo de download.

**Aceite:** `build.sh` do firmware antigo compila e a placa nova enumera na USB.

---

### Fase 1 — Bring-up (não pule) (1–3 h)

Objetivo: descobrir empiricamente os quatro parâmetros que este documento não pode afirmar — **rotação,
inversão de cores, frequência SPI e mapeamento do toque**.

Crie `firmware/es3c28p-bringup/` com um sketch mínimo (código de partida em §7.2 e §7.3) que:

1. Inicializa o ILI9341 e pinta **três barras verticais: vermelha, verde, azul, nessa ordem, da esquerda
   para a direita**, com o rótulo da cor escrito em cima em texto.
2. Escreve `TOP-LEFT` no canto superior esquerdo da tela.
3. Acende o backlight em GPIO45.
4. Inicializa o FT6336G e, a cada toque, **desenha um ponto e escreve `x,y` na própria tela**
   (lembre: serial não é confiável nesta família — §1).
5. Imprime no serial (best effort) o vendor ID lido do registrador `0xA8` do FT6336G — deve ser `0x11`.

**O que você está medindo:**

| leitura | conclusão |
|---|---|
| barras aparecem R,G,B nessa ordem | ordem de cor correta |
| barras aparecem B,G,R invertidas | troque `ips` no construtor, ou aplique swap de bytes (§8) |
| `TOP-LEFT` no canto certo com o Type-C onde você quer | rotação correta — **anote o valor de `r`** |
| toque no canto onde está `TOP-LEFT` retorna x,y ≈ 0,0 | touch alinhado com o display |
| toque em `TOP-LEFT` retorna x alto ou y alto | rotação do touch errada — ajuste o `switch(_rotation)` |
| tela suja/chuviscada | baixe a frequência SPI (teste 80 → 60 → 40 MHz) |

**Aceite:** as três barras aparecem na ordem certa, o texto está legível e na orientação escolhida, e
tocar num canto conhecido devolve as coordenadas daquele canto. **Anote os valores finais de rotação e
frequência — o resto do plano depende deles.**

> Se algo aqui não fechar, **pare e resolva antes de seguir**. Todo o trabalho das fases seguintes assume
> um pipeline de vídeo confiável; depurar layout em cima de um driver duvidoso desperdiça horas.

---

### Fase 2 — Esqueleto do firmware (2–3 h)

Objetivo: **bootar até a tela de PIN** com toda a lógica original intacta, sem se preocupar ainda com o
layout ficar bonito.

1. Criar `firmware/es3c28p/` e **copiar literalmente** de `firmware/claude_stick/`:
   `api.*`, `status.*`, `crypto.*`, `certs.*`, `accounts.*`, `wifi_manager.h`, `logo_assets.h`,
   `lv_conf.h`, `partitions.csv`.
2. Copiar `claude_stick.ino` → `es3c28p.ino` (o nome do `.ino` **precisa** bater com o nome da pasta).
3. Escrever `config.h` novo (§7.1) e `touch_ft6336.h` (§7.3), com os valores confirmados na Fase 1.
4. Trocar o `#include "touch.h"` por `#include "touch_ft6336.h"` e a declaração da instância do touch.
5. Substituir o init de display no `setup()` e o `disp_flush_cb` pelo pipeline SPI (§7.2).
6. Escrever `build.sh` (§7.4) e `flash-es3c28p.sh` (§7.5).
7. Corrigir **apenas** o que impede o boot: as larguras fixas `480` do `refBar`, do tileview e do overlay
   de momento. Nesta fase, aceite que a UI vai estar cortada e feia.

**Ponto de atenção crítico ao copiar:** o `disp_flush_cb` da placa antiga usa
`LV_DISPLAY_RENDER_MODE_FULL` e **ignora o parâmetro `area`**, redesenhando a tela inteira. O pipeline novo
usa `LV_DISPLAY_RENDER_MODE_PARTIAL` e **precisa respeitar `area`**. Copiar o callback antigo sem entender
isso produz uma tela que só desenha lixo — é a armadilha mais provável desta fase.

**Aceite:** a placa liga, o backlight acende, a tela de PIN aparece (mesmo desalinhada), o teclado responde
ao toque e o device conecta no WiFi.

---

### Fase 3 — Reflow do layout (5–7 h, a maior parte do trabalho)

Faça **uma tela por vez**, gravando e conferindo na placa a cada uma. A ordem abaixo é a do fluxo de
primeiro uso, o que ajuda a testar de ponta a ponta.

Os números de linha são do commit atual e servem de mapa; confirme com `grep` antes de editar.

| # | tela | linhas ~ | o que precisa mudar |
|---|---|---|---|
| 1 | PIN (keypad) | 365–511 | keypad `280×180` → cerca de `300×150`; os botões precisam continuar com ≥ 44 px de toque |
| 2 | WiFi (scan + teclado) | 512–606 | lista `452×246` → `~308×150`; campo de texto `452×44` → `~308×40`; o teclado da LVGL ocupa mais proporção da tela — reduza a lista |
| 3 | loading / mensagem | 846–959 | `img_clawd_big` (144×90) ainda cabe; recentralizar |
| 4 | **header + tileview** | 1950–2058 | ver §6.1 abaixo — é o mais delicado |
| 5 | tile 0 — AGORA | 1356–1375 | ver §6.2 |
| 6 | tile 1 — MODELOS | 1377–1393 | ver §6.3 |
| 7 | tile 2 — TENDÊNCIA | 1395–1440 | `TR_W 440` → `~296`, `TR_H 126` → `~96`, card `464×170` → `~308×150` |
| 8 | tile 3 — HEATMAP | 1475–1510 | 24 barras com passo `18 px` = 432 px → passo `12 px` (`18 + h*12` cabe em 306); botões de modo `52×30` → `40×26`, passo 44 |
| 9 | momentos / overlays | 1687–2058 | fundo `480×320` → `320×240`; anel `472×312` → `312×232`; box `176×116` fica; `img_clawd_xl` (176×110) ainda cabe |
| 10 | settings | 2059–2414 | todas as larguras de linha `444`/`452`/`464` → `~300`/`308`; lista `464×268` → `~308×186`; **mantenha as linhas com ≥ 44 px de altura de toque** |
| 11 | about | 2415–2462 | ajuste fino |

#### 6.1 Header e tileview (320×240)

O header atual ocupa 46 px de 320 de altura. Em 240 isso é proporcionalmente muito. Proposta de partida:

```
header:    y 0..33     (34 px)
refBar:    y 34..35    (320 x 2)
tileview:  y 38, tamanho 320 x 186
dots:      alinhados BOTTOM_MID, offset -4
```

No eixo X o header fica apertado. Larguras atuais: Clawd `42` + wordmark `56` + refresh `56` + gear `78`
= 232 px de conteúdo em 320, sem contar espaçamento e o badge de conta. Proposta:

- `img_clawd_sm` (42×26) em `x=6`
- `img_wordmark` (56×26) em `x=52`
- botão refresh reduzido para `44×32`, alinhado `TOP_MID`
- engrenagem reduzida para `52×32`, alinhada `TOP_RIGHT` em `-4`
- `g_hdrStatus` alinhado `TOP_RIGHT` em `-60`
- **badge de conta:** ler o comentário de 6 linhas que existe no código sobre a faixa `138..194`. Ele
  documenta que o hotspot do logo termina em `x=134` e que o `ext_click_area` do botão de refresh começa a
  capturar em `x=202`. Esses números **todos** mudam em 320 — recalcule a faixa livre, não copie. Se não
  sobrar espaço, o badge pode ir para o rodapé, ao lado dos dots.

Ambos os botões precisam manter `lv_obj_set_ext_click_area()`, que é o que torna alvos pequenos usáveis.

#### 6.2 Tile AGORA (320×186)

Dois cards lado a lado, `152` de largura cada:

```
card 5h:     x=6    y=2   w=152  h=176
card semana: x=162  y=2   w=152  h=176
```

Com `pad_all` de 14 (padrão do helper `card()`), sobram ~124 px úteis por card. Ajustes de fonte:

| elemento | antes | proposta |
|---|---|---|
| título | montserrat_14 | montserrat_12 |
| percentual | montserrat_48 | **montserrat_28** |
| medidor (18 segmentos) | `i*11`, seg `8×16` | `i*7`, seg `5×12` (18×7 = 126, cabe) |
| "at" | montserrat_12 | montserrat_12 |
| countdown | montserrat_40 | **montserrat_20** |

O chip de status e a linha de tokens (`g_ui.agChip`, `g_ui.agTok`) descem para `y≈180`, com a largura de
`g_ui.agTok` caindo de `342` para `~200`.

#### 6.3 Tile MODELOS (320×186)

Quatro mascotes lado a lado não cabem: `img_clawd_md` tem **88 px** de largura, e 4×88 = 352 > 320.

**Solução recomendada (sem mexer em assets): grade 2×2.**

```
centros x:  84  e  236
linhas  y:  6   e  96
```

Cada célula: mascote 88×56 + nome (montserrat_14) + chip. O texto de rodapé sobre a sonda sai, e a label
de incidentes (`g_ui.incident`) passa a largura `452` → `~300` e vai para o pé do tile.

**Plano B:** gerar um sprite `img_clawd_xs` com 64 px em `tools/gen_logo_assets.py` (é uma chamada
`clawd(64)` a mais no `main()`), o que permitiria manter os 4 em linha. Custa regenerar o
`logo_assets.h` — e aí ele passa a divergir do da placa antiga, então **os dois firmwares precisariam de
arquivos de asset diferentes**. Só faça isso se a grade 2×2 realmente não ficar boa.

**Lembre:** `build_model_mascot()` posiciona os olhos usando os defines `CLAWD_MD_EYE*` e a piscada é
animada a partir do `loop()`. Se trocar o sprite, os offsets dos olhos mudam junto.

---

### Fase 4 — Extras da placa (opcional, 1–2 h)

A ES3C28P tem hardware que a antiga não tem. Nada disso é requisito; são bônus que só valem se a Fase 3
estiver fechada.

- **LED RGB (GPIO42, IC embutido de 1 fio, tipo WS2812):** indicador de status físico — verde abaixo de
  50 % da janela de 5 h, âmbar em 70 %, vermelho em 100 %, piscando durante o refresh. Combina com o
  `check_thresholds()` que já existe. Use a API RMT do core ESP32 (`neopixelWrite()` já vem no core 3.x).
  **Respeite o ajuste de brilho** — um LED aceso a noite inteira irrita.
- **ADC de bateria (GPIO9):** a placa aceita LiPo. Mostrar percentual de bateria no header ou em Settings
  faz o gadget virar portátil. Precisa de calibração empírica do divisor resistivo, que o manual não
  especifica.
- **microSD / áudio / microfone:** fora do escopo. Não vale a complexidade para este produto.

---

### Fase 5 — Documentação (1 h) — não é opcional

1. `firmware/es3c28p/README.md` — pinagem, rotação e frequência SPI descobertas na Fase 1, e o **aviso
   explícito sobre a duplicação de código** com a placa antiga.
2. `firmware/REFERENCIA-HARDWARE-LVGL.md` — adicionar uma seção da ES3C28P, ou criar um arquivo irmão.
3. **`README.md` e `README.pt-BR.md`** — as duas placas suportadas, com as instruções de gravação de cada
   uma. **A mudança precisa entrar nos dois arquivos** (§2.5).
4. `tools/gen_mockups.py` — se os mockups do README forem incluir a placa nova, o script precisa de um
   alvo 320×240.
5. PR com **foto da placa rodando o firmware** (exigência do template do repositório).

---

## 7. Código de partida

Tudo abaixo é ponto de partida derivado do manual e da leitura da biblioteca — **não foi executado em
hardware**. Espere ajustar na Fase 1.

### 7.1 `firmware/es3c28p/config.h`

```c
#ifndef CONFIG_H
#define CONFIG_H

// ============================================================
// Claude Usage Stick — LCDWIKI ES3C28P (ESP32-S3, ILI9341V + FT6336G)
// Pinos: manual LCDWIKI ES3C28P/ES3N28P V1.0, secao 4.2
// ============================================================

#define FW_VERSION              "2.3-es3c28p"

// ── Display SPI (ILI9341V) ───────────────────────────────
#define TFT_CS     10
#define TFT_DC     46
#define TFT_SCK    12
#define TFT_MOSI   11
#define TFT_MISO   13     // nao usado na escrita
#define TFT_RST    GFX_NOT_DEFINED   // reset compartilhado com o CHIP_PU do S3
#define TFT_BL     45     // strapping pin (VDD_SPI) — ver nota no README

// Painel nativo 240x320 (retrato); rodamos em paisagem 320x240.
#define PANEL_WIDTH    240
#define PANEL_HEIGHT   320
#define SCREEN_WIDTH   320
#define SCREEN_HEIGHT  240
#define TFT_ROTATION   1          // 1 ou 3 — confirmar no bring-up
#define SPI_FREQ       40000000UL // subir para 80 MHz se estavel

// ── Touch I2C (FT6336G) ──────────────────────────────────
#define TOUCH_SDA  16
#define TOUCH_SCL  15
#define TOUCH_RST  18
#define TOUCH_INT  17
#define TOUCH_ADDR 0x38
// tem que casar com TFT_ROTATION — display e touch giram juntos
#define TOUCH_ROTATION 1

// ── Perifericos extras da placa (Fase 4, opcionais) ──────
#define PIN_RGB_LED    42
#define PIN_BAT_ADC     9

// ── Polling ──────────────────────────────────────────────
#define DEFAULT_POLL_SEC        120
#define MIN_POLL_SEC            30
#define MAX_POLL_SEC            300
#define STATUS_POLL_SEC         300

// ── Seguranca (PIN + AES-256-GCM) ────────────────────────
#define PIN_LEN                 4
#define MAX_PIN_ATTEMPTS        10
#define LOCKOUT_BASE_SEC        60
#define KDF_ROUNDS              10000

// ── Rede / API Claude ────────────────────────────────────
#define WIFI_CONNECT_TIMEOUT_MS 8000
#define API_TIMEOUT_MS          15000
#define MESSAGES_ENDPOINT       "https://api.anthropic.com/v1/messages"
#define ANTHROPIC_VERSION       "2023-06-01"
#define PROBE_MODEL             "claude-haiku-4-5-20251001"
#define STATUS_ENDPOINT         "https://status.claude.com/api/v2/incidents/unresolved.json"

#define NTP_SERVER_1            "pool.ntp.org"
#define NTP_SERVER_2            "time.cloudflare.com"

#define NVS_NAMESPACE           "claude"

#endif // CONFIG_H
```

### 7.2 Pipeline de display (substitui o bloco QSPI+canvas do `setup()`)

```c
#include <Arduino_GFX_Library.h>

Arduino_DataBus *bus = nullptr;
Arduino_GFX     *gfx = nullptr;

// ── no setup(), no lugar do bloco QSPI da placa antiga ──
bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCK, TFT_MOSI, TFT_MISO);
// ips=true: paineis IPS costumam precisar da inversao ligada. Se as cores
// sairem negativas no bring-up, troque para false.
gfx = new Arduino_ILI9341(bus, TFT_RST, TFT_ROTATION, /*ips=*/true);
if (!gfx->begin(SPI_FREQ)) { /* trate a falha */ }
gfx->fillScreen(BLACK);

ledcAttach(TFT_BL, 5000, 8);   // backlight PWM, igual a placa antiga

// ── LVGL com buffers PARCIAIS (nao FULL, como na placa antiga) ──
lv_init();
lv_tick_set_cb([]() -> uint32_t { return millis(); });

// 40 linhas de altura -> 320*40*2 = 25.600 bytes por buffer.
// DRAM interna com capacidade DMA e mais rapida que PSRAM para SPI.
static const uint32_t BUF_PX = SCREEN_WIDTH * 40;
lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(BUF_PX * sizeof(lv_color_t),
                                                  MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(BUF_PX * sizeof(lv_color_t),
                                                  MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
lv_display_t *disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
lv_display_set_flush_cb(disp, disp_flush_cb);
lv_display_set_buffers(disp, buf1, buf2, BUF_PX * sizeof(lv_color_t),
                       LV_DISPLAY_RENDER_MODE_PARTIAL);
```

```c
// ATENCAO: ao contrario do callback da placa antiga, este RESPEITA `area`.
// O da placa antiga ignora `area` porque usa RENDER_MODE_FULL — copiar aquele
// aqui produz lixo na tela.
static void disp_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = lv_area_get_width(area);
  uint32_t h = lv_area_get_height(area);
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
  lv_display_flush_ready(disp);
}
```

**Sobre ordem de bytes:** o `Arduino_ESP32SPI::writePixels` já converte para big-endian ao escrever
(`MSB_32_16_16_SET`), então os pixels nativos da LVGL devem sair com a cor certa **sem** conversão extra.
Se no bring-up as cores saírem trocadas (azul virando vermelho), acrescente
`lv_draw_sw_rgb565_swap(px_map, w * h);` antes do `draw16bitRGBBitmap` — a função existe na LVGL 9.2.

### 7.3 `firmware/es3c28p/touch_ft6336.h`

Driver do FT6336G. A estrutura imita `touch.h` da placa antiga de propósito, para o
`touch_read_cb` do `.ino` continuar igual.

```c
#ifndef TOUCH_FT6336_H
#define TOUCH_FT6336_H

#include <Arduino.h>
#include <Wire.h>

// Registradores FocalTech FT6336
#define FT_REG_NUM_TOUCHES  0x02
#define FT_REG_P1_XH        0x03   // [7:6] event flag, [3:0] X[11:8]
#define FT_REG_VENDOR_ID    0xA8   // deve ler 0x11 (FocalTech)

class FT6336_Touch {
public:
    FT6336_Touch(uint8_t scl, uint8_t sda, uint8_t rst, uint8_t int_pin,
                 uint8_t addr, uint8_t rotation)
        : _scl(scl), _sda(sda), _rst(rst), _int(int_pin), _addr(addr), _rot(rotation) {}

    bool begin() {
        _instance = this;
        pinMode(_rst, OUTPUT);          // reset por hardware: baixo 10ms, depois espera
        digitalWrite(_rst, LOW);  delay(10);
        digitalWrite(_rst, HIGH); delay(300);
        pinMode(_int, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(_int), _isr, FALLING);
        return Wire.begin(_sda, _scl, 400000);
    }

    // Leia isto no bring-up: 0x11 confirma que o barramento e o endereco estao ok.
    uint8_t vendorId() { return _read8(FT_REG_VENDOR_ID); }

    bool touched() { return _update(); }
    void readData(uint16_t *x, uint16_t *y) { *x = _x; *y = _y; }
    void setRotation(uint8_t r) { _rot = r; }

private:
    uint8_t _scl, _sda, _rst, _int, _addr, _rot;
    volatile bool _irq = false;
    uint16_t _x = 0, _y = 0;

    static FT6336_Touch *_instance;
    static void ARDUINO_ISR_ATTR _isr() { if (_instance) _instance->_irq = true; }

    uint8_t _read8(uint8_t reg) {
        Wire.beginTransmission(_addr);
        Wire.write(reg);
        Wire.endTransmission(false);
        Wire.requestFrom(_addr, (uint8_t)1);
        return Wire.available() ? Wire.read() : 0;
    }

    bool _update() {
        // O INT do FT6336 e por evento; se o toque continuar pressionado sem
        // novo evento, o LVGL precisa continuar vendo PRESSED. Por isso lemos
        // o contador de toques sempre, e nao so quando a interrupcao dispara.
        _irq = false;

        Wire.beginTransmission(_addr);
        Wire.write(FT_REG_NUM_TOUCHES);
        if (Wire.endTransmission(false) != 0) return false;
        Wire.requestFrom(_addr, (uint8_t)5);          // 0x02..0x06
        if (Wire.available() < 5) return false;

        uint8_t n  = Wire.read() & 0x0F;              // 0x02
        uint8_t xh = Wire.read();                     // 0x03
        uint8_t xl = Wire.read();                     // 0x04
        uint8_t yh = Wire.read();                     // 0x05
        uint8_t yl = Wire.read();                     // 0x06
        if (n == 0 || n > 2) return false;

        uint16_t rx = (uint16_t)(xh & 0x0F) << 8 | xl;   // 0..239 no painel nativo
        uint16_t ry = (uint16_t)(yh & 0x0F) << 8 | yl;   // 0..319 no painel nativo

        // Painel nativo e 240x320 (retrato). Casar com TFT_ROTATION.
        // Confirmar cada caso no bring-up antes de confiar.
        switch (_rot) {
            case 0: _x = rx;            _y = ry;            break;  // retrato
            case 1: _x = ry;            _y = 239 - rx;      break;  // paisagem
            case 2: _x = 239 - rx;      _y = 319 - ry;      break;  // retrato invertido
            case 3: _x = 319 - ry;      _y = rx;            break;  // paisagem invertida
        }
        return true;
    }
};

// `inline` obrigatorio: isto e uma DEFINICAO em escopo de arquivo dentro de um
// header. Sem ele, o segundo .cpp que incluir este arquivo gera
// "multiple definition of FT6336_Touch::_instance" — erro de LINKAGEM, que
// aparece no fim do build e aponta para o header, nao para quem incluiu.
inline FT6336_Touch *FT6336_Touch::_instance = nullptr;

#endif // TOUCH_FT6336_H
```

> **Diferença de comportamento em relação ao driver antigo.** O `touch.h` do AXS15231B só lê o chip quando
> a interrupção disparou (`if (!_touch_int) return false;`). Aqui a leitura é feita sempre, porque o INT do
> FT6336 é por evento e um dedo parado na tela pararia de gerar interrupções — com a lógica antiga, arrastar
> e segurar (que é como o tileview faz swipe) falharia. Se o consumo de I²C incomodar, limite a taxa por
> `millis()`, não pela interrupção.

### 7.4 `firmware/es3c28p/build.sh`

Copie o `firmware/claude_stick/build.sh` e mude só o cabeçalho e a porta padrão. O FQBN é **idêntico**.
O sketch novo **tem** `lv_conf.h` e `partitions.csv` próprios, então mantenha `PartitionScheme=custom` e
as duas flags `compiler.c/cpp.extra_flags` — sem necessidade da terceira.

O `es3c28p-bringup/`, por outro lado, **não** terá `lv_conf.h` nem `partitions.csv` próprios: copie o
`firmware/bringup/build.sh`, que já usa `PartitionScheme=huge_app` e passa o `-I` **também** em
`compiler.S.extra_flags`. O comentário no topo daquele arquivo explica por quê — leia antes de "simplificar".

### 7.5 `flash-es3c28p.sh` (raiz)

Cópia do `flash.sh` existente, chamando `firmware/es3c28p/build.sh upload "$PORT"`.
Se as duas placas puderem estar conectadas ao mesmo tempo, o `find_port()` que pega o primeiro
`/dev/cu.usbmodem*` vira uma armadilha — vale listar as portas e pedir confirmação.

---

## 8. Diagnóstico: sintoma → causa provável

| sintoma | causa provável |
|---|---|
| tela preta, backlight aceso | rotação/tamanho errados no construtor, ou `begin()` falhou — cheque o retorno |
| tela preta, backlight apagado | GPIO45. É **strapping pin (VDD_SPI)**: garanta que só é configurado como saída **depois** do boot e nunca é puxado durante o reset |
| cores negativas (branco vira preto) | flag `ips` do construtor — inverta `true`/`false` |
| vermelho e azul trocados | ordem de bytes RGB565 — aplique `lv_draw_sw_rgb565_swap()` no flush (§7.2) |
| imagem chuviscada ou com listras | frequência SPI alta demais — desça 80 → 60 → 40 MHz |
| imagem só na metade da tela / lixo | `disp_flush_cb` copiado da placa antiga, ignorando `area` (§ Fase 2) |
| toque espelhado em um eixo | rotação do touch não casa com a do display — ajuste o `switch(_rot)` |
| toque não responde nunca | `vendorId()` != 0x11 → I²C errado; ou o reset do touch (GPIO18) não foi pulsado |
| swipe entre tiles não funciona, mas toque simples sim | o driver está só lendo no evento de INT — ver a nota em §7.3 |
| `unknown opcode or format name 'typedef'` | o `-I` não chegou em `compiler.S.extra_flags`, ou o `#include <stdint.h>` escapou da guarda `__ASSEMBLY__` (§2.2) |
| `cp: .../partitions/.csv: No such file` | `PartitionScheme=custom` num sketch sem `partitions.csv` — use `huge_app` |
| `multiple definition of ..._instance` | faltou o `inline` na definição do membro estático no header |
| texto com caracteres tortos/faltando | acento numa string de UI — as fontes embutidas só têm ASCII + `°` + `•` (§2.4) |
| crash aleatório ao trocar de tela | ponteiro `lv_obj_t*` não zerado em `render_state()`, ou `lv_anim` apontando para objeto deletado |
| a placa não enumera na USB | segure BOOT, aperte e solte RESET, solte BOOT |

---

## 9. Checklist de aceite

Hardware:
- [ ] as três barras R/G/B saem na ordem certa, sem chuvisco, na frequência SPI escolhida
- [ ] rotação escolhida e documentada; Type-C na posição pretendida
- [ ] toque alinhado nos quatro cantos
- [ ] `vendorId()` do FT6336G retorna `0x11`
- [ ] brilho do backlight varia pelos 4 níveis do menu de Settings

Firmware:
- [ ] `firmware/es3c28p/build.sh` compila **sem warnings novos**
- [ ] onboarding completo: token pelo navegador → validação real na API → PIN → dashboard
- [ ] os 4 tiles ficam **inteiramente dentro** de 320×240, sem corte, com swipe funcionando
- [ ] settings roláveis, todas as linhas com ≥ 44 px de toque
- [ ] `curl http://claude-stick.local/window` responde (ou pelo IP, se o mDNS falhar)
- [ ] `python3 tools/token_bridge.py --host <ip>` empurra tokens e eles aparecem na tela
- [ ] histórico sobrevive a um ciclo de energia (LittleFS)
- [ ] múltiplas contas: adicionar, trocar e apagar slot funcionam
- [ ] overlays de limiar (25/50/70/100 %) aparecem inteiros e fecham sem crash
- [ ] PIN errado 10 vezes apaga as credenciais (**teste num device sem token real que importe**)

Repositório:
- [ ] `firmware/claude_stick/` **inalterado** — confirme com `git diff --stat`
- [ ] `firmware/claude_stick/build.sh` ainda compila
- [ ] README em inglês **e** em português atualizados
- [ ] nenhum segredo commitado (`.env`, tokens, `CLAUDE.md`, `.claude/`)
- [ ] PR com foto da placa rodando

---

## 10. O que não fazer

- **Não altere nada em `firmware/claude_stick/`.** É firmware em produção em placas de terceiros.
  `git diff --stat` no fim precisa mostrar essa pasta intocada.
- **Não atualize as versões de arduino-cli, core ESP32, Arduino_GFX ou lvgl** (§2.1).
- **Não replique a rotação manual dentro do `disp_flush_cb`** da placa antiga — o ILI9341 rotaciona por
  hardware. Aquele hack existe por uma limitação específica do AXS15231B.
- **Não edite `logo_assets.h` à mão** — é gerado por `tools/gen_logo_assets.py`.
- **Não mude o layout do struct de histórico** sem magic novo + ramo de migração em `load_history()`.
- **Não "limpe" as chaves NVS antigas** (`blob`, `/hist.bin`) — são o caminho de rollback (§2.3).
- **Não commite `CLAUDE.md`, `.env`, `.mcp.json` nem `.claude/`.**
- **Não confie no `Serial.printf` para depurar** — desenhe o estado na tela.
- **Não deixe o README em português desatualizado** em relação ao inglês.
