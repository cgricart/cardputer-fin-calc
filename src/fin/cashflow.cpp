#include "cashflow.h"

#include <cmath>

namespace hp12c {

bool appendCashFlow(State& s, double cf, uint16_t nj) {
    if (s.cf_count >= kMaxCashFlows) return false;
    s.CF[s.cf_count] = cf;
    s.Nj[s.cf_count] = (s.cf_count == 0) ? 1 : (nj == 0 ? 1 : nj);
    ++s.cf_count;
    return true;
}

bool setLastNj(State& s, uint16_t nj) {
    if (s.cf_count <= 1) return false;  // can't set Nj of CF0
    s.Nj[s.cf_count - 1] = (nj == 0) ? 1 : nj;
    return true;
}

namespace {

// Walk the CF list expanding Nj repeats; accumulate Σ CFj / (1+i)^period.
double sumPV(const State& s, double i) {
    if (s.cf_count == 0) return 0.0;
    double v = 1.0 + i;
    if (v <= 0.0) return std::nan("");
    double total = s.CF[0];        // CF0 lives at period 0
    int period = 0;
    for (uint8_t j = 1; j < s.cf_count; ++j) {
        uint16_t reps = s.Nj[j] == 0 ? 1 : s.Nj[j];
        for (uint16_t k = 0; k < reps; ++k) {
            ++period;
            total += s.CF[j] / std::pow(v, period);
        }
    }
    return total;
}

}  // namespace

bool computeNPV(const State& s, double i, double& out) {
    if (s.cf_count == 0) return false;
    double r = sumPV(s, i);
    if (!std::isfinite(r)) return false;
    out = r;
    return true;
}

bool computeNFV(const State& s, double i, double& out) {
    double npv;
    if (!computeNPV(s, i, npv)) return false;
    // total periods = sum of Nj over j>=1
    long total = 0;
    for (uint8_t j = 1; j < s.cf_count; ++j)
        total += (s.Nj[j] == 0 ? 1 : s.Nj[j]);
    out = npv * std::pow(1.0 + i, (double)total);
    return std::isfinite(out);
}

bool computeIRR(const State& s, double& out) {
    if (s.cf_count < 2) return false;

    // Sign-change check: at least one positive and one negative CF.
    bool has_pos = false, has_neg = false;
    for (uint8_t j = 0; j < s.cf_count; ++j) {
        if (s.CF[j] > 0) has_pos = true;
        if (s.CF[j] < 0) has_neg = true;
    }
    if (!(has_pos && has_neg)) return false;

    // Bracket: scan rates from -0.99 to 10.0 in coarse steps for a sign change.
    double lo = -0.99, hi = 10.0;
    double f_lo, f_hi;
    if (!computeNPV(s, lo, f_lo)) return false;
    bool bracketed = false;
    double a = lo, fa = f_lo;
    constexpr int kScan = 50;
    for (int k = 1; k <= kScan; ++k) {
        double t = lo + (hi - lo) * k / kScan;
        double ft;
        if (!computeNPV(s, t, ft)) continue;
        if (fa * ft < 0.0) { lo = a; hi = t; f_lo = fa; f_hi = ft; bracketed = true; break; }
        a = t; fa = ft;
    }
    if (!bracketed) return false;

    // Bisection — robust, fast enough for ≤20 cash flows.
    constexpr int kMaxIter = 200;
    constexpr double kTol = 1e-10;
    for (int k = 0; k < kMaxIter; ++k) {
        double mid = 0.5 * (lo + hi);
        double fm;
        if (!computeNPV(s, mid, fm)) return false;
        if (std::fabs(fm) < kTol || (hi - lo) < kTol) {
            out = mid;
            return true;
        }
        if (fm * f_lo < 0.0) { hi = mid; f_hi = fm; }
        else                 { lo = mid; f_lo = fm; }
    }
    out = 0.5 * (lo + hi);
    return true;
}

}  // namespace hp12c
