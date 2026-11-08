// =============================================================================
//  coflume_tela.h — layout da tela ST7735 1,8" (160×128 em paisagem) · Coflume v0.9
//
//  Lógica pura de layout (sem Arduino): medidas da fonte clássica da Adafruit GFX
//  e quebra de linha POR PALAVRA. A GFX só sabe quebrar por caractere, e o banco
//  de perguntas foi validado com quebra por palavra (coflume-guardian-ai,
//  docs/interface_st7735.md: paisagem, corpo em tamanho 2, moldura em tamanho 1).
// =============================================================================
#pragma once
#include <stdint.h>
#include <string.h>

namespace tela {

constexpr int LARGURA = 160;  // paisagem (setRotation 1 ou 3)
constexpr int ALTURA = 128;
constexpr int CHAR_W = 6;     // célula da fonte clássica em tamanho 1 (5 px + 1 de espaço)
constexpr int CHAR_H = 8;
constexpr int MARGEM_X = 2;   // folga lateral

inline int colunas(int tamanho) { return (LARGURA - MARGEM_X) / (CHAR_W * tamanho); }  // 26 (t1) ou 13 (t2)
inline int largura_texto(const char* s, int tamanho) { return (int)strlen(s) * CHAR_W * tamanho; }

// Regiões da tela de pergunta (y em pixels).
constexpr int Y_CABECALHO = 0;    // nível, progresso e tempo (tamanho 1)
constexpr int Y_BARRA = 9;        // barra do tempo
constexpr int ALTURA_BARRA = 5;
constexpr int Y_CORPO = 16;       // enunciado e alternativas (tamanho 2, 16 px por linha)
constexpr int Y_RODAPE = 112;     // 16 px: lembrete das teclas ou a dica (tamanho 1, até 2 linhas)
constexpr int LINHAS_RODAPE = 2;

constexpr int MAX_LINHAS = 8;
constexpr int MAX_COLUNAS = 27;

struct Linhas {
  char texto[MAX_LINHAS][MAX_COLUNAS + 1];
  int n = 0;
  bool coube = true;  // false se precisou de mais de MAX_LINHAS
};

// Quebra `s` em linhas de até `cols` caracteres, por palavra; palavras maiores
// que `cols` são cortadas. `recuo` espaços no início das linhas de continuação
// (alinha com o texto depois de "A) ").
inline Linhas quebrar(const char* s, int cols, int recuo = 0) {
  Linhas L;
  if (cols > MAX_COLUNAS) cols = MAX_COLUNAS;
  char atual[MAX_COLUNAS + 1] = "";
  int n_atual = 0;
  auto fechar = [&]() {
    if (L.n >= MAX_LINHAS) { L.coube = false; return; }
    memcpy(L.texto[L.n], atual, (size_t)n_atual);
    L.texto[L.n][n_atual] = '\0';
    ++L.n;
    n_atual = 0;
    for (int k = 0; k < recuo && k < cols - 1; ++k) atual[n_atual++] = ' ';
  };
  const char* p = s;
  while (*p) {
    while (*p == ' ') ++p;
    if (!*p) break;
    const char* fim = p;
    while (*fim && *fim != ' ') ++fim;
    int tam = (int)(fim - p);
    const bool comeco = n_atual == 0 || (L.n > 0 && n_atual == recuo);
    const int precisa = comeco ? tam : tam + 1;
    if (!comeco && n_atual + precisa > cols) fechar();
    const bool comeco2 = n_atual == 0 || (L.n > 0 && n_atual == recuo);
    if (!comeco2) atual[n_atual++] = ' ';
    while (tam > 0) {
      const int cabe = cols - n_atual;
      const int usa = tam < cabe ? tam : cabe;
      memcpy(atual + n_atual, p, (size_t)usa);
      n_atual += usa;
      p += usa;
      tam -= usa;
      if (tam > 0) fechar();  // palavra maior que a linha: corta
    }
    p = fim;
  }
  if (n_atual > 0 && !(L.n > 0 && n_atual == recuo)) fechar();
  return L;
}

}  // namespace tela
