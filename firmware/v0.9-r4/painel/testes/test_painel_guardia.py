"""Testes da proposta de visualização da Guardiã no painel USB (Chromium + porta serial falsa).

Executar a partir de firmware/v0.9-r4/painel/:
    python3 -m pytest -q testes/test_painel_guardia.py

Verifica:
- o protocolo AG, as pistas, o relógio e os logs se comportam igual ao painel r3;
- o quadro mostra a previsão da MLP separada da escolha da política;
- campos ausentes aparecem como "indisponível", sem justificativa inventada;
- no Agente Secreto o quadro avisa que a Guardiã não participa;
- a reprodução de log é marcada como DADOS DE DEMONSTRAÇÃO, não envia nada
  ao ESP32 e não entra na área de logs salvos.
"""

from __future__ import annotations

import os
import re
from pathlib import Path

import pytest

RAIZ = Path(__file__).resolve().parents[1]
PROPOSTA = RAIZ / "Coflume_Agente_Secreto_USB.html"
R3 = RAIZ.parents[1] / "v0.9-r3" / "painel" / "Coflume_Agente_Secreto_USB.html"
ORIGINAL_PLACA = os.environ.get("COFLUME_PAINEL_ORIGINAL")  # opcional: painel da placa
DEMO_FLUXO = RAIZ / "demo" / "demo_fluxo08.txt"
DEMO_GRADUAL = RAIZ / "demo" / "demo_gradual.txt"
DEMO_RECUSA = RAIZ / "demo" / "demo_recusa.txt"
CAPTURAS = RAIZ / "docs" / "capturas"

SERIAL_FALSO = """
(() => {
  const escritas = []; let controlador = null;
  const porta = {
    async open() {}, async close() {},
    readable: new ReadableStream({ start(c) { controlador = c; } }),
    writable: new WritableStream({ write(chunk) { escritas.push(new TextDecoder().decode(chunk)); } }),
  };
  Object.defineProperty(navigator, 'serial', { value: { async requestPort() { return porta; } } });
  window.__esp = (texto) => controlador.enqueue(new TextEncoder().encode(texto));
  window.__escritas = escritas;
})();
"""
SESSAO, SENHA = "1A2B3C4D", "4071"


def _js(texto: str) -> str:
    return texto.replace("\\", "\\\\").replace("'", "\\'").replace("\n", "\\n")


def _sem_pings_repetidos(escritas: list[str]) -> list[str]:
    """PINGs consecutivos viram um só: a quantidade depende do relógio (1 por segundo), não do comportamento."""
    out: list[str] = []
    for e in (re.sub(r"PING,.*", "PING", x) for x in escritas):
        if not (out and e == "AG,PING" and out[-1] == "AG,PING"):
            out.append(e)
    return out


def _roteiro_ag(pagina) -> dict:
    """Mesmo roteiro do test_painel.py da r3: conexão, missão com erro e vitória."""
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
    relogio = pagina.text_content("#clock")
    pagina.wait_for_timeout(1300)  # deixa passar um PING
    pagina.evaluate(f"() => window.__esp('AG,EVENT,{SESSAO},CORRECT\\nAG,STATE,{SESSAO},WIN,0,2\\nSERVO,angulo=0\\n')")
    pagina.wait_for_function("document.getElementById('status').textContent==='MISSAO CUMPRIDA'")
    escritas = "".join(pagina.evaluate("window.__escritas")).splitlines()
    return {
        "escritas": _sem_pings_repetidos(escritas),
        "pistas": pistas,
        "feedback_jogo": feedback_jogo,
        "relogio": relogio[:2],
        "status": pagina.text_content("#status"),
        "aviso": pagina.text_content("#notice"),
        "feedback": pagina.text_content("#feedback"),
        "botao_proximo": pagina.is_enabled("#next"),
        "botao_cancelar": pagina.is_enabled("#cancel"),
        "logs": pagina.text_content("#logCount") if pagina.locator("#logCount").count() else None,
        "log_linhas": [l.split(" ", 1)[1] for l in pagina.text_content("#logView").splitlines()]
        if pagina.locator("#logView").count() else None,
    }


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


def _abrir(navegador, arquivo: Path, largura=1280, altura=1400):
    ctx = navegador.new_context(viewport={"width": largura, "height": altura}, accept_downloads=True)
    ctx.add_init_script(SERIAL_FALSO)
    pagina = ctx.new_page()
    erros: list[str] = []
    pagina.on("pageerror", lambda e: erros.append(str(e)))
    pagina.goto(arquivo.as_uri())
    return ctx, pagina, erros


def _quadro(pagina) -> str:
    return " ".join(pagina.inner_text("#guardiaVis").split())


def _capturar(pagina, nome: str):
    CAPTURAS.mkdir(parents=True, exist_ok=True)
    pagina.locator("#guardiaVis").screenshot(path=str(CAPTURAS / nome))


def _linhas(arquivo: Path) -> list[str]:
    out = []
    for l in arquivo.read_text(encoding="utf-8").splitlines():
        if l.startswith("#") or not l.strip():
            continue
        out.append(l.split(" ", 1)[1] if re.match(r"^\d{4}-\d\d-\d\dT", l) else l)
    return out


def test_fontes_da_proposta_estao_no_html():
    import subprocess
    import sys
    r = subprocess.run([sys.executable, str(RAIZ / "ferramentas" / "montar_painel.py"), "--verificar"], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout + r.stderr
    html = PROPOSTA.read_text(encoding="utf-8")
    assert (RAIZ / "ferramentas" / "visualizacao_guardia.js").read_text(encoding="utf-8") in html
    assert (RAIZ / "ferramentas" / "visualizacao_guardia.css").read_text(encoding="utf-8") in html
    # Acréscimo puro: todas as linhas do painel r3 continuam lá, na mesma ordem, exceto o gancho em packet().
    r3 = R3.read_text(encoding="utf-8").splitlines()
    novo = html.splitlines()
    it = iter(novo)
    assert all(any(l == n for n in it) for l in r3), "alguma linha do painel r3 foi alterada ou removida"
    extra = [l for l in novo if l not in set(r3)]
    assert "    guardiaVisUSB(line);" in extra
    # O quadro não chama send() nem mexe nas variáveis de estado do protocolo.
    bloco = (RAIZ / "ferramentas" / "visualizacao_guardia.js").read_text(encoding="utf-8")
    assert not re.search(r"\bsend\(|\bstage\s*=|\bsession\s*=|\bready\s*=|guardiaLinhas|tone\(|music\.", bloco)


@pytest.mark.parametrize("base", ["r3", "placa"])
def test_protocolo_ag_igual_ao_painel_base(navegador, base):
    arquivo_base = R3 if base == "r3" else (Path(ORIGINAL_PLACA) if ORIGINAL_PLACA else None)
    if arquivo_base is None or not arquivo_base.is_file():
        pytest.skip("defina COFLUME_PAINEL_ORIGINAL com o HTML da placa para comparar")
    res = []
    for arquivo in (arquivo_base, PROPOSTA):
        ctx, pagina, erros = _abrir(navegador, arquivo)
        r = _roteiro_ag(pagina)
        assert erros == []
        if arquivo == PROPOSTA:  # durante a missão o quadro mostra o aviso do Agente Secreto
            assert "A Guardiã IA não participa desta modalidade" in _quadro(pagina)
        res.append(r)
        ctx.close()
    if base == "placa":  # o painel da placa não tem área de logs
        for r in res:
            r.pop("logs"), r.pop("log_linhas")
    assert res[0] == res[1]


def test_ao_vivo_fluxo08_quiz_agente_e_quiz(navegador):
    """Log real do firmware r3 (simulado no PC) chegando pela USB, como no cofre."""
    linhas = _linhas(DEMO_FLUXO)
    fim1 = next(i for i, l in enumerate(linhas) if l.startswith("GUARDIA,fim"))
    prep = next(i for i, l in enumerate(linhas) if l.startswith("AG,PREP"))
    ctx, pagina, erros = _abrir(navegador, PROPOSTA)
    pagina.click("#connect")
    pagina.wait_for_function("window.__escritas.join('').includes('AG,HELLO')")
    assert "Nenhuma partida de Quiz recebida ainda" in _quadro(pagina)
    pagina.evaluate(f"() => window.__esp('{_js(chr(10).join(linhas[: fim1 + 1]) + chr(10))}')")
    pagina.wait_for_function("document.getElementById('gvConteudo').textContent.includes('Desbloqueio: NÃO')")
    q = _quadro(pagina)
    assert "AO VIVO · USB" in q and "DADOS DE DEMONSTRAÇÃO" not in q
    assert "Política ativa: fluxo-0.8 (informada no início da partida)" in q
    assert "1 · PREVISÃO DA MLP" in q and "2 · ESCOLHA DA POLÍTICA fluxo-0.8" in q
    assert "Não aplicável: a política ativa é fluxo-0.8, que não tem proteção gradual." in q
    assert "acertou" in q and "errou" in q and "tempo esgotado" in q
    assert "Tempo somado das respostas: 48,9 s" in q and "Tempo total" not in q
    assert "Acertos: 1/3" in q and "Dicas usadas: 1" in q
    # Barras: 3 níveis, o escolhido marcado em texto (não só por cor) e a linha do alvo de 70%.
    assert pagina.locator(".gv-bar-row").count() == 3
    assert pagina.locator(".gv-escolhido .gv-marca").count() == 1
    assert pagina.locator(".gv-alvo").count() == 3
    assert pagina.eval_on_selector(".gv-alvo", "e => e.style.left") == "70%"
    assert "estimativas de um modelo experimental" in q.lower() and "não certeza" in q
    _capturar(pagina, "01_ao_vivo_fluxo08_fim_da_partida.png")
    # Clique na pergunta 1: a MLP não decide; estimativas não avaliadas aparecem como indisponíveis.
    pagina.click("tr[data-ordem='1']")
    q = _quadro(pagina)
    assert "PERGUNTA 1 DE 3" in q and "a MLP não decide esta pergunta" in q
    assert q.count("indisponível · não avaliado nesta pergunta") == 2
    # Agente Secreto: quadro avisa que a Guardiã não participa.
    pagina.evaluate(f"() => window.__esp('{_js(chr(10).join(linhas[fim1 + 1 : prep + 30]) + chr(10))}')")
    pagina.wait_for_function("document.getElementById('status').textContent==='MISSAO EM ANDAMENTO'")
    pagina.wait_for_selector("#gvAgente")
    q = _quadro(pagina)
    assert "MODO AGENTE SECRETO · missão em andamento" in q and "A Guardiã IA não participa desta modalidade" in q
    assert "PREVISÃO DA MLP" not in q
    _capturar(pagina, "03_agente_secreto_guardia_nao_participa.png")
    # Resto do log: fim da missão, menu e um Quiz Explorador com 3 acertos.
    pagina.evaluate(f"() => window.__esp('{_js(chr(10).join(linhas[prep + 30 :]) + chr(10))}')")
    pagina.wait_for_function("document.getElementById('gvConteudo').textContent.includes('Desbloqueio: SIM')")
    q = _quadro(pagina)
    assert "Acertos: 3/3" in q and "Desbloqueio: SIM · cofre aberto" in q and "Dicas usadas: 1" in q
    # Protocolo: o painel continua respondendo só HELLO, READY e PING.
    escritas = "".join(pagina.evaluate("window.__escritas")).splitlines()
    assert {e.split(",")[1] for e in escritas} <= {"HELLO", "READY", "PING"}
    # Área de logs: só linhas GUARDIA, como antes.
    assert int(pagina.text_content("#logCount")) == sum(l.startswith("GUARDIA,") for l in linhas)
    assert erros == []
    ctx.close()


def test_reproducao_demo_gradual(navegador):
    ctx, pagina, erros = _abrir(navegador, PROPOSTA)
    pagina.set_input_files("#gvArquivo", str(DEMO_GRADUAL))
    pagina.wait_for_selector("#gvDemo:not([hidden])")
    q = _quadro(pagina)
    assert "REPRODUÇÃO DE LOG" in q and "DADOS DE DEMONSTRAÇÃO" in q
    assert "não vêm do cofre conectado nem de crianças" in q
    assert "Cabeçalho do arquivo: DADOS DE DEMONSTRACAO" in q
    # Avança evento a evento até o fim da partida.
    for _ in range(400):
        if "Desbloqueio: NÃO" in pagina.text_content("#gvConteudo"):
            break
        pagina.click("#gvPasso")
    q = _quadro(pagina)
    assert "Política ativa: gradual-v2-exp" in q
    assert "PERGUNTA 3 DE 3" in q
    assert "PROTEÇÃO ATUOU · sem a regra gradual, o fluxo-0.8 escolheria Mestre; a regra limitou para Desafiante." in q
    assert "não subir de nível depois de erro ou tempo esgotado" in q
    assert "Pergunta anterior: Desafiante, errou ou esgotou o tempo" in q
    # A previsão da MLP para Mestre continua visível: a regra muda a escolha, não a estimativa.
    linha_mestre = pagina.text_content(".gv-bar-row[data-nivel='2']")
    assert "≈ 60,0% · 10,0 p.p. abaixo do alvo" in linha_mestre and "escolhido" not in linha_mestre
    assert "◆ escolhido pela política" in pagina.text_content(".gv-bar-row[data-nivel='1']")
    assert "≈ 80,1% · 10,1 p.p. acima do alvo" in pagina.text_content(".gv-bar-row[data-nivel='1']")
    assert pagina.locator("#gvEmpate").count() == 0  # 10,1 e 10,0 p.p.: sem empate visual
    tabela = pagina.text_content(".gv-tabela tbody")
    assert "atuou" in tabela and "não precisou" in tabela
    _capturar(pagina, "02_demo_gradual_protecao_atuou.png")
    pagina.click("tr[data-ordem='2']")
    assert "Não precisou intervir: a escolha do fluxo-0.8 (Desafiante) já era compatível." in _quadro(pagina)
    # Depois da partida, o log segue para uma missão do Agente Secreto.
    for _ in range(50):
        if pagina.locator("#gvAgente").count():
            break
        pagina.click("#gvPasso")
    assert "A Guardiã IA não participa desta modalidade" in _quadro(pagina)
    # Ir ao fim: a missão termina, o cofre volta ao menu e o quadro mostra a última partida, encerrada.
    pagina.click("#gvFim")
    assert pagina.text_content("#gvProg").startswith("linha 149 de 149")
    assert "Partida de Quiz 92C2E568 · encerrada" in _quadro(pagina)
    # Nada foi enviado ao ESP32 e nada entrou nos logs salvos.
    assert pagina.evaluate("window.__escritas") == []
    assert pagina.text_content("#logCount") == "0"
    # Reproduzir com velocidade: avança sozinho; pausar para.
    pagina.set_input_files("#gvArquivo", str(DEMO_GRADUAL))
    pagina.select_option("#gvVel", "64")
    pagina.click("#gvPlay")
    pagina.wait_for_function("parseInt(document.getElementById('gvProg').textContent.split(' ')[1]) > 20")
    pagina.click("#gvPlay")
    parado = pagina.text_content("#gvProg")
    pagina.wait_for_timeout(600)
    assert pagina.text_content("#gvProg") == parado
    pagina.click("#gvSair")
    assert pagina.is_hidden("#gvDemo") and "AO VIVO · USB" in _quadro(pagina)
    assert erros == []
    ctx.close()


def test_reproducao_nao_mistura_com_ao_vivo(navegador):
    ctx, pagina, erros = _abrir(navegador, PROPOSTA)
    pagina.click("#connect")
    pagina.wait_for_function("window.__escritas.join('').includes('AG,HELLO')")
    pagina.set_input_files("#gvArquivo", str(DEMO_GRADUAL))
    pagina.click("#gvFim")
    pagina.evaluate("() => window.__esp('AG,HELLO,1,1\\nGUARDIA,sessao,sessao=ABCD0001,nivel_inicial=0,politica=fluxo-0.8\\n')")
    pagina.wait_for_function("document.getElementById('gvAoVivo').textContent.includes('ao vivo recebidas')")
    assert "ABCD0001" not in pagina.text_content("#gvConteudo")
    pagina.click("#gvSair")
    q = _quadro(pagina)
    assert "Partida de Quiz ABCD0001 · em andamento" in q and "Política ativa: fluxo-0.8" in q
    assert pagina.text_content("#logCount") == "1"  # só a linha ao vivo
    assert erros == []
    ctx.close()


def test_campos_ausentes_ficam_indisponiveis(navegador):
    """Linhas incompletas (por exemplo, de firmware antigo): nada é inventado."""
    ctx, pagina, erros = _abrir(navegador, PROPOSTA)
    pagina.click("#connect")
    pagina.wait_for_function("window.__escritas.join('').includes('AG,HELLO')")
    pagina.evaluate("""() => window.__esp('GUARDIA,decisao,modelo=mlp8,ordem=2,p0=0.81,p2=1.7,nivel_escolhido=1\\n'
      + 'GUARDIA,resposta,ordem=2,acertou=1\\n')""")
    pagina.wait_for_selector(".gv-bar-row")
    q = _quadro(pagina)
    assert "Política ativa: indisponível · não informada no log" in q
    assert "início da partida ausente no log" in q
    assert "≈ 81,0%" in q
    assert "Desafiante" in q and "indisponível · campo ausente no log" in q  # p1 ausente
    assert "indisponível · valor inválido no log: 1.7" in q
    assert "indisponível · alvo ausente no log" in q and pagina.locator(".gv-alvo").count() == 0
    assert "Por quê: indisponível · motivo ausente no log" in q
    assert "indisponível · o log não informa a política ativa" in q
    assert "Acertos: 1/3 até agora" in q and "Dicas usadas: indisponível" in q and "Tempo somado das respostas: indisponível" in q
    _capturar(pagina, "04_campos_ausentes_indisponivel.png")
    # Motivo desconhecido: mostrado como registrado, sem descrição inventada.
    pagina.evaluate("() => window.__esp('GUARDIA,decisao,ordem=3,p0=NA,p1=0.5,p2=NA,nivel_escolhido=1,motivo=xyz_novo,alvo=0.70\\n')")
    pagina.wait_for_function("document.getElementById('gvConteudo').textContent.includes('xyz_novo')")
    assert "Motivo registrado: xyz_novo indisponível · sem descrição neste painel" in _quadro(pagina)
    # Texto vindo da USB é escapado.
    pagina.evaluate("() => window.__esp('GUARDIA,decisao,ordem=1,pergunta=<img src=x onerror=window.__xss=1>,nivel_escolhido=0\\n')")
    pagina.click("tr[data-ordem='1']")
    pagina.wait_for_function("document.getElementById('gvConteudo').textContent.includes('<img')")
    assert pagina.locator("#gvConteudo img").count() == 0 and pagina.evaluate("window.__xss") is None
    assert erros == []
    ctx.close()


def test_recusa_gradual_e_banco_inviavel(navegador):
    """Formato das linhas do firmware r3 na recusa (coflume_v09.ino, logGradual e erroBanco).
    Linhas sintéticas escritas para o teste."""
    ctx, pagina, erros = _abrir(navegador, PROPOSTA)
    pagina.click("#connect")
    pagina.wait_for_function("window.__escritas.join('').includes('AG,HELLO')")
    pagina.evaluate("""() => window.__esp(
        'GUARDIA,estado,adaptativa_configurada=1,banco_viavel=1,guardia_ativa=1,servo=OK,politica=gradual-v2-exp,banco_viavel_gradual=1\\n'
      + 'GUARDIA,sessao,sessao=0000BEEF,modelo=mlp8-sint-20261101-e6a4cf00,adaptativa=1,nivel_inicial=2,ordem_categorias=0123,politica=gradual-v2-exp\\n'
      + 'GUARDIA,politica_gradual,politica=gradual-v2-exp,sessao=0000BEEF,ordem=2,anterior=2,anterior_acertou=0,niveis_compativeis=1|2,candidatos=-,categoria_fluxo08=1,categoria=-1,nivel_sem_regra=-1,nivel=-1,limitou=0,motivo=sem_nivel_compativel\\n'
      + 'GUARDIA,erro,sem_nivel_compativel,politica=gradual-v2-exp\\n')""")
    pagina.wait_for_function("document.getElementById('gvConteudo').textContent.includes('RECUSA')")
    q = _quadro(pagina)
    assert "RECUSA · nenhum nível compatível" in q and "Guardiã recusou a próxima pergunta" in q and "sem_nivel_compativel" in q
    assert "linha GUARDIA,decisao desta pergunta ausente no log" in q
    assert "com pergunta no banco: nenhum" in q
    assert "Banco viável: sim · para a regra gradual: sim" in q
    assert pagina.locator(".gv-recusa").count() == 1
    assert "recusou" in pagina.text_content(".gv-tabela tbody")
    _capturar(pagina, "05_recusa_sem_nivel_compativel.png")
    assert erros == []
    ctx.close()


def test_tela_estreita_sem_rolagem_horizontal(navegador):
    ctx, pagina, erros = _abrir(navegador, PROPOSTA, largura=390, altura=900)
    pagina.set_input_files("#gvArquivo", str(DEMO_GRADUAL))
    for _ in range(400):
        if "Desbloqueio" in pagina.text_content("#gvConteudo") and "NÃO" in pagina.text_content(".gv-totais"):
            break
        pagina.click("#gvPasso")
    assert pagina.evaluate("document.documentElement.scrollWidth <= window.innerWidth + 1")
    _capturar(pagina, "06_tela_estreita.png")
    assert erros == []
    ctx.close()


def test_empate_visual_com_uma_casa_decimal(navegador):
    """Distâncias exatas diferentes (4,99 e 4,98 p.p.) que aparecem iguais com 1 casa (5,0)."""
    ctx, pagina, erros = _abrir(navegador, PROPOSTA)
    pagina.click("#connect")
    pagina.wait_for_function("window.__escritas.join('').includes('AG,HELLO')")
    pagina.evaluate("""() => window.__esp('GUARDIA,sessao,sessao=0000E1E1,nivel_inicial=1,politica=fluxo-0.8\\n'
      + 'GUARDIA,decisao,sessao=0000E1E1,ordem=2,nivel_inicial=1,niveis_permitidos=0|1|2,niveis_disponiveis=0|1|2,'
      + 'p0=0.650100,p1=0.900000,p2=0.749800,alvo=0.70,nivel_escolhido=2,p_escolhido=0.749800,motivo=guardia\\n')""")
    pagina.wait_for_selector("#gvEmpate")
    q = _quadro(pagina)
    assert "≈ 65,0% · 5,0 p.p. abaixo do alvo" in q and "≈ 75,0% · 5,0 p.p. acima do alvo" in q
    assert "Empate visual: Mestre e Explorador aparecem iguais com 1 casa decimal" in q
    assert "valores sem arredondamento (p2=0.749800, p0=0.650100)" in q
    assert "menor distância exata do alvo; se ainda empatar, o nível inicial; depois, o nível mais baixo" in q
    assert pagina.get_attribute(".gv-bar-row[data-nivel='2']", "title") == "valor registrado no log: p2=0.749800"
    _capturar(pagina, "07_empate_visual.png")
    # Pergunta 1 ou fallback: as estimativas não decidem, então não há aviso de empate.
    pagina.evaluate("""() => window.__esp('GUARDIA,decisao,sessao=0000E1E1,ordem=3,nivel_inicial=1,niveis_disponiveis=0|2,'
      + 'p0=0.650100,p2=0.749800,alvo=0.70,nivel_escolhido=0,motivo=fallback_previsao_invalida\\n')""")
    pagina.wait_for_function("document.getElementById('gvConteudo').textContent.includes('PERGUNTA 3 DE 3')")
    assert pagina.locator("#gvEmpate").count() == 0
    assert erros == []
    ctx.close()


def test_reproducao_demo_recusa_cenario_forcado(navegador):
    """Log do firmware simulado com banco inviável: bloqueio normal e, depois, cenário forçado."""
    ctx, pagina, erros = _abrir(navegador, PROPOSTA)
    pagina.set_input_files("#gvArquivo", str(DEMO_RECUSA))
    pagina.wait_for_selector("#gvDemo:not([hidden])")
    q = _quadro(pagina)
    assert "BANCO INVIAVEL PARA A REGRA GRADUAL" in q and "CENARIO FORCADO PARA TESTE" in q
    assert pagina.is_hidden("#gvDemoNota")
    # Parte 1: a verificação inicial bloqueia o Quiz.
    for _ in range(100):
        if "Quiz bloqueado" in pagina.text_content("#gvConteudo"):
            break
        pagina.click("#gvPasso")
    q = _quadro(pagina)
    assert "Quiz bloqueado na verificação inicial" in q and "banco_inviavel_gradual" in q
    assert "para a regra gradual: NÃO" in q and "interrompida" in q
    assert pagina.is_hidden("#gvDemoNota")
    _capturar(pagina, "08_demo_banco_inviavel_bloqueio_normal.png")
    # Parte 2: a nota do arquivo marca o cenário forçado no trecho em que ele começa.
    pagina.click("#gvPasso")
    assert pagina.is_visible("#gvDemoNota")
    assert "NOTA DO ARQUIVO NESTE TRECHO: CENARIO FORCADO" in pagina.text_content("#gvDemoNota")
    for _ in range(100):
        if "Guardiã recusou a próxima pergunta" in pagina.text_content("#gvConteudo"):
            break
        pagina.click("#gvPasso")
    q = _quadro(pagina)
    assert "CENARIO FORCADO" in pagina.text_content("#gvDemoNota")
    assert "RECUSA · nenhum nível compatível" in q and "Guardiã recusou a próxima pergunta" in q
    assert "PERGUNTA 3 DE 3" in q and "com pergunta no banco: nenhum" in q
    assert "Pergunta anterior: Desafiante, errou ou esgotou o tempo" in q
    tabela = pagina.text_content(".gv-tabela tbody")
    assert "recusou" in tabela
    assert "Desbloqueio: NÃO · partida interrompida" in q and "Acertos: 0/3" in q
    _capturar(pagina, "09_demo_recusa_cenario_forcado.png")
    pagina.click("#gvFim")
    assert pagina.evaluate("window.__escritas") == [] and pagina.text_content("#logCount") == "0"
    assert erros == []
    ctx.close()
