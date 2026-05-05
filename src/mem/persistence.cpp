#include "persistence.h"

#ifndef UNIT_TEST
  #include <Preferences.h>
  static Preferences gPrefs;
#endif

namespace hp12c {

namespace {
constexpr const char* kNs  = "12c";
constexpr const char* kKey = "state";
}

void Persistence::begin() {
#ifndef UNIT_TEST
    gPrefs.begin(kNs, /*readOnly=*/false);
#endif
}

bool Persistence::load(State& s) {
#ifndef UNIT_TEST
    size_t got = gPrefs.getBytesLength(kKey);
    if (got != sizeof(State)) return false;
    State tmp{};
    size_t read = gPrefs.getBytes(kKey, &tmp, sizeof(State));
    if (read != sizeof(State)) return false;
    if (tmp.version != kStateVersion) return false;
    s = tmp;
    return true;
#else
    (void)s;
    return false;
#endif
}

void Persistence::markDirty() {
    dirty_ = true;
    // dirty_at_ms_ is updated in tick() once we know `now`; mark a sentinel.
    dirty_at_ms_ = 0;
}

void Persistence::tick(State& s, uint32_t now_ms) {
    if (!dirty_) return;
    if (dirty_at_ms_ == 0) {
        dirty_at_ms_ = now_ms ? now_ms : 1;  // avoid colliding with sentinel
        return;
    }
    if (now_ms - dirty_at_ms_ >= kSaveDebounceMs) {
        flush(s);
    }
}

void Persistence::flush(State& s) {
    s.version = kStateVersion;
#ifndef UNIT_TEST
    gPrefs.putBytes(kKey, &s, sizeof(State));
#endif
    dirty_ = false;
    dirty_at_ms_ = 0;
}

}  // namespace hp12c
