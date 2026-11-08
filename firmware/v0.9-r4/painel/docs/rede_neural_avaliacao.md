# Rede neural no painel: de onde vêm as ativações

**Situação:** incorporado à v0.9-r4. A análise abaixo foi feita na proposta, com o firmware r3; na r4 o mesmo teste (`testes/test_rede_guardia.py`) roda com o firmware r4 e também confere 850 de 850 previsões em cada política. Nenhuma linha de log nova foi criada no firmware para a rede.

## Pergunta

Para mostrar a rede da Guardiã (13 entradas → 8 ocultos ReLU → 1 saída sigmoide, 121 parâmetros) com as ativações reais, há dois caminhos:

| | A · Firmware envia as ativações | B · Painel reproduz a inferência |
|---|---|---|
| Mudança no firmware | Sim: nova revisão, novas linhas de log, revalidação na placa | **Não** |
| O que seria exibido | Valores internos da placa (medição) | Valores **recalculados** no computador, a partir do que a placa envia |
| Tráfego USB | +8 ativações e z por candidato (até 3 por pergunta) | Nenhum: usa as linhas `GUARDIA,decisao` e `GUARDIA,features`, que já existem |
| Precisão | A do formato de log escolhido | float32 com as features em 6 casas; a saída é conferida com a previsão do ESP32 |
| Risco | Alterar o firmware perto da Fase C | Divergir da placa sem que se perceba (mitigado pela conferência a cada previsão) |

## O que o firmware já envia

- **`GUARDIA,features`:** as 9 features da pergunta, com 6 casas decimais (`%.6f`) e já calculadas pelo `montar_entrada()` da placa. Vêm para o nível escolhido, uma vez por pergunta.
- **`GUARDIA,decisao`:** `p0`, `p1` e `p2`, a previsão de cada nível avaliado, com 6 casas, ou `NA` quando o nível não foi avaliado.
- **Outros candidatos:** o firmware avalia os níveis com `montar_entrada(historico, nivelInicial, categoria, nivelCandidato)`. Só o `nivel_candidato` muda entre eles. Por isso o painel reconstrói cada candidato a partir da mesma linha `features`, trocando apenas esse campo. O pré-processamento do histórico não precisa ser refeito no painel.

## Fidelidade medida

| Verificação | Casos | Maior diferença |
|---|---|---|
| Vetores de equivalência da referência Python (`vetores_teste.h`) | 400 | 1,2×10⁻⁷ (análise em numpy); no painel, como log de 6 casas, 400 de 400 dentro da tolerância |
| Firmware r3 simulado, 150 partidas com fluxo-0.8 | 850 previsões | 6,4×10⁻⁷ |
| Firmware r3 simulado, 150 partidas com regra gradual | 850 previsões | 6,4×10⁻⁷ |
| Logs de demonstração | 20 previsões | 5,7×10⁻⁷ |

Como se chega à tolerância de 1,9×10⁻⁶:
- **Features:** o arredondamento de 6 casas das features contínuas (±5×10⁻⁷) é padronizado e propagado pela rede. A ReLU não amplia o erro e a sigmoide tem derivada até 0,25. Resultado: 1,2×10⁻⁶.
- **Previsão:** o arredondamento de 6 casas da previsão registrada soma 5×10⁻⁷.
- **Aritmética:** a diferença float32/exp entre a placa e o navegador soma 2×10⁻⁷.

A tolerância é calculada no painel a partir dos próprios pesos.

## Decisão: B, a reprodução no painel

Ela é fiel dentro da precisão dos dados enviados e não exige mudar o firmware. Para não confundir com medição:

- **Rótulos:** o quadro se chama "**cálculo reproduzido no painel**" e diz que as ativações "**não são medições internas da placa**".
- **Conferência por avaliação:** cada avaliação mostra a P reproduzida, a P registrada pelo ESP32, o |Δ| e "confere" ou "**NÃO CONFERE**". A divergência aparece; nunca é escondida.
- **Conferência acumulada:** o painel mostra "N de N previsões dentro da tolerância".
- **Recusas:**
  - modelo do log diferente do embutido (id ou hash);
  - linha `features` ausente ou incompleta;
  - categoria da `features` diferente da decisão.

  Nesses casos a rede aparece como indisponível.
- **Pesos:** são copiados de `v0.9-r4/coflume_v09/guardia_modelo.h` (idêntico ao da r3 e da v0.8) por `ferramentas/extrair_modelo.py`. O script confere o sha256 do arquivo, o id e o hash dos parâmetros contra o `ORIGEM_GUARDIA.json`.

**Limite:** a reprodução confere a saída, mas não as ativações intermediárias, porque a placa não as envia. Se um dia isso for necessário, por exemplo para uma auditoria, o caminho A pode entrar numa revisão futura, com uma linha opcional e desligada por padrão.
