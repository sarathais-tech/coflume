# Testes — Coflume v0.8 EXPERIMENTAL (Guardiã IA no Wokwi)

**Versão:** `v0.8-experimental` · **Pasta:** [`simulations/wokwi/v0.8/`](../../simulations/wokwi/v0.8/)
**Modelo:** `mlp8-sint-20261101-e6a4cf00` (MLP 13 → 8 → 1, 121 parâmetros, contrato 1.0.0, dados **sintéticos**)

> Versão experimental. A Guardiã não foi validada com crianças reais, e os testes abaixo **não comprovam eficácia educacional**. O teste lacrado do modelo e a placa física estão pendentes. A v0.7 continua sendo a versão estável ([relatório](coflume-v0.7-wokwi.md)).

## 1. Testes automatizados (01/11/2026; repetidos em 02/11/2026)

**Ambiente:** Linux, Python 3, pytest e g++ (C++17, `-Wall -Wextra -Werror`).

```bash
python3 -m pytest -q simulations/wokwi/v0.8/testes
```

O comando foi executado com `GFX_GLCDFONT` (Adafruit GFX 1.13.0) e com `COFLUME_GUARDIAN_AI` (`coflume-guardian-ai` no commit `0741455`). **Resultado: 15 de 15 aprovados, nenhum pulado.**

| Requisito | Evidência | Resultado |
|---|---|---|
| Pesos exatos do modelo aprovado | SHA-256 dos arquivos = `ORIGEM_GUARDIA.json` e cópia byte a byte do `coflume-guardian-ai` | Aprovado |
| Equivalência com a bancada | 400 vetores da bancada aprovada: erro máximo de P(acerto) **1,19e-7**, features idênticas, 0 falhas inteiras | Aprovado |
| Escolha adaptativa com as previsões reais da MLP | 1.224 partidas, 3.672 decisões, comparadas com `guardia/politica.py` + MLP do `.npz`: **mesma categoria, nível e motivo em todas**; erro máximo das previsões 1,19e-7 | Aprovado |
| Faixa permitida, alvo 0,70, empates, fallbacks | Testes da política: 19.465 verificações, 0 falhas. Fallback testado com `INVALIDO`, NaN, infinito, 1,5 e −0,01 | Aprovado |
| Exatamente 3 perguntas, 30 s, timeout como erro | Partida completa com o `sketch.ino` real: 2.502 verificações no modo adaptativo e 1.929 no nível fixo, 0 falhas | Aprovado |
| Cronômetro só depois da apresentação | Cada desenho na tela simulada consome 1 ms; o cronômetro começa depois de 10 ou mais desenhos, ao fim da apresentação | Aprovado |
| Histórico, dica, teclado, respostas certas e erradas | Categoria, nível, acerto, tempo, limite e dica conferidos a cada resposta; `#` sem alternativa não confirma; prazo tem prioridade sobre a tecla | Aprovado |
| Três categorias distintas, sem repetir pergunta, rodízio | Mais de 60 partidas; as 2 perguntas de Mestre × Conhecimentos gerais aparecem em rodízio | Aprovado |
| Desbloqueio só com 2 ou mais acertos; servo e LEDs | Partidas com 3, 2, 1 e 0 acertos: duty do PWM do servo e estado dos LEDs conferidos | Aprovado |
| Reinício sem herdar histórico | Toda partida nova começa com histórico vazio e novo `sessao_id` | Aprovado |
| Guardiã não aciona o servo | Nenhum arquivo da Guardiã ou da política usa PWM, LED ou `abrir()`; `abrir()` só aparece em `finalizar()`, depois de `desbloqueia(acertos)` | Aprovado |
| Funcionamento offline | Nenhuma referência a Wi-Fi, HTTP, BLE ou MQTT; `libraries.txt` idêntico ao da v0.7 | Aprovado |
| Acentos na fonte da GFX | `ê` → 0x88 (cp437), abaixo de 176; glifo conferido no `glcdfont.c` | Aprovado, com a limitação da seção 3 |
| v0.7 intacta | SHA-256 dos 3 arquivos iguais aos de `main` (`e20778c`); `git diff` vazio | Aprovado |
| Circuito da v0.7 preservado e monitor serial ligado (02/11/2026) | Mesmos componentes e as 23 ligações da v0.7, na ordem; a mais, **só** `esp:TX → $serialMonitor:RX` e `esp:RX → $serialMonitor:TX` | Aprovado |

### Compilação para ESP32-S3

O CI (`.github/workflows/v08.yml`) compila a v0.8 e a v0.7 com a versão mais recente do core Arduino-ESP32 e registra os tamanhos. Ele só roda depois que a branch for publicada.

Compilação local feita antes da publicação (01/11/2026, repetida em 02/11/2026 com os mesmos tamanhos):

| Item | Configuração |
|---|---|
| Core | Arduino-ESP32 **3.3.12**, toolchain `esp-x32` 2601 |
| FQBN | `esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi`, com `--warnings all` |
| Bibliotecas | Keypad 3.1.1, Adafruit GFX 1.13.0, Adafruit ILI9341 1.7.0, Adafruit BusIO 1.17.4 |

Os avisos de compilação foram verificados na v0.8: nenhum.

| Sketch | Programa | Variáveis globais | Resultado |
|---|---|---|---|
| v0.7 (sem alterações) | 356.555 bytes (27%) | 24.668 bytes (7%) | Compila |
| **v0.8** | **368.463 bytes (28%)** | **25.028 bytes (7%)** | **Compila** |

## 2. Execução manual no Wokwi (02/11/2026): evidência observada

**Fonte:** relato da responsável pelo projeto (Sara Thais) depois de executar a v0.8 no wokwi.com. Os valores abaixo são **transcrição do relato**; o log bruto do monitor serial não foi anexado ao repositório. Esta seção registra o que **foi observado** no Wokwi. A seção 1 registra testes **automatizados** no PC, que não executam o Wokwi.

### 2.1 Monitor serial

- **Problema observado:** com o `diagram.json` copiado da v0.7, o monitor serial mostrava só as mensagens de inicialização do ROM do ESP32-S3 (ESP-ROM), sem as linhas `COFLUME` e `GUARDIA` do programa. Menu, teclado, perguntas e servo funcionavam normalmente.
- **Correção:** foram acrescentadas ao `diagram.json` da v0.8 as conexões `["esp:TX", "$serialMonitor:RX", "", []]` e `["esp:RX", "$serialMonitor:TX", "", []]`. Nenhuma outra peça ou ligação mudou, e o `sketch.ino` não mudou.
- **Verificação:** depois da correção, as linhas do programa apareceram no monitor serial do Wokwi.
- **Mensagens temporárias:** as mensagens de diagnóstico (`TESTE: ENTREI NO SETUP` e `TESTE: ENTREI NO SETUP UART0`) foram usadas só na cópia do projeto no Wokwi. Elas **não existem** nos arquivos deste repositório; a busca no repositório não encontrou nenhuma.

### 2.2 Partida observada

| Item | Observado |
|---|---|
| Autoteste da Guardiã | `OK`, 21 vetores, erro máximo **5,96e-8**, nenhuma falha |
| Modo adaptativo | `guardia_ativa=1` |
| Nível inicial | Desafiante (1) |
| Pergunta 1 | Desafiante, acerto (nível escolhido pela criança, `motivo=nivel_inicial_pergunta_1`) |
| Pergunta 2 | A MLP **manteve Desafiante**, com P(acerto) prevista de **72,38%** |
| Pergunta 3 | A MLP **subiu para Mestre**, com P(acerto) prevista de **66,47%** |
| Resultado | 3 acertos em 3; o servo abriu e apareceu `WIN` |
| Relato anterior (antes da correção do monitor) | Menu e teclado funcionando; 3 perguntas respondidas; servo abre com 2 acertos |

**O que isso mostra:**
- **Inferência embarcada:** a Guardiã executou inferências no ESP32-S3 simulado, e o autoteste com o modelo aprovado passou.
- **Adaptação:** o nível das perguntas 2 e 3 foi escolhido com motivo `guardia` e ficou dentro da faixa permitida (0 a 2).
- **Coerência com os testes no PC:** o erro de 5,96e-8 é o mesmo obtido no PC, e a subida de Desafiante para Mestre depois de acertos rápidos é um comportamento previsto (README da v0.8, seção 4).

**O que isso não mostra:**
- **Probabilidades dos outros níveis:** foram fornecidas posteriormente no chat as linhas completas `GUARDIA,decisao`, incluindo as probabilidades de todos os níveis candidatos; veja a seção 2.4.
- **Eficácia educacional e placa física:** uma partida no simulador não mede efeito educacional nem desempenho na placa física.

### 2.3 Testes manuais concluídos (relato de 02/11/2026)

**Fonte:** relato da responsável pelo projeto (Sara Thais), com os casos aprovados no Wokwi. É evidência **observada** e transcrita; o log bruto não foi anexado.

| Caso relatado como aprovado | Casos do roteiro (seção 3) |
|---|---|
| Partida com 3 acertos e desbloqueio | 11 |
| Partida com 3 erros e uso de dica | 7, 8 e desbloqueio negado com 0 acertos |
| Partida com 3 timeouts e cofre fechado | 9 e desbloqueio negado com 0 acertos |
| Reinício de sessão com histórico zerado | 14 |
| Fallback com `GUARDIA_ADAPTATIVA 0`, mantendo o nível inicial | 16 |
| Autoteste da Guardiã com 21 vetores, sem falhas | 1 |
| Decisões adaptativas reais da MLP, com aumento e redução de dificuldade | 5 |

**Limites deste registro:**
- **Valores por nível:** as linhas completas `GUARDIA,decisao` foram fornecidas posteriormente, e as probabilidades são transcritas na seção 2.4. Os arquivos brutos de monitor serial ainda não foram anexados ao repositório.
- **Fallback por previsão inválida:** o relato cobre o fallback por configuração (`GUARDIA_ADAPTATIVA 0`). O fallback por previsão inválida (`INVALIDO`, NaN, fora de [0, 1]) não pode ser provocado no jogo sem alterar o código e é coberto só pelos testes automatizados.
- **Versão publicada:** o modo `GUARDIA_ADAPTATIVA 0` foi usado só para esse teste. A versão publicada usa `GUARDIA_ADAPTATIVA 1`.

### 2.4 Probabilidades observadas no Wokwi (transcrição adicional de 02/11/2026)

As linhas completas do monitor serial foram coladas pela responsável pelo projeto na conversa. Os valores abaixo são **evidência observada transcrita**, não uma nova execução por CI, nem medição física. Os identificadores de sessão permitem rastreá-las. Nas decisões adaptativas a distância é `abs(P(acerto) - 0,70)`.

| Sessão | Pergunta | Nível inicial | P(Explorador) | P(Desafiante) | P(Mestre) | Escolhido | Motivo |
|---|---:|---|---:|---:|---:|---|---|
| `42C7700F` | 2 | Desafiante | 0,862149 | 0,723767 | 0,665500 | Desafiante | `guardia` |
| `42C7700F` | 3 | Desafiante | 0,853062 | 0,746119 | 0,664680 | Mestre | `guardia` |
| `8333AC1F` | 2 | Desafiante | 0,757212 | 0,537296 | 0,518693 | Explorador | `guardia` |
| `8333AC1F` | 3 | Desafiante | 0,840214 | 0,640039 | 0,530321 | Desafiante | `guardia` |
| `74361DF3` | 2 | Explorador | 0,555481 | 0,346623 | NA | Explorador | `guardia` |
| `74361DF3` | 3 | Explorador | 0,549511 | 0,350334 | NA | Explorador | `guardia` |
| `8D9BBB2E` | 2 | Explorador | 0,552496 | 0,343892 | NA | Explorador | `guardia` |
| `8D9BBB2E` | 3 | Explorador | 0,552500 | 0,343896 | NA | Explorador | `guardia` |

Todas as escolhas acima correspondem ao candidato disponível cuja probabilidade fica mais próxima de 0,70. Os níveis marcados `NA` não são permitidos na respectiva sessão. A sessão `5871BB27` confirma reinício com `n_respondidas=0`. A sessão `15469BC1` confirma fallback desativado, sem previsões (`p0=p1=p2=NA`) e `motivo=fallback_sem_modelo`. Estes exemplos não substituem a avaliação final lacrada nem a verificação na placa física.

## 3. Roteiro manual no Wokwi

Use as instruções do [README da v0.8](../../simulations/wokwi/v0.8/README.md#7-executar-no-wokwi-sem-conta-token-ou-credenciais) e guarde o texto do monitor serial de cada caso.

| # | Caso | Como fazer | Esperado | Resultado |
|---|---|---|---|---|
| 1 | Inicialização | Iniciar a simulação | `GUARDIA,autoteste,...,resultado=OK`, `guardia_ativa=1`, menu com "Guardia IA: ativa (experimental)" | **Observado** em 02/11/2026: autoteste OK e `guardia_ativa=1` (texto do menu não transcrito) |
| 2 | Explorador | Tecla 1, responder as 3 | Pergunta 1 no nível 0; perguntas 2 e 3 no nível 0 ou 1; `motivo=guardia` | _pendente_ |
| 3 | Desafiante | Tecla 2 | Pergunta 1 no nível 1; perguntas 2 e 3 entre 0 e 2 | **Observado** em 02/11/2026: níveis 1, 1 e 2 |
| 4 | Mestre | Tecla 3 | Pergunta 1 no nível 2; perguntas 2 e 3 no nível 1 ou 2 | _pendente_ |
| 5 | Escolha pela MLP | Em cada `GUARDIA,decisao` das perguntas 2 e 3 | `nivel_escolhido` = nível com `p` mais próximo de 0,70 | **Aprovado** (relato de 02/11/2026): decisões reais da MLP com aumento e redução de dificuldade. `p0`, `p1` e `p2` fornecidos posteriormente (seção 2.4) |
| 6 | Categorias | Uma partida qualquer | 3 categorias diferentes | _pendente_ |
| 7 | Resposta errada | Marcar uma alternativa errada e `#` | "VAMOS APRENDER!", letra, texto da correta e dica; LED vermelho | **Aprovado** (relato de 02/11/2026): partida com 3 erros |
| 8 | Dica | `*` durante a pergunta | Dica na tela; `usou_dica=1` no log | **Aprovado** (relato de 02/11/2026) |
| 9 | Tempo esgotado | Não responder por 30 s | "Tempo esgotado!", conta como erro, `esgotou=1` | **Aprovado** (relato de 02/11/2026): 3 timeouts, cofre fechado |
| 10 | Cronômetro | Observar o início da pergunta | Começa em 30 s depois de a pergunta aparecer | _pendente_ |
| 11 | 3 acertos | Acertar as 3 | "COFRE ABERTO!", servo a 0°, LED verde | **Observado** em 02/11/2026: servo abriu e `WIN` (LED verde não transcrito) |
| 12 | 2 acertos | Acertar 2 | Cofre abre | **Observado** (relato de 02/11/2026, antes da correção do monitor serial) |
| 13 | 1 acerto | Acertar 1 | "NOVA CHANCE!", servo fechado, LED vermelho | _pendente com 1 acerto_; cofre fechado com 0 acertos **aprovado** (relato de 02/11/2026) |
| 14 | Reinício | `#` ao final e nova partida | Menu; nova `sessao`; pergunta 1 com `n_respondidas=0` | **Aprovado** (relato de 02/11/2026): histórico zerado |
| 15 | Várias partidas | 5 partidas seguidas | Sem travar; Mestre × Conhecimentos gerais alterna entre as 2 perguntas | _pendente_ |
| 16 | Nível fixo | Recompilar com `#define GUARDIA_ADAPTATIVA 0` | Perguntas 2 e 3 no nível escolhido, `motivo=fallback_sem_modelo` | **Aprovado** (relato de 02/11/2026): nível inicial mantido |
| 17 | Acento | Encontrar V07-MES-1-R1 (Mestre × Matemática) e pedir a dica | "Use potências de 2" legível | _pendente_ |

### Casos sem registro manual (opcionais)

Não constam do relato de 02/11/2026. Todos estão cobertos pelos testes **automatizados** no PC (seção 1):

- Explorador e Mestre como nível inicial (casos 2 e 4).
- Categorias distintas conferidas no log (caso 6).
- Início do cronômetro (caso 10).
- Partida com exatamente 1 acerto (caso 13).
- Várias partidas seguidas com rodízio (caso 15).
- Acento na dica de V07-MES-1-R1 (caso 17).

## 4. Limitações conhecidas

- **Circunflexo:** na fonte clássica da GFX, o circunflexo do `ê` é um traço reto sobre o "e" (ver o README da v0.8, seção 5).
- **O que os testes no PC não verificam:** eles usam imitações de tela, teclado e PWM, então não testam aparência, cores nem a temporização real do Wokwi.
- **Tempo no Wokwi:** o cronômetro segue o tempo simulado.
- **Comportamento do modelo:** em cenários **sintéticos**, a Guardiã costuma **baixar** a dificuldade de Desafiante e Mestre e só sobe depois de acertos rápidos (README da v0.8, seção 4). Isso não é medida de efeito em crianças.
- **Pendências:** teste lacrado do modelo, latência na placa física, migração para ST7735 e ampliação do banco para 36 perguntas. A v0.8 continua **experimental**, treinada com dados sintéticos e sem validação com crianças reais.
