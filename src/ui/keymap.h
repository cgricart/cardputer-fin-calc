#pragma once

#include <cstdint>

namespace hp12c {

// Logical 12C key IDs. The dispatcher maps (key, shift) → action.
enum class Key : uint8_t {
    None,

    // digits & dot
    D0, D1, D2, D3, D4, D5, D6, D7, D8, D9, Dot,

    // stack & entry
    Enter, Chs, Eex, Backspace,

    // arithmetic
    Plus, Minus, Times, Div,

    // 12C function-row keys (each has plain / f-shift / g-shift meaning)
    KeyN,        // n        AMORT       12×
    KeyI,        // i        INT          12÷
    KeyPV,       // PV       NPV         END
    KeyPMT,      // PMT      RND         BEG
    KeyFV,       // FV       IRR         CFj
    KeyYx,       // y^x      √x          Nj
    KeyInv,      // 1/x      ŷ,r         x̂,r
    KeySigma,    // Σ+       Σ−          x̄
    KeyDelta,    // Δ%       DATE        DYS
    KeyPctT,     // %T       PRICE       YTM
    KeyClear,    // CHS      CLx         CLΣ        (sharing CHS row)
    KeyClearFin, // EEX      CL FIN      CL REG
    KeySwap,     // x⇄y      x̂          s
    KeyRoll,     // R↓       R↑          x̄
    KeyPct,      // %        LN          e^x

    // shifts and storage
    F, G, Sto, Rcl,

    // TVM solvers (no direct keystroke; invoked via menu Solve forms)
    SolveN, SolveI, SolvePV, SolvePMT, SolveFV,

    // Bond solvers (menu-only)
    SolveYTM, SolveCoupon,

    // Margin solvers (menu-only). Convention: Y = first input, X = second.
    //   *Pct/*Price/*Cost differ in formula; Markup uses cost as base,
    //   Margin (markdown) uses price as base. SolveDollar = X - Y.
    SolveMarkupPct, SolveMarkupPrice, SolveMarkupCost,
    SolveMarginPct, SolveMarginPrice, SolveMarginCost,
    SolveDollar,

    // CAGR solvers (menu-only). Forms route Prior→PV, Future→FV, n→N,
    // CAGR→I (% per period) under the hood.
    SolveCAGR, SolveCAGRFuture, SolveCAGRPrior, SolveCAGRPeriods,

    // misc
    Help, Sleep,
};

// Shift state: F (yellow) and G (blue), one-shot like a real 12C.
enum class Shift : uint8_t { None, F, G };

// Translate a Cardputer character (as returned by M5Cardputer.Keyboard)
// into a logical 12C Key. Returns Key::None if unmapped.
//
// The Cardputer keyboard returns ASCII for letter rows when Shift is held;
// our `g` shift is the user's physical Shift, so we receive uppercase
// letters when g is active. Plain (no-shift) letters are lowercase.
Key keyForChar(char c);

// Returns true if the input character means "press F shift" (the Fn key).
// On the Cardputer, Fn is reported as a separate flag, not a character.
struct KeyEvent {
    Key   key;
    Shift shift;
    bool  fn_held;     // true while Fn key (= F shift) is held this tick
    bool  shift_held;  // true while physical Shift (= G shift) is held
};

}  // namespace hp12c
