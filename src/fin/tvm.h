#pragma once

#include "../app/errors.h"

namespace hp12c {

// Time-Value-of-Money model used by the HP 12C:
//
//   0 = PV + (1 + i*S) * PMT * [(1 - (1+i)^-n) / i] + FV * (1+i)^-n
//
//  where i is the periodic rate as a decimal, n is the number of periods,
//  and S = 1 for BEG (annuity due) or 0 for END (ordinary annuity).
//  At i=0 the closed form degenerates to: 0 = PV + n*PMT + FV.
//
// All routines take the four known values and the begin flag, and return
// the missing one in `out`. Callers handle Error::E5 on failure.

struct Tvm {
    double n;
    double i;          // decimal per period (e.g. 0.005 for 0.5%/mo)
    double PV;
    double PMT;
    double FV;
    bool   begin;      // true => BEG, false => END
};

bool solvePV(const Tvm& t, double& out);
bool solveFV(const Tvm& t, double& out);
bool solvePMT(const Tvm& t, double& out);
bool solveN(const Tvm& t, double& out);
bool solveI(const Tvm& t, double& out);

}  // namespace hp12c
