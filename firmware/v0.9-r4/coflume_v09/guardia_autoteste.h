// =============================================================================
//  guardia_autoteste.h — autoteste de inicialização da Guardiã IA (Coflume v0.8)
// ARQUIVO GERADO por ferramentas/gerar_autoteste.py — NÃO EDITAR À MÃO.
//  Subconjunto representativo de testes/vetores_teste.h (400 vetores da
//  bancada aprovada no Wokwi). Critério de escolha: ver o gerador.
// =============================================================================
#pragma once
#include <stdint.h>

#define GUARDIA_AUTOTESTE_MODELO_ID "mlp8-sint-20261101-e6a4cf00"
#define GUARDIA_AUTOTESTE_N 21
#define GUARDIA_AUTOTESTE_HIST_MAX 8

struct GuardiaAutoteste {
  int16_t indice_original;  // posição em testes/vetores_teste.h
  guardia::Resposta historico[GUARDIA_AUTOTESTE_HIST_MAX];
  uint8_t n_historico;
  uint8_t nivel_inicial, categoria, nivel_candidato;
  guardia::Entrada entrada_esperada;
  float p_esperado;
};

const GuardiaAutoteste GUARDIA_AUTOTESTE[GUARDIA_AUTOTESTE_N] = {
  { 0, {{0, 0, false, 0.0f, 1.0f, false}}, 0, 2, 2, 2,
    {2, 2, 0, 2, 0.5f, 0.5f, 0.5f, 0.0f, 0, 0}, 0.604623854f },
  { 2, {{0, 0, false, 0.0f, 1.0f, false}}, 0, 2, 0, 0,
    {0, 2, 0, 0, 0.5f, 0.5f, 0.5f, 0.0f, 0, 0}, 0.864316761f },
  { 4, {{0, 0, false, 0.0f, 1.0f, false}}, 0, 2, 0, 1,
    {1, 2, 0, 0, 0.5f, 0.5f, 0.5f, 0.0f, 0, 0}, 0.724500656f },
  { 40, {{3, 0, true, 22.9678764f, 30.0f, true}, {1, 2, false, 2.32089829f, 20.0f, false}, {2, 0, false, 20.9778481f, 20.0f, false}, {3, 1, false, 20.5098896f, 30.0f, false}}, 4, 0, 0, 0,
    {0, 0, 4, 0, 0.25f, 0.25f, 0.641325951f, 0.25f, -1, -3}, 0.59916693f },
  { 41, {{0, 1, true, 30.7954426f, 40.0f, true}, {0, 2, true, 42.0585022f, 40.0f, false}, {3, 0, true, 0.907087326f, 30.0f, false}, {2, 1, true, 10.2772856f, 20.0f, false}}, 4, 2, 1, 2,
    {2, 2, 4, 1, 1.0f, 1.0f, 0.578496635f, 0.25f, 1, 3}, 0.386787802f },
  { 44, {{2, 2, false, 5.43383789f, 40.0f, false}, {3, 1, true, 2.2393074f, 40.0f, true}, {0, 0, true, 4.50008011f, 20.0f, true}, {2, 1, true, 0.505808532f, 20.0f, false}, {0, 0, true, 31.5852528f, 40.0f, true}, {3, 0, true, 23.0417023f, 20.0f, false}, {0, 2, true, 2.70520782f, 20.0f, false}, {3, 2, true, 4.64896584f, 30.0f, false}}, 8, 2, 1, 1,
    {1, 2, 8, 1, 0.875f, 0.875f, 0.315247536f, 0.375f, 1, 3}, 0.520950615f },
  { 56, {{3, 0, false, 21.6125164f, 30.0f, false}}, 1, 0, 0, 0,
    {0, 0, 1, 0, 0.0f, 0.0f, 0.720417202f, 0.0f, -1, -1}, 0.551534295f },
  { 72, {{2, 1, true, 2.60758972f, 40.0f, false}, {0, 0, false, 12.4021778f, 40.0f, false}}, 2, 0, 1, 2,
    {2, 0, 2, 1, 0.5f, 0.5f, 0.1876221f, 0.0f, -1, -1}, 0.504554033f },
  { 73, {{0, 2, true, 38.7173195f, 40.0f, false}, {3, 1, true, 14.9257994f, 20.0f, true}}, 2, 0, 2, 0,
    {0, 0, 2, 2, 1.0f, 1.0f, 0.857111454f, 0.5f, 1, 2}, 0.599536657f },
  { 78, {{3, 0, true, 2.75020218f, 40.0f, false}}, 1, 1, 2, 1,
    {1, 1, 1, 2, 1.0f, 1.0f, 0.068755053f, 0.0f, 1, 1}, 0.716530025f },
  { 81, {{0, 2, true, 4.960989f, 20.0f, false}}, 1, 1, 3, 2,
    {2, 1, 1, 3, 1.0f, 1.0f, 0.248049453f, 0.0f, 1, 1}, 0.560228825f },
  { 153, {{3, 0, true, 6.6056428f, 20.0f, false}, {2, 0, true, 39.4214821f, 40.0f, false}}, 2, 2, 3, 1,
    {1, 2, 2, 3, 1.0f, 1.0f, 0.657909632f, 0.0f, 1, 2}, 0.705494821f },
  { 200, {{0, 0, false, 0.0f, 1.0f, false}}, 0, 1, 3, 1,
    {1, 1, 0, 3, 0.5f, 0.5f, 0.5f, 0.0f, 0, 0}, 0.617097616f },
  { 201, {{3, 1, false, 9.71000004f, 30.0f, true}}, 1, 1, 0, 2,
    {2, 1, 1, 0, 0.0f, 0.0f, 0.323666662f, 1.0f, -1, -1}, 0.48820889f },
  { 202, {{1, 1, true, 6.13000011f, 30.0f, false}, {3, 1, false, 14.1899996f, 30.0f, false}}, 2, 1, 0, 1,
    {1, 1, 2, 0, 0.5f, 0.5f, 0.338666677f, 0.0f, -1, -1}, 0.617715061f },
  { 203, {{0, 0, false, 0.0f, 1.0f, false}}, 0, 2, 0, 2,
    {2, 2, 0, 0, 0.5f, 0.5f, 0.5f, 0.0f, 0, 0}, 0.561109781f },
  { 204, {{0, 0, false, 10.7299995f, 30.0f, false}}, 1, 0, 2, 0,
    {0, 0, 1, 2, 0.0f, 0.0f, 0.357666641f, 0.0f, -1, -1}, 0.691329479f },
  { 205, {{1, 2, false, 19.0900002f, 30.0f, true}, {0, 1, false, 5.59000015f, 30.0f, false}}, 2, 2, 3, 2,
    {2, 2, 2, 3, 0.0f, 0.0f, 0.411333352f, 0.5f, -1, -2}, 0.480893821f },
  { 206, {{0, 0, false, 0.0f, 1.0f, false}}, 0, 0, 3, 0,
    {0, 0, 0, 3, 0.5f, 0.5f, 0.5f, 0.0f, 0, 0}, 0.680574596f },
  { 207, {{0, 0, true, 18.4300003f, 30.0f, false}}, 1, 0, 1, 1,
    {1, 0, 1, 1, 1.0f, 1.0f, 0.614333332f, 0.0f, 1, 1}, 0.447129428f },
  { 208, {{0, 0, true, 5.25f, 30.0f, false}, {1, 1, false, 11.5900002f, 30.0f, false}}, 2, 0, 2, 0,
    {0, 0, 2, 2, 0.5f, 0.5f, 0.280666679f, 0.0f, -1, -1}, 0.743002355f },
};
