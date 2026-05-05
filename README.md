# Cardputer 12C

A faithful HP 12C-style RPN financial calculator for the
[M5Stack Cardputer](https://docs.m5stack.com/en/core/Cardputer).

Built on the Arduino framework using the official `M5Cardputer` library.
Features a 4-level RPN stack, all HP 12C financial functions (TVM,
amortization, NPV/IRR, bonds, depreciation, date math, statistics), 20
storage registers, and persistent NVS-backed continuous memory.

Keystroke programming (R/S, GTO, LBL) is intentionally omitted.

## Build & flash

Requires [PlatformIO](https://platformio.org/install).

```sh
pio run -e cardputer -t upload
pio device monitor
```

## Run host tests

```sh
pio test -e native
```

Or, without PlatformIO, the lightweight smoke test:

```sh
clang++ -std=gnu++17 -DUNIT_TEST -Iinclude -Isrc \
  src/rpn/*.cpp src/fin/*.cpp src/stats/*.cpp \
  src/mem/registers.cpp src/app/errors.cpp \
  test/native/smoke.cpp -o /tmp/hp12c_smoke && /tmp/hp12c_smoke
```

## Keymap

The Cardputer's **Fn** key acts as `f` (yellow shift) and **Shift** acts
as `g` (blue shift). Type a digit, then a function key — or press `f`
or `g` first to access the shifted meaning.

| Key | Plain | f-shift (Fn) | g-shift (Shift) |
|---|---|---|---|
| `0`–`9`, `.` | digit / dot | FIX *n* | (function digit) |
| `Enter` | ENTER | — | LSTx |
| `+ − × ÷` | arithmetic | — | — |
| `n` | n | AMORT | 12× |
| `i` | i | INT | 12÷ |
| `p` | PV | NPV | END |
| `a` | PMT | RND | BEG |
| `m` | open menu | — | — |
| `v` | FV | IRR | CFj |
| `y` | yˣ | √x | Nj |
| `r` | 1/x | ŷ,r | x̂,r |
| `s` | Σ+ | Σ− | x̄ |
| `d` | Δ% | DATE | DYS |
| `b` | %T | PRICE | YTM |
| `c` | CHS | CLx | CLΣ |
| `e` | EEX | CL FIN | CL REG |
| `x` | x⇄y | x̂ | s |
| `o` | R↓ | R↑ | x̄ |
| `t` | % | LN | eˣ |
| `Tab` | STO + digit | — | RCL + digit |
| `Backspace` | ← (delete digit) | — | — |
| `Esc` | sleep | — | — |

## Files

```
src/
  rpn/       4-level stack, number entry, FIX/SCI formatting
  fin/       TVM, amort, NPV/IRR, bond, depreciation, dates, percent
  stats/     Σ+/Σ-, mean, std-dev, linear regression
  mem/       storage registers, NVS persistence (debounced)
  ui/        display layout, keymap, annunciators
  app/       dispatcher (key+shift → action), error codes
  main.cpp   setup/loop wiring
include/state.h   the persisted machine state
test/native/      Unity tests + standalone smoke test
```

## What's not implemented

- Keystroke programming (R/S, GTO, LBL, PSE, conditional tests)
- HP 12C Platinum's algebraic mode and undo buffer
- 12C-style display-rounded amortization (we keep full precision; see
  the comment in `src/fin/amort.cpp`)
- Wi-Fi / OTA / SD-card features of the Cardputer hardware
