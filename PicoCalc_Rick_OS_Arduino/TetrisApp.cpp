#include "TetrisApp.h"

#include <Arduino.h>
#include <string.h>

#include "i2ckbd.h"

static const int BOARD_W = 10;
static const int BOARD_H = 20;
static const int CELL = 12;
static const int BOARD_X = 20;
static const int BOARD_Y = 52;

static const int HEADER_X = 12;
static const int HEADER_Y = 12;
static const int HEADER_W = 296;
static const int HEADER_H = 34;

static const int SIDEBAR_X = 168;
static const int NEXT_BOX_Y = 52;
static const int NEXT_BOX_W = 140;
static const int NEXT_BOX_H = 74;

static const int STATS_BOX_Y = 132;
static const int STATS_BOX_W = 140;
static const int STATS_BOX_H = 94;

static const int FOOTER_Y = 304;

static const uint16_t PIECE_COLORS[8] = {
  DISPLAY_BLACK,
  DISPLAY_CYAN,
  DISPLAY_YELLOW,
  DISPLAY_MAGENTA,
  DISPLAY_GREEN,
  DISPLAY_RED,
  DISPLAY_ORANGE,
  DISPLAY_WHITE
};

static const uint16_t PIECE_SHAPES[7][4] = {
  { 0x0F00, 0x2222, 0x00F0, 0x4444 }, // I
  { 0x6600, 0x6600, 0x6600, 0x6600 }, // O
  { 0x4E00, 0x4640, 0x0E40, 0x4C40 }, // T
  { 0x6C00, 0x4620, 0x06C0, 0x8C40 }, // S
  { 0xC600, 0x2640, 0x0C60, 0x4C80 }, // Z
  { 0x8E00, 0x6440, 0x0E20, 0x44C0 }, // J
  { 0x2E00, 0x4460, 0x0E80, 0xC440 }  // L
};

static bool collides(const TetrisState* state, int type, int rot, int px, int py);

static uint32_t rngState = 0x1234ABCD;
static uint32_t xorshift32() {
  rngState ^= rngState << 13;
  rngState ^= rngState >> 17;
  rngState ^= rngState << 5;
  return rngState;
}

static void addDirtyCell(TetrisState* state, int x, int y) {
  if (x < 0 || x >= BOARD_W || y < 0 || y >= BOARD_H) return;
  for (int i = 0; i < state->dirtyCount; ++i) {
    if (state->dirtyCellsX[i] == x && state->dirtyCellsY[i] == y) return;
  }
  if (state->dirtyCount < 24) {
    state->dirtyCellsX[state->dirtyCount] = (int8_t)x;
    state->dirtyCellsY[state->dirtyCount] = (int8_t)y;
    state->dirtyCount++;
  } else {
    state->boardDirtyFull = true;
  }
}

static void markPieceCells(TetrisState* state, int type, int rot, int px, int py) {
  uint16_t shape = PIECE_SHAPES[type][rot];
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      int bit = 15 - (r * 4 + c);
      if ((shape >> bit) & 1) {
        addDirtyCell(state, px + c, py + r);
      }
    }
  }
}

static int ghostYForPiece(const TetrisState* state, int type, int rot, int px, int py) {
  int gy = py;
  while (!collides(state, type, rot, px, gy + 1)) {
    gy++;
  }
  return gy;
}

static void markGhostCells(TetrisState* state, int type, int rot, int px, int py) {
  int gy = ghostYForPiece(state, type, rot, px, py);
  if (gy == py) return;
  markPieceCells(state, type, rot, px, gy);
}

static bool collides(const TetrisState* state, int type, int rot, int px, int py) {
  uint16_t shape = PIECE_SHAPES[type][rot];
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      int bit = 15 - (r * 4 + c);
      if (!((shape >> bit) & 1)) continue;

      int bx = px + c;
      int by = py + r;
      if (bx < 0 || bx >= BOARD_W || by >= BOARD_H) return true;
      if (by >= 0 && state->board[by][bx] != 0) return true;
    }
  }
  return false;
}

static void spawnPiece(TetrisState* state) {
  state->pieceType = state->nextType;
  state->nextType = (int)(xorshift32() % 7);
  state->rotation = 0;
  state->pieceX = 3;
  state->pieceY = -1;
  if (collides(state, state->pieceType, state->rotation, state->pieceX, state->pieceY)) {
    state->gameOver = true;
    state->eventGameOver = true;
  }
  state->boardDirtyFull = true;
  state->hudDirty = true;
}

static void lockPiece(TetrisState* state) {
  uint16_t shape = PIECE_SHAPES[state->pieceType][state->rotation];
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      int bit = 15 - (r * 4 + c);
      if (!((shape >> bit) & 1)) continue;
      int bx = state->pieceX + c;
      int by = state->pieceY + r;
      if (bx >= 0 && bx < BOARD_W && by >= 0 && by < BOARD_H) {
        state->board[by][bx] = (uint8_t)(state->pieceType + 1);
      }
    }
  }

  int cleared = 0;
  for (int y = BOARD_H - 1; y >= 0; --y) {
    bool full = true;
    for (int x = 0; x < BOARD_W; ++x) {
      if (state->board[y][x] == 0) {
        full = false;
        break;
      }
    }

    if (full) {
      cleared++;
      for (int yy = y; yy > 0; --yy) {
        memcpy(state->board[yy], state->board[yy - 1], BOARD_W);
      }
      memset(state->board[0], 0, BOARD_W);
      y++;
    }
  }

  if (cleared > 0) {
    state->lines = (uint16_t)(state->lines + cleared);
    state->score = (uint16_t)(state->score + (cleared * cleared) * 100);
    uint8_t newLevel = (uint8_t)(1 + state->lines / 10);
    if (newLevel > 15) newLevel = 15;
    state->level = newLevel;
    state->eventLineClear = true;
  }

  state->eventLock = true;
  spawnPiece(state);
}

static bool tryMove(TetrisState* state, int dx, int dy) {
  if (state->gameOver) return false;
  int nx = state->pieceX + dx;
  int ny = state->pieceY + dy;
  if (collides(state, state->pieceType, state->rotation, nx, ny)) return false;

  markGhostCells(state, state->pieceType, state->rotation, state->pieceX, state->pieceY);
  markPieceCells(state, state->pieceType, state->rotation, state->pieceX, state->pieceY);
  state->pieceX = nx;
  state->pieceY = ny;
  markPieceCells(state, state->pieceType, state->rotation, state->pieceX, state->pieceY);
  markGhostCells(state, state->pieceType, state->rotation, state->pieceX, state->pieceY);
  return true;
}

static bool tryRotate(TetrisState* state) {
  if (state->gameOver) return false;

  int nr = (state->rotation + 1) & 3;
  if (!collides(state, state->pieceType, nr, state->pieceX, state->pieceY)) {
    markGhostCells(state, state->pieceType, state->rotation, state->pieceX, state->pieceY);
    markPieceCells(state, state->pieceType, state->rotation, state->pieceX, state->pieceY);
    state->rotation = nr;
    markPieceCells(state, state->pieceType, state->rotation, state->pieceX, state->pieceY);
    markGhostCells(state, state->pieceType, state->rotation, state->pieceX, state->pieceY);
    return true;
  }

  // Basic wall kick
  if (!collides(state, state->pieceType, nr, state->pieceX - 1, state->pieceY)) {
    markGhostCells(state, state->pieceType, state->rotation, state->pieceX, state->pieceY);
    markPieceCells(state, state->pieceType, state->rotation, state->pieceX, state->pieceY);
    state->pieceX -= 1;
    state->rotation = nr;
    markPieceCells(state, state->pieceType, state->rotation, state->pieceX, state->pieceY);
    markGhostCells(state, state->pieceType, state->rotation, state->pieceX, state->pieceY);
    return true;
  }
  if (!collides(state, state->pieceType, nr, state->pieceX + 1, state->pieceY)) {
    markGhostCells(state, state->pieceType, state->rotation, state->pieceX, state->pieceY);
    markPieceCells(state, state->pieceType, state->rotation, state->pieceX, state->pieceY);
    state->pieceX += 1;
    state->rotation = nr;
    markPieceCells(state, state->pieceType, state->rotation, state->pieceX, state->pieceY);
    markGhostCells(state, state->pieceType, state->rotation, state->pieceX, state->pieceY);
    return true;
  }

  return false;
}

void tetrisInit(TetrisState* state) {
  memset(state, 0, sizeof(TetrisState));
  state->level = 1;
  state->nextType = (int)(xorshift32() % 7);
  state->needsFullRedraw = true;
  state->boardDirtyFull = true;
  state->hudDirty = true;
  spawnPiece(state);
  state->lastFallMs = millis();
}

bool tetrisHandleKey(TetrisState* state, int key) {
  if (key < 0) return false;

  if (state->gameOver) {
    if (key == KEY_ENTER || key == 'r' || key == 'R') {
      tetrisInit(state);
      return true;
    }
    return false;
  }

  bool changed = false;
  if (key == KEY_LEFT || key == 'a' || key == 'A') {
    changed = tryMove(state, -1, 0);
  } else if (key == KEY_RIGHT || key == 'd' || key == 'D') {
    changed = tryMove(state, 1, 0);
  } else if (key == KEY_DOWN || key == 's' || key == 'S') {
    if (tryMove(state, 0, 1)) {
      changed = true;
      state->score++;
      state->hudDirty = true;
    } else {
      lockPiece(state);
      changed = true;
    }
  } else if (key == KEY_UP || key == 'w' || key == 'W') {
    changed = tryRotate(state);
  } else if (key == KEY_ENTER || key == ' ') {
    while (tryMove(state, 0, 1)) {
      state->score += 2;
      state->hudDirty = true;
      changed = true;
    }
    lockPiece(state);
    changed = true;
  }

  return changed;
}

bool tetrisTick(TetrisState* state) {
  if (state->gameOver) return false;

  uint32_t now = millis();
  uint32_t interval = 720 - (uint32_t)(state->level - 1) * 45;
  if (interval < 85) interval = 85;

  if (now - state->lastFallMs < interval) return false;
  state->lastFallMs = now;

  if (tryMove(state, 0, 1)) {
    return true;
  }

  lockPiece(state);
  return true;
}

static void drawCell(PicoCalc_Display& display, const TetrisState* state, int x, int y) {
  if (x < 0 || x >= BOARD_W || y < 0 || y >= BOARD_H) return;

  uint8_t v = state->board[y][x];

  uint16_t shape = PIECE_SHAPES[state->pieceType][state->rotation];
  bool activeHere = false;
  for (int r = 0; r < 4; ++r) {
    for (int c = 0; c < 4; ++c) {
      int bit = 15 - (r * 4 + c);
      if (!((shape >> bit) & 1)) continue;
      int bx = state->pieceX + c;
      int by = state->pieceY + r;
      if (bx == x && by == y) {
        v = (uint8_t)(state->pieceType + 1);
        activeHere = true;
      }
    }
  }

  bool ghostHere = false;
  if (!activeHere && v == 0) {
    int gy = ghostYForPiece(state, state->pieceType, state->rotation, state->pieceX, state->pieceY);
    if (gy != state->pieceY) {
      for (int r = 0; r < 4 && !ghostHere; ++r) {
        for (int c = 0; c < 4; ++c) {
          int bit = 15 - (r * 4 + c);
          if (!((shape >> bit) & 1)) continue;
          int bx = state->pieceX + c;
          int by = gy + r;
          if (bx == x && by == y) {
            ghostHere = true;
            break;
          }
        }
      }
    }
  }

  int px = BOARD_X + x * CELL;
  int py = BOARD_Y + y * CELL;
  if (ghostHere) {
    display.fillRect(px + 1, py + 1, CELL - 2, CELL - 2, DISPLAY_BLACK);
    display.drawRect(px + 3, py + 3, CELL - 6, CELL - 6, DISPLAY_CYAN);
  } else {
    display.fillRect(px + 1, py + 1, CELL - 2, CELL - 2, PIECE_COLORS[v]);
  }
}

void tetrisDrawScreen(PicoCalc_Display& display, TetrisState* state) {
  if (state->needsFullRedraw) {
    display.clear();
    display.fillRect(HEADER_X, HEADER_Y, HEADER_W, HEADER_H, DISPLAY_BLACK);
    display.drawRect(HEADER_X, HEADER_Y, HEADER_W, HEADER_H, DISPLAY_CYAN);
    display.print(104, 24, "TETRIS", DISPLAY_WHITE, 2);

    display.drawRect(BOARD_X - 2, BOARD_Y - 2, BOARD_W * CELL + 4, BOARD_H * CELL + 4, DISPLAY_MAGENTA);
    display.drawRect(SIDEBAR_X, NEXT_BOX_Y, NEXT_BOX_W, NEXT_BOX_H, DISPLAY_CYAN);
    display.print(SIDEBAR_X + 8, NEXT_BOX_Y + 8, "NEXT", DISPLAY_CYAN, 1);

    display.drawRect(SIDEBAR_X, STATS_BOX_Y, STATS_BOX_W, STATS_BOX_H, DISPLAY_CYAN);
    display.print(SIDEBAR_X + 8, STATS_BOX_Y + 8, "SCORE", DISPLAY_CYAN, 1);
    display.print(SIDEBAR_X + 8, STATS_BOX_Y + 64, "LINES/LVL", DISPLAY_CYAN, 1);

    display.print(14, FOOTER_Y, "Arrows move/rot  Enter drop  Esc back", DISPLAY_CYAN, 1);

    state->needsFullRedraw = false;
    state->boardDirtyFull = true;
    state->hudDirty = true;
  }

  if (state->boardDirtyFull) {
    for (int y = 0; y < BOARD_H; ++y) {
      for (int x = 0; x < BOARD_W; ++x) {
        drawCell(display, state, x, y);
      }
    }
    state->boardDirtyFull = false;
    state->dirtyCount = 0;
  } else if (state->dirtyCount > 0) {
    for (int i = 0; i < state->dirtyCount; ++i) {
      drawCell(display, state, state->dirtyCellsX[i], state->dirtyCellsY[i]);
    }
    state->dirtyCount = 0;
  }

  if (state->hudDirty) {
    display.fillRect(SIDEBAR_X + 8, NEXT_BOX_Y + 22, NEXT_BOX_W - 16, NEXT_BOX_H - 30, DISPLAY_BLACK);
    uint16_t nshape = PIECE_SHAPES[state->nextType][0];
    for (int r = 0; r < 4; ++r) {
      for (int c = 0; c < 4; ++c) {
        int bit = 15 - (r * 4 + c);
        if ((nshape >> bit) & 1) {
          int px = SIDEBAR_X + 20 + c * 10;
          int py = NEXT_BOX_Y + 30 + r * 10;
          display.fillRect(px, py, 8, 8, PIECE_COLORS[state->nextType + 1]);
        }
      }
    }

    char scoreLine[24];
    snprintf(scoreLine, sizeof(scoreLine), "%u", state->score);
    display.fillRect(SIDEBAR_X + 8, STATS_BOX_Y + 22, STATS_BOX_W - 16, 16, DISPLAY_BLACK);
    display.print(SIDEBAR_X + 8, STATS_BOX_Y + 22, scoreLine, DISPLAY_YELLOW, 1);

    char ll[24];
    snprintf(ll, sizeof(ll), "%u / %u", state->lines, state->level);
    display.fillRect(SIDEBAR_X + 8, STATS_BOX_Y + 78, STATS_BOX_W - 16, 14, DISPLAY_BLACK);
    display.print(SIDEBAR_X + 8, STATS_BOX_Y + 78, ll, DISPLAY_WHITE, 1);

    if (state->gameOver) {
      display.fillRect(30, 132, 120, 48, DISPLAY_BLACK);
      display.drawRect(30, 132, 120, 48, DISPLAY_RED);
      display.print(38, 146, "GAME OVER", DISPLAY_RED, 1);
      display.print(38, 162, "Enter=restart", DISPLAY_CYAN, 1);
    }

    state->hudDirty = false;
  }
}
