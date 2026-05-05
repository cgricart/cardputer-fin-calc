#include "depr.h"

namespace hp12c {

bool deprSL(double cost, double salvage, double life, int year, DeprResult& out) {
    if (life <= 0 || year < 1 || (double)year > life) return false;
    double depreciable = cost - salvage;
    double per_year    = depreciable / life;
    out.depr      = per_year;
    out.remaining = depreciable - per_year * year;
    if (out.remaining < 0) out.remaining = 0;
    return true;
}

bool deprSOYD(double cost, double salvage, double life, int year, DeprResult& out) {
    if (life <= 0 || year < 1 || (double)year > life) return false;
    double depreciable = cost - salvage;
    double soyd = life * (life + 1.0) / 2.0;
    double frac = (life - year + 1.0) / soyd;
    out.depr = depreciable * frac;
    // remaining = depreciable * (sum_{k=year+1..life} (life-k+1) / soyd)
    double consumed_frac = 0.0;
    for (int k = 1; k <= year; ++k)
        consumed_frac += (life - k + 1.0) / soyd;
    out.remaining = depreciable * (1.0 - consumed_frac);
    if (out.remaining < 0) out.remaining = 0;
    return true;
}

bool deprDB(double cost, double salvage, double life, double db_pct, int year,
            DeprResult& out) {
    if (life <= 0 || year < 1 || (double)year > life || db_pct <= 0) return false;
    double rate = (db_pct / 100.0) / life;
    double book = cost;
    double depr_for_year = 0.0;
    for (int k = 1; k <= year; ++k) {
        double d = book * rate;
        // Cap so book never drops below salvage.
        if (book - d < salvage) d = book - salvage;
        if (d < 0) d = 0;
        if (k == year) depr_for_year = d;
        book -= d;
    }
    out.depr      = depr_for_year;
    out.remaining = book - salvage;
    if (out.remaining < 0) out.remaining = 0;
    return true;
}

}  // namespace hp12c
