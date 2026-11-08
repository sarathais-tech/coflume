// Testes da v0.9 no PC: o coflume_v09.ino real (base que funciona na placa),
// compilado com imitações de Arduino, teclado, ST7735, LEDC e da porta serial
// (testes/stub/). Verifica o quiz com a Guardiã, o servo real (duty do LEDC),
// os LEDs simulados, o Agente Secreto (protocolo COFLUME_AGENT/1) e o layout de
// TODAS as telas em 160x128.
// Uso: teste_v09 [arquivo.jsonl] [servo_falha]
#include "../coflume_v09/coflume_v09.ino"

#include <fstream>
#include <set>
#include <vector>

static int falhas = 0, verificacoes = 0;
#define CONFERIR(cond, msg)                                                  \
  do {                                                                       \
    ++verificacoes;                                                          \
    if (!(cond)) {                                                           \
      ++falhas;                                                              \
      std::printf("FALHA %s:%d: %s [%s]\n", __FILE__, __LINE__, msg, #cond); \
    }                                                                        \
  } while (0)

static std::ofstream capturas;
static std::set<std::string> nomes_capturados;

static std::string json(const std::string& s) {
  std::string o = "\"";
  for (unsigned char c : s) {
    if (c == '"' || c == '\\') { o += '\\'; o += (char)c; }
    else if (c < 0x20 || c >= 0x80) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", c); o += b; }
    else o += (char)c;
  }
  return o + "\"";
}
// Grava a tela atual (operações desde o último fillScreen) para gerar a imagem.
static void capturar(const std::string& nome) {
  if (!capturas.is_open() || nomes_capturados.count(nome)) return;
  nomes_capturados.insert(nome);
  capturas << "{\"nome\":" << json(nome) << ",\"ops\":[";
  bool primeiro = true;
  for (const auto& o : stub::ops()) {
    capturas << (primeiro ? "" : ",") << "[\"" << o.tipo << "\"," << o.x << "," << o.y << "," << o.w << "," << o.h
             << "," << o.tamanho << "," << o.cor << "," << json(o.texto) << "]";
    primeiro = false;
  }
  capturas << "]}\n";
}
static void sem_violacoes(const std::string& onde) {
  for (const auto& v : stub::violacoes()) std::printf("FALHA layout (%s): %s\n", onde.c_str(), v.c_str());
  CONFERIR(stub::violacoes().empty(), "layout sem cortes nem sobreposição");
  stub::violacoes().clear();
}

static const uint32_t PASSO_MS = 10;
static void avancar(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += PASSO_MS) { stub::agora_ms() += PASSO_MS; loop(); }
}
static void apertar(char k) { stub::teclas().push_back(k); loop(); }
static int contar(const std::string& s, const std::string& p) {
  int n = 0;
  for (size_t i = s.find(p); i != std::string::npos; i = s.find(p, i + 1)) ++n;
  return n;
}
static bool tela_tem(const std::string& s) {
  for (const auto& o : stub::ops()) if (o.tipo == 'T' && o.texto.find(s) != std::string::npos) return true;
  return false;
}

enum Resp { CERTA, ERRADA, ESGOTA };
struct Apresentada { coflume::Decisao d; int pergunta; guardia::Historico antes; };

static Apresentada responder(Resp r, bool dica) {
  Apresentada a{decisao, perguntaAtual, historico};
  CONFERIR(estado == PERGUNTA, "em uma pergunta");
  if (r == ESGOTA) {
    avancar(TEMPO_QUESTAO_MS + 20);
  } else {
    avancar(2000);
    if (dica) apertar('*');
    const char c = questao().correta;
    apertar(r == CERTA ? c : (c == 'A' ? 'B' : 'A'));
    apertar('#');
  }
  CONFERIR(estado == RESULTADO, "mostra o resultado");
  avancar(FEEDBACK_ERRO_MS + 50);
  return a;
}
static std::vector<Apresentada> jogar(int ini, const Resp r[3], const bool d[3]) {
  CONFERIR(estado == MENU, "no menu");
  apertar((char)('1' + ini));
  std::vector<Apresentada> p;
  for (int k = 0; k < 3; ++k) p.push_back(responder(r[k], d[k]));
  return p;
}
static void verificar_partida(const std::vector<Apresentada>& p, int ini, bool adaptativa) {
  CONFERIR(p.size() == 3, "3 perguntas");
  std::set<int> cats, pergs;
  for (size_t k = 0; k < p.size(); ++k) {
    const coflume::Decisao& d = p[k].d;
    cats.insert(d.categoria);
    pergs.insert(p[k].pergunta);
    CONFERIR(coflume::BANCO[p[k].pergunta].nivel == d.nivel, "pergunta do nível decidido");
    CONFERIR(p[k].antes.n == (int)k, "histórico da mesma partida");
    CONFERIR(abs((int)d.nivel - ini) <= (k == 0 ? 0 : 1), "nível na faixa");
    if (k == 0) { CONFERIR(d.motivo == coflume::MOTIVO_PERGUNTA_1, "pergunta 1"); continue; }
    if (!adaptativa) { CONFERIR(d.motivo == coflume::MOTIVO_SEM_MODELO && d.nivel == ini, "nível fixo"); continue; }
    CONFERIR(d.motivo == coflume::MOTIVO_GUARDIA, "decisão da Guardiã");
    double melhor = 1e9;
    for (int i = 0; i < d.n_disponiveis; ++i) {
      const uint8_t n = d.disponiveis[i];
      const float pr = guardia::prever(guardia::montar_entrada(p[k].antes, ini, d.categoria, n));
      CONFERIR(d.previsoes[n] == pr, "previsão = MLP recalculada");
      melhor = std::min(melhor, std::fabs((double)pr - 0.70));
    }
    CONFERIR(std::fabs(std::fabs((double)d.previsoes[d.nivel] - 0.70) - melhor) < 1e-6, "mais próximo de 0,70");
  }
  CONFERIR(cats.size() == 3 && pergs.size() == 3, "categorias e perguntas distintas");
}

// Duty do LEDC esperado para um ângulo (mesma conta do firmware, 14 bits).
static uint32_t duty(int graus) { return (uint32_t)((map(graus, 0, 180, 500, 2400) * 16383UL) / 20000UL); }
static bool servo_em(int graus) { return stub::pwm()[SERVO_PIN] == duty(graus); }
static std::string ultimo_servo() {  // "aberto", "fechado" ou "?"
  return servo_em(ANGULO_ABERTO) ? "aberto" : servo_em(ANGULO_FECHADO) ? "fechado" : "?";
}
// Painel USB simulado: envia uma linha para o ESP32 e roda o loop.
static void painel(const std::string& linha) {
  for (char c : linha) stub::entrada().push_back(c);
  stub::entrada().push_back('\n');
  loop();
}
static std::string sessao_agente() {
  char b[12];
  std::snprintf(b, sizeof(b), "%08lX", (unsigned long)agente::id);
  return b;
}
static size_t ultimo(const std::string& s) { return stub::serial().rfind(s); }

// Execução separada: ledcAttach falha (servo indisponível).
static int teste_servo_com_falha() {
  stub::ledc_ok() = false;
  stub::passo_por_desenho_ms() = 1;
  setup();
  const std::string& log = stub::serial();
  CONFERIR(!servoPronto && log.find("SERVO,inicializacao=FALHA") != std::string::npos, "falha do servo no log");
  CONFERIR(log.find("servo=FALHA") != std::string::npos, "estado com servo=FALHA");
  CONFERIR(tela_tem("A Agente: SERVO C/ FALHA"), "aviso de falha do servo no menu");
  capturar("menu_servo_com_falha");
  telaBoot();
  CONFERIR(tela_tem("SERVO COM FALHA (GPIO 12)"), "aviso de falha do servo na tela de boot");
  capturar("boot_servo_com_falha");
  sem_violacoes("telas de falha do servo");
  menu();
  apertar('A');
  CONFERIR(estado == MENU && log.find("AG,ERROR,SERVO") != std::string::npos, "Agente recusado sem servo");
  CONFERIR(tela_tem("A Agente: SERVO C/ FALHA"), "aviso continua na tela");
  CONFERIR(stub::pwm().empty(), "nenhum duty escrito sem servo");
  apertar('2');
  CONFERIR(estado == PERGUNTA, "quiz continua funcionando sem servo");
  std::printf("teste_v09 servo_falha: %d verificações, %d falhas\n", verificacoes, falhas);
  return falhas == 0 ? 0 : 1;
}

int main(int argc, char** argv) {
  if (argc > 1 && std::string(argv[1]) != "-") capturas.open(argv[1]);
  if (argc > 2 && std::string(argv[2]) == "servo_falha") return teste_servo_com_falha();
  stub::passo_por_desenho_ms() = 1;
  const bool adaptativa = GUARDIA_ADAPTATIVA == 1;

  // --- Pinagem e configuração --------------------------------------------------
  CONFERIR(TFT_CS == 14 && TFT_RST == 16 && TFT_DC == 15 && TFT_MOSI == 17 && TFT_SCLK == 18, "pinos da ST7735");
  CONFERIR(linhas[0] == 8 && linhas[1] == 9 && linhas[2] == 10 && linhas[3] == 11, "linhas do teclado 8-11");
  CONFERIR(colunas[0] == 4 && colunas[1] == 5 && colunas[2] == 6 && colunas[3] == 7, "colunas do teclado 4-7");
  {
    std::set<int> pinos = {TFT_CS, TFT_RST, TFT_DC, TFT_MOSI, TFT_SCLK};
    for (int i = 0; i < 4; ++i) { pinos.insert(linhas[i]); pinos.insert(colunas[i]); }
    CONFERIR(pinos.size() == 13, "nenhum pino repetido entre tela e teclado");
  }

  // --- Inicialização ------------------------------------------------------------
  setup();
  const std::string& log = stub::serial();
  capturar("menu");
  CONFERIR(stub::painel() == INITR_BLACKTAB, "painel BLACKTAB por padrão");
  CONFERIR(log.find("tela=ST7735 160x128") != std::string::npos, "paisagem 160x128");
  CONFERIR(log.find("GUARDIA,autoteste,vetores=21,") != std::string::npos && log.find("resultado=OK") != std::string::npos,
           "autoteste aprovado");
  CONFERIR(log.find("GUARDIA,latencia_informativa,chamadas=1000,") != std::string::npos, "latência informativa no log");
  CONFERIR(log.find("id=mlp8-sint-20261101-e6a4cf00") != std::string::npos, "modelo no log");
  CONFERIR(log.find("servo=REAL_GPIO12,leds=SIMULADOS") != std::string::npos, "servo real e LEDs simulados declarados");
  CONFERIR(servoPronto && stub::ledc_pino() == 12 && log.find("SERVO,inicializacao=OK") != std::string::npos,
           "servo real inicializado no GPIO 12");
  CONFERIR(ultimo_servo() == "fechado", "servo começa fechado (90 graus)");
  CONFERIR(guardiaAtiva == adaptativa && bancoOK, "Guardiã e banco");
  CONFERIR(estado == MENU && tela_tem("1 Explorador") && tela_tem("3 Mestre") && tela_tem("A Agente Secreto (USB)") &&
               tela_tem("Escolha 1-3 ou A"), "menu com as duas modalidades");
  sem_violacoes("boot e menu");

  // --- Tela de boot (captura) ----------------------------------------------------
  telaBoot();
  capturar("boot");
  sem_violacoes("boot");
  menu();

  // --- Cronômetro, timeout, dica, feedback --------------------------------------
  const uint32_t antes = millis();
  apertar('2');
  capturar("pergunta_inicio");
  CONFERIR(inicioPergunta - antes >= 10 && millis() - inicioPergunta <= 4, "cronômetro começa após desenhar");
  CONFERIR(tela_tem("DESAFIANTE 1/3") && tela_tem("30s"), "cabeçalho com nível, progresso e tempo");
  // Cada redesenho do tempo também consome tempo simulado (1 ms por chamada à tela).
  stub::passo_por_desenho_ms() = 0;
  avancar(TEMPO_QUESTAO_MS - 100);
  CONFERIR(estado == PERGUNTA, "dentro dos 30 s");
  avancar(200);
  stub::passo_por_desenho_ms() = 1;
  CONFERIR(estado == RESULTADO && tela_tem("Tempo esgotado!"), "timeout");
  capturar("feedback_tempo_esgotado");
  CONFERIR(historico.n == 1 && !historico.itens[0].acertou && historico.itens[0].tempo_gasto_s == 30.0f, "timeout no histórico");
  CONFERIR(ledVermelho, "LED vermelho simulado no erro");
  avancar(FEEDBACK_ERRO_MS + 50);
  // Dica e seleção
  avancar(1000);
  apertar('*');
  CONFERIR(tela_tem(std::string("Dica: ").substr(0, 5)), "dica na tela");
  apertar('B');
  apertar('C');
  CONFERIR(selecionada == 'C', "seleção muda");
  capturar("pergunta_dica_e_selecao");
  apertar(questao().correta == 'A' ? 'B' : 'A');
  apertar('#');
  CONFERIR(tela_tem("VAMOS") && tela_tem("APRENDER!"), "feedback de erro");
  capturar("feedback_erro");
  CONFERIR(historico.itens[1].usou_dica && !historico.itens[1].acertou, "dica e erro registrados");
  avancar(FEEDBACK_ERRO_MS + 50);
  avancar(500);
  apertar('#');
  CONFERIR(estado == PERGUNTA, "# sem alternativa não confirma");
  apertar(questao().correta);
  apertar('#');
  capturar("feedback_acerto");
  avancar(FEEDBACK_ERRO_MS + 50);
  CONFERIR(estado == DERROTA && acertos == 1, "1 acerto: não abre");
  capturar("derrota");
  CONFERIR(ultimo_servo() == "fechado", "servo fechado com 1 acerto");
  sem_violacoes("partida 1");
  apertar('#');

  // --- Partidas: desbloqueio, servo e LEDs simulados, Guardiã ---------------------
  const Resp tres[3] = {CERTA, CERTA, CERTA}, dois[3] = {CERTA, ERRADA, CERTA}, um[3] = {ERRADA, ESGOTA, CERTA},
             zero[3] = {ESGOTA, ESGOTA, ESGOTA};
  const bool sem[3] = {false, false, false}, com[3] = {true, true, true};
  struct Caso { const Resp* r; const bool* d; int ini; bool abre; };
  const Caso casos[] = {{tres, sem, 1, true}, {dois, com, 0, true}, {um, sem, 2, false}, {zero, sem, 1, false},
                        {tres, com, 2, true}};
  for (const Caso& c : casos) {
    const uint32_t sessao = sessaoId;
    const size_t fim_antes = contar(log, "GUARDIA,fim,");
    const std::vector<Apresentada> p = jogar(c.ini, c.r, c.d);
    verificar_partida(p, c.ini, adaptativa);
    CONFERIR(sessaoId != sessao && contar(log, "GUARDIA,fim,") == (int)fim_antes + 1, "nova sessão");
    CONFERIR(estado == (c.abre ? VITORIA : DERROTA), "resultado");
    CONFERIR(ultimo_servo() == (c.abre ? "aberto" : "fechado"), "servo real conforme a regra de 2 acertos");
    CONFERIR(ledVerde == c.abre, "LED verde simulado");
    if (c.abre) capturar("vitoria");
    sem_violacoes("partidas");
    apertar('#');
    CONFERIR(ultimo_servo() == "fechado", "menu fecha o cofre");
  }
  int mudou = 0, escolhas = 0;
  for (int i = 0; i < 45; ++i) {
    const int ini = i % 3;
    const Resp r[3] = {i % 2 ? CERTA : ERRADA, CERTA, i % 4 < 2 ? CERTA : ERRADA};
    const std::vector<Apresentada> p = jogar(ini, r, sem);
    CONFERIR(p[0].antes.n == 0, "partida começa sem histórico");
    verificar_partida(p, ini, adaptativa);
    for (const auto& a : p) if (a.d.motivo == coflume::MOTIVO_GUARDIA) { ++escolhas; if (a.d.nivel != ini) ++mudou; }
    sem_violacoes("partidas");
    apertar('#');
  }
  if (adaptativa) CONFERIR(escolhas == 90 && mudou > 0, "Guardiã escolheu níveis diferentes do inicial");

  // --- Agente Secreto (protocolo COFLUME_AGENT/1) ------------------------------
  {
    CONFERIR(estado == MENU, "menu antes do Agente");
    // Conexão do painel: HELLO responde e reenvia o diagnóstico de boot (logs GUARDIA).
    const size_t antes_hello = log.size();
    painel("AG,HELLO");
    const std::string resposta = log.substr(antes_hello);
    CONFERIR(resposta.find("AG,HELLO,1,1\n") == 0, "HELLO responde primeiro, protocolo 1, servo pronto");
    CONFERIR(resposta.find("AG,STATE,00000000,IDLE,0,0") != std::string::npos, "estado IDLE publicado");
    CONFERIR(resposta.find("GUARDIA,reenvio_diagnostico_boot") != std::string::npos &&
                 resposta.find("GUARDIA,autoteste,vetores=21,") != std::string::npos &&
                 resposta.find("GUARDIA,modelo,id=mlp8-sint-20261101-e6a4cf00") != std::string::npos &&
                 resposta.find("GUARDIA,latencia_informativa") != std::string::npos,
             "diagnóstico da Guardiã reenviado ao conectar o painel");
    CONFERIR(estado == MENU, "HELLO não muda a tela");
    // A sem painel respondendo: o preparo expira em 5 s, com aviso de painel ausente.
    apertar('A');
    CONFERIR(estado == AGENTE && agente::fase == agente::PREPARANDO && ultimo("AG,PREP,") != std::string::npos,
             "A no menu prepara a missão");
    CONFERIR(tela_tem("AGUARDANDO") && tela_tem("COMPUTADOR"), "tela aguardando o painel");
    capturar("agente_aguardando_painel");
    const uint32_t t_prep = millis();
    while (agente::fase == agente::PREPARANDO && millis() - t_prep < 6000) { stub::agora_ms() += 10; loop(); }
    CONFERIR(agente::fase == agente::PERDEU && millis() - t_prep >= agente::CONEXAO_MS, "preparo expira em 5 s");
    CONFERIR(ultimo(",PANEL_TIMEOUT") != std::string::npos && tela_tem("PAINEL USB") && tela_tem("AUSENTE"),
             "aviso de painel ausente");
    CONFERIR(ultimo_servo() == "fechado", "cofre fechado sem painel");
    capturar("agente_painel_ausente");
    apertar('#');
    CONFERIR(estado == MENU, "# volta ao menu");
    // Missão completa: READY, senha errada, * apaga, senha certa abre o servo.
    apertar('A');
    const std::string s1 = sessao_agente();
    painel("AG,READY,00000001");
    CONFERIR(agente::fase == agente::PREPARANDO, "READY de outra sessão é ignorado");
    painel("AG,READY," + s1);
    CONFERIR(agente::fase == agente::JOGANDO && agente::restante() > agente::LIMITE_MS - 100, "READY inicia 90 s");
    CONFERIR(guardia::prever != nullptr && contar(log.substr(ultimo("AG,PREP,")), "GUARDIA,decisao") == 0,
             "nenhuma decisão da Guardiã no Agente Secreto");
    char errada[5];
    for (int i = 0; i < 4; ++i) errada[i] = agente::senha[i] == '9' ? '0' : (char)(agente::senha[i] + 1);
    for (int i = 0; i < 2; ++i) apertar(errada[i]);
    CONFERIR(agente::n == 2 && tela_tem("**__"), "máscara com 2 dígitos");
    capturar("agente_jogando");
    apertar('#');
    CONFERIR(ultimo(",INCOMPLETE") != std::string::npos && agente::tentativas == 0, "# com menos de 4 dígitos");
    for (int i = 2; i < 4; ++i) apertar(errada[i]);
    apertar('#');
    CONFERIR(ultimo(",WRONG") != std::string::npos && agente::tentativas == 1 && agente::fase == agente::JOGANDO &&
                 ultimo_servo() == "fechado", "senha errada não abre e permite nova tentativa");
    for (int i = 0; i < 10; ++i) { avancar(900); painel("AG,PING," + s1); }
    CONFERIR(agente::fase == agente::JOGANDO, "PING mantém a missão");
    sem_violacoes("agente relógio");
    apertar('5');
    apertar('*');
    CONFERIR(agente::n == 0, "* apaga");
    const std::string senha1 = agente::senha;
    for (int i = 0; i < 4; ++i) apertar(agente::senha[i]);
    apertar('#');
    CONFERIR(agente::fase == agente::GANHOU && ultimo_servo() == "aberto" && ultimo(",CORRECT") != std::string::npos,
             "senha correta abre o servo");
    CONFERIR(tela_tem("MISSAO") && tela_tem("CUMPRIDA!"), "tela de missão cumprida");
    capturar("agente_missao_cumprida");
    apertar('#');
    CONFERIR(estado == MENU && ultimo_servo() == "fechado", "# fecha o servo e volta ao menu");
    // Painel tentando iniciar no meio do quiz.
    apertar('1');
    CONFERIR(estado == PERGUNTA, "quiz começa");
    painel("AG,START");
    CONFERIR(estado == PERGUNTA && ultimo("AG,ERROR,BUSY") != std::string::npos, "painel recusado durante o quiz");
    for (int k = 0; k < 3; ++k) responder(CERTA, false);
    CONFERIR(estado == VITORIA && ultimo_servo() == "aberto", "quiz depois do Agente abre com 3/3");
    apertar('#');
    // Início pelo painel (START) com o ESP32 no menu e 90 s esgotados.
    painel("AG,START");
    CONFERIR(estado == AGENTE && agente::fase == agente::PREPARANDO, "START do painel com o ESP32 no menu");
    const std::string s2 = sessao_agente();
    CONFERIR(s2 != s1 && std::string(agente::senha) != senha1, "nova sessão e senha diferente da anterior");
    painel("AG,READY," + s2);
    for (int i = 0; i < 95 && agente::fase == agente::JOGANDO; ++i) { avancar(1000); painel("AG,PING," + s2); }
    CONFERIR(agente::fase == agente::PERDEU && ultimo(",TIMEOUT") != std::string::npos, "90 s esgotados");
    CONFERIR(tela_tem("MISSAO") && tela_tem("ENCERRADA"), "tela de missão encerrada");
    capturar("agente_tempo_esgotado");
    apertar('#');
    // Painel para de responder durante a missão.
    apertar('A');
    painel("AG,READY," + sessao_agente());
    avancar(5200);
    CONFERIR(agente::fase == agente::PERDEU && ultimo(",DISCONNECTED") != std::string::npos && tela_tem("CONEXAO") &&
                 tela_tem("PERDIDA"), "aviso de conexão perdida");
    capturar("agente_conexao_perdida");
    apertar('#');
    // D cancela a missão; CANCEL do painel também.
    apertar('A');
    painel("AG,READY," + sessao_agente());
    apertar('D');
    CONFERIR(estado == MENU && ultimo(",CANCELLED") != std::string::npos, "D cancela e volta ao menu");
    apertar('A');
    painel("AG,READY," + sessao_agente());
    painel("AG,CANCEL," + sessao_agente());
    CONFERIR(agente::fase == agente::PERDEU && ultimo_servo() == "fechado", "CANCEL do painel encerra");
    apertar('#');
    // Linha longa e ruído não travam o ESP32.
    painel(std::string(300, 'x'));
    painel("AG,HELLO");
    CONFERIR(ultimo("AG,HELLO,1,1") != std::string::npos && estado == MENU, "ruído na serial é descartado");
    sem_violacoes("Agente Secreto");
  }

  // --- Layout de TODAS as perguntas: tela, dica, seleção e feedbacks ------------
  apertar('2');
  for (int i = 0; i < COFLUME_BANCO_N; ++i) {
    const std::string id = coflume::BANCO[i].id;
    perguntaAtual = i;
    nivel = coflume::BANCO[i].nivel;
    indice = i % 3;
    mostrarQuestao();
    CONFERIR(log.find("TELA,aviso,pergunta=" + id) == std::string::npos, "corpo não invade o rodapé");
    CONFERIR(acertos <= indice + 1, "placar coerente");
    for (int a = 0; a < 4; ++a)
      CONFERIR(yAlternativa[a] + linhasAlternativa[a] * 16 <= tela::Y_RODAPE, "alternativa acima do rodapé");
    capturar("pergunta_" + id);
    apertar('*');
    CONFERIR(tela::quebrar((std::string("Dica: ") + coflume::BANCO[i].dica).c_str(), 26, 0).n <= tela::LINHAS_RODAPE,
             "dica em até 2 linhas");
    for (char k : {'A', 'B', 'C', 'D', 'A'}) apertar(k);
    capturar("pergunta_" + id + "_dica_selecao_A");
    sem_violacoes("pergunta " + id);
    acertos = 0;
    resultado(true, false);
    capturar("feedback_acerto_" + id);
    sem_violacoes("feedback acerto " + id);
    mostrarQuestao();
    acertos = 0;
    resultado(false, false);
    capturar("feedback_erro_" + id);
    sem_violacoes("feedback erro " + id);
    mostrarQuestao();
    acertos = 0;
    resultado(false, true);
    capturar("feedback_tempo_" + id);
    sem_violacoes("feedback tempo " + id);
  }
  erroBanco("banco_inviavel");  // r3: erroBanco recebe o motivo
  capturar("erro_banco");
  sem_violacoes("erro banco");

  // --- Quebra por palavra ------------------------------------------------------------
  const tela::Linhas q1 = tela::quebrar("Qual palavra esta correta?", 13, 0);
  CONFERIR(q1.n == 2 && std::string(q1.texto[0]) == "Qual palavra" && std::string(q1.texto[1]) == "esta correta?",
           "quebra por palavra");
  const tela::Linhas q2 = tela::quebrar("B) Golpe digital", 13, 3);
  CONFERIR(q2.n == 2 && std::string(q2.texto[0]) == "B) Golpe" && std::string(q2.texto[1]) == "   digital",
           "continuação alinhada após 'B) '");
  const tela::Linhas q3 = tela::quebrar("abcdefghijklmnopq", 13, 0);
  CONFERIR(q3.n == 2 && std::string(q3.texto[0]) == "abcdefghijklm", "palavra longa cortada");

  std::printf("teste_v09 (GUARDIA_ADAPTATIVA=%d): %d verificações, %d falhas; %zu telas capturadas; "
              "Guardiã mudou o nível em %d de %d escolhas\n",
              GUARDIA_ADAPTATIVA, verificacoes, falhas, nomes_capturados.size(), mudou, escolhas);
  return falhas == 0 ? 0 : 1;
}
