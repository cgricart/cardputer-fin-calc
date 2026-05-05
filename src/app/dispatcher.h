#pragma once

#include "../../include/state.h"
#include "../app/errors.h"
#include "../rpn/entry.h"
#include "../ui/keymap.h"

namespace hp12c {

// Owns input → state-mutation logic. The main loop calls handle() once
// per key event. The dispatcher manages:
//   - one-shot F/G shift state
//   - stack-lift suppression after ENTER / CLx
//   - number-entry buffer commit
class Dispatcher {
public:
    Dispatcher(State& s, NumberEntry& entry, ErrorState& err)
        : s_(s), entry_(entry), err_(err) {}

    // Returns true if any state changed (caller may schedule a save / re-render).
    bool handle(Key key, Shift shift_override = Shift::None);

    Shift currentShift() const { return shift_; }

    // Mutators for shift keys. The dispatcher arms the next non-shift key.
    void armF() { shift_ = (shift_ == Shift::F) ? Shift::None : Shift::F; }
    void armG() { shift_ = (shift_ == Shift::G) ? Shift::None : Shift::G; }

private:
    // Commit any in-progress number-entry into X (lifting if needed).
    void commitEntry();
    void liftIfNeeded();

    State&        s_;
    NumberEntry&  entry_;
    ErrorState&   err_;
    Shift         shift_   = Shift::None;
    bool          lift_disabled_ = false;  // set after ENTER/CLx; next number replaces X
    int           pending_sto_  = -1;       // -1 = none; else awaiting register digit
    int           pending_rcl_  = -1;
};

}  // namespace hp12c
