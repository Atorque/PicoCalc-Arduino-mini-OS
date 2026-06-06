#include <SPI.h>
#include <SD.h>
#include "PicoCalc_Display.h"
#include <pico/stdlib.h>
#include <hardware/gpio.h>
#include "i2ckbd.h"
#include "SoundEngine.h"
#include "SudokuGame.h"
#include "TetrisApp.h"
#include "SnakeApp.h"
#include "PongApp.h"
#include "CalendarApp.h"
#include "BudgetApp.h"
#include "CalculatorApp.h"
#include "FileExplorerApp.h"
#include "NotepadApp.h"


PicoCalc_Display display;

#define SCREEN_WIDTH display.width()
#define SCREEN_HEIGHT display.height()

const int SD_MISO = 16;  // AKA SPI RX
const int SD_MOSI = 19;  // AKA SPI TX
const int SD_CS = 17;
const int SD_SCK = 18;

const uint LEDPIN = 25;

const int APP_MENU_X = 26;
const int APP_MENU_W = 268;
const int APP_ROW_H = 28;
const int APP_FIRST_Y = 66;
const int APP_ROW_GAP = 6;
const int APP_ROW_TEXT_Y = 8;


SudokuState sudokuState;
TetrisState tetrisState;
SnakeState snakeState;
PongState pongState;
CalendarState calendarState;
BudgetState budgetState;
CalculatorState calculatorState;
FileExplorerState fileExplorerState;
NotepadState notepadState;
bool sdCardReady = false;

enum AppScreen {
  SCREEN_OS_MENU,
  SCREEN_GAMES_MENU,
  SCREEN_CALENDAR,
  SCREEN_BUDGET,
  SCREEN_CALCULATOR,
  SCREEN_FILE_EXPLORER,
  SCREEN_NOTEPAD,
  SCREEN_SUDOKU_INTRO,
  SCREEN_SUDOKU_GAME,
  SCREEN_TETRIS_GAME,
  SCREEN_SNAKE_GAME,
  SCREEN_PONG_GAME,
  SCREEN_APP_PLACEHOLDER
};

enum OSApp {
  APP_GAMES = 0,
  APP_CALENDAR = 1,
  APP_BUDGET = 2,
  APP_CALCULATOR = 3,
  APP_FILE_EXPLORER = 4,
  APP_NOTEPAD = 5
};

enum GameItem {
  GAME_SUDOKU = 0,
  GAME_TETRIS = 1,
  GAME_SNAKE = 2,
  GAME_PONG = 3
};

AppScreen currentScreen = SCREEN_OS_MENU;
SudokuDifficulty currentDifficulty = SUDOKU_DIFFICULTY_MEDIUM;
OSApp selectedMenuApp = APP_GAMES;
OSApp activeApp = APP_GAMES;
GameItem selectedGameItem = GAME_SUDOKU;



void setup() {
  // put your setup code here, to run once:

  setupPicoCalc();
  sudokuInitState(&sudokuState);
  calculatorInit(&calculatorState);
  sound_set_music_enabled(false);
}

void loop() {
  static bool menuNeedsRedraw = true;
  static bool menuSelectionNeedsRedraw = false;
  static OSApp previousMenuApp = APP_GAMES;
  static bool gamesMenuNeedsRedraw = false;
  static bool gamesMenuSelectionNeedsRedraw = false;
  static GameItem previousGameItem = GAME_SUDOKU;
  static bool introNeedsRedraw = true;
  static bool introSelectionNeedsRedraw = false;
  static bool calendarNeedsRedraw = false;
  static bool budgetNeedsRedraw = false;
  static bool calculatorNeedsRedraw = false;
  static bool fileExplorerNeedsRedraw = false;
  static bool notepadNeedsRedraw = false;
  static bool gameNeedsRedraw = false;
  static bool placeholderNeedsRedraw = false;
  static SudokuDifficulty previousIntroDifficulty = SUDOKU_DIFFICULTY_MEDIUM;

  int key = read_i2c_kbd();

  // Background music only plays during game screens; OS sounds still work globally.
  bool gameMusic = (currentScreen == SCREEN_SUDOKU_GAME || currentScreen == SCREEN_TETRIS_GAME || currentScreen == SCREEN_SNAKE_GAME || currentScreen == SCREEN_PONG_GAME);
  sound_set_music_enabled(gameMusic);
  if (currentScreen == SCREEN_TETRIS_GAME) {
    sound_set_music_mode(SOUND_MUSIC_TETRIS);
  } else {
    sound_set_music_mode(SOUND_MUSIC_AMBIENT);
  }

  if (currentScreen == SCREEN_OS_MENU) {
    if (key == KEY_UP || key == 'w' || key == 'W') {
      if (selectedMenuApp > APP_GAMES) {
        previousMenuApp = selectedMenuApp;
        selectedMenuApp = (OSApp)(selectedMenuApp - 1);
        menuSelectionNeedsRedraw = true;
        sound_play_ui_tick();
      }
    } else if (key == KEY_DOWN || key == 's' || key == 'S') {
      if (selectedMenuApp < APP_NOTEPAD) {
        previousMenuApp = selectedMenuApp;
        selectedMenuApp = (OSApp)(selectedMenuApp + 1);
        menuSelectionNeedsRedraw = true;
        sound_play_ui_tick();
      }
    } else if (key == KEY_ENTER || key == 'g' || key == 'G' || key == KEY_RIGHT) {
      activeApp = selectedMenuApp;
      sound_play_ui_confirm();
      if (activeApp == APP_GAMES) {
        currentScreen = SCREEN_GAMES_MENU;
        gamesMenuNeedsRedraw = true;
        gamesMenuSelectionNeedsRedraw = false;
      } else if (activeApp == APP_CALENDAR) {
        currentScreen = SCREEN_CALENDAR;
        calendarInit(&calendarState, sdCardReady);
        calendarNeedsRedraw = true;
      } else if (activeApp == APP_BUDGET) {
        currentScreen = SCREEN_BUDGET;
        budgetInit(&budgetState, sdCardReady);
        budgetNeedsRedraw = true;
      } else if (activeApp == APP_CALCULATOR) {
        currentScreen = SCREEN_CALCULATOR;
        calculatorInit(&calculatorState);
        calculatorNeedsRedraw = true;
      } else if (activeApp == APP_FILE_EXPLORER) {
        currentScreen = SCREEN_FILE_EXPLORER;
        fileExplorerInit(&fileExplorerState, sdCardReady);
        fileExplorerNeedsRedraw = true;
      } else if (activeApp == APP_NOTEPAD) {
        currentScreen = SCREEN_NOTEPAD;
        notepadInit(&notepadState, sdCardReady);
        notepadNeedsRedraw = true;
      } else {
        currentScreen = SCREEN_APP_PLACEHOLDER;
        placeholderNeedsRedraw = true;
      }
    }

    if (menuNeedsRedraw) {
      drawOSMenuScreen();
      menuNeedsRedraw = false;
      menuSelectionNeedsRedraw = false;
    } else if (menuSelectionNeedsRedraw) {
      drawOSMenuRow(previousMenuApp, false);
      drawOSMenuRow(selectedMenuApp, true);
      menuSelectionNeedsRedraw = false;
    }
  } else if (currentScreen == SCREEN_GAMES_MENU) {
    if (key == KEY_UP || key == 'w' || key == 'W') {
      if (selectedGameItem > GAME_SUDOKU) {
        previousGameItem = selectedGameItem;
        selectedGameItem = (GameItem)(selectedGameItem - 1);
        gamesMenuSelectionNeedsRedraw = true;
        sound_play_ui_tick();
      }
    } else if (key == KEY_DOWN || key == 's' || key == 'S') {
      if (selectedGameItem < GAME_PONG) {
        previousGameItem = selectedGameItem;
        selectedGameItem = (GameItem)(selectedGameItem + 1);
        gamesMenuSelectionNeedsRedraw = true;
        sound_play_ui_tick();
      }
    } else if (key == KEY_ESC || key == 'm' || key == 'M' || key == KEY_LEFT) {
      currentScreen = SCREEN_OS_MENU;
      menuNeedsRedraw = true;
    } else if (key == KEY_ENTER || key == 'g' || key == 'G' || key == KEY_RIGHT) {
      sound_play_ui_confirm();
      if (selectedGameItem == GAME_SUDOKU) {
        currentScreen = SCREEN_SUDOKU_INTRO;
        introNeedsRedraw = true;
        introSelectionNeedsRedraw = false;
      } else if (selectedGameItem == GAME_TETRIS) {
        tetrisInit(&tetrisState);
        currentScreen = SCREEN_TETRIS_GAME;
        gameNeedsRedraw = true;
      } else if (selectedGameItem == GAME_SNAKE) {
        snakeInit(&snakeState);
        currentScreen = SCREEN_SNAKE_GAME;
        gameNeedsRedraw = true;
      } else if (selectedGameItem == GAME_PONG) {
        pongInit(&pongState);
        currentScreen = SCREEN_PONG_GAME;
        gameNeedsRedraw = true;
      }
    }

    if (gamesMenuNeedsRedraw) {
      drawGamesMenuScreen();
      gamesMenuNeedsRedraw = false;
      gamesMenuSelectionNeedsRedraw = false;
    } else if (gamesMenuSelectionNeedsRedraw) {
      drawGamesMenuRow(previousGameItem, false);
      drawGamesMenuRow(selectedGameItem, true);
      gamesMenuSelectionNeedsRedraw = false;
    }
  } else if (currentScreen == SCREEN_CALENDAR) {
    if (calendarHandleKey(&calendarState, key)) {
      calendarNeedsRedraw = true;
      sound_play_ui_tick();
    } else if (key == KEY_ESC || key == 'm' || key == 'M') {
      currentScreen = SCREEN_OS_MENU;
      menuNeedsRedraw = true;
      sound_play_ui_confirm();
    }

    if (calendarNeedsRedraw) {
      calendarDrawScreen(display, &calendarState);
      calendarNeedsRedraw = false;
    }
  } else if (currentScreen == SCREEN_BUDGET) {
    if (key == KEY_ESC || key == 'm' || key == 'M') {
      currentScreen = SCREEN_OS_MENU;
      menuNeedsRedraw = true;
      sound_play_ui_confirm();
    } else if (budgetHandleKey(&budgetState, key)) {
      budgetNeedsRedraw = true;
      if (budgetState.hasError) {
        sound_play_error();
      } else {
        sound_play_ui_tick();
      }
    }

    if (budgetNeedsRedraw) {
      budgetDrawScreen(display, &budgetState);
      budgetNeedsRedraw = false;
    }
  } else if (currentScreen == SCREEN_CALCULATOR) {
    if (key == KEY_ESC || key == 'm' || key == 'M') {
      currentScreen = SCREEN_OS_MENU;
      menuNeedsRedraw = true;
      sound_play_ui_confirm();
    } else if (calculatorHandleKey(&calculatorState, key)) {
      calculatorNeedsRedraw = true;
      if (key == '=' || key == KEY_ENTER) {
        if (calculatorState.hasError) {
          sound_play_error();
        } else {
          sound_play_success();
        }
      } else {
        sound_play_ui_tick();
      }
    }

    if (calculatorNeedsRedraw) {
      calculatorDrawScreen(display, &calculatorState);
      calculatorNeedsRedraw = false;
    }
  } else if (currentScreen == SCREEN_FILE_EXPLORER) {
    if (key == KEY_ESC || key == 'm' || key == 'M') {
      currentScreen = SCREEN_OS_MENU;
      menuNeedsRedraw = true;
      sound_play_ui_confirm();
    } else if (fileExplorerHandleKey(&fileExplorerState, key)) {
      fileExplorerNeedsRedraw = true;
      if (key == KEY_ENTER || key == KEY_RIGHT) {
        sound_play_ui_confirm();
      } else {
        sound_play_ui_tick();
      }
    }

    if (fileExplorerNeedsRedraw) {
      fileExplorerDrawScreen(display, &fileExplorerState);
      fileExplorerNeedsRedraw = false;
    }
  } else if (currentScreen == SCREEN_NOTEPAD) {
    if (notepadHandleKey(&notepadState, key)) {
      if (notepadState.requestExit) {
        notepadState.requestExit = false;
        currentScreen = SCREEN_OS_MENU;
        menuNeedsRedraw = true;
        sound_play_ui_confirm();
      } else {
        notepadNeedsRedraw = true;
        if (notepadState.hasError) {
          sound_play_error();
        } else {
          sound_play_ui_tick();
        }
      }
    }

    if (currentScreen == SCREEN_NOTEPAD && notepadNeedsRedraw) {
      notepadDrawScreen(display, &notepadState);
      notepadNeedsRedraw = false;
    }
  } else if (currentScreen == SCREEN_SUDOKU_INTRO) {
    bool startGame = false;

    if (key == '1') {
      currentDifficulty = SUDOKU_DIFFICULTY_EASY;
      startGame = true;
    } else if (key == '2') {
      currentDifficulty = SUDOKU_DIFFICULTY_MEDIUM;
      startGame = true;
    } else if (key == '3') {
      currentDifficulty = SUDOKU_DIFFICULTY_HARD;
      startGame = true;
    } else if (key == '4') {
      currentDifficulty = SUDOKU_DIFFICULTY_EXPERT;
      startGame = true;
    } else if (key == '5') {
      currentDifficulty = SUDOKU_DIFFICULTY_MASTER;
      startGame = true;
    } else if (key == '6') {
      currentDifficulty = SUDOKU_DIFFICULTY_EXTREME;
      startGame = true;
    } else if (key == KEY_UP || key == 'w' || key == 'W') {
      if (currentDifficulty > SUDOKU_DIFFICULTY_EASY) {
        previousIntroDifficulty = currentDifficulty;
        currentDifficulty = (SudokuDifficulty)(currentDifficulty - 1);
        introSelectionNeedsRedraw = true;
        sound_play_ui_tick();
      }
    } else if (key == KEY_DOWN || key == 's' || key == 'S') {
      if (currentDifficulty < SUDOKU_DIFFICULTY_EXTREME) {
        previousIntroDifficulty = currentDifficulty;
        currentDifficulty = (SudokuDifficulty)(currentDifficulty + 1);
        introSelectionNeedsRedraw = true;
        sound_play_ui_tick();
      }
    } else if (key == KEY_ESC || key == 'm' || key == 'M') {
      currentScreen = SCREEN_OS_MENU;
      menuNeedsRedraw = true;
    } else if (key == KEY_ENTER || key == 'g' || key == 'G') {
      startGame = true;
    }

    if (startGame) {
      sudokuStartNewPuzzle(&sudokuState, currentDifficulty);
      sudokuPlayIntroToGameTransition(display, currentDifficulty);
      sound_play_ui_confirm();
      currentScreen = SCREEN_SUDOKU_GAME;
      gameNeedsRedraw = true;
    }

    if (introNeedsRedraw) {
      sudokuDrawIntroScreen(display, currentDifficulty);
      introNeedsRedraw = false;
      introSelectionNeedsRedraw = false;
    } else if (introSelectionNeedsRedraw) {
      sudokuDrawIntroOptionRow(display, previousIntroDifficulty, currentDifficulty, false);
      sudokuDrawIntroOptionRow(display, currentDifficulty, currentDifficulty, true);
      sudokuDrawIntroChallengeLegend(display, currentDifficulty);
      introSelectionNeedsRedraw = false;
    }
  } else if (currentScreen == SCREEN_SUDOKU_GAME) {
    if (key == 'g' || key == 'G') {
      sudokuStartNewPuzzle(&sudokuState, currentDifficulty);
      sound_play_ui_confirm();
      gameNeedsRedraw = true;
    } else if (key == 'm' || key == 'M') {
      currentScreen = SCREEN_OS_MENU;
      menuNeedsRedraw = true;
    } else if (key == 'd' || key == 'D' || key == KEY_ESC) {
      currentScreen = SCREEN_SUDOKU_INTRO;
      introNeedsRedraw = true;
    } else if (key == KEY_UP || key == 'w' || key == 'W') {
      sudokuMoveSelection(display, &sudokuState, currentDifficulty, -1, 0);
    } else if (key == KEY_DOWN || key == 's' || key == 'S') {
      sudokuMoveSelection(display, &sudokuState, currentDifficulty, 1, 0);
    } else if (key == KEY_LEFT || key == 'a' || key == 'A') {
      sudokuMoveSelection(display, &sudokuState, currentDifficulty, 0, -1);
    } else if (key == KEY_RIGHT) {
      sudokuMoveSelection(display, &sudokuState, currentDifficulty, 0, 1);
    } else if (key >= '1' && key <= '9') {
      sudokuSetSelectedCellValue(display, &sudokuState, currentDifficulty, (unsigned char)(key - '0'));
    } else if (key == '0' || key == KEY_BACKSPACE || key == KEY_DEL) {
      sudokuClearSelectedCell(display, &sudokuState, currentDifficulty);
    } else if (key == 'c' || key == 'C') {
      int checkResult = sudokuCheckSelectedCell(display, &sudokuState, currentDifficulty);
      if (checkResult == 2) {
        sound_play_success();
      } else if (checkResult == 3) {
        sound_play_error();
      }
    } else if (key == 'v' || key == 'V') {
      sudokuSolveAndScore(&sudokuState);
      sound_play_success();
      gameNeedsRedraw = true;
    }

    if (gameNeedsRedraw) {
      sudokuDrawPuzzleScreen(display, &sudokuState, currentDifficulty);
      gameNeedsRedraw = false;
    }
  } else if (currentScreen == SCREEN_TETRIS_GAME) {
    bool changed = false;
    if (key == KEY_ESC || key == 'm' || key == 'M') {
      currentScreen = SCREEN_GAMES_MENU;
      gamesMenuNeedsRedraw = true;
      sound_play_ui_confirm();
    } else {
      changed = tetrisHandleKey(&tetrisState, key);
      bool ticked = tetrisTick(&tetrisState);
      changed = changed || ticked;

      if (tetrisState.eventLineClear) {
        sound_play_tetris_line();
        tetrisState.eventLineClear = false;
      } else if (tetrisState.eventGameOver) {
        sound_play_tetris_game_over();
        tetrisState.eventGameOver = false;
      } else if (tetrisState.eventLock) {
        sound_play_tetris_lock();
        tetrisState.eventLock = false;
      } else if (changed && key >= 0) {
        sound_play_ui_tick();
      }
    }

    if (changed || gameNeedsRedraw) {
      tetrisDrawScreen(display, &tetrisState);
      gameNeedsRedraw = false;
    }
  } else if (currentScreen == SCREEN_SNAKE_GAME) {
    bool changed = false;
    if (key == KEY_ESC || key == 'm' || key == 'M') {
      currentScreen = SCREEN_GAMES_MENU;
      gamesMenuNeedsRedraw = true;
      sound_play_ui_confirm();
    } else {
      changed = snakeHandleKey(&snakeState, key);
      bool ticked = snakeTick(&snakeState);
      changed = changed || ticked;

      if (snakeState.eventEat) {
        sound_play_success();
        snakeState.eventEat = false;
      } else if (snakeState.eventGameOver) {
        sound_play_error();
        snakeState.eventGameOver = false;
      } else if (changed && key >= 0) {
        sound_play_ui_tick();
      }
    }

    if (changed || gameNeedsRedraw) {
      snakeDrawScreen(display, &snakeState);
      gameNeedsRedraw = false;
    }
  } else if (currentScreen == SCREEN_PONG_GAME) {
    bool changed = false;
    if (key == KEY_ESC || key == 'm' || key == 'M') {
      currentScreen = SCREEN_GAMES_MENU;
      gamesMenuNeedsRedraw = true;
      sound_play_ui_confirm();
    } else {
      changed = pongHandleKey(&pongState, key);
      bool ticked = pongTick(&pongState);
      changed = changed || ticked;

      if (pongState.eventGameOver) {
        sound_play_error();
        pongState.eventGameOver = false;
      } else if (pongState.eventScore) {
        sound_play_success();
        pongState.eventScore = false;
      } else if (pongState.eventPaddleHit || pongState.eventWallHit) {
        sound_play_ui_tick();
        pongState.eventPaddleHit = false;
        pongState.eventWallHit = false;
      }
    }

    if (changed || gameNeedsRedraw) {
      pongDrawScreen(display, &pongState);
      gameNeedsRedraw = false;
    }
  } else {
    if (key == KEY_ESC || key == 'm' || key == 'M') {
      currentScreen = SCREEN_OS_MENU;
      menuNeedsRedraw = true;
    }

    if (placeholderNeedsRedraw) {
      drawPlaceholderScreen(activeApp);
      placeholderNeedsRedraw = false;
    }
  }

  delay(20);
}

const char* appLabel(OSApp app) {
  if (app == APP_GAMES) return "Games";
  if (app == APP_CALENDAR) return "Calendar";
  if (app == APP_BUDGET) return "Budget";
  if (app == APP_CALCULATOR) return "Calculator";
  if (app == APP_FILE_EXPLORER) return "File Explorer";
  if (app == APP_NOTEPAD) return "Notepad";
  return "Unknown";
}

uint32_t appColor(OSApp app) {
  if (app == APP_GAMES) return DISPLAY_CYAN;
  if (app == APP_CALENDAR) return DISPLAY_GREEN;
  if (app == APP_BUDGET) return DISPLAY_PINK;
  if (app == APP_CALCULATOR) return DISPLAY_YELLOW;
  if (app == APP_FILE_EXPLORER) return DISPLAY_ORANGE;
  if (app == APP_NOTEPAD) return DISPLAY_MAGENTA;
  return DISPLAY_PINK;
}

const char* gameLabel(GameItem game) {
  if (game == GAME_SUDOKU) return "Sudoku";
  if (game == GAME_TETRIS) return "Tetris";
  if (game == GAME_SNAKE) return "Snake";
  if (game == GAME_PONG) return "Pong";
  return "Unknown";
}

uint32_t gameColor(GameItem game) {
  if (game == GAME_SUDOKU) return DISPLAY_CYAN;
  if (game == GAME_TETRIS) return DISPLAY_ORANGE;
  if (game == GAME_SNAKE) return DISPLAY_GREEN;
  if (game == GAME_PONG) return DISPLAY_YELLOW;
  return DISPLAY_WHITE;
}

void drawOSMenuRow(OSApp app, bool selected) {
  int y = APP_FIRST_Y + ((int)app) * (APP_ROW_H + APP_ROW_GAP);
  uint32_t accent = appColor(app);

  display.fillRect(APP_MENU_X, y, APP_MENU_W, APP_ROW_H, DISPLAY_BLACK);
  display.drawRect(APP_MENU_X, y, APP_MENU_W, APP_ROW_H, accent);

  if (selected) {
    display.fillRect(APP_MENU_X + 2, y + 2, APP_MENU_W - 4, APP_ROW_H - 4, DISPLAY_BLUE);
    display.print(APP_MENU_X + APP_MENU_W - 18, y + APP_ROW_TEXT_Y, ">", DISPLAY_WHITE, 1);
  }

  char numberText[2];
  numberText[0] = (char)('1' + (int)app);
  numberText[1] = '\0';
  display.print(APP_MENU_X + 10, y + APP_ROW_TEXT_Y, numberText, DISPLAY_WHITE, 1);
  display.print(APP_MENU_X + 30, y + APP_ROW_TEXT_Y, appLabel(app), accent, 1);
}

void drawOSMenuScreen() {
  display.clear();
  display.fillRect(20, 14, 280, 42, DISPLAY_BLACK);
  display.drawRect(20, 14, 280, 42, DISPLAY_CYAN);
  display.print(62, 28, "RICK OS APPS", DISPLAY_WHITE, 2);

  for (int i = 0; i <= APP_NOTEPAD; ++i) {
    drawOSMenuRow((OSApp)i, ((OSApp)i == selectedMenuApp));
  }

  display.print(24, 306, "UP/DOWN select", DISPLAY_CYAN, 1);
  display.print(168, 306, "ENTER open", DISPLAY_CYAN, 1);
}

void drawGamesMenuRow(GameItem game, bool selected) {
  int y = APP_FIRST_Y + ((int)game) * (APP_ROW_H + APP_ROW_GAP);
  uint32_t accent = gameColor(game);

  display.fillRect(APP_MENU_X, y, APP_MENU_W, APP_ROW_H, DISPLAY_BLACK);
  display.drawRect(APP_MENU_X, y, APP_MENU_W, APP_ROW_H, accent);

  if (selected) {
    display.fillRect(APP_MENU_X + 2, y + 2, APP_MENU_W - 4, APP_ROW_H - 4, DISPLAY_BLUE);
    display.print(APP_MENU_X + APP_MENU_W - 18, y + APP_ROW_TEXT_Y, ">", DISPLAY_WHITE, 1);
  }

  char numberText[2];
  numberText[0] = (char)('1' + (int)game);
  numberText[1] = '\0';
  display.print(APP_MENU_X + 10, y + APP_ROW_TEXT_Y, numberText, DISPLAY_WHITE, 1);
  display.print(APP_MENU_X + 30, y + APP_ROW_TEXT_Y, gameLabel(game), accent, 1);
}

void drawGamesMenuScreen() {
  display.clear();
  display.fillRect(20, 14, 280, 42, DISPLAY_BLACK);
  display.drawRect(20, 14, 280, 42, DISPLAY_CYAN);
  display.print(82, 28, "GAMES", DISPLAY_WHITE, 2);

  for (int i = 0; i <= GAME_PONG; ++i) {
    drawGamesMenuRow((GameItem)i, ((GameItem)i == selectedGameItem));
  }

  display.print(14, 306, "ESC/M:back", DISPLAY_CYAN, 1);
  display.print(124, 306, "UP/DOWN select", DISPLAY_CYAN, 1);
  display.print(250, 306, "ENTER", DISPLAY_CYAN, 1);
}

void drawPlaceholderScreen(OSApp app) {
  display.clear();
  uint32_t accent = appColor(app);

  display.fillRect(24, 34, 272, 58, DISPLAY_BLACK);
  display.drawRect(24, 34, 272, 58, accent);
  display.print(36, 50, appLabel(app), DISPLAY_WHITE, 2);

  display.drawRect(24, 110, 272, 120, accent);
  display.print(34, 130, "Work in progress", accent, 2);
  display.print(34, 168, "This app skeleton is", DISPLAY_WHITE, 1);
  display.print(34, 184, "ready for implementation.", DISPLAY_WHITE, 1);

  display.print(30, 286, "Esc or M: back to menu", DISPLAY_CYAN, 1);
}

void setupPicoCalc() {
  Serial.begin(115200);

  display.begin();


  gpio_init(LEDPIN);
  gpio_set_dir(LEDPIN, GPIO_OUT);
  gpio_put(LEDPIN, 1);
  sleep_ms(5000);
  gpio_put(LEDPIN, 0);

  // Ensure the SPI pinout the SD card is connected to is configured properly
  // Select the correct SPI based on _MISO pin for the RP2040
  sdCardReady = false;
  SPI.setRX(SD_MISO);
  SPI.setTX(SD_MOSI);
  SPI.setSCK(SD_SCK);
  sdCardReady = SD.begin(SD_CS);


  if (!sdCardReady) {
    Serial.println("initialization failed!");

  } else {
    Serial.println("initialization done.");

    Serial.println("done!");
  }

  init_i2c_kbd();
}

void setup1(void) {
  sound_begin();
}

void loop1(void) {
  sound_update();
}