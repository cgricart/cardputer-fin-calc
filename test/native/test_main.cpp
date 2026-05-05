// Host-native tests for the calculator math modules. Reference values come
// from worked examples in the HP 12C Owner's Handbook (PV/FV problems,
// amortization, NPV/IRR, depreciation, date math).
//
// Build:  pio test -e native

#include <unity.h>
#include <cmath>

#include "rpn/stack.h"
#include "rpn/entry.h"
#include "rpn/format.h"
#include "fin/pct.h"
#include "fin/tvm.h"
#include "fin/amort.h"
#include "fin/cashflow.h"
#include "fin/depr.h"
#include "fin/dates.h"
#include "fin/bond.h"
#include "stats/sigma.h"
#include "mem/registers.h"
#include "state.h"

using namespace hp12c;

static State freshState() {
    State s{};
    resetState(s);
    return s;
}

// ── Stack ────────────────────────────────────────────────────────
void test_stack_enter_lifts() {
    State s = freshState();
    s.X = 1;
    Stack::enterValue(s, 1);
    TEST_ASSERT_EQUAL_DOUBLE(1, s.X);
    TEST_ASSERT_EQUAL_DOUBLE(1, s.Y);
    s.X = 2;
    Stack::enterValue(s, 2);
    TEST_ASSERT_EQUAL_DOUBLE(2, s.Y);
    TEST_ASSERT_EQUAL_DOUBLE(1, s.Z);
}

void test_stack_binary_drops_T_repeats() {
    State s = freshState();
    s.T = 9; s.Z = 4; s.Y = 3; s.X = 2;
    Stack::binary(s, [](double y, double x, double& r) {
        r = y + x; return true;
    });
    TEST_ASSERT_EQUAL_DOUBLE(5, s.X);
    TEST_ASSERT_EQUAL_DOUBLE(4, s.Y);
    TEST_ASSERT_EQUAL_DOUBLE(9, s.Z);
    TEST_ASSERT_EQUAL_DOUBLE(9, s.T);  // T retained
    TEST_ASSERT_EQUAL_DOUBLE(2, s.lastX);
}

// ── Number entry ─────────────────────────────────────────────────
void test_entry_basic() {
    NumberEntry e;
    e.digit('1'); e.digit('2'); e.dot(); e.digit('5');
    TEST_ASSERT_EQUAL_DOUBLE(12.5, e.value());
}

void test_entry_chs_eex() {
    NumberEntry e;
    e.digit('3'); e.chs();
    TEST_ASSERT_EQUAL_DOUBLE(-3, e.value());
    e.eex(); e.digit('4');
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -3e4, e.value());
    e.chs();
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, -3e-4, e.value());
}

// ── Formatter ────────────────────────────────────────────────────
void test_format_thousands() {
    FormatOpts o; o.fix_digits = 2;
    TEST_ASSERT_EQUAL_STRING("1,234.57", formatNumber(1234.567, o).c_str());
    TEST_ASSERT_EQUAL_STRING("-12,345,678.90",
                             formatNumber(-12345678.9, o).c_str());
}

void test_format_comma_radix() {
    FormatOpts o; o.fix_digits = 2; o.comma_radix = true;
    TEST_ASSERT_EQUAL_STRING("1.234,57", formatNumber(1234.567, o).c_str());
}

// ── Percent ──────────────────────────────────────────────────────
void test_pct() {
    TEST_ASSERT_EQUAL_DOUBLE(20.0, pct(200, 10));   // 10% of 200
    double r;
    TEST_ASSERT_TRUE(pctTotal(200, 50, r));
    TEST_ASSERT_EQUAL_DOUBLE(25.0, r);              // 50 is 25% of 200
    TEST_ASSERT_TRUE(pctChange(200, 250, r));
    TEST_ASSERT_EQUAL_DOUBLE(25.0, r);              // 200 → 250 = +25%
}

// ── TVM (HP 12C manual examples) ─────────────────────────────────
// Example: $1,000 at 6% APR compounded monthly for 5 years → FV.
void test_tvm_solveFV() {
    Tvm t{60, 0.06/12, -1000, 0, 0, false};
    double fv;
    TEST_ASSERT_TRUE(solveFV(t, fv));
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 1348.85, fv);
}

// Example: 30-year fixed mortgage, $100,000 PV, 9% APR monthly → PMT.
void test_tvm_solvePMT() {
    Tvm t{360, 0.09/12, 100000, 0, 0, false};
    double pmt;
    TEST_ASSERT_TRUE(solvePMT(t, pmt));
    TEST_ASSERT_DOUBLE_WITHIN(0.01, -804.62, pmt);
}

// Example: solve for i. $1000 → $2000 in 10 years compounded annually.
void test_tvm_solveI() {
    Tvm t{10, 0.0, -1000, 0, 2000, false};
    double i;
    TEST_ASSERT_TRUE(solveI(t, i));
    // (1+i)^10 = 2 → i = 2^0.1 - 1 ≈ 0.0717734625
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0717734625, i);
}

// Example: solve for n. PV=−500, PMT=0, FV=1000, i=10%/yr → n ≈ 7.27.
void test_tvm_solveN() {
    Tvm t{0, 0.10, -500, 0, 1000, false};
    double n;
    TEST_ASSERT_TRUE(solveN(t, n));
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 7.2725, n);
}

// ── Amortization ─────────────────────────────────────────────────
// 30-yr $100,000 mortgage at 9%: first 12 payments. We compare against
// exact-math expected values (the HP 12C manual quotes display-rounded
// numbers a few dollars off — we deliberately don't emulate that).
void test_amort_first_year() {
    Tvm t{360, 0.09/12, 100000, -804.6226, 0, false};
    AmortResult r = amortize(t, 12);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 8972.27, r.interest);
    TEST_ASSERT_DOUBLE_WITHIN(1.0,  683.20, r.principal);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 99316.80, r.new_balance);
}

// ── NPV / IRR ────────────────────────────────────────────────────
// Initial outlay -1000, then 200, 300, 400, 500 at i=10% → NPV ≈ 71.78
void test_npv_simple() {
    State s = freshState();
    appendCashFlow(s, -1000, 1);
    appendCashFlow(s, 200, 1);
    appendCashFlow(s, 300, 1);
    appendCashFlow(s, 400, 1);
    appendCashFlow(s, 500, 1);
    double npv;
    TEST_ASSERT_TRUE(computeNPV(s, 0.10, npv));
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 71.78, npv);
}

// IRR for {-1000, 200, 300, 400, 500} ≈ 12.83%
void test_irr_simple() {
    State s = freshState();
    appendCashFlow(s, -1000, 1);
    appendCashFlow(s, 200, 1);
    appendCashFlow(s, 300, 1);
    appendCashFlow(s, 400, 1);
    appendCashFlow(s, 500, 1);
    double irr;
    TEST_ASSERT_TRUE(computeIRR(s, irr));
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 0.1283, irr);
}

void test_irr_no_sign_change() {
    State s = freshState();
    appendCashFlow(s, 100, 1);
    appendCashFlow(s, 200, 1);
    double irr;
    TEST_ASSERT_FALSE(computeIRR(s, irr));
}

// ── Depreciation ─────────────────────────────────────────────────
// Cost 10000, salvage 1000, life 5 years.
// SL year 3: 1800/yr; remaining after year 3 = 9000 - 5400 = 3600.
void test_sl() {
    DeprResult r;
    TEST_ASSERT_TRUE(deprSL(10000, 1000, 5, 3, r));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1800.0, r.depr);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 3600.0, r.remaining);
}

// SOYD with same params, year 1: depreciable 9000, frac 5/15 → 3000
void test_soyd() {
    DeprResult r;
    TEST_ASSERT_TRUE(deprSOYD(10000, 1000, 5, 1, r));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 3000.0, r.depr);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 6000.0, r.remaining);
}

// ── Dates ────────────────────────────────────────────────────────
void test_days_actual() {
    Date a{1789, 6, 17};
    Date b{1989, 6, 17};
    long d;
    TEST_ASSERT_TRUE(daysActual(a, b, d));
    // 200 * 365 + 48 leap days (1800, 1900 not leap) = 73048
    TEST_ASSERT_EQUAL_INT64(73048, d);
}
void test_days_360() {
    Date a{2024, 1, 1};
    Date b{2024, 12, 31};
    long d;
    TEST_ASSERT_TRUE(days360(a, b, d));
    // 11 months * 30 = 330, plus 30-1 = 29 → 359
    TEST_ASSERT_EQUAL_INT64(359, d);
}
void test_pack_parse_round_trip() {
    Date d{1985, 7, 4};
    double packed = packDate(d, false);
    Date out;
    TEST_ASSERT_TRUE(parsePackedDate(packed, false, out));
    TEST_ASSERT_EQUAL_INT(1985, out.y);
    TEST_ASSERT_EQUAL_INT(7,    out.m);
    TEST_ASSERT_EQUAL_INT(4,    out.d);
}

// ── Statistics ───────────────────────────────────────────────────
void test_lin_regression() {
    State s = freshState();
    // y = 2x + 1 perfect fit
    sigmaPlus(s, 1, 3);
    sigmaPlus(s, 2, 5);
    sigmaPlus(s, 3, 7);
    sigmaPlus(s, 4, 9);
    double m, b, r;
    TEST_ASSERT_TRUE(linRegression(s, m, b, r));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 2.0, m);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, b);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, r);
}

// ── Storage register arithmetic ──────────────────────────────────
void test_sto_arith() {
    State s = freshState();
    ErrorState err;
    storeRegister(s, 0, 100, StoOp::Replace, err);
    storeRegister(s, 0, 50, StoOp::Add, err);
    storeRegister(s, 0, 3,  StoOp::Mul, err);
    TEST_ASSERT_EQUAL_DOUBLE(450.0, s.R[0]);
    storeRegister(s, 0, 0, StoOp::Div, err);
    TEST_ASSERT_TRUE(err.active());
    TEST_ASSERT_EQUAL_INT(0, (int)err.current());  // Error 0
}

// ── Bond: par bond at coupon=yield prices to 100 ──────────────────
void test_bond_par() {
    BondInputs in;
    in.settlement = {2024, 1, 1};
    in.maturity   = {2034, 1, 1};
    in.coupon     = 5.0;
    in.yield      = 5.0;
    in.thirty360  = true;
    BondPrice p;
    TEST_ASSERT_TRUE(bondPrice(in, p));
    TEST_ASSERT_DOUBLE_WITHIN(1e-4, 100.0, p.clean);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_stack_enter_lifts);
    RUN_TEST(test_stack_binary_drops_T_repeats);
    RUN_TEST(test_entry_basic);
    RUN_TEST(test_entry_chs_eex);
    RUN_TEST(test_format_thousands);
    RUN_TEST(test_format_comma_radix);
    RUN_TEST(test_pct);
    RUN_TEST(test_tvm_solveFV);
    RUN_TEST(test_tvm_solvePMT);
    RUN_TEST(test_tvm_solveI);
    RUN_TEST(test_tvm_solveN);
    RUN_TEST(test_amort_first_year);
    RUN_TEST(test_npv_simple);
    RUN_TEST(test_irr_simple);
    RUN_TEST(test_irr_no_sign_change);
    RUN_TEST(test_sl);
    RUN_TEST(test_soyd);
    RUN_TEST(test_days_actual);
    RUN_TEST(test_days_360);
    RUN_TEST(test_pack_parse_round_trip);
    RUN_TEST(test_lin_regression);
    RUN_TEST(test_sto_arith);
    RUN_TEST(test_bond_par);
    return UNITY_END();
}
