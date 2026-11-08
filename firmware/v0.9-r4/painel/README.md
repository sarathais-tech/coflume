# Painel USB da v0.9-r4: quadro da Guardiã IA, rede neural e monitor da IA

**Situação:** painel da **v0.9-r4 EXPERIMENTAL**, ainda **não testado na placa física**. Os painéis da r2 e da r3 continuam intactos (hash conferido nos testes). Instruções do apresentador: [`LEIA_PRIMEIRO.txt`](LEIA_PRIMEIRO.txt).

## O que é

`Coflume_Agente_Secreto_USB.html` é o painel da r3 com acréscimos. O principal é o quadro **"GUARDIÃ IA · ATUAÇÃO NO QUIZ"**, montado só com as linhas que o firmware já envia pela USB e mostrado em três blocos separados.

| Bloco | Linha do log | O que mostra |
|---|---|---|
| 1 · Previsão da MLP | `GUARDIA,decisao` (`p0`, `p1`, `p2`, `alvo`) | P(acerto) estimada por nível, em barras, **com 1 casa decimal** ("≈ 80,1% · 10,1 p.p. acima do alvo"), e o alvo de 70% tracejado. O valor completo do log fica no texto de ajuda da barra. Valor `NA` aparece como "indisponível · não avaliado". |
| 2 · Escolha da política | `GUARDIA,decisao` (`nivel_escolhido`, `motivo`, níveis permitidos e disponíveis) | Nível escolhido pela política e o motivo registrado. A marca "◆ escolhido pela política" fica na barra, em texto, e não só em cor. |
| 3 · Proteção gradual | `GUARDIA,politica_gradual` | Ver as situações abaixo. |
| Tabela da partida | `GUARDIA,resposta` e `GUARDIA,fim` | As 3 perguntas, com resultado (acertou, errou ou tempo esgotado), tempo, dica e a coluna "Proteção gradual". Abaixo, os totais: acertos, dicas, **tempo somado das respostas** (soma de `tempo_gasto_s`) e desbloqueio. Partida interrompida por erro aparece como "interrompida". Clicar numa linha abre os detalhes daquela pergunta. |
| Contexto | `GUARDIA,sessao`, `estado`, `modelo` e `autoteste` | Política ativa e de qual linha ela veio, modelo, autoteste e se o banco é viável. |

O bloco 3 pode mostrar:
- **"PROTEÇÃO ATUOU"**: o que o fluxo-0.8 escolheria (`nivel_sem_regra`) e o que a regra permitiu (`nivel`);
- **"não precisou intervir"**;
- **"RECUSA"**, quando `sem_nivel_compativel`;
- **"não aplicável"**, com fluxo-0.8.

## Regras seguidas

- **A MLP prevê e a política decide.** A MLP nunca aparece como quem escolhe. Quando a proteção atua, a estimativa da MLP para o nível barrado continua visível.
- **Estimativas, não certezas.** As probabilidades aparecem como "≈ N,N%", com a distância do alvo. Uma legenda fixa explica: "modelo experimental treinado com dados sintéticos: indicam tendência, não certeza, e não avaliam a criança".
- **Arredondamento só na tela.** O painel não decide nada; a escolha do firmware usa os valores completos. Quando o nível escolhido aparece com a mesma estimativa ou a mesma distância do alvo que outro nível avaliado (com 1 casa), surge o aviso **"Empate visual"**. Ele mostra os valores registrados e o desempate da política, tirado de `escolher_nivel` em `coflume_politica.h`: menor distância exata do alvo (tolerância 1e-6); se ainda empatar, o nível inicial; depois, o mais baixo. O aviso só aparece quando as estimativas decidiram (`motivo=guardia`).
- **Erros do firmware com texto próprio.** `banco_inviavel` e `banco_inviavel_gradual` aparecem como "Quiz bloqueado na verificação inicial"; `sem_nivel_compativel` aparece como "Guardiã recusou a próxima pergunta". O código do log continua visível.
- **Informação ausente aparece como "indisponível · motivo".** Exemplos: "campo ausente no log", "não avaliado nesta pergunta", "valor inválido no log: 1.7". Um `motivo` desconhecido aparece como "Motivo registrado: X · sem descrição neste painel".
- **Única dedução:** a regra que impediu a escolha ("não subir depois de erro" ou "no máximo 1 nível de diferença"). Ela é calculada dos campos `anterior`, `anterior_acertou` e `nivel_sem_regra` segundo a regra documentada, e aparece rotulada como deduzida. Se os campos não bastam, aparece como indisponível.
- **Agente Secreto:** durante PREP, PLAYING, WIN e LOSE, o quadro mostra só "MODO AGENTE SECRETO · A Guardiã IA não participa desta modalidade".
- **Somente leitura:** o quadro não chama `send()` e não mexe em estado AG, relógio, áudio, pistas ou área de logs. O teste confere isso no código-fonte.
- **Mudança no painel original:** a única linha nova no código existente é a chamada `guardiaVisUSB(line);` no início de `packet()`, protegida por try/catch. Todo o resto é acréscimo.
- **HTML montado, não editado à mão:** `ferramentas/montar_painel.py` gera o HTML a partir do painel r3. Ele acrescenta um script comum (`modelo_guardia.js`, gerado por `extrair_modelo.py`, mais `guardia_comum.js` e `monitor_guardia.js`), os blocos `visualizacao_guardia.js`, `rede_guardia.js` e `ponte_monitor.js`, e o `.css`.
- **Segunda linha nova no código existente:** no início do script do painel, `if(...modoMonitor)return;`. Ela faz a janela de exposição não executar USB, áudio, pistas nem a animação de fundo. Na janela principal não muda nada. O teste confere com `--verificar`.

## Monitor da IA (dois monitores)

O botão **ABRIR MONITOR DA IA**, no quadro da Guardiã, abre uma janela de exposição para o segundo monitor. É o mesmo arquivo, com `?monitor=ia`.

- **USB e sincronização:** só o painel principal usa a USB. O monitor recebe por `postMessage` um resumo pronto para exibição: o estado completo ao abrir, reabrir ou dar F5, e depois a cada mudança, com uma batida por segundo.
- **O que o monitor mostra:** a rede grande, as previsões por nível, a decisão da política com uma explicação curta e a identificação "cálculo reproduzido no painel". Os detalhes técnicos ficam recolhidos. Há botão de tela cheia.
- **Estados:** última avaliação parada entre perguntas; Agente Secreto sem a Guardiã; perda do cofre ou do painel principal sem mostrar dados como atuais; DADOS DE DEMONSTRAÇÃO na reprodução de log.

Guia da InovaTech, estados e testes: `docs/monitor_ia_dois_monitores.md`.

## Rede neural da Guardiã (cálculo reproduzido no painel)

Abaixo do quadro fica a rede **13 entradas → 8 neurônios ocultos (ReLU) → 1 saída (sigmoide)**, com os neurônios em posições fixas. Por que a reprodução no painel foi escolhida em vez de logs novos no firmware: `docs/rede_neural_avaliacao.md`.

- **Origem dos valores:** as ativações são **recalculadas no computador** com os pesos do `guardia_modelo.h` da r4 (idêntico ao da v0.8), a partir da linha `GUARDIA,features` enviada pelo ESP32. O quadro diz que não são medições da placa.
- **Conferência:** cada avaliação é comparada com a previsão registrada pelo ESP32 (|Δ| e "confere" dentro de 1,9×10⁻⁶). A conferência acumulada aparece embaixo.
- **Cores e conexões:**
  - entradas padronizadas em âmbar (acima da média) ou azul (abaixo);
  - ocultos acesos pela ativação ReLU (0 = apagado, contorno tracejado);
  - saída pela P(acerto);
  - conexões coloridas pelo sinal do peso, com espessura pelo |peso|.
  - Ao passar o mouse, aparecem os valores enviados, padronizados e reproduzidos.
- **Avaliações em sequência:** "Avaliando: Desafiante (2 de 3)", com a lista dos candidatos (aguardando, ▶ avaliando, feita).
- **Decisão da política:** fica numa caixa à parte, "**DECISÃO DA POLÍTICA (não é a rede)**", que só aparece depois das avaliações. O "◆ escolhido pela política" marca o nível na lista, mas a rede continua mostrando a previsão.
- **Câmera lenta:** a animação é rotulada "visualização em câmera lenta · o pulso nas conexões é recurso visual, não o tempo real do cálculo". O ritmo pode ser câmera lenta (cerca de 5 s por candidato) ou mais rápido; há a opção "animar" e o botão ↻ REPETIR. Com "reduzir movimento" no sistema, o painel mostra direto o estado final.
- **Sem acúmulo:** cada pergunta anima uma vez. Uma pergunta nova cancela a animação anterior, e voltar a uma pergunta já vista mostra o estado final parado. A animação só lê valores já calculados, e o teste mede que o painel responde `AG,READY` em menos de 300 ms durante a animação.
- **Agente Secreto:** a rede apaga, os pulsos param e aparece "a Guardiã IA não participa desta modalidade. A rede não é calculada nem animada".

**Demonstração no notebook:** abra o HTML e reproduza `demo/demo_fluxo08.txt` ou `demo/demo_gradual.txt` (faixa "DADOS DE DEMONSTRAÇÃO"). A rede anima cada pergunta conforme a reprodução avança.

## Reprodução de logs no notebook (sem o cofre)

1. Abra o HTML no Chrome ou Edge. Não é preciso conectar.
2. Em "Reproduzir log", clique em **ABRIR ARQUIVO…** e escolha um `.txt` de `demo/`, ou um log salvo pelo botão "Salvar" da área de logs.
3. Use **▶ REPRODUZIR** (1× a 64×, respeitando os tempos do log), **PRÓXIMO EVENTO**, **IR AO FIM** ou **VOLTAR AO VIVO**.

Durante a reprodução:
- aparece uma faixa listrada **"DADOS DE DEMONSTRAÇÃO · reprodução de arquivo de log; não vêm do cofre conectado nem de crianças"**, com o nome e o cabeçalho do arquivo;
- linhas `#` no meio do arquivo aparecem como **"NOTA DO ARQUIVO NESTE TRECHO"** a partir do ponto em que estão (é assim que o cenário forçado da recusa fica marcado);
- as linhas reproduzidas não são enviadas ao ESP32 e não entram na área de logs nem no arquivo salvo;
- se o cofre estiver conectado, as linhas ao vivo continuam sendo recebidas em segundo plano, num modelo separado.

### Arquivos de demonstração (`demo/`)

Foram gerados por `ferramentas/gerar_demo.cpp`. O sketch real da **r4** roda no PC com os stubs de teclado, tela e serial, e as respostas são roteirizadas. Usam o banco da r4 (`banco_v09`, 36 perguntas).

| Arquivo | Política | Conteúdo |
|---|---|---|
| `demo_fluxo08.txt` | `GUARDIA_POLITICA_GRADUAL=0` | Quiz Desafiante (acerto; erro com dica; tempo esgotado), missão do Agente Secreto (senha errada, depois certa) e Quiz Explorador com 3 acertos. |
| `demo_gradual.txt` | `GUARDIA_POLITICA_GRADUAL=1` | Quiz Mestre em que a proteção atua na pergunta 3 (erro → o fluxo-0.8 subiria para Mestre → limitado a Desafiante), seguido de uma missão do Agente Secreto. |
| `demo_recusa.txt` | `GUARDIA_POLITICA_GRADUAL=1`, **banco inviável** para a regra gradual (banco_v08 sem Desafiante × Português e Desafiante × Conhecimentos gerais, numa cópia temporária do sketch) | **Parte 1, uso normal:** a verificação inicial detecta `banco_viavel_gradual=0` e o Quiz é bloqueado (`banco_inviavel_gradual`, BANCO INCOMPLETO). **Parte 2, CENÁRIO FORÇADO PARA TESTE:** o gerador define `bancoGradualOK=true` para passar pela verificação inicial e exercitar a proteção defensiva em jogo. Partida Mestre, dois erros; na pergunta 3 só Desafiante é compatível e falta pergunta nas categorias restantes, então a Guardiã registra `sem_nivel_compativel` e não escolhe nada fora da regra. O cabeçalho do arquivo e uma nota no ponto do desvio identificam o cenário forçado. |

Para gerar de novo, a partir desta pasta (requer g++):

```bash
python3 ferramentas/gerar_demos.py
```

O script compila `ferramentas/gerar_demo.cpp` com o sketch da r4 (sem alterá-lo) e, para a recusa, com uma cópia temporária do sketch com banco inviável, montada com a mesma regra de `../testes/test_r3.py`. Rodar de novo produz arquivos idênticos byte a byte.

## Testes

Arquivos de teste: `testes/test_painel_guardia.py` (11), `testes/test_monitor_ia.py` (9: dois monitores; ver o guia) e `testes/test_rede_guardia.py` (8: pesos e HTML conferidos com as fontes, 400 vetores de equivalência, 150 partidas do firmware em cada política com 850 de 850 previsões conferidas, sequência e decisão, sem acúmulo e sem atraso do protocolo, Agente Secreto, casos indisponíveis e divergência, movimento reduzido).

```bash
python3 -m pytest -q testes/test_painel_guardia.py
# para comparar também com o painel original da placa:
COFLUME_PAINEL_ORIGINAL=/caminho/Coflume_Agente_Secreto_USB.html python3 -m pytest -q testes/test_painel_guardia.py
```

São 11 testes (Chromium com porta serial falsa):

- **HTML confere com as fontes** e com o painel r3: só acréscimos, mais o gancho em `packet()`. O quadro não chama `send()` nem mexe no estado do protocolo.
- **Protocolo AG:** igual ao painel r3 e ao painel original da placa. São comparados os comandos enviados, as pistas, os textos, o relógio, os botões e os logs.
- **Ao vivo:** o log `demo_fluxo08` chega pela "USB" em três trechos: quiz, Agente Secreto e quiz. Confere o "Tempo somado das respostas".
- **Reprodução de `demo_gradual`:** a proteção atua na pergunta 3 (≈ 80,1% e 60,0%, 10,1 e 10,0 p.p.: sem empate visual); nada vai ao ESP32 nem à área de logs; há pausa e velocidade.
- **Reprodução de `demo_recusa`:** bloqueio normal; depois a nota do cenário forçado e a recusa `sem_nivel_compativel` na pergunta 3.
- **Empate visual:** 65,01% e 74,98% aparecem a 5,0 p.p.; o aviso mostra os valores completos e o desempate. Sem aviso quando o motivo é fallback.
- **Ao vivo e reprodução não se misturam.**
- **Campos ausentes, valor inválido e motivo desconhecido** aparecem como indisponíveis. O texto da USB é escapado.
- **Recusa com linhas sintéticas** no formato do firmware (mantido como teste de formato).
- **Tela de 390 px:** sem rolagem horizontal.

As capturas ficam em `docs/capturas/`.

## Limites

- **Fonte dos dados:** os dados de demonstração vêm da simulação no PC, não do cofre físico. Falta conferir no cofre com o ESP32-S3 real.
- **Banco das demonstrações:** banco atual (v0.8). O banco_v09 está em `../banco_v09/`, ainda em revisão.
- **Linhas usadas:** o quadro não usa `GUARDIA,features`. A área de logs continua mostrando essa linha.
- **Linhas sem campo `sessao`:** com firmware anterior à r3, sem `politica=`, a política aparece como indisponível. Linhas sem o campo `sessao` abrem uma "partida" marcada com "início da partida ausente no log".
