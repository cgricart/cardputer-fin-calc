#include "keymap.h"

namespace hp12c {

Key keyForChar(char c) {
    switch (c) {
        case '0': return Key::D0;
        case '1': return Key::D1;
        case '2': return Key::D2;
        case '3': return Key::D3;
        case '4': return Key::D4;
        case '5': return Key::D5;
        case '6': return Key::D6;
        case '7': return Key::D7;
        case '8': return Key::D8;
        case '9': return Key::D9;
        case '.': return Key::Dot;

        case '+': return Key::Plus;
        case '-': return Key::Minus;
        case '*': return Key::Times;
        case '/': return Key::Div;

        case '\r':
        case '\n': return Key::Enter;
        case '\b': return Key::Backspace;
        case 0x1b: return Key::Sleep;

        // Letter row → 12C functions. The dispatcher decides what each
        // does based on the active shift.
        case 'n': case 'N': return Key::KeyN;
        case 'i': case 'I': return Key::KeyI;
        case 'p': case 'P': return Key::KeyPV;
        case 'a': case 'A': return Key::KeyPMT;
        // 'm' is reserved for the menu (handled in main.cpp before keymap).
        case 'v': case 'V': return Key::KeyFV;
        case 'y': case 'Y': return Key::KeyYx;
        case 'r': case 'R': return Key::KeyInv;
        case 's': case 'S': return Key::KeySigma;
        case 'd': case 'D': return Key::KeyDelta;
        case 'b': case 'B': return Key::KeyPctT;
        case 'c': case 'C': return Key::KeyClear;
        case 'e': case 'E': return Key::KeyClearFin;
        case 'x': case 'X': return Key::KeySwap;
        case 'o': case 'O': return Key::KeyRoll;
        case 't': case 'T': return Key::KeyPct;

        case 'h': case 'H': return Key::Chs;       // 'h' = CHS (HP convention)
        case '\t': return Key::Sto;                 // Tab = STO (Shift+Tab = RCL)

        case '?': return Key::Help;
    }
    return Key::None;
}

}  // namespace hp12c
