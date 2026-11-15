// Tela ST7735 simulada para os testes no PC.
// Registra cada operação de desenho (para gerar as imagens das telas) e verifica
// o layout: texto dentro de 160x128, sem quebra automática da GFX e sem
// sobreposição com outro texto ou linha ainda visível.
#pragma once
#include <string>
#include <vector>
#include "Arduino.h"
#include "SPI.h"
#define ST77XX_BLACK 0x0000
#define ST77XX_WHITE 0xFFFF
#define ST77XX_RED 0xF800
#define ST77XX_GREEN 0x07E0
#define ST77XX_BLUE 0x001F
#define ST77XX_CYAN 0x07FF
#define ST77XX_MAGENTA 0xF81F
#define ST77XX_YELLOW 0xFFE0
#define INITR_GREENTAB 0x00
#define INITR_REDTAB 0x01
#define INITR_BLACKTAB 0x02
class SPIClass;
namespace stub {
struct Op { char tipo; int x, y, w, h, tamanho; uint16_t cor; std::string texto; };  // 'S' tela, 'R' ret., 'L' linha, 'T' texto
inline std::vector<Op>& ops() { static std::vector<Op> v; return v; }           // desde o último fillScreen
inline std::vector<std::string>& violacoes() { static std::vector<std::string> v; return v; }
inline int& telas_desenhadas() { static int n = 0; return n; }
inline int& painel() { static int p = -1; return p; }
inline bool& wrap() { static bool w = true; return w; }
struct Caixa { int x, y, w, h; std::string desc; };
inline std::vector<Caixa>& visiveis() { static std::vector<Caixa> v; return v; }
inline bool cruza(const Caixa& a, const Caixa& b) {
  return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}
inline bool contem(const Caixa& fora, const Caixa& dentro) {
  return dentro.x >= fora.x && dentro.y >= fora.y && dentro.x + dentro.w <= fora.x + fora.w &&
         dentro.y + dentro.h <= fora.y + fora.h;
}
}  // namespace stub
class Adafruit_ST7735 {
 public:
  Adafruit_ST7735(SPIClass*, int8_t, int8_t, int8_t) {}
  void initR(uint8_t p) { stub::painel() = p; gasto(); }
  void setRotation(uint8_t r) { w_ = (r & 1) ? 160 : 128; h_ = (r & 1) ? 128 : 160; }
  int16_t width() const { return (int16_t)w_; }
  int16_t height() const { return (int16_t)h_; }
  void setTextWrap(bool w) { stub::wrap() = w; }
  void setTextSize(uint8_t t) { tamanho_ = t; }
  void setTextColor(uint16_t c) { cor_ = c; }
  void setCursor(int16_t x, int16_t y) { x_ = x; y_ = y; }
  void fillScreen(uint16_t c) {
    stub::ops().clear();
    stub::visiveis().clear();
    stub::ops().push_back({'S', 0, 0, w_, h_, 0, c, ""});
    ++stub::telas_desenhadas();
    gasto();
  }
  void fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t c) {
    limites(x, y, w, h, "fillRect");
    const stub::Caixa r{x, y, w, h, ""};
    auto& v = stub::visiveis();
    for (size_t i = 0; i < v.size();) {
      if (stub::contem(r, v[i])) v.erase(v.begin() + (long)i);
      else if (stub::cruza(r, v[i]) && w > 0 && h > 0) { violar("apaga parte de '" + v[i].desc + "'"); ++i; }
      else ++i;
    }
    stub::ops().push_back({'R', x, y, w, h, 0, c, ""});
    gasto();
  }
  void drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c) {
    limites(x, y, w, 1, "linha");
    adicionar({x, y, w, 1, "linha"});
    stub::ops().push_back({'L', x, y, w, 1, 0, c, ""});
    gasto();
  }
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
 private:
  void gasto() { stub::agora_ms() += stub::passo_por_desenho_ms(); }
  void violar(const std::string& m) { stub::violacoes().push_back(m); }
  void limites(int x, int y, int w, int h, const std::string& o) {
    if (x < 0 || y < 0 || x + w > w_ || y + h > h_)
      violar(o + " fora da tela em (" + std::to_string(x) + "," + std::to_string(y) + ") " + std::to_string(w) + "x" +
             std::to_string(h));
  }
  void adicionar(const stub::Caixa& c) {
    for (const auto& v : stub::visiveis())
      if (stub::cruza(c, v)) violar("'" + c.desc + "' sobrepõe '" + v.desc + "'");
    stub::visiveis().push_back(c);
  }
  void escrever(const char* s) {
    if (stub::wrap()) violar("setTextWrap(true): a GFX quebraria por caractere");
    const int w = 6 * tamanho_ * (int)std::strlen(s), h = 8 * tamanho_;
    limites(x_, y_, w, h, std::string("texto '") + s + "'");
    if (w > 0) adicionar({x_, y_, w, h, s});
    stub::ops().push_back({'T', x_, y_, w, h, tamanho_, cor_, s});
    x_ += w;
    gasto();
  }
  int x_ = 0, y_ = 0, tamanho_ = 1, w_ = 128, h_ = 160;
  uint16_t cor_ = 0xFFFF;
};
