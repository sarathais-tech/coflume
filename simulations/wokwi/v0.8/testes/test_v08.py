"""Testes automatizados da v0.8 EXPERIMENTAL do Coflume (Guardiã IA no Wokwi).

Executar a partir da raiz do repositório:
    python3 -m pytest -q simulations/wokwi/v0.8/testes

Requisitos: Python 3.10+, pytest, g++ (C++17).
Teste de renderização dos acentos: precisa do glcdfont.c da Adafruit GFX
(variável GFX_GLCDFONT ou biblioteca instalada no sketchbook do Arduino).
"""

from __future__ import annotations

import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

V08 = Path(__file__).resolve().parents[1]
V07 = V08.parent / "v0.7"
RAIZ = V08.parents[2]
TESTES = V08 / "testes"
sys.path.insert(0, str(V08 / "ferramentas"))
import gerar_banco  # noqa: E402

# SHA-256 da v0.7 em main (commit e20778c), registrados antes da criação da v0.8.
SHA_V07 = {
    "diagram.json": "ff8ec751816787692dc1b007a78f6c33d9dc2c1e21304f3074bac4197260b3af",
    "libraries.txt": "12dbcb573d3b2c58fcc858a8b06ba19f1322c0c0e185fc38306b63006e3b6854",
    "sketch.ino": "c10f16f74d296b1ee872b0321bda43532fe0ae672e75aa789f53f8b552a2aa42",
}
MODELO_ID = "mlp8-sint-20261101-e6a4cf00"
MODELO_HASH = "e6a4cf0086ccf5ab1eccecbe7aea02f3962a6ad7ced29b136d36c5b37b5b9991"
FONTES_FIRMWARE = ("sketch.ino", "coflume_politica.h", "coflume_banco.h", "guardia_autoteste.h",
                   "guardia_modelo.h", "guardia_runtime.h")


def sha(p: Path) -> str:
    return hashlib.sha256(p.read_bytes()).hexdigest()


@pytest.fixture(scope="session")
def gpp() -> str:
    g = shutil.which("g++")
    if not g:
        pytest.skip("g++ não encontrado")
    return g


def compilar_e_rodar(gpp: str, tmp: Path, fonte: Path, nome: str, extras: list[str], args: list[str] = ()) -> str:
    exe = tmp / nome
    r = subprocess.run([gpp, "-std=c++17", "-O2", "-Wall", "-Wextra", "-Werror", *extras, str(fonte), "-o", str(exe)],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr[-4000:]
    r = subprocess.run([str(exe), *args], capture_output=True, text=True, timeout=300)
    assert r.returncode == 0, r.stdout[-4000:]
    return r.stdout


# ---------------------------------------------------------------------------
# Modelo: pesos exatos, origem e identificação
# ---------------------------------------------------------------------------

def test_arquivos_da_guardia_conferem_com_a_origem():
    origem = json.loads((V08 / "ORIGEM_GUARDIA.json").read_text(encoding="utf-8"))
    assert origem["repositorio"] == "sarathais-tech/coflume-guardian-ai"
    assert origem["modelo_id"] == MODELO_ID and origem["modelo_hash_parametros_sha256"] == MODELO_HASH
    for nome, info in origem["arquivos"].items():
        assert sha(V08 / nome) == info["sha256"], nome


def test_pesos_identicos_ao_repositorio_da_guardia():
    """Comparação byte a byte com o coflume-guardian-ai, quando disponível localmente."""
    raiz = os.environ.get("COFLUME_GUARDIAN_AI")
    if not raiz:
        pytest.skip("defina COFLUME_GUARDIAN_AI para comparar com o repositório da Guardiã")
    origem = json.loads((V08 / "ORIGEM_GUARDIA.json").read_text(encoding="utf-8"))
    for nome, info in origem["arquivos"].items():
        assert (V08 / nome).read_bytes() == (Path(raiz) / info["origem"]).read_bytes(), nome


def test_modelo_identificado_e_rotulado():
    meta = json.loads((V08 / "MODELO.json").read_text(encoding="utf-8"))
    assert meta["modelo_id"] == MODELO_ID and meta["hash_sha256"] == MODELO_HASH
    assert meta["contrato"] == "1.0.0" and meta["fonte_dados"] == "SINTETICO"
    assert meta["arquitetura"]["ocultas"] == [8] and meta["arquitetura"]["n_parametros"] == 121
    cab = (V08 / "guardia_modelo.h").read_text(encoding="utf-8")
    assert f'#define GUARDIA_MODELO_ID "{MODELO_ID}"' in cab and f'#define GUARDIA_MODELO_HASH "{MODELO_HASH}"' in cab
    sketch = (V08 / "sketch.ino").read_text(encoding="utf-8")
    assert "EXPERIMENTAL" in sketch and "SINTETICOS" in sketch and "NAO e versao de producao" in sketch
    # A versão publicada usa a Guardiã adaptativa; 0 é só para teste de fallback.
    assert re.findall(r"^#define GUARDIA_ADAPTATIVA (\d)", sketch, re.M) == ["1"]


# ---------------------------------------------------------------------------
# Arquivos gerados: atualizados e fiéis às fontes
# ---------------------------------------------------------------------------

def test_banco_gerado_esta_atualizado():
    assert gerar_banco.main(["--verificar"]) == 0


def test_autoteste_gerado_esta_atualizado(gpp):
    r = subprocess.run([sys.executable, str(V08 / "ferramentas" / "gerar_autoteste.py"), "--verificar"],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr


def test_bytes_do_banco_no_firmware(gpp, tmp_path):
    """Cada texto do coflume_banco.h tem exatamente os bytes convertidos do CSV."""
    fonte = tmp_path / "bytes.cpp"
    fonte.write_text("""#include <cstdio>
#include "coflume_banco.h"
static void h(const char* s){for(;*s;++s)std::printf("%02x",(unsigned char)*s);std::printf("\\n");}
int main(){for(int i=0;i<COFLUME_BANCO_N;++i){const auto& q=coflume::BANCO[i];std::printf("%s %d %d %c\\n",q.id,q.nivel,q.categoria,q.correta);
h(q.enunciado);for(int a=0;a<4;++a)h(q.alt[a]);h(q.dica);}}""", encoding="utf-8")
    saida = compilar_e_rodar(gpp, tmp_path, fonte, "bytes", [f"-I{V08}"]).splitlines()
    linhas = gerar_banco.ler_banco(gerar_banco.CSV_PADRAO)
    assert len(saida) == 7 * len(linhas) == 7 * 13
    for k, r in enumerate(linhas):
        bloco = saida[7 * k: 7 * k + 7]
        assert bloco[0] == f"{r['id']} {r['nivel']} {r['categoria']} {r['correta']}"
        textos = [r["enunciado"], r["alt_a"], r["alt_b"], r["alt_c"], r["alt_d"], r["dica"]]
        assert bloco[1:] == [gerar_banco.para_fonte_gfx(t).hex() for t in textos]


def test_conversao_de_acentos():
    assert gerar_banco.para_fonte_gfx("potências") == b"pot\x88ncias"
    assert gerar_banco.para_fonte_gfx("Unica") == b"Unica"
    for proibido in ("ã", "õ", "Ã"):  # não existem no cp437: o gerador recusa
        with pytest.raises(ValueError):
            gerar_banco.para_fonte_gfx(proibido)


# ---------------------------------------------------------------------------
# Renderização na fonte clássica da Adafruit GFX
# ---------------------------------------------------------------------------

def _glcdfont() -> Path | None:
    candidatos = [os.environ.get("GFX_GLCDFONT", "")]
    for base in (Path.home() / "Arduino" / "libraries", Path(os.environ.get("ARDUINO_LIBRARIES", "/nao-existe"))):
        candidatos += [str(p) for p in base.glob("Adafruit*GFX*/glcdfont.c")] if base.exists() else []
    for c in candidatos:
        if c and Path(c).is_file():
            return Path(c)
    return None


def _fonte(caminho: Path) -> list[int]:
    t = caminho.read_text(encoding="utf-8", errors="replace")
    corpo = t[t.index("{", t.index("font[]")) + 1: t.index("};")]
    corpo = re.sub(r"/\*.*?\*/", "", re.sub(r"//.*", "", corpo), flags=re.S)
    b = [int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", corpo)]
    assert len(b) == 256 * 5, "glcdfont.c com 256 caracteres de 5 colunas"
    return b


def _glifo(fonte: list[int], codigo: int) -> list[str]:
    col = fonte[codigo * 5: codigo * 5 + 5]
    return ["".join("#" if col[i] >> r & 1 else "." for i in range(5)) for r in range(8)]


def renderizar(fonte: list[int], dados: bytes) -> list[str]:
    """Simula o drawChar da GFX (fonte clássica, cp437(false)) em texto ASCII."""
    linhas = [""] * 8
    for c in dados:
        if c >= 176:  # comportamento 'classic' da GFX sem cp437(true)
            c += 1
        g = _glifo(fonte, c)
        for r in range(8):
            linhas[r] += g[r] + "."
    return linhas


def test_renderizacao_dos_acentos():
    caminho = _glcdfont()
    if caminho is None:
        pytest.skip("glcdfont.c da Adafruit GFX não encontrado (defina GFX_GLCDFONT)")
    fonte = _fonte(caminho)
    # A fonte clássica segue o cp437 nas posições conhecidas.
    assert _glifo(fonte, ord("^"))[0] == "..#.." and _glifo(fonte, ord("e"))[2:6] == [".###.", "#...#", "#####", "#...."]
    usados = set()
    for r in gerar_banco.ler_banco(gerar_banco.CSV_PADRAO):
        for campo in ("enunciado", "alt_a", "alt_b", "alt_c", "alt_d", "dica"):
            for ch in r[campo]:
                if ord(ch) >= 128:
                    usados.add(ch)
    assert usados == {"ê"}, usados
    for ch in usados:
        codigo = ch.encode("cp437")[0]
        assert codigo < 176  # não depende de cp437(true)
        base = "e" if ch in "éêè" else ch
        g, gb = _glifo(fonte, codigo), _glifo(fonte, ord(base))
        assert g != gb, "o acento precisa aparecer"
        assert "#" in g[0] + g[1] and "#" not in gb[0] + gb[1], "acento desenhado acima da letra"
        assert g[2:6] == gb[2:6], "corpo da letra igual ao da letra sem acento"
    # Renderiza a dica que tem acento, como aparece na tela.
    texto = renderizar(fonte, gerar_banco.para_fonte_gfx("potências"))
    print("\n".join(texto))
    i = gerar_banco.para_fonte_gfx("potências").index(0x88) * 6  # 5 colunas + 1 de espaço por caractere
    assert [linha[i:i + 5] for linha in texto[:7]] == _glifo(fonte, 0x88)[:7]
    assert texto[0][i:i + 5] == "#####" and texto[1][i:i + 5] == "....."  # acento separado do corpo do 'e'


# ---------------------------------------------------------------------------
# Inferência, política e partida completa (C++ no PC)
# ---------------------------------------------------------------------------

def test_400_vetores_politica_e_referencia_python(gpp, tmp_path):
    saida = compilar_e_rodar(gpp, tmp_path, TESTES / "teste_politica.cpp", "politica", [f"-I{V08}", f"-I{TESTES}"],
                             [str(TESTES / "referencia_politica.csv")])
    print(saida)
    assert "0 falhas" in saida


@pytest.mark.parametrize("adaptativa", [1, 0])
def test_partida_completa(gpp, tmp_path, adaptativa):
    saida = compilar_e_rodar(gpp, tmp_path, TESTES / "teste_partida.cpp", f"partida{adaptativa}",
                             [f"-DGUARDIA_ADAPTATIVA={adaptativa}", f"-I{TESTES / 'stub'}", f"-I{V08}", "-x", "c++"])
    print(saida)
    assert "0 falhas" in saida


# ---------------------------------------------------------------------------
# Segurança, funcionamento offline e preservação da v0.7
# ---------------------------------------------------------------------------

def test_sem_dependencia_de_internet():
    proibidos = re.compile(r"\b(WiFi\w*|HTTPClient|esp_wifi\w*|esp_http\w*|WebServer|BLE\w*|BluetoothSerial|PubSubClient|AsyncTCP|esp_now\w*)\b")
    for nome in FONTES_FIRMWARE:
        texto = (V08 / nome).read_text(encoding="utf-8")
        assert not proibidos.search(texto), nome
    assert (V08 / "libraries.txt").read_bytes() == (V07 / "libraries.txt").read_bytes()


def test_guardia_nao_aciona_o_servo():
    for nome in ("coflume_politica.h", "guardia_runtime.h", "guardia_modelo.h", "guardia_autoteste.h", "coflume_banco.h"):
        texto = (V08 / nome).read_text(encoding="utf-8")
        assert not re.search(r"ledcWrite|ledcAttach|digitalWrite|servoAngulo|abrir\(", texto), nome
    sketch = (V08 / "sketch.ino").read_text(encoding="utf-8")
    # abrir() só é chamado no validador (finalizar), depois de desbloqueia(acertos).
    chamadas = [m.start() for m in re.finditer(r"(?<!void )\babrir\(\)", sketch)]
    inicio = sketch.index("void finalizar(){")
    fim = sketch.index("\nvoid ", inicio + 1)
    assert chamadas and all(inicio < c < fim for c in chamadas)
    assert sketch.index("coflume::desbloqueia(acertos)") < chamadas[0]


CONEXOES_MONITOR_SERIAL = [["esp:TX", "$serialMonitor:RX", "", []], ["esp:RX", "$serialMonitor:TX", "", []]]


def test_circuito_da_v07_preservado_com_monitor_serial():
    """Mesmas peças e ligações da v0.7, mais SÓ as 2 conexões do monitor serial.

    Sem elas, o Wokwi mostrava apenas as mensagens do ROM do ESP32-S3, e não o
    Serial do programa (verificado manualmente no Wokwi em 02/11/2026).
    """
    v07 = json.loads((V07 / "diagram.json").read_text(encoding="utf-8"))
    v08 = json.loads((V08 / "diagram.json").read_text(encoding="utf-8"))
    assert v08["parts"] == v07["parts"], "componentes, tipos, posições e atributos idênticos aos da v0.7"
    assert {k: v for k, v in v08.items() if k != "connections"} == {k: v for k, v in v07.items() if k != "connections"}
    assert v08["connections"][: len(v07["connections"])] == v07["connections"], "ligações da v0.7 preservadas, na ordem"
    assert v08["connections"][len(v07["connections"]):] == CONEXOES_MONITOR_SERIAL, "só as conexões do monitor serial"
    # Nenhum pino do jogo usado pelo monitor serial (TX/RX ficam livres na v0.7).
    pinos_v07 = {ponta for c in v07["connections"] for ponta in c[:2]}
    assert not {"esp:TX", "esp:RX"} & pinos_v07


def test_v07_intacta():
    for nome, esperado in SHA_V07.items():
        assert sha(V07 / nome) == esperado, nome
    assert sorted(p.name for p in V07.iterdir()) == sorted(SHA_V07)
    if shutil.which("git") and (RAIZ / ".git").exists():
        base = subprocess.run(["git", "-C", str(RAIZ), "merge-base", "HEAD", "origin/main"], capture_output=True, text=True)
        if base.returncode == 0:
            diff = subprocess.run(["git", "-C", str(RAIZ), "diff", "--stat", base.stdout.strip(), "--",
                                   "simulations/wokwi/v0.7"], capture_output=True, text=True)
            assert diff.stdout.strip() == "", diff.stdout
