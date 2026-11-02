// Testes no PC da política (coflume_politica.h) e da inferência embarcada:
//  1. os 400 vetores da bancada aprovada (equivalência numérica com o Python);
//  2. regras do fluxo-0.8: faixa, alvo 0,70, empates, fallbacks, categorias, rodízio;
//  3. 1.224 partidas comparadas com a referência em Python (referencia_politica.csv).
// Uso: teste_politica <caminho de referencia_politica.csv>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "guardia_modelo.h"
#include "guardia_runtime.h"
#include "vetores_teste.h"
#include "guardia_autoteste.h"
#include "coflume_banco.h"
#include "coflume_politica.h"

using namespace coflume;

static int falhas = 0, verificacoes = 0;
#define CONFERIR(cond, msg)                                                  \
  do {                                                                       \
    ++verificacoes;                                                          \
    if (!(cond)) {                                                           \
      ++falhas;                                                              \
      std::printf("FALHA %s:%d: %s [%s]\n", __FILE__, __LINE__, msg, #cond); \
    }                                                                        \
  } while (0)

// ---------------------------------------------------------------------------
// 1. Equivalência com a bancada (400 vetores) e autoteste = subconjunto exato
// ---------------------------------------------------------------------------
static void teste_vetores() {
  guardia::Historico h;
  float erro_p = 0.0f, erro_f = 0.0f;
  int falhas_int = 0;
  for (int k = 0; k < GUARDIA_N_VETORES; ++k) {
    const GuardiaVetorTeste& v = GUARDIA_VETORES[k];
    guardia::historico_limpar(h);
    for (int i = 0; i < v.n_historico; ++i) guardia::historico_adicionar(h, v.historico[i]);
    const guardia::Entrada e = guardia::montar_entrada(h, v.nivel_inicial, v.categoria, v.nivel_candidato);
    const guardia::Entrada& x = v.entrada_esperada;
    if (e.nivel_candidato != x.nivel_candidato || e.nivel_inicial != x.nivel_inicial ||
        e.n_respondidas != x.n_respondidas || e.categoria != x.categoria || e.resultado_ultima != x.resultado_ultima ||
        e.sequencia != x.sequencia)
      ++falhas_int;
    const float d[4] = {std::fabs(e.taxa_acerto_sessao - x.taxa_acerto_sessao),
                        std::fabs(e.taxa_acerto_categoria - x.taxa_acerto_categoria),
                        std::fabs(e.tempo_medio_norm - x.tempo_medio_norm),
                        std::fabs(e.dicas_por_questao - x.dicas_por_questao)};
    for (float di : d) erro_f = std::max(erro_f, di);
    erro_p = std::max(erro_p, std::fabs(guardia::prever(e) - v.p_esperado));
  }
  CONFERIR(GUARDIA_N_VETORES == 400, "400 vetores da bancada");
  CONFERIR(std::strcmp(GUARDIA_VETORES_MODELO_ID, GUARDIA_MODELO_ID) == 0, "vetores do mesmo modelo");
  CONFERIR(erro_p < 1e-5f, "erro de P(acerto) < 1e-5 nos 400 vetores");
  CONFERIR(erro_f <= 1e-6f && falhas_int == 0, "features idênticas nos 400 vetores");
  std::printf("400 vetores: erro_p_max=%.3g erro_features_max=%.3g falhas_inteiras=%d\n", (double)erro_p,
              (double)erro_f, falhas_int);

  std::set<int> cats, tamanhos, niveis;
  for (int k = 0; k < GUARDIA_AUTOTESTE_N; ++k) {
    const GuardiaAutoteste& a = GUARDIA_AUTOTESTE[k];
    const GuardiaVetorTeste& v = GUARDIA_VETORES[a.indice_original];
    CONFERIR(std::memcmp(&a.entrada_esperada, &v.entrada_esperada, sizeof(guardia::Entrada)) == 0 &&
                 a.p_esperado == v.p_esperado && a.n_historico == v.n_historico &&
                 a.nivel_candidato == v.nivel_candidato && a.categoria == v.categoria,
             "autoteste é cópia exata do vetor original");
    for (int i = 0; i < a.n_historico; ++i)
      CONFERIR(std::memcmp(&a.historico[i], &v.historico[i], sizeof(guardia::Resposta)) == 0, "histórico copiado");
    cats.insert(a.categoria);
    tamanhos.insert(a.n_historico);
    niveis.insert(a.nivel_candidato);
  }
  CONFERIR(cats.size() == 4 && niveis.size() == 3 && tamanhos.count(0) && tamanhos.size() >= 5,
           "autoteste cobre 4 categorias, 3 níveis, partida a frio e históricos longos");
}

// ---------------------------------------------------------------------------
// 2. Regras do fluxo-0.8 com previsões forçadas
// ---------------------------------------------------------------------------
static float P_FORCADA[3];
static float prever_forcada(void*, uint8_t nivel, uint8_t) { return P_FORCADA[nivel]; }
static const uint8_t TODAS[4] = {0, 1, 2, 3};

static Decisao decidir_com(int ordem, uint8_t ini, float p0, float p1, float p2) {
  P_FORCADA[0] = p0; P_FORCADA[1] = p1; P_FORCADA[2] = p2;
  return decidir_pergunta(ordem, ini, TODAS, 4, prever_forcada, nullptr);
}

static uint32_t semente = 1;
static uint32_t aleatorio_teste() { semente = semente * 1664525u + 1013904223u; return semente; }

static void teste_regras() {
  uint8_t s[3];
  CONFERIR(niveis_permitidos(0, 2, s) == 1 && s[0] == 2, "pergunta 1: só o nível inicial");
  CONFERIR(niveis_permitidos(1, 0, s) == 2 && s[0] == 0 && s[1] == 1, "Explorador: 0 e 1");
  CONFERIR(niveis_permitidos(2, 1, s) == 3, "Desafiante: 0, 1 e 2");
  CONFERIR(niveis_permitidos(1, 2, s) == 2 && s[0] == 1 && s[1] == 2, "Mestre: 1 e 2");

  // Alvo 0,70.
  CONFERIR(decidir_com(1, 1, 0.90f, 0.40f, 0.20f).nivel == 0, "mais próximo de 0,70 (abaixo do inicial)");
  CONFERIR(decidir_com(1, 1, 0.95f, 0.85f, 0.69f).nivel == 2, "mais próximo de 0,70 (acima do inicial)");
  CONFERIR(decidir_com(2, 1, 0.80f, 0.71f, 0.30f).nivel == 1, "mais próximo de 0,70 (inicial)");
  CONFERIR(decidir_com(1, 0, 0.50f, 0.95f, 0.70f).nivel == 0, "Explorador nunca vai para Mestre");
  // Empates (diferença < 1e-6): prefere o inicial; entre dois não iniciais, o mais baixo.
  CONFERIR(decidir_com(1, 1, 0.60f, 0.80f, 0.20f).nivel == 1, "empate com o inicial: fica no inicial");
  CONFERIR(decidir_com(1, 1, 0.80f, 0.20f, 0.60f).nivel == 0, "empate entre 0 e 2: o mais baixo");
  CONFERIR(decidir_com(1, 1, 0.7000004f, 0.6999998f, 0.1f).nivel == 1, "diferença < 1e-6 conta como empate");
  // Fallbacks.
  const float ruins[] = {guardia::INVALIDO, NAN, INFINITY, 1.5f, -0.01f};
  for (float r : ruins) {
    const Decisao d = decidir_com(1, 1, 0.70f, r, 0.69f);
    CONFERIR(d.motivo == MOTIVO_PREVISAO_INVALIDA && d.nivel == 1, "previsão inválida: nível inicial");
    CONFERIR(d.p_previsto == SEM_PREVISAO, "previsão inválida não é registrada como p");
  }
  const Decisao sem = decidir_pergunta(2, 2, TODAS, 4, nullptr, nullptr);
  CONFERIR(sem.motivo == MOTIVO_SEM_MODELO && sem.nivel == 2 && sem.p_previsto == SEM_PREVISAO, "sem modelo");
  const Decisao q1 = decidir_com(0, 1, 0.1f, 0.9f, 0.1f);
  CONFERIR(q1.motivo == MOTIVO_PERGUNTA_1 && q1.nivel == 1 && q1.p_previsto == 0.9f, "pergunta 1 só registra");
  // Banco completo (13 perguntas, 12 células): sem substituição de categoria.
  for (int n = 0; n < 3; ++n)
    for (int c = 0; c < 4; ++c) CONFERIR(tem_pergunta(n, c), "todas as 12 células cobertas");
  CONFERIR(perguntas_na_celula(2, 3) == 2, "Mestre x Conhecimentos gerais tem 2 perguntas");
  CONFERIR(banco_viavel(), "72 combinações viáveis");
  CONFERIR(!decidir_pergunta(0, 0, TODAS, 0, nullptr, nullptr).viavel, "sem categorias: inviável");

  // Sorteio: sempre uma permutação de 0..3.
  for (int i = 0; i < 1000; ++i) {
    uint8_t o[4];
    sortear_categorias(o, aleatorio_teste);
    CONFERIR(std::set<int>(o, o + 4).size() == 4, "ordem de categorias é permutação");
  }
  // Rodízio: alterna dentro da célula; nunca repete na partida.
  Rodizio r;
  rodizio_limpar(r);
  bool usada[COFLUME_BANCO_N] = {};
  const int a = escolher_pergunta(r, 2, 3, usada), b = escolher_pergunta(r, 2, 3, usada),
            c = escolher_pergunta(r, 2, 3, usada);
  CONFERIR(a != b && a == c, "rodízio entre partidas alterna as perguntas da célula");
  usada[b] = true;
  CONFERIR(escolher_pergunta(r, 2, 3, usada) == a, "pula a pergunta já usada na partida");
  usada[a] = true;
  CONFERIR(escolher_pergunta(r, 2, 3, usada) == -1, "célula esgotada na partida devolve -1");
  CONFERIR(desbloqueia(2) && desbloqueia(3) && !desbloqueia(1) && !desbloqueia(0), "2 ou mais acertos");
}

// ---------------------------------------------------------------------------
// 3. Comparação com a referência Python (guardia/politica.py + MLP do .npz)
// ---------------------------------------------------------------------------
struct Perfil { bool acertou; float tempo; bool dica; };
static const Perfil PERFIS[4] = {{true, 6.0f, false}, {true, 20.0f, true}, {false, 12.0f, false}, {false, 30.0f, false}};

static guardia::Historico H_REF;
static uint8_t INI_REF;
static float prever_ref(void*, uint8_t nivel, uint8_t cat) {
  return guardia::prever(guardia::montar_entrada(H_REF, INI_REF, cat, nivel));
}

static void teste_referencia(const char* caminho) {
  std::ifstream f(caminho);
  CONFERIR(f.good(), "referencia_politica.csv encontrado");
  std::string linha;
  std::getline(f, linha);
  int partidas = 0, decisoes = 0, mudancas = 0;
  float erro_p = 0.0f;
  while (std::getline(f, linha)) {
    std::vector<std::string> c;
    std::stringstream ss(linha);
    for (std::string x; std::getline(ss, x, ',');) c.push_back(x);
    if (c.size() != 22) { CONFERIR(false, "linha mal formada"); continue; }
    INI_REF = (uint8_t)std::stoi(c[0]);
    uint8_t restantes[4];
    int n = 4;
    for (int i = 0; i < 4; ++i) restantes[i] = (uint8_t)(c[1][i] - '0');
    const bool com_modelo = c[3] == "1";
    guardia::historico_limpar(H_REF);
    for (int q = 0; q < 3; ++q) {
      const int b = 4 + 6 * q;
      const Decisao d = decidir_pergunta(q, INI_REF, restantes, n, com_modelo ? prever_ref : nullptr, nullptr);
      CONFERIR(d.categoria == std::stoi(c[b]) && d.nivel == std::stoi(c[b + 1]), "categoria e nível iguais ao Python");
      CONFERIR(c[b + 2] == nome_motivo(d.motivo), "motivo igual ao Python");
      for (int nv = 0; nv < 3; ++nv) {
        const float p_py = std::stof(c[b + 3 + nv]);
        const float p_cpp = d.avaliado[nv] ? d.previsoes[nv] : -1.0f;
        CONFERIR((p_py < 0) == (p_cpp < 0), "mesmos níveis avaliados");
        if (p_py >= 0) erro_p = std::max(erro_p, std::fabs(p_py - p_cpp));
      }
      ++decisoes;
      if (d.motivo == MOTIVO_GUARDIA && d.nivel != INI_REF) ++mudancas;
      int w = 0;
      for (int k = 0; k < n; ++k)
        if (restantes[k] != d.categoria) restantes[w++] = restantes[k];
      n = w;
      if (q < 2) {
        const Perfil& p = PERFIS[c[2][q] - '0'];
        guardia::historico_adicionar(H_REF, {d.categoria, d.nivel, p.acertou, p.tempo, 30.0f, p.dica});
      }
    }
    ++partidas;
  }
  CONFERIR(partidas == 1224, "1.224 partidas de referência");
  CONFERIR(erro_p < 1e-5f, "previsões iguais ao Python (< 1e-5)");
  std::printf("referência Python: %d partidas, %d decisões, erro_p_max=%.3g, Guardiã mudou o nível em %d\n", partidas,
              decisoes, (double)erro_p, mudancas);
}

int main(int argc, char** argv) {
  teste_vetores();
  teste_regras();
  teste_referencia(argc > 1 ? argv[1] : "referencia_politica.csv");
  std::printf("teste_politica: %d verificações, %d falhas\n", verificacoes, falhas);
  return falhas == 0 ? 0 : 1;
}
