"""r4 (herdado da r3): política gradual atrás de GUARDIA_POLITICA_GRADUAL (desligada por padrão).

Executar a partir da raiz do repositório:
    python3 -m pytest -q firmware/v0.9-r4/testes

Os testes da r2 (test_v09.py, test_painel.py) rodam nesta pasta com a opção
DESLIGADA. Este arquivo cobre a opção LIGADA: partidas completas, bancos com
lacunas (trocando coflume_banco.h numa cópia do sketch), recusa explícita e
alternância com o Agente Secreto.
"""

from __future__ import annotations

import hashlib
import re
import shutil
import subprocess
from pathlib import Path

import pytest

R3 = Path(__file__).resolve().parents[1]
SKETCH = R3 / "coflume_v09"
TESTES = R3 / "testes"
R2 = R3.parent / "v0.9"
R3_ANTERIOR = R3.parent / "v0.9-r3"  # (a variável R3 aponta para a pasta desta revisão, r4)
EXPERIMENTO = R3.parent / "experimentos" / "politica_gradual" / "coflume_politica_gradual.h"
ENTRADA = re.compile(r'  \{"([^"]+)", (\d), (\d),.*?\n.*?\},\n')


@pytest.fixture(scope="session")
def gpp() -> str:
    g = shutil.which("g++")
    if not g:
        pytest.skip("g++ não encontrado")
    return g


def banco_sem(celulas: set[tuple[int, int]]) -> str:
    """coflume_banco.h sem as perguntas das células (nível, categoria) indicadas."""
    t = (SKETCH / "coflume_banco.h").read_text(encoding="utf-8")
    removidas = [m.group(0) for m in ENTRADA.finditer(t) if (int(m.group(2)), int(m.group(3))) in celulas]
    assert removidas, celulas
    for r in removidas:
        t = t.replace(r, "", 1)
    n = int(re.search(r"#define COFLUME_BANCO_N (\d+)", t).group(1))
    return t.replace(f"#define COFLUME_BANCO_N {n}", f"#define COFLUME_BANCO_N {n - len(removidas)}")


def rodar(gpp: str, tmp: Path, cenario: str, gradual: int, sem: set | None = None) -> str:
    pasta = tmp / "sketch" / "coflume_v09"
    shutil.copytree(SKETCH, pasta)
    shutil.copytree(TESTES / "stub", tmp / "sketch" / "stub")
    (tmp / "sketch" / "testes").mkdir()
    shutil.copy(TESTES / "teste_gradual_jogo.cpp", tmp / "sketch" / "testes")
    if sem:
        (pasta / "coflume_banco.h").write_text(banco_sem(sem), encoding="utf-8")
    exe = tmp / "jogo"
    r = subprocess.run([gpp, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", f"-DGUARDIA_POLITICA_GRADUAL={gradual}",
                        f"-I{tmp / 'sketch' / 'stub'}", f"-I{pasta}", "-x", "c++",
                        str(tmp / "sketch" / "testes" / "teste_gradual_jogo.cpp"), "-o", str(exe)],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr[-4000:]
    r = subprocess.run([str(exe), cenario], capture_output=True, text=True, timeout=600)
    print(r.stdout)
    assert r.returncode == 0 and "0 falhas" in r.stdout, r.stdout[-4000:]
    return r.stdout


SEM_DESAFIANTE_POR = {(1, 1)}                 # uma lacuna: viável nas duas políticas
SEM_DESAFIANTE_POR_CG = {(1, 1), (1, 3)}      # viável no fluxo-0.8, inviável na gradual


def test_gradual_banco_completo_partidas_e_agente(gpp, tmp_path):
    saida = rodar(gpp, tmp_path, "completo", 1)
    assert "partidas=150 recusas=0" in saida


def test_gradual_banco_com_uma_lacuna(gpp, tmp_path):
    saida = rodar(gpp, tmp_path, "lacuna_viavel", 1, SEM_DESAFIANTE_POR)
    assert "partidas=150 recusas=0" in saida


def test_gradual_banco_inviavel_bloqueia_o_quiz(gpp, tmp_path):
    rodar(gpp, tmp_path, "lacuna_inviavel", 1, SEM_DESAFIANTE_POR_CG)


def test_desligada_mesmo_banco_joga_com_fluxo08(gpp, tmp_path):
    rodar(gpp, tmp_path, "lacuna_inviavel_off", 0, SEM_DESAFIANTE_POR_CG)


def test_gradual_recusa_explicita_em_jogo(gpp, tmp_path):
    saida = rodar(gpp, tmp_path, "recusa_defensiva", 1, SEM_DESAFIANTE_POR_CG)
    assert re.search(r"recusas=([5-9]|\d\d+)", saida)


def test_padrao_desligado_e_nucleo_identico():
    s = (SKETCH / "coflume_v09.ino").read_text(encoding="utf-8")
    assert re.findall(r"^#define GUARDIA_POLITICA_GRADUAL (\d)", s, re.M) == ["0"]
    assert re.findall(r"^#define GUARDIA_ADAPTATIVA (\d)", s, re.M) == ["1"]
    assert (SKETCH / "coflume_politica_gradual.h").read_bytes() == EXPERIMENTO.read_bytes()
    # r4: só o banco (banco_v09) e o .ino (versão e hash do banco no boot) mudam em relação à r3.
    for nome in ("guardia_modelo.h", "guardia_runtime.h", "guardia_autoteste.h", "coflume_politica.h",
                 "coflume_tela.h", "coflume_agente.h", "coflume_politica_gradual.h"):
        assert (SKETCH / nome).read_bytes() == (R3_ANTERIOR / "coflume_v09" / nome).read_bytes(), nome
    assert '#define COFLUME_BANCO_VERSAO "banco_v09"' in (SKETCH / "coflume_banco.h").read_text(encoding="utf-8")
    ino_r3 = (R3_ANTERIOR / "coflume_v09" / "coflume_v09.ino").read_text(encoding="utf-8").splitlines()
    ino_r4 = (SKETCH / "coflume_v09.ino").read_text(encoding="utf-8").splitlines()
    mudadas = [(a, b) for a, b in zip(ino_r3, ino_r4) if a != b]
    assert len(ino_r3) == len(ino_r4) and len(mudadas) == 2
    assert "experimental-r4" in mudadas[0][1] and "banco_csv_sha256=%s" in mudadas[1][1]


def test_nunca_volta_ao_fluxo08_quando_ligada():
    """Com a opção ligada, a decisão gradual sem candidato vai para a recusa, nunca para decidir_pergunta."""
    s = (SKETCH / "coflume_v09.ino").read_text(encoding="utf-8")
    i = s.index("void prepararPergunta(){")
    corpo = s[i: s.index("\n}", i)]
    ramo_gradual = corpo[corpo.index("if(GUARDIA_POLITICA_GRADUAL){"): corpo.index("}else{")]
    assert "coflume::decidir_pergunta(" not in ramo_gradual
    assert 'if(!decisaoGradual.viavel){erroBanco("sem_nivel_compativel");return;}' in ramo_gradual
    assert "if(GUARDIA_POLITICA_GRADUAL&&!bancoGradualOK){erroBanco(\"banco_inviavel_gradual\");return;}" in s


def test_r2_preservada():
    """A pasta da r2 não foi alterada pela r3 (hashes registrados no início da r3)."""
    esperado = {}
    for linha in (TESTES / "r2_sha256.txt").read_text(encoding="utf-8").splitlines():
        h, nome = linha.split("  ", 1)
        esperado[nome] = h
    atual = {str(p.relative_to(R2)): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in R2.rglob("*") if p.is_file() and "__pycache__" not in p.parts}
    assert atual == esperado


def test_r3_preservada():
    """A pasta da r3 não foi alterada pela r4 (hashes registrados ao criar a r4)."""
    esperado = {}
    for linha in (TESTES / "r3_sha256.txt").read_text(encoding="utf-8").splitlines():
        h, nome = linha.split("  ", 1)
        esperado[nome] = h
    atual = {str(p.relative_to(R3_ANTERIOR)): hashlib.sha256(p.read_bytes()).hexdigest()
             for p in R3_ANTERIOR.rglob("*") if p.is_file() and "__pycache__" not in p.parts and ".pytest_cache" not in p.parts}
    assert atual == esperado
