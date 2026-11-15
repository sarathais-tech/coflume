// Testes da política gradual em C++ (experimento gradual-v2-exp):
//  1. regras e lacunas no banco, com previsões aleatórias;
//  2. as MESMAS partidas da referência Python (referencia_gradual.csv), com a
//     MLP REAL no runtime embarcado: mesmas decisões; previsões com erro < 1e-5.
// Uso: teste_gradual <referencia_gradual.csv descompactado>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "guardia_modelo.h"
#include "guardia_runtime.h"
#include "coflume_politica_gradual.h"

using namespace coflume;
namespace G = coflume::gradual;

static int falhas = 0, verificacoes = 0;
#define CONFERIR(cond, msg)                                                                   \
  do {                                                                                        \
    ++verificacoes;                                                                           \
    if (!(cond)) {                                                                            \
      if (++falhas <= 20) std::printf("FALHA %s:%d: %s [%s]\n", __FILE__, __LINE__, msg, #cond); \
    }                                                                                         \
  } while (0)

// Banco como máscara de 12 células (n*4+c).
struct Mascara { bool tem[N_NIVEIS][N_CATEGORIAS]; };
static bool tem_mascara(void* ctx, uint8_t n, uint8_t c) { return static_cast<Mascara*>(ctx)->tem[n][c]; }
static Mascara de_texto(const std::string& s) {
  Mascara m;
  for (int n = 0; n < 3; ++n) for (int c = 0; c < 4; ++c) m.tem[n][c] = s[n * 4 + c] == '1';
  return m;
}
static G::Anterior ant(uint8_t nivel, bool acertou) { G::Anterior a; a.existe = true; a.nivel = nivel; a.acertou = acertou; return a; }

// --- 1. Regras com previsões aleatórias e bancos com lacunas -----------------------
static float P_ALEATORIA[3];
static float prever_tabela(void*, uint8_t n, uint8_t) { return P_ALEATORIA[n]; }

static void teste_regras() {
  std::mt19937 rng(2026);
  std::uniform_real_distribution<float> u(0.0f, 1.0f);
  int partidas = 0, recusas = 0, bancos_viaveis = 0;
  for (int b = 0; b < 400; ++b) {
    Mascara m;
    for (auto& linha : m.tem) for (bool& x : linha) x = true;
    const int vazias = (int)(rng() % 8);
    for (int k = 0; k < vazias; ++k) m.tem[rng() % 3][rng() % 4] = false;
    const bool viavel = G::banco_viavel(tem_mascara, &m);
    bancos_viaveis += viavel;
    for (int p = 0; p < 12; ++p) {
      const uint8_t ini = (uint8_t)(rng() % 3);
      uint8_t rest[4] = {0, 1, 2, 3};
      std::shuffle(rest, rest + 4, rng);
      int n = 4;
      const int modo = (int)(rng() % 3);  // 0 MLP fictícia, 1 com inválidas, 2 sem modelo
      G::Anterior a;
      bool recusou = false;
      for (int q = 0; q < 3; ++q) {
        for (float& x : P_ALEATORIA) {
          x = u(rng);
          if (modo == 1 && rng() % 3 == 0) x = (rng() % 2) ? NAN : guardia::INVALIDO;
        }
        const G::Decisao d = G::decidir_pergunta(q, ini, rest, n, tem_mascara, &m,
                                                 modo == 2 ? nullptr : prever_tabela, nullptr, a);
        if (!d.viavel) { recusou = true; break; }
        CONFERIR(m.tem[d.nivel][d.categoria], "pergunta existe no banco");
        CONFERIR(abs((int)d.nivel - ini) <= (q == 0 ? 0 : 1), "faixa do fluxo-0.8");
        if (q > 0) {
          CONFERIR(abs((int)d.nivel - (int)a.nivel) <= 1, "no máximo 1 nível de mudança");
          CONFERIR(a.acertou || d.nivel <= a.nivel, "não sobe depois de erro");
        }
        // categoria = primeira viável na ordem sorteada
        int primeira = -1;
        for (int c = 0; c < n && primeira < 0; ++c)
          for (int i = 0; i < d.n_compativeis; ++i) if (m.tem[d.compativeis[i]][rest[c]]) { primeira = rest[c]; break; }
        CONFERIR(primeira == d.categoria, "primeira categoria viável da ordem sorteada");
        int w = 0;
        for (int k = 0; k < n; ++k) if (rest[k] != d.categoria) rest[w++] = rest[k];
        n = w;
        a = ant(d.nivel, rng() % 2 == 0);
      }
      if (recusou) { ++recusas; CONFERIR(!viavel, "recusa só em banco declarado inviável"); }
      else ++partidas;
    }
  }
  CONFERIR(partidas > 2000 && recusas > 0 && bancos_viaveis > 50 && bancos_viaveis < 350, "cobertura do teste");

  // Caso que a v1 errava: Matemática só no Mestre, erro em Desafiante.
  Mascara m = de_texto("011101111111");  // sem (0,0) e (1,0); célula = n*4+c
  P_ALEATORIA[0] = .80f; P_ALEATORIA[1] = .40f; P_ALEATORIA[2] = .65f;
  const uint8_t r[3] = {0, 2, 3};
  const G::Decisao d = G::decidir_pergunta(1, 1, r, 3, tem_mascara, &m, prever_tabela, nullptr, ant(1, false));
  CONFERIR(d.viavel && d.categoria == 2 && d.nivel <= 1 && d.categoria_fluxo08 == 0, "caso da v1 corrigido");
  // Nenhuma categoria compatível: recusa explícita.
  Mascara m2 = de_texto("010101011111");  // sem (0,0),(0,2),(1,0),(1,2)
  const uint8_t r2[2] = {0, 2};
  const G::Decisao d2 = G::decidir_pergunta(1, 1, r2, 2, tem_mascara, &m2, prever_tabela, nullptr, ant(1, false));
  CONFERIR(!d2.viavel && std::strcmp(d2.nome_motivo(), "sem_nivel_compativel") == 0, "recusa explícita");
  // Anterior ausente nas perguntas 2 e 3: recusa (entrada inválida).
  CONFERIR(!G::decidir_pergunta(1, 1, r, 3, tem_mascara, &m, prever_tabela, nullptr, G::Anterior()).viavel,
           "anterior obrigatório");
  // Banco completo do firmware é viável para a regra gradual.
  CONFERIR(G::banco_viavel(G::tem_pergunta_banco, nullptr), "banco_v08 viável");
  std::printf("regras: %d partidas, %d recusas em bancos inviáveis, %d de 400 bancos viáveis\n", partidas, recusas,
              bancos_viaveis);
}

// --- 2. Mesmas partidas da referência Python, com a MLP real ------------------------
struct Perfil { bool acertou; float tempo; bool dica; };
static const Perfil PERFIS[4] = {{true, 3.0f, false}, {true, 15.0f, true}, {false, 12.0f, false}, {false, 30.0f, false}};
static guardia::Historico H;
static uint8_t INI;
static bool INVALIDA_N2 = false;
static float prever_mlp(void*, uint8_t n, uint8_t c) {
  if (INVALIDA_N2 && n == 2) return NAN;
  return guardia::prever(guardia::montar_entrada(H, INI, c, n));
}

static void teste_referencia(const char* caminho) {
  std::ifstream f(caminho);
  CONFERIR(f.good(), "referência encontrada");
  std::string linha;
  std::getline(f, linha);
  std::map<std::string, bool> viab;
  long partidas = 0, decisoes = 0, recusas = 0, limitou = 0;
  float erro_p = 0.0f;
  while (std::getline(f, linha)) {
    std::vector<std::string> v;
    std::stringstream ss(linha);
    for (std::string x; std::getline(ss, x, ',');) v.push_back(x);
    if (v.size() != 30) { CONFERIR(false, "linha mal formada"); continue; }
    Mascara m = de_texto(v[0]);
    if (!viab.count(v[0])) {
      viab[v[0]] = G::banco_viavel(tem_mascara, &m);
      CONFERIR(viab[v[0]] == (v[1] == "1"), "viabilidade igual à do Python");
    }
    const std::string modo = v[2];
    INI = (uint8_t)std::stoi(v[3]);
    INVALIDA_N2 = modo == "invalida_n2";
    uint8_t rest[4];
    for (int i = 0; i < 4; ++i) rest[i] = (uint8_t)(v[4][i] - '0');
    int n = 4;
    guardia::historico_limpar(H);
    G::Anterior a;
    for (int q = 0; q < 3; ++q) {
      const int b = 6 + 8 * q;
      const G::Decisao d = G::decidir_pergunta(q, INI, rest, n, tem_mascara, &m,
                                               modo == "sem_modelo" ? nullptr : prever_mlp, nullptr, a);
      if (v[b] == "RECUSA") {
        CONFERIR(!d.viavel, "recusa igual à do Python");
        ++recusas;
        break;
      }
      CONFERIR(d.viavel, "decisão viável igual à do Python");
      if (!d.viavel) break;
      ++decisoes;
      CONFERIR(d.categoria == std::stoi(v[b + 1]) && d.nivel == std::stoi(v[b + 2]), "categoria e nível iguais");
      CONFERIR(v[b + 3] == d.nome_motivo(), "motivo igual");
      CONFERIR(d.nivel_sem_regra == std::stoi(v[b + 7]), "escolha sem a regra igual");
      limitou += d.limitou();
      for (int nv = 0; nv < 3; ++nv) {
        const std::string& p = v[b + 4 + nv];
        if (p == "NA") { CONFERIR(!d.avaliado[nv], "mesmos níveis avaliados"); continue; }
        CONFERIR(d.avaliado[nv], "mesmos níveis avaliados");
        if (p == "nan") { CONFERIR(std::isnan(d.previsoes[nv]), "NaN igual"); continue; }
        erro_p = std::max(erro_p, std::fabs(std::stof(p) - d.previsoes[nv]));
      }
      int w = 0;
      for (int k = 0; k < n; ++k) if (rest[k] != d.categoria) rest[w++] = rest[k];
      n = w;
      if (q < 2) {
        const Perfil& pf = PERFIS[v[5][q] - '0'];
        guardia::historico_adicionar(H, {d.categoria, d.nivel, pf.acertou, pf.tempo, 30.0f, pf.dica});
        a = ant(d.nivel, pf.acertou);
      }
    }
    ++partidas;
  }
  CONFERIR(partidas == 49536, "49.536 partidas de referência");
  CONFERIR(erro_p < 1e-5f, "previsões iguais às do Python (< 1e-5)");
  std::printf("referência Python: %ld partidas, %ld decisões, %ld recusas, %zu bancos, erro_p_max=%.3g, "
              "regra limitou %ld decisões\n", partidas, decisoes, recusas, viab.size(), (double)erro_p, limitou);
}

int main(int argc, char** argv) {
  teste_regras();
  teste_referencia(argc > 1 ? argv[1] : "referencia_gradual.csv");
  std::printf("teste_gradual: %d verificações, %d falhas\n", verificacoes, falhas);
  return falhas == 0 ? 0 : 1;
}
