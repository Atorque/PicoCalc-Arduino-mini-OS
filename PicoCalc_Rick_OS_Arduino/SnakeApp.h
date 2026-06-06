#ifndef SNAKE_APP_H
#define SNAKE_APP_H

#include <stdint.h>
#include "PicoCalc_Display.h"

struct SnakeState {
  int8_t bodyX[220];
  int8_t bodyY[220];
  int length;

  int8_t dirX;
  int8_t dirY;
  int8_t nextDirX;
  int8_t nextDirY;

  int8_t foodX;
  int8_t foodY;

  bool occupied[20][24];

  bool gameOver;
  uint16_t score;
  uint8_t level;
  uint32_t lastStepMs;

  bool needsFullRedraw;
  bool boardDirtyFull;
  bool hudDirty;

  uint8_t dirtyCount;
  int8_t dirtyX[40];
  int8_t dirtyY[40];

  bool eventEat;
  bool eventGameOver;
};

void snakeInit(SnakeState* state);
bool snakeHandleKey(SnakeState* state, int key);
bool snakeTick(SnakeState* state);
void snakeDrawScreen(PicoCalc_Display& display, SnakeState* state);

#endif // SNAKE_APP_H
