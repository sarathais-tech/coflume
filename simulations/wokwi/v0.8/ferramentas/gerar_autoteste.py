"""Gera guardia_autoteste.h: subconjunto representativo dos 400 vetores da bancada.

Uso (a partir de simulations/wokwi/v0.8/, requer g++):
    python3 ferramentas/gerar_autoteste.py
    python3 ferramentas/gerar_autoteste.py --verificar

Os 400 vetores completos (testes/vetores_teste.h, cópia da bancada aprovada no
Wokwi) continuam nos testes automatizados. O firmware jogável roda ao ligar só
este subconjunto, escolhido de forma determinística:

- grupo (aleatórios da bancada / tirados da validação)
  × tamanho do histórico (0 = partida a frio, 1, 2, 3 ou mais)
  × nível candidato (0, 1, 2):
  o PRIMEIRO vetor de cada combinação, na ordem do arquivo.
- depois, se alguma categoria ainda não apareceu, o primeiro vetor dela.

Os valores (histórico, entrada esperada, P esperada) são copiados sem nenhuma
conta (só as respostas válidas do histórico; as posições livres ficam zeradas): um programa C++ lê vetores_teste.h e imprime os escolhidos com 9
algarismos significativos, o que reproduz exatamente cada float32.
"""

from __future__ import annotations

import argparse
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

PASTA = Path(__file__).resolve().parents[1]
SAIDA = PASTA / "guardia_autoteste.h"

SELETOR = r"""
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "guardia_modelo.h"
#include "guardia_runtime.h"
#include "vetores_teste.h"

static void f(float v) {
  char b[32];
  std::snprintf(b, sizeof(b), "%.9g", (double)v);
  std::printf("%s%sf", b, std::strpbrk(b, ".e") ? "" : ".0");
}

int main() {
  bool escolhido[GUARDIA_N_VETORES] = {false};
  bool visto[2][4][3] = {};
  bool cat_vista[4] = {};
  for (int k = 0; k < GUARDIA_N_VETORES; ++k) {
    const GuardiaVetorTeste& v = GUARDIA_VETORES[k];
    const int g = k < GUARDIA_N_VETORES_ALEATORIOS ? 0 : 1;
    const int b = v.n_historico >= 3 ? 3 : v.n_historico;
    if (!visto[g][b][v.nivel_candidato]) {
      visto[g][b][v.nivel_candidato] = true;
      escolhido[k] = true;
      cat_vista[v.categoria] = true;
    }
  }
  for (int k = 0; k < GUARDIA_N_VETORES; ++k)
    if (!cat_vista[GUARDIA_VETORES[k].categoria]) {
      cat_vista[GUARDIA_VETORES[k].categoria] = true;
      escolhido[k] = true;
    }
  for (int k = 0; k < GUARDIA_N_VETORES; ++k) {
    if (!escolhido[k]) continue;
    const GuardiaVetorTeste& v = GUARDIA_VETORES[k];
    std::printf("  { %d, {", k);
    for (int i = 0; i < (v.n_historico > 0 ? v.n_historico : 1); ++i) {
      const guardia::Resposta& r = v.historico[i];
      std::printf("%s{%d, %d, %s, ", i ? ", " : "", r.categoria, r.nivel, r.acertou ? "true" : "false");
      f(r.tempo_gasto_s); std::printf(", "); f(r.tempo_limite_s);
      std::printf(", %s}", r.usou_dica ? "true" : "false");
    }
    std::printf("}, %d, %d, %d, %d,\n    {%d, %d, %d, %d, ", v.n_historico, v.nivel_inicial, v.categoria,
                v.nivel_candidato, v.entrada_esperada.nivel_candidato, v.entrada_esperada.nivel_inicial,
                v.entrada_esperada.n_respondidas, v.entrada_esperada.categoria);
    f(v.entrada_esperada.taxa_acerto_sessao); std::printf(", ");
    f(v.entrada_esperada.taxa_acerto_categoria); std::printf(", ");
    f(v.entrada_esperada.tempo_medio_norm); std::printf(", ");
    f(v.entrada_esperada.dicas_por_questao);
    std::printf(", %d, %d}, ", v.entrada_esperada.resultado_ultima, v.entrada_esperada.sequencia);
    f(v.p_esperado);
    std::printf(" },\n");
  }
  return 0;
}
"""


def selecionar() -> str:
    gpp = shutil.which("g++")
    if not gpp:
        raise SystemExit("g++ não encontrado")
    with tempfile.TemporaryDirectory() as tmp:
        fonte, exe = Path(tmp) / "seletor.cpp", Path(tmp) / "seletor"
        fonte.write_text(SELETOR, encoding="utf-8")
        subprocess.run([gpp, "-std=c++17", "-O0", f"-I{PASTA}", f"-I{PASTA / 'testes'}", str(fonte), "-o", str(exe)],
                       check=True)
        return subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout


def gerar() -> str:
    corpo = selecionar()
    n = corpo.count("  { ")
    return "\n".join([
        "// =============================================================================",
        "//  guardia_autoteste.h — autoteste de inicialização da Guardiã IA (Coflume v0.8)",
        "// ARQUIVO GERADO por ferramentas/gerar_autoteste.py — NÃO EDITAR À MÃO.",
        "//  Subconjunto representativo de testes/vetores_teste.h (400 vetores da",
        "//  bancada aprovada no Wokwi). Critério de escolha: ver o gerador.",
        "// =============================================================================",
        "#pragma once",
        "#include <stdint.h>",
        "",
        '#define GUARDIA_AUTOTESTE_MODELO_ID "mlp8-sint-20261101-e6a4cf00"',
        f"#define GUARDIA_AUTOTESTE_N {n}",
        "#define GUARDIA_AUTOTESTE_HIST_MAX 8",
        "",
        "struct GuardiaAutoteste {",
        "  int16_t indice_original;  // posição em testes/vetores_teste.h",
        "  guardia::Resposta historico[GUARDIA_AUTOTESTE_HIST_MAX];",
        "  uint8_t n_historico;",
        "  uint8_t nivel_inicial, categoria, nivel_candidato;",
        "  guardia::Entrada entrada_esperada;",
        "  float p_esperado;",
        "};",
        "",
        "const GuardiaAutoteste GUARDIA_AUTOTESTE[GUARDIA_AUTOTESTE_N] = {",
        corpo.rstrip("\n"),
        "};",
        "",
    ])


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--verificar", action="store_true")
    args = ap.parse_args(argv)
    texto = gerar()
    if args.verificar:
        if not SAIDA.exists() or SAIDA.read_text(encoding="utf-8") != texto:
            print("guardia_autoteste.h desatualizado; rode ferramentas/gerar_autoteste.py", file=sys.stderr)
            return 1
        print("guardia_autoteste.h confere com testes/vetores_teste.h")
        return 0
    SAIDA.write_text(texto, encoding="utf-8")
    print(f"Gerado {SAIDA}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
