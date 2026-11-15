// r3: partidas completas com a política GRADUAL ligada (GUARDIA_POLITICA_GRADUAL=1),
// usando o sketch real, o banco do firmware (ou um banco com lacunas, trocando
// coflume_banco.h na cópia do sketch) e alternância com o Agente Secreto.
// Uso: teste_gradual_jogo <cenario>
//   completo | lacuna_viavel | lacuna_inviavel | lacuna_inviavel_off | recusa_defensiva
#include "../coflume_v09/coflume_v09.ino"

#include <set>
#include <string>

static int falhas = 0, verificacoes = 0;
#define CONFERIR(cond, msg)                                                                         \
  do {                                                                                              \
    ++verificacoes;                                                                                 \
    if (!(cond) && ++falhas <= 25) std::printf("FALHA %s:%d: %s [%s]\n", __FILE__, __LINE__, msg, #cond); \
  } while (0)

static void avancar(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 10) { stub::agora_ms() += 10; loop(); } }
static void apertar(char k) { stub::teclas().push_back(k); loop(); }
static void painel(const std::string& l) { for (char c : l) stub::entrada().push_back(c); stub::entrada().push_back('\n'); loop(); }
static bool tela_tem(const std::string& s) {
  for (const auto& o : stub::ops()) if (o.tipo == 'T' && o.texto.find(s) != std::string::npos) return true;
  return false;
}
static uint32_t duty(int g) { return (uint32_t)((map(g, 0, 180, 500, 2400) * 16383UL) / 20000UL); }
static bool servo_aberto() { return stub::pwm()[SERVO_PIN] == duty(ANGULO_ABERTO); }
static bool servo_fechado() { return stub::pwm()[SERVO_PIN] == duty(ANGULO_FECHADO); }
static size_t ultimo(const std::string& s) { return stub::serial().rfind(s); }
static int contar(const std::string& s, const std::string& p) {
  int n = 0;
  for (size_t i = s.find(p); i != std::string::npos; i = s.find(p, i + 1)) ++n;
  return n;
}
static uint32_t semente = 12345;
static uint32_t sorteio() { semente = semente * 1103515245u + 12345u; return semente >> 8; }

enum Resp { CERTA, ERRADA, ESGOTA };
static int partidas = 0, recusas = 0, limitou = 0, decisoes_g = 0, substituidas = 0;

// Joga uma partida verificando a regra gradual a cada pergunta. Devolve false se recusada.
static bool jogar(int ini, bool conferir_mlp) {
  CONFERIR(estado == MENU, "no menu");
  const size_t inicio_log = stub::serial().size();
  apertar((char)('1' + ini));
  if (estado == ERRO_BANCO) return false;
  std::set<int> cats;
  int nivel_ant = -1;
  bool acertou_ant = false;
  int ac = 0;
  for (int k = 0; k < 3; ++k) {
    if (estado == ERRO_BANCO) {  // recusa explícita durante a partida
      CONFERIR(ultimo("GUARDIA,erro,sem_nivel_compativel,politica=gradual-v2-exp") != std::string::npos,
               "recusa registrada");
      CONFERIR(tela_tem("BANCO") && tela_tem("INCOMPLETO") && servo_fechado(), "tela de banco incompleto, cofre fechado");
      CONFERIR(contar(stub::serial().substr(inicio_log), "QUESTAO:") == k, "nenhuma pergunta depois da recusa");
      ++recusas;
      apertar('#');
      return false;
    }
    CONFERIR(estado == PERGUNTA, "pergunta apresentada");
    const coflume::gradual::Decisao g = decisaoGradual;
    ++decisoes_g;
    limitou += g.limitou();
    substituidas += g.categoria_substituida();
    CONFERIR(g.viavel && decisao.nivel == g.nivel && decisao.categoria == g.categoria && nivel == g.nivel,
             "decisão gradual é a usada no jogo");
    CONFERIR(coflume::tem_pergunta(g.nivel, g.categoria) && coflume::BANCO[perguntaAtual].nivel == g.nivel &&
                 coflume::BANCO[perguntaAtual].categoria == g.categoria, "pergunta da célula decidida");
    CONFERIR(abs((int)g.nivel - ini) <= (k == 0 ? 0 : 1), "faixa do fluxo-0.8");
    if (k > 0) {
      CONFERIR(abs((int)g.nivel - nivel_ant) <= 1, "no máximo 1 nível de mudança");
      CONFERIR(acertou_ant || (int)g.nivel <= nivel_ant, "não sobe depois de erro ou tempo esgotado");
      CONFERIR(anteriorGradual.existe && anteriorGradual.nivel == nivel_ant && anteriorGradual.acertou == acertou_ant,
               "anterior registrado");
    }
    CONFERIR(!cats.count(g.categoria), "categorias distintas");
    cats.insert(g.categoria);
    // Auditoria: linha da política com os campos propostos.
    const size_t a = ultimo("GUARDIA,politica_gradual,politica=gradual-v2-exp,");
    CONFERIR(a != std::string::npos && a > inicio_log, "linha de auditoria");
    const std::string linha = stub::serial().substr(a, stub::serial().find('\n', a) - a);
    for (const char* campo : {"anterior=", "niveis_compativeis=", "candidatos=", "categoria_fluxo08=", "nivel_sem_regra=",
                              "limitou=", "motivo="})
      CONFERIR(linha.find(campo) != std::string::npos, "campo de auditoria");
    // Recalcula a decisão com o mesmo histórico: o jogo usou exatamente a política.
    if (conferir_mlp && k > 0) {
      uint8_t rest[4];
      int n = 0;
      // restantes antes desta pergunta = restantes atuais + a categoria escolhida, na posição original
      for (int i = 0; i < 4; ++i) {
        const uint8_t c = ordemCategorias[i];
        bool usada_antes = false;
        for (int u : cats) if (u == c && c != g.categoria) usada_antes = true;
        if (!usada_antes) rest[n++] = c;
      }
      coflume::gradual::Anterior an;
      an.existe = true; an.nivel = (uint8_t)nivel_ant; an.acertou = acertou_ant;
      const coflume::gradual::Decisao r = coflume::gradual::decidir_pergunta(
          k, (uint8_t)ini, rest, n, coflume::gradual::tem_pergunta_banco, nullptr,
          guardiaAtiva ? preverGuardia : nullptr, nullptr, an);
      CONFERIR(r.viavel && r.nivel == g.nivel && r.categoria == g.categoria && r.motivo == g.motivo, "decisão recalculada igual");
    }
    // Responde.
    const Resp resp = (Resp)(sorteio() % 3);
    if (resp == ESGOTA) {
      avancar(TEMPO_QUESTAO_MS + 20);
    } else {
      avancar(500 + sorteio() % 6000);
      if (sorteio() % 3 == 0) apertar('*');
      const char c = questao().correta;
      apertar(resp == CERTA ? c : (c == 'A' ? 'B' : 'A'));
      apertar('#');
    }
    CONFERIR(estado == RESULTADO, "resultado");
    nivel_ant = nivel;
    acertou_ant = resp == CERTA;
    ac += resp == CERTA;
    avancar(FEEDBACK_ERRO_MS + 50);
  }
  CONFERIR(estado == (ac >= 2 ? VITORIA : DERROTA), "fim da partida");
  CONFERIR(ac >= 2 ? servo_aberto() : servo_fechado(), "servo pela regra 2/3");
  ++partidas;
  apertar('#');
  CONFERIR(estado == MENU && servo_fechado(), "volta ao menu");
  return true;
}

// Missão do Agente Secreto completa (senha correta), no meio das partidas.
static void agente_secreto() {
  CONFERIR(estado == MENU, "menu antes do Agente");
  const size_t antes = stub::serial().size();
  apertar('A');
  char sid[12];
  std::snprintf(sid, sizeof(sid), "%08lX", (unsigned long)agente::id);
  painel(std::string("AG,READY,") + sid);
  CONFERIR(agente::fase == agente::JOGANDO, "missão em andamento");
  for (int i = 0; i < 4; ++i) apertar(agente::senha[i]);
  apertar('#');
  CONFERIR(agente::fase == agente::GANHOU && servo_aberto(), "senha correta abre");
  const std::string trecho = stub::serial().substr(antes);
  CONFERIR(trecho.find("GUARDIA,decisao") == std::string::npos && trecho.find("GUARDIA,politica_gradual") == std::string::npos,
           "Agente Secreto não usa a Guardiã nem a política");
  apertar('#');
  CONFERIR(estado == MENU && servo_fechado(), "volta ao menu");
}

int main(int argc, char** argv) {
  const std::string cenario = argc > 1 ? argv[1] : "completo";
  stub::passo_por_desenho_ms() = 1;
  setup();
  const std::string& log = stub::serial();
  painel("AG,HELLO");
  const bool ligada = GUARDIA_POLITICA_GRADUAL == 1;

  if (cenario == "lacuna_inviavel_off") {
    // Mesmo banco inviável para a regra gradual, com a opção DESLIGADA: fluxo-0.8 joga normalmente.
    CONFERIR(!ligada && log.find("politica=fluxo-0.8,banco_viavel_gradual=NA") != std::string::npos, "opção desligada");
    for (int i = 0; i < 30; ++i) {
      apertar((char)('1' + i % 3));
      CONFERIR(estado == PERGUNTA, "fluxo-0.8 inicia a partida");
      for (int k = 0; k < 3; ++k) { avancar(1000); apertar(questao().correta); apertar('#'); avancar(4000); }
      CONFERIR(estado == VITORIA, "partida completa");
      apertar('#');
    }
    CONFERIR(log.find("GUARDIA,politica_gradual") == std::string::npos, "sem linhas da política gradual");
    agente_secreto();
  } else {
    CONFERIR(ligada, "opção ligada");
    CONFERIR(log.find("politica=gradual-v2-exp") != std::string::npos, "política ativa registrada no boot");
    telaBoot();
    CONFERIR(tela_tem("Politica: gradual (exp.)"), "política na tela de boot");
    menu();
    if (cenario == "lacuna_inviavel") {
      CONFERIR(log.find("banco_viavel_gradual=0") != std::string::npos && bancoOK, "banco inviável só para a regra gradual");
      for (char k : {'1', '2', '3'}) {
        apertar(k);
        CONFERIR(estado == ERRO_BANCO && tela_tem("BANCO") && tela_tem("INCOMPLETO"), "quiz não começa: BANCO INCOMPLETO");
        CONFERIR(ultimo("GUARDIA,erro,banco_inviavel_gradual,politica=gradual-v2-exp") != std::string::npos, "motivo no log");
        apertar('#');
        CONFERIR(estado == MENU, "# volta ao menu");
      }
      CONFERIR(log.find("QUESTAO:") == std::string::npos && log.find("GUARDIA,decisao") == std::string::npos,
               "nenhuma pergunta apresentada");
      agente_secreto();  // a outra modalidade continua funcionando
      apertar('2');
      CONFERIR(estado == ERRO_BANCO, "quiz continua bloqueado depois do Agente");
      apertar('#');
    } else if (cenario == "recusa_defensiva") {
      // Força o banco inviável como se tivesse passado na verificação, para exercitar a recusa em jogo.
      CONFERIR(log.find("banco_viavel_gradual=0") != std::string::npos, "banco inviável");
      bancoGradualOK = true;
      for (int i = 0; i < 400 && recusas < 5; ++i) {
        jogar(i % 3, false);
        if (i % 50 == 0) agente_secreto();
      }
      CONFERIR(recusas >= 5, "recusa explícita exercitada");
    } else {
      CONFERIR(log.find("banco_viavel_gradual=1") != std::string::npos, "banco viável para a regra gradual");
      for (int i = 0; i < 150; ++i) {
        CONFERIR(jogar(i % 3, true), "partida completa sem recusa");
        if (i % 25 == 0) agente_secreto();
      }
      CONFERIR(recusas == 0 && partidas == 150, "150 partidas completas");
    }
  }
  for (const auto& v : stub::violacoes()) std::printf("LAYOUT: %s\n", v.c_str());
  CONFERIR(stub::violacoes().empty(), "layout sem cortes nem sobreposição em todas as telas");
  std::printf("teste_gradual_jogo %s (GUARDIA_POLITICA_GRADUAL=%d): %d verificações, %d falhas; partidas=%d recusas=%d "
              "decisoes=%d limitou=%d categorias_substituidas=%d\n",
              cenario.c_str(), GUARDIA_POLITICA_GRADUAL, verificacoes, falhas, partidas, recusas, decisoes_g, limitou,
              substituidas);
  return falhas == 0 ? 0 : 1;
}
