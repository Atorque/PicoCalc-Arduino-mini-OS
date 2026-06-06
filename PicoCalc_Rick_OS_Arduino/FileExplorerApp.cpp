#include "FileExplorerApp.h"

#include <Arduino.h>
#include <SD.h>
#include <stdio.h>
#include <string.h>
#include "i2ckbd.h"

static const int LIST_X = 14;
static const int LIST_Y = 62;
static const int LIST_W = 292;
static const int LIST_H = 220;
static const int ROW_H = 16;
static const int MAX_VISIBLE_ROWS = 13;
static const int PATH_Y = 48;
static const int STATUS_Y = 288;

static bool isRootPath(const char* path) {
  return strcmp(path, "/") == 0;
}

static void setStatus(FileExplorerState* state, const char* text) {
  strncpy(state->statusLine, text, sizeof(state->statusLine) - 1);
  state->statusLine[sizeof(state->statusLine) - 1] = '\0';
}

static void joinPath(const char* base, const char* name, char* out, size_t outSize) {
  if (isRootPath(base)) {
    snprintf(out, outSize, "/%s", name);
  } else {
    snprintf(out, outSize, "%s/%s", base, name);
  }
}

static void parentPath(const char* path, char* out, size_t outSize) {
  strncpy(out, path, outSize - 1);
  out[outSize - 1] = '\0';

  if (isRootPath(out)) return;

  char* slash = strrchr(out, '/');
  if (slash == nullptr || slash == out) {
    strncpy(out, "/", outSize - 1);
    out[outSize - 1] = '\0';
    return;
  }

  *slash = '\0';
}

void fileExplorerReload(FileExplorerState* state) {
  state->entryCount = 0;
  state->selectedIndex = 0;
  state->scrollOffset = 0;
  state->hasError = false;

  if (!state->sdAvailable) {
    state->hasError = true;
    setStatus(state, "SD card not initialized");
    return;
  }

  File dir = SD.open(state->currentPath);
  if (!dir || !dir.isDirectory()) {
    state->hasError = true;
    setStatus(state, "Cannot open directory");
    return;
  }

  while (true) {
    File entry = dir.openNextFile();
    if (!entry) break;

    if (state->entryCount < FILE_EXPLORER_MAX_ENTRIES) {
      const char* name = entry.name();
      if (name == nullptr || name[0] == '\0') {
        strncpy(state->entries[state->entryCount], "(unnamed)", FILE_EXPLORER_NAME_LEN - 1);
      } else {
        strncpy(state->entries[state->entryCount], name, FILE_EXPLORER_NAME_LEN - 1);
      }
      state->entries[state->entryCount][FILE_EXPLORER_NAME_LEN - 1] = '\0';
      state->isDirectory[state->entryCount] = entry.isDirectory();
      state->entryCount++;
    }

    entry.close();
  }

  dir.close();

  if (state->entryCount == 0) {
    setStatus(state, "Directory is empty");
  } else {
    setStatus(state, "Enter=open  Left=up  R=refresh");
  }
}

void fileExplorerInit(FileExplorerState* state, bool sdAvailable) {
  strncpy(state->currentPath, "/", sizeof(state->currentPath) - 1);
  state->currentPath[sizeof(state->currentPath) - 1] = '\0';
  state->statusLine[0] = '\0';
  state->entryCount = 0;
  state->selectedIndex = 0;
  state->scrollOffset = 0;
  state->sdAvailable = sdAvailable;
  state->hasError = false;
  state->needsFullRedraw = true;

  fileExplorerReload(state);
}

static bool moveSelection(FileExplorerState* state, int delta) {
  if (state->entryCount <= 0) return false;

  int next = state->selectedIndex + delta;
  if (next < 0 || next >= state->entryCount) return false;

  state->selectedIndex = next;

  if (state->selectedIndex < state->scrollOffset) {
    state->scrollOffset = state->selectedIndex;
  } else if (state->selectedIndex >= state->scrollOffset + MAX_VISIBLE_ROWS) {
    state->scrollOffset = state->selectedIndex - (MAX_VISIBLE_ROWS - 1);
  }

  return true;
}

bool fileExplorerHandleKey(FileExplorerState* state, int key) {
  if (key < 0) return false;

  if (key == KEY_UP || key == 'w' || key == 'W') {
    return moveSelection(state, -1);
  }

  if (key == KEY_DOWN || key == 's' || key == 'S') {
    return moveSelection(state, 1);
  }

  if (key == 'r' || key == 'R' || key == 'g' || key == 'G') {
    fileExplorerReload(state);
    return true;
  }

  if (key == KEY_LEFT || key == KEY_BACKSPACE || key == KEY_DEL) {
    if (isRootPath(state->currentPath)) {
      setStatus(state, "Already at root");
      return true;
    }

    char newPath[96];
    parentPath(state->currentPath, newPath, sizeof(newPath));
    strncpy(state->currentPath, newPath, sizeof(state->currentPath) - 1);
    state->currentPath[sizeof(state->currentPath) - 1] = '\0';
    fileExplorerReload(state);
    return true;
  }

  if (key == KEY_ENTER || key == KEY_RIGHT) {
    if (state->entryCount <= 0) return false;

    int index = state->selectedIndex;
    if (index < 0 || index >= state->entryCount) return false;

    if (state->isDirectory[index]) {
      char newPath[96];
      joinPath(state->currentPath, state->entries[index], newPath, sizeof(newPath));
      strncpy(state->currentPath, newPath, sizeof(state->currentPath) - 1);
      state->currentPath[sizeof(state->currentPath) - 1] = '\0';
      fileExplorerReload(state);
    } else {
      char message[64];
      snprintf(message, sizeof(message), "File: %s", state->entries[index]);
      setStatus(state, message);
    }
    return true;
  }

  return false;
}

void fileExplorerDrawScreen(PicoCalc_Display& display, FileExplorerState* state) {
  if (state->needsFullRedraw) {
    display.clear();

    display.fillRect(12, 12, 296, 40, DISPLAY_BLACK);
    display.drawRect(12, 12, 296, 40, DISPLAY_CYAN);
    display.print(48, 25, "FILE EXPLORER", DISPLAY_WHITE, 2);

    display.drawRect(LIST_X, LIST_Y, LIST_W, LIST_H, DISPLAY_ORANGE);
    display.print(16, 304, "Esc/M:menu  Enter:open  Left:up  R:refresh", DISPLAY_CYAN, 1);

    state->needsFullRedraw = false;
  }

  // Partial updates only: path line, list interior, and status line.
  display.fillRect(16, PATH_Y, 292, 12, DISPLAY_BLACK);
  display.fillRect(LIST_X + 1, LIST_Y + 1, LIST_W - 2, LIST_H - 2, DISPLAY_BLACK);
  display.fillRect(16, STATUS_Y, 292, 12, DISPLAY_BLACK);

  char pathLine[96];
  snprintf(pathLine, sizeof(pathLine), "Path: %s", state->currentPath);
  display.print(16, PATH_Y, pathLine, DISPLAY_CYAN, 1);

  int rowCount = 0;
  for (int i = state->scrollOffset; i < state->entryCount && rowCount < MAX_VISIBLE_ROWS; ++i, ++rowCount) {
    int y = LIST_Y + 2 + rowCount * ROW_H;
    bool selected = (i == state->selectedIndex);

    if (selected) {
      display.fillRect(LIST_X + 1, y, LIST_W - 2, ROW_H, DISPLAY_BLUE);
    }

    char line[48];
    const char* prefix = state->isDirectory[i] ? "[D] " : "[F] ";
    snprintf(line, sizeof(line), "%s%s", prefix, state->entries[i]);
    display.print(LIST_X + 4, y + 3, line, selected ? DISPLAY_WHITE : DISPLAY_YELLOW, 1);
  }

  if (state->entryCount == 0) {
    display.print(LIST_X + 8, LIST_Y + 10, "(No entries)", DISPLAY_WHITE, 1);
  }

  if (state->hasError) {
    display.print(16, STATUS_Y, state->statusLine, DISPLAY_RED, 1);
  } else {
    display.print(16, STATUS_Y, state->statusLine, DISPLAY_GREEN, 1);
  }
}
