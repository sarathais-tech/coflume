"""Gera coflume_banco.h a partir do banco de perguntas da v0.8 (CSV).

Uso (a partir de simulations/wokwi/v0.8/):
    python3 ferramentas/gerar_banco.py
    python3 ferramentas/gerar_banco.py --verificar   # só confere se o .h está atualizado

O CSV é cópia de `questoes/banco_v08.csv` do coflume-guardian-ai (ver ORIGEM_GUARDIA.json).

Acentos: a fonte clássica da Adafruit GFX (glcdfont.c) segue o code page 437.
Cada caractere fora do ASCII é convertido com o codec cp437 do Python; códigos
de 176 em diante são recusados porque a GFX os desloca quando cp437(false),
e caracteres que não existem no cp437 (ex.: ã, õ) também são recusados.
O texto do CSV não é alterado.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import sys
from pathlib import Path

PASTA = Path(__file__).resolve().parents[1]
CSV_PADRAO = PASTA / "ferramentas" / "banco_v08.csv"
SAIDA_PADRAO = PASTA / "coflume_banco.h"

N_NIVEIS = 3
N_CATEGORIAS = 4
MAX_POR_CELULA = 8  # comporta a meta de 36 (3 por célula) e a de 60 (5 por célula)
CAMPOS = ("id", "origem", "nivel", "categoria", "enunciado", "alt_a", "alt_b", "alt_c", "alt_d", "correta", "dica")
LIMITE_GFX_SEM_DESLOCAMENTO = 176


def para_fonte_gfx(texto: str) -> bytes:
    """Converte para os códigos da fonte clássica da GFX (cp437)."""
    saida = bytearray()
    for ch in texto:
        if ord(ch) < 128:
            if ord(ch) < 32:
                raise ValueError(f"Caractere de controle em {texto!r}")
            saida.append(ord(ch))
            continue
        try:
            codigo = ch.encode("cp437")[0]
        except UnicodeEncodeError as erro:
            raise ValueError(f"'{ch}' não existe na fonte da GFX (cp437): {texto!r}") from erro
        if codigo >= LIMITE_GFX_SEM_DESLOCAMENTO:
            raise ValueError(f"'{ch}' (0x{codigo:02X}) depende de cp437(true) na GFX: {texto!r}")
        saida.append(codigo)
    return bytes(saida)


def literal_c(texto: str) -> str:
    """Literal C seguro: bytes não ASCII como \\xNN isolado por concatenação."""
    partes, atual = [], ""
    for b in para_fonte_gfx(texto):
        if b >= 128:
            partes.append(f'"{atual}"')
            partes.append(f'"\\x{b:02X}"')
            atual = ""
        else:
            ch = chr(b)
            atual += "\\" + ch if ch in '"\\' else ch
    partes.append(f'"{atual}"')
    return " ".join(p for i, p in enumerate(partes) if p != '""' or len(partes) == 1)


def ler_banco(caminho: Path) -> list[dict]:
    with caminho.open(encoding="utf-8", newline="") as f:
        linhas = list(csv.DictReader(f))
    ids, por_celula = set(), {}
    for n, r in enumerate(linhas, start=2):
        for campo in CAMPOS:
            if not (r.get(campo) or "").strip():
                raise ValueError(f"Linha {n}: campo vazio '{campo}'")
        nivel, cat = int(r["nivel"]), int(r["categoria"])
        if not (0 <= nivel < N_NIVEIS and 0 <= cat < N_CATEGORIAS):
            raise ValueError(f"Linha {n}: nível/categoria fora da faixa")
        if r["correta"] not in "ABCD" or len(r["correta"]) != 1:
            raise ValueError(f"Linha {n}: alternativa correta inválida")
        if r["id"] in ids:
            raise ValueError(f"Linha {n}: id repetido {r['id']}")
        ids.add(r["id"])
        por_celula[(nivel, cat)] = por_celula.get((nivel, cat), 0) + 1
        if por_celula[(nivel, cat)] > MAX_POR_CELULA:
            raise ValueError(f"Mais de {MAX_POR_CELULA} perguntas na célula {nivel}x{cat}")
    vazias = [(n, c) for n in range(N_NIVEIS) for c in range(N_CATEGORIAS) if (n, c) not in por_celula]
    if vazias:
        print(f"Aviso: células vazias {vazias}; o firmware usa a substituição de categoria.", file=sys.stderr)
    return linhas


def gerar(caminho: Path) -> str:
    linhas = ler_banco(caminho)
    sha = hashlib.sha256(caminho.read_bytes()).hexdigest()
    L = [
        "// =============================================================================",
        "//  coflume_banco.h — banco de perguntas da v0.8 (Coflume — O Conhecimento Abre Portas)",
        "// ARQUIVO GERADO por ferramentas/gerar_banco.py — NÃO EDITAR À MÃO.",
        "//  Origem: questoes/banco_v08.csv do coflume-guardian-ai (ver ORIGEM_GUARDIA.json).",
        "//  Textos convertidos para a fonte clássica da Adafruit GFX (cp437).",
        "// =============================================================================",
        "#pragma once",
        "#include <stdint.h>",
        "",
        '#define COFLUME_BANCO_VERSAO "banco_v08"',
        f'#define COFLUME_BANCO_CSV_SHA256 "{sha}"',
        f"#define COFLUME_BANCO_N {len(linhas)}",
        f"#define COFLUME_MAX_POR_CELULA {MAX_POR_CELULA}",
        "",
        "namespace coflume {",
        "",
        "struct Pergunta {",
        "  const char* id;",
        "  uint8_t nivel;      // 0 Explorador, 1 Desafiante, 2 Mestre",
        "  uint8_t categoria;  // 0 Matematica, 1 Portugues, 2 Logica, 3 Conhecimentos gerais",
        "  const char* enunciado;",
        "  const char* alt[4];",
        "  char correta;       // 'A'..'D'",
        "  const char* dica;",
        "};",
        "",
        "const Pergunta BANCO[COFLUME_BANCO_N] = {",
    ]
    for r in linhas:
        alts = ", ".join(literal_c(r[f"alt_{x}"]) for x in "abcd")
        L.append(f'  {{"{r["id"]}", {int(r["nivel"])}, {int(r["categoria"])}, {literal_c(r["enunciado"])},')
        L.append(f"   {{{alts}}}, '{r['correta']}', {literal_c(r['dica'])}}},")
    L += ["};", "", "}  // namespace coflume", ""]
    return "\n".join(L)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--csv", type=Path, default=CSV_PADRAO)
    ap.add_argument("--saida", type=Path, default=SAIDA_PADRAO)
    ap.add_argument("--verificar", action="store_true")
    args = ap.parse_args(argv)
    texto = gerar(args.csv)
    if args.verificar:
        if not args.saida.exists() or args.saida.read_text(encoding="utf-8") != texto:
            print(f"{args.saida.name} desatualizado; rode ferramentas/gerar_banco.py", file=sys.stderr)
            return 1
        print(f"{args.saida.name} confere com {args.csv.name}")
        return 0
    args.saida.write_text(texto, encoding="utf-8")
    print(f"Gerado {args.saida} ({texto.count(chr(10))} linhas)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
