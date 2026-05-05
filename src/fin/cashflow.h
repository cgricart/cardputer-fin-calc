#pragma once

#include "../../include/state.h"

namespace hp12c {

// Cash-flow analysis matching the HP 12C:
//   CF0 (initial outlay) is stored in CF[0] with Nj[0] = 1.
//   CF1..CFn are stored in CF[1..n], each with a repetition count Nj[j].
//
// `g CFj` (in the calculator's UI) appends the next CF; `g Nj` sets Nj of
// the most-recent CF. `f NPV` computes net present value at rate i; `f IRR`
// solves NPV(i) = 0.

bool computeNPV(const State& s, double i_per_period, double& out);
bool computeNFV(const State& s, double i_per_period, double& out);

// IRR: scan for a sign change, then Newton-Raphson. Returns false if no
// real root in (-0.99, 10.0); caller raises Error::E7.
bool computeIRR(const State& s, double& out);

// Append a CFj to the cash-flow list. Returns false if list is full.
bool appendCashFlow(State& s, double cf, uint16_t nj);

// Set the Nj of the most-recently-appended CF (CF0 is fixed at 1).
bool setLastNj(State& s, uint16_t nj);

}  // namespace hp12c
