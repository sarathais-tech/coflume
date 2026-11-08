# Coflume v0.9 EXPERIMENTAL r4: banco de 36 perguntas e painel com monitor da IA

> **Versão experimental, ainda não testada na placa física.** Revisão `v0.9-agente-usb-experimental-r4`. Ela integra o trabalho revisado até 08/11/2026:
> - **r3**, que continua preservada em [`../v0.9-r3`](../v0.9-r3/);
> - o **banco de 36 perguntas** (`banco_v09`);
> - o **painel USB** com o quadro da Guardiã, a rede neural com cálculo reproduzido no painel e o **monitor da IA** para um segundo monitor.
>
> A **política gradual continua desligada por padrão** (`GUARDIA_POLITICA_GRADUAL 0`, fluxo-0.8).
>
> - **Guardiã IA:** a MLP é a mesma da v0.8, `mlp8-sint-20261101-e6a4cf00`, treinada com **dados sintéticos**. Não foi validada com crianças reais e não tem eficácia educacional comprovada.
> - **Versões anteriores preservadas:** v0.7, v0.8, r2 e r3 não mudam; os testes conferem os hashes. A fase C e o teste lacrado do coflume-guardian-ai não foram tocados.

Guias:
- montar, instalar e gravar: [`docs/guia_iniciantes.md`](../../docs/guia_iniciantes.md);
- usar (quizzes, Agente Secreto, música, USB, dois monitores e demonstração): [`docs/guia_de_uso.md`](../../docs/guia_de_uso.md);
- situação dos testes: [`docs/testes/coflume-v0.9-r4.md`](../../docs/testes/coflume-v0.9-r4.md);
- o que falta documentar da montagem: [`docs/montagem_pendencias.md`](../../docs/montagem_pendencias.md).

## 1. O que muda em relação à r3

| Parte | r3 | r4 |
|---|---|---|
| Banco de perguntas | `banco_v08`: 13 perguntas | **`banco_v09`: 36 perguntas, 3 por nível × categoria** (revisão em `docs/banco_v09_proposta.md` do coflume-guardian-ai) |
| Linha de boot | `COFLUME,versao=…,banco=banco_v08,perguntas=13` | `COFLUME,versao=v0.9-agente-usb-experimental-r4,banco=banco_v09,perguntas=36,banco_csv_sha256=<sha256 do CSV>` |
| Versão e hash do banco | só no `coflume_banco.h` | também em [`BANCO_FIRMWARE.json`](BANCO_FIRMWARE.json), separado do `ORIGEM_GUARDIA.json` da v0.8 (origem histórica do modelo, que **não muda**) |
| Painel USB | logs da Guardiã | logs, **quadro da Guardiã**, **rede neural** (cálculo reproduzido no painel), **monitor da IA** e **reprodução de logs de demonstração** ([`painel/README.md`](painel/README.md)) |
| Política gradual | opcional, desligada | igual (opcional, desligada) |
| Guardiã, política fluxo-0.8, Agente Secreto, tela, servo | — | **idênticos à r3** (conferido por teste) |

No `.ino` mudam só 2 linhas: a versão e a linha de boot. O diff completo está em [`docs/diferencas_r4_vs_r3.diff`](docs/diferencas_r4_vs_r3.diff).

## 2. Arquivos

```
firmware/v0.9-r4/
  coflume_v09/                 ← abrir no Arduino IDE (coflume_v09.ino e os .h)
    coflume_banco.h            gerado de ferramentas/banco_v09.csv (não editar à mão)
  BANCO_FIRMWARE.json          versão, hash do CSV e do .h do banco
  ferramentas/
    banco_v09.csv              cópia de questoes/banco_v09.csv do coflume-guardian-ai
    gerar_banco_v09.py         gera coflume_banco.h e BANCO_FIRMWARE.json (--verificar confere)
    renderizar_telas.py        imagens das telas a partir dos testes
  painel/
    Coflume_Agente_Secreto_USB.html   painel do notebook (Chrome ou Edge)
    LEIA_PRIMEIRO.txt          instruções do apresentador
    demo/                      logs de DEMONSTRAÇÃO (firmware r4 em simulação no PC)
    ferramentas/               fontes do painel e geradores (montar_painel, extrair_modelo, gerar_demos)
    docs/                      monitor da IA, avaliação da rede e capturas
    testes/                    testes do painel no Chromium
  docs/                        diff r4 × r3, telas da r4, banco (antes/depois)
  testes/                      testes do firmware no PC (herdados da r2/r3 e adaptados)
```

### Hashes (SHA-256) do sketch

| Arquivo | SHA-256 |
|---|---|
| `coflume_banco.h` | ver `BANCO_FIRMWARE.json` (`cabecalho.sha256`) |
| `guardia_modelo.h` | `9a98304baaf92bdc508a94a4861bf73bbbd4267da8ed34532f63e251693bc42e` (igual à v0.8) |
| `guardia_runtime.h` | `79b3d9a98eccc25ec76fcc383a53a73539ca5239971ac25ef0d7daa36eb112c0` (igual à v0.8) |

## 3. Compilar e gravar

Configuração da placa e bibliotecas iguais às da r2/r3. Passo a passo no [guia para iniciantes](../../docs/guia_iniciantes.md), seções 6 a 8.

**Compilação feita em 08/11/2026** com Arduino-ESP32 3.3.12, `FlashSize=16M,PSRAM=opi` e `--warnings all`. Bibliotecas: Keypad 3.1.1, Adafruit GFX 1.13.0, Adafruit ST7735 and ST7789 1.12.0, Adafruit BusIO 1.17.4. Nenhum aviso.

| Política gradual | Programa | RAM global |
|---|---|---|
| Desligada (padrão) | 378.487 bytes (28%) | 26.292 bytes (8%) |
| Ligada | 380.671 bytes (29%) | 26.340 bytes (8%) |

## 4. Testes automatizados

A partir da raiz do repositório `coflume`:

```bash
python3 -m pytest -q firmware/v0.9-r4/testes                    # firmware no PC (g++)
cd firmware/v0.9-r4/painel && python3 -m pytest -q testes       # painel no Chromium (playwright)
```

Resultados de 08/11/2026:
- **firmware:** 26 de 26 aprovados;
- **painel:** 28 de 28 aprovados.

As listas completas, e o que ainda falta testar na placa, estão em [`docs/testes/coflume-v0.9-r4.md`](../../docs/testes/coflume-v0.9-r4.md).

## 5. Limitações

- **Placa física:** **não testada** com a r4. Há testes físicos da **r3**, com a política gradual desligada, relatados pela responsável: quiz com 2/3 e 0/3, e Agente Secreto. Eles não cobrem o banco de 36 perguntas, a política gradual ligada nem o painel novo. Ver a seção 2.1 do [relatório de testes](../../docs/testes/coflume-v0.9-r4.md).
- **Modelo:** experimental, com dados sintéticos. O teste lacrado (fase C) não foi aberto, e a latência na placa física continua pendente.
- **Níveis das perguntas novas:** são estimativas de quem escreveu, não medições.
- **Dois monitores:** testados no Chromium com HTML local; o uso real com dois monitores físicos está pendente.
- **USB:** a porta é uma só. Com o painel conectado, o Monitor Serial da IDE não abre; os logs ficam no painel.
