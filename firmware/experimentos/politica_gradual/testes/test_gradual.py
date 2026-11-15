"""Política gradual (experimento): C++ contra a referência Python, com a MLP real.

Executar a partir da raiz do repositório:
    python3 -m pytest -q firmware/experimentos/politica_gradual/testes

Opcional: COFLUME_GUARDIAN_AI=<coflume-guardian-ai na branch exp/politica-gradual>
regenera a referência e confere que ela é idêntica à versionada.
"""

from __future__ import annotations

import gzip
import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

PASTA = Path(__file__).resolve().parents[1]
V09 = PASTA.parents[1] / "v0.9" / "coflume_v09"
REF = PASTA / "testes" / "referencia_gradual.csv.gz"


@pytest.fixture(scope="session")
def gpp() -> str:
    g = shutil.which("g++")
    if not g:
        pytest.skip("g++ não encontrado")
    return g


def test_cpp_igual_a_referencia_python(gpp, tmp_path):
    csv = tmp_path / "ref.csv"
    csv.write_bytes(gzip.decompress(REF.read_bytes()))
    exe = tmp_path / "tg"
    r = subprocess.run([gpp, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", f"-I{PASTA}", f"-I{V09}",
                        str(PASTA / "testes" / "teste_gradual.cpp"), "-o", str(exe)], capture_output=True, text=True)
    assert r.returncode == 0, r.stderr[-4000:]
    r = subprocess.run([str(exe), str(csv)], capture_output=True, text=True, timeout=600)
    print(r.stdout)
    assert r.returncode == 0 and "0 falhas" in r.stdout and "49536 partidas" in r.stdout, r.stdout[-4000:]


def test_referencia_reproduzivel(tmp_path):
    raiz = os.environ.get("COFLUME_GUARDIAN_AI")
    if not raiz:
        pytest.skip("defina COFLUME_GUARDIAN_AI para regenerar a referência")
    copia = tmp_path / "copia.csv.gz"
    shutil.copy(REF, copia)
    try:
        r = subprocess.run([sys.executable, str(PASTA / "ferramentas" / "gerar_referencia_gradual.py"),
                            "--guardian-ai", raiz], capture_output=True, text=True)
        assert r.returncode == 0, r.stderr
        assert gzip.decompress(REF.read_bytes()) == gzip.decompress(copia.read_bytes())
    finally:
        shutil.copy(copia, REF)


def test_r2_continua_com_o_fluxo08():
    """O experimento não entra no firmware r2."""
    s = (V09 / "coflume_v09.ino").read_text(encoding="utf-8")
    assert "coflume_politica_gradual.h" not in s and "gradual::" not in s
    assert "coflume::decidir_pergunta(" in s
