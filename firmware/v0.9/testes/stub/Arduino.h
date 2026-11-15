// Imitação mínima da API Arduino-ESP32 para testar o sketch da v0.9 no PC.
// De propósito, NÃO declara pinMode nem digitalWrite: os LEDs são simulados, e
// se o firmware tentar acioná-los por GPIO a compilação do teste falha.
// O servo é real (LEDC no GPIO 12): ledcAttach/ledcWrite registram o duty.
#pragma once
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

typedef uint8_t byte;

namespace stub {
inline uint32_t& agora_ms() { static uint32_t t = 0; return t; }
inline uint32_t& passo_por_desenho_ms() { static uint32_t p = 0; return p; }
inline std::string& serial() { static std::string s; return s; }
}  // namespace stub

inline uint32_t millis() { return stub::agora_ms(); }
inline void delay(uint32_t ms) { stub::agora_ms() += ms; }
inline uint32_t getCpuFrequencyMhz() { return 240; }
#include <deque>
#include <map>
namespace stub {
inline std::map<int, uint32_t>& pwm() { static std::map<int, uint32_t> m; return m; }
inline bool& ledc_ok() { static bool ok = true; return ok; }
inline int& ledc_pino() { static int p = -1; return p; }
inline std::deque<char>& entrada() { static std::deque<char> d; return d; }  // bytes vindos do painel
}  // namespace stub
inline bool ledcAttach(uint8_t pino, uint32_t, uint8_t) { stub::ledc_pino() = pino; return stub::ledc_ok(); }
inline bool ledcWrite(uint8_t pino, uint32_t duty) { stub::pwm()[pino] = duty; return true; }
inline long map(long x, long a, long b, long c, long d) { return (x - a) * (d - c) / (b - a) + c; }
template <typename T> inline T constrain(T x, T a, T b) { return x < a ? a : (x > b ? b : x); }

class SerialStub {
 public:
  void begin(unsigned long) {}
  explicit operator bool() const { return true; }
  int available() { return (int)stub::entrada().size(); }
  int read() {
    if (stub::entrada().empty()) return -1;
    const char c = stub::entrada().front();
    stub::entrada().pop_front();
    return (unsigned char)c;
  }
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

struct EspStub { const char* getChipModel() { return "ESP32-S3"; } };
inline EspStub ESP;
