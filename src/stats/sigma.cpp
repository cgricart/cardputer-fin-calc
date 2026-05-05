#include "sigma.h"

#include <cmath>

namespace hp12c {

namespace {
inline double& N (State& s) { return s.R[1]; }
inline double& Sx(State& s) { return s.R[2]; }
inline double& Sxx(State& s){ return s.R[3]; }
inline double& Sy(State& s) { return s.R[4]; }
inline double& Syy(State& s){ return s.R[5]; }
inline double& Sxy(State& s){ return s.R[6]; }
inline double  N (const State& s) { return s.R[1]; }
inline double  Sx(const State& s) { return s.R[2]; }
inline double  Sxx(const State& s){ return s.R[3]; }
inline double  Sy(const State& s) { return s.R[4]; }
inline double  Syy(const State& s){ return s.R[5]; }
inline double  Sxy(const State& s){ return s.R[6]; }
}  // namespace

void sigmaPlus(State& s, double x, double y) {
    N(s)   += 1.0;
    Sx(s)  += x;
    Sxx(s) += x*x;
    Sy(s)  += y;
    Syy(s) += y*y;
    Sxy(s) += x*y;
}

void sigmaMinus(State& s, double x, double y) {
    N(s)   -= 1.0;
    Sx(s)  -= x;
    Sxx(s) -= x*x;
    Sy(s)  -= y;
    Syy(s) -= y*y;
    Sxy(s) -= x*y;
}

bool meanX(const State& s, double& out) {
    if (N(s) <= 0) return false;
    out = Sx(s) / N(s);
    return true;
}
bool meanY(const State& s, double& out) {
    if (N(s) <= 0) return false;
    out = Sy(s) / N(s);
    return true;
}

bool meanXWeighted(const State& s, double& out) {
    if (Sy(s) == 0.0) return false;
    out = Sxy(s) / Sy(s);
    return true;
}

bool stdDevX(const State& s, double& out) {
    double n = N(s);
    if (n < 2.0) return false;
    double var = (Sxx(s) - Sx(s)*Sx(s)/n) / (n - 1.0);
    if (var < 0) var = 0;
    out = std::sqrt(var);
    return true;
}

bool stdDevY(const State& s, double& out) {
    double n = N(s);
    if (n < 2.0) return false;
    double var = (Syy(s) - Sy(s)*Sy(s)/n) / (n - 1.0);
    if (var < 0) var = 0;
    out = std::sqrt(var);
    return true;
}

bool linRegression(const State& s, double& slope, double& intercept, double& r) {
    double n = N(s);
    if (n < 2.0) return false;
    double denom_x = n*Sxx(s) - Sx(s)*Sx(s);
    double denom_y = n*Syy(s) - Sy(s)*Sy(s);
    double covar   = n*Sxy(s) - Sx(s)*Sy(s);
    if (denom_x == 0.0) return false;
    slope     = covar / denom_x;
    intercept = (Sy(s) - slope*Sx(s)) / n;
    r = (denom_y > 0)
            ? covar / std::sqrt(denom_x * denom_y)
            : 0.0;
    return std::isfinite(slope) && std::isfinite(intercept);
}

bool predictY(const State& s, double x, double& y_hat, double& r) {
    double m, b;
    if (!linRegression(s, m, b, r)) return false;
    y_hat = m*x + b;
    return true;
}

bool predictX(const State& s, double y, double& x_hat, double& r) {
    double m, b;
    if (!linRegression(s, m, b, r)) return false;
    if (m == 0.0) return false;
    x_hat = (y - b) / m;
    return true;
}

}  // namespace hp12c
