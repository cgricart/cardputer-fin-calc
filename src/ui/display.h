#pragma once

#include <string>
#include "../../include/state.h"

namespace hp12c {

class ErrorState;
class NumberEntry;

// Hybrid display: large X line, small Y/Z/T preview, status row, footer.
//
// The platform's M5GFX display is referenced through a forward-declared
// pointer so this header doesn't pull in <M5Cardputer.h> for unit tests.
class Display {
public:
    void begin();
    void render(const State& s,
                const NumberEntry& entry,
                const ErrorState& err,
                bool shift_f,
                bool shift_g);

    // One-time boot splash: "Financial Calculator / by CGR" for ~1.8s.
    void splash();

    // Flash a transient message (e.g. "running") for `ms` milliseconds.
    void flash(const char* msg, uint32_t ms = 600);

private:
    bool dirty_ = true;
};

}  // namespace hp12c
