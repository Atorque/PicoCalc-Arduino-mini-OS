

# Sudoku Generation Steps

| Step | Description |
| --- | --- |
| Create Fully Solved Grid | Fill a 9x9 grid with numbers 1-9, ensuring no repeats in rows, columns, or subgrids. |
| Remove Numbers | Randomly remove numbers while checking for uniqueness of the solution. |
| Ensure Unique Solution | Verify that the remaining numbers allow for only one solution. |
| Finalize Puzzle | Adjust difficulty and finalize the grid for presentation. |

# Difficulty Levels

## Implemented

| Level | Removed Cells |
| --- | --- |
| Easy | 30 |
| Medium | 36 |
| Hard | 45 |
| Expert | 50 |
| Master | 54 |
| Extreme | 58 |

## Controls

- Intro screen: select with 1-6 or Up/Down, then Enter or G to start.
- Game screen: press G for a new puzzle with the current difficulty.
- Game screen: press D or Esc to return to the difficulty selection screen.
- Game screen: use arrow keys (or W/A/S and keyboard right arrow) to move selection.
- Game screen: use 1-9 to enter value in editable cells.
- Game screen: use 0, Backspace, or Delete to clear editable cells.
- Game screen: press C to check the selected cell against the solution.
- Game screen: press V to solve the puzzle and display score based on correct editable entries.

## Completed Steps

- [x] Create a Generator
- [x] Create the game board grid
- [x] Create a launch screen
- [x] Create difficulty levels: Easy, Medium, Hard, Expert, Master, Extreme
- [x] Implement arrow key movement, and number input in puzzle
- [x] Escape key returns to menu
- [x] Implement a check function so user can check the current number correctness
- [x] Implement solve where we check the user puzzle against the generated puzzle, then give score
