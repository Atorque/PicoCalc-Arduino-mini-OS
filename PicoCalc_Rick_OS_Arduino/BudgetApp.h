#ifndef BUDGET_APP_H
#define BUDGET_APP_H

#include "PicoCalc_Display.h"

#define BUDGET_MAX_ITEMS 64

struct BudgetItem {
  long cents;
  bool credit;
  bool recurring;
  char label[24];
};

struct BudgetState {
  BudgetItem items[BUDGET_MAX_ITEMS];
  int itemCount;
  int selectedIndex;
  bool sdAvailable;
  bool hasError;
  bool needsFullRedraw;

  bool addMode;
  bool addCredit;
  bool addRecurring;
  int addStep;
  char amountBuffer[16];
  int amountLen;
  char labelBuffer[24];
  int labelLen;

  char statusLine[72];
};

void budgetInit(BudgetState* state, bool sdAvailable);
bool budgetHandleKey(BudgetState* state, int key);
void budgetDrawScreen(PicoCalc_Display& display, BudgetState* state);

#endif // BUDGET_APP_H
