"""Extrai os pesos da Guardiã do guardia_modelo.h da r4 para o painel (somente leitura).

Uso (a partir de firmware/v0.9-r4/painel/):
    python3 ferramentas/extrair_modelo.py             # gera ferramentas/modelo_guardia.js
    python3 ferramentas/extrair_modelo.py --verificar

Os valores são copiados como texto, exatamente como estão no cabeçalho (literais float).
O painel converte cada um com Math.fround, o mesmo float32 que o compilador grava na placa.
Também confere o id e o hash do modelo contra o ORIGEM_GUARDIA.json da v0.8.
"""
from __future__ import annotations

import hashlib
import json
import re
import sys
from pathlib import Path

PASTA = Path(__file__).resolve().parents[1]
FIRMWARE = PASTA.parents[1]
MODELO_H = PASTA.parent / "coflume_v09" / "guardia_modelo.h"
RUNTIME_H = PASTA.parent / "coflume_v09" / "guardia_runtime.h"
ORIGEM = FIRMWARE.parent / "simulations" / "wokwi" / "v0.8" / "ORIGEM_GUARDIA.json"
SAIDA = PASTA / "ferramentas" / "modelo_guardia.js"


def numeros(bloco: str) -> list[str]:
    return [n.rstrip("f") for n in re.findall(r"-?\d+(?:\.\d+)?(?:e-?\d+)?f", bloco)]


def array(texto: str, nome: str) -> list[str]:
    m = re.search(rf"{nome}\[[^\]]*\](?:\[[^\]]*\])?\s*=\s*\{{(.*?)\}};", texto, re.S)
    assert m, nome
    return numeros(m.group(1))


def extrair() -> dict:
    t = MODELO_H.read_text(encoding="utf-8")
    define = lambda n: re.search(rf'#define {n} "?([^"\n]+)"?', t).group(1)
    origem = json.loads(ORIGEM.read_text(encoding="utf-8"))
    sha_h = hashlib.sha256(MODELO_H.read_bytes()).hexdigest()
    assert sha_h == origem["arquivos"]["guardia_modelo.h"]["sha256"], "guardia_modelo.h diferente da origem registrada"
    assert define("GUARDIA_MODELO_ID") == origem["modelo_id"]
    assert define("GUARDIA_MODELO_HASH") == origem["modelo_hash_parametros_sha256"]
    assert define("GUARDIA_H1") == "8" and define("GUARDIA_H2") == "0" and define("GUARDIA_N_ENTRADAS") == "13"
    w1 = array(t, "GUARDIA_W_O1")
    m = {
        "id": define("GUARDIA_MODELO_ID"),
        "hash_parametros": define("GUARDIA_MODELO_HASH"),
        "fonte_dados": define("GUARDIA_MODELO_FONTE"),
        "arquivo_origem": "v0.9-r4/coflume_v09/guardia_modelo.h",
        "arquivo_sha256": sha_h,
        "media": array(t, "GUARDIA_MEDIA"),
        "desvio": array(t, "GUARDIA_DESVIO"),
        "w1": [w1[i * 13:(i + 1) * 13] for i in range(8)],
        "b1": array(t, "GUARDIA_B_O1"),
        "w2": array(t, "GUARDIA_W_SAIDA"),
        "b2": re.search(r"GUARDIA_B_SAIDA\s*=\s*(-?[\d.e-]+)f", t).group(1),
    }
    assert len(m["media"]) == 9 and len(m["desvio"]) == 9 and len(w1) == 104 and len(m["b1"]) == 8 and len(m["w2"]) == 8
    n = 104 + 8 + 8 + 1
    assert n == int(define("GUARDIA_MODELO_N_PARAMETROS"))
    return m


def gerar() -> str:
    m = extrair()
    js = json.dumps(m, ensure_ascii=False, separators=(",", ":"))
    # números como literais JS (sem aspas), preservando o texto do cabeçalho
    js = re.sub(r'"(-?\d+(?:\.\d+)?(?:e-?\d+)?)"', r"\1", js)
    return ("  // Pesos da Guardiã copiados de " + m["arquivo_origem"] + " (gerado por ferramentas/extrair_modelo.py; não editar).\n"
            "  const GV_MODELO=" + js + ";\n")


if __name__ == "__main__":
    texto = gerar()
    if "--verificar" in sys.argv:
        ok = SAIDA.exists() and SAIDA.read_text(encoding="utf-8") == texto
        print("modelo_guardia.js confere com guardia_modelo.h" if ok else "modelo_guardia.js desatualizado")
        raise SystemExit(0 if ok else 1)
    SAIDA.write_text(texto, encoding="utf-8")
    print(f"Gerado {SAIDA.name}")
