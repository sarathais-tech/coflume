// Relógio de microssegundos para os testes: acompanha o tempo simulado.
#pragma once
#include <cstdint>
#include "Arduino.h"
inline int64_t esp_timer_get_time() { return (int64_t)stub::agora_ms() * 1000; }
