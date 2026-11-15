// Gerador determinístico para os testes (no ESP32-S3 é o gerador do chip).
#pragma once
#include <cstdint>
namespace stub { inline uint32_t& semente() { static uint32_t s = 12345u; return s; } }
inline uint32_t esp_random() {
  uint32_t& s = stub::semente();
  s ^= s << 13; s ^= s >> 17; s ^= s << 5;
  return s;
}
