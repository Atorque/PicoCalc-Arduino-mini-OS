#ifndef CALENDAR_APP_H
#define CALENDAR_APP_H

#include "PicoCalc_Display.h"

#define CALENDAR_MAX_ENTRIES 64
#define CALENDAR_TEXT_MAX 40

struct CalendarEntry {
  int year;
  int month;
  int day;
  char text[CALENDAR_TEXT_MAX];
};

struct CalendarState {
  int month;
  int year;
  int selectedDay;
  int todayDay;
  int todayMonth;
  int todayYear;
  bool sdAvailable;
  bool hasError;
  bool editMode;
  bool monthViewMode;
  int monthViewMonth;
  int monthViewYear;
  int entryCount;
  CalendarEntry entries[CALENDAR_MAX_ENTRIES];
  char editBuffer[CALENDAR_TEXT_MAX];
  int editLen;
  char statusLine[64];
  bool needsFullRedraw;
};

void calendarInit(CalendarState* state, bool sdAvailable);
bool calendarHandleKey(CalendarState* state, int key);
void calendarDrawScreen(PicoCalc_Display& display, CalendarState* state);

#endif // CALENDAR_APP_H
