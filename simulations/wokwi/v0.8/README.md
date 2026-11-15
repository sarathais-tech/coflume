# Coflume v0.8 EXPERIMENTAL — Guardiã IA no Wokwi

> **Versão experimental.** A Guardiã IA é uma rede neural MLP treinada com **dados sintéticos**. A avaliação final no conjunto de teste lacrado e o teste na **placa física** ainda estão pendentes. A Guardiã **não foi validada com crianças reais**, e esta versão **não comprova eficácia educacional**. Não é versão de produção.
>
> A **v0.7 continua intacta** em [`../v0.7/`](../v0.7/) e segue como versão estável de referência.

## 1. O que muda em relação à v0.7

| Aspecto | v0.7 | v0.8 experimental |
|---|---|---|
| Tela, teclado, servo, LEDs, pinos | ILI9341 (simulação), teclado 4×4, servo no GPIO 12, LEDs nos GPIO 13 e 14 | **Iguais**: mesmos componentes e ligações da v0.7, **mais** as 2 conexões do monitor serial (seção 7) |
| Menu e reinício | Teclas 1, 2 e 3; `#` volta ao menu | **Iguais** |
| Perguntas por partida | 3 | **3** |
| Nível da pergunta 1 | Escolhido pela criança | **Escolhido pela criança** |
| Nível das perguntas 2 e 3 | Fixo | **Escolhido pela Guardiã IA**, na faixa nível inicial ± 1 (0 a 2) |
| Categorias | Ordem fixa do banco | **3 categorias distintas** por partida, em ordem sorteada |
| Banco | 9 perguntas no código | **13 perguntas** de `banco_v08.csv` (12 células nível × categoria) |
| Tempo | 30 s por pergunta | **30 s**, contados só **depois** de a pergunta aparecer na tela |
| Dica (`*`) e confirmação (`#`) | Sim | **Sim** |
| Resposta errada | Mostra a letra correta | Mostra a **letra, o texto da alternativa correta e a dica** (3,5 s em vez de 1,7 s) |
| Tempo esgotado | Erro | **Erro**, com o aviso "Tempo esgotado!" |
| Desbloqueio | ≥ 2 acertos em 3 | **≥ 2 acertos em 3**, decidido só pelo validador do jogo |
| Monitor serial | Teclas, questão, resultado | O mesmo, **mais** o log da Guardiã (seção 6) |

## 2. Arquitetura da integração

```
 teclado ──► sketch.ino (máquina de estados da v0.7)
               │
               ├─ início da partida: sessao_id, sorteio das categorias, historico_limpar()
               │
               ├─ antes de cada pergunta ──► coflume_politica.h  (fluxo-0.8, lógica pura)
               │                               │  níveis permitidos e disponíveis
               │                               └─► para cada nível disponível:
               │                                     guardia::montar_entrada() + guardia::prever()
               │                                     (guardia_runtime.h + guardia_modelo.h: MLP 13→8→1, float32)
               │                               ◄── nível com P(acerto) mais próxima de 0,70 (ou fallback)
               │
               ├─ depois de cada resposta ──► guardia::historico_adicionar(categoria, nível, acerto, tempo, 30 s, dica)
               │
               └─ fim da partida ──► validador: acertos ≥ 2 ? abrir() : fechar()   ← único ponto que move o servo
```

- **A Guardiã só estima P(acerto).** A escolha do nível é feita por `coflume_politica.h`, e **o servo só é acionado por `finalizar()`**, a partir da contagem de acertos. Um teste automatizado verifica que nenhum arquivo da Guardiã ou da política chama o servo, os LEDs ou o PWM.
- **Tudo roda no ESP32-S3**, sem Wi-Fi, servidor, API ou computador. As bibliotecas são as mesmas da v0.7 (`libraries.txt` idêntico).

### Arquivos

| Arquivo | Papel | Origem |
|---|---|---|
| `sketch.ino` | Jogo: v0.7 + integração da Guardiã | Escrito a partir da v0.7 |
| `diagram.json` | Circuito | v0.7 **+ 2 conexões do monitor serial** (`esp:TX`/`esp:RX`), nada mais |
| `libraries.txt` | Bibliotecas | **Cópia idêntica** da v0.7 |
| `guardia_modelo.h` | Pesos da MLP (gerado; **não editar**) | Cópia byte a byte do `coflume-guardian-ai` |
| `guardia_runtime.h` | Features e inferência, contrato 1.0.0 (**não editar**) | Cópia byte a byte do `coflume-guardian-ai` |
| `MODELO.json` | Metadados do modelo | Cópia byte a byte do `coflume-guardian-ai` |
| `ORIGEM_GUARDIA.json` | Repositório, branch, commit e SHA-256 de cada arquivo copiado | Registro desta integração |
| `coflume_politica.h` | Política do fluxo-0.8: faixa, alvo 0,70, empates, fallbacks, categorias, rodízio | Porta de `guardia/politica.py` |
| `coflume_banco.h` | 13 perguntas, com acentos convertidos para a fonte da Adafruit GFX (gerado) | `ferramentas/gerar_banco.py` |
| `guardia_autoteste.h` | Autoteste de inicialização com 21 vetores (gerado) | `ferramentas/gerar_autoteste.py` |
| `ferramentas/` | Geradores e a cópia do banco (`banco_v08.csv`) | — |
| `testes/` | Testes automatizados (seção 8) | — |

## 3. Modelo e contrato

| Campo | Valor |
|---|---|
| Modelo | `mlp8-sint-20261101-e6a4cf00` |
| Arquitetura | MLP 13 → 8 → 1 (ReLU, sigmoide), 121 parâmetros, float32 |
| Hash dos parâmetros | `e6a4cf0086ccf5ab1eccecbe7aea02f3962a6ad7ced29b136d36c5b37b5b9991` |
| Contrato de integração | **1.0.0** (`static_assert(GUARDIA_CONTRATO_MAJOR == 1)`) |
| Dados de treino | **SINTÉTICOS** (simulador sim-1.1) |
| Origem dos arquivos | `sarathais-tech/coflume-guardian-ai`, branch `feat/validacao-final-ab`, commit `0741455`, pasta `embarcado/bancada_wokwi/` |
| Bancada aprovada | Wokwi, 01/11/2026: `APROVADO_FUNCIONAL` (400 vetores, erro máximo 1,19e-7). Latência na placa física: pendente |

| Arquivo copiado | SHA-256 |
|---|---|
| `guardia_modelo.h` | `9a98304baaf92bdc508a94a4861bf73bbbd4267da8ed34532f63e251693bc42e` |
| `guardia_runtime.h` | `79b3d9a98eccc25ec76fcc383a53a73539ca5239971ac25ef0d7daa36eb112c0` |
| `MODELO.json` | `7ebe34d7ec05edb18a521c7568f148011c2a65bab8573361e44c540db1ebb4ea` |
| `testes/vetores_teste.h` | `8187d46ebb28c988592a891822642014271df94d447390e3cc5ec5e2e932ae70` |
| `ferramentas/banco_v08.csv` | `5e9c14e22ae5dd1d4af3f494038a4bb1e8ba41c6839b7847513fc80134d06daa` |

O `guardia_modelo.h` traz `GUARDIA_MODELO_USO "bancada (nao e versao de producao)"`, texto que aparece no monitor serial. O modelo **não passou** pelos portões de release do contrato (seção 6): o teste lacrado e a latência na placa física estão pendentes. A integração na v0.8 experimental foi autorizada pela responsável pelo projeto em 01/11/2026, **somente para o Wokwi**.

## 4. Fluxo da partida (fluxo-0.8)

1. A criança escolhe **1 – Explorador**, **2 – Desafiante** ou **3 – Mestre**.
2. O jogo gera um `sessao_id` aleatório, **sorteia a ordem das 4 categorias** (0 Matemática, 1 Português, 2 Lógica, 3 Conhecimentos gerais) e **limpa o histórico**. Nada da partida anterior é herdado.
3. Para cada pergunta `k = 1, 2, 3`:
   1. **Níveis permitidos:** pergunta 1, só o nível escolhido; perguntas 2 e 3, nível escolhido ± 1, limitado a 0..2.
   2. **Categoria:** a primeira ainda não usada, na ordem sorteada, que tenha pergunta em algum nível permitido. Com o banco atual, todas as 12 células têm pergunta, então não há substituição.
   3. **Previsão:** para cada nível disponível, `montar_entrada(historico, nivel_inicial, categoria, nivel)` e `prever()`.
   4. **Escolha:**
      - pergunta 1: o nível escolhido (a previsão é só registrada);
      - perguntas 2 e 3: o nível com **P(acerto) mais próxima de 0,70**;
      - empate (diferença < 1e-6): prefere o nível inicial; entre dois níveis diferentes do inicial, o mais baixo.
   5. **Pergunta:** a próxima da célula no rodízio, sem repetir pergunta na partida.
   6. A pergunta é desenhada e **só então** o cronômetro de 30 s começa.
   7. A criança marca A–D, pode pedir a dica (`*`) e confirma com `#`. Tempo esgotado conta como erro.
   8. A resposta entra no histórico: categoria, nível, acerto, tempo gasto, limite de 30 s e uso de dica.
4. **Desbloqueio:** com **2 ou mais acertos**, o validador abre o cofre (servo a 0°, LED verde). Senão, o cofre fica fechado (LED vermelho). `#` volta ao menu.

### Fallback seguro

| Situação | Comportamento | `motivo` no log |
|---|---|---|
| Previsão `INVALIDO`, NaN ou fora de [0, 1] em algum nível candidato | Nível inicial (ou o disponível mais próximo dele) | `fallback_previsao_invalida` |
| Autoteste de inicialização reprovado | Guardiã desativada; nível inicial em todas as perguntas | `fallback_sem_modelo` |
| `#define GUARDIA_ADAPTATIVA 0` | Nível fixo, como na v0.7 | `fallback_sem_modelo` |
| Banco inviável (nenhuma categoria restante serve) | Tela "BANCO INCOMPLETO"; `#` volta ao menu. Verificado nas 72 combinações ao ligar | `GUARDIA,erro,banco_inviavel` |

Em todos os casos o nível fica **dentro da faixa permitida** e a partida não trava.

### O que a Guardiã tende a fazer (cenários sintéticos)

Nos 1.152 cenários de referência com o modelo (seção 8), com 4 perfis fixos de resposta (o mais rápido: acerto em 6 s), a escolha das perguntas 2 e 3 ficou assim. São resultados **[SINTÉTICO]**: mostram o comportamento do modelo, não o efeito em crianças.

| Nível inicial | Desce 1 nível | Mantém | Sobe 1 nível |
|---|---|---|---|
| Explorador | — | 94% | 6% |
| Desafiante | 70% | 30% | 0% |
| Mestre | 88% | 12% | — |

O modelo estima P(acerto) abaixo de 0,70 na maioria das perguntas de Desafiante e Mestre. Por isso a Guardiã **costuma baixar** a dificuldade, e só sobe depois de acertos rápidos. Exemplo: em Desafiante, depois de 2 acertos sem dica em até 3 s cada, a MLP passa a escolher Mestre na pergunta 3 em parte das combinações de categorias; com 4 s ou mais, mantém Desafiante. Na partida manual no Wokwi de 02/11/2026, a Guardiã de fato subiu de Desafiante para Mestre na pergunta 3 ([relatório](../../../docs/testes/coflume-v0.8-wokwi.md)). Esse é o comportamento esperado da política aprovada (alvo de 0,70) com este modelo, e convém explicá-lo na apresentação.

## 5. Acentos na tela

A fonte clássica da Adafruit GFX (`glcdfont.c`) segue o code page 437 e não lê UTF-8. O gerador do banco converte cada caractere com o codec cp437:

- **Único caractere acentuado no banco:** `ê`, na dica de V07-MES-1-R1 ("Use potências de 2"). Ele vira o código **0x88**.
- **Códigos a partir de 176 são recusados.** A GFX desloca esses códigos quando `cp437(false)`, que é o padrão da v0.7. O 0x88 fica abaixo desse limite e não depende dessa configuração.
- **Caracteres que não existem no cp437 também são recusados** (por exemplo, `ã` e `õ`). O gerador avisa antes de compilar.

Verificação na fonte (Adafruit GFX 1.13.0), saída do teste de renderização:

```
..................#...#####...............#...............
..............#.......................................
#.##...###..#####..###..#.##...###...##....##....####.
##..#.#...#...#...#...#.##..#.#...#...#......#..#.....
##..#.#...#...#...#####.#...#.#.......#....###...###..
#.##..#...#...#.#.#.....#...#.#...#...#...#..#......#.
#......###.....#...####.#...#..###...###...####.####..
#.....................................................
```

**Limitação:** nessa fonte, o acento circunflexo do `ê` é desenhado como um **traço reto** sobre o "e", não como um "^". Fica legível como "e acentuado", mas convém conferir na tela.

## 6. Monitor serial

As linhas da v0.7 continuam iguais: `MENU`, `KEY:x`, `QUESTAO:n NIVEL:n`, `RESULTADO:...`, `WIN`/`LOSE` e `PWM:...`. A linha `QUESTAO` passa a incluir o `ID`. Em `QUESTAO`, o nível vai de 1 a 3, como na v0.7. Nas linhas `GUARDIA`, o nível vai de **0 a 2**, como no contrato.

| Linha | Quando | Conteúdo |
|---|---|---|
| `GUARDIA,modelo` | Ao ligar | id, hash, contrato, fonte (`SINTETICO`), uso, parâmetros |
| `GUARDIA,autoteste` | Ao ligar | vetores, erro máximo, falhas, entradas inválidas, resultado |
| `GUARDIA,estado` | Ao ligar | modo adaptativo configurado, banco viável, Guardiã ativa |
| `GUARDIA,sessao` | Início da partida | `sessao`, modelo, nível inicial, ordem sorteada das categorias |
| `GUARDIA,decisao` | **Antes** de cada pergunta | níveis permitidos e disponíveis, **`p0`, `p1`, `p2`** (`NA` = não avaliado), `alvo=0.70`, `nivel_escolhido`, `p_escolhido`, `motivo`, `pergunta` |
| `GUARDIA,features` | Antes de cada pergunta | as features do nível apresentado (contrato 1.0.0) |
| `GUARDIA,resposta` | Depois de cada resposta | acerto, dica, tempo gasto, limite, `esgotou`, tamanho do histórico |
| `GUARDIA,fim` | Fim da partida | acertos e desbloqueio |

**Como mostrar que a adaptação veio da MLP:** na linha `decisao` das perguntas 2 e 3, o `nivel_escolhido` é o nível cujo `p` está mais próximo de 0,70, com `motivo=guardia`. Exemplo de partida simulada no PC (Desafiante; P1 certa, P2 errada com dica, P3 certa):

```
GUARDIA,decisao,...,ordem=2,nivel_inicial=1,...,niveis_disponiveis=0|1|2,celula_vazia=0,p0=0.843988,p1=0.693282,p2=0.620566,alvo=0.70,nivel_escolhido=1,p_escolhido=0.693282,motivo=guardia,pergunta=V07-DES-1
GUARDIA,resposta,...,ordem=2,pergunta=V07-DES-1,categoria=0,nivel=1,acertou=0,usou_dica=1,tempo_gasto_s=14.3,tempo_limite_s=30,esgotou=0,historico_n=2
GUARDIA,decisao,...,ordem=3,nivel_inicial=1,...,p0=0.811663,p1=0.595033,p2=0.499985,alvo=0.70,nivel_escolhido=1,p_escolhido=0.595033,motivo=guardia,pergunta=V08-DES-POR-1
```

Na pergunta 3, |0,595 − 0,70| = 0,105 é menor que |0,812 − 0,70| = 0,112, então a Guardiã manteve o nível 1. Para comparar, compile com `#define GUARDIA_ADAPTATIVA 0`: as perguntas 2 e 3 ficam no nível escolhido, com `motivo=fallback_sem_modelo`.

## 7. Executar no Wokwi (sem conta, token ou credenciais)

1. Em **wokwi.com**, crie um projeto **Arduino para ESP32-S3**, ou abra uma cópia do projeto da v0.7.
2. Substitua `sketch.ino`, `diagram.json` e `libraries.txt` pelos desta pasta. O `diagram.json` da v0.8 liga o monitor serial à UART0 do ESP32-S3:
   ```json
   ["esp:TX", "$serialMonitor:RX", "", []],
   ["esp:RX", "$serialMonitor:TX", "", []]
   ```
   Sem essas duas conexões, o Wokwi mostra só as mensagens de inicialização do ROM (ESP-ROM), e não as linhas `COFLUME` e `GUARDIA` do programa. O jogo funciona igual nos dois casos.
3. Crie **cinco** arquivos com exatamente estes nomes e cole o conteúdo desta pasta:
   - `guardia_modelo.h`
   - `guardia_runtime.h`
   - `guardia_autoteste.h`
   - `coflume_banco.h`
   - `coflume_politica.h`
4. Inicie a simulação. No monitor serial, confira `GUARDIA,autoteste,...,resultado=OK` e `guardia_ativa=1`. A tela mostra "Guardia IA: ativa (experimental)".
5. Jogue e acompanhe as linhas `GUARDIA,decisao` (seção 6).

O roteiro completo de testes manuais está em [`docs/testes/coflume-v0.8-wokwi.md`](../../../docs/testes/coflume-v0.8-wokwi.md).

## 8. Testes

```bash
python3 -m pytest -q simulations/wokwi/v0.8/testes
```

Opcionais:
- `GFX_GLCDFONT=<caminho>/glcdfont.c` ativa o teste de renderização dos acentos.
- `COFLUME_GUARDIAN_AI=<caminho do coflume-guardian-ai>` compara os arquivos byte a byte com a origem.

O CI ([`.github/workflows/v08.yml`](../../../.github/workflows/v08.yml)) roda esses testes e compila a v0.8 e a v0.7 para ESP32-S3.

| Teste | Verifica |
|---|---|
| `test_arquivos_da_guardia_conferem_com_a_origem`, `test_pesos_identicos_ao_repositorio_da_guardia`, `test_modelo_identificado_e_rotulado` | Pesos exatos, hashes, id, contrato e rótulo experimental |
| `test_banco_gerado_esta_atualizado`, `test_bytes_do_banco_no_firmware`, `test_conversao_de_acentos` | As 13 perguntas no firmware são exatamente as do CSV aprovado |
| `test_renderizacao_dos_acentos` | Mapeamento do `ê` na fonte da GFX (seção 5) |
| `test_autoteste_gerado_esta_atualizado` | Autoteste é cópia exata de 21 dos 400 vetores |
| `test_400_vetores_politica_e_referencia_python` | 400 vetores da bancada (erro < 1e-5); regras do fluxo-0.8 (faixa, 0,70, empates, cada fallback, categorias, rodízio); **1.224 partidas** comparadas com `guardia/politica.py` e a MLP do `.npz` (mesmas decisões; previsões com erro < 1e-5) |
| `test_partida_completa[1]` e `[0]` | `sketch.ino` real, compilado no PC com imitações de Arduino, teclado, tela e PWM (detalhes abaixo) |
| `test_sem_dependencia_de_internet`, `test_guardia_nao_aciona_o_servo`, `test_circuito_da_v07_preservado_com_monitor_serial`, `test_v07_intacta` | Funcionamento offline, servo só no validador, circuito da v0.7 preservado (só com as 2 conexões do monitor serial a mais) e v0.7 sem alterações |

O `test_partida_completa` cobre:
- 3 perguntas por partida, 30 s contados depois da apresentação e tempo esgotado como erro;
- dica, `#` sem alternativa marcada e prazo com prioridade sobre a tecla;
- histórico com acerto, nível, categoria, tempo e dica;
- escolha conferida recalculando a MLP;
- faixa de níveis, categorias distintas e nenhuma pergunta repetida;
- rodízio entre partidas;
- desbloqueio só com 2 ou mais acertos, servo e LEDs;
- reinício sem herdar histórico, em mais de 60 partidas;
- fallback quando a Guardiã falha.

### Autoteste de inicialização (firmware jogável)

O jogo não carrega os 400 vetores (cerca de 75 KB de código-fonte). Ao ligar, ele roda **21 vetores** escolhidos de forma determinística: o primeiro vetor de cada combinação de grupo (aleatórios / validação) × tamanho do histórico (0, 1, 2, 3 ou mais) × nível candidato (0, 1, 2). A seleção cobre:

| Verificação | Cobertura |
|---|---|
| Features calculadas (inteiras e reais, tolerância 1e-6) | Partida a frio e históricos de 1 a 8 respostas |
| P(acerto) contra o Python (erro < 1e-5) | 3 níveis candidatos, 3 níveis iniciais, 4 categorias |
| Contrato, item 2 | 2 entradas fora da faixa devem retornar `INVALIDO` |
| Identidade | `GUARDIA_MODELO_ID` do autoteste igual ao do modelo |

Se qualquer item falhar, a Guardiã é desativada e o jogo segue no nível fixo.

## 9. Limitações

- **Experimental.** O modelo usa dados sintéticos, o teste lacrado não foi aberto e a latência na placa física não foi medida. Não houve validação com crianças reais, e não há comprovação de eficácia educacional.
- **Tela.** A ILI9341 é só a tela da simulação. A ST7735 de 1,8" da montagem física exige outro firmware de tela (não migrado nesta etapa).
- **Banco pequeno.** São 13 perguntas, quase sempre 1 por célula, então partidas seguidas repetem perguntas. O rodízio já está pronto e o código aceita até 8 perguntas por célula; a meta da feira é 36. Para ampliar:
  1. aprovar o novo CSV no `coflume-guardian-ai`;
  2. copiá-lo para `ferramentas/banco_v08.csv`;
  3. rodar `ferramentas/gerar_banco.py`;
  4. atualizar os hashes em `ORIGEM_GUARDIA.json`.
- **Rodízio na RAM.** O rodízio volta ao início quando a placa reinicia.
- **Aleatoriedade.** O `esp_random()` sem rádio ligado basta para o `sessao_id` e para sortear as categorias, mas não é um gerador criptográfico.
- **Tempo no Wokwi.** O cronômetro usa `millis()`, que segue o tempo **simulado**. Se a simulação rodar abaixo de 100%, os 30 s duram mais no relógio de parede.
- **Acento.** O circunflexo do `ê` aparece como um traço reto (seção 5).
- **Compilação.** O CI usa a versão mais recente do core Arduino-ESP32 3.x. Localmente, a v0.8 foi compilada com o core 3.3.12 (detalhes no relatório de testes).
