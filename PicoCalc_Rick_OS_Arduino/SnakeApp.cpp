#include "SnakeApp.h"

#include <Arduino.h>
#include <string.h>

#include "i2ckbd.h"

static const int GRID_W = 24;
static const int GRID_H = 20;
static const int CELL = 12;
static const int BOARD_X = 16;
static const int BOARD_Y = 52;

static uint32_t rngStateSnake = 0xACED1234;
static uint32_t snakeRand() {
  rngStateSnake ^= rngStateSnake << 13;
  rngStateSnake ^= rngStateSnake >> 17;
  rngStateSnake ^= rngStateSnake << 5;
  return rngStateSnake;
}

static void addDirty(SnakeState* state, int x, int y) {
  if (x < 0 || x >= GRID_W || y < 0 || y >= GRID_H) return;
  for (int i = 0; i < state->dirtyCount; ++i) {
    if (state->dirtyX[i] == x && state->dirtyY[i] == y) return;
  }
  if (state->dirtyCount < 40) {
    state->dirtyX[state->dirtyCount] = (int8_t)x;
    state->dirtyY[state->dirtyCount] = (int8_t)y;
    state->dirtyCount++;
  } else {
    state->boardDirtyFull = true;
  }
}

static void spawnFood(SnakeState* state) {
  int freeCount = GRID_W * GRID_H - state->length;
  if (freeCount <= 0) {
    state->foodX = -1;
    state->foodY = -1;
    return;
  }

  int target = (int)(snakeRand() % (uint32_t)freeCount);
  int idx = 0;
  for (int y = 0; y < GRID_H; ++y) {
    for (int x = 0; x < GRID_W; ++x) {
      if (!state->occupied[y][x]) {
        if (idx == target) {
          state->foodX = (int8_t)x;
          state->foodY = (int8_t)y;
          addDirty(state, x, y);
          return;
        }
        idx++;
      }
    }
  }
}

void snakeInit(SnakeState* state) {
  memset(state, 0, sizeof(SnakeState));

  state->length = 3;
  state->bodyX[0] = 12;
  state->bodyY[0] = 10;
  state->bodyX[1] = 11;
  state->bodyY[1] = 10;
  state->bodyX[2] = 10;
  state->bodyY[2] = 10;

  state->dirX = 1;
  state->dirY = 0;
  state->nextDirX = 1;
  state->nextDirY = 0;

  state->occupied[10][12] = true;
  state->occupied[10][11] = true;
  state->occupied[10][10] = true;

  state->foodX = -1;
  state->foodY = -1;
  spawnFood(state);

  state->gameOver = false;
  state->score = 0;
  state->level = 1;
  state->lastStepMs = millis();

  state->needsFullRedraw = true;
  state->boardDirtyFull = true;
  state->hudDirty = true;
  state->dirtyCount = 0;

  state->eventEat = false;
  state->eventGameOver = false;
}

bool snakeHandleKey(SnakeState* state, int key) {
  if (key < 0) return false;

  if (state->gameOver) {
    if (key == KEY_ENTER || key == 'r' || key == 'R') {
      snakeInit(state);
      return true;
    }
    return false;
  }

  int8_t nx = state->nextDirX;
  int8_t ny = state->nextDirY;

  if (key == KEY_UP || key == 'w' || key == 'W') {
    nx = 0;
    ny = -1;
  } else if (key == KEY_DOWN || key == 's' || key == 'S') {
    nx = 0;
    ny = 1;
  } else if (key == KEY_LEFT || key == 'a' || key == 'A') {
    nx = -1;
    ny = 0;
  } else if (key == KEY_RIGHT || key == 'd' || key == 'D') {
    nx = 1;
    ny = 0;
  } else {
    return false;
  }

  if (state->length > 1) {
    if (nx == -state->dirX && ny == -state->dirY) {
      return false;
    }
  }

  if (state->nextDirX == nx && state->nextDirY == ny) {
    return false;
  }

  state->nextDirX = nx;
  state->nextDirY = ny;
  return true;
}

bool snakeTick(SnakeState* state) {
  state->eventEat = false;
  state->eventGameOver = false;

  if (state->gameOver) return false;

  uint32_t now = millis();
  uint32_t interval = 220 - (uint32_t)(state->level - 1) * 12;
  if (interval < 70) interval = 70;
  if (now - state->lastStepMs < interval) return false;

  state->lastStepMs = now;
  state->dirX = state->nextDirX;
  state->dirY = state->nextDirY;

  int headX = state->bodyX[0];
  int headY = state->bodyY[0];
  int nx = headX + state->dirX;
  int ny = headY + state->dirY;

  bool eat = (nx == state->foodX && ny == state->foodY);

  int tailX = state->bodyX[state->length - 1];
  int tailY = state->bodyY[state->length - 1];

  if (nx < 0 || nx >= GRID_W || ny < 0 || ny >= GRID_H) {
    state->gameOver = true;
    state->eventGameOver = true;
    state->hudDirty = true;
    return true;
  }

  bool hitsBody = state->occupied[ny][nx];
  if (hitsBody && !( !eat && nx == tailX && ny == tailY )) {
    state->gameOver = true;
    state->eventGameOver = true;
    state->hudDirty = true;
    return true;
  }

  if (!eat) {
    state->occupied[tailY][tailX] = false;
    addDirty(state, tailX, tailY);
  }

  int newLen = state->length + (eat ? 1 : 0);
  if (newLen > 220) newLen = 220;

  for (int i = newLen - 1; i > 0; --i) {
    state->bodyX[i] = state->bodyX[i - 1];
    state->bodyY[i] = state->bodyY[i - 1];
  }

  state->bodyX[0] = (int8_t)nx;
  state->bodyY[0] = (int8_t)ny;
  state->occupied[ny][nx] = true;

  addDirty(state, headX, headY);
  addDirty(state, nx, ny);

  if (eat && state->length < 220) {
    state->length++;
    state->score += 10;
    uint8_t lvl = (uint8_t)(1 + state->score / 80);
    if (lvl > 12) lvl = 12;
    state->level = lvl;
    spawnFood(state);
    state->eventEat = true;
    state->hudDirty = true;
  }

  return true;
}

static void drawCell(PicoCalc_Display& display, const SnakeState* state, int x, int y) {
  if (x < 0 || x >= GRID_W || y < 0 || y >= GRID_H) return;

  int px = BOARD_X + x * CELL;
  int py = BOARD_Y + y * CELL;

  if (x == state->foodX && y == state->foodY) {
    display.fillRect(px + 1, py + 1, CELL - 2, CELL - 2, DISPLAY_RED);
    return;
  }

  if (state->bodyX[0] == x && state->bodyY[0] == y) {
    display.fillRect(px + 1, py + 1, CELL - 2, CELL - 2, DISPLAY_GREEN);
    return;
  }

  if (state->occupied[y][x]) {
    display.fillRect(px + 1, py + 1, CELL - 2, CELL - 2, DISPLAY_CYAN);
    return;
  }

  display.fillRect(px + 1, py + 1, CELL - 2, CELL - 2, DISPLAY_BLACK);
}

void snakeDrawScreen(PicoCalc_Display& display, SnakeState* state) {
  if (state->needsFullRedraw) {
    display.clear();
    display.fillRect(12, 12, 296, 34, DISPLAY_BLACK);
    display.drawRect(12, 12, 296, 34, DISPLAY_CYAN);
    display.print(110, 24, "SNAKE", DISPLAY_WHITE, 2);

    display.drawRect(BOARD_X - 2, BOARD_Y - 2, GRID_W * CELL + 4, GRID_H * CELL + 4, DISPLAY_MAGENTA);

    display.print(14, 304, "Arrows move  Enter restart  Esc back", DISPLAY_CYAN, 1);

    state->needsFullRedraw = false;
    state->boardDirtyFull = true;
    state->hudDirty = true;
  }

  if (state->boardDirtyFull) {
    for (int y = 0; y < GRID_H; ++y) {
      for (int x = 0; x < GRID_W; ++x) {
        drawCell(display, state, x, y);
      }
    }
    state->boardDirtyFull = false;
    state->dirtyCount = 0;
  } else if (state->dirtyCount > 0) {
    for (int i = 0; i < state->dirtyCount; ++i) {
      drawCell(display, state, state->dirtyX[i], state->dirtyY[i]);
    }
    state->dirtyCount = 0;
  }

  if (state->hudDirty) {
    char line[64];
    display.fillRect(16, 294, 288, 10, DISPLAY_BLACK);
    snprintf(line, sizeof(line), "Score:%u  Len:%d  Lvl:%u", state->score, state->length, state->level);
    display.print(16, 294, line, DISPLAY_YELLOW, 1);

    if (state->gameOver) {
      display.fillRect(78, 134, 164, 54, DISPLAY_BLACK);
      display.drawRect(78, 134, 164, 54, DISPLAY_RED);
      display.print(94, 148, "GAME OVER", DISPLAY_RED, 2);
      display.print(90, 170, "Enter to restart", DISPLAY_CYAN, 1);
    }

    state->hudDirty = false;
  }
}
