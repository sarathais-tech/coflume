// Tela ILI9341 simulada: registra cada texto com posição, tamanho e cor.
// Cada chamada consome stub::passo_por_desenho_ms() de tempo simulado.
#pragma once
#include <string>
#include <vector>
#include "Arduino.h"
#define ILI9341_BLACK 0x0000
#define ILI9341_WHITE 0xFFFF
#define ILI9341_CYAN 0x07FF
#define ILI9341_YELLOW 0xFFE0
#define ILI9341_RED 0xF800
#define ILI9341_GREEN 0x07E0
namespace stub {
struct Texto { int x, y, tamanho; uint16_t cor; std::string texto; };
inline std::vector<Texto>& tela() { static std::vector<Texto> t; return t; }
inline int& telas_desenhadas() { static int n = 0; return n; }
}  // namespace stub
class Adafruit_ILI9341 {
 public:
  Adafruit_ILI9341(int, int) {}
  void begin() { gasto(); }
  void setRotation(int) {}
  void fillScreen(uint16_t) { stub::tela().clear(); ++stub::telas_desenhadas(); gasto(); }
  void setTextWrap(bool) {}
  void setTextSize(int t) { tamanho_ = t; }
  void setTextColor(uint16_t c) { cor_ = c; }
  void setCursor(int x, int y) { x_ = x; y_ = y; }
  void drawFastHLine(int, int, int, uint16_t) { gasto(); }
  void fillRect(int, int, int, int, uint16_t) { gasto(); }
  void print(const char* s) { escrever(s); }
  void println(const char* s) { escrever(s); y_ += 8 * tamanho_; x_ = 0; }
  int printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    escrever(buf);
    return n;
  }
  static constexpr int LARGURA = 320, ALTURA = 240;  // rotação 1
 private:
  void gasto() { stub::agora_ms() += stub::passo_por_desenho_ms(); }
  void escrever(const char* s) {
    stub::tela().push_back({x_, y_, tamanho_, cor_, s});
    x_ += 6 * tamanho_ * (int)std::strlen(s);
    gasto();
  }
  int x_ = 0, y_ = 0, tamanho_ = 1;
  uint16_t cor_ = 0xFFFF;
};
