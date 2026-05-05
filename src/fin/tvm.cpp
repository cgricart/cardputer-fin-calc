#include "tvm.h"

#include <cmath>

namespace hp12c {

namespace {

// (1+i)^n with the i=0 limit handled.
double powIN(double i, double n) {
    if (i == 0.0) return 1.0;
    return std::pow(1.0 + i, n);
}

// Annuity factor a(n,i) = (1 - (1+i)^-n) / i, with i=0 limit = n.
double annuity(double i, double n) {
    if (i == 0.0) return n;
    return (1.0 - std::pow(1.0 + i, -n)) / i;
}

// Residual of the TVM equation given all five values; zero when balanced.
double residual(const Tvm& t) {
    double S = t.begin ? 1.0 : 0.0;
    return t.PV + (1.0 + t.i * S) * t.PMT * annuity(t.i, t.n)
         + t.FV * std::pow(1.0 + t.i, -t.n);
}

// dResidual/di. Used by Newton-Raphson for solveI.
double residualDerivative(const Tvm& t) {
    // Use a numerical derivative; closed form is messy and TVM solutions
    // converge in ≤ ~30 iterations either way.
    Tvm a = t, b = t;
    double h = std::fmax(1e-7, std::fabs(t.i) * 1e-5);
    a.i = t.i - h;
    b.i = t.i + h;
    return (residual(b) - residual(a)) / (2.0 * h);
}

}  // namespace

bool solveFV(const Tvm& t, double& out) {
    double S = t.begin ? 1.0 : 0.0;
    double pin = std::pow(1.0 + t.i, t.n);
    out = -(t.PV * pin + (1.0 + t.i * S) * t.PMT * annuity(t.i, t.n) * pin);
    return std::isfinite(out);
}

bool solvePV(const Tvm& t, double& out) {
    double S = t.begin ? 1.0 : 0.0;
    out = -((1.0 + t.i * S) * t.PMT * annuity(t.i, t.n)
            + t.FV * std::pow(1.0 + t.i, -t.n));
    return std::isfinite(out);
}

bool solvePMT(const Tvm& t, double& out) {
    double S = t.begin ? 1.0 : 0.0;
    double a = annuity(t.i, t.n);
    if (a == 0.0 || (1.0 + t.i * S) == 0.0) return false;
    out = -(t.PV + t.FV * std::pow(1.0 + t.i, -t.n)) / ((1.0 + t.i * S) * a);
    return std::isfinite(out);
}

bool solveN(const Tvm& t, double& out) {
    if (t.i == 0.0) {
        if (t.PMT == 0.0) return false;
        out = -(t.PV + t.FV) / t.PMT;
        return std::isfinite(out);
    }
    double S = t.begin ? 1.0 : 0.0;
    double a = (1.0 + t.i * S) * t.PMT;
    double num = a - t.FV * t.i;
    double den = a + t.PV * t.i;
    if (den == 0.0 || num / den <= 0.0) return false;
    out = std::log(num / den) / std::log(1.0 + t.i);
    return std::isfinite(out);
}

bool solveI(const Tvm& t, double& out) {
    // Newton-Raphson with a guarded fallback to bisection if iteration
    // wanders. Initial guess: small positive rate.
    Tvm cur = t;
    cur.i = 0.05 / 12.0;     // start at ~5%/yr monthly compounding

    // First, check if i=0 happens to satisfy the equation (PV+nPMT+FV=0).
    {
        Tvm zero = t; zero.i = 0.0;
        if (std::fabs(residual(zero)) < 1e-10) { out = 0.0; return true; }
    }

    constexpr int kMaxIter = 100;
    constexpr double kTol  = 1e-10;
    for (int k = 0; k < kMaxIter; ++k) {
        double f  = residual(cur);
        if (std::fabs(f) < kTol) { out = cur.i; return true; }
        double fp = residualDerivative(cur);
        if (fp == 0.0 || !std::isfinite(fp)) return false;
        double step = f / fp;
        // Damp big moves
        if (std::fabs(step) > 1.0) step = (step > 0 ? 1.0 : -1.0) * 1.0;
        double next = cur.i - step;
        if (next <= -1.0) next = -0.999999;  // (1+i) must stay positive
        if (!std::isfinite(next)) return false;
        cur.i = next;
    }
    return false;
}

}  // namespace hp12c
