"""Testes da visualização da rede neural (cálculo reproduzido no painel).

Executar a partir de firmware/v0.9-r4/painel/:
    python3 -m pytest -q testes/test_rede_guardia.py

Confere:
- pesos extraídos e HTML montado batem com as fontes (guardia_modelo.h da r3);
- a inferência do painel reproduz os 400 vetores de equivalência e todas as previsões
  de 300 partidas do firmware r3 simulado (fluxo-0.8 e gradual), dentro da tolerância;
- animação: sequência dos candidatos, decisão da política separada, sem acúmulo,
  sem atrasar o protocolo USB e parada no Agente Secreto;
- casos indisponíveis e movimento reduzido.
"""
from __future__ import annotations

import importlib.util
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

import pytest

RAIZ = Path(__file__).resolve().parents[1]
PAINEL = RAIZ / "Coflume_Agente_Secreto_USB.html"
VETORES = RAIZ.parents[2] / "simulations" / "wokwi" / "v0.8" / "testes" / "vetores_teste.h"
DEMO_FLUXO = RAIZ / "demo" / "demo_fluxo08.txt"
CAPTURAS = RAIZ / "docs" / "capturas"
MODELO = "mlp8-sint-20261101-e6a4cf00"

SERIAL_FALSO = """
(() => {
  const escritas = []; let controlador = null;
  const porta = {
    async open() {}, async close() {},
    readable: new ReadableStream({ start(c) { controlador = c; } }),
    writable: new WritableStream({ write(chunk) { escritas.push([performance.now(), new TextDecoder().decode(chunk)]); } }),
  };
  Object.defineProperty(navigator, 'serial', { value: { async requestPort() { return porta; } } });
  window.__esp = (texto) => controlador.enqueue(new TextEncoder().encode(texto));
  window.__escritas = escritas;
})();
"""


def _js(t: str) -> str:
    return t.replace("\\", "\\\\").replace("'", "\\'").replace("\n", "\\n")


@pytest.fixture(scope="module")
def navegador():
    sync_api = pytest.importorskip("playwright.sync_api")
    with sync_api.sync_playwright() as p:
        try:
            b = p.chromium.launch()
        except Exception as erro:  # noqa: BLE001
            pytest.skip(f"Chromium indisponível: {erro}")
        yield b
        b.close()


def _abrir(navegador, **kw):
    ctx = navegador.new_context(viewport={"width": 1280, "height": 1800}, **kw)
    ctx.add_init_script(SERIAL_FALSO)
    pg = ctx.new_page()
    erros: list[str] = []
    pg.on("pageerror", lambda e: erros.append(str(e)))
    pg.goto(PAINEL.as_uri())
    return ctx, pg, erros


def _conectar(pg):
    pg.click("#connect")
    pg.wait_for_function("window.__escritas.map(e=>e[1]).join('').includes('AG,HELLO')")
    pg.evaluate("() => window.__esp('AG,HELLO,1,1\\nAG,STATE,00000000,IDLE,0,0\\n')")
    pg.wait_for_function("document.getElementById('status').textContent==='COFRE PRONTO'")


def _linhas_demo() -> list[str]:
    return [l.split(" ", 1)[1] for l in DEMO_FLUXO.read_text(encoding="utf-8").splitlines() if l and not l.startswith("#")]


def _pergunta(linhas, sessao, ordem) -> str:
    """Linhas sessao + decisao + features de uma pergunta real do log de demonstração."""
    sel = [l for l in linhas if f"sessao={sessao}" in l and (l.startswith("GUARDIA,sessao")
           or (re.search(rf",ordem={ordem},", l) and l.split(",")[1] in ("decisao", "features")))]
    return "\n".join(sel) + "\n"


def _conf(pg) -> tuple[int, int]:
    m = re.search(r"(\d+) de (\d+) previsões", pg.text_content("#gvrConf"))
    return (int(m.group(1)), int(m.group(2))) if m else (0, 0)


def _replay_fim(pg, arquivo: Path):
    pg.set_input_files("#gvArquivo", str(arquivo))
    pg.click("#gvFim")


def test_pesos_e_html_conferem_com_as_fontes():
    for script in ("extrair_modelo.py", "montar_painel.py"):
        r = subprocess.run([sys.executable, str(RAIZ / "ferramentas" / script), "--verificar"], capture_output=True, text=True)
        assert r.returncode == 0, r.stdout + r.stderr
    js = (RAIZ / "ferramentas" / "modelo_guardia.js").read_text(encoding="utf-8")
    assert MODELO in js and "e6a4cf0086ccf5ab1eccecbe7aea02f3962a6ad7ced29b136d36c5b37b5b9991" in js
    # O bloco da rede só lê: não envia ao ESP32 nem mexe em áudio, protocolo ou logs.
    rede = (RAIZ / "ferramentas" / "rede_guardia.js").read_text(encoding="utf-8")
    assert not re.search(r"\bsend\(|\bstage\s*=|\bsession\s*=|\bready\s*=|guardiaLinhas|tone\(|music\.|\bawait\b", rede)


def test_reproduz_os_400_vetores_de_equivalencia(navegador, tmp_path):
    """Vetores da referência Python, formatados como o firmware registra (6 casas). Linhas sintéticas."""
    np = pytest.importorskip("numpy")
    t = VETORES.read_text(encoding="utf-8")
    padrao = re.compile(r"\{(\d), (\d), (\d), (\d), ([-\d.e]+)f, ([-\d.e]+)f, ([-\d.e]+)f, ([-\d.e]+)f, (-?\d), (-?\d)\}, ([-\d.e]+)f")
    f6 = lambda v: format(float(np.float32(float(v))), ".6f")
    linhas = ["# DADOS DE TESTE SINTETICOS: os 400 vetores de equivalencia de vetores_teste.h no formato dos logs."]
    n = 0
    for k, g in enumerate(padrao.findall(t)):
        nc, ni, nr, cat, ts, tc, tm, dq, ru, sq, p = g
        if float(p) < 0:
            continue
        s = f"V{k:07d}"
        ps = ",".join(f"p{x}=" + (f6(p) if str(x) == nc else "NA") for x in range(3))
        linhas += [f"GUARDIA,sessao,sessao={s},modelo={MODELO},nivel_inicial={ni},politica=fluxo-0.8",
                   f"GUARDIA,decisao,modelo={MODELO},sessao={s},ordem=2,nivel_inicial={ni},categoria={cat},niveis_disponiveis={nc},"
                   f"{ps},alvo=0.70,nivel_escolhido={nc},p_escolhido={f6(p)},motivo=guardia",
                   f"GUARDIA,features,sessao={s},ordem=2,nivel_candidato={nc},nivel_inicial={ni},n_respondidas={nr},categoria={cat},"
                   f"taxa_acerto_sessao={f6(ts)},taxa_acerto_categoria={f6(tc)},tempo_medio_norm={f6(tm)},dicas_por_questao={f6(dq)},"
                   f"resultado_ultima={ru},sequencia={sq}"]
        n += 1
    assert n == 400
    arq = tmp_path / "vetores_como_log.txt"
    arq.write_text("\n".join(linhas) + "\n", encoding="utf-8")
    ctx, pg, erros = _abrir(navegador)
    _replay_fim(pg, arq)
    assert _conf(pg) == (400, 400), pg.text_content("#gvrConf")
    assert erros == []
    ctx.close()


def _carregar_gerador():
    spec = importlib.util.spec_from_file_location("gerar_demos_mod", RAIZ / "ferramentas" / "gerar_demos.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


@pytest.mark.parametrize("gradual", [0, 1])
def test_reproduz_todas_as_previsoes_de_150_partidas_do_firmware(navegador, tmp_path, gradual):
    if not __import__("shutil").which("g++"):
        pytest.skip("g++ não encontrado")
    mod = _carregar_gerador()
    exe = mod.compilar(tmp_path, gradual)
    arq = tmp_path / f"massa_{gradual}.txt"
    r = subprocess.run([str(exe), str(arq), "massa"], capture_output=True, text=True, timeout=600)
    assert r.returncode == 0, r.stderr
    texto = arq.read_text(encoding="utf-8")
    esperadas = sum(len(re.findall(r",p[012]=[0-9.]+", l)) for l in texto.splitlines() if " GUARDIA,decisao," in l)
    assert esperadas > 400
    ctx, pg, erros = _abrir(navegador)
    _replay_fim(pg, arq)
    ok, n = _conf(pg)
    assert n == esperadas and ok == n, pg.text_content("#gvrConf")
    assert erros == []
    ctx.close()


def test_animacao_sequencia_decisao_e_rotulos(navegador):
    ctx, pg, erros = _abrir(navegador)
    _conectar(pg)
    pg.select_option("#gvrRitmo", "0.3")
    pg.evaluate(f"() => window.__esp('{_js(_pergunta(_linhas_demo(), 'C6E5747A', 2))}')")
    pg.wait_for_function("document.getElementById('gvrAvaliando').textContent.startsWith('Avaliando: Explorador (1 de 3)')")
    rede = " ".join(pg.inner_text("#gvRede").split())
    assert "cálculo reproduzido no painel" in rede and "Não são medições internas da placa" in rede
    assert "visualização em câmera lenta" in rede and "recurso visual" in rede
    assert pg.locator("#gvrSeq li.gvr-avaliando").count() == 1 and pg.locator("#gvrSeq li.gvr-pendente").count() == 2
    assert pg.is_hidden("#gvrDecisao")
    pg.wait_for_selector("#gvrA1.gvr-pulso", timeout=3000)
    _capturar(pg, "10_rede_avaliando.png")
    pg.wait_for_function("document.getElementById('gvrAvaliando').textContent.startsWith('Avaliando: Desafiante (2 de 3)')", timeout=8000)
    pg.wait_for_selector("#gvrDecisao:not([hidden])", timeout=15000)
    seq = pg.text_content("#gvrSeq")
    assert seq.count("confere") == 3 and "NÃO CONFERE" not in seq
    assert "◆ escolhido pela política" in pg.text_content("#gvrSeq li:nth-child(2)")
    dec = " ".join(pg.inner_text("#gvrDecisao").split())
    assert dec.startswith("DECISÃO DA POLÍTICA (não é a rede)") and "fluxo-0.8: Desafiante" in dec
    assert "a rede mostra a última (Mestre)" in pg.text_content("#gvrAvaliando")
    assert pg.locator(".gvr-pulso").count() == 0
    _capturar(pg, "11_rede_decisao_da_politica.png")
    # Clique numa avaliação: mostra a previsão daquele nível, parada.
    pg.click("#gvrSeq li:nth-child(1)")
    assert "Mostrando a avaliação de Explorador (previsão da rede, não a decisão)" in pg.text_content("#gvrAvaliando")
    assert erros == []
    ctx.close()


def test_nao_acumula_animacoes_nem_atrasa_o_protocolo(navegador):
    ctx, pg, erros = _abrir(navegador)
    _conectar(pg)
    linhas = _linhas_demo()
    pg.evaluate("""() => { window.__textos = []; new MutationObserver(() =>
        window.__textos.push(document.getElementById('gvrAvaliando').textContent)).observe(
        document.getElementById('gvrAvaliando'), {childList: true, characterData: true, subtree: true}); }""")
    for ordem in (1, 2, 3):  # três perguntas em sequência rápida
        pg.evaluate(f"() => window.__esp('{_js(_pergunta(linhas, 'C6E5747A', ordem))}')")
        pg.wait_for_timeout(150)
    pg.wait_for_timeout(400)
    pg.evaluate("() => { window.__textos = []; }")
    # Durante a animação, o painel responde ao ESP32 na hora (PREP -> READY).
    antes = pg.evaluate("performance.now()")
    pg.evaluate("() => window.__esp('AG,PREP,0BADC0DE,1234\\nAG,STATE,0BADC0DE,PREP,0,0\\n')")
    pg.wait_for_function("window.__escritas.some(e=>e[1].includes('AG,READY,0BADC0DE'))")
    t_ready = pg.evaluate("window.__escritas.find(e=>e[1].includes('AG,READY,0BADC0DE'))[0]")
    assert t_ready - antes < 300
    # O PREP leva ao Agente Secreto: a rede para e avisa que a Guardiã não participa.
    pg.wait_for_function("document.getElementById('gvrMsg').textContent.includes('não participa')")
    assert pg.locator(".gvr-pulso").count() == 0 and pg.locator("#gvrSeq li").count() == 0
    assert pg.text_content("#gvrAvaliando") == "" and pg.locator("#gvrSvg.gvr-desligada").count() == 1
    pg.wait_for_timeout(2500)  # nenhum passo pendente de animação antiga roda depois
    assert pg.evaluate("window.__textos.filter(t=>t)") == []
    assert pg.locator(".gvr-pulso").count() == 0
    _capturar(pg, "12_rede_agente_secreto.png")
    # Volta ao menu e a pergunta seguinte anima só ela, uma vez.
    pg.evaluate("() => window.__esp('AG,STATE,0BADC0DE,LOSE,0,0\\nAG,STATE,0BADC0DE,IDLE,0,0\\n')")
    pg.evaluate("() => { window.__textos = []; }")
    pg.evaluate(f"() => window.__esp('{_js(_pergunta(linhas, '826D104C', 2))}')")
    pg.wait_for_timeout(1200)
    inicios = [t for t in pg.evaluate("window.__textos") if "(1 de" in t]
    assert len(set(inicios)) == 1 and pg.locator("#gvrSeq li.gvr-avaliando").count() == 1
    assert erros == []
    ctx.close()


def test_indisponivel_e_divergencia(navegador):
    ctx, pg, erros = _abrir(navegador)
    _conectar(pg)
    linhas = _linhas_demo()
    dec = [l for l in linhas if l.startswith("GUARDIA,decisao") and "sessao=C6E5747A" in l and ",ordem=2," in l][0]
    fea = [l for l in linhas if l.startswith("GUARDIA,features") and "sessao=C6E5747A" in l and ",ordem=2," in l][0]
    # Sem a linha features: nada é inventado.
    pg.evaluate(f"() => window.__esp('{_js('GUARDIA,sessao,sessao=AAAA0001,nivel_inicial=1,politica=fluxo-0.8' + chr(10) + dec.replace('C6E5747A', 'AAAA0001') + chr(10))}')")
    pg.wait_for_function("document.getElementById('gvrMsg').textContent.includes('features desta pergunta ausente')")
    assert pg.locator("#gvrSeq li").count() == 0 and pg.is_visible("#gvrDecisao")
    # Modelo diferente do embutido: reprodução recusada.
    outro = dec.replace("C6E5747A", "AAAA0002").replace(f"modelo={MODELO}", "modelo=mlp8-outro")
    pg.evaluate(f"() => window.__esp('{_js('GUARDIA,sessao,sessao=AAAA0002,nivel_inicial=1' + chr(10) + outro + chr(10) + fea.replace('C6E5747A', 'AAAA0002') + chr(10))}')")
    pg.wait_for_function("document.getElementById('gvrMsg').textContent.includes('o log é do modelo mlp8-outro')")
    # Feature adulterada: a divergência aparece, não é escondida.
    ruim = re.sub(r"taxa_acerto_sessao=[0-9.]+", "taxa_acerto_sessao=0.900000", fea.replace("C6E5747A", "AAAA0003"))
    pg.uncheck("#gvrAnimar")
    pg.evaluate(f"() => window.__esp('{_js('GUARDIA,sessao,sessao=AAAA0003,nivel_inicial=1,politica=fluxo-0.8' + chr(10) + dec.replace('C6E5747A', 'AAAA0003') + chr(10) + ruim + chr(10))}')")
    pg.wait_for_function("document.getElementById('gvrSeq').textContent.includes('NÃO CONFERE')")
    assert "Há previsões que não conferem" in pg.text_content("#gvrConf")
    assert erros == []
    ctx.close()


def test_movimento_reduzido_mostra_estado_final(navegador):
    ctx, pg, erros = _abrir(navegador, reduced_motion="reduce")
    _conectar(pg)
    assert not pg.is_checked("#gvrAnimar")
    pg.evaluate(f"() => window.__esp('{_js(_pergunta(_linhas_demo(), 'C6E5747A', 3))}')")
    pg.wait_for_selector("#gvrDecisao:not([hidden])", timeout=2000)
    assert pg.is_hidden("#gvrLenta") and pg.locator(".gvr-pulso").count() == 0
    assert "Sem animação" in pg.text_content("#gvrAvaliando")
    assert erros == []
    ctx.close()


def _capturar(pg, nome):
    CAPTURAS.mkdir(parents=True, exist_ok=True)
    pg.locator("#gvRede").screenshot(path=str(CAPTURAS / nome))
