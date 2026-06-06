#ifndef NOTEPAD_APP_H
#define NOTEPAD_APP_H

#include "PicoCalc_Display.h"

#define NOTEPAD_TEXT_MAX 1536

struct NotepadState {
  char text[NOTEPAD_TEXT_MAX];
  int length;
  int cursorIndex;
  int firstVisibleLine;
  bool sdAvailable;
  bool hasError;
  bool needsFullRedraw;
  bool menuOpen;
  int menuIndex;
  bool requestExit;
  bool filenameEditMode;
  bool filePickerMode;
  int filePickerIndex;
  int filePickerCount;
  char filePickerItems[16][24];
  char filename[24];
  char filePath[40];
  char statusLine[64];
};

void notepadInit(NotepadState* state, bool sdAvailable);
bool notepadHandleKey(NotepadState* state, int key);
void notepadDrawScreen(PicoCalc_Display& display, NotepadState* state);

#endif // NOTEPAD_APP_H
