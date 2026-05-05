// Lightweight smoke test runnable without Unity / PlatformIO.
// Compile from project root:
//   clang++ -std=gnu++17 -Iinclude -Isrc \
//     src/rpn/*.cpp src/fin/*.cpp src/stats/*.cpp src/mem/registers.cpp \
//     src/app/errors.cpp test/native/smoke.cpp -o /tmp/hp12c_smoke
//   /tmp/hp12c_smoke

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "state.h"
#include "fin/amort.h"
#include "fin/bond.h"
#include "fin/cashflow.h"
#include "fin/dates.h"
#include "fin/depr.h"
#include "fin/pct.h"
#include "fin/tvm.h"
#include "mem/registers.h"
#include "rpn/entry.h"
#include "rpn/format.h"
#include "rpn/stack.h"
#include "stats/sigma.h"

using namespace hp12c;

static int g_fails = 0;

#define CHECK_NEAR(label, expected, actual, tol) do { \
    double _e = (expected), _a = (actual);             \
    bool ok = std::fabs(_e - _a) <= (tol);             \
    std::printf("%-40s %s  expected=%.6f got=%.6f\n", \
        label, ok ? "PASS" : "FAIL", _e, _a);          \
    if (!ok) ++g_fails;                                \
} while (0)

#define CHECK_TRUE(label, cond) do {                   \
    bool _c = (cond);                                  \
    std::printf("%-40s %s\n", label, _c ? "PASS" : "FAIL"); \
    if (!_c) ++g_fails;                                \
} while (0)

int main() {
    State s{};
    resetState(s);

    // Stack
    s.X = 1; Stack::enterValue(s, 1);
    s.X = 2; Stack::enterValue(s, 2);
    s.X = 3; Stack::binary(s, [](double y, double x, double& r){ r = y + x; return true; });
    CHECK_NEAR("stack: 1 ENT 2 ENT 3 +", 5, s.X, 1e-9);

    // Entry
    NumberEntry e;
    e.digit('1'); e.digit('2'); e.dot(); e.digit('5');
    CHECK_NEAR("entry: 12.5", 12.5, e.value(), 1e-9);

    // Format
    FormatOpts o; o.fix_digits = 2;
    std::string f = formatNumber(1234.567, o);
    CHECK_TRUE("format thousands", f == "1,234.57");

    // %
    CHECK_NEAR("10% of 200", 20.0, pct(200, 10), 1e-9);

    // TVM examples
    {
        Tvm t{60, 0.06/12, -1000, 0, 0, false};
        double fv;
        bool ok = solveFV(t, fv);
        CHECK_TRUE("solveFV ok", ok);
        CHECK_NEAR("FV 1000@6%/12 5y", 1348.85, fv, 0.01);
    }
    {
        Tvm t{360, 0.09/12, 100000, 0, 0, false};
        double pmt;
        solvePMT(t, pmt);
        CHECK_NEAR("PMT 100k@9%/12 30y", -804.62, pmt, 0.01);
    }
    {
        Tvm t{10, 0.0, -1000, 0, 2000, false};
        double i;
        solveI(t, i);
        CHECK_NEAR("solveI doubles in 10y", 0.0717734625, i, 1e-6);
    }
    {
        Tvm t{0, 0.10, -500, 0, 1000, false};
        double n;
        solveN(t, n);
        CHECK_NEAR("solveN", 7.27, n, 0.01);
    }

    // Amortization — exact-math expectation (the 12C manual uses display-
    // precision rounding which differs by a few dollars; we don't emulate it).
    // Closed form: balance_12 = 100000*(1.0075^12) - PMT*((1.0075^12-1)/0.0075)
    {
        Tvm t{360, 0.09/12, 100000, -804.6226, 0, false};
        AmortResult r = amortize(t, 12);
        // total payment - principal = interest; principal = PV - new_balance
        CHECK_NEAR("amort y1 interest",  8972.27, r.interest,  1.0);
        CHECK_NEAR("amort y1 principal",  683.20, r.principal, 1.0);
        CHECK_NEAR("amort y1 balance",  99316.80, r.new_balance, 1.0);
    }

    // NPV / IRR
    {
        State cs{}; resetState(cs);
        appendCashFlow(cs, -1000, 1);
        appendCashFlow(cs, 200, 1);
        appendCashFlow(cs, 300, 1);
        appendCashFlow(cs, 400, 1);
        appendCashFlow(cs, 500, 1);
        double npv, irr;
        computeNPV(cs, 0.10, npv);
        // Hand-computed: -1000 + 200/1.1 + 300/1.21 + 400/1.331 + 500/1.4641
        CHECK_NEAR("NPV @10%", 71.78, npv, 0.5);
        computeIRR(cs, irr);
        CHECK_NEAR("IRR", 0.1283, irr, 1e-3);
    }

    // Depreciation
    {
        DeprResult r;
        deprSL(10000, 1000, 5, 3, r);
        CHECK_NEAR("SL year 3 depr",      1800.0, r.depr,      1e-6);
        CHECK_NEAR("SL year 3 remaining", 3600.0, r.remaining, 1e-6);
        deprSOYD(10000, 1000, 5, 1, r);
        CHECK_NEAR("SOYD year 1 depr",    3000.0, r.depr,      1e-6);
    }

    // Dates
    {
        Date a{1789,6,17}, b{1989,6,17};
        long d;
        daysActual(a, b, d);
        // 200*365 = 73000 + 48 leap days (1800 and 1900 are non-leap) = 73048.
        std::printf("    (days computed = %ld)\n", d);
        CHECK_TRUE("days 1789-1989 == 73048", d == 73048);

        Date out;
        bool ok = parsePackedDate(packDate({1985,7,4}, false), false, out);
        CHECK_TRUE("date round trip", ok && out.y==1985 && out.m==7 && out.d==4);
    }

    // Stats
    {
        State ss{}; resetState(ss);
        sigmaPlus(ss, 1, 3);
        sigmaPlus(ss, 2, 5);
        sigmaPlus(ss, 3, 7);
        sigmaPlus(ss, 4, 9);
        double m, b, r;
        linRegression(ss, m, b, r);
        CHECK_NEAR("regression slope",     2.0, m, 1e-9);
        CHECK_NEAR("regression intercept", 1.0, b, 1e-9);
        CHECK_NEAR("regression r",         1.0, r, 1e-9);
    }

    // STO arithmetic
    {
        State rs{}; resetState(rs);
        ErrorState err;
        storeRegister(rs, 0, 100, StoOp::Replace, err);
        storeRegister(rs, 0,  50, StoOp::Add,     err);
        storeRegister(rs, 0,   3, StoOp::Mul,     err);
        CHECK_NEAR("STO arithmetic", 450.0, rs.R[0], 1e-9);
    }

    // Bond at par
    {
        BondInputs in;
        in.settlement = {2024,1,1};
        in.maturity   = {2034,1,1};
        in.coupon = 5.0;
        in.yield  = 5.0;
        in.thirty360 = true;
        BondPrice p;
        bool ok = bondPrice(in, p);
        CHECK_TRUE("bond par computes", ok);
        CHECK_NEAR("par bond price", 100.0, p.clean, 1e-3);
    }

    std::printf("\n%s — %d failure(s)\n", g_fails ? "SOME TESTS FAILED" : "ALL PASSED", g_fails);
    return g_fails ? 1 : 0;
}
