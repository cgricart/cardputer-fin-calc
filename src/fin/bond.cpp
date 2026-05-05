#include "bond.h"

#include <cmath>

namespace hp12c {

namespace {

// Day count between two dates per requested basis.
long dayCount(const Date& a, const Date& b, bool thirty360) {
    long d = 0;
    if (thirty360) days360(a, b, d);
    else            daysActual(a, b, d);
    return d;
}

// Find the most recent coupon date on or before `settlement`, given the
// maturity date and semiannual coupon frequency. We work backwards by
// 6 months from maturity.
Date previousCouponDate(const Date& settlement, const Date& maturity) {
    Date c = maturity;
    while (true) {
        // Step back 6 months
        Date prev = c;
        prev.m -= 6;
        if (prev.m <= 0) { prev.m += 12; prev.y -= 1; }
        // Clamp day if overshoot (e.g. Aug 31 -> Feb 31 invalid)
        // Use last day of target month if needed.
        static const int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        bool leap = (prev.y % 4 == 0 && prev.y % 100 != 0) || (prev.y % 400 == 0);
        int dmax = dim[prev.m - 1] + (prev.m == 2 && leap ? 1 : 0);
        if (prev.d > dmax) prev.d = dmax;

        long diff;
        daysActual(prev, settlement, diff);
        if (diff >= 0) return prev;
        c = prev;
    }
}

}  // namespace

bool bondPrice(const BondInputs& in, BondPrice& out) {
    if (!in.settlement.valid() || !in.maturity.valid()) return false;
    long settle_to_mat;
    if (!daysActual(in.settlement, in.maturity, settle_to_mat) || settle_to_mat <= 0)
        return false;

    Date prev_coupon = previousCouponDate(in.settlement, in.maturity);
    Date next_coupon = prev_coupon;
    next_coupon.m += 6;
    if (next_coupon.m > 12) { next_coupon.m -= 12; next_coupon.y += 1; }

    long e_days = dayCount(prev_coupon, next_coupon, in.thirty360);
    long a_days = dayCount(prev_coupon, in.settlement, in.thirty360);
    if (e_days <= 0) return false;

    double f = (double)a_days / (double)e_days;          // fraction into period
    double c = in.coupon / 2.0;                           // semiannual coupon $
    double y = in.yield / 200.0;                          // per-period yield

    // Number of remaining coupons N. Walk forward from next_coupon until maturity.
    int N = 0;
    Date d = next_coupon;
    while (true) {
        ++N;
        if (d.y == in.maturity.y && d.m == in.maturity.m && d.d == in.maturity.d) break;
        Date nx = d; nx.m += 6;
        if (nx.m > 12) { nx.m -= 12; nx.y += 1; }
        long check;
        daysActual(nx, in.maturity, check);
        if (check < 0) {
            // Maturity not landing on a coupon — shouldn't happen for whole-year
            // bonds but guard anyway.
            ++N;
            break;
        }
        d = nx;
        if (N > 200) return false;  // sanity
    }

    // Dirty price = Σ c/(1+y)^(k-f) for k=1..N + 100/(1+y)^(N-f)
    double dirty = 0.0;
    for (int k = 1; k <= N; ++k) {
        dirty += c / std::pow(1.0 + y, (double)k - f);
    }
    dirty += 100.0 / std::pow(1.0 + y, (double)N - f);

    double accrued = c * f;
    out.clean   = dirty - accrued;
    out.accrued = accrued;
    return std::isfinite(out.clean);
}

bool bondCoupon(const BondInputs& in, double clean_price, double& cpn_out) {
    BondInputs probe = in;
    double lo = 0.0, hi = 100.0;  // search 0%..100% annual coupon
    auto f = [&](double c) -> double {
        probe.coupon = c;
        BondPrice p;
        if (!bondPrice(probe, p)) return std::nan("");
        return p.clean - clean_price;
    };
    double f_lo = f(lo), f_hi = f(hi);
    if (!std::isfinite(f_lo) || !std::isfinite(f_hi)) return false;
    if (f_lo * f_hi > 0) return false;       // not bracketed in [0, 100]

    for (int k = 0; k < 200; ++k) {
        double mid = 0.5 * (lo + hi);
        double fm = f(mid);
        if (!std::isfinite(fm)) return false;
        if (std::fabs(fm) < 1e-8 || (hi - lo) < 1e-10) { cpn_out = mid; return true; }
        if (fm * f_lo < 0) { hi = mid; f_hi = fm; }
        else               { lo = mid; f_lo = fm; }
    }
    cpn_out = 0.5 * (lo + hi);
    return true;
}

bool bondYield(const BondInputs& in, double clean_price, double& yld_out) {
    BondInputs probe = in;
    double lo = 0.0001, hi = 100.0;
    auto f = [&](double y) -> double {
        probe.yield = y;
        BondPrice p;
        if (!bondPrice(probe, p)) return std::nan("");
        return p.clean - clean_price;
    };
    double f_lo = f(lo), f_hi = f(hi);
    if (!std::isfinite(f_lo) || !std::isfinite(f_hi)) return false;
    if (f_lo * f_hi > 0) return false;

    for (int k = 0; k < 200; ++k) {
        double mid = 0.5 * (lo + hi);
        double fm = f(mid);
        if (!std::isfinite(fm)) return false;
        if (std::fabs(fm) < 1e-8 || (hi - lo) < 1e-10) { yld_out = mid; return true; }
        if (fm * f_lo < 0) { hi = mid; f_hi = fm; }
        else               { lo = mid; f_lo = fm; }
    }
    yld_out = 0.5 * (lo + hi);
    return true;
}

}  // namespace hp12c
