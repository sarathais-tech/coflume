"""banco_v09 na r4: manifesto, cabeçalho gerado, conteúdo e histórico preservado.

Executar a partir de firmware/v0.9-r4/:
    python3 -m pytest -q testes/test_banco_v09.py

Layout das 196 telas e partidas com o banco_v09 ficam em test_v09.py e test_r3.py, com o sketch da r4.
"""

from __future__ import annotations

import csv
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from collections import Counter
from pathlib import Path

import pytest

PROPOSTA = Path(__file__).resolve().parents[1]
FIRMWARE = PROPOSTA.parent
R3 = FIRMWARE / "v0.9-r3"
V08 = FIRMWARE.parent / "simulations" / "wokwi" / "v0.8"
CSV = PROPOSTA / "ferramentas" / "banco_v09.csv"
CAB = PROPOSTA / "coflume_v09" / "coflume_banco.h"
MAN = PROPOSTA / "BANCO_FIRMWARE.json"
SAIDA_TELAS = os.environ.get("COFLUME_TELAS_SAIDA")  # opcional: pasta para guardar telas.jsonl


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


@pytest.fixture(scope="session")
def gpp() -> str:
    g = shutil.which("g++")
    if not g:
        pytest.skip("g++ não encontrado")
    return g


def sketch_com_banco(tmp: Path) -> Path:
    raiz = tmp / "sketch"
    shutil.copytree(R3 / "coflume_v09", raiz / "coflume_v09")
    shutil.copytree(R3 / "testes", raiz / "testes")
    shutil.copy(CAB, raiz / "coflume_v09" / "coflume_banco.h")
    return raiz


def compilar_e_rodar(gpp, raiz: Path, fonte: str, defs: list[str], args: list[str]) -> str:
    exe = raiz / ("exe_" + "_".join(d.split("=")[-1] for d in defs) + Path(fonte).stem)
    r = subprocess.run([gpp, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", *defs,
                        f"-I{raiz / 'testes' / 'stub'}", f"-I{raiz / 'coflume_v09'}", "-x", "c++",
                        str(raiz / "testes" / fonte), "-o", str(exe)], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr[-4000:]
    r = subprocess.run([str(exe), *args], capture_output=True, text=True, timeout=900)
    print(r.stdout[-1500:])
    assert r.returncode == 0, r.stdout[-4000:]
    return r.stdout


def test_cabecalho_e_manifesto_conferem_com_o_csv():
    r = subprocess.run([sys.executable, str(PROPOSTA / "ferramentas" / "gerar_banco_v09.py"), "--verificar"],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    m = json.loads(MAN.read_text(encoding="utf-8"))
    cab = CAB.read_text(encoding="utf-8")
    assert m["banco_versao"] == "banco_v09" and '#define COFLUME_BANCO_VERSAO "banco_v09"' in cab
    assert m["csv"]["sha256"] == sha(CSV) and f'#define COFLUME_BANCO_CSV_SHA256 "{sha(CSV)}"' in cab
    assert m["cabecalho"]["sha256"] == sha(CAB)
    assert m["perguntas"] == 36 and m["perguntas_por_celula_min"] == 3 and "#define COFLUME_BANCO_N 36" in cab


def test_csv_coerente():
    linhas = list(csv.DictReader(CSV.open(encoding="utf-8")))
    assert len(linhas) == 36 and len({r["id"] for r in linhas}) == 36 and len({r["enunciado"] for r in linhas}) == 36
    assert Counter(r["correta"] for r in linhas) == Counter({"A": 9, "B": 9, "C": 9, "D": 9})
    assert Counter((r["nivel"], r["categoria"]) for r in linhas) == Counter({(str(n), str(c)): 3 for n in range(3) for c in range(4)})
    for r in linhas:
        alts = [r[f"alt_{x}"] for x in "abcd"]
        assert len(set(alts)) == 4, r["id"]


def test_historico_preservado():
    """banco_v08 e ORIGEM_GUARDIA.json continuam como estavam; r3 continua com banco_v08."""
    origem = json.loads((V08 / "ORIGEM_GUARDIA.json").read_text(encoding="utf-8"))
    assert sha(V08 / "ferramentas" / "banco_v08.csv") == origem["arquivos"]["ferramentas/banco_v08.csv"]["sha256"]
    assert sha(V08 / "ferramentas" / "banco_v08.csv") == "5e9c14e22ae5dd1d4af3f494038a4bb1e8ba41c6839b7847513fc80134d06daa"
    assert "banco_v09" not in (V08 / "ORIGEM_GUARDIA.json").read_text(encoding="utf-8")
    assert '"banco_v08"' in (R3 / "coflume_v09" / "coflume_banco.h").read_text(encoding="utf-8")
    # o CSV da r4 é o mesmo de questoes/banco_v09.csv do coflume-guardian-ai (conferido na consolidação)
