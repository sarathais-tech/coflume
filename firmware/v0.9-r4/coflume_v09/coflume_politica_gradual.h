// =============================================================================
//  coflume_politica_gradual.h — política adaptativa GRADUAL (EXPERIMENTO gradual-v2-exp)
//
//  Porta em C++ de guardia/politica_gradual.py (coflume-guardian-ai, branch local
//  exp/politica-gradual). NÃO está integrada ao firmware v0.9 r2, que continua
//  com o fluxo-0.8 (coflume_politica.h).
//
//  É o fluxo-0.8 com uma proteção nas perguntas 2 e 3. Nível COMPATÍVEL:
//    1. permitido no fluxo-0.8 (nível inicial ± 1, limitado a 0..2);
//    2. no máximo 1 nível de diferença da pergunta anterior;
//    3. depois de erro ou tempo esgotado, não maior que o nível anterior.
//  Categoria: a primeira da ordem sorteada que tenha pergunta em nível compatível.
//  Sem nenhuma: decisão recusada (viavel=false, "sem_nivel_compativel"), nunca
//  uma escolha que viole a regra. banco_viavel() detecta isso antes da partida.
//
//  Lógica pura: sem Arduino, sem alocação dinâmica. A Guardiã só estima P(acerto).
// =============================================================================
#pragma once
#include <stdint.h>
#include <stdlib.h>

#include "coflume_politica.h"

namespace coflume {
namespace gradual {

constexpr const char* VERSAO = "gradual-v2-exp";
constexpr int MAX_MUDANCA = 1;

// (nível, categoria) -> há pergunta no banco? O firmware usa tem_pergunta_banco.
typedef bool (*FuncaoTemPergunta)(void* contexto, uint8_t nivel, uint8_t categoria);
inline bool tem_pergunta_banco(void*, uint8_t nivel, uint8_t categoria) { return coflume::tem_pergunta(nivel, categoria); }

struct Anterior {
  bool existe = false;
  uint8_t nivel = 0;
  bool acertou = false;  // false também para tempo esgotado
};

struct Decisao {
  bool viavel = false;  // false: nenhuma categoria restante com nível compatível
  uint8_t ordem = 0, nivel_inicial = 0, categoria_sorteada = 0, categoria = 0;
  uint8_t permitidos[N_NIVEIS] = {0, 0, 0}, compativeis[N_NIVEIS] = {0, 0, 0};
  uint8_t disponiveis[N_NIVEIS] = {0, 0, 0}, candidatos[N_NIVEIS] = {0, 0, 0};
  uint8_t n_permitidos = 0, n_compativeis = 0, n_disponiveis = 0, n_candidatos = 0;
  float previsoes[N_NIVEIS] = {SEM_PREVISAO, SEM_PREVISAO, SEM_PREVISAO};  // de todos os disponíveis (log)
  bool avaliado[N_NIVEIS] = {false, false, false};
  uint8_t nivel = 0;
  float p_previsto = SEM_PREVISAO;
  Motivo motivo = MOTIVO_GUARDIA;
  int8_t nivel_sem_regra = -1;    // escolha do fluxo-0.8 NESTA categoria (só com previsões válidas)
  int8_t categoria_fluxo08 = -1;  // categoria que o fluxo-0.8 escolheria

  bool limitou() const { return nivel_sem_regra >= 0 && nivel_sem_regra != (int8_t)nivel; }
  bool categoria_substituida() const { return categoria != categoria_sorteada; }
  const char* nome_motivo() const { return viavel ? coflume::nome_motivo(motivo) : "sem_nivel_compativel"; }
};

// Permitidos do fluxo-0.8 que respeitam a mudança gradual. Devolve -1 se o
// anterior faltar ou estiver fora dos permitidos (entrada inválida).
inline int niveis_compativeis(int ordem, uint8_t nivel_inicial, const Anterior& a, uint8_t saida[N_NIVEIS]) {
  uint8_t perm[N_NIVEIS];
  const int np = niveis_permitidos(ordem, nivel_inicial, perm);
  if (ordem == 0) {
    for (int i = 0; i < np; ++i) saida[i] = perm[i];
    return np;
  }
  bool anterior_ok = false;
  for (int i = 0; i < np; ++i) anterior_ok |= a.existe && perm[i] == a.nivel;
  if (!anterior_ok) return -1;
  int n = 0;
  for (int i = 0; i < np; ++i) {
    const int d = (int)perm[i] - (int)a.nivel;
    if (abs(d) <= MAX_MUDANCA && (a.acertou || perm[i] <= a.nivel)) saida[n++] = perm[i];
  }
  return n;
}

// Fallback: candidato mais próximo da referência; empate -> o mais baixo.
inline uint8_t mais_proximo(const uint8_t* niveis, int n, uint8_t referencia) {
  uint8_t melhor = niveis[0];
  int melhor_d = 99;
  for (int i = 0; i < n; ++i) {
    const int d = abs((int)niveis[i] - (int)referencia);
    if (d < melhor_d || (d == melhor_d && niveis[i] < melhor)) { melhor = niveis[i]; melhor_d = d; }
  }
  return melhor;
}

// Referência: politica_gradual.decidir_pergunta_gradual.
inline Decisao decidir_pergunta(int ordem, uint8_t nivel_inicial, const uint8_t* categorias_restantes, int n_restantes,
                                FuncaoTemPergunta tem, void* contexto_banco, FuncaoPrever prever,
                                void* contexto_prever, const Anterior& anterior) {
  Decisao d;
  d.ordem = (uint8_t)ordem;
  d.nivel_inicial = nivel_inicial;
  if (n_restantes <= 0) return d;
  d.categoria_sorteada = categorias_restantes[0];
  d.n_permitidos = (uint8_t)niveis_permitidos(ordem, nivel_inicial, d.permitidos);
  const int nc = niveis_compativeis(ordem, nivel_inicial, anterior, d.compativeis);
  if (nc < 0) return d;  // entrada inválida: recusa
  d.n_compativeis = (uint8_t)nc;

  for (int c = 0; c < n_restantes && d.categoria_fluxo08 < 0; ++c)
    for (int i = 0; i < d.n_permitidos; ++i)
      if (tem(contexto_banco, d.permitidos[i], categorias_restantes[c])) { d.categoria_fluxo08 = (int8_t)categorias_restantes[c]; break; }

  for (int c = 0; c < n_restantes && !d.viavel; ++c) {
    d.n_candidatos = 0;
    for (int i = 0; i < d.n_compativeis; ++i)
      if (tem(contexto_banco, d.compativeis[i], categorias_restantes[c])) d.candidatos[d.n_candidatos++] = d.compativeis[i];
    if (d.n_candidatos > 0) { d.viavel = true; d.categoria = categorias_restantes[c]; }
  }
  if (!d.viavel) return d;  // sem_nivel_compativel: o jogo NÃO escolhe nível fora da regra

  for (int i = 0; i < d.n_permitidos; ++i)
    if (tem(contexto_banco, d.permitidos[i], d.categoria)) d.disponiveis[d.n_disponiveis++] = d.permitidos[i];
  if (prever != nullptr)
    for (int i = 0; i < d.n_disponiveis; ++i) {
      const uint8_t n = d.disponiveis[i];
      d.previsoes[n] = prever(contexto_prever, n, d.categoria);
      d.avaliado[n] = true;
    }
  bool candidatos_validos = true, disponiveis_validos = true;
  for (int i = 0; i < d.n_candidatos; ++i) candidatos_validos &= d.avaliado[d.candidatos[i]] && previsao_valida(d.previsoes[d.candidatos[i]]);
  for (int i = 0; i < d.n_disponiveis; ++i) disponiveis_validos &= d.avaliado[d.disponiveis[i]] && previsao_valida(d.previsoes[d.disponiveis[i]]);

  if (ordem == 0) {
    d.nivel = nivel_inicial;
    d.motivo = MOTIVO_PERGUNTA_1;
  } else if (prever == nullptr) {
    d.nivel = mais_proximo(d.candidatos, d.n_candidatos, anterior.nivel);
    d.motivo = MOTIVO_SEM_MODELO;
  } else if (!candidatos_validos) {
    d.nivel = mais_proximo(d.candidatos, d.n_candidatos, anterior.nivel);
    d.motivo = MOTIVO_PREVISAO_INVALIDA;
  } else {
    d.nivel = escolher_nivel(d.candidatos, d.n_candidatos, d.previsoes, nivel_inicial);
    d.motivo = MOTIVO_GUARDIA;
  }
  if (ordem > 0 && prever != nullptr && d.n_disponiveis > 0 && disponiveis_validos)
    d.nivel_sem_regra = (int8_t)escolher_nivel(d.disponiveis, d.n_disponiveis, d.previsoes, nivel_inicial);
  const float p = d.avaliado[d.nivel] ? d.previsoes[d.nivel] : SEM_PREVISAO;
  d.p_previsto = previsao_valida(p) ? p : SEM_PREVISAO;
  return d;
}

// Referência: politica_gradual.problemas_de_viabilidade_gradual. Verdadeiro se
// NENHUMA partida pode chegar a "sem_nivel_compativel": 3 níveis iniciais × 24
// ordens × acerto/erro nas perguntas 1 e 2 × qualquer nível possível na pergunta 2.
inline bool banco_viavel(FuncaoTemPergunta tem, void* contexto) {
  uint8_t o[N_CATEGORIAS] = {0, 1, 2, 3};
  for (;;) {
    for (uint8_t ini = 0; ini < N_NIVEIS; ++ini) {
      const Anterior nenhum;
      const Decisao d1 = decidir_pergunta(0, ini, o, N_CATEGORIAS, tem, contexto, nullptr, nullptr, nenhum);
      if (!d1.viavel) return false;
      uint8_t r1[N_CATEGORIAS];
      int n1 = 0;
      for (int k = 0; k < N_CATEGORIAS; ++k) if (o[k] != d1.categoria) r1[n1++] = o[k];
      for (int ac1 = 0; ac1 < 2; ++ac1) {
        Anterior a1; a1.existe = true; a1.nivel = d1.nivel; a1.acertou = ac1 == 1;
        const Decisao d2 = decidir_pergunta(1, ini, r1, n1, tem, contexto, nullptr, nullptr, a1);
        if (!d2.viavel) return false;
        uint8_t r2[N_CATEGORIAS];
        int n2 = 0;
        for (int k = 0; k < n1; ++k) if (r1[k] != d2.categoria) r2[n2++] = r1[k];
        for (int i = 0; i < d2.n_candidatos; ++i)
          for (int ac2 = 0; ac2 < 2; ++ac2) {
            Anterior a2; a2.existe = true; a2.nivel = d2.candidatos[i]; a2.acertou = ac2 == 1;
            if (!decidir_pergunta(2, ini, r2, n2, tem, contexto, nullptr, nullptr, a2).viavel) return false;
          }
      }
    }
    int i = N_CATEGORIAS - 2;
    while (i >= 0 && o[i] >= o[i + 1]) --i;
    if (i < 0) return true;
    int j = N_CATEGORIAS - 1;
    while (o[j] <= o[i]) --j;
    uint8_t t = o[i]; o[i] = o[j]; o[j] = t;
    for (int a = i + 1, b = N_CATEGORIAS - 1; a < b; ++a, --b) { t = o[a]; o[a] = o[b]; o[b] = t; }
  }
}

}  // namespace gradual
}  // namespace coflume
