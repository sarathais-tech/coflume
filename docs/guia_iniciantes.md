# Guia para iniciantes: montar, instalar, gravar e testar o Coflume

**Versão de referência:** firmware [`v0.9-r4`](../firmware/v0.9-r4/), experimental e **ainda não testada na placa física**.

Este guia é para quem nunca montou o projeto. Cada parte diz de onde veio a informação:

| Marca | Significado |
|---|---|
| **[documentado]** | Está no código ou nos READMEs do projeto |
| **[testado no PC]** | Verificado por testes automáticos ou compilação, sem a placa |
| **[recomendação geral]** | Boa prática comum em eletrônica; **não descreve a montagem real do Coflume** |
| **[a confirmar]** | Informação que falta. Lista completa em [`montagem_pendencias.md`](montagem_pendencias.md) |

> **Segurança:** faça todas as ligações com a placa **desconectada** do USB e de qualquer fonte. Confira duas vezes os fios de alimentação (5 V, 3V3 e GND) antes de ligar.

---

## 1. Visão geral

O Coflume é um cofre educacional. A criança responde perguntas no teclado, vê as perguntas na tela e, ao acertar 2 de 3, o servo abre a porta.

| Parte | O que faz |
|---|---|
| ESP32-S3 | Roda o jogo, a Guardiã IA (uma rede neural pequena) e controla o servo |
| Tela TFT 1,8" | Mostra perguntas, dicas e resultados |
| Teclado 4×4 | Respostas (A, B, C, D), dica (`*`), confirmação (`#`) e menu |
| Servo | Trava ou destrava a porta |
| Notebook (opcional) | Painel USB: Agente Secreto (pistas e música), logs e visualização da Guardiã. Um segundo monitor pode mostrar só a IA |

O quiz funciona **sem** o notebook. O Agente Secreto precisa do painel no notebook.

## 2. Componentes

| Componente | Detalhe | Situação |
|---|---|---|
| Placa ESP32-S3 DevKitC-1 **N16R8** (16 MB de Flash, 8 MB de PSRAM) | Placa usada no projeto | [documentado] |
| Tela TFT 1,8", 128×160, SPI | O firmware usa o driver ST7735 (Adafruit), o mesmo do sketch em uso na placa. Fabricante e versão do módulo: [a confirmar] | [documentado] |
| Teclado matricial 4×4 | Teclas 1–9, 0, A–D, `*` e `#` | [documentado] |
| Servo motor | Ligado ao GPIO 12; 90° = fechado, 0° = aberto. **Modelo e tensão de trabalho: [a confirmar]** | [documentado] |
| Cabo USB **de dados** | Para gravar e para o painel (cabo só de carga não serve) | [documentado] |
| Protoboard e jumpers | Montagem de bancada | [documentado] |
| LEDs e resistores | **Opcionais nesta versão.** O firmware **simula** os LEDs (linhas `SIMULACAO,led_*` na serial) e não usa GPIO para eles | [documentado] |
| Notebook com Chrome ou Edge | Painel USB (Agente Secreto, logs, Guardiã) | [documentado] |
| Caixa de som ligada ao notebook | Música do Agente Secreto; o cofre não tem buzzer | [documentado] |
| Segundo monitor (opcional) | Monitor da IA | [documentado] |
| Fonte de alimentação do servo | **[a confirmar]** (ver seção 3) | — |
| Caixa de MDF e mecanismo da trava | **[a confirmar]** | — |

## 3. Alimentação

**[documentado]** Durante o uso, o ESP32 fica ligado ao notebook por cabo USB de dados. Isso alimenta a placa e liga o painel.

**[a confirmar]** O projeto ainda não registra:
- como o servo é alimentado na montagem real;
- de onde vêm o VCC e o backlight da tela;
- se há fonte externa.

**[recomendação geral]** Enquanto isso não for documentado, siga as regras comuns para servos e módulos TFT:

1. **Não alimente o servo pelo pino 3V3** do ESP32. Servos de hobby costumam trabalhar entre 4,8 V e 6 V e puxam picos de corrente ao se mover. Confira a tensão na folha de dados do seu servo.
2. Se o servo for alimentado por uma **fonte externa**, ligue o **GND da fonte ao GND do ESP32** (GND comum). Sem isso, o sinal do GPIO 12 não tem referência.
3. Se a placa reiniciar quando o servo se mexe, a alimentação está fraca. Use uma fonte separada para o servo. Um capacitor eletrolítico perto do servo também costuma ajudar; o valor depende do servo.
4. **Tela:** muitos módulos ST7735 aceitam VCC de 3,3 V ou 5 V (alguns têm regulador), mas não todos. Leia a serigrafia ou a documentação **do seu módulo**. Os sinais SPI do ESP32 são de 3,3 V.

## 4. Pinagem [documentado]

Pinos usados pelo firmware (constantes no início de `coflume_v09.ino`):

| Componente | Sinal | GPIO do ESP32-S3 |
|---|---|---|
| Tela ST7735 | CS | **14** |
| | RST (às vezes marcado RES) | **16** |
| | DC (às vezes marcado A0 ou RS) | **15** |
| | MOSI (às vezes marcado SDA ou DIN) | **17** |
| | SCLK (às vezes marcado SCL, SCK ou CLK) | **18** |
| | VCC, GND, LED/BL | **[a confirmar]** (ver seção 3) |
| Teclado 4×4 | Linhas 1 a 4 | **8, 9, 10, 11** |
| | Colunas 1 a 4 | **4, 5, 6, 7** |
| Servo | Sinal (PWM LEDC, 50 Hz, pulso de 500 a 2.400 µs) | **12** |
| | Alimentação e GND | **[a confirmar]** (ver seção 3) |
| LEDs | — | **nenhum** (simulados) |

**Mapa do teclado no firmware:**

```
        col1 col2 col3 col4
linha1   1    2    3    A
linha2   4    5    6    B
linha3   7    8    9    C
linha4   *    0    #    D
```

**[a confirmar]** A ordem dos fios do conector do teclado. É comum que os 8 fios venham na ordem linha 1–4 e depois coluna 1–4, mas isso varia entre fabricantes. Se as teclas aparecerem trocadas na seção 9, inverta os fios.

**Não use** estes GPIOs para LEDs reais no futuro:
- 14, que já é o CS da tela;
- 0, 3, 45 e 46, que são de boot;
- 19 e 20, que são do USB;
- 35, 36 e 37, que são da PSRAM octal da N16R8.

## 5. Montagem passo a passo

> **[recomendação geral]** Monte e teste por partes. Assim, se algo falhar, você sabe em qual etapa foi.

1. **Só o ESP32 no USB.** Grave o firmware (seções 6 a 8). Sem tela, o jogo roda e escreve no Monitor Serial.
2. **Tela.** Desligue o USB e ligue CS, RST, DC, MOSI, SCLK, GND, VCC e backlight conforme a seção 4 e o seu módulo. Religue: aparece a tela de início (seção 9).
3. **Teclado.** Desligue o USB e ligue os 8 fios aos GPIOs 8–11 e 4–7. Religue e aperte as teclas: cada uma gera uma linha `KEY:<tecla>` no Monitor Serial.
4. **Servo.** Desligue tudo. Ligue o sinal ao GPIO 12 e a alimentação conforme a seção 3, com GND comum. Ao ligar, o servo vai para 90° (fechado).
5. **Mecanismo e caixa.** Só depois de tudo funcionar na bancada. **[a confirmar]**: o projeto ainda não documenta a trava nem a caixa.

## 6. Instalação dos programas

### 6.1 Arduino IDE

1. Instale o **Arduino IDE 2.x** (arduino.cc).
2. Em **Arquivo → Preferências → URLs adicionais do Gerenciador de Placas**, adicione a URL oficial da Espressif:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
3. Em **Ferramentas → Placa → Gerenciador de Placas**, procure **esp32** (da Espressif Systems) e instale a versão **3.3.12**. Foi a usada para compilar a r4 [testado no PC]; outras 3.x devem funcionar, mas não foram testadas.

### 6.2 Bibliotecas

Em **Ferramentas → Gerenciar Bibliotecas**:

| Biblioteca | Versão usada na compilação da r4 |
|---|---|
| **Keypad** (Mark Stanley e Alexander Brevig) | 3.1.1 |
| **Adafruit ST7735 and ST7789 Library** | 1.12.0 |
| **Adafruit GFX Library** (a IDE oferece junto com a anterior) | 1.13.0 |
| **Adafruit BusIO** (idem) | 1.17.4 |

O firmware **não** usa Wi-Fi, Bluetooth nem internet.

### 6.3 Programas para os testes no PC (opcional)

- **Python 3.10+** e **g++** (no Windows, WSL ou MSYS2).
- **Teste do painel:** `pip install pytest playwright` e depois `python -m playwright install chromium`.

## 7. Configuração da placa [documentado]

Em **Ferramentas**:

| Opção | Valor |
|---|---|
| Placa | **ESP32S3 Dev Module** |
| Flash Size | **16MB (128Mb)** |
| PSRAM | **OPI PSRAM** |
| CPU Frequency | 240MHz (WiFi) |
| USB CDC On Boot | **Enabled** se o cabo estiver na porta marcada **USB**; **Disabled** na porta **UART/COM** |
| Partition Scheme | padrão (a compilação da r4 usou a partição padrão) |

**[a confirmar]** Qual das duas portas USB da DevKitC-1 a montagem usa.

## 8. Gravação

1. Copie a pasta `firmware/v0.9-r4/coflume_v09/` inteira. O nome da pasta precisa ser igual ao do `.ino`, e na pasta não pode haver outro `.ino` com `setup()` e `loop()`.
2. Abra `coflume_v09.ino`. A IDE abre os `.h` como abas.
3. **Não mude** `#define GUARDIA_POLITICA_GRADUAL 0`. A política gradual é experimental e fica desligada.
4. Escolha a porta em **Ferramentas → Porta** e clique em **Carregar**.
5. Se a gravação não começar: segure o botão **BOOT**, aperte e solte **RST**, solte **BOOT** e tente de novo. É o modo de gravação comum das placas ESP32-S3 [recomendação geral].
6. **Feche o Monitor Serial antes de abrir o painel:** a porta USB é uma só.

**Tela com cores trocadas ou faixas na borda:** troque `#define TFT_PAINEL INITR_BLACKTAB` por `INITR_GREENTAB` ou `INITR_REDTAB`. **Imagem de cabeça para baixo:** troque `#define TFT_ROTACAO 1` por `3`.

## 9. Primeiro teste na placa

> A r4 **ainda não foi testada na placa**. Este é o roteiro para o primeiro teste. Anote o resultado de cada item em [`testes/coflume-v0.9-r4.md`](testes/coflume-v0.9-r4.md), seção 3.

Com o Monitor Serial aberto a 115200 baud:

| # | Faça | Esperado |
|---|---|---|
| 1 | Ligue a placa | Tela de início: "COFLUME", "Autoteste: OK 21 vet.", "Politica: fluxo-0.8", "Servo real / LEDs simul." |
| 2 | Leia o Monitor Serial | `COFLUME,versao=v0.9-agente-usb-experimental-r4,banco=banco_v09,perguntas=36,banco_csv_sha256=…` e `GUARDIA,autoteste,…,resultado=OK` |
| 3 | Aperte cada tecla no menu | `KEY:<tecla>` na serial; o menu responde a 1, 2, 3 e A |
| 4 | Jogue um quiz (1, 2 ou 3) acertando 2 ou 3 perguntas | Perguntas legíveis, sem cortes; com 2 ou mais acertos, o servo vai a 0° (aberto); `#` fecha e volta ao menu |
| 5 | Jogue errando 2 ou 3 | O servo continua fechado |
| 6 | Deixe acabar os 30 s numa pergunta | A tela mostra "Tempo esgotado!", e a pergunta conta como erro |

Depois disso siga os testes com o painel: [`guia_de_uso.md`](guia_de_uso.md) e o roteiro completo em [`testes/coflume-v0.9-r4.md`](testes/coflume-v0.9-r4.md).

## 10. Testes automatizados no PC

A partir da raiz do repositório `coflume`:

```bash
python3 -m pytest -q firmware/v0.9-r4/testes                   # firmware r4 com imitações de hardware
cd firmware/v0.9-r4/painel && python3 -m pytest -q testes      # painel, rede neural e dois monitores (Chromium)
```

E no repositório `coflume-guardian`, com o ambiente da seção "Início rápido" do README dele: `pytest`.

Os testes no PC **não substituem** o teste na placa: eles usam imitações de teclado, tela, servo e porta serial.

## 11. Problemas comuns

| Sintoma | O que verificar |
|---|---|
| A placa não aparece em Porta | Cabo de dados (não só de carga); trocar de porta USB da placa; driver da porta UART no Windows [recomendação geral] |
| Tela branca | Fios CS, DC, RST, MOSI e SCLK; VCC e backlight do módulo; `TFT_PAINEL` |
| Teclas trocadas | Ordem dos fios do teclado (seção 4) |
| Placa reinicia quando o servo mexe | Alimentação do servo (seção 3) |
| "SERVO COM FALHA (GPIO 12)" no início | O LEDC não iniciou no GPIO 12: confira a gravação e a configuração da placa. O quiz continua; o Agente Secreto não inicia |
| Painel não conecta | Feche o Monitor Serial da IDE; use Chrome ou Edge; aguarde "COFRE PRONTO" |
