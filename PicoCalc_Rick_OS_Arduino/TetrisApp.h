#ifndef TETRIS_APP_H
#define TETRIS_APP_H

#include <stdint.h>
#include "PicoCalc_Display.h"

struct TetrisState {
  uint8_t board[20][10];

  int pieceType;
  int rotation;
  int pieceX;
  int pieceY;
  int nextType;

  bool gameOver;
  uint16_t score;
  uint16_t lines;
  uint8_t level;

  uint32_t lastFallMs;

  bool needsFullRedraw;
  bool boardDirtyFull;
  bool hudDirty;

  uint8_t dirtyCount;
  int8_t dirtyCellsX[24];
  int8_t dirtyCellsY[24];

  bool eventLineClear;
  bool eventLock;
  bool eventGameOver;
};

void tetrisInit(TetrisState* state);
bool tetrisHandleKey(TetrisState* state, int key);
bool tetrisTick(TetrisState* state);
void tetrisDrawScreen(PicoCalc_Display& display, TetrisState* state);

#endif // TETRIS_APP_H
