#pragma once

namespace hp12c {

// Depreciation calculations matching the HP 12C.
//   cost     = original cost (PV)
//   salvage  = salvage value (FV)
//   life     = total life in years (n)
//   year     = depreciation year being computed (1..life)
//
// Each function returns:
//   out.depr      = depreciation for year `year`
//   out.remaining = remaining depreciable value after year `year`
//   (book value = remaining + salvage)

struct DeprResult {
    double depr;
    double remaining;
};

// Straight-line.
bool deprSL(double cost, double salvage, double life, int year, DeprResult& out);

// Sum-of-the-years' digits.
bool deprSOYD(double cost, double salvage, double life, int year, DeprResult& out);

// Declining-balance with factor `db_pct` (e.g. 200 for double-declining).
// Caps depreciation so book value never falls below salvage.
bool deprDB(double cost, double salvage, double life, double db_pct, int year,
            DeprResult& out);

}  // namespace hp12c
