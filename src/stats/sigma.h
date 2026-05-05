#pragma once

#include "../../include/state.h"

namespace hp12c {

// Statistics, HP 12C style. The 12C reuses storage registers R1..R6 for:
//   R1 = n     (count)
//   R2 = Σx
//   R3 = Σx²
//   R4 = Σy
//   R5 = Σy²
//   R6 = Σxy
// We keep that contract so RCL R1..R6 surfaces the running sums verbatim.

void sigmaPlus (State& s, double x, double y);   // Σ+
void sigmaMinus(State& s, double x, double y);   // Σ-

bool meanX(const State& s, double& out);
bool meanY(const State& s, double& out);
bool meanXWeighted(const State& s, double& out); // x weighted by y (12C "x̄w")

bool stdDevX(const State& s, double& out);
bool stdDevY(const State& s, double& out);

// Linear regression y = m*x + b
bool linRegression(const State& s, double& slope, double& intercept,
                   double& r);

// Predict y given x (and r), and predict x given y.
bool predictY(const State& s, double x, double& y_hat, double& r);
bool predictX(const State& s, double y, double& x_hat, double& r);

}  // namespace hp12c
