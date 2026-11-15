# Revisão: Adaptação Gradual v1 (não integrada)

**Pacote revisado:** `Coflume_Adaptacao_Gradual_v1.zip` (SHA-256 `fff08463…a689b`), com `coflume_adaptacao.h`, `sketch.ino`, `teste_adaptacao.cpp` e `LEIA_PRIMEIRO.txt`.
**Situação:** **revisada separadamente e NÃO integrada à v0.9.** O firmware da v0.9 continua com a política fluxo-0.8.

## 1. O que a proposta faz

Depois da decisão original (faixa nível inicial ± 1 e P(acerto) mais próxima de 0,70), restringe os candidatos das perguntas 2 e 3 a:
- **no máximo 1 nível de diferença** em relação à pergunta anterior;
- **depois de erro ou tempo esgotado, nunca subir.**

A pergunta 1, o fallback sem modelo ou com previsão inválida e o banco inviável ficam como antes. A rede neural e seus arquivos não mudam.

O `sketch.ino` do pacote é o sketch do Agente USB com 3 trechos a mais:
- a chamada `decidir_gradual`;
- o reset da adaptação no início da partida;
- o registro da última resposta, mais a linha `GUARDIA,adaptacao` no log.

O Agente Secreto não é afetado.

## 2. Verificações

| Verificação | Resultado |
|---|---|
| `teste_adaptacao.cpp` do pacote, C++11, com AddressSanitizer e UndefinedBehaviorSanitizer | **OK**: 23.328 decisões, sem erros |
| Efeito com a **MLP real**, nos mesmos 1.152 cenários da referência Python (acertos a partir de 6 s) | **0 decisões alteradas** |
| Efeito com a MLP real, em varredura maior: 3 níveis iniciais × 24 ordens × 24 perfis por pergunta (acerto ou erro × tempos de 2, 4, 6, 10, 20 e 30 s × dica) = 124.416 decisões | **448 alteradas (0,36%)**, todas na pergunta 3: 330 subidas depois de erro bloqueadas e 118 saltos de 2 níveis bloqueados |
| Banco com lacuna (ver seção 3) | **Falha da regra**: sobe depois de erro |

O teste do pacote usa previsões fixas (0,20, 0,45 e 0,70), e não a MLP. Com o banco de 13 perguntas, o caminho `sem_candidato` nunca é exercitado: o teste só afirma que ele não ocorre.

## 3. Conflito principal: o fallback quando não há nível compatível

Quando a categoria escolhida pela política original não tem nenhum nível compatível, `decidir_gradual` devolve a **decisão original sem restrição** e só registra `banco_sem_candidato=1`. Nesse caso, a regra "não subir depois de erro" é violada, **mesmo quando outra categoria restante teria um nível compatível**.

**Caso reproduzido.** Banco de teste sem as perguntas de Matemática nos níveis 0 e 1 (só existe Matemática no Mestre). Desafiante; a pergunta 1, de Português, foi **errada**; a próxima categoria sorteada é Matemática.

```
categoria=0 nivel=2 nivel_mlp=2 sem_candidato=1 limitou=0
Logica tem nivel 0 ou 1? 1 1 | Conhecimentos gerais? 1 1
```

A pergunta 2 vai para **Mestre** logo depois de um erro, embora Lógica e Conhecimentos gerais tenham perguntas nos níveis 0 e 1.

Com o banco atual de 13 perguntas isso **não acontece**, porque todas as 12 células têm pergunta. Mas o banco deve crescer para 36 perguntas, e qualquer célula vazia reativa o problema.

**Correção sugerida (não aplicada):**
1. Antes de decidir, filtrar as categorias restantes, mantendo a ordem sorteada, e deixar só as que têm algum nível **compatível**.
2. Chamar `decidir_pergunta` com essa lista filtrada. Assim a categoria escolhida sempre tem candidato, e a previsão é calculada para a categoria certa.
3. Só se **nenhuma** categoria restante tiver nível compatível, usar a decisão original e registrar `banco_sem_candidato=1`. O caso deve ser coberto por teste, com um banco de lacunas.

## 4. Outros pontos

1. **Mudança de política sem referência.** A proposta altera a política aprovada (fluxo-0.8), definida em:
   - `docs/fluxo_partida.md` e `guardia/politica.py`, no `coflume-guardian-ai`;
   - o adendo 02, cuja seção 7 simula o fluxo-0.8 na fase C.

   Integrada como está:
   - o firmware deixa de bater com a referência Python, e a comparação das 1.224 partidas passa a falhar;
   - o que for medido na fase C não corresponde mais ao que roda na placa.

   Sugestão: implementar a mesma regra em `guardia/politica.py`, como fluxo-0.9 numa branch `exp/*`, e só depois integrar ao firmware. Outra opção é integrar atrás de um `#define` desligado por padrão, só para experimento.
2. **Fallback sem modelo.** Sem modelo ou com previsão inválida, a regra gradual não se aplica: a decisão vai para o nível inicial. Isso pode subir 1 nível depois de um erro se a falha acontecer no meio da partida. O LEIA_PRIMEIRO do pacote declara isso. É raro, mas vale decidir se é aceitável.
3. **Efeito pequeno com este modelo.** Como a MLP já tende a baixar a dificuldade depois de erro, a regra raramente muda a escolha: 0% nos cenários de referência e 0,36% na varredura maior. É uma proteção para casos extremos, não uma mudança perceptível na maior parte das partidas.
4. **Base desatualizada.** O `sketch.ino` do pacote parte do sketch do Agente USB **anterior** às correções da revisão r2 da v0.9: mensagem do servo, avisos de painel ausente e falha do servo, reenvio dos logs. Se for integrado, os 3 trechos devem ser reaplicados sobre a r2, sem substituir o arquivo inteiro.
5. **Log.** A linha `GUARDIA,adaptacao` (`nivel_mlp`, `nivel_final`, `limitou`, `banco_sem_candidato`) é útil e já apareceria na área de logs do painel.

## 5. Recomendação

Não integrar a v1 como está. Antes:
1. corrigir o fallback (seção 3), com teste de banco com lacunas;
2. decidir onde a política fica definida (seção 4, item 1);
3. reaplicar os 3 trechos sobre a r2.
