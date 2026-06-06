#ifndef SUDOKU_GAME_H
#define SUDOKU_GAME_H

#include "PicoCalc_Display.h"

enum SudokuDifficulty {
  SUDOKU_DIFFICULTY_EASY = 0,
  SUDOKU_DIFFICULTY_MEDIUM = 1,
  SUDOKU_DIFFICULTY_HARD = 2,
  SUDOKU_DIFFICULTY_EXPERT = 3,
  SUDOKU_DIFFICULTY_MASTER = 4,
  SUDOKU_DIFFICULTY_EXTREME = 5
};

struct SudokuState {
  unsigned char solvedGrid[9][9];
  unsigned char puzzleGrid[9][9];
  bool fixedCell[9][9];
  bool wrongCell[9][9];
  unsigned char selectedRow;
  unsigned char selectedCol;
  int editableCellCount;
  char statusLine[40];
};

const char* sudokuDifficultyLabel(SudokuDifficulty difficulty);
const char* sudokuChallengeLabel(SudokuDifficulty difficulty);
uint32_t sudokuDifficultyColor(SudokuDifficulty difficulty);
uint32_t sudokuDifficultyLegendColor(SudokuDifficulty difficulty);

void sudokuInitState(SudokuState* state);
void sudokuStartNewPuzzle(SudokuState* state, SudokuDifficulty difficulty);

void sudokuDrawIntroScreen(PicoCalc_Display& display, SudokuDifficulty difficulty);
void sudokuDrawIntroOptionRow(PicoCalc_Display& display, SudokuDifficulty level, SudokuDifficulty currentDifficulty, bool selected);
void sudokuDrawIntroChallengeLegend(PicoCalc_Display& display, SudokuDifficulty difficulty);
void sudokuPlayIntroToGameTransition(PicoCalc_Display& display, SudokuDifficulty difficulty);

void sudokuDrawPuzzleScreen(PicoCalc_Display& display, SudokuState* state, SudokuDifficulty difficulty);
void sudokuDrawGameFooter(PicoCalc_Display& display, const SudokuState* state, uint32_t accent);
void sudokuRedrawGameCell(PicoCalc_Display& display, SudokuState* state, SudokuDifficulty difficulty, unsigned char row, unsigned char col, bool selected);

bool sudokuMoveSelection(PicoCalc_Display& display, SudokuState* state, SudokuDifficulty difficulty, int deltaRow, int deltaCol);
bool sudokuSetSelectedCellValue(PicoCalc_Display& display, SudokuState* state, SudokuDifficulty difficulty, unsigned char value);
bool sudokuClearSelectedCell(PicoCalc_Display& display, SudokuState* state, SudokuDifficulty difficulty);

// Returns 0 for locked, 1 for empty, 2 for correct, 3 for incorrect.
int sudokuCheckSelectedCell(PicoCalc_Display& display, SudokuState* state, SudokuDifficulty difficulty);
int sudokuSolveAndScore(SudokuState* state);

#endif // SUDOKU_GAME_H
