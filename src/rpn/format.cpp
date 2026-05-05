#include "format.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace hp12c {

namespace {

void groupThousands(std::string& s, char group_sep) {
    // Group only the integer part (before the decimal char, '.' here).
    size_t dot = s.find('.');
    size_t end = (dot == std::string::npos) ? s.size() : dot;
    size_t start = 0;
    if (!s.empty() && s[0] == '-') start = 1;
    if (end <= start + 3) return;
    size_t insert_pos = end;
    while (insert_pos > start + 3) {
        insert_pos -= 3;
        s.insert(insert_pos, 1, group_sep);
    }
}

}  // namespace

std::string formatNumber(double v, const FormatOpts& opts) {
    if (std::isnan(v)) return "Error";

    // Fall back to scientific if the value can't be shown in the 10-digit
    // window with the requested fix.
    bool sci = opts.sci;
    if (!sci) {
        double absv = std::fabs(v);
        if (absv != 0.0 && (absv >= 1e10 || absv < 1e-4 && opts.fix_digits < 4)) {
            // Heuristic: very large or very small => sci
            sci = (absv >= 1e10) || (absv < 1e-9);
        }
    }

    char tmp[64];
    if (sci) {
        std::snprintf(tmp, sizeof(tmp), "%.*e", opts.fix_digits, v);
    } else {
        std::snprintf(tmp, sizeof(tmp), "%.*f", opts.fix_digits, v);
    }
    std::string out = tmp;

    if (!sci) {
        char group = opts.comma_radix ? '.' : ',';
        groupThousands(out, group);
        if (opts.comma_radix) {
            // Swap '.' (decimal) with ',' AFTER grouping so we don't confuse
            // groupThousands. We grouped with '.', and the snprintf decimal
            // is also '.', so detect the radix as the LAST '.'.
            size_t last = out.find_last_of('.');
            if (last != std::string::npos && opts.fix_digits > 0)
                out[last] = ',';
        }
    }
    return out;
}

}  // namespace hp12c
