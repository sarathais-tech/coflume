// Teclado 4x4 simulado: as teclas vêm de uma fila preenchida pelo teste.
#pragma once
#include <deque>
#include "Arduino.h"
#define NO_KEY '\0'
#define makeKeymap(x) ((char*)x)
namespace stub { inline std::deque<char>& teclas() { static std::deque<char> f; return f; } }
class Keypad {
 public:
  Keypad(char*, byte*, byte*, byte, byte) {}
  char getKey() {
    if (stub::teclas().empty()) return NO_KEY;
    const char k = stub::teclas().front();
    stub::teclas().pop_front();
    return k;
  }
};
