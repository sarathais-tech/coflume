# Montagem: fotos e informações que faltam

**Situação em 08/11/2026.** O repositório documenta a **pinagem lógica** (os GPIOs no firmware) e a **configuração da placa**. Ele **não** documenta a montagem física. Os itens abaixo precisam ser **observados na montagem real e registrados**: nenhum deles foi preenchido por suposição.

Até lá, o [guia para iniciantes](guia_iniciantes.md) traz apenas **recomendações gerais**, marcadas como tal.

## 1. Informações que faltam

### Alimentação

| # | Informação | Por que importa |
|---|---|---|
| A1 | De onde vem a alimentação do **servo**: pino 5V/VBUS da placa, fonte externa ou outro; tensão e corrente máxima | Evitar reinício da placa e dano ao regulador |
| A2 | Se há **GND comum** entre a fonte do servo e o ESP32 | Sem ele, o sinal do GPIO 12 fica sem referência |
| A3 | Se há **capacitor** perto do servo, e qual valor | Estabilidade quando o servo se move |
| A4 | Como o conjunto é alimentado **no evento**: só a USB do notebook, fonte na tomada ou power bank | Roteiro do dia e plano B |

### Componentes

| # | Informação | Por que importa |
|---|---|---|
| C1 | **Modelo do servo** (SG90, MG90S, MG996R…) e a faixa de tensão da folha de dados | Alimentação e torque da trava |
| C2 | **Módulo da tela**: fabricante e serigrafia dos pinos (por exemplo, GND, VCC, SCL, SDA, RES, DC, CS, BL) e se aceita 5 V no VCC | Ligação correta de VCC e backlight |
| C3 | Ligação do **backlight** (BL/LED): 3V3, 5V ou GPIO; com ou sem resistor | Hoje o firmware não controla o backlight |
| C4 | Opção de `TFT_PAINEL` que dá as cores certas nessa tela (BLACKTAB, GREENTAB ou REDTAB) e a rotação usada | Hoje o padrão é BLACKTAB e rotação 1, nunca confirmados na tela física |

### Teclado

| # | Informação | Por que importa |
|---|---|---|
| T1 | **Ordem dos fios** do conector do teclado e a qual GPIO cada um vai | O firmware espera as linhas em 8–11 e as colunas em 4–7 |

### Placa e USB

| # | Informação | Por que importa |
|---|---|---|
| P1 | Qual porta USB da DevKitC-1 é usada (**USB** ou **UART/COM**) e o valor de "USB CDC On Boot" | Gravação e conexão do painel |
| P2 | Comprimento e tipo do cabo USB usado no evento | Perda de conexão com o painel |

### Trava e caixa

| # | Informação | Por que importa |
|---|---|---|
| M1 | **Mecanismo da trava**: como o braço do servo prende a porta e quais ângulos abrem e fecham na prática (no firmware: 90° fechado e 0° aberto) | Se os ângulos precisam de ajuste |
| M2 | **Caixa de MDF**: dimensões, onde ficam a tela, o teclado, a placa e o servo; furos e passagem de cabos | Guia de montagem completo |
| M3 | Como a porta é **reposicionada** entre jogadores, e quem faz isso | Roteiro do apresentador |

### LEDs

| # | Informação | Por que importa |
|---|---|---|
| L1 | Se haverá **LEDs físicos**; se sim, quais GPIOs livres e quais resistores | Hoje os LEDs são simulados; os GPIOs proibidos estão no guia, seção 4 |

### Evento

| # | Informação | Por que importa |
|---|---|---|
| E1 | Notebook, navegador e versão; se haverá **segundo monitor** e como ele se conecta | Teste dos dois monitores |
| E2 | **Caixa de som** do Agente Secreto e o arquivo de música autorizado (nome; o arquivo não vai para o repositório) | Ensaio de áudio |

## 2. Fotos que faltam

Tire as fotos com boa luz, de cima e de lado, de modo que dê para ler a serigrafia dos pinos e a cor dos fios. **Não inclua pessoas, crianças, nomes ou dados pessoais nas fotos.**

| # | Foto | Mostra |
|---|---|---|
| F1 | Montagem completa de bancada, vista de cima | Placa, tela, teclado, servo e protoboard |
| F2 | ESP32-S3 de perto, com os fios | Os GPIOs 4–11 (teclado), 12 (servo) e 14–18 (tela) e os pinos de alimentação usados |
| F3 | Tela por trás | Serigrafia dos pinos e cor de cada fio |
| F4 | Conector do teclado | Ordem dos fios e GPIO de cada um |
| F5 | Servo e sua alimentação | Fios de sinal, VCC e GND; fonte, se houver; GND comum |
| F6 | Mecanismo da trava | Posição fechada (90°) e aberta (0°) |
| F7 | Caixa por fora | Tela, teclado e porta |
| F8 | Caixa por dentro | Fixação da placa, do servo e passagem de cabos |
| F9 | Etiqueta ou modelo do servo e da fonte | Modelo e tensão |
| F10 | Mesa do evento | Notebook, caixa de som e segundo monitor com o monitor da IA |
| F11 | Tela de início da r4 na tela real | "Autoteste: OK", "Politica: fluxo-0.8" |
| F12 | Uma pergunta e um feedback na tela real | Legibilidade (item F3 dos testes) |

## 3. Onde registrar

- Fotos em `docs/montagem/` (pasta a criar), com nomes como `F2_esp32_fios.jpg`.
- As informações da seção 1, quando confirmadas, entram nas seções 2 a 4 do [guia para iniciantes](guia_iniciantes.md), no lugar das marcas **[a confirmar]**, com a data.
- Os resultados dos testes físicos vão em [`testes/coflume-v0.9-r4.md`](testes/coflume-v0.9-r4.md), seção 3.
