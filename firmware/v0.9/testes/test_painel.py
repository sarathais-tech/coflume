"""Teste do painel USB do Agente Secreto no Chromium, com porta serial simulada.

Executar a partir da raiz do repositório (requer o pacote Python `playwright`
com o Chromium instalado; sem ele, o teste é pulado):
    python3 -m pytest -q firmware/v0.9/testes/test_painel.py

Verifica que a área de logs da Guardiã:
- registra e salva só as linhas GUARDIA,...;
- não envia nada ao ESP32;
- não muda as pistas, o relógio nem o tratamento do protocolo AG.

Para isso, o mesmo roteiro AG roda no painel ORIGINAL e no painel atualizado,
e os comandos enviados e os textos visíveis são comparados.
"""

from __future__ import annotations

import os
import re
from pathlib import Path

import pytest

PAINEL = Path(__file__).resolve().parents[1] / "painel" / "Coflume_Agente_Secreto_USB.html"
ORIGINAL = os.environ.get("COFLUME_PAINEL_ORIGINAL")  # opcional: painel da placa, para comparação

# Porta serial falsa: o teste empurra linhas do "ESP32" e lê o que o painel escreveu.
SERIAL_FALSO = """
(() => {
  const escritas = []; let controlador = null;
  const porta = {
    async open() {},
    async close() {},
    readable: new ReadableStream({ start(c) { controlador = c; } }),
    writable: new WritableStream({ write(chunk) { escritas.push(new TextDecoder().decode(chunk)); } }),
  };
  Object.defineProperty(navigator, 'serial', { value: { async requestPort() { return porta; } } });
  window.__esp = (texto) => controlador.enqueue(new TextEncoder().encode(texto));
  window.__escritas = escritas;
})();
"""

SESSAO, SENHA = "1A2B3C4D", "4071"


def _avaliar(expr: str) -> float:
    py = expr.replace("×", "*").replace("÷", "/").replace("−", "-")
    assert re.fullmatch(r"[0-9+\-*/() .]+", py), expr
    return eval(py)  # noqa: S307 - expressão numérica validada acima


def _roteiro(pagina, com_logs: bool) -> dict:
    pagina.click("#connect")
    pagina.wait_for_function("window.__escritas.join('').includes('AG,HELLO')")
    pagina.evaluate("""() => window.__esp('AG,HELLO,1,1\\nAG,STATE,00000000,IDLE,0,0\\n'
      + 'GUARDIA,reenvio_diagnostico_boot,motivo=painel_conectado\\n'
      + 'GUARDIA,autoteste,vetores=21,erro_p_max=5.96e-08,erro_features_max=0,falhas=0,invalidos_ok=2/2,modelo_confere=sim,resultado=OK\\n'
      + 'SERVO,angulo=90\\nMENU\\n')""")
    pagina.wait_for_function("document.getElementById('status').textContent==='COFRE PRONTO'")
    pagina.evaluate(f"() => window.__esp('KEY:A\\nAG,PREP,{SESSAO},{SENHA}\\nAG,STATE,{SESSAO},PREP,0,0\\n')")
    pagina.wait_for_function(f"window.__escritas.join('').includes('AG,READY,{SESSAO}')")
    pagina.evaluate(f"""() => window.__esp('AG,STATE,{SESSAO},PLAYING,90000,0\\n'
      + 'GUARDIA,decisao,modelo=mlp8,ordem=2,p0=0.81,p1=0.69,p2=0.55,nivel_escolhido=1,motivo=guardia\\n'
      + 'KEY:1\\nAG,EVENT,{SESSAO},WRONG\\nAG,STATE,{SESSAO},PLAYING,80000,1\\n')""")
    pagina.wait_for_function("document.getElementById('status').textContent==='MISSAO EM ANDAMENTO'")
    pistas = pagina.eval_on_selector_all("#clues .expression", "els => els.map(e => e.textContent)")
    feedback_jogo = pagina.text_content("#feedback")
    pagina.wait_for_timeout(1300)  # deixa passar um PING
    pagina.evaluate(f"() => window.__esp('AG,EVENT,{SESSAO},CORRECT\\nAG,STATE,{SESSAO},WIN,0,2\\nSERVO,angulo=0\\n')")
    pagina.wait_for_function("document.getElementById('status').textContent==='MISSAO CUMPRIDA'")
    escritas = "".join(pagina.evaluate("window.__escritas")).splitlines()
    return {
        "escritas": [re.sub(r"PING,.*", "PING", e) for e in escritas],
        "pistas": pistas,
        "feedback_jogo": feedback_jogo,
        "status": pagina.text_content("#status"),
        "aviso": pagina.text_content("#notice"),
        "feedback": pagina.text_content("#feedback"),
        "botao_proximo": pagina.is_enabled("#next"),
    }


@pytest.fixture(scope="module")
def navegador():
    sync_api = pytest.importorskip("playwright.sync_api")
    with sync_api.sync_playwright() as p:
        try:
            b = p.chromium.launch()
        except Exception as erro:  # noqa: BLE001 - Chromium ausente no ambiente
            pytest.skip(f"Chromium indisponível: {erro}")
        yield b
        b.close()


def _abrir(navegador, arquivo: Path):
    ctx = navegador.new_context(accept_downloads=True)
    ctx.add_init_script(SERIAL_FALSO)
    pagina = ctx.new_page()
    erros = []
    pagina.on("pageerror", lambda e: erros.append(str(e)))
    pagina.goto(arquivo.as_uri())
    return ctx, pagina, erros


def test_painel_protocolo_pistas_e_logs(navegador, tmp_path):
    ctx, pagina, erros = _abrir(navegador, PAINEL)
    r = _roteiro(pagina, com_logs=True)
    # Protocolo AG: o painel só envia HELLO, READY e PING.
    assert {e.split(",")[1] for e in r["escritas"]} <= {"HELLO", "READY", "PING"}
    assert f"AG,READY,{SESSAO}" in r["escritas"] and "AG,PING" in r["escritas"]
    # Pistas: 4 expressões, cada uma resultando no dígito da senha, na ordem.
    assert [_avaliar(e) for e in r["pistas"]] == [int(d) for d in SENHA]
    assert r["status"] == "MISSAO CUMPRIDA" and "ACESSO AUTORIZADO" in r["aviso"] and r["botao_proximo"]
    # Logs: só as 3 linhas GUARDIA (nem AG, nem KEY, nem SERVO, nem MENU).
    assert pagina.text_content("#logCount") == "3"
    texto = pagina.text_content("#logView")
    assert "GUARDIA,autoteste" in texto and "GUARDIA,decisao" in texto and "GUARDIA,reenvio_diagnostico_boot" in texto
    assert not re.search(r"\bAG,|KEY:|SERVO,|MENU", texto)
    # Área recolhível: fechada por padrão.
    assert pagina.eval_on_selector("#guardiaLogs", "e => e.open") is False
    # Salvar: arquivo .txt com cabeçalho e as linhas.
    pagina.click("#guardiaLogs summary")
    with pagina.expect_download() as d:
        pagina.click("#logSave")
    destino = tmp_path / d.value.suggested_filename
    d.value.save_as(destino)
    conteudo = destino.read_text(encoding="utf-8")
    assert re.fullmatch(r"coflume_logs_guardia_\d{8}_\d{6}\.txt", destino.name)
    assert conteudo.startswith("# Coflume - logs da Guardia IA") and conteudo.count("GUARDIA,") == 3
    pagina.click("#logClear")
    assert pagina.text_content("#logCount") == "0" and pagina.text_content("#logView") == ""
    # Limite: no máximo 5.000 linhas guardadas e 300 na tela.
    pagina.evaluate("() => window.__esp(Array.from({length:5100},(_,i)=>'GUARDIA,teste,i='+i).join('\\n')+'\\n')")
    pagina.wait_for_function("document.getElementById('logCount').textContent==='5000'")
    assert len(pagina.text_content("#logView").splitlines()) == 300
    assert erros == []
    ctx.close()


def test_painel_igual_ao_original_no_protocolo_ag(navegador):
    """Mesmo roteiro no painel da placa e no atualizado: mesmos comandos e textos."""
    if not ORIGINAL or not Path(ORIGINAL).is_file():
        pytest.skip("defina COFLUME_PAINEL_ORIGINAL com o HTML da placa para comparar")
    resultados = []
    for arquivo in (Path(ORIGINAL), PAINEL):
        ctx, pagina, erros = _abrir(navegador, arquivo)
        r = _roteiro(pagina, com_logs=arquivo == PAINEL)
        r.pop("pistas")  # mesmas regras de geração; comparadas pela avaliação no outro teste
        assert erros == []
        resultados.append(r)
        ctx.close()
    assert resultados[0] == resultados[1]
