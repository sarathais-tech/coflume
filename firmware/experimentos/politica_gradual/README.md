# Experimento: política adaptativa gradual (`gradual-v2-exp`), porta em C++

> **Não integrado.** O firmware v0.9 r2 (`firmware/v0.9`) continua com o fluxo-0.8, e um teste confere isso.
> A política está definida na referência em Python do `coflume-guardian-ai`:
> - `guardia/politica_gradual.py`;
> - `docs/politica_gradual_exp.md`, com as regras, os fallbacks, os resultados e os próximos passos.
>
> É uma **proteção** da escolha de dificuldade. Não melhora a precisão da rede neural e não comprova efeito na aprendizagem. Resultados **[SINTÉTICO]**.

## Arquivos

| Arquivo | Papel |
|---|---|
| `coflume_politica_gradual.h` | Política gradual em C++, sem Arduino e sem alocação. Usa `coflume_politica.h` da v0.9 e recebe o banco por função, o que permite testar bancos com lacunas |
| `ferramentas/gerar_referencia_gradual.py` | Gera `testes/referencia_gradual.csv.gz` com a referência Python e a MLP real (precisa do `coflume-guardian-ai`) |
| `testes/teste_gradual.cpp` | Regras com bancos com lacunas e as **mesmas 49.536 partidas** da referência Python, com o runtime embarcado |
| `testes/test_gradual.py` | Executa o teste C++ e confere que a r2 não usa o experimento |

## Executar

```bash
python3 -m pytest -q firmware/experimentos/politica_gradual/testes
# opcional: regenera e compara a referência
COFLUME_GUARDIAN_AI=/caminho/coflume-guardian-ai python3 -m pytest -q firmware/experimentos/politica_gradual/testes
```

## Resultados (07/11/2026)

- **Python × C++:**
  - 49.536 partidas e 144.706 decisões **iguais** (categoria, nível, motivo e escolha sem a regra);
  - 3.710 recusas `sem_nivel_compativel` iguais, todas em bancos inviáveis;
  - viabilidade dos 33 bancos igual;
  - maior diferença de P(acerto) de 1,19e-7.
- **Regras em C++** (400 bancos com lacunas e previsões aleatórias, inclusive inválidas, além de partidas sem modelo):
  - a regra nunca foi violada;
  - só houve recusa em bancos declarados inviáveis;
  - rodou também com AddressSanitizer e UndefinedBehaviorSanitizer, sem erros.
- **Efeito no banco completo:**
  - 0 de 3.456 decisões alteradas nos cenários de referência;
  - 448 de 124.416 (0,36%) na varredura ampla, todas na pergunta 3: 330 subidas depois de erro e 118 saltos de 2 níveis impedidos.
