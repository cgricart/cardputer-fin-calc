#pragma once

#include <cstdint>

namespace hp12c {

// HP 12C error codes (subset that this firmware can raise).
//   0  improper math (÷ by 0, log of ≤0, sqrt of <0, etc.)
//   1  storage register overflow / out of range
//   2  improper statistics (n=0, divide by 0 in regression)
//   3  improper IRR
//   4  improper memory / cash-flow index
//   5  TVM no solution / Newton failed to converge
//   6  improper register number
//   7  IRR with no sign change
//   8  improper date / calendar
//   9  bond / depreciation arg out of range
enum class Error : uint8_t {
    None = 0xFF,
    E0 = 0, E1 = 1, E2 = 2, E3 = 3, E4 = 4,
    E5 = 5, E6 = 6, E7 = 7, E8 = 8, E9 = 9
};

class ErrorState {
public:
    void raise(Error e) { current_ = e; }
    void clear()        { current_ = Error::None; }
    Error current() const { return current_; }
    bool active() const   { return current_ != Error::None; }
private:
    Error current_ = Error::None;
};

}  // namespace hp12c
