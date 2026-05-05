#pragma once

#include "state.h"

namespace hp12c {

// NVS-backed persistence for the calculator State. On non-Arduino builds
// (UNIT_TEST), these are no-ops returning false / true as appropriate so
// math modules can be tested without flash.

class Persistence {
public:
    void begin();

    // Load returns true if a valid versioned blob was found.
    bool load(State& s);

    // Mark state as dirty; tick() will flush after `kSaveDebounceMs`.
    void markDirty();

    // Call from loop(); writes to NVS once the debounce window elapses
    // since the last markDirty().
    void tick(State& s, uint32_t now_ms);

    // Force an immediate flush (e.g. before sleep).
    void flush(State& s);

    static constexpr uint32_t kSaveDebounceMs = 2000;

private:
    bool     dirty_      = false;
    uint32_t dirty_at_ms_ = 0;
};

}  // namespace hp12c
