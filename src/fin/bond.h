#pragma once

#include "dates.h"

namespace hp12c {

// Bond pricing per HP 12C: semiannual coupons, par = 100.
//   settlement: trade settlement date
//   maturity:   maturity / redemption date
//   coupon:     annual coupon rate as a percent (e.g. 5.5 for 5.5%)
//   yld:        annual yield to maturity as a percent
//   thirty360:  true => 30/360 day count, false => actual/actual

struct BondInputs {
    Date settlement;
    Date maturity;
    double coupon;
    double yield;
    bool thirty360;
};

// Returns price (per 100 par) plus accrued interest separately.
struct BondPrice {
    double clean;
    double accrued;
};

bool bondPrice(const BondInputs& in, BondPrice& out);

// Solve for yield given clean price (Newton/bisection on bondPrice).
bool bondYield(const BondInputs& in, double clean_price, double& yld_out);

// Solve for annual coupon (%) given clean price, dates, and yield. Bond
// price is monotonic in coupon, so a simple bisection lands quickly.
bool bondCoupon(const BondInputs& in, double clean_price, double& cpn_out);

}  // namespace hp12c
