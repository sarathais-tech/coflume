# Testes funcionais — Coflume v0.7 (Wokwi)

**Data do relato:** 28/10/2026  
**Ambiente:** Wokwi, ESP32-S3 DevKitC-1, display ILI9341 simulado, teclado 4x4, servo e LEDs.  
**Responsável pelo relato:** Sara Thais.

## Resultados relatados

| Caso | Resultado |
| --- | --- |
| Inicialização da simulação | Aprovado |
| Seleção e execução do nível Explorador | Aprovado |
| Seleção e execução do nível Desafiante | Aprovado |
| Seleção e execução do nível Mestre | Aprovado |
| Respostas às três questões e conclusão da partida | Aprovado |
| Abertura do servo ao concluir as três questões corretamente | Aprovado |
| Indicação por LEDs | Aprovado |
| Reinício por tecla | Aprovado |

**Origem da evidência:** execução manual da simulação relatada pela responsável. Não equivale a teste automatizado nem à validação física.

## Testes pendentes

- Tempo limite de 30 segundos e comportamento após esgotamento do tempo.
- Respostas erradas e condição de derrota.
- Uso da tecla de dica e confirmação em todas as situações.
- Repetição de várias partidas, estabilidade e reinício após falha.
- Testes em hardware real: ST7735 de 1,8 polegada, alimentação externa do servo, identificação das linhas/colunas do teclado físico e ligações elétricas.
- Treinamento, integração e medição da rede neural Guardiã IA.

## Limitações da versão

Esta v0.7 contém **perguntas fixas e nenhuma inferência de IA integrada**. O display ILI9341 usado no Wokwi é substituto de simulação e não representa automaticamente a compatibilidade com o display físico ST7735.
