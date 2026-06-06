#include "PongApp.h"

#include <Arduino.h>

#include "i2ckbd.h"

static const int FIELD_X = 12;
static const int FIELD_Y = 44;
static const int FIELD_W = 296;
static const int FIELD_H = 252;

static const int PADDLE_W = 6;
static const int PADDLE_H = 42;
static const int PADDLE_MARGIN = 10;

static const int BALL_S = 6;
static const int WIN_SCORE = 7;

static const int SCORE_BOX_X = 220;
static const int SCORE_BOX_Y = 300;
static const int SCORE_BOX_W = 88;
static const int SCORE_BOX_H = 18;

static void drawFieldChrome(PicoCalc_Display& display) {
  display.drawRect(FIELD_X, FIELD_Y, FIELD_W, FIELD_H, DISPLAY_MAGENTA);
  for (int y = FIELD_Y + 4; y < FIELD_Y + FIELD_H - 4; y += 12) {
    display.fillRect(FIELD_X + FIELD_W / 2 - 1, y, 2, 8, DISPLAY_CYAN);
  }
}

static void resetBall(PongState* state, int dir) {
  state->ballX = FIELD_X + FIELD_W / 2;
  state->ballY = FIELD_Y + FIELD_H / 2;
  state->ballVX = dir;
  state->ballVY = (dir > 0) ? 2 : -2;
  state->playfieldDirty = true;
}

void pongInit(PongState* state) {
  state->playerY = FIELD_Y + FIELD_H / 2 - PADDLE_H / 2;
  state->aiY = state->playerY;

  state->playerScore = 0;
  state->aiScore = 0;
  state->gameOver = false;

  state->lastStepMs = millis();

  state->needsFullRedraw = true;
  state->playfieldDirty = true;
  state->hudDirty = true;

  state->eventPaddleHit = false;
  state->eventWallHit = false;
  state->eventScore = false;
  state->eventGameOver = false;

  state->oldPlayerY = state->playerY;
  state->oldAiY = state->aiY;
  state->oldBallX = FIELD_X + FIELD_W / 2;
  state->oldBallY = FIELD_Y + FIELD_H / 2;

  resetBall(state, 3);
}

bool pongHandleKey(PongState* state, int key) {
  if (key < 0) return false;

  if (state->gameOver) {
    if (key == KEY_ENTER || key == 'r' || key == 'R') {
      pongInit(state);
      return true;
    }
    return false;
  }

  state->oldPlayerY = state->playerY;

  if (key == KEY_UP || key == 'w' || key == 'W') {
    state->playerY -= 10;
  } else if (key == KEY_DOWN || key == 's' || key == 'S') {
    state->playerY += 10;
  } else {
    return false;
  }

  if (state->playerY < FIELD_Y + 2) state->playerY = FIELD_Y + 2;
  int maxY = FIELD_Y + FIELD_H - 2 - PADDLE_H;
  if (state->playerY > maxY) state->playerY = maxY;

  if (state->playerY != state->oldPlayerY) {
    state->playfieldDirty = true;
    return true;
  }
  return false;
}

bool pongTick(PongState* state) {
  state->eventPaddleHit = false;
  state->eventWallHit = false;
  state->eventScore = false;
  state->eventGameOver = false;

  if (state->gameOver) return false;

  uint32_t now = millis();
  if (now - state->lastStepMs < 18) return false;
  state->lastStepMs = now;

  state->oldBallX = state->ballX;
  state->oldBallY = state->ballY;
  state->oldAiY = state->aiY;

  int targetY = state->ballY - PADDLE_H / 2;
  if (targetY > state->aiY + 3) state->aiY += 3;
  else if (targetY < state->aiY - 3) state->aiY -= 3;

  if (state->aiY < FIELD_Y + 2) state->aiY = FIELD_Y + 2;
  int maxY = FIELD_Y + FIELD_H - 2 - PADDLE_H;
  if (state->aiY > maxY) state->aiY = maxY;

  state->ballX += state->ballVX;
  state->ballY += state->ballVY;

  if (state->ballY <= FIELD_Y + 2) {
    state->ballY = FIELD_Y + 2;
    state->ballVY = -state->ballVY;
    state->eventWallHit = true;
  } else if (state->ballY >= FIELD_Y + FIELD_H - 2 - BALL_S) {
    state->ballY = FIELD_Y + FIELD_H - 2 - BALL_S;
    state->ballVY = -state->ballVY;
    state->eventWallHit = true;
  }

  int pX = FIELD_X + PADDLE_MARGIN;
  int aX = FIELD_X + FIELD_W - PADDLE_MARGIN - PADDLE_W;

  if (state->ballVX < 0 &&
      state->ballX <= pX + PADDLE_W &&
      state->ballX + BALL_S >= pX &&
      state->ballY + BALL_S >= state->playerY &&
      state->ballY <= state->playerY + PADDLE_H) {
    state->ballX = pX + PADDLE_W + 1;
    state->ballVX = -state->ballVX;
    int rel = (state->ballY + BALL_S / 2) - (state->playerY + PADDLE_H / 2);
    state->ballVY = rel / 6;
    if (state->ballVY == 0) state->ballVY = (rel >= 0) ? 1 : -1;
    state->eventPaddleHit = true;
  }

  if (state->ballVX > 0 &&
      state->ballX + BALL_S >= aX &&
      state->ballX <= aX + PADDLE_W &&
      state->ballY + BALL_S >= state->aiY &&
      state->ballY <= state->aiY + PADDLE_H) {
    state->ballX = aX - BALL_S - 1;
    state->ballVX = -state->ballVX;
    int rel = (state->ballY + BALL_S / 2) - (state->aiY + PADDLE_H / 2);
    state->ballVY = rel / 6;
    if (state->ballVY == 0) state->ballVY = (rel >= 0) ? 1 : -1;
    state->eventPaddleHit = true;
  }

  if (state->ballX < FIELD_X) {
    state->aiScore++;
    state->hudDirty = true;
    state->eventScore = true;
    if (state->aiScore >= WIN_SCORE) {
      state->gameOver = true;
      state->eventGameOver = true;
    } else {
      resetBall(state, 3);
    }
  } else if (state->ballX > FIELD_X + FIELD_W) {
    state->playerScore++;
    state->hudDirty = true;
    state->eventScore = true;
    if (state->playerScore >= WIN_SCORE) {
      state->gameOver = true;
      state->eventGameOver = true;
    } else {
      resetBall(state, -3);
    }
  }

  state->playfieldDirty = true;
  return true;
}

static void drawPaddle(PicoCalc_Display& display, int x, int y, uint16_t color) {
  display.fillRect(x, y, PADDLE_W, PADDLE_H, color);
}

static void drawBall(PicoCalc_Display& display, int x, int y, uint16_t color) {
  display.fillRect(x, y, BALL_S, BALL_S, color);
}

void pongDrawScreen(PicoCalc_Display& display, PongState* state) {
  int pX = FIELD_X + PADDLE_MARGIN;
  int aX = FIELD_X + FIELD_W - PADDLE_MARGIN - PADDLE_W;

  if (state->needsFullRedraw) {
    display.clear();
    display.fillRect(12, 12, 296, 28, DISPLAY_BLACK);
    display.drawRect(12, 12, 296, 28, DISPLAY_CYAN);
    display.print(118, 20, "PONG", DISPLAY_WHITE, 2);

    drawFieldChrome(display);

    display.print(14, 304, "W/S move  Enter restart  Esc back", DISPLAY_CYAN, 1);
    display.drawRect(SCORE_BOX_X, SCORE_BOX_Y, SCORE_BOX_W, SCORE_BOX_H, DISPLAY_CYAN);

    state->needsFullRedraw = false;
    state->playfieldDirty = true;
    state->hudDirty = true;
  }

  if (state->playfieldDirty) {
    drawPaddle(display, pX, state->oldPlayerY, DISPLAY_BLACK);
    drawPaddle(display, aX, state->oldAiY, DISPLAY_BLACK);
    drawBall(display, state->oldBallX, state->oldBallY, DISPLAY_BLACK);

    // Restore field visuals that may have been overdrawn by object erase/motion.
    drawFieldChrome(display);

    drawPaddle(display, pX, state->playerY, DISPLAY_GREEN);
    drawPaddle(display, aX, state->aiY, DISPLAY_ORANGE);
    drawBall(display, state->ballX, state->ballY, DISPLAY_WHITE);

    state->playfieldDirty = false;
  }

  if (state->hudDirty) {
    display.fillRect(SCORE_BOX_X + 2, SCORE_BOX_Y + 2, SCORE_BOX_W - 4, SCORE_BOX_H - 4, DISPLAY_BLACK);
    char scoreLine[32];
    snprintf(scoreLine, sizeof(scoreLine), "%u : %u", state->playerScore, state->aiScore);
    display.print(SCORE_BOX_X + 14, SCORE_BOX_Y + 5, scoreLine, DISPLAY_YELLOW, 1);

    if (state->gameOver) {
      display.fillRect(84, 132, 152, 54, DISPLAY_BLACK);
      display.drawRect(84, 132, 152, 54, DISPLAY_RED);
      if (state->playerScore > state->aiScore) {
        display.print(118, 148, "YOU WIN", DISPLAY_GREEN, 2);
      } else {
        display.print(110, 148, "CPU WINS", DISPLAY_RED, 2);
      }
      display.print(96, 170, "Enter to restart", DISPLAY_CYAN, 1);
    }

    state->hudDirty = false;
  }
}
