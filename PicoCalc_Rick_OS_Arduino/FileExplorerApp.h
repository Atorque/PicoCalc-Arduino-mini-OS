#ifndef FILE_EXPLORER_APP_H
#define FILE_EXPLORER_APP_H

#include "PicoCalc_Display.h"

#define FILE_EXPLORER_MAX_ENTRIES 48
#define FILE_EXPLORER_NAME_LEN 32

struct FileExplorerState {
  char currentPath[96];
  char statusLine[64];
  char entries[FILE_EXPLORER_MAX_ENTRIES][FILE_EXPLORER_NAME_LEN];
  bool isDirectory[FILE_EXPLORER_MAX_ENTRIES];
  int entryCount;
  int selectedIndex;
  int scrollOffset;
  bool sdAvailable;
  bool hasError;
  bool needsFullRedraw;
};

void fileExplorerInit(FileExplorerState* state, bool sdAvailable);
void fileExplorerReload(FileExplorerState* state);
bool fileExplorerHandleKey(FileExplorerState* state, int key);
void fileExplorerDrawScreen(PicoCalc_Display& display, FileExplorerState* state);

#endif // FILE_EXPLORER_APP_H
