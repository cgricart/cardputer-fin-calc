#include "amort.h"

#include <cmath>

namespace hp12c {

AmortResult amortize(const Tvm& tvm, int periods) {
    AmortResult r{0.0, 0.0, tvm.PV};
    if (periods <= 0) return r;

    double balance = tvm.PV;
    double total_interest = 0.0;
    double total_principal = 0.0;

    for (int k = 0; k < periods; ++k) {
        // BEG mode: first payment is at start of period before interest.
        double interest = (tvm.begin && k == 0) ? 0.0 : balance * tvm.i;
        // Amount of payment that reduces principal (positive when paying down a loan).
        double principal = -tvm.PMT - interest;
        balance -= principal;
        total_interest += interest;
        total_principal += principal;
    }
    r.interest    = total_interest;
    r.principal   = total_principal;
    r.new_balance = balance;
    return r;
}

}  // namespace hp12c
