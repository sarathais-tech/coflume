"""Testes automatizados da v0.9 EXPERIMENTAL (hardware físico: ESP32-S3 + ST7735).

Base: o sketch que funciona na placa (quiz com a Guardiã + Agente Secreto USB,
servo real no GPIO 12, LEDs simulados).

Executar a partir da raiz do repositório:
    python3 -m pytest -q firmware/v0.9/testes

Requisitos: Python 3.10+, pytest, g++ (C++17). Usa os vetores e a referência
Python já versionados na v0.8 (sem copiá-los).
Opcional: GFX_GLCDFONT=<glcdfont.c> e Pillow geram as imagens das telas.
"""

from __future__ import annotations

import hashlib
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

V09 = Path(__file__).resolve().parents[1]
SKETCH = V09 / "coflume_v09"
TESTES = V09 / "testes"
RAIZ = V09.parents[1]
V08 = RAIZ / "simulations" / "wokwi" / "v0.8"
V07 = RAIZ / "simulations" / "wokwi" / "v0.7"
NUCLEO = ("guardia_modelo.h", "guardia_runtime.h", "guardia_autoteste.h", "coflume_banco.h", "coflume_politica.h")
SHA_V07 = {
    "diagram.json": "ff8ec751816787692dc1b007a78f6c33d9dc2c1e21304f3074bac4197260b3af",
    "libraries.txt": "12dbcb573d3b2c58fcc858a8b06ba19f1322c0c0e185fc38306b63006e3b6854",
    "sketch.ino": "c10f16f74d296b1ee872b0321bda43532fe0ae672e75aa789f53f8b552a2aa42",
}


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


# --- Guardiã: o mesmo núcleo da v0.8 ------------------------------------------------

def test_nucleo_da_guardia_identico_ao_da_v08():
    for nome in NUCLEO:
        assert (SKETCH / nome).read_bytes() == (V08 / nome).read_bytes(), nome


def test_modelo_e_banco_conferem_com_a_origem():
    import json
    origem = json.loads((V08 / "ORIGEM_GUARDIA.json").read_text(encoding="utf-8"))
    for nome in ("guardia_modelo.h", "guardia_runtime.h"):
        assert sha(SKETCH / nome) == origem["arquivos"][nome]["sha256"], nome
    cab = (SKETCH / "guardia_modelo.h").read_text(encoding="utf-8")
    assert '"mlp8-sint-20261101-e6a4cf00"' in cab and "e6a4cf0086ccf5ab1eccecbe7aea02f3962a6ad7ced29b136d36c5b37b5b9991" in cab


def test_400_vetores_politica_e_referencia_python(gpp, tmp_path):
    """Mesmo teste da v0.8, compilado com os cabeçalhos da v0.9."""
    saida = compilar_e_rodar(gpp, tmp_path, V08 / "testes" / "teste_politica.cpp", "politica",
                             [f"-I{SKETCH}", f"-I{V08 / 'testes'}"], [str(V08 / "testes" / "referencia_politica.csv")])
    print(saida)
    assert "0 falhas" in saida and "1224 partidas" in saida


# --- Partida, saídas simuladas e layout da ST7735 -------------------------------------

@pytest.mark.parametrize("adaptativa", [1, 0])
def test_partida_e_layout_st7735(gpp, tmp_path, adaptativa):
    telas = tmp_path / "telas.jsonl"
    saida = compilar_e_rodar(gpp, tmp_path, TESTES / "teste_v09.cpp", f"v09_{adaptativa}",
                             [f"-DGUARDIA_ADAPTATIVA={adaptativa}", f"-I{TESTES / 'stub'}", f"-I{SKETCH}", "-x", "c++"],
                             [str(telas)])
    print(saida)
    assert "0 falhas" in saida and "81 telas capturadas" in saida
    fonte = os.environ.get("GFX_GLCDFONT")
    if adaptativa and fonte and Path(fonte).is_file():
        pytest.importorskip("PIL")
        r = subprocess.run([sys.executable, str(V09 / "ferramentas" / "renderizar_telas.py"), str(telas), fonte,
                            str(tmp_path / "png")], capture_output=True, text=True)
        assert r.returncode == 0, r.stderr
        assert len(list((tmp_path / "png").glob("*.png"))) == 82


def test_servo_com_falha_avisa_na_tela(gpp, tmp_path):
    saida = compilar_e_rodar(gpp, tmp_path, TESTES / "teste_v09.cpp", "v09_falha",
                             [f"-I{TESTES / 'stub'}", f"-I{SKETCH}", "-x", "c++"], ["-", "servo_falha"])
    print(saida)
    assert "servo_falha" in saida and "0 falhas" in saida


def test_configuracao_publicada():
    s = (SKETCH / "coflume_v09.ino").read_text(encoding="utf-8")
    assert re.findall(r"^#define GUARDIA_ADAPTATIVA (\d)", s, re.M) == ["1"]
    assert "constexpr uint8_t TFT_CS=14, TFT_RST=16, TFT_DC=15, TFT_MOSI=17, TFT_SCLK=18;" in s
    assert "byte linhas[4]={8,9,10,11};" in s and "byte colunas[4]={4,5,6,7};" in s
    assert "#include <Adafruit_ST7735.h>" in s and "ILI9341" not in s
    assert "tft.setTextWrap(false);" in s
    assert "constexpr uint8_t SERVO_PIN = 12;" in s and "constexpr int ANGULO_FECHADO=90, ANGULO_ABERTO=0;" in s
    # A linha de inicialização do servo sai DEPOIS da espera pelo USB CDC.
    assert s.index("while(!Serial&&millis()-t0<3000)") < s.index('Serial.printf("SERVO,inicializacao=%s')


def _funcao(texto: str, assinatura: str) -> str:
    i = texto.index(assinatura)
    return texto[i: texto.index("\n}", i)]


def test_servo_real_so_pelo_validador_ou_pela_senha():
    """O servo (LEDC no GPIO 12) abre só pelo validador 2/3 do quiz ou pela senha
    correta do Agente Secreto; nunca pela Guardiã nem pelo painel. LEDs simulados."""
    s = (SKETCH / "coflume_v09.ino").read_text(encoding="utf-8")
    ag = (SKETCH / "coflume_agente.h").read_text(encoding="utf-8")
    for texto in (s, ag):
        assert not re.search(r"\b(pinMode|digitalWrite|analogWrite)\b", texto)
    assert "SIMULACAO,%s=%d" in s  # LEDs continuam simulados
    # LEDC só na inicialização e em servoAngulo().
    assert [m.start() for m in re.finditer(r"\bledcWrite\(", s)] == [s.index("ledcWrite(", s.index("void servoAngulo("))]
    assert len(re.findall(r"\bledcAttach\(", s)) == 1 and s.index("ledcAttach(") > s.index("void setup(){")
    assert "ledc" not in ag
    # Quiz: abrir() só em finalizar(), depois de desbloqueia(acertos).
    chamadas = [m.start() for m in re.finditer(r"(?<!void )\babrir\(\)", s)]
    fin = _funcao(s, "void finalizar(){")
    assert len(chamadas) == 1 and fin.index("coflume::desbloqueia(acertos)") < fin.index("abrir()")
    # Agente: abrir() só em terminar(venceu), e venceu=true só com a senha digitada correta.
    assert len(re.findall(r"\babrir\(\)", ag)) == 1 and "if(venceu)abrir();" in _funcao(ag, "void terminar(")
    assert re.findall(r"terminar\(true,[^)]*\)", ag) == ['terminar(true,"CORRECT")']
    assert 'if(strcmp(digitada,senha)==0)terminar(true,"CORRECT");' in ag
    # Comandos do painel não abrem o servo: nenhum caminho de comando() chama terminar(true...).
    assert "terminar(true" not in _funcao(ag, "void comando(char* s)")
    for nome in NUCLEO + ("coflume_tela.h",):
        assert not re.search(r"servoAngulo|abrir\(|digitalWrite|ledc|SIMULACAO", (SKETCH / nome).read_text(encoding="utf-8")), nome


def test_protocolo_agente_preservado():
    """Mensagens, tempos e comandos do COFLUME_AGENT/1 iguais aos da versão da placa."""
    ag = (SKETCH / "coflume_agente.h").read_text(encoding="utf-8")
    for trecho in ('"AG,STATE,%08lX,%s,%lu,%u\\n"', '"AG,PREP,%08lX,%s\\n"', '"AG,HELLO,1,%d\\n"',
                   '"AG,EVENT,%08lX,%s\\n"', '"AG,ERROR,BUSY"', '"AG,ERROR,SERVO"', '"AG,EVENT,%08lX,INCOMPLETE\\n"',
                   '"AG,EVENT,%08lX,WRONG\\n"', "LIMITE_MS=90000, CONEXAO_MS=5000",
                   'strcmp(cmd,"HELLO")', 'strcmp(cmd,"START")', 'strcmp(cmd,"PING")', 'strcmp(cmd,"READY")',
                   'strcmp(cmd,"CANCEL")', '"PANEL_TIMEOUT"', '"DISCONNECTED"', '"TIMEOUT"', '"CANCELLED"', '"CORRECT"',
                   "millis()-ultimaPublicacao>=250", "for(int i=0;i<192&&Serial.available();++i)"):
        assert trecho in ag, trecho


def test_sem_dependencia_de_internet():
    proibidos = re.compile(r"\b(WiFi\w*|HTTPClient|esp_wifi\w*|esp_http\w*|WebServer|BLE\w*|BluetoothSerial|"
                           r"PubSubClient|AsyncTCP|esp_now\w*)\b")
    for p in SKETCH.iterdir():
        assert not proibidos.search(p.read_text(encoding="utf-8")), p.name


# --- Versões anteriores intactas --------------------------------------------------------

def test_v07_intacta():
    for nome, esperado in SHA_V07.items():
        assert sha(V07 / nome) == esperado, nome
    assert sorted(p.name for p in V07.iterdir()) == sorted(SHA_V07)


def test_v08_intacta():
    """Hashes de todos os arquivos da v0.8, registrados antes de criar a v0.9."""
    esperado = {}
    for linha in (TESTES / "v08_sha256.txt").read_text(encoding="utf-8").splitlines():
        h, nome = linha.split("  ", 1)
        esperado[nome] = h
    atual = {str(p.relative_to(V08)): sha(p) for p in V08.rglob("*") if p.is_file() and "__pycache__" not in p.parts}
    assert atual == esperado
