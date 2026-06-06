#include "CalendarApp.h"

#include <Arduino.h>
#include <SD.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "i2ckbd.h"

static const char* CALENDAR_FILE = "/CALENDAR.TXT";

static const char* MONTH_NAMES[12] = {
  "January", "February", "March", "April", "May", "June",
  "July", "August", "September", "October", "November", "December"
};

static const char* WEEKDAY_NAMES[7] = {
  "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa"
};

static const char* MONTH_ABBR[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static void setStatus(CalendarState* state, const char* text, bool isError) {
  strncpy(state->statusLine, text, sizeof(state->statusLine) - 1);
  state->statusLine[sizeof(state->statusLine) - 1] = '\0';
  state->hasError = isError;
}

static int monthFromName(const char* m3) {
  if (strcmp(m3, "Jan") == 0) return 1;
  if (strcmp(m3, "Feb") == 0) return 2;
  if (strcmp(m3, "Mar") == 0) return 3;
  if (strcmp(m3, "Apr") == 0) return 4;
  if (strcmp(m3, "May") == 0) return 5;
  if (strcmp(m3, "Jun") == 0) return 6;
  if (strcmp(m3, "Jul") == 0) return 7;
  if (strcmp(m3, "Aug") == 0) return 8;
  if (strcmp(m3, "Sep") == 0) return 9;
  if (strcmp(m3, "Oct") == 0) return 10;
  if (strcmp(m3, "Nov") == 0) return 11;
  return 12;
}

static void parseBuildDate(int* day, int* month, int* year) {
  const char* date = __DATE__;
  char m3[4];
  m3[0] = date[0];
  m3[1] = date[1];
  m3[2] = date[2];
  m3[3] = '\0';

  *month = monthFromName(m3);
  *day = atoi(date + 4);
  *year = atoi(date + 7);

  if (*day <= 0) *day = 1;
  if (*month < 1 || *month > 12) *month = 1;
  if (*year < 1970) *year = 1970;
}

static bool isLeapYear(int year) {
  if ((year % 400) == 0) return true;
  if ((year % 100) == 0) return false;
  return (year % 4) == 0;
}

static int daysInMonth(int month, int year) {
  if (month == 2) return isLeapYear(year) ? 29 : 28;
  if (month == 4 || month == 6 || month == 9 || month == 11) return 30;
  return 31;
}

// Returns 0=Sunday..6=Saturday
static int dayOfWeek(int day, int month, int year) {
  int m = month;
  int y = year;

  if (m < 3) {
    m += 12;
    y -= 1;
  }

  int k = y % 100;
  int j = y / 100;
  int h = (day + (13 * (m + 1)) / 5 + k + k / 4 + j / 4 + 5 * j) % 7;
  return (h + 6) % 7;
}

static void clampMonthYear(CalendarState* state) {
  if (state->month < 1) {
    state->month = 12;
    state->year--;
  } else if (state->month > 12) {
    state->month = 1;
    state->year++;
  }

  if (state->year < 1970) state->year = 1970;
  if (state->year > 2199) state->year = 2199;
}

static void clampSelectedDay(CalendarState* state) {
  int maxDay = daysInMonth(state->month, state->year);
  if (state->selectedDay < 1) state->selectedDay = 1;
  if (state->selectedDay > maxDay) state->selectedDay = maxDay;
}

static int entryIndexForDate(const CalendarState* state, int year, int month, int day) {
  for (int i = 0; i < state->entryCount; ++i) {
    if (state->entries[i].year == year && state->entries[i].month == month && state->entries[i].day == day) {
      return i;
    }
  }
  return -1;
}

static bool saveEntries(CalendarState* state) {
  if (!state->sdAvailable) {
    setStatus(state, "SD unavailable", true);
    return false;
  }

  SD.remove(CALENDAR_FILE);
  File f = SD.open(CALENDAR_FILE, FILE_WRITE);
  if (!f) {
    setStatus(state, "Calendar save failed", true);
    return false;
  }

  for (int i = 0; i < state->entryCount; ++i) {
    char line[96];
    snprintf(line, sizeof(line), "%04d-%02d-%02d|%s\n",
             state->entries[i].year,
             state->entries[i].month,
             state->entries[i].day,
             state->entries[i].text);
    f.print(line);
  }

  f.close();
  return true;
}

static void loadEntries(CalendarState* state) {
  state->entryCount = 0;

  if (!state->sdAvailable) {
    setStatus(state, "SD unavailable", true);
    return;
  }

  File f = SD.open(CALENDAR_FILE, FILE_READ);
  if (!f) {
    setStatus(state, "No calendar file yet", false);
    return;
  }

  char line[96];
  int pos = 0;

  while (f.available()) {
    char c = (char)f.read();
    if (c == '\r') continue;

    if (c == '\n') {
      line[pos] = '\0';
      pos = 0;

      int y = 0, m = 0, d = 0;
      char txt[CALENDAR_TEXT_MAX];
      txt[0] = '\0';

      if (sscanf(line, "%d-%d-%d|%39[^\n]", &y, &m, &d, txt) == 4) {
        if (y >= 1970 && y <= 2199 && m >= 1 && m <= 12 && d >= 1 && d <= 31) {
          int idx = entryIndexForDate(state, y, m, d);
          if (idx >= 0) {
            strncpy(state->entries[idx].text, txt, sizeof(state->entries[idx].text) - 1);
            state->entries[idx].text[sizeof(state->entries[idx].text) - 1] = '\0';
          } else if (state->entryCount < CALENDAR_MAX_ENTRIES) {
            state->entries[state->entryCount].year = y;
            state->entries[state->entryCount].month = m;
            state->entries[state->entryCount].day = d;
            strncpy(state->entries[state->entryCount].text, txt, sizeof(state->entries[state->entryCount].text) - 1);
            state->entries[state->entryCount].text[sizeof(state->entries[state->entryCount].text) - 1] = '\0';
            state->entryCount++;
          }
        }
      }

      continue;
    }

    if (pos < (int)sizeof(line) - 1) {
      line[pos++] = c;
    }
  }

  if (pos > 0) {
    line[pos] = '\0';
    int y = 0, m = 0, d = 0;
    char txt[CALENDAR_TEXT_MAX];
    txt[0] = '\0';
    if (sscanf(line, "%d-%d-%d|%39[^\n]", &y, &m, &d, txt) == 4 && state->entryCount < CALENDAR_MAX_ENTRIES) {
      int idx = entryIndexForDate(state, y, m, d);
      if (idx >= 0) {
        strncpy(state->entries[idx].text, txt, sizeof(state->entries[idx].text) - 1);
        state->entries[idx].text[sizeof(state->entries[idx].text) - 1] = '\0';
      } else {
        state->entries[state->entryCount].year = y;
        state->entries[state->entryCount].month = m;
        state->entries[state->entryCount].day = d;
        strncpy(state->entries[state->entryCount].text, txt, sizeof(state->entries[state->entryCount].text) - 1);
        state->entries[state->entryCount].text[sizeof(state->entries[state->entryCount].text) - 1] = '\0';
        state->entryCount++;
      }
    }
  }

  f.close();

  char msg[64];
  snprintf(msg, sizeof(msg), "Loaded %d calendar entries", state->entryCount);
  setStatus(state, msg, false);
}

static void moveSelectedByDays(CalendarState* state, int delta) {
  int day = state->selectedDay + delta;
  int month = state->month;
  int year = state->year;

  while (day < 1) {
    month--;
    if (month < 1) {
      month = 12;
      year--;
    }
    if (year < 1970) {
      year = 1970;
      month = 1;
      day = 1;
      break;
    }
    day += daysInMonth(month, year);
  }

  while (day > daysInMonth(month, year)) {
    day -= daysInMonth(month, year);
    month++;
    if (month > 12) {
      month = 1;
      year++;
    }
    if (year > 2199) {
      year = 2199;
      month = 12;
      day = daysInMonth(month, year);
      break;
    }
  }

  state->year = year;
  state->month = month;
  state->selectedDay = day;
  clampMonthYear(state);
  clampSelectedDay(state);
}

static const char* selectedEntryText(const CalendarState* state) {
  int idx = entryIndexForDate(state, state->year, state->month, state->selectedDay);
  if (idx < 0) return "(none)";
  return state->entries[idx].text;
}

static bool beginEditSelected(CalendarState* state) {
  int idx = entryIndexForDate(state, state->year, state->month, state->selectedDay);
  state->editMode = true;
  if (idx >= 0) {
    strncpy(state->editBuffer, state->entries[idx].text, sizeof(state->editBuffer) - 1);
    state->editBuffer[sizeof(state->editBuffer) - 1] = '\0';
  } else {
    state->editBuffer[0] = '\0';
  }
  state->editLen = (int)strlen(state->editBuffer);
  setStatus(state, "Edit entry: Enter save, Esc cancel", false);
  return true;
}

static bool commitEdit(CalendarState* state) {
  int idx = entryIndexForDate(state, state->year, state->month, state->selectedDay);

  if (state->editLen <= 0) {
    if (idx >= 0) {
      for (int i = idx; i + 1 < state->entryCount; ++i) {
        state->entries[i] = state->entries[i + 1];
      }
      state->entryCount--;
      if (!saveEntries(state)) return false;
      setStatus(state, "Entry cleared", false);
    } else {
      setStatus(state, "No entry to clear", false);
    }
    state->editMode = false;
    return true;
  }

  if (idx >= 0) {
    strncpy(state->entries[idx].text, state->editBuffer, sizeof(state->entries[idx].text) - 1);
    state->entries[idx].text[sizeof(state->entries[idx].text) - 1] = '\0';
  } else {
    if (state->entryCount >= CALENDAR_MAX_ENTRIES) {
      setStatus(state, "Entry limit reached", true);
      return false;
    }
    state->entries[state->entryCount].year = state->year;
    state->entries[state->entryCount].month = state->month;
    state->entries[state->entryCount].day = state->selectedDay;
    strncpy(state->entries[state->entryCount].text, state->editBuffer, sizeof(state->entries[state->entryCount].text) - 1);
    state->entries[state->entryCount].text[sizeof(state->entries[state->entryCount].text) - 1] = '\0';
    state->entryCount++;
  }

  if (!saveEntries(state)) return false;

  setStatus(state, "Entry saved", false);
  state->editMode = false;
  return true;
}

static bool deleteSelectedEntry(CalendarState* state) {
  int idx = entryIndexForDate(state, state->year, state->month, state->selectedDay);
  if (idx < 0) {
    setStatus(state, "No entry on selected day", false);
    return true;
  }

  for (int i = idx; i + 1 < state->entryCount; ++i) {
    state->entries[i] = state->entries[i + 1];
  }
  state->entryCount--;

  if (!saveEntries(state)) return false;
  setStatus(state, "Entry deleted", false);
  return true;
}

void calendarInit(CalendarState* state, bool sdAvailable) {
  parseBuildDate(&state->todayDay, &state->todayMonth, &state->todayYear);
  state->month = state->todayMonth;
  state->year = state->todayYear;
  state->selectedDay = state->todayDay;
  state->sdAvailable = sdAvailable;
  state->hasError = false;
  state->editMode = false;
  state->monthViewMode = false;
  state->monthViewMonth = state->month;
  state->monthViewYear = state->year;
  state->entryCount = 0;
  state->editBuffer[0] = '\0';
  state->editLen = 0;
  setStatus(state, "Calendar ready", false);
  loadEntries(state);
  state->needsFullRedraw = true;
}

bool calendarHandleKey(CalendarState* state, int key) {
  if (key < 0) return false;

  if (state->monthViewMode) {
    if (key == KEY_ESC) {
      state->monthViewMode = false;
      state->needsFullRedraw = true;
      setStatus(state, "Month view closed", false);
      return true;
    }

    if (key == KEY_LEFT || key == 'a' || key == 'A') {
      state->monthViewMonth--;
      if (state->monthViewMonth < 1) {
        state->monthViewMonth = 12;
        state->monthViewYear--;
      }
      if (state->monthViewYear < 1970) {
        state->monthViewYear = 1970;
        state->monthViewMonth = 1;
      }
      return true;
    }

    if (key == KEY_RIGHT || key == 'd' || key == 'D') {
      state->monthViewMonth++;
      if (state->monthViewMonth > 12) {
        state->monthViewMonth = 1;
        state->monthViewYear++;
      }
      if (state->monthViewYear > 2199) {
        state->monthViewYear = 2199;
        state->monthViewMonth = 12;
      }
      return true;
    }

    if (key == KEY_UP || key == 'w' || key == 'W') {
      state->monthViewMonth -= 3;
      while (state->monthViewMonth < 1) {
        state->monthViewMonth += 12;
        state->monthViewYear--;
      }
      if (state->monthViewYear < 1970) {
        state->monthViewYear = 1970;
        if (state->monthViewMonth < 1) state->monthViewMonth = 1;
      }
      return true;
    }

    if (key == KEY_DOWN || key == 's' || key == 'S') {
      state->monthViewMonth += 3;
      while (state->monthViewMonth > 12) {
        state->monthViewMonth -= 12;
        state->monthViewYear++;
      }
      if (state->monthViewYear > 2199) {
        state->monthViewYear = 2199;
        if (state->monthViewMonth > 12) state->monthViewMonth = 12;
      }
      return true;
    }

    if (key == KEY_ENTER) {
      state->month = state->monthViewMonth;
      state->year = state->monthViewYear;
      clampMonthYear(state);
      clampSelectedDay(state);
      state->monthViewMode = false;
      state->needsFullRedraw = true;
      setStatus(state, "Month changed", false);
      return true;
    }

    return false;
  }

  if (state->editMode) {
    if (key == KEY_ESC) {
      state->editMode = false;
      setStatus(state, "Edit cancelled", false);
      return true;
    }

    if (key == KEY_ENTER) {
      return commitEdit(state);
    }

    if (key == KEY_BACKSPACE || key == KEY_DEL) {
      if (state->editLen > 0) {
        state->editLen--;
        state->editBuffer[state->editLen] = '\0';
        return true;
      }
      return false;
    }

    if (key >= 32 && key <= 126 && isprint(key)) {
      if (state->editLen >= CALENDAR_TEXT_MAX - 1) {
        setStatus(state, "Entry text full", true);
        return false;
      }
      state->editBuffer[state->editLen++] = (char)key;
      state->editBuffer[state->editLen] = '\0';
      return true;
    }

    return false;
  }

  if (key == KEY_LEFT) {
    moveSelectedByDays(state, -1);
    return true;
  }

  if (key == KEY_RIGHT) {
    moveSelectedByDays(state, 1);
    return true;
  }

  if (key == KEY_UP) {
    moveSelectedByDays(state, -7);
    return true;
  }

  if (key == KEY_DOWN) {
    moveSelectedByDays(state, 7);
    return true;
  }

  if (key == 'a' || key == 'A') {
    state->month--;
    clampMonthYear(state);
    clampSelectedDay(state);
    return true;
  }

  if (key == 'd' || key == 'D') {
    state->month++;
    clampMonthYear(state);
    clampSelectedDay(state);
    return true;
  }

  if (key == 'w' || key == 'W') {
    state->year++;
    clampMonthYear(state);
    clampSelectedDay(state);
    return true;
  }

  if (key == 's' || key == 'S') {
    state->year--;
    clampMonthYear(state);
    clampSelectedDay(state);
    return true;
  }

  if (key == KEY_ENTER) {
    return beginEditSelected(state);
  }

  if (key == 't' || key == 'T') {
    state->month = state->todayMonth;
    state->year = state->todayYear;
    state->selectedDay = state->todayDay;
    clampSelectedDay(state);
    setStatus(state, "Jumped to today", false);
    return true;
  }

  if (key == 'n' || key == 'N') {
    return beginEditSelected(state);
  }

  if (key == KEY_DEL) {
    return deleteSelectedEntry(state);
  }

  if (key == 'r' || key == 'R') {
    loadEntries(state);
    return true;
  }

  if (key == 'm' || key == 'M') {
    state->monthViewMode = true;
    state->monthViewMonth = state->month;
    state->monthViewYear = state->year;
    setStatus(state, "Month view: arrows move, Enter apply", false);
    return true;
  }

  return false;
}

static const int GRID_X = 13;
static const int GRID_Y = 84;
static const int CELL_W = 42;
static const int CELL_H = 30;
static const int INFO_Y = 286;
static const int STATUS_Y = 298;
static const int LEGEND_Y = 310;

static bool dayToCell(int month, int year, int day, int* x, int* y) {
  int maxDay = daysInMonth(month, year);
  if (day < 1 || day > maxDay) return false;

  int firstDow = dayOfWeek(1, month, year);
  int index = firstDow + (day - 1);
  int row = index / 7;
  int col = index % 7;
  *x = GRID_X + col * CELL_W;
  *y = GRID_Y + 20 + row * CELL_H;
  return true;
}

static void drawDayCell(PicoCalc_Display& display, const CalendarState* state, int day, bool selected) {
  int x = 0;
  int y = 0;
  if (!dayToCell(state->month, state->year, day, &x, &y)) return;

  display.fillRect(x + 1, y + 1, CELL_W - 2, CELL_H - 2, DISPLAY_BLACK);

  bool isToday = (state->month == state->todayMonth && state->year == state->todayYear && day == state->todayDay);
  if (isToday) {
    display.fillRect(x + 2, y + 2, CELL_W - 4, CELL_H - 4, DISPLAY_BLUE);
  }

  if (selected) {
    display.drawRect(x + 1, y + 1, CELL_W - 2, CELL_H - 2, DISPLAY_YELLOW);
  }

  char dayText[4];
  snprintf(dayText, sizeof(dayText), "%d", day);
  int tx = x + (day >= 10 ? 10 : 14);
  display.print(tx, y + 6, dayText, DISPLAY_WHITE, 1);

  if (entryIndexForDate(state, state->year, state->month, day) >= 0) {
    display.fillRect(x + CELL_W - 8, y + CELL_H - 7, 4, 4, DISPLAY_GREEN);
  }
}

static void drawMonthCells(PicoCalc_Display& display, const CalendarState* state) {
  for (int row = 0; row < 6; ++row) {
    for (int col = 0; col < 7; ++col) {
      int x = GRID_X + col * CELL_W;
      int y = GRID_Y + 20 + row * CELL_H;
      display.fillRect(x + 1, y + 1, CELL_W - 2, CELL_H - 2, DISPLAY_BLACK);
    }
  }

  int maxDay = daysInMonth(state->month, state->year);
  for (int day = 1; day <= maxDay; ++day) {
    drawDayCell(display, state, day, day == state->selectedDay);
  }
}

static void drawGridBorders(PicoCalc_Display& display) {
  for (int c = 0; c < 7; ++c) {
    int x = GRID_X + c * CELL_W;
    display.drawRect(x, GRID_Y, CELL_W, 20, DISPLAY_CYAN);
  }

  for (int row = 0; row < 6; ++row) {
    for (int col = 0; col < 7; ++col) {
      int x = GRID_X + col * CELL_W;
      int y = GRID_Y + 20 + row * CELL_H;
      display.drawRect(x, y, CELL_W, CELL_H, DISPLAY_MAGENTA);
    }
  }
}

void calendarDrawScreen(PicoCalc_Display& display, CalendarState* state) {
  static bool hasPrev = false;
  static int prevMonth = 0;
  static int prevYear = 0;
  static int prevSelectedDay = 0;
  static int prevEntryCount = 0;
  static bool prevEditMode = false;
  static bool prevMonthViewMode = false;
  static bool prevHasError = false;
  static char prevStatus[64] = "";
  static char prevSelectedLine[96] = "";

  if (state->needsFullRedraw) {
    display.clear();

    display.fillRect(20, 14, 280, 42, DISPLAY_BLACK);
    display.drawRect(20, 14, 280, 42, DISPLAY_GREEN);
    display.print(82, 28, "CALENDAR", DISPLAY_WHITE, 2);

    for (int c = 0; c < 7; ++c) {
      int x = GRID_X + c * CELL_W;
      display.drawRect(x, GRID_Y, CELL_W, 20, DISPLAY_CYAN);
      display.print(x + 12, GRID_Y + 5, WEEKDAY_NAMES[c], DISPLAY_CYAN, 1);
    }

    for (int row = 0; row < 6; ++row) {
      for (int col = 0; col < 7; ++col) {
        int x = GRID_X + col * CELL_W;
        int y = GRID_Y + 20 + row * CELL_H;
        display.drawRect(x, y, CELL_W, CELL_H, DISPLAY_MAGENTA);
      }
    }

    display.print(12, LEGEND_Y, "Arrows day  M month view  N edit", DISPLAY_CYAN, 1);
    state->needsFullRedraw = false;
    hasPrev = false;
  }

  bool monthChanged = !hasPrev || prevMonth != state->month || prevYear != state->year;
  bool selectionChanged = !hasPrev || prevSelectedDay != state->selectedDay || monthChanged;
  bool entriesChanged = !hasPrev || prevEntryCount != state->entryCount;
  bool editClosed = (!state->editMode) && hasPrev && prevEditMode;
  bool monthViewClosed = (!state->monthViewMode) && hasPrev && prevMonthViewMode;
  bool overlayClosed = editClosed || monthViewClosed;

  if (monthChanged) {
    display.fillRect(60, 62, 210, 16, DISPLAY_BLACK);
    char title[48];
    snprintf(title, sizeof(title), "%s %d", MONTH_NAMES[state->month - 1], state->year);
    display.print(74, 62, title, DISPLAY_YELLOW, 2);
  }

  if (overlayClosed) {
    drawGridBorders(display);
    drawMonthCells(display, state);
  } else if (monthChanged || entriesChanged) {
    drawMonthCells(display, state);
  } else if (selectionChanged) {
    if (prevSelectedDay >= 1 && prevSelectedDay <= daysInMonth(state->month, state->year)) {
      drawDayCell(display, state, prevSelectedDay, false);
    }
    drawDayCell(display, state, state->selectedDay, true);
  }

  char selectedLine[96];
  snprintf(selectedLine, sizeof(selectedLine), "%04d-%02d-%02d: %s",
           state->year, state->month, state->selectedDay, selectedEntryText(state));
  bool selectedLineChanged = !hasPrev || strcmp(selectedLine, prevSelectedLine) != 0;
  bool statusChanged = !hasPrev || state->hasError != prevHasError || strcmp(state->statusLine, prevStatus) != 0;

  if (selectedLineChanged) {
    display.fillRect(12, INFO_Y, 308, 10, DISPLAY_BLACK);
    display.print(12, INFO_Y, selectedLine, DISPLAY_WHITE, 1);
  }

  if (statusChanged) {
    display.fillRect(12, STATUS_Y, 308, 10, DISPLAY_BLACK);
    if (state->hasError) {
      display.print(12, STATUS_Y, state->statusLine, DISPLAY_RED, 1);
    } else {
      display.print(12, STATUS_Y, state->statusLine, DISPLAY_GREEN, 1);
    }
  }

  if (state->editMode) {
    display.fillRect(16, 124, 288, 72, DISPLAY_BLACK);
    display.drawRect(16, 124, 288, 72, DISPLAY_CYAN);
    display.print(24, 132, "Edit Entry", DISPLAY_CYAN, 1);

    char editLine[56];
    snprintf(editLine, sizeof(editLine), "%04d-%02d-%02d",
             state->year, state->month, state->selectedDay);
    display.print(24, 148, editLine, DISPLAY_YELLOW, 1);
    display.print(24, 164, state->editBuffer, DISPLAY_WHITE, 1);
    display.drawLine(24 + state->editLen * 6, 164, 24 + state->editLen * 6, 174, DISPLAY_YELLOW);
    display.print(24, 184, "Enter save, Esc cancel", DISPLAY_CYAN, 1);
  }

  if (state->monthViewMode) {
    const int mx = 38;
    const int my = 92;
    const int mw = 244;
    const int mh = 152;

    display.fillRect(mx, my, mw, mh, DISPLAY_BLACK);
    display.drawRect(mx, my, mw, mh, DISPLAY_CYAN);

    char title[32];
    snprintf(title, sizeof(title), "MONTH VIEW %d", state->monthViewYear);
    display.print(mx + 10, my + 8, title, DISPLAY_CYAN, 1);

    for (int i = 0; i < 12; ++i) {
      int row = i / 3;
      int col = i % 3;
      int bx = mx + 10 + col * 76;
      int by = my + 28 + row * 28;

      bool selected = (state->monthViewMonth == (i + 1));
      if (selected) {
        display.fillRect(bx - 4, by - 2, 68, 20, DISPLAY_BLUE);
      }
      display.print(bx, by, MONTH_ABBR[i], selected ? DISPLAY_WHITE : DISPLAY_YELLOW, 1);
    }

    display.print(mx + 10, my + mh - 16, "Enter apply  Esc close", DISPLAY_CYAN, 1);
  }

  prevMonth = state->month;
  prevYear = state->year;
  prevSelectedDay = state->selectedDay;
  prevEntryCount = state->entryCount;
  prevEditMode = state->editMode;
  prevMonthViewMode = state->monthViewMode;
  prevHasError = state->hasError;
  strncpy(prevStatus, state->statusLine, sizeof(prevStatus) - 1);
  prevStatus[sizeof(prevStatus) - 1] = '\0';
  strncpy(prevSelectedLine, selectedLine, sizeof(prevSelectedLine) - 1);
  prevSelectedLine[sizeof(prevSelectedLine) - 1] = '\0';
  hasPrev = true;
}
