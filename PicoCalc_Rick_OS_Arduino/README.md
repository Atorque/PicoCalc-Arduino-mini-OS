# Rick OS for PicoCalc

Keyboard-driven embedded OS shell for RP2040 + TFT + SD + stereo PWM audio.

Rick OS started as a Sudoku prototype and evolved into a modular multi-app system with games, productivity apps, persistent storage, and a reusable sound engine.

## Highlights

- Multi-screen OS menu with app/game routing
- 4 games: Sudoku, Tetris, Snake, Pong
- 5 productivity apps: Calculator, File Explorer, Notepad, Calendar, Budget
- SD-backed persistence for text/calendar/budget data
- Stereo audio engine with UI SFX and dedicated Tetris music mode
- Partial redraw strategy across major apps to reduce flicker

## Quick Start

1. Install RP2040 board support in Arduino IDE. Tested with Philhower III core and Pico 2 RP2350.
2. Open [PicoCalc_Rick_OS_Arduino.ino](PicoCalc_Rick_OS_Arduino.ino).
3. Select your RP2040 board + serial port.
4. Ensure required hardware is connected (display, keyboard, SD, audio pins).
5. Build and upload.

## Roadmap Status

- [x] Continue extracting Sudoku/game logic from PicoCalc_Rick_OS_Arduino.ino into dedicated modules
- [x] Move audio generation into separate files
- [x] Turn audio into a sound library so we can play music for games, use sound effects for OS
- [x] Make a basic menu system
- [x] Implement one real app first (Calculator is quickest), replacing its placeholder screen
- [x] Add SD card-backed File Explorer app using your existing SD init path
- [x] Make a Notepad application
- [x] Make Calculator application
- [x] Make Calendar application
- [x] Make a Budget program (debits, credits, items can be marked as one time or reoccuring, and balance)
- [x] Make Tetris
- [x] Make Snake
- [x] Make Pong

## Feature Matrix

| App | Category | Core Features | Persistence | Render Strategy |
|---|---|---|---|---|
| Sudoku | Game | Difficulty select, puzzle play, scoring/checking | No | Mixed/full + targeted intro updates |
| Tetris | Game | Rotation, line clear, score/level, next piece, ghost piece | No | Dirty-cell partial redraw |
| Snake | Game | Growth, collisions, score/level | No | Dirty-cell partial redraw |
| Pong | Game | Player vs CPU, scoring, game-over | No | Partial playfield redraw |
| Calculator | Productivity | Scientific expression parsing, full formula input | No | Region-based partial redraw |
| File Explorer | Productivity | SD directory/file browsing | SD card | List/row partial redraw |
| Notepad | Productivity | Cursor editor, Esc menu actions, TXT picker | SD card | Region-based partial redraw |
| Calendar | Productivity | Day entries, month overlay picker | SD card (/CALENDAR.TXT) | Partial redraw + full restore on overlay close |
| Budget | Productivity | Credit/debit + recurring flags + balance | SD card (/BUDGET.TXT) | Viewport + partial redraw |

## Controls

### Global

| Key | Action |
|---|---|
| Arrow keys / WASD | Navigate or move (screen dependent) |
| Enter | Confirm/select |
| Esc or M | Back/close/exit (screen dependent) |

### Notable App-Specific

| App | Key | Action |
|---|---|---|
| Notepad | Esc | Open command menu (Exit/Save/Name/Open) |
| Calendar | M | Open month picker overlay |
| Budget | C/D/R/F | Add credit/debit one-time/recurring |
| Tetris | Enter/Space | Hard drop |
| Snake/Pong | Enter | Restart after game over |

## Audio

Audio is implemented via [SoundEngine.h](SoundEngine.h) and [SoundEngine.cpp](SoundEngine.cpp), backed by [pwm_sound.h](pwm_sound.h) and [pwm_sound.ino](pwm_sound.ino).

Capabilities:

- Stereo PWM output
- UI + gameplay SFX
- Music mode switching by screen
- Dedicated Tetris music sequencer based on [TETRIS.md](TETRIS.md)

## Storage

SD-backed files currently used:

- Calendar entries: /CALENDAR.TXT
- Budget entries: /BUDGET.TXT
- Notepad text files: user-selected .TXT files (root)

## Hardware Notes

Current pin constants in [PicoCalc_Rick_OS_Arduino.ino](PicoCalc_Rick_OS_Arduino.ino):

- SD SPI: MISO 16, MOSI 19, SCK 18, CS 17
- Stereo audio: Left 26, Right 27

## Project Layout

Core router:

- [PicoCalc_Rick_OS_Arduino.ino](PicoCalc_Rick_OS_Arduino.ino)

Display/input/audio:

- [PicoCalc_Display.h](PicoCalc_Display.h), [PicoCalc_Display.cpp](PicoCalc_Display.cpp)
- [i2ckbd.h](i2ckbd.h), [i2ckbd.ino](i2ckbd.ino)
- [SoundEngine.h](SoundEngine.h), [SoundEngine.cpp](SoundEngine.cpp)
- [pwm_sound.h](pwm_sound.h), [pwm_sound.ino](pwm_sound.ino)

Apps/games:

- [SudokuGame.h](SudokuGame.h), [SudokuGame.cpp](SudokuGame.cpp)
- [TetrisApp.h](TetrisApp.h), [TetrisApp.cpp](TetrisApp.cpp)
- [SnakeApp.h](SnakeApp.h), [SnakeApp.cpp](SnakeApp.cpp)
- [PongApp.h](PongApp.h), [PongApp.cpp](PongApp.cpp)
- [CalculatorApp.h](CalculatorApp.h), [CalculatorApp.cpp](CalculatorApp.cpp)
- [FileExplorerApp.h](FileExplorerApp.h), [FileExplorerApp.cpp](FileExplorerApp.cpp)
- [NotepadApp.h](NotepadApp.h), [NotepadApp.cpp](NotepadApp.cpp)
- [CalendarApp.h](CalendarApp.h), [CalendarApp.cpp](CalendarApp.cpp)
- [BudgetApp.h](BudgetApp.h), [BudgetApp.cpp](BudgetApp.cpp)

## Troubleshooting

### IntelliSense says headers are missing (for example SPI.h)

This is often an editor include-path configuration issue, not a firmware logic issue. Verify board package/toolchain configuration in Arduino IDE.

### SD features do not work

Check SD wiring and card formatting, then verify SD init success in startup logs.

### Audio silent or one channel missing

Verify PWM audio pin wiring and speaker/amplifier path for pins 26 (L) and 27 (R).

## License

Use according to your project license policy (add a LICENSE file if you plan to publish publicly).

