#include "SudokuGame.h"

#include <Arduino.h>
#include <stdio.h>

static const int GRID_X = 20;
static const int GRID_Y = 20;
static const int CELL_SIZE = 30;
static const int GRID_SIZE = CELL_SIZE * 9;

static const int INTRO_MENU_X = 34;
static const int INTRO_MENU_W = 252;
static const int INTRO_ROW_H = 24;
static const int INTRO_FIRST_Y = 88;
static const int INTRO_ROW_GAP = 5;

static const uint32_t SELECTION_COLOR_OUTER = DISPLAY_WHITE;
static const uint32_t SELECTION_COLOR_INNER = DISPLAY_YELLOW;

const char* sudokuDifficultyLabel(SudokuDifficulty difficulty) {
  if (difficulty == SUDOKU_DIFFICULTY_EASY) return "EASY";
  if (difficulty == SUDOKU_DIFFICULTY_MEDIUM) return "MEDIUM";
  if (difficulty == SUDOKU_DIFFICULTY_HARD) return "HARD";
  if (difficulty == SUDOKU_DIFFICULTY_EXPERT) return "EXPERT";
  if (difficulty == SUDOKU_DIFFICULTY_MASTER) return "MASTER";
  return "EXTREME";
}

const char* sudokuChallengeLabel(SudokuDifficulty difficulty) {
  if (difficulty == SUDOKU_DIFFICULTY_EASY) return "Relaxed";
  if (difficulty == SUDOKU_DIFFICULTY_MEDIUM) return "Balanced";
  if (difficulty == SUDOKU_DIFFICULTY_HARD) return "Tough";
  if (difficulty == SUDOKU_DIFFICULTY_EXPERT) return "Intense";
  if (difficulty == SUDOKU_DIFFICULTY_MASTER) return "Brutal";
  return "Extreme";
}

uint32_t sudokuDifficultyColor(SudokuDifficulty difficulty) {
  if (difficulty == SUDOKU_DIFFICULTY_EASY) return DISPLAY_GREEN;
  if (difficulty == SUDOKU_DIFFICULTY_MEDIUM) return DISPLAY_YELLOW;
  if (difficulty == SUDOKU_DIFFICULTY_HARD) return DISPLAY_ORANGE;
  if (difficulty == SUDOKU_DIFFICULTY_EXPERT) return DISPLAY_RED;
  if (difficulty == SUDOKU_DIFFICULTY_MASTER) return DISPLAY_MAGENTA;
  return DISPLAY_PINK;
}

uint32_t sudokuDifficultyLegendColor(SudokuDifficulty difficulty) {
  if (difficulty == SUDOKU_DIFFICULTY_EASY) return DISPLAY_CYAN;
  if (difficulty == SUDOKU_DIFFICULTY_MEDIUM) return DISPLAY_BLUE;
  if (difficulty == SUDOKU_DIFFICULTY_HARD) return DISPLAY_CYAN;
  if (difficulty == SUDOKU_DIFFICULTY_EXPERT) return DISPLAY_YELLOW;
  if (difficulty == SUDOKU_DIFFICULTY_MASTER) return DISPLAY_GREEN;
  return DISPLAY_ORANGE;
}

void sudokuInitState(SudokuState* state) {
  state->selectedRow = 0;
  state->selectedCol = 0;
  state->editableCellCount = 0;
  state->statusLine[0] = '\0';

  for (unsigned char row = 0; row < 9; ++row) {
    for (unsigned char col = 0; col < 9; ++col) {
      state->solvedGrid[row][col] = 0;
      state->puzzleGrid[row][col] = 0;
      state->fixedCell[row][col] = false;
      state->wrongCell[row][col] = false;
    }
  }
}

static void seedRandomGenerator() {
  randomSeed((unsigned long)micros());
}

static int nextRandom(int limit) {
  return random(limit);
}

static unsigned char pattern(unsigned char row, unsigned char col) {
  return (row * 3 + row / 3 + col) % 9;
}

static void swapValues(unsigned char& left, unsigned char& right) {
  unsigned char temp = left;
  left = right;
  right = temp;
}

static void shuffleValues(unsigned char values[], unsigned char count) {
  for (int index = count - 1; index > 0; --index) {
    int otherIndex = nextRandom(index + 1);
    swapValues(values[index], values[otherIndex]);
  }
}

static void generateSolvedGrid(unsigned char solvedGrid[9][9]) {
  unsigned char rowBands[3] = { 0, 1, 2 };
  unsigned char columnStacks[3] = { 0, 1, 2 };
  unsigned char rowOrder[9];
  unsigned char columnOrder[9];
  unsigned char digits[9] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };

  seedRandomGenerator();
  shuffleValues(rowBands, 3);
  shuffleValues(columnStacks, 3);
  shuffleValues(digits, 9);

  for (unsigned char band = 0; band < 3; ++band) {
    unsigned char rowBase = (unsigned char)(rowBands[band] * 3);
    rowOrder[band * 3 + 0] = rowBase + 0;
    rowOrder[band * 3 + 1] = rowBase + 1;
    rowOrder[band * 3 + 2] = rowBase + 2;
  }

  for (unsigned char stack = 0; stack < 3; ++stack) {
    unsigned char columnBase = (unsigned char)(columnStacks[stack] * 3);
    columnOrder[stack * 3 + 0] = columnBase + 0;
    columnOrder[stack * 3 + 1] = columnBase + 1;
    columnOrder[stack * 3 + 2] = columnBase + 2;
  }

  for (unsigned char row = 0; row < 9; ++row) {
    for (unsigned char col = 0; col < 9; ++col) {
      unsigned char valueIndex = pattern(rowOrder[row], columnOrder[col]);
      solvedGrid[row][col] = digits[valueIndex];
    }
  }
}

void sudokuStartNewPuzzle(SudokuState* state, SudokuDifficulty difficulty) {
  int removeCount = 36;
  if (difficulty == SUDOKU_DIFFICULTY_EASY) {
    removeCount = 30;
  } else if (difficulty == SUDOKU_DIFFICULTY_HARD) {
    removeCount = 45;
  } else if (difficulty == SUDOKU_DIFFICULTY_EXPERT) {
    removeCount = 50;
  } else if (difficulty == SUDOKU_DIFFICULTY_MASTER) {
    removeCount = 54;
  } else if (difficulty == SUDOKU_DIFFICULTY_EXTREME) {
    removeCount = 58;
  }

  generateSolvedGrid(state->solvedGrid);
  for (unsigned char row = 0; row < 9; ++row) {
    for (unsigned char col = 0; col < 9; ++col) {
      state->puzzleGrid[row][col] = state->solvedGrid[row][col];
      state->fixedCell[row][col] = true;
      state->wrongCell[row][col] = false;
    }
  }

  unsigned char cells[81];
  for (unsigned char i = 0; i < 81; ++i) {
    cells[i] = i;
  }
  shuffleValues(cells, 81);

  for (int i = 0; i < removeCount; ++i) {
    unsigned char index = cells[i];
    unsigned char row = index / 9;
    unsigned char col = index % 9;
    state->puzzleGrid[row][col] = 0;
    state->fixedCell[row][col] = false;
    state->wrongCell[row][col] = false;
  }

  state->editableCellCount = removeCount;
  state->selectedRow = 0;
  state->selectedCol = 0;
  state->statusLine[0] = '\0';
}

void sudokuDrawIntroOptionRow(PicoCalc_Display& display, SudokuDifficulty level, SudokuDifficulty currentDifficulty, bool selected) {
  int y = INTRO_FIRST_Y + ((int)level) * (INTRO_ROW_H + INTRO_ROW_GAP);
  uint32_t color = sudokuDifficultyColor(level);

  display.fillRect(INTRO_MENU_X, y, INTRO_MENU_W, INTRO_ROW_H, DISPLAY_BLACK);
  display.drawRect(INTRO_MENU_X, y, INTRO_MENU_W, INTRO_ROW_H, color);
  if (selected) {
    display.fillRect(INTRO_MENU_X + 2, y + 2, INTRO_MENU_W - 4, INTRO_ROW_H - 4, DISPLAY_BLUE);
    display.print(INTRO_MENU_X + 230, y + 8, "<", DISPLAY_YELLOW, 1);
  }

  char numberText[2];
  numberText[0] = (char)('1' + (int)level);
  numberText[1] = '\0';
  display.print(INTRO_MENU_X + 10, y + 8, numberText, DISPLAY_WHITE, 1);
  display.print(INTRO_MENU_X + 28, y + 8, sudokuDifficultyLabel(level), color, 1);

  if (level == currentDifficulty) {
    display.print(INTRO_MENU_X + 164, y + 8, sudokuChallengeLabel(level), DISPLAY_CYAN, 1);
  }
}

void sudokuDrawIntroChallengeLegend(PicoCalc_Display& display, SudokuDifficulty difficulty) {
  display.fillRect(34, 257, 252, 20, DISPLAY_BLACK);
  display.drawRect(34, 257, 252, 20, DISPLAY_CYAN);
  display.print(40, 263, "Challenge:", DISPLAY_CYAN, 1);
  display.print(126, 263, sudokuChallengeLabel(difficulty), sudokuDifficultyColor(difficulty), 1);
}

void sudokuDrawIntroScreen(PicoCalc_Display& display, SudokuDifficulty difficulty) {
  display.clear();
  display.fillRect(22, 14, 276, 50, DISPLAY_BLACK);
  display.drawRect(22, 14, 276, 50, DISPLAY_CYAN);
  display.drawRect(24, 16, 272, 46, DISPLAY_CYAN);
  display.print(56, 29, "PICO SUDOKU", DISPLAY_WHITE, 2);
  display.print(70, 71, "Select Difficulty", DISPLAY_YELLOW, 2);

  for (int i = 0; i < 6; ++i) {
    SudokuDifficulty level = (SudokuDifficulty)i;
    sudokuDrawIntroOptionRow(display, level, difficulty, level == difficulty);
  }

  sudokuDrawIntroChallengeLegend(display, difficulty);

  display.print(34, 286, "UP/DOWN or 1-6", DISPLAY_CYAN, 1);
  display.print(34, 301, "ENTER or G to start", DISPLAY_CYAN, 1);
}

void sudokuPlayIntroToGameTransition(PicoCalc_Display& display, SudokuDifficulty difficulty) {
  uint32_t accent = sudokuDifficultyColor(difficulty);
  int width = display.width();
  int height = display.height();

  for (int y = 0; y < height; y += 16) {
    display.fillRect(0, y, width, 8, accent);
    delay(8);
  }

  for (int inset = 0; inset < 80; inset += 8) {
    display.drawRect(inset, inset, width - inset * 2, height - inset * 2, accent);
    delay(5);
  }

  display.clear();
}

static void drawGrid(PicoCalc_Display& display, uint32_t lineColor) {
  for (unsigned char i = 0; i <= 9; ++i) {
    const int x = GRID_X + i * CELL_SIZE;
    const int y = GRID_Y + i * CELL_SIZE;

    if (i % 3 == 0) {
      display.drawLine(x, GRID_Y, x, GRID_Y + GRID_SIZE, lineColor);
      display.drawLine(x + 1, GRID_Y, x + 1, GRID_Y + GRID_SIZE, lineColor);
      display.drawLine(GRID_X, y, GRID_X + GRID_SIZE, y, lineColor);
      display.drawLine(GRID_X, y + 1, GRID_X + GRID_SIZE, y + 1, lineColor);
    } else {
      display.drawLine(x, GRID_Y, x, GRID_Y + GRID_SIZE, lineColor);
      display.drawLine(GRID_X, y, GRID_X + GRID_SIZE, y, lineColor);
    }
  }
}

void sudokuDrawGameFooter(PicoCalc_Display& display, const SudokuState* state, uint32_t accent) {
  display.fillRect(0, 292, display.width(), 28, DISPLAY_BLACK);
  display.print(10, 295, "Arrows 1-9  0/Bksp  C:check  V:solve", accent, 1);
  display.print(10, 309, "D/Esc:menu  G:new", accent, 1);
  if (state->statusLine[0] != '\0') {
    display.print(170, 309, state->statusLine, DISPLAY_WHITE, 1);
  }
}

void sudokuRedrawGameCell(PicoCalc_Display& display, SudokuState* state, SudokuDifficulty difficulty, unsigned char row, unsigned char col, bool selected) {
  (void)difficulty;
  int cellLeft = GRID_X + col * CELL_SIZE;
  int cellTop = GRID_Y + row * CELL_SIZE;

  display.fillRect(cellLeft + 2, cellTop + 2, CELL_SIZE - 3, CELL_SIZE - 3, DISPLAY_BLACK);

  if (state->puzzleGrid[row][col] != 0) {
    char cellText[2];
    cellText[0] = (char)('0' + state->puzzleGrid[row][col]);
    cellText[1] = '\0';
    uint32_t numberColor = state->fixedCell[row][col] ? DISPLAY_WHITE : DISPLAY_CYAN;
    if (state->wrongCell[row][col]) {
      numberColor = DISPLAY_RED;
    }
    display.print(GRID_X + 10 + col * CELL_SIZE, GRID_Y + 9 + row * CELL_SIZE, cellText, numberColor, 2);
  }

  if (selected) {
    display.drawRect(cellLeft + 2, cellTop + 2, CELL_SIZE - 3, CELL_SIZE - 3, SELECTION_COLOR_OUTER);
    display.drawRect(cellLeft + 3, cellTop + 3, CELL_SIZE - 5, CELL_SIZE - 5, SELECTION_COLOR_INNER);
  }
}

void sudokuDrawPuzzleScreen(PicoCalc_Display& display, SudokuState* state, SudokuDifficulty difficulty) {
  uint32_t accent = sudokuDifficultyColor(difficulty);
  uint32_t legendAccent = sudokuDifficultyLegendColor(difficulty);

  display.clear();
  display.print(18, 2, "Difficulty:", DISPLAY_CYAN, 1);
  display.print(90, 2, sudokuDifficultyLabel(difficulty), accent, 1);
  drawGrid(display, accent);

  for (unsigned char row = 0; row < 9; ++row) {
    for (unsigned char col = 0; col < 9; ++col) {
      sudokuRedrawGameCell(display, state, difficulty, row, col, row == state->selectedRow && col == state->selectedCol);
    }
  }

  sudokuDrawGameFooter(display, state, legendAccent);
}

bool sudokuMoveSelection(PicoCalc_Display& display, SudokuState* state, SudokuDifficulty difficulty, int deltaRow, int deltaCol) {
  int nextRow = (int)state->selectedRow + deltaRow;
  int nextCol = (int)state->selectedCol + deltaCol;
  if (nextRow < 0 || nextRow > 8 || nextCol < 0 || nextCol > 8) {
    return false;
  }

  uint32_t accent = sudokuDifficultyColor(difficulty);
  unsigned char previousRow = state->selectedRow;
  unsigned char previousCol = state->selectedCol;
  state->selectedRow = (unsigned char)nextRow;
  state->selectedCol = (unsigned char)nextCol;
  sudokuRedrawGameCell(display, state, difficulty, previousRow, previousCol, false);
  sudokuRedrawGameCell(display, state, difficulty, state->selectedRow, state->selectedCol, true);
  return true;
}

bool sudokuSetSelectedCellValue(PicoCalc_Display& display, SudokuState* state, SudokuDifficulty difficulty, unsigned char value) {
  if (value < 1 || value > 9) return false;
  if (state->fixedCell[state->selectedRow][state->selectedCol]) return false;

  state->puzzleGrid[state->selectedRow][state->selectedCol] = value;
  state->wrongCell[state->selectedRow][state->selectedCol] = false;
  sudokuRedrawGameCell(display, state, difficulty, state->selectedRow, state->selectedCol, true);
  return true;
}

bool sudokuClearSelectedCell(PicoCalc_Display& display, SudokuState* state, SudokuDifficulty difficulty) {
  if (state->fixedCell[state->selectedRow][state->selectedCol]) return false;
  state->puzzleGrid[state->selectedRow][state->selectedCol] = 0;
  state->wrongCell[state->selectedRow][state->selectedCol] = false;
  sudokuRedrawGameCell(display, state, difficulty, state->selectedRow, state->selectedCol, true);
  return true;
}

int sudokuCheckSelectedCell(PicoCalc_Display& display, SudokuState* state, SudokuDifficulty difficulty) {
  unsigned char row = state->selectedRow;
  unsigned char col = state->selectedCol;

  if (state->fixedCell[row][col]) {
    snprintf(state->statusLine, sizeof(state->statusLine), "Given cell (locked)");
    sudokuDrawGameFooter(display, state, sudokuDifficultyLegendColor(difficulty));
    return 0;
  }

  if (state->puzzleGrid[row][col] == 0) {
    snprintf(state->statusLine, sizeof(state->statusLine), "Empty cell");
    sudokuDrawGameFooter(display, state, sudokuDifficultyLegendColor(difficulty));
    return 1;
  }

  if (state->puzzleGrid[row][col] == state->solvedGrid[row][col]) {
    state->wrongCell[row][col] = false;
    snprintf(state->statusLine, sizeof(state->statusLine), "Correct at R%dC%d", row + 1, col + 1);
    sudokuRedrawGameCell(display, state, difficulty, row, col, true);
    sudokuDrawGameFooter(display, state, sudokuDifficultyLegendColor(difficulty));
    return 2;
  }

  state->wrongCell[row][col] = true;
  snprintf(state->statusLine, sizeof(state->statusLine), "Incorrect at R%dC%d", row + 1, col + 1);
  sudokuRedrawGameCell(display, state, difficulty, row, col, true);
  sudokuDrawGameFooter(display, state, sudokuDifficultyLegendColor(difficulty));
  return 3;
}

int sudokuSolveAndScore(SudokuState* state) {
  int correctEditable = 0;
  for (unsigned char row = 0; row < 9; ++row) {
    for (unsigned char col = 0; col < 9; ++col) {
      if (!state->fixedCell[row][col] && state->puzzleGrid[row][col] == state->solvedGrid[row][col]) {
        correctEditable++;
      }
      state->wrongCell[row][col] = false;
      state->puzzleGrid[row][col] = state->solvedGrid[row][col];
      state->fixedCell[row][col] = true;
    }
  }

  int scorePercent = 100;
  if (state->editableCellCount > 0) {
    scorePercent = (correctEditable * 100) / state->editableCellCount;
  }
  snprintf(state->statusLine, sizeof(state->statusLine), "Solved. Score: %d%%", scorePercent);
  return scorePercent;
}
