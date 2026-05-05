#pragma once

#include <cstdint>
#include <string>

namespace hp12c {

// Number-entry buffer. The dispatcher accumulates digits here while the
// user is typing; when an op fires, finish() commits the parsed value to
// the stack and clears the buffer.
class NumberEntry {
public:
    void reset() {
        buf_.clear();
        has_dot_ = false;
        eex_ = false;
        eex_neg_ = false;
        eex_digits_ = 0;
        active_ = false;
    }

    bool active() const { return active_; }
    const std::string& buffer() const { return buf_; }

    // Append a digit '0'..'9'. Returns false if buffer full.
    bool digit(char c);

    // Place decimal point. No-op if already present (or in EEX exponent).
    void dot();

    // Toggle sign of mantissa or exponent depending on entry mode.
    void chs();

    // Begin exponent entry. No-op if no mantissa typed yet.
    void eex();

    // Backspace: remove last character. Returns true if buffer became empty.
    bool backspace();

    // Parse current buffer to a double. If buffer is empty, returns 0.
    double value() const;

private:
    std::string buf_;
    bool has_dot_     = false;
    bool eex_         = false;
    bool eex_neg_     = false;
    uint8_t eex_digits_ = 0;
    bool active_      = false;
};

}  // namespace hp12c
