// Testes da partida completa da v0.8 no PC: o sketch.ino real é compilado com
// imitações do Arduino, do teclado, da tela e do PWM (testes/stub/).
// Compilar com -DGUARDIA_ADAPTATIVA=0 testa o modo de nível fixo.
#include "../sketch.ino"

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

static const uint32_t PASSO_MS = 10;

static void avancar(uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += PASSO_MS) {
    stub::agora_ms() += PASSO_MS;
    loop();
  }
}
static void apertar(char k) {
  stub::teclas().push_back(k);
  loop();
}
static bool tela_contem(const std::string& s) {
  for (const auto& t : stub::tela())
    if (t.texto.find(s) != std::string::npos) return true;
  return false;
}
static int contar(const std::string& texto, const std::string& s) {
  int n = 0;
  for (size_t p = texto.find(s); p != std::string::npos; p = texto.find(s, p + 1)) ++n;
  return n;
}
static uint32_t duty_servo(int graus) {
  const uint32_t pulso = map(graus, 0, 180, 500, 2400);
  return (pulso * ((1UL << PWM_BITS) - 1)) / 20000UL;
}

// Uma pergunta como apresentada, com o histórico que a Guardiã viu.
struct Apresentada {
  coflume::Decisao d;
  int pergunta;
  guardia::Historico hist_antes;
};

enum Resposta { CERTA, ERRADA, ESGOTA };

// Responde a pergunta atual e devolve o que foi apresentado.
static Apresentada responder(Resposta r, bool dica, uint32_t espera_ms = 2000) {
  Apresentada a{decisao, perguntaAtual, historico};
  CONFERIR(estado == PERGUNTA, "deveria estar em uma pergunta");
  if (r == ESGOTA) {
    avancar(TEMPO_QUESTAO_MS + 20);
  } else {
    avancar(espera_ms);
    if (dica) apertar('*');
    const char certa = questao().correta;
    apertar(r == CERTA ? certa : (certa == 'A' ? 'B' : 'A'));
    apertar('#');
  }
  CONFERIR(estado == RESULTADO, "deveria mostrar o resultado");
  avancar(FEEDBACK_ERRO_MS + 50);
  return a;
}

static void iniciar(int nivel_inicial) {
  CONFERIR(estado == MENU, "deveria estar no menu");
  apertar((char)('1' + nivel_inicial));
}

// Verifica uma partida inteira: faixa, categorias, histórico e escolha pela MLP.
static void verificar_partida(const std::vector<Apresentada>& p, int ini, bool adaptativa) {
  CONFERIR(p.size() == 3, "partida deve ter exatamente 3 perguntas");
  std::set<int> categorias, perguntas;
  for (size_t k = 0; k < p.size(); ++k) {
    const coflume::Decisao& d = p[k].d;
    const coflume::Pergunta& q = coflume::BANCO[p[k].pergunta];
    categorias.insert(d.categoria);
    perguntas.insert(p[k].pergunta);
    CONFERIR(q.nivel == d.nivel && q.categoria == d.categoria, "pergunta apresentada é da célula decidida");
    CONFERIR(p[k].hist_antes.n == (int)k, "histórico com as respostas anteriores da MESMA partida");
    CONFERIR(abs((int)d.nivel - ini) <= (k == 0 ? 0 : 1), "nível dentro da faixa permitida");
    CONFERIR(d.nivel <= 2, "nível entre 0 e 2");
    if (k == 0) {
      CONFERIR(d.motivo == coflume::MOTIVO_PERGUNTA_1 && d.nivel == ini, "pergunta 1 no nível escolhido");
      continue;
    }
    if (!adaptativa) {
      CONFERIR(d.motivo == coflume::MOTIVO_SEM_MODELO && d.nivel == ini, "sem Guardiã: nível fixo (v0.7)");
      continue;
    }
    CONFERIR(d.motivo == coflume::MOTIVO_GUARDIA, "perguntas 2 e 3 decididas pela Guardiã");
    // Recalcula as previsões com o runtime e o histórico visto: devem ser idênticas às registradas.
    double melhor = 1e9;
    for (int i = 0; i < d.n_disponiveis; ++i) {
      const uint8_t n = d.disponiveis[i];
      const float p_ref = guardia::prever(guardia::montar_entrada(p[k].hist_antes, ini, d.categoria, n));
      CONFERIR(d.avaliado[n] && d.previsoes[n] == p_ref, "previsão registrada = MLP recalculada");
      melhor = std::min(melhor, std::fabs((double)p_ref - 0.70));
    }
    CONFERIR(std::fabs(std::fabs((double)d.previsoes[d.nivel] - 0.70) - melhor) < 1e-6,
             "nível escolhido tem a previsão mais próxima de 0,70");
    CONFERIR(d.p_previsto == d.previsoes[d.nivel], "p registrada é a do nível apresentado");
  }
  CONFERIR(categorias.size() == 3, "três categorias distintas");
  CONFERIR(perguntas.size() == 3, "nenhuma pergunta repetida na partida");
}

static std::vector<Apresentada> jogar(int ini, const Resposta r[3], const bool dica[3]) {
  iniciar(ini);
  std::vector<Apresentada> p;
  for (int k = 0; k < 3; ++k) p.push_back(responder(r[k], dica[k]));
  return p;
}

int main() {
  stub::passo_por_desenho_ms() = 1;  // cada desenho na tela consome tempo, como no SPI real
  setup();
  const std::string& log = stub::serial();
  const bool adaptativa = GUARDIA_ADAPTATIVA == 1;

  // --- Inicialização ----------------------------------------------------------
  CONFERIR(log.find("GUARDIA,autoteste,vetores=21,") != std::string::npos, "autoteste com 21 vetores");
  CONFERIR(log.find("resultado=OK") != std::string::npos, "autoteste aprovado");
  CONFERIR(log.find("id=mlp8-sint-20261101-e6a4cf00") != std::string::npos, "modelo identificado no log");
  CONFERIR(log.find("fonte=SINTETICO") != std::string::npos, "fonte sintética no log");
  CONFERIR(bancoOK, "banco viável nas 72 combinações");
  CONFERIR(guardiaAtiva == adaptativa, "Guardiã ativa conforme GUARDIA_ADAPTATIVA");
  CONFERIR(estado == MENU && tela_contem("1 - Explorador"), "menu da v0.7 preservado");
  CONFERIR(stub::pwm()[SERVO_PIN] == duty_servo(ANGULO_FECHADO), "servo começa fechado");

  // --- Cronômetro: começa só depois da pergunta desenhada -----------------------
  const uint32_t antes_da_tecla = millis();
  iniciar(1);
  CONFERIR(estado == PERGUNTA, "partida iniciada");
  // Desenhar a pergunta consome tempo (1 ms por chamada à tela): o cronômetro só
  // começa depois disso. Depois, loop() só redesenha o tempo (2 chamadas).
  CONFERIR(inicioPergunta - antes_da_tecla >= 10, "decisão e desenho acontecem antes do cronômetro");
  CONFERIR(millis() - inicioPergunta <= 2, "cronômetro começa ao fim da apresentação");
  CONFERIR(tela_contem("30s"), "tempo inicial de 30 s na tela");
  CONFERIR(tela_contem("Desafiante"), "nível mostrado na pergunta");
  avancar(TEMPO_QUESTAO_MS - 100);
  CONFERIR(estado == PERGUNTA, "ainda dentro dos 30 s");
  avancar(200);
  CONFERIR(estado == RESULTADO && tela_contem("Tempo esgotado!"), "tempo esgotado conta como erro");
  CONFERIR(historico.n == 1 && !historico.itens[0].acertou && historico.itens[0].tempo_gasto_s == 30.0f,
           "timeout registrado no histórico: erro com 30 s");
  CONFERIR(log.find("esgotou=1") != std::string::npos, "timeout no log");
  CONFERIR(stub::pinos()[LED_VERMELHO] == HIGH, "LED vermelho no erro");
  avancar(FEEDBACK_ERRO_MS + 50);
  // Responde as outras duas e termina.
  responder(ERRADA, true);
  responder(CERTA, false);
  CONFERIR(estado == DERROTA && acertos == 1, "1 acerto: cofre não abre");
  CONFERIR(stub::pwm()[SERVO_PIN] == duty_servo(ANGULO_FECHADO), "servo continua fechado com 1 acerto");
  CONFERIR(historico.itens[1].usou_dica && !historico.itens[1].acertou, "dica e erro registrados");
  CONFERIR(contar(log, "GUARDIA,decisao,") == 3 && contar(log, "GUARDIA,resposta,") == 3, "3 decisões e 3 respostas");
  apertar('#');
  CONFERIR(estado == MENU, "reinício por # volta ao menu");

  // --- Feedback educativo ---------------------------------------------------------
  iniciar(0);
  {
    const coflume::Pergunta& q = questao();
    avancar(500);
    apertar(q.correta == 'A' ? 'B' : 'A');
    apertar('#');
    CONFERIR(tela_contem("VAMOS APRENDER!"), "cabeçalho de erro da v0.7");
    char esperado[16];
    std::snprintf(esperado, sizeof(esperado), "Resposta: %c", q.correta);
    CONFERIR(tela_contem(esperado), "letra correta");
    CONFERIR(tela_contem(q.alt[q.correta - 'A']), "texto da alternativa correta");
    CONFERIR(tela_contem(std::string("Dica: ") + q.dica), "dica no feedback de erro");
    avancar(FEEDBACK_ERRO_MS + 50);
  }
  // '#' sem alternativa marcada não confirma; tecla após o prazo vale como timeout.
  {
    apertar('#');
    CONFERIR(estado == PERGUNTA, "# sem alternativa não confirma");
    stub::agora_ms() = inicioPergunta + TEMPO_QUESTAO_MS + 5;  // a tecla chega depois do prazo, sem loop()
    apertar('A');
    CONFERIR(estado == RESULTADO && historico.itens[historico.n - 1].tempo_gasto_s == 30.0f,
             "prazo tem prioridade sobre a tecla");
    avancar(FEEDBACK_ERRO_MS + 50);
  }
  responder(CERTA, false);
  CONFERIR(estado == DERROTA, "partida terminada");
  apertar('#');

  // --- Desbloqueio: só com 2 ou mais acertos, pelo validador do jogo -------------
  const Resposta tres[3] = {CERTA, CERTA, CERTA}, dois[3] = {CERTA, ERRADA, CERTA}, um[3] = {ERRADA, ESGOTA, CERTA},
                 zero[3] = {ERRADA, ERRADA, ESGOTA};
  const bool sem[3] = {false, false, false}, com[3] = {true, false, true};
  struct Caso { const Resposta* r; const bool* dica; int ini; bool abre; };
  const Caso casos[] = {{tres, sem, 0, true}, {dois, com, 1, true}, {um, sem, 2, false}, {zero, com, 1, false},
                        {tres, com, 2, true}, {dois, sem, 0, true}};
  for (const Caso& c : casos) {
    const uint32_t sessao_anterior = sessaoId;
    const std::vector<Apresentada> p = jogar(c.ini, c.r, c.dica);
    verificar_partida(p, c.ini, adaptativa);
    CONFERIR(sessaoId != sessao_anterior, "nova sessão a cada partida");
    CONFERIR(estado == (c.abre ? VITORIA : DERROTA), "resultado da partida");
    CONFERIR(stub::pwm()[SERVO_PIN] == duty_servo(c.abre ? ANGULO_ABERTO : ANGULO_FECHADO), "servo conforme a regra");
    CONFERIR(stub::pinos()[LED_VERDE] == (c.abre ? HIGH : LOW), "LED verde conforme a regra");
    for (int k = 0; k < 3; ++k) {
      const guardia::Resposta& h = historico.itens[k];
      CONFERIR(h.acertou == (c.r[k] == CERTA), "acerto registrado");
      CONFERIR(h.usou_dica == (c.r[k] != ESGOTA && c.dica[k]), "dica registrada");
      CONFERIR(h.categoria == p[k].d.categoria && h.nivel == p[k].d.nivel, "categoria e nível registrados");
      CONFERIR(h.tempo_limite_s == 30.0f && h.tempo_gasto_s > 0.0f && h.tempo_gasto_s <= 30.0f, "tempo registrado");
    }
    apertar('#');
    CONFERIR(estado == MENU && stub::pwm()[SERVO_PIN] == duty_servo(ANGULO_FECHADO), "menu fecha o cofre");
  }

  // --- Muitas partidas: faixa, categorias, rodízio e histórico novo ----------------
  std::set<std::string> vistas_mestre_cg;
  int escolhas_guardia = 0, mudou_nivel = 0;
  for (int partida = 0; partida < 60; ++partida) {
    const int ini = partida % 3;
    const Resposta r[3] = {partida % 2 ? CERTA : ERRADA, partida % 4 < 2 ? CERTA : ERRADA, CERTA};
    const std::vector<Apresentada> p = jogar(ini, r, sem);
    CONFERIR(p[0].hist_antes.n == 0, "partida nova começa sem histórico");
    verificar_partida(p, ini, adaptativa);
    for (const Apresentada& a : p) {
      if (a.d.nivel == 2 && a.d.categoria == 3) vistas_mestre_cg.insert(coflume::BANCO[a.pergunta].id);
      if (a.d.motivo == coflume::MOTIVO_GUARDIA) {
        ++escolhas_guardia;
        if (a.d.nivel != ini) ++mudou_nivel;
      }
    }
    apertar('#');
  }
  CONFERIR(vistas_mestre_cg.size() == 2, "rodízio usa as 2 perguntas da célula Mestre x Conhecimentos gerais");
  if (adaptativa) CONFERIR(escolhas_guardia == 120 && mudou_nivel > 0, "Guardiã escolheu níveis diferentes do inicial");

  // --- Falha da Guardiã durante a execução: fallback seguro ----------------------
  guardiaAtiva = false;
  const std::vector<Apresentada> fb = jogar(1, dois, sem);
  verificar_partida(fb, 1, false);
  CONFERIR(estado == VITORIA, "jogo continua sem a Guardiã");
  apertar('#');
  guardiaAtiva = adaptativa;

  // --- Telas: alternativas cabem na coluna --------------------------------------
  for (int i = 0; i < COFLUME_BANCO_N; ++i)
    for (int a = 0; a < 4; ++a) {
      const int largura = 6 * (std::strlen(coflume::BANCO[i].alt[a]) > (size_t)ALT_MAX_TAMANHO_2 ? 1 : 2) *
                          (3 + (int)std::strlen(coflume::BANCO[i].alt[a]));
      CONFERIR((a % 2 == 0 ? 12 : 165) + largura <= (a % 2 == 0 ? 165 : 320), "alternativa cabe na coluna");
    }

  std::printf("teste_partida (GUARDIA_ADAPTATIVA=%d): %d verificações, %d falhas; Guardiã mudou o nível em %d de %d escolhas\n",
              GUARDIA_ADAPTATIVA, verificacoes, falhas, mudou_nivel, escolhas_guardia);
  return falhas == 0 ? 0 : 1;
}
