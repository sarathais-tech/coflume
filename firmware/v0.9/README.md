# Coflume v0.9 EXPERIMENTAL — hardware físico (ESP32-S3 + ST7735), quiz com a Guardiã IA e Agente Secreto

> **Versão experimental. Base: o sketch que funciona na placa.** Revisão `v0.9-agente-usb-experimental-r2`.
>
> - **Guardiã IA:** a MLP é a mesma da v0.8, `mlp8-sint-20261101-e6a4cf00`, treinada com **dados sintéticos**. Não foi validada com crianças reais, não tem eficácia educacional comprovada e não é versão de produção.
> - **Servo real** no GPIO 12 (LEDC). **LEDs simulados** no monitor serial.
> - **As versões anteriores não mudam:** a v0.7 ([`simulations/wokwi/v0.7`](../../simulations/wokwi/v0.7/)) e a v0.8 ([`simulations/wokwi/v0.8`](../../simulations/wokwi/v0.8/)) continuam intactas.

## 1. Modalidades

| Tecla no menu | Modalidade | Quem decide a abertura |
|---|---|---|
| **1**, **2**, **3** | Quiz com a Guardiã IA (Explorador, Desafiante, Mestre): 3 perguntas, 30 s cada, dica com `*`, confirmação com `#` | Validador do jogo: **2 ou mais acertos** |
| **A** | **Agente Secreto**: senha de 4 dígitos digitada no teclado físico, 90 s, pistas e áudio no painel USB do notebook. **Não usa a Guardiã.** | ESP32: **senha correta** |
| **#** | Volta ao menu nas telas de resultado | — |

A Guardiã só estima P(acerto) para escolher o nível das perguntas 2 e 3 do quiz. **Ela nunca aciona o servo.** O painel USB também não tem como abrir o servo: ele não tem campo de senha nem comando de abertura.

## 2. Hardware e pinagem

| Componente | Pino do ESP32-S3 DevKitC-1 N16R8 |
|---|---|
| ST7735 1,8": CS / RST / DC / MOSI / SCLK | **14 / 16 / 15 / 17 / 18** |
| Teclado 4×4: linhas / colunas | **8, 9, 10, 11 / 4, 5, 6, 7** |
| **Servo** | **GPIO 12**, LEDC 50 Hz, 14 bits, pulso de 500 a 2.400 µs; 90° fechado, 0° aberto |
| LED vermelho, LED verde | **simulados**: `SIMULACAO,led_*` no monitor serial |
| Painel do Agente Secreto | Cabo USB de dados até o notebook (115200 baud) |

Para os LEDs reais no futuro, não use estes GPIOs:
- 14, que é o CS da tela;
- 0, 3, 45 e 46, que são pinos de boot;
- 19 e 20, que são do USB;
- 35, 36 e 37, que são da PSRAM octal.

## 3. Arquivos

```
firmware/v0.9/
  coflume_v09/                        ← abrir no Arduino IDE
    coflume_v09.ino                   jogo: menu, quiz, servo real, LEDs simulados, diagnóstico
    coflume_agente.h                  Agente Secreto: protocolo COFLUME_AGENT/1, senha, 90 s
    coflume_tela.h                    layout ST7735 160x128 (quebra por palavra)
    guardia_modelo.h  guardia_runtime.h  guardia_autoteste.h  coflume_banco.h  coflume_politica.h
                                      ← Guardiã: cópias byte a byte da v0.8
  painel/
    Coflume_Agente_Secreto_USB.html   painel do notebook (Chrome ou Edge): pistas, relógio, áudio e logs da Guardiã
    LEIA_PRIMEIRO.txt                 instruções do apresentador
  docs/                               prévias das telas e revisão da Adaptação Gradual v1
  ferramentas/renderizar_telas.py     gera as imagens das telas a partir dos testes
  testes/                             testes no PC (firmware e painel)
```

Nenhuma música vem no pacote. O painel continua tocando o arquivo de áudio que o apresentador escolher (seção 7).

### Hashes (SHA-256) para conferir a pasta da placa

| Arquivo | SHA-256 |
|---|---|
| `guardia_modelo.h` | `9a98304baaf92bdc508a94a4861bf73bbbd4267da8ed34532f63e251693bc42e` |
| `guardia_runtime.h` | `79b3d9a98eccc25ec76fcc383a53a73539ca5239971ac25ef0d7daa36eb112c0` |
| `guardia_autoteste.h` | `009c871b93f87144a63b96a0307e6f0e64acc58a54dde9575685d40b447ca4a8` |
| `coflume_banco.h` | `d08c89946dbac8a3a7d42a53ab180e5b91f2cf30097a3b5d6362edaf8b1666a8` |
| `coflume_politica.h` | `271d997f6e975384225dd08fe67b76bdc6dab4dfb0dcbb1e03e396f33bef402f` |
| `coflume_tela.h` | `6ec40d84a707f333a70e86bd3802b2f32ea18106bb9a137f4e1a0853f9b4d06e` |

No Windows: `certutil -hashfile guardia_modelo.h SHA256`. No macOS ou Linux: `shasum -a 256 *.h`.

## 4. Compilar e gravar (Arduino IDE 2.x)

1. Abra `firmware/v0.9/coflume_v09/coflume_v09.ino`.
2. **Bibliotecas:**
   - `Keypad` (Mark Stanley e Alexander Brevig)
   - `Adafruit GFX Library`
   - `Adafruit ST7735 and ST7789 Library`, que instala também a `Adafruit BusIO`
3. **Placa:** core `esp32` da Espressif 3.x, com a mesma configuração que já funciona na placa.

   | Ferramentas | Valor |
   |---|---|
   | Placa | ESP32S3 Dev Module |
   | Flash Size | 16MB (128Mb) |
   | PSRAM | OPI PSRAM |
   | CPU Frequency | 240MHz (WiFi) |
   | USB CDC On Boot | **Enabled** se o cabo estiver na porta "USB"; **Disabled** na porta "UART/COM" |

4. Grave. **Para usar o painel, feche o Monitor Serial da IDE**: a porta USB é uma só.

| Sintoma na tela | Ajuste no início do `.ino` |
|---|---|
| Vermelho e azul trocados ou faixas na borda | `#define TFT_PAINEL INITR_GREENTAB` (ou `INITR_REDTAB`) |
| Imagem de cabeça para baixo | `#define TFT_ROTACAO 3` |

## 5. O que mudou nesta revisão (r2), em relação ao sketch da placa

Todo o funcionamento do sketch da placa foi preservado: menu, quiz, Guardiã, Agente Secreto, comandos e mensagens `AG`, tempos de 90 s e de 5 s, senha, servo e áudio. Só três mudanças foram aplicadas, as aprovadas:

| # | Mudança | Onde |
|---|---|---|
| 1 | A linha `SERVO,inicializacao=...` sai **depois** da espera pelo USB (antes podia se perder com USB CDC) | `setup()` |
| 2 | **Aviso de falha do servo** na tela de boot ("SERVO COM FALHA (GPIO 12)") e no menu ("A Agente: SERVO C/ FALHA"); a linha `GUARDIA,estado` passa a ter `servo=OK/FALHA` | `telaBoot()`, `menu()` |
| 3 | **Aviso de painel ausente ou desconectado**: "PAINEL USB AUSENTE" quando as pistas não são confirmadas em 5 s (`PANEL_TIMEOUT`), e "CONEXAO PERDIDA" quando o painel para de responder na missão (`DISCONNECTED`). As outras telas não mudam | `agente::tela()` |
| 4 | **Logs da Guardiã no painel**: o diagnóstico de boot (`GUARDIA,modelo`, `autoteste`, `latencia_informativa`, `estado`) fica guardado e é reenviado quando o painel conecta (`AG,HELLO`), logo depois das respostas `AG` | `registrarDiag()`, `reenviarDiagBoot()` |

As linhas reenviadas são logs comuns, sem prefixo `AG`: o protocolo não muda. O painel ganhou a área recolhível **"Logs da Guardia IA"** (seção 7).

## 6. Protocolo COFLUME_AGENT/1 (inalterado)

O ESP32 é a autoridade da partida. Linhas de texto a 115200 baud, terminadas em `\n`.

| Sentido | Mensagem | Significado |
|---|---|---|
| painel → ESP32 | `AG,HELLO` | Identificação, repetida a cada 1 s até a resposta |
| ESP32 → painel | `AG,HELLO,1,<servo_ok>` | Protocolo 1; servo pronto (1/0) |
| painel → ESP32 | `AG,START` | Nova missão (só com o ESP32 no menu ou no Agente; durante o quiz responde `AG,ERROR,BUSY`) |
| ESP32 → painel | `AG,PREP,<sessao>,<senha>` | Prepara as pistas da sessão |
| painel → ESP32 | `AG,READY,<sessao>` | Pistas na tela: começam os 90 s |
| painel → ESP32 | `AG,PING,<sessao>` | A cada 1 s; 5 s sem PING encerram com `DISCONNECTED` |
| painel → ESP32 | `AG,CANCEL,<sessao>` | Encerra a missão |
| ESP32 → painel | `AG,STATE,<sessao>,<IDLE/PREP/PLAYING/WIN/LOSE>,<ms_restantes>,<tentativas>` | Estado, a cada 250 ms na missão |
| ESP32 → painel | `AG,EVENT,<sessao>,<CORRECT/WRONG/INCOMPLETE/TIMEOUT/CANCELLED/PANEL_TIMEOUT/DISCONNECTED>` | Eventos |
| ESP32 → painel | `AG,ERROR,<BUSY/SERVO>` | Pedido recusado |

As linhas sem o prefixo `AG,` são logs. O painel ignora todas elas, exceto `GUARDIA,...`, que vão para a área de logs.

**Observação:** `AG,PREP` leva a senha em texto pela USB, porque o painel precisa dos dígitos para gerar as expressões. Para o evento, o risco é baixo, mas fica registrado.

## 7. Painel USB e logs da Guardiã

As instruções completas do apresentador estão em [`painel/LEIA_PRIMEIRO.txt`](painel/LEIA_PRIMEIRO.txt).

- **Uso:**
  - abra o HTML no Chrome ou Edge;
  - clique em **CONECTAR ESP32** uma vez;
  - escolha a música em "Musica e volume";
  - a cada agente, aperte **A** no teclado do cofre.
- **Logs da Guardia IA** (área recolhível, fechada por padrão, abaixo de "Musica e volume"):
  - mostra as linhas `GUARDIA,...`: o diagnóstico de boot (reenviado ao conectar) e, nos quizzes, cada `GUARDIA,decisao` com `p0`, `p1` e `p2`, mais `features`, `resposta` e `fim`;
  - **SALVAR LOGS (.TXT)** baixa as linhas com data e hora; **LIMPAR** apaga a lista;
  - guarda até 5.000 linhas e mostra as 300 mais recentes;
  - só lê: não envia nada ao ESP32 e não altera pistas, relógio, áudio ou protocolo.
- **Prévia:** [`docs/painel_logs_guardia.png`](docs/painel_logs_guardia.png).

## 8. Telas

Prévias geradas com a fonte real da Adafruit GFX a partir das operações de desenho do firmware. Mostram posição e tamanho do texto; não mostram cores reais nem contraste do painel físico.

- [`docs/telas_v09.png`](docs/telas_v09.png): menu, boot, as 13 perguntas, feedbacks, vitória, derrota e banco incompleto.
- [`docs/telas_v09_agente_secreto.png`](docs/telas_v09_agente_secreto.png): menu, aguardando o computador, painel ausente, missão em andamento, cumprida, encerrada, conexão perdida e as duas telas de falha do servo.
- [`docs/telas_v09_casos_limite.png`](docs/telas_v09_casos_limite.png): dica mais longa, alternativa em 2 linhas, acento e tempo esgotado.

## 9. Testes automatizados (PC)

```bash
python3 -m pytest -q firmware/v0.9/testes
```

Requisitos: Python 3, pytest e g++.

Opcionais:
- `GFX_GLCDFONT=<glcdfont.c>` com Pillow: gera as imagens das telas;
- pacote `playwright` com Chromium: teste do painel;
- `COFLUME_PAINEL_ORIGINAL=<html da placa>`: compara o painel novo com o original.

| Teste | Verifica |
|---|---|
| `test_nucleo_da_guardia_identico_ao_da_v08`, `test_modelo_e_banco_conferem_com_a_origem` | Modelo, runtime, política, banco e autoteste idênticos à v0.8 e aos hashes de origem |
| `test_400_vetores_politica_e_referencia_python` | 400 vetores da bancada (erro < 1e-5) e 1.224 partidas iguais à referência em Python |
| `test_partida_e_layout_st7735[1]` e `[0]` | Sketch real com imitações de Arduino, teclado, ST7735, LEDC e porta serial (detalhes abaixo) |
| `test_servo_com_falha_avisa_na_tela` | `ledcAttach` falha: avisos no boot e no menu, `AG,ERROR,SERVO`, nenhum duty escrito, quiz continua |
| `test_configuracao_publicada` | `GUARDIA_ADAPTATIVA 1`, pinagem, servo no GPIO 12, mensagem do servo depois da espera USB |
| `test_servo_real_so_pelo_validador_ou_pela_senha` | LEDC só no `setup` e em `servoAngulo()`; `abrir()` só depois de `desbloqueia(acertos)` ou da senha digitada correta; nenhum comando do painel abre; Guardiã sem acesso ao servo; LEDs sem GPIO |
| `test_protocolo_agente_preservado` | Mensagens, comandos e tempos do `COFLUME_AGENT/1` |
| `test_sem_dependencia_de_internet` | Sem Wi-Fi, HTTP, BLE ou MQTT |
| `test_v07_intacta`, `test_v08_intacta` | Hashes de todos os arquivos das duas versões |
| `test_painel_protocolo_pistas_e_logs` | Painel no Chromium com porta serial simulada (detalhes abaixo) |
| `test_painel_igual_ao_original_no_protocolo_ag` | O mesmo roteiro AG no painel da placa e no atualizado: **mesmos comandos enviados e mesmos textos** |

O `test_partida_e_layout_st7735` cobre:
- quiz completo e timeout;
- desbloqueio com 3, 2, 1 e 0 acertos, conferido pelo duty do servo;
- 50 partidas com a escolha da MLP conferida;
- Agente Secreto:
  - HELLO com reenvio dos logs;
  - painel ausente;
  - `READY` de outra sessão ignorado;
  - máscara, `INCOMPLETE`, senha errada, `*`, senha certa abrindo o servo e `#` fechando;
  - `START` recusado durante o quiz e quiz normal depois do Agente;
  - `START` pelo painel e senha nova;
  - 90 s esgotados, conexão perdida, `D` e `CANCEL`;
  - ruído na serial;
- **81 telas** sem cortes nem sobreposições.

O `test_painel_protocolo_pistas_e_logs` cobre:
- o painel só envia `HELLO`, `READY` e `PING`;
- as 4 expressões resultam nos dígitos da senha, na ordem;
- só as linhas `GUARDIA` entram nos logs;
- salvar, limpar e o limite de 5.000 linhas;
- nenhum erro de JavaScript.

**Resultados (07/11/2026): 14 de 14 aprovados.**
- **Firmware no PC:** 1.723 verificações no modo adaptativo, 1.388 no nível fixo e 9 com falha do servo, sem falhas.
- **Política e referência:** 19.465 verificações, sem falhas.
- **Compilação ESP32-S3:** Arduino-ESP32 3.3.12, Adafruit ST7735 1.12.0, GFX 1.13.0, BusIO 1.17.4 e Keypad 3.1.1, com `FlashSize=16M,PSRAM=opi` e `--warnings all`. Nenhum aviso. **375.851 bytes (28%)** de programa e **26.268 bytes (8%)** de RAM global.

## 10. Roteiro do teste físico desta revisão

| # | Caso | Esperado |
|---|---|---|
| 1 | Ligar com o servo conectado | Boot com "Autoteste: OK 21 vet." e "Servo real / LEDs simul."; menu com as duas modalidades |
| 2 | Conectar o painel | "COFRE PRONTO"; em "Logs da Guardia IA", a linha `reenvio_diagnostico_boot` e o autoteste |
| 3 | Quiz (1, 2 ou 3) com o painel conectado | Linhas `GUARDIA,decisao` aparecem nos logs do painel; abre com 2 ou mais acertos |
| 4 | **A** com o painel conectado | Pistas, áudio e contagem iniciam sozinhos; senha correta abre; `#` fecha e volta |
| 5 | **A** com o painel fechado | Depois de 5 s, "PAINEL USB AUSENTE"; `#` volta ao menu |
| 6 | Fechar a aba do painel durante a missão | Depois de 5 s, "CONEXAO PERDIDA"; o servo continua fechado |
| 7 | Salvar logs | Arquivo `coflume_logs_guardia_<data>_<hora>.txt` com as linhas GUARDIA |
| 8 | (Opcional) servo desconectado do LEDC | Se `ledcAttach` falhar: avisos no boot e no menu; **A** não inicia |

## 11. Limitações

- **Experimental.** O modelo usa dados sintéticos, o teste lacrado não foi aberto e a bancada de latência da fase D está pendente.
- **Porta USB única.** Com o painel conectado, o Monitor Serial da IDE não abre. Os logs da Guardiã ficam no painel.
- **Comunicação do painel.** Aba em segundo plano ou computador suspenso podem derrubar o `PING`, e a missão é encerrada em 5 s.
- **Testes sem hardware.** Os testes no PC usam imitações de hardware. O teste físico desta revisão, com teclado, tela, áudio e servo, está pendente.
- **Adaptação Gradual v1 não integrada.** A revisão está em [`docs/revisao_adaptacao_gradual_v1.md`](docs/revisao_adaptacao_gradual_v1.md).
