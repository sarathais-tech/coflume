// =============================================================================
//  coflume_banco.h — banco de perguntas da v0.8 (Coflume — O Conhecimento Abre Portas)
// ARQUIVO GERADO por ferramentas/gerar_banco.py — NÃO EDITAR À MÃO.
//  Origem: questoes/banco_v08.csv do coflume-guardian-ai (ver ORIGEM_GUARDIA.json).
//  Textos convertidos para a fonte clássica da Adafruit GFX (cp437).
// =============================================================================
#pragma once
#include <stdint.h>

#define COFLUME_BANCO_VERSAO "banco_v08"
#define COFLUME_BANCO_CSV_SHA256 "5e9c14e22ae5dd1d4af3f494038a4bb1e8ba41c6839b7847513fc80134d06daa"
#define COFLUME_BANCO_N 13
#define COFLUME_MAX_POR_CELULA 8

namespace coflume {

struct Pergunta {
  const char* id;
  uint8_t nivel;      // 0 Explorador, 1 Desafiante, 2 Mestre
  uint8_t categoria;  // 0 Matematica, 1 Portugues, 2 Logica, 3 Conhecimentos gerais
  const char* enunciado;
  const char* alt[4];
  char correta;       // 'A'..'D'
  const char* dica;
};

const Pergunta BANCO[COFLUME_BANCO_N] = {
  {"V07-EXP-1", 0, 0, "Quanto e 6 x 7?",
   {"36", "42", "48", "40"}, 'B', "6 grupos de 7"},
  {"V07-EXP-3-R1", 0, 1, "Qual palavra esta correta?",
   {"Caza", "Casa", "Kasa", "Casza"}, 'B', "S entre vogais tem som de Z"},
  {"V08-EXP-LOG-1", 0, 2, "Continue: 1,3,5,7",
   {"8", "9", "10", "11"}, 'B', "Pule de 2 em 2"},
  {"V07-EXP-2", 0, 3, "Qual e um planeta?",
   {"Lua", "Sol", "Marte", "Nuvem"}, 'C', "Orbita o Sol"},
  {"V07-DES-1", 1, 0, "Quanto e 12 x 8?",
   {"86", "94", "96", "108"}, 'C', "10x8 + 2x8"},
  {"V08-DES-POR-1", 1, 1, "Plural de flor?",
   {"Flors", "Flores", "Floris", "Floros"}, 'B', "Se termina em R, use ES"},
  {"V07-DES-3", 1, 2, "Proximo: 2,4,8,16...",
   {"18", "24", "30", "32"}, 'D', "Dobra a cada vez"},
  {"V07-DES-2", 1, 3, "Qual e um mamifero?",
   {"Tubarao", "Golfinho", "Polvo", "Sardinha"}, 'B', "Respira ar"},
  {"V07-MES-1-R1", 2, 0, "Binario 1010 = decimal?",
   {"8", "9", "10", "12"}, 'C', "Use pot" "\x88" "ncias de 2"},
  {"V08-MES-POR-1", 2, 1, "Ontem eles ___ bola.",
   {"jogou", "jogaram", "jogam", "joga"}, 'B', "Eles: plural. Ontem: passado"},
  {"V08-MES-LOG-1", 2, 2, "Continue: 1,1,2,3,5",
   {"6", "7", "8", "9"}, 'C', "Some os dois anteriores"},
  {"V07-MES-2-R1", 2, 3, "Phishing e...",
   {"Backup", "Golpe digital", "Antivirus", "Hardware"}, 'B', "Engana o usuario"},
  {"V07-MES-3", 2, 3, "Qual senha e melhor?",
   {"123456", "senha123", "nome2026", "G7!mP2#x"}, 'D', "Unica e aleatoria"},
};

}  // namespace coflume
