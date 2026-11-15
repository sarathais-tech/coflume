"""Gera a referência Python da política gradual para comparar com o C++.

Uso (a partir de firmware/experimentos/politica_gradual/):
    python3 ferramentas/gerar_referencia_gradual.py --guardian-ai /caminho/coflume-guardian-ai

Usa do coflume-guardian-ai (branch local exp/politica-gradual):
- guardia/politica_gradual.py: a política gradual (referência);
- guardia/politica.py: fluxo-0.8, para registrar o que mudaria;
- guardia/features.py e guardia/inferencia_ref.py: features e inferência da MLP;
- resultados/sinteticos/treino_v2/mlp_candidata.npz: pesos da MLP REAL, conferidos pelo hash.
O conjunto de teste lacrado não é lido.

Bancos: o completo (13 perguntas, 12 células), os 12 bancos com UMA célula vazia
e 20 bancos aleatórios (semente 2027) com 2 a 7 células vazias.
Cenários por banco: 3 níveis iniciais × 24 ordens de categorias × 4 perfis de
resposta nas perguntas 1 e 2 (16 combinações) = 1.152 partidas com a MLP.
Nos 5 primeiros bancos, as mesmas partidas também sem modelo e com previsão
inválida (NaN) no nível 2.

Saída: testes/referencia_gradual.csv.gz
"""

from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import io
import itertools
import json
import math
import random
import sys
from pathlib import Path

PASTA = Path(__file__).resolve().parents[1]
V09 = PASTA.parents[1] / "v0.9" / "coflume_v09"
SAIDA = PASTA / "testes" / "referencia_gradual.csv.gz"
# (acertou, tempo_gasto_s, usou_dica); 30 s = tempo esgotado (erro)
PERFIS = [(True, 3.0, False), (True, 15.0, True), (False, 12.0, False), (False, 30.0, False)]
CELULAS = [(n, c) for n in range(3) for c in range(4)]


def bancos() -> list[frozenset]:
    lista = [frozenset()] + [frozenset([c]) for c in CELULAS]
    rng = random.Random(2027)
    while len(lista) < 33:
        m = frozenset(rng.sample(CELULAS, rng.randint(2, 7)))
        if m not in lista:
            lista.append(m)
    return lista


def mascara(vazias: frozenset) -> str:
    """12 caracteres, célula n*4+c: 1 = tem pergunta."""
    return "".join("0" if (n, c) in vazias else "1" for n, c in CELULAS)


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--guardian-ai", type=Path, required=True)
    args = ap.parse_args(argv)
    raiz = args.guardian_ai.resolve()
    sys.path.insert(0, str(raiz))
    from guardia import politica as P
    from guardia import politica_gradual as G
    from guardia.artefato import carregar_mlp_npz
    from guardia.features import Resposta, montar_entrada
    from guardia.inferencia_ref import prever

    meta = json.loads((V09.parents[2] / "simulations" / "wokwi" / "v0.8" / "MODELO.json").read_text(encoding="utf-8"))
    npz = raiz / "resultados" / "sinteticos" / "treino_v2" / "mlp_candidata.npz"
    if hashlib.sha256(npz.read_bytes()).hexdigest() != meta["origem"]["pesos_npz_sha256"]:
        raise SystemExit("mlp_candidata.npz não confere com MODELO.json")
    modelo = carregar_mlp_npz(npz)
    if modelo.hash() != meta["hash_sha256"]:
        raise SystemExit("hash do modelo não confere")

    buf = io.StringIO()
    w = csv.writer(buf, lineterminator="\n")
    cab = ["mascara", "viavel", "modo", "nivel_inicial", "ordem_categorias", "perfis"]
    for q in (1, 2, 3):
        cab += [f"q{q}_{x}" for x in ("status", "categoria", "nivel", "motivo", "p0", "p1", "p2", "sem_regra")]
    w.writerow(cab)
    resumo = {"bancos": 0, "inviaveis": 0, "partidas": 0, "decisoes": 0, "recusas": 0, "limitou": 0, "limitou_mlp": 0,
              "decisoes_mlp": 0}
    for ib, vazias in enumerate(bancos()):
        tem = (lambda v: (lambda n, c: (n, c) not in v))(vazias)
        viavel = not G.problemas_de_viabilidade_gradual(tem)
        resumo["bancos"] += 1
        resumo["inviaveis"] += not viavel
        modos = ["mlp", "sem_modelo", "invalida_n2"] if ib < 5 else ["mlp"]
        for modo, ini, ordem, perfis in itertools.product(modos, range(3), itertools.permutations(range(4)),
                                                          itertools.product(range(4), repeat=2)):
            hist: list = []
            restantes, anterior = list(ordem), None
            linha = [mascara(vazias), int(viavel), modo, ini, "".join(map(str, ordem)), "".join(map(str, perfis))]
            for q in range(3):
                def prev(n, c):
                    if modo == "invalida_n2" and n == 2:
                        return math.nan
                    return float(prever(modelo, montar_entrada(hist, ini, c, n)))
                try:
                    d = G.decidir_pergunta_gradual(q, ini, restantes, tem, None if modo == "sem_modelo" else prev,
                                                   anterior)
                except (G.SemNivelCompativelError, P.BancoIncompletoError):
                    linha += ["RECUSA", "-", "-", "sem_nivel_compativel", "-", "-", "-", "-"]
                    linha += ["-"] * 8 * (2 - q)
                    resumo["recusas"] += 1
                    break
                ps = [d.previsoes.get(n) for n in range(3)]
                linha += ["OK", d.categoria, d.nivel, d.motivo] + \
                         ["NA" if p is None else ("nan" if math.isnan(p) else f"{p:.9g}") for p in ps] + \
                         [-1 if d.nivel_sem_regra is None else d.nivel_sem_regra]
                resumo["decisoes"] += 1
                resumo["limitou"] += d.limitou
                if modo == "mlp" and q > 0:
                    resumo["decisoes_mlp"] += 1
                    resumo["limitou_mlp"] += d.limitou
                restantes.remove(d.categoria)
                if q < 2:
                    ac, tempo, dica = PERFIS[perfis[q]]
                    hist.append(Resposta(d.categoria, d.nivel, ac, tempo, 30.0, dica))
                    anterior = G.Anterior(d.nivel, ac)
            w.writerow(linha)
            resumo["partidas"] += 1
    SAIDA.parent.mkdir(parents=True, exist_ok=True)
    with gzip.GzipFile(SAIDA, "wb", mtime=0) as f:  # mtime=0: arquivo reprodutível
        f.write(buf.getvalue().encode("utf-8"))
    print(json.dumps(resumo, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
