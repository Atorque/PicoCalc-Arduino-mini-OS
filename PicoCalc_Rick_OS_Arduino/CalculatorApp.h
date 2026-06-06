#ifndef CALCULATOR_APP_H
#define CALCULATOR_APP_H

#include "PicoCalc_Display.h"

struct CalculatorState {
  char formula[120];
  char result[32];
  bool hasResult;
  bool hasError;
  bool needsFullRedraw;
};

void calculatorInit(CalculatorState* state);
void calculatorReset(CalculatorState* state);
void calculatorDrawScreen(PicoCalc_Display& display, CalculatorState* state);

// Returns true when state changed and screen should be redrawn.
bool calculatorHandleKey(CalculatorState* state, int key);

#endif // CALCULATOR_APP_H
