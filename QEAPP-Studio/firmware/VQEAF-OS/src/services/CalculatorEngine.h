#pragma once
#include <stdint.h>
// Compact arithmetic model for the S60 4x4 keypad. No dynamic allocations.
class CalculatorEngine {
public:
  CalculatorEngine();
  void press(char key); // 0-9 . + - * / = C, '<' backspace
  const char *display() const { return screen; }
  bool error() const { return fault; }
private:
  char input[22];
  char screen[24];
  double accumulator;
  char operation;
  bool nextNumber, fault;
  void refresh();
  void calculate(double rhs);
};
