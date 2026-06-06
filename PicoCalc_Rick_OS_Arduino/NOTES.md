# Pico Sudoku

## Data Types Reference

| Data Type | Size | Description |
| --- | --- | --- |
| `boolean` | 8 bit | `true` / `false` |
| `byte` | 8 bit | 0-255 unsigned number |
| `char` | 8 bit | -128 to 127 signed number |
| `unsigned char` | 8 bit | -128 to 127 signed number |
| `word` | 16 bit | 0-65535 unsigned number |
| `unsigned int` | 16 bit | 0-65535 unsigned number |
| `int` | 16 bit | -32768 to 32767 signed number |
| `unsigned long` | 32 bit | 0-4,294,967,295 unsigned number, usually for `millis()` |
| `long` | 32 bit | -2,147,483,648 to 2,147,483,647 signed number |
| `float` | 32 bit | -3.4028235E38 to 3.4028235E38 signed number |
| `uint8_t` | 8 bit | 0-255 unsigned number |
| `int8_t` | 8 bit | -127 to 127 signed number |
| `uint16_t` | 16 bit | 0-65,535 unsigned number |
| `int16_t` | 16 bit | -32,768 to 32,767 signed number |
| `uint32_t` | 32 bit | 0-4,294,967,295 unsigned number |
| `int32_t` | 32 bit | -2,147,483,648 to 2,147,483,647 signed number |
| `uint64_t` | 64 bit | 0-18,446,744,073,709,551,615 unsigned number |
| `int64_t` | 64 bit | -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807 signed number |

## Naming Conventions

| Style | Use |
| --- | --- |
| `camelCase` | Anything that changes |
| `snake_case` | Variables that are exclusive in a function |
| `Snake_Case` | Class or struct exclusive variables/functions |
| `iNVERTEDcAMELcASE` | Outside code that is being accessed, such as a database |
| `SNake_CAse` | Duplicate variables inside the case function, frequently used in library names |
| `ALL_CAPS` | Constant variable names or defines |

*by-jediRick*