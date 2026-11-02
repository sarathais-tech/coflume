"""Gera testes/referencia_politica.csv com a implementação de REFERÊNCIA em Python.

Uso (a partir de simulations/wokwi/v0.8/):
    python3 ferramentas/gerar_referencia_politica.py --guardian-ai /caminho/coflume-guardian-ai

Usa, do repositório coflume-guardian-ai (branch feat/validacao-final-ab):
- guardia/politica.py: decidir_pergunta (fluxo-0.8);
- guardia/features.py e guardia/inferencia_ref.py: features e inferência da MLP;
- resultados/sinteticos/treino_v2/mlp_candidata.npz: pesos da MLP candidata,
  conferidos pelo hash do modelo antes de qualquer previsão.

Nada do conjunto de teste lacrado é lido: só os pesos e a política.

Cenários: 3 níveis iniciais × 24 ordens de categorias × 16 combinações de
respostas nas perguntas 1 e 2 (4 perfis cada) = 1.152 partidas, todas com o
modelo, e mais 72 partidas sem modelo (fallback). Cada linha registra, para as
3 perguntas, a categoria, o nível, o motivo e as previsões de cada nível.
O teste C++ (testes/teste_referencia.cpp) refaz cada partida com o runtime
embarcado e exige as mesmas decisões e previsões com erro < 1e-5.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import itertools
import json
import sys
from pathlib import Path

PASTA = Path(__file__).resolve().parents[1]
SAIDA = PASTA / "testes" / "referencia_politica.csv"

# Perfis de resposta (acertou, tempo_gasto_s, usou_dica); 30 s = tempo esgotado (erro).
PERFIS = [(True, 6.0, False), (True, 20.0, True), (False, 12.0, False), (False, 30.0, False)]
LIMITE_S = 30.0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--guardian-ai", type=Path, required=True)
    args = ap.parse_args(argv)
    raiz = args.guardian_ai.resolve()
    sys.path.insert(0, str(raiz))
    from guardia import politica
    from guardia.artefato import carregar_mlp_npz
    from guardia.features import Resposta, montar_entrada
    from guardia.inferencia_ref import prever

    modelo_json = json.loads((PASTA / "MODELO.json").read_text(encoding="utf-8"))
    npz = raiz / "resultados" / "sinteticos" / "treino_v2" / "mlp_candidata.npz"
    if hashlib.sha256(npz.read_bytes()).hexdigest() != modelo_json["origem"]["pesos_npz_sha256"]:
        raise SystemExit("mlp_candidata.npz não confere com MODELO.json")
    modelo = carregar_mlp_npz(npz)
    if modelo.hash() != modelo_json["hash_sha256"]:
        raise SystemExit("Hash do modelo carregado não confere com MODELO.json")

    banco = list(csv.DictReader((PASTA / "ferramentas" / "banco_v08.csv").open(encoding="utf-8")))
    celulas = {(int(r["nivel"]), int(r["categoria"])) for r in banco}

    def tem_pergunta(nivel: int, categoria: int) -> bool:
        return (nivel, categoria) in celulas

    linhas = []
    cenarios = [(ini, ordem, perfis, True) for ini in range(3) for ordem in itertools.permutations(range(4))
                for perfis in itertools.product(range(len(PERFIS)), repeat=2)]
    cenarios += [(ini, ordem, (0, 2), False) for ini in range(3) for ordem in itertools.permutations(range(4))]
    for ini, ordem, perfis, com_modelo in cenarios:
        hist: list = []
        restantes = list(ordem)
        campos = [ini, "".join(map(str, ordem)), "".join(map(str, perfis)), int(com_modelo)]
        for q in range(3):
            def prev(nivel: int, categoria: int) -> float:
                return float(prever(modelo, montar_entrada(hist, ini, categoria, nivel)))

            d = politica.decidir_pergunta(q, ini, restantes, tem_pergunta, prev if com_modelo else None)
            ps = [d.previsoes.get(n, -1.0) for n in range(3)]
            campos += [d.categoria, d.nivel, d.motivo] + [f"{p:.9g}" for p in ps]
            restantes.remove(d.categoria)
            if q < 2:
                acertou, tempo, dica = PERFIS[perfis[q]]
                hist.append(Resposta(d.categoria, d.nivel, acertou, tempo, LIMITE_S, dica))
        linhas.append(campos)

    cab = ["nivel_inicial", "ordem_categorias", "perfis", "com_modelo"]
    for q in (1, 2, 3):
        cab += [f"q{q}_categoria", f"q{q}_nivel", f"q{q}_motivo", f"q{q}_p0", f"q{q}_p1", f"q{q}_p2"]
    with SAIDA.open("w", encoding="utf-8", newline="") as f:
        w = csv.writer(f, lineterminator="\n")
        w.writerow(cab)
        w.writerows(linhas)
    print(f"Gerado {SAIDA}: {len(linhas)} partidas ({sum(c[3] for c in cenarios)} com modelo)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
