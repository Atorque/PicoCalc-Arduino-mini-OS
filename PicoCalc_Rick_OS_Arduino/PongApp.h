#ifndef PONG_APP_H
#define PONG_APP_H

#include <stdint.h>
#include "PicoCalc_Display.h"

struct PongState {
  int playerY;
  int aiY;
  int ballX;
  int ballY;
  int ballVX;
  int ballVY;

  uint8_t playerScore;
  uint8_t aiScore;
  bool gameOver;

  uint32_t lastStepMs;

  bool needsFullRedraw;
  bool playfieldDirty;
  bool hudDirty;

  bool eventPaddleHit;
  bool eventWallHit;
  bool eventScore;
  bool eventGameOver;

  int oldPlayerY;
  int oldAiY;
  int oldBallX;
  int oldBallY;
};

void pongInit(PongState* state);
bool pongHandleKey(PongState* state, int key);
bool pongTick(PongState* state);
void pongDrawScreen(PicoCalc_Display& display, PongState* state);

#endif // PONG_APP_H
