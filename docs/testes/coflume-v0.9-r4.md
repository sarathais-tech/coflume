# Testes: Coflume v0.9-r4 EXPERIMENTAL

**Versão:** `v0.9-agente-usb-experimental-r4` · **Pasta:** [`firmware/v0.9-r4/`](../../firmware/v0.9-r4/) · **Data:** 08/11/2026

> **A r4 não foi validada na placa física.** A seção 1 vem de testes no PC, com imitações de hardware. A seção 2 reúne testes manuais relatados: em simulação e, para a **r3** com a política gradual desligada, na placa física. O modelo da Guardiã foi treinado com dados **sintéticos**: nada aqui comprova eficácia educacional. O teste lacrado (fase C) não foi aberto, e a latência na placa física continua pendente.

## 1. Testes automatizados (PC)

Ambiente: Linux, Python 3, pytest, g++ (C++17, `-Wall -Wextra -Werror`) e Chromium (Playwright) com uma porta serial falsa. Executados em 08/11/2026.

### 1.1 coflume

| Suíte | Comando (da raiz do repositório) | Resultado |
|---|---|---|
| **Firmware r4** | `python3 -m pytest -q firmware/v0.9-r4/testes` | **26 de 26** |
| **Painel r4** (quadro, rede, monitor da IA) | `cd firmware/v0.9-r4/painel && python3 -m pytest -q testes` | **35 de 35** |
| r3 (preservada) | `python3 -m pytest -q firmware/v0.9-r3/testes` | 21 aprovados, 1 pulado¹ |
| r2 (preservada) | `python3 -m pytest -q firmware/v0.9/testes` | 13 aprovados, 1 pulado¹ |
| Experimento da política gradual | `python3 -m pytest -q firmware/experimentos/politica_gradual/testes` | 2 aprovados, 1 pulado² |
| v0.8 Wokwi (preservada) | `COFLUME_GUARDIAN_AI=<clone> python3 -m pytest -q simulations/wokwi/v0.8/testes` | 15 de 15 |

¹ Pulado: a comparação com o painel original da placa, que precisa de `COFLUME_PAINEL_ORIGINAL`. Na r4 essa comparação foi executada com a variável definida e passou.
² Pulado: regenerar a referência, que precisa de `COFLUME_GUARDIAN_AI`.

O que a suíte da **r4** cobre:

- **Firmware (26):**
  - sketch real com imitações de teclado, ST7735, LEDC e serial: quiz completo, timeout e desbloqueio com 3, 2, 1 e 0 acertos (pelo duty do servo);
  - **196 telas** do banco de 36 perguntas, sem cortes nem sobreposições, com a Guardiã ligada e desligada;
  - Agente Secreto completo: protocolo, senha, 90 s, painel ausente, conexão perdida, `CANCEL` e ruído;
  - falha do servo;
  - política gradual **ligada**: 150 partidas, bancos com lacuna e recusa explícita. Com a opção **desligada** (padrão), o jogo usa o fluxo-0.8;
  - 400 vetores e 1.224 partidas iguais à referência Python;
  - versão e hash do banco: `BANCO_FIRMWARE.json` e `.h` conferem com o CSV;
  - preservação: banco_v08 e `ORIGEM_GUARDIA.json` intactos; v0.7, v0.8, r2 e r3 sem alteração (hashes).
- **Painel (35):**
  - **protocolo AG idêntico** ao painel r3 e ao painel original da placa;
  - quadro da Guardiã ao vivo e em reprodução;
  - campos ausentes mostrados como "indisponível"; empate visual com 1 casa decimal;
  - **rede reproduzida:** 400 de 400 vetores; **850 de 850** previsões do firmware r4 simulado conferidas em cada política (|Δ| ≤ 6,4×10⁻⁷, tolerância 1,9×10⁻⁶);
  - animação sem acúmulo, sem atrasar o protocolo (READY em < 300 ms) e parada no Agente Secreto;
  - **monitor da IA:**
    - abertura no meio da partida, sincronização, fechar e reabrir, F5;
    - Agente Secreto: o monitor não recebe senha nem pistas;
    - reprodução de log identificada como demonstração;
    - perda da USB e do painel principal sem mostrar dados como atuais;
    - protocolo AG igual com o monitor aberto;
    - **rede grande:** sem rolagem e com o resultado da partida visível em 1920×1080 e 1366×768; desenho com ≈ 2/3 da largura útil e círculos maiores; 13 nós e 112 conexões reais, sem rótulos no desenho e com as 13 entradas nos detalhes técnicos;
    - **brilho:** halo proporcional à ativação reproduzida; neurônio com ativação zero sem halo.

  Os testes automatizados do monitor rodam no Chromium do PC. A animação no navegador da InovaTech, com o segundo monitor real, ainda não foi testada (ver testes físicos pendentes).

**Compilação para ESP32-S3** (arduino-cli, Arduino-ESP32 3.3.12, `FlashSize=16M,PSRAM=opi`, `--warnings all`): **sem avisos**.

| Política gradual | Programa | RAM global |
|---|---|---|
| desligada (padrão) | 378.487 bytes (28%) | 26.292 bytes (8%) |
| ligada | 380.671 bytes (29%) | 26.340 bytes (8%) |

Compilar não é o mesmo que rodar na placa.

### 1.2 coflume-guardian-ai

`pytest` → **375 aprovados e 2 pulados.** Os dois pulados (`test_mlp_keras`, `test_treino` da MLP) precisam do TensorFlow, que roda no CI. A suíte inclui:
- `tests/test_banco_v09.py` (7): 36 aptas na ST7735, viabilidade nas duas políticas, históricas preservadas, letras equilibradas e a substituição da pergunta da água;
- `tests/test_politica_gradual.py` (30).

O teste lacrado continua fechado: `resultados/sinteticos/teste_v2/teste.json` não existe.


### 1.3 Verificação durante a publicação no GitHub (08/11/2026)

Rodada posterior aos resultados históricos das seções 1.1 e 1.2, sem substituí-los:

| Escopo | Resultado nesta rodada |
|---|---|
| `firmware/v0.9-r4/testes` | **24 aprovados, 2 pulados**; os dois testes dependentes de navegador não foram executados |
| `tests/test_politica_gradual.py` e `tests/test_banco_v09.py`, no conteúdo do `coflume-guardian` acrescido dos dois lotes | **37 aprovados** |
| `firmware/v0.9-r4/painel/testes` | **3 aprovados, 32 pulados** por ausência inicial do Playwright; a tentativa posterior de instalar o Chromium falhou no download. Os 35 testes não foram repetidos integralmente |

Os vídeos manuais da seção 2.2 complementam essa rodada; não alteram os resultados dos testes pulados nem comprovam validação física.

## 2. Testes relatados (manuais)

| Data | Ambiente | Versão | O que foi relatado | Registro |
|---|---|---|---|---|
| 28/10/2026 | **Wokwi (simulação)** | v0.7 | Níveis, respostas, abertura do servo, LEDs e reinício aprovados | [`coflume-v0.7-wokwi.md`](coflume-v0.7-wokwi.md) |
| 01/11/2026 | **Wokwi (simulação)** | Bancada da MLP (coflume-guardian-ai) | `APROVADO_FUNCIONAL` (equivalência); a latência simulada não vale | `docs/viabilidade_embarcada.md` do coflume-guardian-ai |
| 02/11/2026 | **Wokwi (simulação)** | v0.8 | 7 casos aprovados (desbloqueio, erros com dica, timeouts, reinício, fallback, autoteste, decisões da MLP) | [`coflume-v0.8-wokwi.md`](coflume-v0.8-wokwi.md), seção 2.3 |
| 07/11/2026 | **Placa física** | Sketch da pasta da placa (`coflume_v09_AGENTE_copy_20261107181940`), anterior à r2 | A responsável indicou que essa pasta é o código atual em uso na placa, com o quiz da Guardiã e o Agente Secreto USB. **Não há registro de casos, resultados ou logs.** A r2 foi construída sobre esse sketch | Conversa de desenvolvimento; sem relatório |
| Relato de 08/11/2026 (data da execução não informada) | **Placa física** | **r3**, com `GUARDIA_POLITICA_GRADUAL 0` (padrão) | Ver seção 2.1 | Conversa da responsável com o ChatGPT; os logs brutos não estão no repositório |

### 2.1 Testes físicos da r3 relatados pela responsável

**Fonte:** relato da responsável pelo projeto (Sara Thais) em 08/11/2026, a partir dos testes registrados na conversa dela com o ChatGPT.
- **Versão testada:** r3 (`v0.9-agente-usb-experimental-r3`), com a política gradual **desligada**: fluxo-0.8 e banco_v08, de 13 perguntas.
- **Data da execução:** não informada.
- **Logs brutos:** não foram anexados ao repositório. Os valores abaixo foram transcritos do relato.

**Evidência por log** (valores das linhas de log citados no relato):

| Sessão | Caso | Valores relatados | Comportamento esperado na r3 |
|---|---|---|---|
| `5F9C20D7` | Quiz com 2 acertos em 3 | `acertos` 2/3, `desbloqueio=SIM`, `SERVO,angulo=0`; depois `#`, `SERVO,angulo=90` e `MENU` | Abrir com 2 ou mais acertos; `#` fecha e volta ao menu |
| `55DC1E88` | Quiz com 0 acertos e tempos esgotados | `acertos` 0/3, tempos esgotados, `desbloqueio=NAO` e `LOSE` | Tempo esgotado conta como erro; cofre continua fechado |

**Confirmação manual** (observada pela responsável, sem log citado):

| Caso | Relato |
|---|---|
| Agente Secreto ao apertar **A** | Pistas, música e cronômetro iniciam automaticamente |
| Senha correta e retorno ao menu | A senha correta destrava; `#` trava ao voltar ao menu |

**O que estes testes não cobrem:**
- **Versões e configurações:** a r4, o **banco de 36 perguntas**, a **política gradual ligada** e a r2, que não foi relatada separadamente.
- **Painel novo:** a **visualização ao vivo da rede** e o **monitor da IA** (dois monitores).
- **Outros casos da r3** fora dos itens acima: dica, 1 acerto, painel ausente, conexão perdida, falha do servo, salvar logs.

Como a r4 mantém o mesmo quiz, servo e Agente Secreto da r3, esses resultados são um **precedente** para os testes F5–F8 e F13 da seção 3, mas não os substituem.

O pacote Agente USB anterior registrava como pendente o "teste físico com teclado, tela, áudio e servo". Para a r3, esse teste foi feito em parte, nos casos acima.


### 2.2 Demonstração manual da r4 sem componentes (08/11/2026)

**Execução:** Sara Thais, em computador pessoal, sem ESP32 conectado. **Revisão:** observação de quadros extraídos dos vídeos enviados na conversa de desenvolvimento. Esta revisão é visual e amostral; não equivale à execução das suítes automatizadas nem valida o hardware.

**Material aberto:** painel HTML da pasta `firmware/v0.9-r4/painel/` do pacote de consolidação r4, em Microsoft Edge, com painel principal e monitor da IA lado a lado. Resolução informada: **1920×1080**, escala do Windows **125%**, zoom do navegador **100%**. O segundo monitor físico do evento não foi utilizado neste registro.

Os três arquivos de demonstração foram reproduzidos. Seus dados vêm de logs gerados em simulação no PC; as referências a previsões do ESP32 na interface não significam que uma placa física esteve conectada neste teste.

| Caso | Evidência observada | Resultado da revisão |
|---|---|---|
| M1 — `demo_gradual.txt` | As duas janelas exibem Desafiante ≈ **80,1%** e Mestre ≈ **60,0%**. A política mantém **Desafiante** pela proteção após erro. O monitor mostra **1 acerto em 3 / Cofre fechado**, reconhece o Agente Secreto e informa que essa modalidade não usa a Guardiã. A interface informa **5 de 5** previsões dentro da tolerância | Resultados exibidos coerentes nesse cenário; sincronização exata dos quadros da animação não demonstrada |
| M2 — `demo_recusa.txt` | O monitor avisa que nenhum nível compatível tem pergunta disponível, mostra **Recusada**, **Partida interrompida** e probabilidades **Não avaliado**. Ao final a rede está apagada. A interface informa **3 de 3** previsões anteriores dentro da tolerância | Estado de recusa visível no monitor; explicação na área da rede do painel principal ficou insuficiente nos quadros observados |
| M3 — `demo_fluxo08.txt` | Há passagem pelo Agente Secreto e retorno ao Quiz. Na última pergunta observada, Explorador ≈ **72,7%** é escolhido; Desafiante ≈ **61,4%** também aparece avaliado. A interface informa **12 de 12** previsões dentro da tolerância | Valores e escolha coerentes entre as janelas; destaque final das redes difere conforme descrito abaixo |

**Evidências:** vídeos enviados por Sara na conversa; não incorporados ao repositório. Os nomes abaixo permitem localizar os registros originais, sem depender de links privados:

- M1: `20261108-1712-45.8350317.mp4` (monitor isolado) e `20261108-1911-45.2514147.mp4` (duas janelas).
- M2: `20261108-1915-56.9651958.mp4`.
- M3: `20261108-1921-12.3734182.mp4`.

#### Pendências observadas nesta revisão

1. **Recusa no painel principal:** tornar explícita a razão da recusa também na área da rede, que aparece vazia ou aguardando dados nos quadros observados. O monitor já apresenta o aviso. Confirmar o comportamento após eventual correção.
2. **Avaliada × escolhida:** em M3, a rede do painel principal termina na última opção avaliada (**Desafiante, 61,4%**), enquanto a rede do monitor destaca a escolhida (**Explorador, 72,7%**). A lista de avaliações e a decisão no painel principal concordam com a escolha. Clarificar essa distinção para evitar confusão; não foi observada divergência numérica para a mesma opção.
3. **Animações:** em alguns trechos, as janelas mostram etapas diferentes. Os resultados finais conferem nos casos descritos, mas não se declara sincronização visual quadro a quadro.
4. **Layout no ambiente pessoal:** houve rolagem com escala de 125% e também nas janelas lado a lado. A responsável decidiu adiar o ajuste até verificar o monitor da apresentação. Não declarar F18 aprovado por esses vídeos.

**Limites:** não foram verificados USB real, áudio, teclado, tela ST7735, servo, latência na placa nem estabilidade prolongada. F1–F24 continuam pendentes nas condições especificadas; M1–M3 fornecem evidência parcial para F23, sem confirmar execução no equipamento do evento. Os vídeos não substituem os 35 testes automatizados do painel.

## 3. Testes físicos pendentes (r4)

Preencha as colunas de resultado ao executar. Guarde o arquivo salvo pelo painel (SALVAR LOGS) e fotos ou vídeo. Um teste só pode ser marcado como aprovado depois de executado e registrado.

### 3.1 Placa e tela

| # | Caso | Esperado | Resultado | Data / quem |
|---|---|---|---|---|
| F1 | Ligar a placa com a r4 | Tela de início com "Autoteste: OK 21 vet.", "Politica: fluxo-0.8" e "Servo real / LEDs simul." | | |
| F2 | Monitor Serial | `COFLUME,versao=…-r4,banco=banco_v09,perguntas=36,banco_csv_sha256=…` igual ao `BANCO_FIRMWARE.json` | | |
| F3 | Legibilidade das 36 perguntas na tela real | Sem cortes; leitura confortável a distância de uso; cores corretas (`TFT_PAINEL`) | | |
| F4 | Todas as teclas | `KEY:<tecla>` correto para as 16 teclas | | |

### 3.2 Quiz e servo

| # | Caso | Esperado | Resultado | Data / quem |
|---|---|---|---|---|
| F5 | Quiz com 3 acertos, nos níveis 1, 2 e 3 | Servo abre; `#` fecha | | |
| F6 | Quiz com 2 acertos | Servo abre | | |
| F7 | Quiz com 0 ou 1 acerto | Servo fechado | | |
| F8 | Dica e tempo esgotado | Dica na tela; "Tempo esgotado!" conta como erro | | |
| F9 | Servo e mecanismo | A trava abre e fecha sem travar; a placa não reinicia quando o servo se move | | |
| F10 | Latência da Guardiã na placa (fase D, bancada do coflume-guardian-ai com `BANCADA_AMBIENTE "placa"`) | Menor que 1 ms, com média e pior caso registrados | | |

### 3.3 Painel, Agente Secreto e música

| # | Caso | Esperado | Resultado | Data / quem |
|---|---|---|---|---|
| F11 | Conectar o painel r4 | "COFRE PRONTO"; logs com `reenvio_diagnostico_boot` | | |
| F12 | Quiz com o painel conectado | Quadro da Guardiã e rede animam a cada pergunta; "confere" nas avaliações | | |
| F13 | Agente Secreto com senha correta | Pistas, música e contagem começam sozinhas; o servo abre | | |
| F14 | Senha errada, `*`, `D` e 90 s esgotados | Comportamento do guia de uso, seção 4 | | |
| F15 | **A** com o painel fechado | "PAINEL USB AUSENTE" depois de 5 s | | |
| F16 | Fechar a aba do painel durante a missão | "CONEXAO PERDIDA" depois de 5 s; servo fechado | | |
| F17 | Música e volume na caixa de som do evento | Toca, ajusta o volume e para no fim da missão | | |

### 3.4 Dois monitores e demonstração

| # | Caso | Esperado | Resultado | Data / quem |
|---|---|---|---|---|
| F18 | Monitor da IA no segundo monitor físico, em tela cheia (anotar a resolução) | Rede grande legível à distância; brilho dos neurônios perceptível na animação; decisão, previsões e resultado da partida visíveis sem rolagem | | |
| F19 | Partidas seguidas com o monitor aberto | Animação a cada pergunta; "ÚLTIMA AVALIAÇÃO" entre elas; nada atrasa o jogo | | |
| F20 | Agente Secreto com o monitor aberto | Aviso de que a Guardiã não participa; nenhuma senha ou pista no monitor | | |
| F21 | Fechar e reabrir o monitor durante uma partida | O jogo, o áudio e a USB seguem; o monitor volta com o estado atual | | |
| F22 | Desconectar o cabo USB | Painel e monitor avisam; dados marcados como não atuais | | |
| F23 | Demonstração sem o cofre no notebook do evento | Os 3 logs de `demo/` reproduzem com "DADOS DE DEMONSTRAÇÃO" | Evidência parcial em computador pessoal: M1–M3, seção 2.2. Confirmar no equipamento do evento; pendências visuais registradas | 08/11/2026, Sara Thais (parcial) |
| F24 | Sessão longa (por exemplo, 30 minutos alternando Quiz e Agente) | Sem travar, sem reiniciar, sem perder a USB | | |
