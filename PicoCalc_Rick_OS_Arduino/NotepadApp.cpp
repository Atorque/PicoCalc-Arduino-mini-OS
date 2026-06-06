#include "NotepadApp.h"

#include <Arduino.h>
#include <SD.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "i2ckbd.h"

static const int TEXT_X = 14;
static const int TEXT_Y = 64;
static const int TEXT_W = 292;
static const int TEXT_H = 214;

static const int FILE_Y = 48;
static const int STATUS_Y = 288;
static const int LINES_VISIBLE = 14;
static const int CHARS_PER_LINE = 40;

static const int CHAR_W = 6;
static const int CHAR_H = 15;

static const char* MENU_ITEMS[4] = {
  "Exit",
  "Save",
  "Name",
  "Open"
};

#define NOTEPAD_MAX_VISUAL_LINES 1700

struct NotepadLayout {
  int lineCount;
  int starts[NOTEPAD_MAX_VISUAL_LINES];
  int lengths[NOTEPAD_MAX_VISUAL_LINES];
};

static void setStatus(NotepadState* state, const char* text, bool isError) {
  strncpy(state->statusLine, text, sizeof(state->statusLine) - 1);
  state->statusLine[sizeof(state->statusLine) - 1] = '\0';
  state->hasError = isError;
}

static void rebuildPath(NotepadState* state) {
  snprintf(state->filePath, sizeof(state->filePath), "/%s", state->filename);
}

static bool hasTxtExtension(const char* name) {
  if (!name) return false;
  size_t len = strlen(name);
  if (len < 4) return false;
  const char* ext = name + len - 4;
  return (tolower((unsigned char)ext[0]) == '.') &&
         (tolower((unsigned char)ext[1]) == 't') &&
         (tolower((unsigned char)ext[2]) == 'x') &&
         (tolower((unsigned char)ext[3]) == 't');
}

static void refreshTxtFilePicker(NotepadState* state) {
  state->filePickerCount = 0;
  state->filePickerIndex = 0;

  if (!state->sdAvailable) {
    return;
  }

  File root = SD.open("/");
  if (!root) {
    return;
  }

  while (true) {
    File entry = root.openNextFile();
    if (!entry) {
      break;
    }

    if (!entry.isDirectory()) {
      const char* entryName = entry.name();
      if (hasTxtExtension(entryName) && state->filePickerCount < 16) {
        strncpy(state->filePickerItems[state->filePickerCount], entryName, sizeof(state->filePickerItems[0]) - 1);
        state->filePickerItems[state->filePickerCount][sizeof(state->filePickerItems[0]) - 1] = '\0';
        state->filePickerCount++;
      }
    }

    entry.close();
  }

  root.close();
}

static bool isAllowedFilenameChar(char c) {
  if (c >= 'a' && c <= 'z') return true;
  if (c >= 'A' && c <= 'Z') return true;
  if (c >= '0' && c <= '9') return true;
  if (c == '_' || c == '-' || c == '.') return true;
  return false;
}

static void setDefaultFilename(NotepadState* state) {
  strncpy(state->filename, "NOTEPAD.TXT", sizeof(state->filename) - 1);
  state->filename[sizeof(state->filename) - 1] = '\0';
  rebuildPath(state);
}

static void buildLayout(const NotepadState* state, NotepadLayout* layout) {
  layout->lineCount = 0;

  int lineStart = 0;
  int col = 0;
  for (int i = 0; i < state->length; ++i) {
    char c = state->text[i];
    if (c == '\n') {
      if (layout->lineCount < NOTEPAD_MAX_VISUAL_LINES) {
        layout->starts[layout->lineCount] = lineStart;
        layout->lengths[layout->lineCount] = col;
        layout->lineCount++;
      }
      lineStart = i + 1;
      col = 0;
    } else {
      col++;
      if (col >= CHARS_PER_LINE) {
        if (layout->lineCount < NOTEPAD_MAX_VISUAL_LINES) {
          layout->starts[layout->lineCount] = lineStart;
          layout->lengths[layout->lineCount] = col;
          layout->lineCount++;
        }
        lineStart = i + 1;
        col = 0;
      }
    }
  }

  if (layout->lineCount < NOTEPAD_MAX_VISUAL_LINES) {
    layout->starts[layout->lineCount] = lineStart;
    layout->lengths[layout->lineCount] = col;
    layout->lineCount++;
  }
}

static void clampCursor(NotepadState* state) {
  if (state->cursorIndex < 0) state->cursorIndex = 0;
  if (state->cursorIndex > state->length) state->cursorIndex = state->length;
}

static void cursorToRowCol(const NotepadLayout* layout, int cursorIndex, int* row, int* col) {
  *row = 0;
  *col = 0;

  for (int i = 0; i < layout->lineCount; ++i) {
    int start = layout->starts[i];
    int lineLen = layout->lengths[i];
    int lineEnd = start + lineLen;
    if (cursorIndex >= start && cursorIndex <= lineEnd) {
      *row = i;
      *col = cursorIndex - start;
      return;
    }
  }

  int last = layout->lineCount - 1;
  if (last >= 0) {
    *row = last;
    *col = layout->lengths[last];
  }
}

static int rowColToCursor(const NotepadLayout* layout, int row, int col) {
  if (layout->lineCount <= 0) return 0;
  if (row < 0) row = 0;
  if (row >= layout->lineCount) row = layout->lineCount - 1;

  int start = layout->starts[row];
  int len = layout->lengths[row];
  if (col < 0) col = 0;
  if (col > len) col = len;
  return start + col;
}

static void ensureCursorVisible(NotepadState* state, const NotepadLayout* layout) {
  int row = 0;
  int col = 0;
  cursorToRowCol(layout, state->cursorIndex, &row, &col);

  if (row < state->firstVisibleLine) {
    state->firstVisibleLine = row;
  } else if (row >= state->firstVisibleLine + LINES_VISIBLE) {
    state->firstVisibleLine = row - (LINES_VISIBLE - 1);
  }

  if (state->firstVisibleLine < 0) state->firstVisibleLine = 0;
}

static bool insertCharAtCursor(NotepadState* state, char c) {
  if (state->length >= NOTEPAD_TEXT_MAX - 1) {
    setStatus(state, "Buffer full", true);
    return false;
  }

  memmove(&state->text[state->cursorIndex + 1], &state->text[state->cursorIndex], (size_t)(state->length - state->cursorIndex + 1));
  state->text[state->cursorIndex] = c;
  state->cursorIndex++;
  state->length++;
  return true;
}

static bool deleteBeforeCursor(NotepadState* state) {
  if (state->cursorIndex <= 0) return false;

  memmove(&state->text[state->cursorIndex - 1], &state->text[state->cursorIndex], (size_t)(state->length - state->cursorIndex + 1));
  state->cursorIndex--;
  state->length--;
  return true;
}

static bool deleteAtCursor(NotepadState* state) {
  if (state->cursorIndex >= state->length) return false;

  memmove(&state->text[state->cursorIndex], &state->text[state->cursorIndex + 1], (size_t)(state->length - state->cursorIndex));
  state->length--;
  return true;
}

static bool loadFromSd(NotepadState* state) {
  if (!state->sdAvailable) {
    setStatus(state, "SD unavailable", true);
    return false;
  }

  File f = SD.open(state->filePath, FILE_READ);
  if (!f) {
    state->length = 0;
    state->cursorIndex = 0;
    state->text[0] = '\0';
    setStatus(state, "File missing, new note", false);
    return true;
  }

  state->length = 0;
  while (f.available() && state->length < NOTEPAD_TEXT_MAX - 1) {
    state->text[state->length++] = (char)f.read();
  }
  state->text[state->length] = '\0';
  state->cursorIndex = state->length;
  f.close();

  char msg[64];
  snprintf(msg, sizeof(msg), "Loaded %s", state->filename);
  setStatus(state, msg, false);
  return true;
}

static bool saveToSd(NotepadState* state) {
  if (!state->sdAvailable) {
    setStatus(state, "SD unavailable", true);
    return false;
  }

  SD.remove(state->filePath);
  File f = SD.open(state->filePath, FILE_WRITE);
  if (!f) {
    setStatus(state, "Save failed", true);
    return false;
  }

  f.write((const uint8_t*)state->text, (size_t)state->length);
  f.close();

  char msg[64];
  snprintf(msg, sizeof(msg), "Saved %s", state->filename);
  setStatus(state, msg, false);
  return true;
}

void notepadInit(NotepadState* state, bool sdAvailable) {
  state->length = 0;
  state->cursorIndex = 0;
  state->firstVisibleLine = 0;
  state->text[0] = '\0';
  state->sdAvailable = sdAvailable;
  state->hasError = false;
  state->needsFullRedraw = true;
  state->menuOpen = false;
  state->menuIndex = 0;
  state->requestExit = false;
  state->filenameEditMode = false;
  state->filePickerMode = false;
  state->filePickerIndex = 0;
  state->filePickerCount = 0;
  setDefaultFilename(state);
  setStatus(state, "Esc menu: Exit Save Name Open", false);
  loadFromSd(state);
}

static bool handleFilenameEdit(NotepadState* state, int key) {
  if (key == KEY_ENTER) {
    if (state->filename[0] == '\0') {
      setDefaultFilename(state);
    }
    rebuildPath(state);
    state->filenameEditMode = false;
    setStatus(state, "Filename set. Use Esc menu for Open/Save", false);
    return true;
  }

  if (key == KEY_ESC) {
    state->filenameEditMode = false;
    setStatus(state, "Filename edit cancelled", false);
    return true;
  }

  if (key == KEY_BACKSPACE || key == KEY_DEL) {
    size_t len = strlen(state->filename);
    if (len > 0) {
      state->filename[len - 1] = '\0';
      rebuildPath(state);
    }
    return true;
  }

  if (key >= 32 && key <= 126) {
    char c = (char)key;
    if (!isAllowedFilenameChar(c)) return false;
    size_t len = strlen(state->filename);
    if (len >= sizeof(state->filename) - 1) return false;
    state->filename[len] = c;
    state->filename[len + 1] = '\0';
    rebuildPath(state);
    return true;
  }

  return false;
}

bool notepadHandleKey(NotepadState* state, int key) {
  if (key < 0) return false;

  if (state->filePickerMode) {
    if (key == KEY_ESC) {
      state->filePickerMode = false;
      setStatus(state, "Open cancelled", false);
      return true;
    }

    if (key == KEY_UP || key == 'w' || key == 'W') {
      if (state->filePickerIndex > 0) {
        state->filePickerIndex--;
        return true;
      }
      return false;
    }

    if (key == KEY_DOWN || key == 's' || key == 'S') {
      if (state->filePickerIndex + 1 < state->filePickerCount) {
        state->filePickerIndex++;
        return true;
      }
      return false;
    }

    if (key == KEY_ENTER || key == KEY_RIGHT) {
      if (state->filePickerCount <= 0) {
        return false;
      }

      strncpy(state->filename, state->filePickerItems[state->filePickerIndex], sizeof(state->filename) - 1);
      state->filename[sizeof(state->filename) - 1] = '\0';
      rebuildPath(state);
      state->filePickerMode = false;
      return loadFromSd(state);
    }

    return false;
  }

  if (state->menuOpen) {
    if (key == KEY_ESC) {
      state->menuOpen = false;
      setStatus(state, "Esc menu closed", false);
      return true;
    }

    if (key == KEY_UP || key == 'w' || key == 'W') {
      if (state->menuIndex > 0) {
        state->menuIndex--;
        return true;
      }
      return false;
    }

    if (key == KEY_DOWN || key == 's' || key == 'S') {
      if (state->menuIndex < 3) {
        state->menuIndex++;
        return true;
      }
      return false;
    }

    if (key == KEY_ENTER || key == KEY_RIGHT) {
      int selected = state->menuIndex;
      state->menuOpen = false;
      if (selected == 0) {
        state->requestExit = true;
        setStatus(state, "Exiting Notepad", false);
        return true;
      }
      if (selected == 1) {
        return saveToSd(state);
      }
      if (selected == 2) {
        state->filenameEditMode = true;
        setStatus(state, "Filename mode: Enter apply", false);
        return true;
      }
      refreshTxtFilePicker(state);
      if (state->filePickerCount <= 0) {
        setStatus(state, "No .txt files on SD", true);
        return true;
      }
      state->filePickerMode = true;
      setStatus(state, "Open: select .txt and Enter", false);
      return true;
    }

    return false;
  }

  if (state->filenameEditMode) {
    return handleFilenameEdit(state, key);
  }

  if (key == KEY_ESC) {
    state->menuOpen = true;
    state->menuIndex = 0;
    setStatus(state, "Menu: Exit Save Name Open", false);
    return true;
  }

  if (key == KEY_LEFT) {
    if (state->cursorIndex > 0) {
      state->cursorIndex--;
      return true;
    }
    return false;
  }

  if (key == KEY_RIGHT) {
    if (state->cursorIndex < state->length) {
      state->cursorIndex++;
      return true;
    }
    return false;
  }

  if (key == KEY_UP || key == KEY_DOWN) {
    NotepadLayout layout;
    buildLayout(state, &layout);
    int row = 0;
    int col = 0;
    cursorToRowCol(&layout, state->cursorIndex, &row, &col);
    int newRow = (key == KEY_UP) ? (row - 1) : (row + 1);
    int newCursor = rowColToCursor(&layout, newRow, col);
    if (newCursor != state->cursorIndex) {
      state->cursorIndex = newCursor;
      return true;
    }
    return false;
  }

  if (key == KEY_BACKSPACE) {
    bool ok = deleteBeforeCursor(state);
    if (ok) setStatus(state, "Editing", false);
    return ok;
  }

  if (key == KEY_DEL) {
    bool ok = deleteAtCursor(state);
    if (ok) setStatus(state, "Editing", false);
    return ok;
  }

  if (key == KEY_ENTER) {
    bool ok = insertCharAtCursor(state, '\n');
    if (ok) setStatus(state, "Editing", false);
    return ok;
  }

  if (key >= 32 && key <= 126) {
    bool ok = insertCharAtCursor(state, (char)key);
    if (ok) setStatus(state, "Editing", false);
    return ok;
  }

  return false;
}

void notepadDrawScreen(PicoCalc_Display& display, NotepadState* state) {
  if (state->needsFullRedraw) {
    display.clear();
    display.fillRect(12, 12, 296, 40, DISPLAY_BLACK);
    display.drawRect(12, 12, 296, 40, DISPLAY_CYAN);
    display.print(88, 25, "NOTEPAD", DISPLAY_WHITE, 2);

    display.drawRect(TEXT_X, TEXT_Y, TEXT_W, TEXT_H, DISPLAY_MAGENTA);
    display.print(16, 304, "Esc:menu  Arrows:move  Enter:newline", DISPLAY_CYAN, 1);
    state->needsFullRedraw = false;
  }

  // Partial updates: filename/status/text only.
  display.fillRect(16, FILE_Y, 292, 12, DISPLAY_BLACK);
  display.fillRect(TEXT_X + 1, TEXT_Y + 1, TEXT_W - 2, TEXT_H - 2, DISPLAY_BLACK);
  display.fillRect(16, STATUS_Y, 292, 12, DISPLAY_BLACK);

  char fileLine[64];
  if (state->filenameEditMode) {
    snprintf(fileLine, sizeof(fileLine), "File*: %s", state->filename);
    display.print(16, FILE_Y, fileLine, DISPLAY_YELLOW, 1);
  } else {
    snprintf(fileLine, sizeof(fileLine), "File: %s", state->filename);
    display.print(16, FILE_Y, fileLine, DISPLAY_CYAN, 1);
  }

  NotepadLayout layout;
  buildLayout(state, &layout);
  clampCursor(state);
  ensureCursorVisible(state, &layout);

  int drawRow = 0;
  for (int line = state->firstVisibleLine; line < layout.lineCount && drawRow < LINES_VISIBLE; ++line, ++drawRow) {
    int start = layout.starts[line];
    int len = layout.lengths[line];

    char lineBuf[48];
    int copyLen = len;
    if (copyLen > (int)sizeof(lineBuf) - 1) copyLen = (int)sizeof(lineBuf) - 1;
    if (copyLen > 0) {
      memcpy(lineBuf, &state->text[start], (size_t)copyLen);
    }
    lineBuf[copyLen] = '\0';
    display.print(TEXT_X + 6, TEXT_Y + 4 + drawRow * CHAR_H, lineBuf, DISPLAY_WHITE, 1);
  }

  int cursorRow = 0;
  int cursorCol = 0;
  cursorToRowCol(&layout, state->cursorIndex, &cursorRow, &cursorCol);
  if (cursorRow >= state->firstVisibleLine && cursorRow < state->firstVisibleLine + LINES_VISIBLE) {
    int localRow = cursorRow - state->firstVisibleLine;
    int cx = TEXT_X + 6 + cursorCol * CHAR_W;
    int cy = TEXT_Y + 4 + localRow * CHAR_H;
    display.drawLine(cx, cy, cx, cy + 10, DISPLAY_YELLOW);
  }

  if (state->hasError) {
    display.print(16, STATUS_Y, state->statusLine, DISPLAY_RED, 1);
  } else {
    display.print(16, STATUS_Y, state->statusLine, DISPLAY_GREEN, 1);
  }

  if (state->menuOpen) {
    const int mx = 182;
    const int my = 88;
    const int mw = 110;
    const int mh = 92;
    display.fillRect(mx, my, mw, mh, DISPLAY_BLACK);
    display.drawRect(mx, my, mw, mh, DISPLAY_CYAN);
    display.print(mx + 8, my + 6, "Notepad Menu", DISPLAY_CYAN, 1);

    for (int i = 0; i < 4; ++i) {
      int rowY = my + 22 + i * 16;
      if (i == state->menuIndex) {
        display.fillRect(mx + 4, rowY - 2, mw - 8, 14, DISPLAY_BLUE);
      }
      display.print(mx + 10, rowY, MENU_ITEMS[i], i == state->menuIndex ? DISPLAY_WHITE : DISPLAY_YELLOW, 1);
    }
  }

  if (state->filePickerMode) {
    const int px = 28;
    const int py = 74;
    const int pw = 264;
    const int ph = 174;
    display.fillRect(px, py, pw, ph, DISPLAY_BLACK);
    display.drawRect(px, py, pw, ph, DISPLAY_CYAN);
    display.print(px + 8, py + 6, "Open .txt from SD", DISPLAY_CYAN, 1);

    int first = 0;
    if (state->filePickerIndex >= 8) {
      first = state->filePickerIndex - 7;
    }

    for (int i = 0; i < 8; ++i) {
      int idx = first + i;
      if (idx >= state->filePickerCount) break;

      int rowY = py + 24 + i * 16;
      if (idx == state->filePickerIndex) {
        display.fillRect(px + 4, rowY - 2, pw - 8, 14, DISPLAY_BLUE);
      }
      display.print(px + 8, rowY, state->filePickerItems[idx], idx == state->filePickerIndex ? DISPLAY_WHITE : DISPLAY_YELLOW, 1);
    }

    display.print(px + 8, py + ph - 14, "Esc close  Enter open", DISPLAY_CYAN, 1);
  }
}
