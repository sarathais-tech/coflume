"""Testes do MONITOR DA IA (dois monitores) com o HTML aberto localmente (file://) no Chromium.

Executar a partir de firmware/v0.9-r4/painel/:
    python3 -m pytest -q testes/test_monitor_ia.py

O painel principal é o único que usa a (falsa) porta USB. A janela de exposição é o mesmo
arquivo com ?monitor=ia, aberta pelo botão; recebe o estado por postMessage.
"""
from __future__ import annotations

import importlib.util
import json
import re
from pathlib import Path

import pytest

RAIZ = Path(__file__).resolve().parents[1]
PAINEL = RAIZ / "Coflume_Agente_Secreto_USB.html"
R3 = RAIZ.parents[1] / "v0.9-r3" / "painel" / "Coflume_Agente_Secreto_USB.html"
DEMO_FLUXO = RAIZ / "demo" / "demo_fluxo08.txt"
DEMO_RECUSA = RAIZ / "demo" / "demo_recusa.txt"
CAPTURAS = RAIZ / "docs" / "capturas"
SENHA = "4071"

# Porta serial falsa (em todas as páginas do contexto) e registro das mensagens recebidas por janela.
INIT = """
(() => {
  const escritas = []; let controlador = null; window.__pedidosPorta = 0;
  const porta = {
    async open() {}, async close() {},
    readable: new ReadableStream({ start(c) { controlador = c; } }),
    writable: new WritableStream({ write(chunk) { escritas.push(new TextDecoder().decode(chunk)); } }),
  };
  Object.defineProperty(navigator, 'serial', { value: { async requestPort() { window.__pedidosPorta++; return porta; } } });
  window.__esp = (texto) => controlador.enqueue(new TextEncoder().encode(texto));
  window.__fecharUSB = () => controlador.close();
  window.__escritas = escritas;
  window.__msgs = [];
  window.addEventListener('message', e => { try { window.__msgs.push(JSON.stringify(e.data)); } catch (x) {} });
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


class Sessao:
    def __init__(self, navegador, arquivo=PAINEL):
        self.ctx = navegador.new_context(viewport={"width": 1280, "height": 1000})
        self.ctx.add_init_script(INIT)
        self.erros: list[str] = []
        self.pg = self.ctx.new_page()
        self.pg.on("pageerror", lambda e: self.erros.append("principal: " + str(e)))
        self.pg.goto(arquivo.as_uri())
        self.mon = None

    def conectar(self):
        self.pg.click("#connect")
        self.pg.wait_for_function("window.__escritas.join('').includes('AG,HELLO')")
        self.esp("AG,HELLO,1,1\nAG,STATE,00000000,IDLE,0,0\n")
        self.pg.wait_for_function("document.getElementById('status').textContent==='COFRE PRONTO'")

    def esp(self, texto: str):
        self.pg.evaluate(f"() => window.__esp('{_js(texto)}')")

    def abrir_monitor(self, ritmo="0.3"):
        with self.ctx.expect_page() as info:
            self.pg.click("#gvMonitor")
        self.mon = info.value
        self.mon.on("pageerror", lambda e: self.erros.append("monitor: " + str(e)))
        self.mon.set_viewport_size({"width": 1920, "height": 1080})
        self.mon.wait_for_load_state()
        self.mon.wait_for_function("window.__msgs.some(m => m.includes('\"tipo\":\"estado\"'))")
        if ritmo:
            # controle de diagnóstico (recolhido): ritmo mais rápido só para o teste
            self.mon.eval_on_selector("#monRitmo", "(e, v) => { e.value = v; }", ritmo)
        return self.mon

    def texto_mon(self) -> str:
        return " ".join(self.mon.inner_text("#monitorIA").split())

    def fechar(self):
        self.ctx.close()


def _linhas() -> list[str]:
    return [l.split(" ", 1)[1] for l in DEMO_FLUXO.read_text(encoding="utf-8").splitlines() if l and not l.startswith("#")]


def _pergunta(sessao: str, ordem: int, resposta=True) -> str:
    tipos = ("decisao", "features", "politica_gradual") + (("resposta",) if resposta else ())
    sel = [l for l in _linhas() if f"sessao={sessao}" in l and (
        (ordem == 1 and l.startswith("GUARDIA,sessao")) or (re.search(rf",ordem={ordem},", l) and l.split(",")[1] in tipos))]
    return "\n".join(sel) + "\n"


def _capturar(pagina, nome):
    CAPTURAS.mkdir(parents=True, exist_ok=True)
    pagina.screenshot(path=str(CAPTURAS / nome))


def test_abrir_durante_a_partida_recebe_estado_completo_sem_usb(navegador):
    s = Sessao(navegador)
    s.conectar()
    s.esp(_pergunta("C6E5747A", 1) + _pergunta("C6E5747A", 2))
    mon = s.abrir_monitor()
    mon.wait_for_function("document.getElementById('monTitulo').textContent==='Última avaliação: Desafiante'")
    t = s.texto_mon()
    assert "Como a Guardiã escolhe o próximo desafio" in t
    assert "A rede neural estima a chance de acerto. A política do jogo usa essa estimativa para escolher a dificuldade." in t
    assert "AO VIVO · USB" in t and "cofre conectado" in t and "PERGUNTA 2 DE 3" in t
    assert "2 / A POLÍTICA ESCOLHE A DIFICULDADE (não é a rede)" in t
    assert "Desafiante tem a chance estimada mais próxima do alvo de 70% entre as permitidas." in t
    assert "Chance estimada de acerto" in t and "Alvo: 70%" in t
    # Dados usados antes da pergunta 2: 1 resposta certa, sem dica (da linha GUARDIA,features).
    assert "Dados usados antes da pergunta 2" in t
    dados = " ".join(mon.inner_text("#monDados").split())
    assert "1 de 1 acertos anteriores" in dados and "0 dicas usadas (total)" in dados and " s tempo médio aproximado" in dados
    assert "1 acerto em 2 respondidas (em andamento)" in mon.inner_text("#monPartida")
    assert mon.inner_text("#monPartidaCab") == "PARTIDA ATÉ AGORA"
    assert mon.inner_text("#monCofre") == "Aguardando o fim da partida"
    assert "cálculo reproduzido no painel" in t and "DADOS DE DEMONSTRAÇÃO" not in t
    assert mon.locator(".mon-barra").count() == 3 and "≈ 70," in mon.text_content("#monBarras")
    # Ao abrir no meio da partida: último resultado parado, sem simular processamento.
    assert mon.is_hidden("#monLenta") and mon.locator(".gvr-pulso").count() == 0
    # A janela de exposição não pede a porta USB nem roda o painel do Agente.
    assert mon.evaluate("window.__pedidosPorta") == 0 and mon.evaluate("window.__escritas.length") == 0
    assert mon.locator("#connect").count() == 0 and mon.evaluate("document.getElementById('clues').children.length") == 0
    assert s.pg.evaluate("window.__pedidosPorta") == 1
    assert "MONITOR DA IA ABERTO" in s.pg.text_content("#gvMonitor")
    # Detalhes técnicos recolhidos por padrão, com a conferência dentro.
    assert mon.eval_on_selector("#monTecDet", "e => e.open") is False
    assert "confere" in mon.text_content("#monTec") and "NÃO CONFERE" not in mon.text_content("#monTec")
    assert s.erros == []
    s.fechar()


def test_sincroniza_e_anima_cada_pergunta_nova(navegador):
    s = Sessao(navegador)
    s.conectar()
    s.esp(_pergunta("C6E5747A", 1))
    mon = s.abrir_monitor()
    s.esp(_pergunta("C6E5747A", 2))
    mon.wait_for_function("document.getElementById('monTitulo').textContent.startsWith('Avaliando: Explorador')")
    assert mon.is_visible("#monLenta") and mon.is_hidden("#monDecisao")
    assert "visualização em câmera lenta" in mon.text_content("#monLenta") and "recurso visual" in mon.text_content("#monLenta")
    mon.wait_for_selector(".gvr-pulso", timeout=3000)
    _capturar(mon, "20_monitor_avaliando.png")
    mon.wait_for_function("document.getElementById('monTitulo').textContent==='Última avaliação: Desafiante'", timeout=15000)
    assert mon.is_visible("#monDecisao") and mon.locator(".gvr-pulso").count() == 0 and mon.is_hidden("#monLenta")
    assert "DIFICULDADE ESCOLHIDA PELA POLÍTICA" in mon.text_content(".mon-barra[data-nivel='1']")
    assert "Última avaliação · visualização parada" in mon.text_content("#monEstado")
    _capturar(mon, "21_monitor_ultima_avaliacao.png")
    # Nada acontece entre perguntas: nenhuma animação nova sem dado novo.
    mon.wait_for_timeout(1500)
    assert mon.text_content("#monTitulo").startswith("Última avaliação") and mon.locator(".gvr-pulso").count() == 0
    # Uma resposta (sem pergunta nova) atualiza o placar sem reanimar.
    s.esp([l for l in _linhas() if l.startswith("GUARDIA,resposta") and "C6E5747A" in l and ",ordem=2," in l][0] + "\n")
    mon.wait_for_function("document.getElementById('monPartida').textContent.includes('1 acerto em 2 respondidas')")
    assert mon.text_content("#monTitulo").startswith("Última avaliação")
    # Só o necessário à exposição: sem linhas do log, sem features brutas da USB, mensagens pequenas.
    msgs = [json.loads(m) for m in mon.evaluate("window.__msgs")]
    estados = [m for m in msgs if m.get("tipo") == "estado"]
    assert estados and all(len(json.dumps(m)) < 12000 for m in estados)
    assert not any("GUARDIA," in json.dumps(m) or "AG," in json.dumps(m) for m in msgs)
    assert set(estados[-1]["estado"]) == {"fonte", "demo", "conexao", "agente", "politica", "modelo", "partida", "erros", "pergunta", "conf"}
    assert s.erros == []
    s.fechar()


def test_fechar_e_reabrir_o_monitor_nao_afeta_o_jogo(navegador):
    s = Sessao(navegador)
    s.conectar()
    s.esp(_pergunta("C6E5747A", 1))
    mon = s.abrir_monitor()
    mon.close()
    s.pg.wait_for_function("document.getElementById('gvMonitor').textContent==='ABRIR MONITOR DA IA'", timeout=3000)
    # O jogo segue: protocolo AG responde, a missão começa e o quadro continua.
    s.esp(f"AG,PREP,1A2B3C4D,{SENHA}\nAG,STATE,1A2B3C4D,PREP,0,0\nAG,STATE,1A2B3C4D,PLAYING,90000,0\n")
    s.pg.wait_for_function("window.__escritas.join('').includes('AG,READY,1A2B3C4D')")
    s.pg.wait_for_function("document.getElementById('status').textContent==='MISSAO EM ANDAMENTO'")
    assert s.pg.evaluate("window.__escritas.join('')").count("AG,HELLO") >= 1
    s.esp("AG,STATE,1A2B3C4D,WIN,0,1\nAG,STATE,1A2B3C4D,IDLE,0,1\n" + _pergunta("826D104C", 1) + _pergunta("826D104C", 2))
    # Reabrir: recebe o estado completo atual (a nova partida), parado.
    mon = s.abrir_monitor()
    mon.wait_for_function("document.getElementById('monTitulo').textContent.startsWith('Última avaliação')")
    assert mon.text_content("#monPerg").startswith("PERGUNTA 2 DE 3") and "Dados usados antes da pergunta 2" in s.texto_mon()
    # Recarregar o monitor (F5) também ressincroniza.
    mon.reload()
    mon.wait_for_function("document.getElementById('monTitulo').textContent.startsWith('Última avaliação')")
    assert mon.text_content("#monPerg").startswith("PERGUNTA 2 DE 3")
    assert s.pg.evaluate("window.__pedidosPorta") == 1
    assert s.erros == []
    s.fechar()


def test_agente_secreto_sem_guardia_e_sem_senha_ou_pistas(navegador):
    s = Sessao(navegador)
    s.conectar()
    mon = s.abrir_monitor()
    s.esp(_pergunta("C6E5747A", 1) + _pergunta("C6E5747A", 2))
    mon.wait_for_function("document.getElementById('monTitulo').textContent.startsWith('Avaliando')")
    # Começa o Agente Secreto no meio da animação: para tudo e avisa.
    s.esp(f"AG,PREP,1A2B3C4D,{SENHA}\nAG,STATE,1A2B3C4D,PREP,0,0\nAG,STATE,1A2B3C4D,PLAYING,90000,0\n")
    mon.wait_for_function("document.getElementById('monAviso').textContent.includes('MODO AGENTE SECRETO')")
    assert "Esta modalidade não usa a Guardiã IA" in s.texto_mon()
    assert mon.is_hidden("#monCorpo") and mon.locator(".gvr-pulso").count() == 0
    mon.wait_for_timeout(2500)
    assert mon.locator(".gvr-pulso").count() == 0 and "Avaliando" not in s.texto_mon()
    assert mon.is_hidden("#monFinal")
    _capturar(mon, "22_monitor_agente_secreto.png")
    # Nada da missão chega ao monitor: nem senha, nem pistas, nem sessão do Agente.
    tudo = " ".join(mon.evaluate("window.__msgs"))
    assert SENHA not in tudo and "1A2B3C4D" not in tudo and "×" not in tudo and "÷" not in tudo
    assert SENHA not in mon.inner_text("body")
    # Volta ao Quiz: anima a próxima pergunta.
    s.esp("AG,STATE,1A2B3C4D,LOSE,0,1\nAG,STATE,1A2B3C4D,IDLE,0,1\n")
    mon.wait_for_function("document.getElementById('monAviso').hidden")
    s.esp(_pergunta("C6E5747A", 3))
    mon.wait_for_function("document.getElementById('monTitulo').textContent.startsWith('Avaliando')")
    assert s.erros == []
    s.fechar()


def test_reproducao_de_log_identificada_como_demonstracao(navegador):
    s = Sessao(navegador)
    mon = s.abrir_monitor()
    s.pg.set_input_files("#gvArquivo", str(DEMO_RECUSA))
    mon.wait_for_function("document.getElementById('monDemo').textContent.includes('DADOS DE DEMONSTRAÇÃO')")
    assert mon.is_visible("#monDemo") and "Não vêm do cofre conectado nem de crianças." in mon.text_content("#monDemo")
    assert mon.is_hidden("#monConexao") and mon.is_hidden("#monFonte")
    # Um único aviso de demonstração na tela.
    assert mon.inner_text("#monitorIA").count("DEMONSTRAÇÃO") == 1
    for _ in range(200):
        if "Guardiã recusou a próxima pergunta" in s.pg.text_content("#gvConteudo"):
            break
        s.pg.click("#gvPasso")
    mon.wait_for_function("document.getElementById('monDecisao').textContent.includes('recusou')", timeout=20000)
    mon.wait_for_function("document.getElementById('monAviso').textContent.includes('sem_nivel_compativel')")
    t = s.texto_mon()
    assert "CENARIO FORCADO" in t and "sem_nivel_compativel" in t and "Partida interrompida" in t
    assert "Guardiã recusou a próxima pergunta" in mon.text_content("#monAviso")
    assert mon.text_content("#monTitulo").startswith("Pergunta recusada pela regra gradual")
    assert "Recusada" in mon.text_content("#monDecisao") and mon.text_content("#monBarras").count("Não avaliado") == 3
    assert "A Guardiã recusou em vez de violar a regra." in mon.text_content("#monDecisao")
    assert mon.inner_text("#monitorIA").count("DEMONSTRAÇÃO") == 1
    _capturar(mon, "23_monitor_demo_recusa.png")
    # Voltar ao vivo: o monitor deixa de mostrar a demonstração.
    s.pg.click("#gvSair")
    mon.wait_for_function("document.getElementById('monFonte').textContent==='AO VIVO · USB'")
    assert mon.is_hidden("#monDemo")
    assert s.erros == []
    s.fechar()


def test_perda_da_usb_e_do_painel_principal_nao_mostra_dados_como_atuais(navegador):
    s = Sessao(navegador)
    s.conectar()
    s.esp(_pergunta("C6E5747A", 1))
    mon = s.abrir_monitor()
    mon.wait_for_function("document.getElementById('monTitulo').textContent.startsWith('Última avaliação')")
    s.pg.evaluate("() => window.__fecharUSB()")
    s.pg.wait_for_function("document.getElementById('status').textContent==='CONEXAO INTERROMPIDA'")
    mon.wait_for_function("document.getElementById('monAviso').textContent.includes('COMUNICAÇÃO COM O COFRE PERDIDA')", timeout=4000)
    assert "não é atual" in mon.text_content("#monAviso") and mon.locator("#monCorpo.mon-antigo").count() == 1
    assert "COMUNICAÇÃO COM O COFRE PERDIDA" in mon.text_content("#monConexao")
    _capturar(mon, "24_monitor_usb_perdida.png")
    # Fechar o painel principal: o monitor avisa e não apresenta os dados como atuais.
    s.pg.close()
    mon.wait_for_function("document.getElementById('monAviso').textContent.includes('SEM COMUNICAÇÃO COM O PAINEL PRINCIPAL')", timeout=6000)
    assert "não são atuais" in mon.text_content("#monAviso") and mon.locator("#monCorpo.mon-antigo").count() == 1
    assert [e for e in s.erros if not e.startswith("principal")] == []
    s.fechar()


def test_monitor_aberto_direto_pede_para_usar_o_botao(navegador):
    ctx = navegador.new_context()
    pg = ctx.new_page()
    erros = []
    pg.on("pageerror", lambda e: erros.append(str(e)))
    pg.goto(PAINEL.as_uri() + "?monitor=ia")
    assert "ABRIR MONITOR DA IA" in pg.text_content("#monAviso") and pg.is_hidden("#monCorpo")
    assert erros == []
    ctx.close()


def test_protocolo_ag_igual_ao_r3_com_o_monitor_aberto(navegador):
    """O mesmo roteiro AG do test_painel_guardia, com a janela de exposição aberta."""
    spec = importlib.util.spec_from_file_location("roteiro_painel", RAIZ / "testes" / "test_painel_guardia.py")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    res = []
    for arquivo, com_monitor in ((R3, False), (PAINEL, True)):
        s = Sessao(navegador, arquivo)
        if com_monitor:
            s.abrir_monitor()
        r = mod._roteiro_ag(s.pg)
        if com_monitor:
            s.mon.wait_for_function("document.getElementById('monAviso').textContent.includes('MODO AGENTE SECRETO')")
        assert s.erros == []
        res.append(r)
        s.fechar()
    assert res[0] == res[1]


def test_ponte_e_monitor_so_leem():
    """A ponte só lê o estado do painel; o monitor e o código comum não tocam em USB, áudio ou protocolo."""
    # código sem os comentários de linha (que explicam justamente o que não é feito)
    ler = lambda n: re.sub(r"^\s*//.*$", "", (RAIZ / "ferramentas" / n).read_text(encoding="utf-8"), flags=re.M)
    ponte = ler("ponte_monitor.js")
    assert not re.search(r"\bsend\(|\bstage\s*=|\bsession\s*=|\bready\s*=|\bconnected\s*=|guardiaLinhas|tone\(|music\.|navigator\.serial|\bport\b", ponte)
    for n in ("monitor_guardia.js", "guardia_comum.js"):
        assert not re.search(r"navigator\.serial|\bsend\(|\bAudio\b|\btone\(|\bmusic\b|\bsenha\b|\bclues\b", ler(n)), n
    html = PAINEL.read_text(encoding="utf-8")
    assert html.count("if(window.CoflumeGuardia&&window.CoflumeGuardia.modoMonitor)return;") == 1


def test_dados_antes_da_pergunta_sem_invencao(navegador):
    """Totais só quando média × respondidas dá inteiro; média com esse nome; segundos só com o tempo limite registrado."""
    s = Sessao(navegador)
    s.conectar()
    mon = s.abrir_monitor()
    mon.uncheck("#monAnimar") if mon.is_visible("#monAnimar") else mon.eval_on_selector("#monAnimar", "e => { e.checked = false; }")
    s.esp("GUARDIA,sessao,sessao=0000D0D0,modelo=mlp8-sint-20261101-e6a4cf00,nivel_inicial=1,politica=fluxo-0.8\n"
          "GUARDIA,decisao,modelo=mlp8-sint-20261101-e6a4cf00,sessao=0000D0D0,ordem=3,nivel_inicial=1,categoria=0,"
          "niveis_permitidos=0|1|2,niveis_disponiveis=0|1|2,p0=0.841234,p1=0.712345,p2=0.501234,alvo=0.70,nivel_escolhido=1,"
          "p_escolhido=0.712345,motivo=guardia\n"
          "GUARDIA,features,sessao=0000D0D0,ordem=3,nivel_candidato=1,nivel_inicial=1,n_respondidas=3,categoria=0,"
          "taxa_acerto_sessao=0.333333,taxa_acerto_categoria=0.000000,tempo_medio_norm=0.400000,dicas_por_questao=0.500000,"
          "resultado_ultima=-1,sequencia=-1\n")
    mon.wait_for_function("document.getElementById('monDados').textContent.includes('acertos anteriores')")
    d = " ".join(mon.inner_text("#monDados").split())
    assert "1 de 3 acertos anteriores" in d            # 0,333333 × 3 = 1 (inteiro)
    assert "0,50 dicas por pergunta (média)" in d       # 0,5 × 3 = 1,5: não é total
    assert "dicas usadas" not in d
    assert "40% do limite tempo médio por resposta" in d  # sem tempo limite nas respostas: não vira segundos
    # Valores técnicos (6 casas) só nos detalhes recolhidos.
    assert not re.search(r"\d[.,]\d{6}", mon.inner_text("#monCorpo") + mon.inner_text("#monFinal"))
    assert mon.eval_on_selector("#monTecDet", "e => e.open") is False
    assert "taxa_acerto_sessao=0.333333" in mon.text_content("#monTec")
    # Sem a linha features: indisponível.
    s.esp("GUARDIA,sessao,sessao=0000D0D1,nivel_inicial=1,politica=fluxo-0.8\n"
          "GUARDIA,decisao,modelo=mlp8-sint-20261101-e6a4cf00,sessao=0000D0D1,ordem=2,nivel_inicial=1,categoria=0,"
          "niveis_disponiveis=0|1|2,p0=0.8,p1=0.7,p2=0.5,alvo=0.70,nivel_escolhido=1,p_escolhido=0.7,motivo=guardia\n")
    mon.wait_for_function("document.getElementById('monDados').textContent.includes('indisponível')")
    assert "dados da pergunta ausentes no log" in mon.text_content("#monDados")
    assert mon.text_content("#monTitulo") == "Previsão da rede indisponível nesta pergunta"
    assert s.erros == []
    s.fechar()


def test_explicacao_da_protecao_vem_dos_logs(navegador):
    """'após um erro' ou 'após um tempo esgotado' conforme a resposta anterior registrada."""
    s = Sessao(navegador)
    s.conectar()
    mon = s.abrir_monitor()
    base = ("GUARDIA,sessao,sessao=0000E5E5,modelo=mlp8-sint-20261101-e6a4cf00,nivel_inicial=2,politica=gradual-v2-exp\n"
            "GUARDIA,resposta,sessao=0000E5E5,ordem=2,acertou=0,usou_dica=0,tempo_gasto_s=30.0,tempo_limite_s=30,esgotou=1,historico_n=2\n"
            "GUARDIA,politica_gradual,politica=gradual-v2-exp,sessao=0000E5E5,ordem=3,anterior=1,anterior_acertou=0,"
            "niveis_compativeis=1,candidatos=1,categoria_fluxo08=2,categoria=2,nivel_sem_regra=2,nivel=1,limitou=1,motivo=limitado\n"
            "GUARDIA,decisao,modelo=mlp8-sint-20261101-e6a4cf00,sessao=0000E5E5,ordem=3,nivel_inicial=2,categoria=2,"
            "niveis_permitidos=1|2,niveis_disponiveis=1|2,p0=NA,p1=0.801162,p2=0.600218,alvo=0.70,nivel_escolhido=1,"
            "p_escolhido=0.801162,motivo=guardia\n")
    s.esp(base)
    mon.wait_for_function("document.getElementById('monDecisao').textContent.includes('proteção gradual')")
    dec = " ".join(mon.inner_text("#monDecisao").split())
    assert "A proteção gradual impediu aumentar a dificuldade após um tempo esgotado." in dec
    assert "Mestre estava mais perto do alvo de 70%, mas fora da regra." in dec
    s.esp(base.replace("0000E5E5", "0000E5E6").replace("tempo_gasto_s=30.0", "tempo_gasto_s=8.0").replace("esgotou=1", "esgotou=0"))
    mon.wait_for_function("document.getElementById('monDecisao').textContent.includes('após um erro')")
    s.fechar()


def test_rede_do_monitor_usa_os_pesos_reais_com_conexoes_discretas(navegador):
    s = Sessao(navegador)
    mon = s.abrir_monitor()
    assert mon.locator("#monA1 line").count() == 104 and mon.locator("#monA2 line").count() == 8
    for i in (0, 37, 103):
        w_mon = float(mon.eval_on_selector_all("#monA1 line", f"l => l[{i}].getAttribute('stroke-width')"))
        w_pri = float(s.pg.eval_on_selector_all("#gvrA1 line", f"l => l[{i}].getAttribute('stroke-width')"))
        cor_mon = mon.eval_on_selector_all("#monA1 line", f"l => l[{i}].getAttribute('stroke')")
        cor_pri = s.pg.eval_on_selector_all("#gvrA1 line", f"l => l[{i}].getAttribute('stroke')")
        assert abs(w_mon - 0.7 * w_pri) < 0.02 and cor_mon == cor_pri  # mesmo peso (sinal e |w|), traço mais fino
    s.fechar()


DEMO_GRADUAL = RAIZ / "demo" / "demo_gradual.txt"


def _demo_ate_o_fim(s):
    s.pg.set_input_files("#gvArquivo", str(DEMO_GRADUAL))
    for _ in range(300):
        if "Desbloqueio: NÃO" in s.pg.text_content("#gvConteudo"):
            break
        s.pg.click("#gvPasso")
    s.mon.wait_for_function("document.getElementById('monTitulo').textContent.startsWith('Última avaliação')", timeout=40000)
    s.mon.wait_for_timeout(600)


@pytest.mark.parametrize("largura,altura", [(1920, 1080), (1366, 768)])
def test_rede_grande_sem_rolagem_e_resultado_visivel(navegador, largura, altura):
    s = Sessao(navegador)
    mon = s.abrir_monitor()
    mon.set_viewport_size({"width": largura, "height": altura})
    _demo_ate_o_fim(s)
    m = mon.evaluate("""() => {
      const r = id => document.getElementById(id).getBoundingClientRect();
      const nos = [...document.querySelectorAll('#monSvg .gvr-no')].map(c => c.getBoundingClientRect());
      return {sh: document.documentElement.scrollHeight, sw: document.documentElement.scrollWidth,
        corpo: r('monCorpo').width, svgW: r('monSvg').width, svgH: r('monSvg').height, finalBase: r('monFinal').bottom,
        partida: r('monPartida').bottom, lado: r('monBarras').width,
        larg: Math.max(...nos.map(b => b.right)) - Math.min(...nos.map(b => b.left)),
        alt: Math.max(...nos.map(b => b.bottom)) - Math.min(...nos.map(b => b.top)),
        rOculto: nos[13].width / 2, rSaida: nos[21].width / 2, n: nos.length}; }""")
    assert m["sh"] <= altura and m["sw"] <= largura  # sem rolagem
    assert m["finalBase"] <= altura and m["partida"] <= altura  # resultado da partida visível
    assert m["svgW"] >= 0.6 * m["corpo"] and m["lado"] <= 0.36 * m["corpo"]  # rede ≈ 2/3, lateral estreita
    assert m["alt"] >= 0.5 * altura and m["larg"] >= 0.45 * largura  # o desenho em si é grande
    # círculos maiores que os do painel principal (não só o contêiner)
    assert m["n"] == 22 and m["rOculto"] >= 20 and m["rSaida"] >= 45
    assert "Cofre fechado" in mon.inner_text("#monCofre")
    _capturar(mon, f"27_monitor_rede_grande_{largura}x{altura}.png")
    assert not s.erros, s.erros
    s.fechar()


def test_rede_grande_sem_rotulos_no_desenho_e_entradas_nos_detalhes(navegador):
    s = Sessao(navegador)
    mon = s.abrir_monitor()
    _demo_ate_o_fim(s)
    assert mon.locator("#monSvg .gvr-rot").count() == 0
    assert mon.locator("#monA1 line").count() == 104 and mon.locator("#monA2 line").count() == 8
    assert mon.locator("#monSvg circle.gvr-no").count() == 22  # 13 + 8 + 1 nós
    cab = mon.eval_on_selector_all("#monSvg .gvr-cab", "l => l.map(e => e.textContent)")
    assert cab[:3] == ["Dados da partida", "8 neurônios", "Chance de acerto"]
    linhas = mon.eval_on_selector_all("#monTec table.mon-entradas tr", "l => l.map(e => e.textContent)")
    assert len(linhas) == 14  # cabeçalho + 13 entradas
    for nome in ("dificuldade avaliada", "tempo médio", "dicas por pergunta", "Conh. gerais"):
        assert any(nome in l for l in linhas)
    s.fechar()


def test_brilho_proporcional_a_ativacao_reproduzida(navegador):
    s = Sessao(navegador)
    mon = s.abrir_monitor()
    _demo_ate_o_fim(s)
    nos = mon.evaluate("""() => [...document.querySelectorAll('#monSvg circle.gvr-no')].slice(13, 21).map(c => ({
      t: c.querySelector('title').textContent, f: c.style.filter}))""")
    vals = []
    for n in nos:
        a = float(re.search(r"reproduzida (-?[0-9.]+)", n["t"]).group(1))
        if a == 0:
            assert n["f"] == "none"  # neurônio inativo: sem halo (nada inventado)
        else:
            raio = max(float(x) for x in re.findall(r"([0-9.]+)px", n["f"]))
            vals.append((a, raio))
    assert vals
    vals.sort()
    assert all(r1 <= r2 + 1e-9 for (_, r1), (_, r2) in zip(vals, vals[1:]))  # mais ativação, mais brilho
    # a ativação do monitor é a mesma reproduzida no painel principal (mesmo cálculo)
    s.fechar()
