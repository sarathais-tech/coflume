"""Gera os três logs de DEMONSTRAÇÃO com o firmware r4 real rodando no PC.

Uso (a partir de firmware/v0.9-r4/painel/):
    python3 ferramentas/gerar_demos.py

- demo/demo_fluxo08.txt  política fluxo-0.8 (GUARDIA_POLITICA_GRADUAL=0), banco atual
- demo/demo_gradual.txt  regra gradual ligada, banco atual; a proteção atua numa pergunta
- demo/demo_recusa.txt   regra gradual ligada, banco INVIÁVEL para ela (banco_v09 sem as
                         células Desafiante x Português e Desafiante x Conhecimentos gerais).
                         Parte 1: a verificação inicial bloqueia o Quiz. Parte 2: CENÁRIO
                         FORÇADO (bancoGradualOK=true) para testar a proteção defensiva em jogo.

A r4 não é alterada: o banco inviável é montado numa cópia temporária do sketch,
com a mesma regra de remoção usada em testes/test_r3.py.
"""
from __future__ import annotations

import re
import shutil
import subprocess
import tempfile
from pathlib import Path

PASTA = Path(__file__).resolve().parents[1]
R3 = PASTA.parent  # sketch e stubs da r4 (o nome da variável vem da r3)
ENTRADA = re.compile(r'  \{"([^"]+)", (\d), (\d),.*?\n.*?\},\n')  # igual a test_r3.py
SEM_DESAFIANTE_POR_CG = {(1, 1), (1, 3)}


def banco_sem(texto: str, celulas: set[tuple[int, int]]) -> str:
    removidas = [m.group(0) for m in ENTRADA.finditer(texto) if (int(m.group(2)), int(m.group(3))) in celulas]
    assert removidas
    for r in removidas:
        texto = texto.replace(r, "", 1)
    n = int(re.search(r"#define COFLUME_BANCO_N (\d+)", texto).group(1))
    return texto.replace(f"#define COFLUME_BANCO_N {n}", f"#define COFLUME_BANCO_N {n - len(removidas)}")


def compilar(tmp: Path, gradual: int, sketch: Path | None = None) -> Path:
    exe = tmp / f"gerar_demo_{gradual}_{'copia' if sketch else 'r3'}"
    extras = [f'-DCOFLUME_SKETCH="{sketch / "coflume_v09.ino"}"', f"-I{sketch}"] if sketch else []
    cmd = ["g++", "-std=c++17", "-O2", "-Wall", "-Wextra", f"-DGUARDIA_POLITICA_GRADUAL={gradual}", *extras,
           f"-I{R3 / 'testes' / 'stub'}", f"-I{R3 / 'coflume_v09'}", "-x", "c++",
           str(PASTA / "ferramentas" / "gerar_demo.cpp"), "-o", str(exe)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr[-3000:]
    return exe


def rodar(exe: Path, saida: str, roteiro: str) -> None:
    r = subprocess.run([str(exe), str(PASTA / "demo" / saida), roteiro], capture_output=True, text=True, timeout=600)
    print(saida, "-", r.stderr.strip().replace("\n", "; "))
    assert r.returncode == 0


def main() -> None:
    (PASTA / "demo").mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory() as t:
        tmp = Path(t)
        rodar(compilar(tmp, 0), "demo_fluxo08.txt", "fluxo08")
        rodar(compilar(tmp, 1), "demo_gradual.txt", "gradual")
        copia = tmp / "sketch_banco_inviavel" / "coflume_v09"
        shutil.copytree(R3 / "coflume_v09", copia)
        cab = copia / "coflume_banco.h"
        cab.write_text(banco_sem(cab.read_text(encoding="utf-8"), SEM_DESAFIANTE_POR_CG), encoding="utf-8")
        rodar(compilar(tmp, 1, copia), "demo_recusa.txt", "recusa")


if __name__ == "__main__":
    main()
