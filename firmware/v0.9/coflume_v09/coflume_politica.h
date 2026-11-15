// =============================================================================
//  coflume_politica.h — política da partida fluxo-0.8 (Coflume v0.8 EXPERIMENTAL)
//
//  Porta em C++ de guardia/politica.py (coflume-guardian-ai, branch
//  feat/validacao-final-ab), conforme docs/fluxo_partida.md, seções 2 e 3.
//
//  Lógica pura: sem Arduino, sem Serial, sem hardware, sem alocação dinâmica.
//  Testada no PC (testes/) e comparada com a referência em Python.
//
//  A Guardiã só ESTIMA P(acerto). Quem decide o nível é esta política, e quem
//  aciona o servo é o validador do jogo (sketch.ino). Nada aqui controla o servo.
// =============================================================================
#pragma once
#include <stdint.h>
#include <stdlib.h>

#include <cmath>

#include "coflume_banco.h"

namespace coflume {

constexpr int N_NIVEIS = 3;
constexpr int N_CATEGORIAS = 4;
constexpr int N_PERGUNTAS = 3;
constexpr int FAIXA_NIVEL = 1;
constexpr int ACERTOS_PARA_DESBLOQUEIO = 2;
constexpr double ALVO_P_ACERTO = 0.70;
constexpr double TOLERANCIA_EMPATE = 1e-6;
constexpr float SEM_PREVISAO = -1.0f;  // igual a guardia::INVALIDO

enum Motivo : uint8_t {
  MOTIVO_PERGUNTA_1 = 0,           // pergunta 1: sempre o nível escolhido pela criança
  MOTIVO_GUARDIA = 1,              // nível com previsão mais próxima de 0,70
  MOTIVO_SEM_MODELO = 2,           // modelo desativado ou reprovado no autoteste
  MOTIVO_PREVISAO_INVALIDA = 3,    // alguma previsão INVALIDO, NaN ou fora de [0, 1]
};

inline const char* nome_motivo(Motivo m) {
  switch (m) {
    case MOTIVO_PERGUNTA_1: return "nivel_inicial_pergunta_1";
    case MOTIVO_GUARDIA: return "guardia";
    case MOTIVO_SEM_MODELO: return "fallback_sem_modelo";
    default: return "fallback_previsao_invalida";
  }
}

inline bool previsao_valida(float p) { return std::isfinite(p) && p >= 0.0f && p <= 1.0f; }

// ---------------------------------------------------------------------------
// Banco: quantas perguntas existem em cada célula (nível × categoria)
// ---------------------------------------------------------------------------
inline int perguntas_na_celula(uint8_t nivel, uint8_t categoria) {
  int n = 0;
  for (int i = 0; i < COFLUME_BANCO_N; ++i)
    if (BANCO[i].nivel == nivel && BANCO[i].categoria == categoria) ++n;
  return n;
}
inline bool tem_pergunta(uint8_t nivel, uint8_t categoria) { return perguntas_na_celula(nivel, categoria) > 0; }

// Níveis que podem ser oferecidos na pergunta `ordem` (0 = primeira).
inline int niveis_permitidos(int ordem, uint8_t nivel_inicial, uint8_t saida[N_NIVEIS]) {
  if (ordem == 0) {
    saida[0] = nivel_inicial;
    return 1;
  }
  int n = 0;
  for (int nivel = 0; nivel < N_NIVEIS; ++nivel) {
    const int d = nivel - (int)nivel_inicial;
    if (d >= -FAIXA_NIVEL && d <= FAIXA_NIVEL) saida[n++] = (uint8_t)nivel;
  }
  return n;
}

// ---------------------------------------------------------------------------
// Decisão de uma pergunta
// ---------------------------------------------------------------------------
struct Decisao {
  bool viavel = false;           // false: nenhuma categoria restante serve (banco inviável)
  uint8_t ordem = 0;
  uint8_t nivel_inicial = 0;
  uint8_t categoria_sorteada = 0;
  uint8_t categoria = 0;         // categoria efetivamente apresentada
  uint8_t permitidos[N_NIVEIS] = {0, 0, 0};
  uint8_t n_permitidos = 0;
  uint8_t disponiveis[N_NIVEIS] = {0, 0, 0};
  uint8_t n_disponiveis = 0;
  float previsoes[N_NIVEIS] = {SEM_PREVISAO, SEM_PREVISAO, SEM_PREVISAO};  // por nível; só disponíveis
  bool avaliado[N_NIVEIS] = {false, false, false};
  uint8_t nivel = 0;             // nível efetivamente apresentado
  float p_previsto = SEM_PREVISAO;  // previsão DO NÍVEL APRESENTADO
  Motivo motivo = MOTIVO_GUARDIA;

  bool categoria_substituida() const { return categoria != categoria_sorteada; }
  bool celula_vazia() const { return n_disponiveis != n_permitidos; }
};

// Nível disponível mais próximo do inicial; empate -> o mais baixo.
inline uint8_t mais_proximo_do_inicial(const uint8_t* niveis, int n, uint8_t nivel_inicial) {
  uint8_t melhor = niveis[0];
  int melhor_d = 99;
  for (int i = 0; i < n; ++i) {
    const int d = abs((int)niveis[i] - (int)nivel_inicial);
    if (d < melhor_d || (d == melhor_d && niveis[i] < melhor)) {
      melhor = niveis[i];
      melhor_d = d;
    }
  }
  return melhor;
}

// Política ADAPTATIVA: nível com |P - 0,70| mínimo.
// Distâncias que diferem menos que 1e-6 contam como empate (como em politica.py):
// prefere o nível inicial; entre dois níveis diferentes do inicial, o mais baixo.
inline uint8_t escolher_nivel(const uint8_t* niveis, int n, const float* previsoes, uint8_t nivel_inicial) {
  uint8_t melhor = niveis[0];
  long long melhor_chave[3] = {0, 0, 0};
  for (int i = 0; i < n; ++i) {
    const uint8_t nivel = niveis[i];
    const long long chave[3] = {std::llround(std::fabs((double)previsoes[nivel] - ALVO_P_ACERTO) / TOLERANCIA_EMPATE),
                                nivel == nivel_inicial ? 0LL : 1LL, (long long)nivel};
    bool menor = i == 0;
    for (int k = 0; k < 3 && !menor; ++k) {
      if (chave[k] != melhor_chave[k]) {
        menor = chave[k] < melhor_chave[k];
        break;
      }
    }
    if (menor) {
      melhor = nivel;
      for (int k = 0; k < 3; ++k) melhor_chave[k] = chave[k];
    }
  }
  return melhor;
}

// Função que devolve P(acerto) para (nível, categoria); nullptr = sem modelo.
typedef float (*FuncaoPrever)(void* contexto, uint8_t nivel, uint8_t categoria);

// Decide categoria e nível da pergunta `ordem` (referência: politica.decidir_pergunta).
// `categorias_restantes`: ordem sorteada, sem as categorias já usadas na partida.
inline Decisao decidir_pergunta(int ordem, uint8_t nivel_inicial, const uint8_t* categorias_restantes,
                                int n_restantes, FuncaoPrever prever, void* contexto) {
  Decisao d;
  d.ordem = (uint8_t)ordem;
  d.nivel_inicial = nivel_inicial;
  if (n_restantes <= 0) return d;
  d.categoria_sorteada = categorias_restantes[0];
  d.n_permitidos = (uint8_t)niveis_permitidos(ordem, nivel_inicial, d.permitidos);

  for (int c = 0; c < n_restantes && !d.viavel; ++c) {
    d.n_disponiveis = 0;
    for (int i = 0; i < d.n_permitidos; ++i)
      if (tem_pergunta(d.permitidos[i], categorias_restantes[c])) d.disponiveis[d.n_disponiveis++] = d.permitidos[i];
    if (d.n_disponiveis > 0) {
      d.viavel = true;
      d.categoria = categorias_restantes[c];
    }
  }
  if (!d.viavel) return d;

  bool todas_validas = true;
  if (prever != nullptr) {
    for (int i = 0; i < d.n_disponiveis; ++i) {
      const uint8_t nivel = d.disponiveis[i];
      d.previsoes[nivel] = prever(contexto, nivel, d.categoria);
      d.avaliado[nivel] = true;
      if (!previsao_valida(d.previsoes[nivel])) todas_validas = false;
    }
  }

  if (ordem == 0) {
    d.nivel = nivel_inicial;
    d.motivo = MOTIVO_PERGUNTA_1;
  } else if (prever == nullptr) {
    d.nivel = mais_proximo_do_inicial(d.disponiveis, d.n_disponiveis, nivel_inicial);
    d.motivo = MOTIVO_SEM_MODELO;
  } else if (!todas_validas) {
    d.nivel = mais_proximo_do_inicial(d.disponiveis, d.n_disponiveis, nivel_inicial);
    d.motivo = MOTIVO_PREVISAO_INVALIDA;
  } else {
    d.nivel = escolher_nivel(d.disponiveis, d.n_disponiveis, d.previsoes, nivel_inicial);
    d.motivo = MOTIVO_GUARDIA;
  }
  const float p = d.avaliado[d.nivel] ? d.previsoes[d.nivel] : SEM_PREVISAO;
  d.p_previsto = previsao_valida(p) ? p : SEM_PREVISAO;
  return d;
}

// Todas as 72 combinações (3 níveis iniciais × 24 ordens de categorias) são viáveis?
inline bool banco_viavel() {
  uint8_t ordem_cat[N_CATEGORIAS] = {0, 1, 2, 3};
  // Percorre as 24 permutações em ordem lexicográfica (algoritmo clássico).
  for (;;) {
    for (uint8_t ini = 0; ini < N_NIVEIS; ++ini) {
      uint8_t restantes[N_CATEGORIAS];
      int n = N_CATEGORIAS;
      for (int k = 0; k < n; ++k) restantes[k] = ordem_cat[k];
      for (int q = 0; q < N_PERGUNTAS; ++q) {
        const Decisao d = decidir_pergunta(q, ini, restantes, n, nullptr, nullptr);
        if (!d.viavel) return false;
        int w = 0;
        for (int k = 0; k < n; ++k)
          if (restantes[k] != d.categoria) restantes[w++] = restantes[k];
        n = w;
      }
    }
    int i = N_CATEGORIAS - 2;
    while (i >= 0 && ordem_cat[i] >= ordem_cat[i + 1]) --i;
    if (i < 0) return true;
    int j = N_CATEGORIAS - 1;
    while (ordem_cat[j] <= ordem_cat[i]) --j;
    uint8_t t = ordem_cat[i]; ordem_cat[i] = ordem_cat[j]; ordem_cat[j] = t;
    for (int a = i + 1, b = N_CATEGORIAS - 1; a < b; ++a, --b) {
      t = ordem_cat[a]; ordem_cat[a] = ordem_cat[b]; ordem_cat[b] = t;
    }
  }
}

// ---------------------------------------------------------------------------
// Sorteio da ordem das categorias (Fisher-Yates) com gerador fornecido
// ---------------------------------------------------------------------------
typedef uint32_t (*FuncaoAleatorio)();

inline void sortear_categorias(uint8_t ordem[N_CATEGORIAS], FuncaoAleatorio aleatorio) {
  for (int i = 0; i < N_CATEGORIAS; ++i) ordem[i] = (uint8_t)i;
  for (int i = N_CATEGORIAS - 1; i > 0; --i) {
    const int j = (int)(aleatorio() % (uint32_t)(i + 1));
    const uint8_t t = ordem[i]; ordem[i] = ordem[j]; ordem[j] = t;
  }
}

// ---------------------------------------------------------------------------
// Rodízio dentro da célula: entre partidas, avança para a próxima pergunta da
// célula; dentro da partida, nunca repete uma pergunta já apresentada.
// O estado fica na RAM (volta ao início quando a placa reinicia).
// ---------------------------------------------------------------------------
struct Rodizio {
  uint8_t proximo[N_NIVEIS][N_CATEGORIAS];
};
inline void rodizio_limpar(Rodizio& r) {
  for (int n = 0; n < N_NIVEIS; ++n)
    for (int c = 0; c < N_CATEGORIAS; ++c) r.proximo[n][c] = 0;
}

// Devolve o índice em BANCO, ou -1 se a célula não tiver pergunta ainda não usada.
inline int escolher_pergunta(Rodizio& r, uint8_t nivel, uint8_t categoria, const bool usada[COFLUME_BANCO_N]) {
  int celula[COFLUME_MAX_POR_CELULA];
  int n = 0;
  for (int i = 0; i < COFLUME_BANCO_N && n < COFLUME_MAX_POR_CELULA; ++i)
    if (BANCO[i].nivel == nivel && BANCO[i].categoria == categoria) celula[n++] = i;
  if (n == 0) return -1;
  for (int k = 0; k < n; ++k) {
    const int pos = (r.proximo[nivel][categoria] + k) % n;
    if (!usada[celula[pos]]) {
      r.proximo[nivel][categoria] = (uint8_t)((pos + 1) % n);
      return celula[pos];
    }
  }
  return -1;
}

// Regra do cofre (inalterada desde a v0.7): pelo menos 2 acertos em 3.
inline bool desbloqueia(int acertos) { return acertos >= ACERTOS_PARA_DESBLOQUEIO; }

}  // namespace coflume
