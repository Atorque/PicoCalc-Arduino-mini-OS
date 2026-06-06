#include "BudgetApp.h"

#include <Arduino.h>
#include <SD.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i2ckbd.h"

static const char* BUDGET_FILE = "/BUDGET.TXT";
static const int LIST_VISIBLE = 8;

static const int LIST_BOX_X = 12;
static const int LIST_BOX_Y = 62;
static const int LIST_BOX_W = 296;
static const int LIST_BOX_H = 186;

static const int LIST_INNER_X = 14;
static const int LIST_INNER_Y = 64;
static const int LIST_INNER_W = 292;
static const int LIST_INNER_H = 180;

static const int BAL_BOX_X = 188;
static const int BAL_BOX_Y = 250;
static const int BAL_BOX_W = 120;
static const int BAL_BOX_H = 32;

static void setStatus(BudgetState* state, const char* text, bool isError) {
  strncpy(state->statusLine, text, sizeof(state->statusLine) - 1);
  state->statusLine[sizeof(state->statusLine) - 1] = '\0';
  state->hasError = isError;
}

static long computeBalanceCents(const BudgetState* state) {
  long total = 0;
  for (int i = 0; i < state->itemCount; ++i) {
    total += state->items[i].credit ? state->items[i].cents : -state->items[i].cents;
  }
  return total;
}

static int listTopIndex(const BudgetState* state) {
  if (state->selectedIndex >= LIST_VISIBLE) {
    return state->selectedIndex - (LIST_VISIBLE - 1);
  }
  return 0;
}

static unsigned long itemsHash(const BudgetState* state) {
  unsigned long h = 2166136261u;
  for (int i = 0; i < state->itemCount; ++i) {
    const BudgetItem* it = &state->items[i];
    h ^= (unsigned long)it->cents;
    h *= 16777619u;
    h ^= (unsigned long)(it->credit ? 1 : 0);
    h *= 16777619u;
    h ^= (unsigned long)(it->recurring ? 1 : 0);
    h *= 16777619u;
    for (int c = 0; it->label[c] != '\0'; ++c) {
      h ^= (unsigned long)(unsigned char)it->label[c];
      h *= 16777619u;
    }
  }
  return h;
}

static void formatMoney(long cents, char* out, size_t outLen) {
  bool neg = cents < 0;
  long absCents = neg ? -cents : cents;
  long whole = absCents / 100;
  int frac = (int)(absCents % 100);
  snprintf(out, outLen, "%s%ld.%02d", neg ? "-" : "", whole, frac);
}

static bool saveItems(BudgetState* state) {
  if (!state->sdAvailable) {
    setStatus(state, "SD unavailable", true);
    return false;
  }

  SD.remove(BUDGET_FILE);
  File f = SD.open(BUDGET_FILE, FILE_WRITE);
  if (!f) {
    setStatus(state, "Budget save failed", true);
    return false;
  }

  for (int i = 0; i < state->itemCount; ++i) {
    char line[96];
    snprintf(line, sizeof(line), "%c|%c|%ld|%s\n",
             state->items[i].credit ? 'C' : 'D',
             state->items[i].recurring ? 'R' : 'O',
             state->items[i].cents,
             state->items[i].label);
    f.print(line);
  }

  f.close();
  return true;
}

static void loadItems(BudgetState* state) {
  state->itemCount = 0;
  state->selectedIndex = 0;

  if (!state->sdAvailable) {
    setStatus(state, "SD unavailable", true);
    return;
  }

  File f = SD.open(BUDGET_FILE, FILE_READ);
  if (!f) {
    File create = SD.open(BUDGET_FILE, FILE_WRITE);
    if (create) {
      create.close();
      setStatus(state, "Created new budget file", false);
    } else {
      setStatus(state, "Budget file create failed", true);
    }
    return;
  }

  char line[96];
  int pos = 0;

  while (f.available()) {
    char c = (char)f.read();
    if (c == '\r') continue;

    if (c == '\n') {
      line[pos] = '\0';
      pos = 0;

      char type = 'D';
      char freq = 'O';
      long cents = 0;
      char label[24];
      label[0] = '\0';
      if (sscanf(line, "%c|%c|%ld|%23[^\n]", &type, &freq, &cents, label) == 4) {
        if (state->itemCount < BUDGET_MAX_ITEMS && cents >= 0) {
          BudgetItem* item = &state->items[state->itemCount++];
          item->credit = (type == 'C');
          item->recurring = (freq == 'R');
          item->cents = cents;
          strncpy(item->label, label, sizeof(item->label) - 1);
          item->label[sizeof(item->label) - 1] = '\0';
        }
      }
      continue;
    }

    if (pos < (int)sizeof(line) - 1) {
      line[pos++] = c;
    }
  }

  f.close();

  char msg[64];
  snprintf(msg, sizeof(msg), "Loaded %d budget items", state->itemCount);
  setStatus(state, msg, false);
}

static bool parseAmountToCents(const char* text, long* centsOut) {
  if (!text || !text[0]) return false;

  long whole = 0;
  int frac = 0;
  int fracDigits = 0;
  bool seenDot = false;

  for (int i = 0; text[i] != '\0'; ++i) {
    char c = text[i];
    if (c == '.') {
      if (seenDot) return false;
      seenDot = true;
      continue;
    }

    if (c < '0' || c > '9') return false;

    if (!seenDot) {
      whole = whole * 10 + (c - '0');
      if (whole > 200000000) return false;
    } else if (fracDigits < 2) {
      frac = frac * 10 + (c - '0');
      fracDigits++;
    } else {
      return false;
    }
  }

  if (fracDigits == 1) frac *= 10;

  *centsOut = whole * 100 + frac;
  return true;
}

static void beginAdd(BudgetState* state, bool credit, bool recurring) {
  state->addMode = true;
  state->addCredit = credit;
  state->addRecurring = recurring;
  state->addStep = 0;
  state->amountBuffer[0] = '\0';
  state->amountLen = 0;
  state->labelBuffer[0] = '\0';
  state->labelLen = 0;
  setStatus(state, "Add: enter amount, Enter next", false);
}

void budgetInit(BudgetState* state, bool sdAvailable) {
  state->itemCount = 0;
  state->selectedIndex = 0;
  state->sdAvailable = sdAvailable;
  state->hasError = false;
  state->needsFullRedraw = true;

  state->addMode = false;
  state->addCredit = false;
  state->addRecurring = false;
  state->addStep = 0;
  state->amountBuffer[0] = '\0';
  state->amountLen = 0;
  state->labelBuffer[0] = '\0';
  state->labelLen = 0;

  setStatus(state, "C credit, D debit, R/F recurring", false);
  loadItems(state);
}

bool budgetHandleKey(BudgetState* state, int key) {
  if (key < 0) return false;

  if (state->addMode) {
    if (key == KEY_ESC) {
      state->addMode = false;
      setStatus(state, "Add cancelled", false);
      return true;
    }

    if (state->addStep == 0) {
      if (key == KEY_BACKSPACE || key == KEY_DEL) {
        if (state->amountLen > 0) {
          state->amountLen--;
          state->amountBuffer[state->amountLen] = '\0';
          return true;
        }
        return false;
      }

      if (key == KEY_ENTER) {
        long cents = 0;
        if (!parseAmountToCents(state->amountBuffer, &cents) || cents <= 0) {
          setStatus(state, "Invalid amount", true);
          return false;
        }
        state->addStep = 1;
        setStatus(state, "Add: enter label, Enter save", false);
        return true;
      }

      if ((key >= '0' && key <= '9') || key == '.') {
        if (state->amountLen >= (int)sizeof(state->amountBuffer) - 1) return false;
        state->amountBuffer[state->amountLen++] = (char)key;
        state->amountBuffer[state->amountLen] = '\0';
        return true;
      }

      return false;
    }

    if (state->addStep == 1) {
      if (key == KEY_BACKSPACE || key == KEY_DEL) {
        if (state->labelLen > 0) {
          state->labelLen--;
          state->labelBuffer[state->labelLen] = '\0';
          return true;
        }
        return false;
      }

      if (key == KEY_ENTER) {
        if (state->itemCount >= BUDGET_MAX_ITEMS) {
          setStatus(state, "Item limit reached", true);
          return false;
        }

        long cents = 0;
        if (!parseAmountToCents(state->amountBuffer, &cents) || cents <= 0) {
          setStatus(state, "Invalid amount", true);
          return false;
        }

        BudgetItem* item = &state->items[state->itemCount++];
        item->credit = state->addCredit;
        item->recurring = state->addRecurring;
        item->cents = cents;
        if (state->labelLen > 0) {
          strncpy(item->label, state->labelBuffer, sizeof(item->label) - 1);
          item->label[sizeof(item->label) - 1] = '\0';
        } else {
          strncpy(item->label, "item", sizeof(item->label) - 1);
          item->label[sizeof(item->label) - 1] = '\0';
        }

        state->selectedIndex = state->itemCount - 1;
        state->addMode = false;

        if (!saveItems(state)) return false;
        setStatus(state, "Budget item saved", false);
        return true;
      }

      if (key >= 32 && key <= 126) {
        if (state->labelLen >= (int)sizeof(state->labelBuffer) - 1) return false;
        state->labelBuffer[state->labelLen++] = (char)key;
        state->labelBuffer[state->labelLen] = '\0';
        return true;
      }

      return false;
    }
  }

  if (key == KEY_UP || key == 'w' || key == 'W') {
    if (state->selectedIndex > 0) {
      state->selectedIndex--;
      return true;
    }
    return false;
  }

  if (key == KEY_DOWN || key == 's' || key == 'S') {
    if (state->selectedIndex + 1 < state->itemCount) {
      state->selectedIndex++;
      return true;
    }
    return false;
  }

  if (key == 'c' || key == 'C') {
    beginAdd(state, true, false);
    return true;
  }

  if (key == 'd' || key == 'D') {
    beginAdd(state, false, false);
    return true;
  }

  if (key == 'r' || key == 'R') {
    beginAdd(state, true, true);
    return true;
  }

  if (key == 'f' || key == 'F') {
    beginAdd(state, false, true);
    return true;
  }

  if (key == 'l' || key == 'L') {
    loadItems(state);
    if (state->selectedIndex >= state->itemCount) state->selectedIndex = state->itemCount - 1;
    if (state->selectedIndex < 0) state->selectedIndex = 0;
    return true;
  }

  if (key == KEY_DEL) {
    if (state->itemCount <= 0) {
      setStatus(state, "No item to delete", false);
      return true;
    }

    int idx = state->selectedIndex;
    if (idx < 0) idx = 0;
    if (idx >= state->itemCount) idx = state->itemCount - 1;

    for (int i = idx; i + 1 < state->itemCount; ++i) {
      state->items[i] = state->items[i + 1];
    }
    state->itemCount--;

    if (state->selectedIndex >= state->itemCount) state->selectedIndex = state->itemCount - 1;
    if (state->selectedIndex < 0) state->selectedIndex = 0;

    if (!saveItems(state)) return false;
    setStatus(state, "Budget item deleted", false);
    return true;
  }

  if (key == 't' || key == 'T') {
    if (state->itemCount <= 0) {
      setStatus(state, "No item to toggle", false);
      return true;
    }

    int idx = state->selectedIndex;
    if (idx < 0) idx = 0;
    if (idx >= state->itemCount) idx = state->itemCount - 1;

    state->items[idx].recurring = !state->items[idx].recurring;
    if (!saveItems(state)) return false;
    setStatus(state, "Recurring toggled", false);
    return true;
  }

  return false;
}

void budgetDrawScreen(PicoCalc_Display& display, BudgetState* state) {
  static bool hasPrev = false;
  static int prevSelectedIndex = 0;
  static int prevItemCount = 0;
  static int prevTop = 0;
  static bool prevAddMode = false;
  static int prevAddStep = 0;
  static int prevAmountLen = 0;
  static int prevLabelLen = 0;
  static unsigned long prevItemsHash = 0;
  static long prevBalance = 0;
  static bool prevHasError = false;
  static char prevStatus[72] = "";

  if (state->needsFullRedraw) {
    display.clear();
    display.fillRect(16, 12, 288, 40, DISPLAY_BLACK);
    display.drawRect(16, 12, 288, 40, DISPLAY_CYAN);
    display.print(94, 24, "BUDGET", DISPLAY_WHITE, 2);

    display.drawRect(LIST_BOX_X, LIST_BOX_Y, LIST_BOX_W, LIST_BOX_H, DISPLAY_MAGENTA);
    display.print(14, 250, "C/D one-time  R/F recurring", DISPLAY_CYAN, 1);
    display.print(14, 264, "Del remove  T toggle recur", DISPLAY_CYAN, 1);
    display.print(14, 278, "L reload  Esc back", DISPLAY_CYAN, 1);

    display.drawRect(BAL_BOX_X, BAL_BOX_Y, BAL_BOX_W, BAL_BOX_H, DISPLAY_CYAN);
    display.print(BAL_BOX_X + 10, BAL_BOX_Y + 4, "BALANCE", DISPLAY_CYAN, 1);

    state->needsFullRedraw = false;
    hasPrev = false;
  }

  unsigned long curHash = itemsHash(state);
  int top = listTopIndex(state);
  long bal = computeBalanceCents(state);
  bool listChanged = !hasPrev || prevSelectedIndex != state->selectedIndex || prevItemCount != state->itemCount ||
                     prevTop != top || prevItemsHash != curHash || prevAddMode != state->addMode;
  bool balanceChanged = !hasPrev || prevBalance != bal || prevItemsHash != curHash;
  bool statusChanged = !hasPrev || prevHasError != state->hasError || strcmp(prevStatus, state->statusLine) != 0;
  bool addChanged = !hasPrev || prevAddMode != state->addMode || prevAddStep != state->addStep ||
                    prevAmountLen != state->amountLen || prevLabelLen != state->labelLen;

  if (listChanged) {
    // Keep list confined inside the magenta frame.
    display.fillRect(LIST_INNER_X, LIST_INNER_Y, LIST_INNER_W, LIST_INNER_H, DISPLAY_BLACK);

    for (int row = 0; row < LIST_VISIBLE; ++row) {
      int idx = top + row;
      if (idx >= state->itemCount) break;

      int y = 68 + row * 22;
      if (idx == state->selectedIndex) {
        display.fillRect(16, y - 1, 286, 18, DISPLAY_BLUE);
      }

      char amt[24];
      formatMoney(state->items[idx].cents, amt, sizeof(amt));

      char line[64];
      snprintf(line, sizeof(line), "%c %c %s %s",
               state->items[idx].credit ? '+' : '-',
               state->items[idx].recurring ? 'R' : 'O',
               amt,
               state->items[idx].label);
      line[46] = '\0';
      display.print(18, y, line, idx == state->selectedIndex ? DISPLAY_WHITE : DISPLAY_YELLOW, 1);
    }
  }

  if (balanceChanged) {
    display.fillRect(BAL_BOX_X + 2, BAL_BOX_Y + 14, BAL_BOX_W - 4, BAL_BOX_H - 16, DISPLAY_BLACK);
    char money[24];
    formatMoney(bal, money, sizeof(money));
    display.print(BAL_BOX_X + 8, BAL_BOX_Y + 18, money, bal >= 0 ? DISPLAY_GREEN : DISPLAY_RED, 1);
  }

  if (statusChanged) {
    display.fillRect(12, 292, 308, 14, DISPLAY_BLACK);
    if (state->hasError) {
      display.print(12, 292, state->statusLine, DISPLAY_RED, 1);
    } else {
      display.print(12, 292, state->statusLine, DISPLAY_GREEN, 1);
    }
  }

  if (state->addMode) {
    display.fillRect(24, 112, 272, 90, DISPLAY_BLACK);
    display.drawRect(24, 112, 272, 90, DISPLAY_CYAN);

    char title[40];
    snprintf(title, sizeof(title), "Add %s %s",
             state->addCredit ? "Credit" : "Debit",
             state->addRecurring ? "Recurring" : "One-time");
    display.print(32, 122, title, DISPLAY_CYAN, 1);

    if (state->addStep == 0) {
      display.print(32, 140, "Amount:", DISPLAY_YELLOW, 1);
      display.print(88, 140, state->amountBuffer, DISPLAY_WHITE, 1);
      display.drawLine(88 + state->amountLen * 6, 140, 88 + state->amountLen * 6, 150, DISPLAY_YELLOW);
      display.print(32, 182, "Enter next, Esc cancel", DISPLAY_CYAN, 1);
    } else {
      display.print(32, 140, "Label:", DISPLAY_YELLOW, 1);
      display.print(74, 140, state->labelBuffer, DISPLAY_WHITE, 1);
      display.drawLine(74 + state->labelLen * 6, 140, 74 + state->labelLen * 6, 150, DISPLAY_YELLOW);
      display.print(32, 182, "Enter save, Esc cancel", DISPLAY_CYAN, 1);
    }
  } else if (hasPrev && prevAddMode) {
    // Remove popup artifacts and restore list beneath.
    display.fillRect(24, 112, 272, 90, DISPLAY_BLACK);
    display.fillRect(LIST_INNER_X, LIST_INNER_Y, LIST_INNER_W, LIST_INNER_H, DISPLAY_BLACK);
    for (int row = 0; row < LIST_VISIBLE; ++row) {
      int idx = top + row;
      if (idx >= state->itemCount) break;

      int y = 68 + row * 22;
      if (idx == state->selectedIndex) {
        display.fillRect(16, y - 1, 286, 18, DISPLAY_BLUE);
      }

      char amt[24];
      formatMoney(state->items[idx].cents, amt, sizeof(amt));

      char line[64];
      snprintf(line, sizeof(line), "%c %c %s %s",
               state->items[idx].credit ? '+' : '-',
               state->items[idx].recurring ? 'R' : 'O',
               amt,
               state->items[idx].label);
      line[46] = '\0';
      display.print(18, y, line, idx == state->selectedIndex ? DISPLAY_WHITE : DISPLAY_YELLOW, 1);
    }
  } else if (addChanged && !state->addMode) {
    // no-op: addChanged is tracked to avoid stale previous snapshots
  }

  prevSelectedIndex = state->selectedIndex;
  prevItemCount = state->itemCount;
  prevTop = top;
  prevAddMode = state->addMode;
  prevAddStep = state->addStep;
  prevAmountLen = state->amountLen;
  prevLabelLen = state->labelLen;
  prevItemsHash = curHash;
  prevBalance = bal;
  prevHasError = state->hasError;
  strncpy(prevStatus, state->statusLine, sizeof(prevStatus) - 1);
  prevStatus[sizeof(prevStatus) - 1] = '\0';
  hasPrev = true;
}
