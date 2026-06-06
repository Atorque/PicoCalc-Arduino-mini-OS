#include "CalculatorApp.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>
#include "i2ckbd.h"

static const int TITLE_X = 16;
static const int TITLE_Y = 14;
static const int TITLE_W = 288;
static const int TITLE_H = 40;

static const int DISPLAY_X = 16;
static const int DISPLAY_Y = 68;
static const int DISPLAY_W = 288;
static const int DISPLAY_H = 72;

static const int HELP_X = 16;
static const int HELP_Y = 152;
static const int HELP_W = 288;
static const int HELP_H = 128;

static void formatNumber(double value, char* out, size_t outSize) {
  dtostrf((float)value, 0, 6, out);

  char* p = out;
  while (*p == ' ') p++;
  if (p != out) {
    memmove(out, p, strlen(p) + 1);
  }

  int len = (int)strlen(out);
  while (len > 0 && out[len - 1] == '0') {
    out[len - 1] = '\0';
    len--;
  }
  if (len > 0 && out[len - 1] == '.') {
    out[len - 1] = '\0';
  }
  if (out[0] == '\0') {
    strncpy(out, "0", outSize - 1);
    out[outSize - 1] = '\0';
  }
}

static bool isAllowedFormulaChar(char c) {
  if (c >= '0' && c <= '9') return true;
  if (c == '.' || c == '+' || c == '-' || c == '*' || c == '/' || c == '^') return true;
  if (c == '(' || c == ')' || c == ' ') return true;
  if (c >= 'a' && c <= 'z') return true;
  return false;
}

static bool appendFormulaChar(CalculatorState* state, char c) {
  size_t len = strlen(state->formula);
  if (len >= sizeof(state->formula) - 1) return false;
  state->formula[len] = c;
  state->formula[len + 1] = '\0';
  return true;
}

struct Parser {
  const char* p;
  bool ok;
};

static void skipSpaces(Parser* parser) {
  while (*parser->p == ' ' || *parser->p == '\t') {
    parser->p++;
  }
}

static double parseExpression(Parser* parser);

static bool match(Parser* parser, char c) {
  skipSpaces(parser);
  if (*parser->p == c) {
    parser->p++;
    return true;
  }
  return false;
}

static double parsePrimary(Parser* parser) {
  skipSpaces(parser);

  if (match(parser, '(')) {
    double value = parseExpression(parser);
    if (!match(parser, ')')) {
      parser->ok = false;
    }
    return value;
  }

  if (isalpha((unsigned char)*parser->p)) {
    char ident[12];
    int n = 0;
    while (isalpha((unsigned char)*parser->p) && n < (int)sizeof(ident) - 1) {
      ident[n++] = (char)tolower((unsigned char)*parser->p);
      parser->p++;
    }
    ident[n] = '\0';

    if (strcmp(ident, "pi") == 0) return 3.141592653589793;
    if (strcmp(ident, "e") == 0) return 2.718281828459045;

    if (!match(parser, '(')) {
      parser->ok = false;
      return 0.0;
    }
    double arg = parseExpression(parser);
    if (!match(parser, ')')) {
      parser->ok = false;
      return 0.0;
    }

    if (strcmp(ident, "sin") == 0) return sin(arg);
    if (strcmp(ident, "cos") == 0) return cos(arg);
    if (strcmp(ident, "tan") == 0) return tan(arg);
    if (strcmp(ident, "sqrt") == 0) {
      if (arg < 0.0) {
        parser->ok = false;
        return 0.0;
      }
      return sqrt(arg);
    }
    if (strcmp(ident, "log") == 0) {
      if (arg <= 0.0) {
        parser->ok = false;
        return 0.0;
      }
      return log10(arg);
    }
    if (strcmp(ident, "ln") == 0) {
      if (arg <= 0.0) {
        parser->ok = false;
        return 0.0;
      }
      return log(arg);
    }
    if (strcmp(ident, "abs") == 0) return fabs(arg);

    parser->ok = false;
    return 0.0;
  }

  char* endPtr = nullptr;
  double value = strtod(parser->p, &endPtr);
  if (endPtr == parser->p) {
    parser->ok = false;
    return 0.0;
  }
  parser->p = endPtr;
  return value;
}

static double parseUnary(Parser* parser) {
  skipSpaces(parser);
  if (match(parser, '+')) return parseUnary(parser);
  if (match(parser, '-')) return -parseUnary(parser);
  return parsePrimary(parser);
}

static double parsePower(Parser* parser) {
  double base = parseUnary(parser);
  if (!parser->ok) return 0.0;

  skipSpaces(parser);
  if (match(parser, '^')) {
    double exponent = parsePower(parser);
    if (!parser->ok) return 0.0;
    return pow(base, exponent);
  }
  return base;
}

static double parseTerm(Parser* parser) {
  double value = parsePower(parser);
  if (!parser->ok) return 0.0;

  while (true) {
    skipSpaces(parser);
    if (match(parser, '*')) {
      value *= parsePower(parser);
      if (!parser->ok) return 0.0;
    } else if (match(parser, '/')) {
      double divisor = parsePower(parser);
      if (!parser->ok || divisor == 0.0) {
        parser->ok = false;
        return 0.0;
      }
      value /= divisor;
    } else {
      return value;
    }
  }
}

static double parseExpression(Parser* parser) {
  double value = parseTerm(parser);
  if (!parser->ok) return 0.0;

  while (true) {
    skipSpaces(parser);
    if (match(parser, '+')) {
      value += parseTerm(parser);
      if (!parser->ok) return 0.0;
    } else if (match(parser, '-')) {
      value -= parseTerm(parser);
      if (!parser->ok) return 0.0;
    } else {
      return value;
    }
  }
}

static bool evaluateExpression(const char* expression, double* outResult) {
  Parser parser;
  parser.p = expression;
  parser.ok = true;

  double result = parseExpression(&parser);
  skipSpaces(&parser);
  if (!parser.ok || *parser.p != '\0') {
    return false;
  }

  if (isnan(result) || isinf(result)) {
    return false;
  }

  *outResult = result;
  return true;
}

static void drawFormulaWrapped(PicoCalc_Display& display, const char* formula) {
  const int charsPerLine = 45;
  char line1[48];
  char line2[48];
  char line3[48];
  line1[0] = '\0';
  line2[0] = '\0';
  line3[0] = '\0';

  size_t len = strlen(formula);
  if (len <= (size_t)charsPerLine) {
    strncpy(line1, formula, sizeof(line1) - 1);
    line1[sizeof(line1) - 1] = '\0';
  } else {
    strncpy(line1, formula, charsPerLine);
    line1[charsPerLine] = '\0';

    if (len > (size_t)charsPerLine) {
      size_t remaining = len - (size_t)charsPerLine;
      size_t take = remaining > (size_t)charsPerLine ? (size_t)charsPerLine : remaining;
      strncpy(line2, formula + charsPerLine, take);
      line2[take] = '\0';
    }

    if (len > (size_t)(charsPerLine * 2)) {
      size_t remaining = len - (size_t)(charsPerLine * 2);
      size_t take = remaining > (sizeof(line3) - 1) ? (sizeof(line3) - 1) : remaining;
      strncpy(line3, formula + charsPerLine * 2, take);
      line3[take] = '\0';
    }
  }

  display.print(24, 80, line1[0] ? line1 : "0", DISPLAY_YELLOW, 1);
  if (line2[0]) {
    display.print(24, 92, line2, DISPLAY_YELLOW, 1);
  }
  if (line3[0]) {
    display.print(24, 104, line3, DISPLAY_YELLOW, 1);
  }
}

void calculatorReset(CalculatorState* state) {
  state->formula[0] = '\0';
  strncpy(state->result, "0", sizeof(state->result) - 1);
  state->result[sizeof(state->result) - 1] = '\0';
  state->hasResult = false;
  state->hasError = false;
}

void calculatorInit(CalculatorState* state) {
  calculatorReset(state);
  state->needsFullRedraw = true;
}

bool calculatorHandleKey(CalculatorState* state, int key) {
  if (key < 0) return false;

  char c = (char)key;

  if (key == 'c' || key == 'C') {
    calculatorReset(state);
    return true;
  }

  if (key == KEY_BACKSPACE || key == KEY_DEL) {
    size_t len = strlen(state->formula);
    if (len > 0) {
      state->formula[len - 1] = '\0';
      state->hasError = false;
      state->hasResult = false;
      return true;
    }
    return false;
  }

  if (key == '=' || key == KEY_ENTER) {
    if (state->formula[0] == '\0') {
      return false;
    }

    double result = 0.0;
    if (!evaluateExpression(state->formula, &result)) {
      state->hasError = true;
      state->hasResult = false;
      return true;
    }

    formatNumber(result, state->result, sizeof(state->result));
    state->hasResult = true;
    state->hasError = false;
    return true;
  }

  if (isalpha((unsigned char)c)) {
    c = (char)tolower((unsigned char)c);
  }

  if (isAllowedFormulaChar(c)) {
    if (state->hasError && c != ' ') {
      state->hasError = false;
    }
    if (state->hasResult && c != ' ') {
      state->hasResult = false;
    }
    return appendFormulaChar(state, c);
  }

  return false;
}

void calculatorDrawScreen(PicoCalc_Display& display, CalculatorState* state) {
  if (state->needsFullRedraw) {
    display.clear();

    display.fillRect(TITLE_X, TITLE_Y, TITLE_W, TITLE_H, DISPLAY_BLACK);
    display.drawRect(TITLE_X, TITLE_Y, TITLE_W, TITLE_H, DISPLAY_CYAN);
    display.print(58, 27, "SCIENTIFIC CALC", DISPLAY_WHITE, 2);

    display.drawRect(DISPLAY_X, DISPLAY_Y, DISPLAY_W, DISPLAY_H, DISPLAY_CYAN);

    display.drawRect(HELP_X, HELP_Y, HELP_W, HELP_H, DISPLAY_GREEN);
    display.print(24, 166, "Expr: + - * / ^ ( ) .", DISPLAY_GREEN, 1);
    display.print(24, 182, "Funcs: sin cos tan sqrt", DISPLAY_GREEN, 1);
    display.print(24, 198, "       log ln abs", DISPLAY_GREEN, 1);
    display.print(24, 214, "Consts: pi e", DISPLAY_GREEN, 1);
    display.print(24, 230, "Enter/= eval  C clear", DISPLAY_GREEN, 1);
    display.print(24, 246, "Bksp del char  Esc menu", DISPLAY_GREEN, 1);
    state->needsFullRedraw = false;
  }

  // Partial redraw: only refresh the calculator display area to avoid full-screen flicker.
  display.fillRect(DISPLAY_X + 1, DISPLAY_Y + 1, DISPLAY_W - 2, DISPLAY_H - 2, DISPLAY_BLACK);

  drawFormulaWrapped(display, state->formula);

  if (state->hasError) {
    display.print(24, 116, "Error: invalid formula", DISPLAY_RED, 1);
    return;
  }

  if (state->hasResult) {
    char resultLine[40];
    snprintf(resultLine, sizeof(resultLine), "= %s", state->result);
    display.print(24, 120, resultLine, DISPLAY_WHITE, 1);
  } else {
    display.print(24, 120, "Press Enter to evaluate", DISPLAY_CYAN, 1);
  }
}
