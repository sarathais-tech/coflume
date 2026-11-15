# Monitor da IA: demonstração em dois monitores

**Situação:** incorporado à v0.9-r4 (experimental). O monitor não exige mudança no firmware. Uso real em dois monitores ainda não testado.

## Como funciona

| | Painel principal | Monitor da IA (janela de exposição) |
|---|---|---|
| Arquivo | `Coflume_Agente_Secreto_USB.html` | o mesmo arquivo, aberto com `?monitor=ia` pelo botão |
| USB | **é o único que conecta** | nunca pede a porta |
| Áudio, pistas, protocolo AG | iguais ao painel r3 | não roda nada disso (o script do painel termina na primeira linha) |
| Dados da Guardiã | lê a USB, calcula e confere a rede | recebe um resumo pronto para exibição |

**Comunicação:** `window.open` + `postMessage`. Funciona com o HTML aberto localmente (`file://`) no Chrome e no Edge, sem servidor.
- **Por que não BroadcastChannel ou localStorage:** em `file://` a origem é opaca, então a comunicação por esses meios não é garantida.
- **Origem das mensagens:** o monitor só aceita mensagens da janela que o abriu.

### Envios do principal ao monitor

- **Na abertura e na reabertura:** o monitor carrega e avisa "pronto". O principal responde com o **estado completo**. Isso também vale depois de um F5 em qualquer uma das duas janelas.
- **A cada mudança:** um resumo novo, enviado só se algo mudou. Linhas que chegam juntas viram um único envio.
- **A cada segundo:** uma batida. Se ela não chega por 3,5 s, o monitor mostra "SEM COMUNICAÇÃO COM O PAINEL PRINCIPAL".
- **Conteúdo do resumo:**
  - política;
  - partida (status, acertos, nível inicial);
  - última pergunta: previsões por nível, nível escolhido, explicação curta, proteção gradual e as avaliações da rede (entradas, ativações e saída reproduzidas, |Δ| com o ESP32);
  - conferência acumulada;
  - estado da conexão USB;
  - modalidade;
  - identificação da demonstração.
- **O que nunca vai ao monitor:** linhas do log, senha, pistas, sessão do Agente Secreto. Os testes conferem isso nas mensagens recebidas.

## Na InovaTech

1. Abra `Coflume_Agente_Secreto_USB.html` no Chrome ou Edge, no monitor principal.
2. Clique em **ABRIR MONITOR DA IA**, no quadro da Guardiã. Se o navegador bloquear, permita pop-ups para o arquivo e clique de novo.
3. Arraste a janela nova para o segundo monitor e clique em **⛶ TELA CHEIA**.
4. No painel principal, clique em **CONECTAR ESP32**, como hoje.

**Cuidados:**
- **Painel principal visível:** não minimize o painel principal. Com a janela minimizada, o navegador desacelera os temporizadores dela, e a batida pode atrasar. Nesse caso o monitor avisa que perdeu a comunicação; ele não mostra dados velhos como atuais.
- **Fechar o monitor:** não afeta o jogo, o áudio nem a USB. O botão volta a "ABRIR MONITOR DA IA", e reabrir recebe o estado atual.

## O que o monitor mostra

Visual para leitura à distância (preto e verde neon), em tela cheia. A janela cabe **sem rolagem em 1920×1080 e 1366×768**, com o resultado da partida sempre visível. A rede ocupa cerca de **2/3 da largura útil**; a coluna lateral, mais estreita, traz as previsões e a decisão. Capturas atuais: `capturas/27_monitor_rede_grande_1920x1080.png` e `capturas/27_monitor_rede_grande_1366x768.png`. As capturas 25 e 26 mostram a etapa anterior do visual.

- **Topo:**
  - "Como a Guardiã escolhe o próximo desafio", com a frase "A rede neural estima a chance de acerto. A política do jogo usa essa estimativa para escolher a dificuldade.";
  - à direita, o estado da conexão. Na reprodução de log aparece **um único** aviso, "DADOS DE DEMONSTRAÇÃO", com a nota do arquivo, se houver.
- **1 / A rede faz a previsão:**
  - "Avaliando: …" durante a animação, ou "Última avaliação: …";
  - **Dados usados antes da pergunta N**, tirados da linha `GUARDIA,features`. Cada um só aparece no formato que os dados permitem; na falta da linha `features`, aparece "indisponível":

    | Dado | Quando aparece como total ou em segundos | Caso contrário |
    |---|---|---|
    | Acertos | "X de N acertos anteriores", quando a taxa × respondidas dá um inteiro | A taxa |
    | Dicas | Total, só quando a média × respondidas dá um inteiro | "dicas por pergunta (média)" |
    | Tempo médio | Em segundos, só quando o tempo limite aparece nas respostas da partida | "% do limite" |

  - **Rede 13 → 8 → 1, grande,** com os **pesos e as ativações reais**:
    - camadas "Dados da partida", "8 neurônios" e "Chance de acerto";
    - círculos maiores que os do painel principal (neurônios e saída), em geometria própria desta janela;
    - os **13 nós de entrada sem rótulos** no desenho; os nomes, valores enviados e valores padronizados ficam numa tabela nos detalhes técnicos;
    - as 112 conexões reais, com as mesmas cores e espessuras relativas do painel principal, em traço 30% mais fino e mais transparente;
    - **brilho proporcional à ativação reproduzida**: cada neurônio ganha um halo do tamanho da sua ativação (relativa à maior da pergunta), com transição suave; ativação zero fica escura, tracejada e sem halo. A saída brilha conforme a chance calculada; as entradas, conforme o valor padronizado. Nada é aceso sem um valor calculado;
    - a saída mostra "≈ N% estimados". No rodapé: "cálculo reproduzido no painel".
- **Chance estimada de acerto:**
  - uma barra por dificuldade, com "≈ N%" ou "Não avaliado";
  - o "Alvo: 70%" tracejado;
  - "DIFICULDADE ESCOLHIDA PELA POLÍTICA" sob a escolhida.
- **2 / A política escolhe a dificuldade (não é a rede):**
  - a dificuldade em destaque e uma explicação curta, montada com os logs. Exemplos: "A proteção gradual impediu aumentar a dificuldade após um erro." (ou "após um tempo esgotado", conforme a resposta anterior registrada); "Mestre estava mais perto do alvo de 70%, mas fora da regra.";
  - com o fluxo-0.8, por exemplo: "Desafiante tem a chance estimada mais próxima do alvo de 70% entre as permitidas.".
- **Rodapé, separado dos dados usados antes da pergunta:**
  - "Partida até agora" (acertos em respondidas) ou **"Resultado final da partida"** (acertos em 3), conforme o log já tenha o fim;
  - "Cofre aberto", "Cofre fechado" ou "Aguardando o fim da partida";
  - "Previsões são estimativas, não certezas."
- **Detalhes técnicos e diagnóstico (recolhidos):**
  - entradas enviadas pelo ESP32, com 6 casas;
  - P reproduzida × P do ESP32, |Δ| e conferência acumulada;
  - modelo e hash, tempo limite registrado e controles de animação e ritmo.

## Estados

| Situação | Monitor |
|---|---|
| Pergunta nova no Quiz | "AVALIANDO: DESAFIANTE (2 de 3)", em câmera lenta (rotulada); cada barra aparece quando a rede termina aquele nível; a decisão aparece no fim |
| Entre perguntas | "ÚLTIMA AVALIAÇÃO · pergunta N de 3", parado, sem pulsos |
| Abrir ou reabrir no meio da partida | estado completo, parado como "última avaliação" (não reanima pergunta antiga) |
| Agente Secreto | só "MODO AGENTE SECRETO · Esta modalidade não usa a Guardiã IA"; a animação para; nada da missão aparece |
| Cofre desconectado ou comunicação USB perdida | aviso no topo, e o conteúdo fica cinza com "não é atual" |
| Painel principal fechado ou sem batida | "SEM COMUNICAÇÃO COM O PAINEL PRINCIPAL · Os dados abaixo não são atuais" |
| Reprodução de log no principal | faixa "DADOS DE DEMONSTRAÇÃO", com a nota do arquivo (por exemplo, o cenário forçado da recusa) |
| Monitor aberto direto, sem o painel | pede para usar o botão do painel principal |

## Demonstração sem o cofre

1. Abra o HTML.
2. Clique em **ABRIR MONITOR DA IA** e coloque a janela no segundo monitor.
3. No principal, em "Reproduzir log", abra `demo/demo_gradual.txt` (proteção gradual atuando) ou `demo/demo_fluxo08.txt` (Quiz, Agente Secreto e Quiz).
4. Use **▶ REPRODUZIR** (velocidade 1× ou 4×) ou **PRÓXIMO EVENTO**.

O monitor anima cada pergunta e troca para o Agente Secreto quando o log chega lá. `demo/demo_recusa.txt` mostra o bloqueio normal e depois o cenário forçado de recusa.

## Testes (`testes/test_monitor_ia.py`, 9)

- **Abertura no meio da partida:** o monitor recebe o estado completo, parado, e não usa a USB (0 pedidos de porta, 0 escritas). O principal pede a porta uma vez só. Os detalhes técnicos ficam recolhidos.
- **Sincronização:** cada pergunta nova anima, e o monitor termina em "última avaliação". Uma resposta atualiza o placar sem reanimar. As mensagens não trazem linhas do log e têm menos de 12 kB.
- **Fechar e reabrir:** fechar o monitor não afeta o jogo (o PREP leva a READY e a missão anda). Reabrir e dar F5 recebem o estado atual.
- **Agente Secreto:** começar a missão no meio da animação para a rede e mostra o aviso. Senha, sessão e pistas nunca chegam ao monitor. Ao voltar ao Quiz, a rede volta a animar.
- **Reprodução de log:** demonstração identificada; recusa com a nota do cenário forçado; voltar ao vivo tira a faixa.
- **Perda da USB:** aviso, e os dados aparecem como não atuais.
- **Perda do painel principal:** aviso de falta de comunicação.
- **Monitor aberto direto:** pede o botão.
- **Protocolo AG:** igual ao painel r3 com o monitor aberto.
- **Leitura somente:** verificação estática de que a ponte e o monitor não tocam em USB, áudio ou protocolo.
