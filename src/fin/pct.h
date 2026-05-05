#pragma once

namespace hp12c {

// HP 12C percent operations. All operate on (Y, X) and return the result
// to be placed in X; Y is preserved (one of the things that makes the 12C
// pleasant for chained markups).

// %  : returns Y * X / 100
double pct(double y, double x);

// %T : returns 100 * X / Y
bool pctTotal(double y, double x, double& out);

// Δ% : returns 100 * (X - Y) / Y
bool pctChange(double y, double x, double& out);

}  // namespace hp12c
