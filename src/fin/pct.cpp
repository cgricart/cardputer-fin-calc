#include "pct.h"

namespace hp12c {

double pct(double y, double x) {
    return y * x / 100.0;
}

bool pctTotal(double y, double x, double& out) {
    if (y == 0.0) return false;
    out = 100.0 * x / y;
    return true;
}

bool pctChange(double y, double x, double& out) {
    if (y == 0.0) return false;
    out = 100.0 * (x - y) / y;
    return true;
}

}  // namespace hp12c
