#pragma once

#include <string>

namespace hp12c {

struct FormatOpts {
    int  fix_digits = 2;     // 0..9
    bool comma_radix = false; // true => "1.234,56"
    bool sci = false;         // forced scientific (overflow / very small)
};

// Format a number HP 12C-style with thousands separators.
std::string formatNumber(double v, const FormatOpts& opts);

}  // namespace hp12c
