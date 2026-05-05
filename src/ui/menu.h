#pragma once

#include <cstdint>

#include "../../include/state.h"
#include "../rpn/entry.h"
#include "keymap.h"

namespace hp12c {

class Dispatcher;
class ErrorState;

// Form is fully defined in menu.cpp; the Menu only needs an opaque pointer.
struct Form;

// Overlay menu: opens with Esc or 'm', hides the calculator while open.
// Pages group advanced functions by topic; selecting an action either runs
// the calculator key combo directly (no inputs) or opens an input form
// that walks the user through each variable, then runs.
class Menu {
public:
    bool isOpen() const { return open_; }
    void open();
    void close();

    bool handleChar(char c, bool fn_held, bool shift_held,
                    Dispatcher& dispatcher,
                    State& state);

    void render(const State& s) const;

private:
    enum class Mode : uint8_t { List, Form };

    void openForm(const Form* form, State& s);
    void commitEntryToField();
    void applyForm(State& s);
    bool handleFormChar(char c, Dispatcher& dispatcher, State& state);

    bool open_ = false;
    int  page_ = 0;
    int  sel_  = 0;

    Mode  mode_       = Mode::List;
    const Form* form_ = nullptr;
    int8_t      field_idx_     = 0;
    int16_t     loop_count_    = 0;   // # of loop iterations completed
    int16_t     loop_max_      = 0;   // 0 = unlimited; else auto-run at this count
    double      field_vals_[5] = {};
    NumberEntry entry_;
};

}  // namespace hp12c
