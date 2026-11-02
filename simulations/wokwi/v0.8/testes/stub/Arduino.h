// Imitação mínima da API Arduino-ESP32 para testar o sketch no PC.
// Só para testes: registra Serial, pinos, PWM e tempo simulado.
#pragma once
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>

typedef uint8_t byte;
#define OUTPUT 1
#define INPUT 0
#define HIGH 1
#define LOW 0

namespace stub {
inline uint32_t& agora_ms() { static uint32_t t = 0; return t; }
inline uint32_t& passo_por_desenho_ms() { static uint32_t p = 0; return p; }  // tempo gasto por chamada à tela
inline std::map<int, int>& pinos() { static std::map<int, int> m; return m; }
inline std::map<int, uint32_t>& pwm() { static std::map<int, uint32_t> m; return m; }
inline std::string& serial() { static std::string s; return s; }
}  // namespace stub

inline uint32_t millis() { return stub::agora_ms(); }
inline void delay(uint32_t ms) { stub::agora_ms() += ms; }
inline void pinMode(int, int) {}
inline void digitalWrite(int pino, int v) { stub::pinos()[pino] = v; }
inline long map(long x, long a, long b, long c, long d) { return (x - a) * (d - c) / (b - a) + c; }
template <typename T> inline T constrain(T x, T a, T b) { return x < a ? a : (x > b ? b : x); }
inline bool ledcAttach(int, uint32_t, uint8_t) { return true; }
inline bool ledcWrite(int pino, uint32_t duty) { stub::pwm()[pino] = duty; return true; }

class SerialStub {
 public:
  void begin(unsigned long) {}
  void print(const char* s) { stub::serial() += s; }
  void println(const char* s = "") { stub::serial() += s; stub::serial() += "\n"; }
  int printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    const int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    stub::serial() += buf;
    return n;
  }
};
inline SerialStub Serial;
