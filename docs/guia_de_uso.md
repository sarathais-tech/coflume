# Guia de uso: quizzes, Agente Secreto, música, USB, dois monitores e demonstração

**Versão de referência:** firmware e painel da [`v0.9-r4`](../firmware/v0.9-r4/), experimental e **ainda não testada na placa física**. Montagem e gravação: [`guia_iniciantes.md`](guia_iniciantes.md). Instruções curtas para quem apresenta: [`LEIA_PRIMEIRO.txt`](../firmware/v0.9-r4/painel/LEIA_PRIMEIRO.txt).

## 1. Menu do cofre

| Tecla | Modalidade |
|---|---|
| **1**, **2** ou **3** | **Quiz** com a Guardiã IA, começando em Explorador, Desafiante ou Mestre |
| **A** | **Agente Secreto**: precisa do painel USB no notebook |
| **#** | Nas telas de resultado, volta ao menu |

## 2. Quiz com a Guardiã IA

- **Uma partida:** **3 perguntas**, de categorias diferentes (Matemática, Português, Lógica, Conhecimentos gerais), com **30 segundos** cada. O banco da r4 tem 36 perguntas, 3 por nível e categoria.
- **Teclado:** **A–D** marcam a alternativa, **`*`** mostra a dica e **`#`** confirma. Tempo esgotado conta como erro.
- **Feedback:** depois de cada resposta, a tela diz se acertou e, no erro, mostra a resposta certa e a dica.
- **Abertura:** com **2 ou mais acertos em 3**, o servo abre. Quem decide é o **validador do jogo**, não a IA. `#` fecha e volta ao menu.

### O papel da Guardiã

1. **Pergunta 1:** é sempre do nível que a criança escolheu.
2. **Previsão (perguntas 2 e 3):** a **rede neural** estima a chance de acerto, P(acerto), em cada nível permitido. Os níveis permitidos são o inicial ± 1.
3. **Escolha:** a **política fluxo-0.8**, uma regra fixa e auditável, escolhe o nível cuja estimativa fica mais perto de **70%**.
4. **Política gradual:** é experimental e fica **desligada** por padrão. Ela limitaria mudanças bruscas de nível.

A IA **nunca** aciona o servo. O modelo foi treinado com **dados sintéticos** e não comprova efeito educacional. A diferença entre rede, política e validador está explicada no `coflume-guardian` (`docs/guia_da_ia.md`).

## 3. Conexão USB e painel

1. Ligue o ESP32 ao notebook com um **cabo USB de dados**.
2. Feche o **Monitor Serial** e o **Plotter Serial** da IDE: a porta é uma só.
3. Abra `firmware/v0.9-r4/painel/Coflume_Agente_Secreto_USB.html` no **Chrome** ou no **Edge** do computador. É um arquivo local; não precisa de internet nem de servidor.
4. Clique em **CONECTAR ESP32**, escolha a porta da placa e aguarde **COFRE PRONTO**. Se a placa reiniciar ao conectar, espere a inicialização.

**Durante a apresentação:** mantenha a aba do painel **ativa**, sem minimizar e sem suspender o computador. O painel manda um sinal (PING) a cada segundo. Se o cofre ficar 5 s sem recebê-lo durante a missão, ele encerra o Agente Secreto ("CONEXAO PERDIDA").

**Logs da Guardiã:** é a área recolhível "Logs da Guardia IA". Ela mostra as linhas `GUARDIA,...` do cofre:
- diagnóstico de inicialização (reenviado ao conectar);
- decisões, features e respostas de cada pergunta.

**SALVAR LOGS (.TXT)** baixa o arquivo. O painel guarda até 5.000 linhas.

## 4. Agente Secreto

1. Com o painel em **COFRE PRONTO**, prepare a porta do cofre e aperte **A** no **teclado físico**.
2. O painel mostra **quatro expressões**. Cada resultado é um dígito de 0 a 9, na ordem, inclusive quando começa com 0. Ao mesmo tempo, começam a **música** e a contagem de **90 segundos**.
3. Digite os 4 dígitos no teclado do cofre e confirme com **`#`**.
   - **`*`** apaga o último dígito.
   - **`D`** cancela e volta ao menu.
   - Código errado permite tentar de novo enquanto houver tempo.
4. **Senha correta:** o **ESP32** aciona o servo.
   - O painel **não** tem campo de senha nem botão para abrir.
   - Nos últimos 15 s há bipes.
5. **Próximo agente:** depois do resultado, prepare ou feche a porta, aperte **`#`** e depois **A**. Uma senha nova é sorteada.

O Agente Secreto **não usa a Guardiã IA**: não há inferência nessa modalidade.

| Aviso na tela do cofre | Causa |
|---|---|
| PAINEL USB AUSENTE | As pistas não foram confirmadas em 5 s (painel fechado ou desconectado) |
| CONEXAO PERDIDA | O painel parou de responder durante a missão |
| A Agente: SERVO C/ FALHA | O servo não inicializou; o Agente Secreto não começa |

**Observação de segurança:** a senha viaja em texto pela USB até o painel, que precisa dela para gerar as expressões. Para a apresentação o risco é baixo, mas está registrado.

## 5. Música

- **Nenhuma música vem no projeto.** Use um arquivo de áudio que você tenha **autorização** para reproduzir.
- No painel, abra **"Musica e volume"** e selecione o arquivo **antes do primeiro jogador**. Isso também libera o áudio no navegador.
- Ligue a **caixa de som** ao notebook e escolha-a como saída de áudio do sistema.
- A música começa com a missão e para no fim. Há também o volume e a opção de silenciar.

## 6. Quadro da Guardiã e rede neural (no painel principal)

Abaixo dos botões do painel:

- **Quadro "GUARDIÃ IA · ATUAÇÃO NO QUIZ":**
  1. **previsão da rede** por nível, com 1 casa decimal e o alvo de 70%;
  2. **escolha da política**, com o motivo;
  3. **proteção gradual**, que aparece como "não se aplica" com a política fluxo-0.8.

  Abaixo, a tabela da partida: resultado, tempo, dica e o tempo somado das respostas. Informação que falta no log aparece como **"indisponível"**.
- **Rede neural 13 → 8 → 1, "cálculo reproduzido no painel":** o notebook refaz a conta da rede com os mesmos pesos do firmware, a partir das features enviadas pelo ESP32. Depois **confere** com a previsão enviada pela placa (|Δ| e "confere"). **Não são medições internas da placa.** A animação é em **câmera lenta**, e o pulso nas conexões é só recurso visual.

## 7. Dois monitores: monitor da IA

1. No painel principal, clique em **ABRIR MONITOR DA IA**. Se o navegador bloquear, permita pop-ups para o arquivo.
2. Arraste a janela nova para o **segundo monitor** e clique em **⛶ TELA CHEIA**.
3. Conecte o cofre no painel principal, como sempre. **Só o painel principal usa a USB.**

O monitor mostra:
- a frase "A rede neural estima a chance de acerto. A política do jogo usa essa estimativa para escolher a dificuldade.";
- a rede grande (cerca de 2/3 da tela), com os neurônios mais brilhantes quanto maior a ativação calculada, e os **dados usados antes da pergunta**: acertos anteriores, tempo médio e dicas, só no formato que os dados permitem;
- a **chance estimada de acerto** por dificuldade, com o alvo de 70%;
- a **dificuldade escolhida pela política**, com uma explicação curta tirada dos logs;
- o resultado da partida, separado dos dados usados antes da pergunta;
- a identificação discreta "cálculo reproduzido no painel".

Os valores e termos técnicos, incluindo os nomes das 13 entradas da rede, ficam nos detalhes recolhidos. A janela cabe sem rolagem em 1920×1080 e 1366×768.

| Situação | O que o monitor mostra |
|---|---|
| Pergunta nova | "AVALIANDO: …", em câmera lenta; no fim, a decisão |
| Entre perguntas | "ÚLTIMA AVALIAÇÃO", parado |
| Agente Secreto | "Esta modalidade não usa a Guardiã IA" (sem senha nem pistas) |
| Cofre desconectado ou comunicação perdida | Aviso, e o conteúdo fica cinza, marcado como não atual |
| Painel principal fechado | "SEM COMUNICAÇÃO COM O PAINEL PRINCIPAL" |

**Fechar ou recarregar o monitor** não afeta o jogo, o áudio nem a USB. Para reabrir, use o mesmo botão: o monitor recebe o estado atual.

**Não minimize o painel principal.** O navegador desacelera janelas minimizadas, e o monitor avisaria que perdeu a comunicação.

Detalhes técnicos: [`painel/docs/monitor_ia_dois_monitores.md`](../firmware/v0.9-r4/painel/docs/monitor_ia_dois_monitores.md).

## 8. Demonstração sem hardware

Útil para ensaiar ou apresentar sem o cofre.

1. Abra o HTML do painel. Não é preciso conectar.
2. Opcional: **ABRIR MONITOR DA IA** e leve a janela ao segundo monitor.
3. Em **"Reproduzir log"**, clique em **ABRIR ARQUIVO…** e escolha um arquivo de `firmware/v0.9-r4/painel/demo/`:

| Arquivo | Mostra |
|---|---|
| `demo_fluxo08.txt` | Quiz Desafiante (acerto, erro com dica, tempo esgotado), Agente Secreto e Quiz Explorador com 3 acertos |
| `demo_gradual.txt` | Política gradual ligada: na pergunta 3, a proteção limita a subida depois de um erro |
| `demo_recusa.txt` | Banco inviável: primeiro o bloqueio normal; depois, um **cenário forçado de teste**, identificado no arquivo e na tela, com a recusa da Guardiã |

4. Use **▶ REPRODUZIR** (velocidade de 1× a 64×), **PRÓXIMO EVENTO** ou **IR AO FIM**. **VOLTAR AO VIVO** encerra a reprodução.

A tela mostra **"DADOS DE DEMONSTRAÇÃO"**. Os logs vieram do firmware r4 rodando **em simulação no PC**, com respostas roteirizadas: **não são do cofre nem de crianças**. A reprodução não envia nada ao ESP32 e não entra nos logs salvos.
