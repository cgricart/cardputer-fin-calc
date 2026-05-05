#include "entry.h"

#include <cstdlib>

namespace hp12c {

namespace {
constexpr size_t kMaxMantissa = 10;  // HP 12C shows 10 digits of mantissa
constexpr size_t kMaxExpDigits = 2;
}

bool NumberEntry::digit(char c) {
    if (c < '0' || c > '9') return false;
    active_ = true;
    if (eex_) {
        if (eex_digits_ >= kMaxExpDigits) return false;
        buf_.push_back(c);
        ++eex_digits_;
        return true;
    }
    // Count significant mantissa digits (excluding leading zeros and dot).
    size_t mant_digits = 0;
    bool started = false;
    for (char ch : buf_) {
        if (ch == '-' || ch == '.') continue;
        if (!started && ch == '0') continue;
        started = true;
        ++mant_digits;
    }
    if (mant_digits >= kMaxMantissa) return false;
    buf_.push_back(c);
    return true;
}

void NumberEntry::dot() {
    active_ = true;
    if (eex_ || has_dot_) return;
    if (buf_.empty() || buf_ == "-") buf_.push_back('0');
    buf_.push_back('.');
    has_dot_ = true;
}

void NumberEntry::chs() {
    if (eex_) {
        // Toggle exponent sign: find the 'e' and flip the next char.
        size_t e = buf_.find('e');
        if (e == std::string::npos) return;
        if (e + 1 < buf_.size() && buf_[e + 1] == '-') {
            buf_.erase(e + 1, 1);
            eex_neg_ = false;
        } else {
            buf_.insert(e + 1, "-");
            eex_neg_ = true;
        }
        return;
    }
    if (buf_.empty()) {
        buf_ = "-";
        active_ = true;
        return;
    }
    if (buf_.front() == '-') buf_.erase(0, 1);
    else                     buf_.insert(0, "-");
    active_ = true;
}

void NumberEntry::eex() {
    if (eex_) return;
    active_ = true;
    if (buf_.empty()) buf_ = "1";
    buf_.push_back('e');
    eex_ = true;
    eex_digits_ = 0;
    eex_neg_ = false;
}

bool NumberEntry::backspace() {
    if (buf_.empty()) {
        active_ = false;
        return true;
    }
    char dropped = buf_.back();
    buf_.pop_back();
    if (dropped == '.') has_dot_ = false;
    if (dropped == 'e') { eex_ = false; eex_digits_ = 0; eex_neg_ = false; }
    else if (eex_ && dropped >= '0' && dropped <= '9' && eex_digits_ > 0) {
        --eex_digits_;
    }
    if (buf_.empty()) {
        active_ = false;
        return true;
    }
    return false;
}

double NumberEntry::value() const {
    if (buf_.empty() || buf_ == "-") return 0.0;
    return std::strtod(buf_.c_str(), nullptr);
}

}  // namespace hp12c
