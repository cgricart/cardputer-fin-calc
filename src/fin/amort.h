#pragma once

#include "tvm.h"

namespace hp12c {

// Result of `amortize`: principal, interest, and the new balance after
// running `periods` payments starting from the current TVM state.
struct AmortResult {
    double interest;
    double principal;
    double new_balance;   // signed PV after the periods (negative for loan)
};

// HP 12C `f AMORT` semantics: given the current TVM (n, i, PV, PMT, BEG/END),
// compute the interest and principal portions for the next `periods`
// payments. PV is updated to the post-amort balance.
//
// Sign convention follows the calculator: positive PV is money received,
// PMT is signed accordingly (typically negative for a loan payment).
AmortResult amortize(const Tvm& tvm, int periods);

}  // namespace hp12c
