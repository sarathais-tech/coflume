// Gera logs de DEMONSTRAÇÃO para o painel: o firmware r3 REAL roda no PC, com
// imitações de teclado, tela, servo e porta serial (stubs da r3). As respostas
// são roteirizadas aqui; nenhum dado vem de crianças.
// Compilar com -DGUARDIA_POLITICA_GRADUAL=0 ou 1. Uso: gerar_demo <saida.txt> <roteiro>
//   roteiro "fluxo08": quiz Desafiante (acerto, erro com dica, tempo esgotado),
//                      missão do Agente Secreto e quiz Explorador com 3 acertos.
//   roteiro "gradual": procura uma partida em que a proteção gradual atua e a grava.
//   roteiro "recusa":  exige uma CÓPIA do sketch com banco inviável para a regra gradual
//                      (-DCOFLUME_SKETCH='"<copia>/coflume_v09.ino"'). Parte 1: a verificação
//                      inicial bloqueia o Quiz. Parte 2 (CENÁRIO FORÇADO): o gerador define
//                      bancoGradualOK=true para passar pela verificação inicial e exercitar a
//                      proteção defensiva em jogo (recusa sem_nivel_compativel).
#ifndef COFLUME_SKETCH
#define COFLUME_SKETCH "../../coflume_v09/coflume_v09.ino"
#endif
#include COFLUME_SKETCH

#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

static std::vector<std::string> saida;
static size_t lido = 0;
static void carimbar() {  // linhas novas do serial, com o tempo simulado atual
  const std::string& s = stub::serial();
  size_t fim;
  while ((fim = s.find('\n', lido)) != std::string::npos) {
    const std::string linha = s.substr(lido, fim - lido);
    lido = fim + 1;
    const uint64_t ms = 1794132000000ULL + stub::agora_ms();  // 2026-11-08T10:00:00Z + tempo simulado
    const time_t seg = (time_t)(ms / 1000);
    char iso[40];
    struct tm t;
    gmtime_r(&seg, &t);
    std::snprintf(iso, sizeof(iso), "%04d-%02d-%02dT%02d:%02d:%02d.%03uZ", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                  t.tm_hour, t.tm_min, t.tm_sec, (unsigned)(ms % 1000));
    saida.push_back(std::string(iso) + " " + linha);
  }
}
static void avancar(uint32_t ms) { for (uint32_t t = 0; t < ms; t += 10) { stub::agora_ms() += 10; loop(); } carimbar(); }
static void apertar(char k) { stub::teclas().push_back(k); loop(); carimbar(); }
static void painel(const std::string& l) { for (char c : l) stub::entrada().push_back(c); stub::entrada().push_back('\n'); loop(); carimbar(); }
static void nota(const std::string& texto) { saida.push_back("# " + texto); }  // nota no meio do log
static std::string sid() { char b[12]; std::snprintf(b, sizeof(b), "%08lX", (unsigned long)agente::id); return b; }
enum R { CERTA, ERRADA, ESGOTA };
static void responder(R r, bool dica, uint32_t espera) {
  if (r == ESGOTA) { avancar(TEMPO_QUESTAO_MS + 20); }
  else {
    avancar(espera);
    if (dica) apertar('*');
    const char c = questao().correta;
    apertar(r == CERTA ? c : (c == 'A' ? 'B' : 'A'));
    apertar('#');
  }
  avancar(FEEDBACK_ERRO_MS + 100);
}
static void missao() {
  apertar('A');
  painel("AG,READY," + sid());
  for (int i = 0; i < 12; ++i) { avancar(1000); painel("AG,PING," + sid()); }
  for (int i = 0; i < 4; ++i) apertar(agente::senha[i] == '9' ? '0' : (char)(agente::senha[i] + 1));
  apertar('#');
  for (int i = 0; i < 6; ++i) { avancar(1000); painel("AG,PING," + sid()); }
  for (int i = 0; i < 4; ++i) apertar(agente::senha[i]);
  apertar('#');
  avancar(3000);
  apertar('#');
}

int main(int argc, char** argv) {
  const std::string roteiro = argc > 2 ? argv[2] : "fluxo08";
  setup();
  avancar(20000);  // o painel conecta depois do boot
  painel("AG,HELLO");
  if (roteiro == "fluxo08") {
    avancar(4000);
    apertar('2');
    responder(CERTA, false, 5200);
    responder(ERRADA, true, 11800);
    responder(ESGOTA, false, 0);
    apertar('#');
    avancar(6000);
    missao();
    avancar(5000);
    apertar('1');
    responder(CERTA, false, 3400);
    responder(CERTA, true, 8600);
    responder(CERTA, false, 4100);
    apertar('#');
  } else if (roteiro == "massa") {
    // DADOS DE TESTE (não é demonstração): 150 partidas variadas, para conferir a reprodução da rede.
    const R pad[][3] = {{CERTA, CERTA, CERTA}, {ERRADA, CERTA, ERRADA}, {CERTA, ESGOTA, CERTA}, {ERRADA, ERRADA, CERTA},
                        {CERTA, ERRADA, ESGOTA}, {ESGOTA, CERTA, CERTA}};
    for (int t = 0; t < 150; ++t) {
      apertar((char)('1' + t % 3));
      const R* pa = pad[(t / 3) % 6];
      for (int k = 0; k < 3 && estado != ERRO_BANCO; ++k)
        responder(pa[k], (t + k) % 4 == 0, 1200 + (uint32_t)((t * 3797 + k * 911) % 26000));
      apertar('#');
      if (t % 40 == 0) { avancar(2000); missao(); avancar(3000); }
    }
  } else if (roteiro == "recusa") {
    // Parte 1 - comportamento normal: banco inviável para a regra gradual bloqueia o Quiz.
    if (bancoGradualOK) { std::fprintf(stderr, "o banco desta copia e viavel; use um banco com lacuna\n"); return 1; }
    avancar(4000);
    apertar('2');
    avancar(4000);
    apertar('#');
    avancar(4000);
    // Parte 2 - CENÁRIO FORÇADO: contorna a verificação inicial para testar a proteção defensiva.
    nota("CENARIO FORCADO a partir daqui: o gerador definiu bancoGradualOK=true para passar pela verificacao inicial "
         "e testar a protecao defensiva em jogo. Em uso normal o Quiz continuaria bloqueado (BANCO INCOMPLETO).");
    bancoGradualOK = true;
    const R padroes[][3] = {{ERRADA, ERRADA, CERTA}, {CERTA, ERRADA, CERTA}, {ERRADA, CERTA, CERTA}, {CERTA, CERTA, CERTA}};
    bool achou = false;
    for (int tentativa = 0; tentativa < 900 && !achou; ++tentativa) {
      const size_t marca = saida.size();
      apertar((char)('1' + tentativa % 3));
      const R* pa = padroes[(tentativa / 3) % 4];
      bool recusou = estado == ERRO_BANCO;
      for (int k = 0; k < 3 && !recusou; ++k) {
        responder(pa[k], false, 2500 + 700 * k);
        recusou = estado == ERRO_BANCO;
      }
      if (recusou) {
        achou = true;
        std::fprintf(stderr, "recusa defensiva: tentativa %d\n", tentativa);
        avancar(4000);
      } else {
        saida.resize(marca);  // descarta partidas sem recusa
      }
      apertar('#');
    }
    if (!achou) { std::fprintf(stderr, "nenhuma recusa encontrada\n"); return 1; }
  } else {
    // Procura, sem gravar, uma partida em que a regra gradual muda a escolha da MLP.
    const R padroes[][3] = {{CERTA, ERRADA, CERTA}, {ERRADA, CERTA, CERTA}, {CERTA, CERTA, CERTA}, {ERRADA, ERRADA, CERTA}};
    const uint32_t tempos[] = {2000, 2600, 3000, 9000};
    bool achou = false;
    for (int tentativa = 0; tentativa < 600 && !achou; ++tentativa) {
      const size_t marca = saida.size();
      const int ini = tentativa % 3;
      const R* p = padroes[(tentativa / 3) % 4];
      apertar((char)('1' + ini));
      bool limitou = false;
      for (int k = 0; k < 3; ++k) {
        limitou |= decisaoGradual.limitou();
        responder(p[k], false, tempos[(tentativa / 12 + k) % 4]);
      }
      apertar('#');
      if (limitou) { achou = true; std::fprintf(stderr, "partida com protecao gradual: tentativa %d\n", tentativa); }
      else saida.resize(marca);  // descarta partidas sem intervenção
    }
    if (!achou) { std::fprintf(stderr, "nenhuma partida com intervencao\n"); return 1; }
    avancar(5000);
    missao();
  }
  std::ofstream f(argv[1]);
  f << "# DADOS DE DEMONSTRACAO - gerados pelo firmware " << COFLUME_VERSAO << ", banco " << COFLUME_BANCO_VERSAO << " (GUARDIA_POLITICA_GRADUAL=" << GUARDIA_POLITICA_GRADUAL
    << ") rodando em SIMULACAO no PC.\n";
  if (roteiro == "recusa")
    f << "# BANCO INVIAVEL PARA A REGRA GRADUAL: " << COFLUME_BANCO_VERSAO << " sem as celulas Desafiante x Portugues e Desafiante x Conhecimentos gerais.\n"
         "# Parte 1 - uso normal: a verificacao inicial detecta o banco inviavel e bloqueia o Quiz (BANCO INCOMPLETO).\n"
         "# Parte 2 - CENARIO FORCADO PARA TESTE: o gerador define bancoGradualOK=true para passar pela verificacao inicial e "
         "testar a protecao defensiva (recusa sem_nivel_compativel). Nao acontece em uso normal.\n";
  if (roteiro == "massa") f << "# DADOS DE TESTE (nao e demonstracao): 150 partidas variadas para conferir a reproducao da rede no painel.\n";
  f << "# Respostas roteirizadas; nao sao dados de criancas nem do cofre fisico. Modelo experimental (dados sinteticos).\n";
  for (const auto& l : saida) f << l << "\n";
  std::fprintf(stderr, "%zu linhas\n", saida.size());
  return 0;
}
