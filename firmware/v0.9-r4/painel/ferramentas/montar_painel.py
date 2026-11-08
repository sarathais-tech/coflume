"""Monta o painel da r4 (Coflume_Agente_Secreto_USB.html) a partir do painel da r3, sem alterá-lo.

Uso (a partir de firmware/v0.9-r4/painel/):
    python3 ferramentas/montar_painel.py            # gera o HTML
    python3 ferramentas/montar_painel.py --verificar

Acréscimos ao painel r3: um script comum antes do painel (modelo_guardia.js, guardia_comum.js e
monitor_guardia.js), uma linha no início do script do painel que encerra a execução na janela de
exposição (?monitor=ia), os blocos visualizacao_guardia.js, rede_guardia.js e ponte_monitor.js
depois da criação do painel,
a chamada guardiaVisUSB(line) no início de packet() e o visualizacao_guardia.css.
"""
from __future__ import annotations

import sys
from pathlib import Path

PASTA = Path(__file__).resolve().parents[1]
R3 = PASTA.parents[1] / "v0.9-r3" / "painel" / "Coflume_Agente_Secreto_USB.html"
SAIDA = PASTA / "Coflume_Agente_Secreto_USB.html"


def montar() -> str:
    h = R3.read_text(encoding="utf-8")
    ler = lambda n: (PASTA / "ferramentas" / n).read_text(encoding="utf-8")
    js = "".join(ler(n) for n in ("visualizacao_guardia.js", "rede_guardia.js", "ponte_monitor.js"))
    # Script comum (antes do script do painel): pesos, inferência, desenho/animador da rede e o monitor da IA.
    comum = ("<script>\n// Guardiã IA (proposta): código comum ao painel principal e à janela de exposição (?monitor=ia).\n"
             "(() => {\n  'use strict';\n" + ler("modelo_guardia.js") + ler("guardia_comum.js") + ler("monitor_guardia.js") + "})();\n</script>\n")
    css = (PASTA / "ferramentas" / "visualizacao_guardia.css").read_text(encoding="utf-8")
    a = "  document.querySelector('.terminal').append(panel);\n"
    b = "  function packet(line){\n    if(!line.startsWith('AG,')){guardiaLog(line);return;}"
    c = "</style>\n</body>"
    d = "<script>\n(() => {\n  'use strict';\n"
    for s in (a, b, c, d):
        assert h.count(s) == 1, f"ponto de inserção não encontrado no painel r3: {s!r}"
    h = h.replace(a, a + js)
    h = h.replace(b, "  function packet(line){\n    guardiaVisUSB(line);\n    if(!line.startsWith('AG,')){guardiaLog(line);return;}")
    # Na janela de exposição o painel principal não roda: sem USB, áudio, pistas nem animação de fundo.
    h = h.replace(d, comum + d + "  if(window.CoflumeGuardia&&window.CoflumeGuardia.modoMonitor)return; // monitor da IA: sem USB, áudio nem pistas\n")
    return h.replace(c, css + c)


if __name__ == "__main__":
    texto = montar()
    if "--verificar" in sys.argv:
        ok = SAIDA.exists() and SAIDA.read_text(encoding="utf-8") == texto
        print("HTML confere com as fontes" if ok else "HTML desatualizado: rode ferramentas/montar_painel.py")
        raise SystemExit(0 if ok else 1)
    SAIDA.write_text(texto, encoding="utf-8")
    print(f"Gerado {SAIDA.name}")
