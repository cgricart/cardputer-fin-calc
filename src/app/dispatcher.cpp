#include "dispatcher.h"

#include <cmath>

#include "../fin/amort.h"
#include "../fin/bond.h"
#include "../fin/cashflow.h"
#include "../fin/dates.h"
#include "../fin/depr.h"
#include "../fin/pct.h"
#include "../fin/tvm.h"
#include "../mem/registers.h"
#include "../rpn/stack.h"
#include "../stats/sigma.h"

namespace hp12c {

namespace {

// Wrap a unary operation with stack semantics.
void unary(State& s, ErrorState& err, double (*op)(double), bool* domain_ok) {
    if (domain_ok && !*domain_ok) { err.raise(Error::E0); return; }
    s.lastX = s.X;
    s.X = op(s.X);
    if (!std::isfinite(s.X)) err.raise(Error::E0);
}

void doSqrt(State& s, ErrorState& err) {
    if (s.X < 0) { err.raise(Error::E0); return; }
    s.lastX = s.X; s.X = std::sqrt(s.X);
}
void doInv(State& s, ErrorState& err) {
    if (s.X == 0) { err.raise(Error::E0); return; }
    s.lastX = s.X; s.X = 1.0 / s.X;
}
void doLn(State& s, ErrorState& err) {
    if (s.X <= 0) { err.raise(Error::E0); return; }
    s.lastX = s.X; s.X = std::log(s.X);
}
void doExp(State& s) {
    s.lastX = s.X; s.X = std::exp(s.X);
}
void doIntg(State& s) {
    s.lastX = s.X; s.X = std::floor(s.X);
}
void doFrac(State& s) {
    s.lastX = s.X; s.X = s.X - std::floor(s.X);
}
void doRound(State& s, int digits) {
    s.lastX = s.X;
    double f = std::pow(10.0, digits);
    s.X = std::round(s.X * f) / f;
}

}  // namespace

void Dispatcher::commitEntry() {
    if (!entry_.active()) return;
    double v = entry_.value();
    if (lift_disabled_) {
        s_.X = v;
        lift_disabled_ = false;
    } else {
        Stack::lift(s_);
        s_.X = v;
    }
    entry_.reset();
}

void Dispatcher::liftIfNeeded() {
    if (entry_.active()) commitEntry();
    else if (!lift_disabled_) Stack::lift(s_);
    lift_disabled_ = false;
}

bool Dispatcher::handle(Key key, Shift shift_override) {
    Shift sh = (shift_override != Shift::None) ? shift_override : shift_;
    // Most keys consume the shift; we reset it at the bottom unless we
    // explicitly preserve it (digit keys for STO/RCL register selection).
    bool consume_shift = true;
    bool changed = true;

    // Pending STO/RCL register digit?
    if ((pending_sto_ >= 0 || pending_rcl_ >= 0)
        && key >= Key::D0 && key <= Key::D9) {
        int reg = (int)key - (int)Key::D0;
        // pending_sto_ encodes the StoOp+1 (so 0 reserved for none)
        if (pending_sto_ >= 0) {
            commitEntry();
            StoOp op = (StoOp)pending_sto_;
            storeRegister(s_, reg, s_.X, op, err_);
            pending_sto_ = -1;
        } else {
            double v;
            if (recallRegister(s_, reg, v, err_)) {
                liftIfNeeded();
                s_.X = v;
            }
            pending_rcl_ = -1;
        }
        shift_ = Shift::None;
        return true;
    }

    switch (key) {
        // ── digits & dot ─────────────────────────────────────────
        case Key::D0: case Key::D1: case Key::D2: case Key::D3: case Key::D4:
        case Key::D5: case Key::D6: case Key::D7: case Key::D8: case Key::D9: {
            char c = '0' + ((int)key - (int)Key::D0);
            // FIX with f-shift + digit
            if (sh == Shift::F) {
                s_.fix_digits = (c - '0');
                break;
            }
            if (!entry_.active()) {
                if (!lift_disabled_) Stack::lift(s_);
                lift_disabled_ = false;
            }
            entry_.digit(c);
            break;
        }
        case Key::Dot:
            if (!entry_.active()) {
                if (!lift_disabled_) Stack::lift(s_);
                lift_disabled_ = false;
            }
            entry_.dot();
            break;

        case Key::Chs:
            if (entry_.active()) entry_.chs();
            else { s_.X = -s_.X; }
            break;

        case Key::Eex:
            if (!entry_.active()) {
                if (!lift_disabled_) Stack::lift(s_);
                lift_disabled_ = false;
            }
            entry_.eex();
            break;

        case Key::Backspace:
            if (entry_.active()) entry_.backspace();
            else                 Stack::clearX(s_);
            lift_disabled_ = true;
            break;

        case Key::Enter:
            commitEntry();
            Stack::enterValue(s_, s_.X);  // duplicate X up
            lift_disabled_ = true;
            break;

        // ── arithmetic ───────────────────────────────────────────
        case Key::Plus:
            commitEntry();
            Stack::binary(s_, [](double y, double x, double& r) {
                r = y + x; return std::isfinite(r);
            });
            break;
        case Key::Minus:
            commitEntry();
            Stack::binary(s_, [](double y, double x, double& r) {
                r = y - x; return std::isfinite(r);
            });
            break;
        case Key::Times:
            commitEntry();
            Stack::binary(s_, [](double y, double x, double& r) {
                r = y * x; return std::isfinite(r);
            });
            break;
        case Key::Div:
            commitEntry();
            if (sh == Shift::F) {
                // f ÷ → SL depreciation (cost in PV, salvage in FV, life in n,
                // year in X). Result: depreciation in X, remaining in Y.
                DeprResult d;
                if (deprSL(s_.PV, s_.FV, s_.n, (int)s_.X, d)) {
                    s_.lastX = s_.X;
                    Stack::lift(s_); s_.X = d.depr; s_.Y = d.remaining;
                } else err_.raise(Error::E9);
                break;
            }
            if (s_.X == 0.0) { err_.raise(Error::E0); break; }
            Stack::binary(s_, [](double y, double x, double& r) {
                if (x == 0.0) return false;
                r = y / x; return std::isfinite(r);
            });
            break;

        // ── %, %T, Δ% (and shifts: LN, e^x) ──────────────────────
        case Key::KeyPct:
            commitEntry();
            if (sh == Shift::F)      doLn(s_, err_);
            else if (sh == Shift::G) doExp(s_);
            else { s_.lastX = s_.X; s_.X = pct(s_.Y, s_.X); }
            break;
        case Key::KeyPctT: {
            commitEntry();
            if (sh == Shift::F) {
                // PRICE: settlement Y, maturity X, coupon PMT, yield i, basis flag g
                BondInputs bi;
                if (parsePackedDate(s_.Y, s_.flags & kFlagDmy, bi.settlement)
                 && parsePackedDate(s_.X, s_.flags & kFlagDmy, bi.maturity)) {
                    bi.coupon = s_.PMT;
                    bi.yield  = s_.i;
                    bi.thirty360 = true;
                    BondPrice bp;
                    if (bondPrice(bi, bp)) {
                        s_.lastX = s_.X;
                        s_.X = bp.clean;
                        s_.Y = bp.accrued;
                    } else err_.raise(Error::E9);
                } else err_.raise(Error::E8);
                break;
            }
            double r;
            if (pctTotal(s_.Y, s_.X, r)) { s_.lastX = s_.X; s_.X = r; }
            else err_.raise(Error::E0);
            break;
        }
        case Key::KeyDelta: {
            commitEntry();
            if (sh == Shift::F) {
                // DATE: Y is base packed date, X is days; result in X is new packed date.
                Date base;
                if (parsePackedDate(s_.Y, s_.flags & kFlagDmy, base)) {
                    Date out = addDays(base, (long)s_.X);
                    s_.lastX = s_.X;
                    s_.X = packDate(out, s_.flags & kFlagDmy);
                } else err_.raise(Error::E8);
                break;
            }
            if (sh == Shift::G) {
                // DYS: returns actual in X, 30/360 in Y.
                Date a, b;
                if (parsePackedDate(s_.Y, s_.flags & kFlagDmy, a)
                 && parsePackedDate(s_.X, s_.flags & kFlagDmy, b)) {
                    long actual=0, t360=0;
                    daysActual(a, b, actual);
                    days360(a, b, t360);
                    s_.lastX = s_.X;
                    s_.X = (double)actual;
                    s_.Y = (double)t360;
                } else err_.raise(Error::E8);
                break;
            }
            double r;
            if (pctChange(s_.Y, s_.X, r)) { s_.lastX = s_.X; s_.X = r; }
            else err_.raise(Error::E0);
            break;
        }

        // ── y^x / √x / Nj ────────────────────────────────────────
        case Key::KeyYx:
            commitEntry();
            if (sh == Shift::F) doSqrt(s_, err_);
            else if (sh == Shift::G) {
                // g Nj — set Nj of last CFj from X.
                if (!setLastNj(s_, (uint16_t)s_.X)) err_.raise(Error::E4);
            } else {
                Stack::binary(s_, [](double y, double x, double& r) {
                    if (y < 0 && std::floor(x) != x) return false;
                    r = std::pow(y, x); return std::isfinite(r);
                });
            }
            break;
        case Key::KeyInv:
            commitEntry();
            if (sh == Shift::F) {
                double yhat, r;
                if (predictY(s_, s_.X, yhat, r)) {
                    s_.lastX = s_.X; Stack::lift(s_); s_.X = yhat; s_.Y = r;
                } else err_.raise(Error::E2);
            } else if (sh == Shift::G) {
                double xhat, r;
                if (predictX(s_, s_.X, xhat, r)) {
                    s_.lastX = s_.X; Stack::lift(s_); s_.X = xhat; s_.Y = r;
                } else err_.raise(Error::E2);
            } else doInv(s_, err_);
            break;

        // ── Σ+, Σ-, mean ─────────────────────────────────────────
        case Key::KeySigma:
            commitEntry();
            if (sh == Shift::F) sigmaMinus(s_, s_.X, s_.Y);
            else if (sh == Shift::G) {
                double m;
                if (meanX(s_, m)) { liftIfNeeded(); s_.X = m; }
                else err_.raise(Error::E2);
            } else sigmaPlus(s_, s_.X, s_.Y);
            // After Σ+/Σ−, X register shows new n.
            if (sh != Shift::G) { s_.X = s_.R[1]; }
            break;

        // ── n / AMORT / 12× ──────────────────────────────────────
        case Key::KeyN:
            commitEntry();
            if (sh == Shift::F) {
                Tvm t{ s_.n, s_.i/100.0, s_.PV, s_.PMT, s_.FV,
                       (s_.flags & kFlagBegin) != 0 };
                AmortResult ar = amortize(t, (int)s_.X);
                s_.PV = ar.new_balance;
                s_.lastX = s_.X;
                Stack::lift(s_); s_.X = ar.interest;
                Stack::lift(s_); s_.X = ar.principal;
                // X = principal, Y = interest, balance updated in PV
            } else if (sh == Shift::G) { s_.n = s_.X * 12.0; }
            else { s_.n = s_.X; }
            break;
        case Key::KeyI:
            commitEntry();
            if (sh == Shift::G) { s_.i = s_.X / 12.0; }
            else                { s_.i = s_.X; }
            break;
        case Key::KeyPV: {
            commitEntry();
            if (sh == Shift::F) {
                double npv;
                if (computeNPV(s_, s_.i/100.0, npv)) { liftIfNeeded(); s_.X = npv; }
                else err_.raise(Error::E4);
            } else if (sh == Shift::G) {
                s_.flags &= ~kFlagBegin;   // END
            } else s_.PV = s_.X;
            break;
        }
        case Key::KeyPMT:
            commitEntry();
            if (sh == Shift::F) doRound(s_, s_.fix_digits);
            else if (sh == Shift::G) s_.flags |= kFlagBegin;  // BEG
            else                     s_.PMT = s_.X;
            break;
        case Key::KeyFV: {
            commitEntry();
            if (sh == Shift::F) {
                double irr;
                if (computeIRR(s_, irr)) { liftIfNeeded(); s_.X = irr * 100.0; }
                else err_.raise(Error::E7);
            } else if (sh == Shift::G) {
                if (!appendCashFlow(s_, s_.X, 1)) err_.raise(Error::E4);
            } else s_.FV = s_.X;
            break;
        }

        // ── solve missing TVM variable: dispatched via dedicated keys
        //    is the conventional 12C way (n/i/PV/PMT/FV with no value
        //    typed).  Here we expose a single "solve" — long-press of the
        //    target key; for v1 we keep simple "type-or-solve":
        //    if a number was just entered, store; else solve. Implemented
        //    elsewhere via a long-press handler; left as a TODO hook.

        // ── x⇄y, R↓, LASTx, CLx, CL FIN, CL REG ──────────────────
        case Key::KeySwap:
            commitEntry();
            if (sh == Shift::F) {
                // ŷ already on KeyInv; here we expose: x̂ from regression.
                double xhat, r;
                if (predictX(s_, s_.X, xhat, r)) { s_.lastX = s_.X; s_.X = xhat; }
                else err_.raise(Error::E2);
            } else if (sh == Shift::G) {
                // s : sample std-dev; place sx in X, sy in Y.
                double sx, sy;
                bool okx = stdDevX(s_, sx);
                bool oky = stdDevY(s_, sy);
                if (okx) { liftIfNeeded(); s_.X = sx; if (oky) s_.Y = sy; }
                else err_.raise(Error::E2);
            } else Stack::swapXY(s_);
            break;
        case Key::KeyRoll:
            commitEntry();
            if (sh == Shift::F)      Stack::rollUp(s_);
            else if (sh == Shift::G) {
                double m;
                if (meanX(s_, m)) { liftIfNeeded(); s_.X = m; }
                else err_.raise(Error::E2);
            } else Stack::rollDown(s_);
            break;
        case Key::KeyClear:
            if (sh == Shift::F) {
                // CLx — clear X register, disable lift on next entry.
                Stack::clearX(s_);
                lift_disabled_ = true;
                entry_.reset();
            } else if (sh == Shift::G) {
                clearStatsRegs(s_);
            } else {
                if (entry_.active()) entry_.chs();
                else                 s_.X = -s_.X;
            }
            break;
        case Key::KeyClearFin:
            if (sh == Shift::F)      clearFinancialRegs(s_);
            else if (sh == Shift::G) clearStorageRegs(s_);
            else { /* EEX: handled via Eex key separately on this build */ }
            break;

        // ── STO / RCL ────────────────────────────────────────────
        case Key::Sto:
            commitEntry();
            // Default operation is Replace; arithmetic STO can be added by
            // chord with +/-/*/÷ (left as a v2 enhancement).
            pending_sto_ = (int)StoOp::Replace;
            consume_shift = false;
            break;
        case Key::Rcl:
            commitEntry();
            pending_rcl_ = 1;
            consume_shift = false;
            break;

        case Key::F: armF();  consume_shift = false; break;
        case Key::G: armG();  consume_shift = false; break;

        // ── TVM solvers ─────────────────────────────────────────
        // Each builds a Tvm from the current registers, runs the solver,
        // and writes the result to both the corresponding register and X.
        case Key::SolveN: {
            commitEntry();
            Tvm t{s_.n, s_.i/100.0, s_.PV, s_.PMT, s_.FV,
                  (s_.flags & kFlagBegin) != 0};
            double r;
            if (solveN(t, r)) {
                s_.lastX = s_.X;
                s_.n = r;
                liftIfNeeded();
                s_.X = r;
            } else err_.raise(Error::E5);
            break;
        }
        case Key::SolveI: {
            commitEntry();
            Tvm t{s_.n, s_.i/100.0, s_.PV, s_.PMT, s_.FV,
                  (s_.flags & kFlagBegin) != 0};
            double r;
            if (solveI(t, r)) {
                s_.lastX = s_.X;
                s_.i = r * 100.0;          // store/display as percent
                liftIfNeeded();
                s_.X = r * 100.0;
            } else err_.raise(Error::E5);
            break;
        }
        case Key::SolvePV: {
            commitEntry();
            Tvm t{s_.n, s_.i/100.0, s_.PV, s_.PMT, s_.FV,
                  (s_.flags & kFlagBegin) != 0};
            double r;
            if (solvePV(t, r)) {
                s_.lastX = s_.X;
                s_.PV = r;
                liftIfNeeded();
                s_.X = r;
            } else err_.raise(Error::E5);
            break;
        }
        case Key::SolvePMT: {
            commitEntry();
            Tvm t{s_.n, s_.i/100.0, s_.PV, s_.PMT, s_.FV,
                  (s_.flags & kFlagBegin) != 0};
            double r;
            if (solvePMT(t, r)) {
                s_.lastX = s_.X;
                s_.PMT = r;
                liftIfNeeded();
                s_.X = r;
            } else err_.raise(Error::E5);
            break;
        }
        case Key::SolveFV: {
            commitEntry();
            Tvm t{s_.n, s_.i/100.0, s_.PV, s_.PMT, s_.FV,
                  (s_.flags & kFlagBegin) != 0};
            double r;
            if (solveFV(t, r)) {
                s_.lastX = s_.X;
                s_.FV = r;
                liftIfNeeded();
                s_.X = r;
            } else err_.raise(Error::E5);
            break;
        }

        // ── Bond solvers ─────────────────────────────────────────
        // Convention used by the menu forms:
        //   Y = settlement date, X = maturity date,
        //   PMT = coupon (% annual), I = yield (% annual),
        //   PV = clean price (per 100). 30/360 day count.
        case Key::SolveYTM: {
            commitEntry();
            BondInputs bi;
            if (parsePackedDate(s_.Y, s_.flags & kFlagDmy, bi.settlement)
             && parsePackedDate(s_.X, s_.flags & kFlagDmy, bi.maturity)) {
                bi.coupon    = s_.PMT;
                bi.yield     = 0.0;          // unknown
                bi.thirty360 = true;
                double y;
                if (bondYield(bi, s_.PV, y)) {
                    s_.lastX = s_.X;
                    s_.i = y;                // store as % into i register
                    liftIfNeeded();
                    s_.X = y;
                } else err_.raise(Error::E9);
            } else err_.raise(Error::E8);
            break;
        }
        case Key::SolveCoupon: {
            commitEntry();
            BondInputs bi;
            if (parsePackedDate(s_.Y, s_.flags & kFlagDmy, bi.settlement)
             && parsePackedDate(s_.X, s_.flags & kFlagDmy, bi.maturity)) {
                bi.coupon    = 0.0;          // unknown
                bi.yield     = s_.i;
                bi.thirty360 = true;
                double c;
                if (bondCoupon(bi, s_.PV, c)) {
                    s_.lastX = s_.X;
                    s_.PMT = c;              // store as % into PMT register
                    liftIfNeeded();
                    s_.X = c;
                } else err_.raise(Error::E9);
            } else err_.raise(Error::E8);
            break;
        }

        // ── Margin / markup solvers ─────────────────────────────
        // Forms place inputs as Y = first field, X = second field.
        // Result overwrites X (Y kept as-is).
        case Key::SolveMarkupPct: {                // Y=cost, X=price
            commitEntry();
            if (s_.Y == 0.0) { err_.raise(Error::E0); break; }
            s_.lastX = s_.X;
            s_.X = (s_.X - s_.Y) / s_.Y * 100.0;
            break;
        }
        case Key::SolveMarkupPrice: {              // Y=cost, X=markup%
            commitEntry();
            s_.lastX = s_.X;
            s_.X = s_.Y * (1.0 + s_.X / 100.0);
            break;
        }
        case Key::SolveMarkupCost: {               // Y=price, X=markup%
            commitEntry();
            double denom = 1.0 + s_.X / 100.0;
            if (denom == 0.0) { err_.raise(Error::E0); break; }
            s_.lastX = s_.X;
            s_.X = s_.Y / denom;
            break;
        }
        case Key::SolveMarginPct: {                // Y=cost, X=price
            commitEntry();
            if (s_.X == 0.0) { err_.raise(Error::E0); break; }
            s_.lastX = s_.X;
            s_.X = (s_.X - s_.Y) / s_.X * 100.0;
            break;
        }
        case Key::SolveMarginPrice: {              // Y=cost, X=margin%
            commitEntry();
            double denom = 1.0 - s_.X / 100.0;
            if (denom == 0.0) { err_.raise(Error::E0); break; }
            s_.lastX = s_.X;
            s_.X = s_.Y / denom;
            break;
        }
        case Key::SolveMarginCost: {               // Y=price, X=margin%
            commitEntry();
            s_.lastX = s_.X;
            s_.X = s_.Y * (1.0 - s_.X / 100.0);
            break;
        }
        case Key::SolveDollar: {                   // Y=cost, X=price
            commitEntry();
            s_.lastX = s_.X;
            s_.X = s_.X - s_.Y;
            break;
        }

        // ── CAGR solvers (compound growth) ──────────────────────
        // Convention used by the Growth forms (TVM registers reused):
        //   PV = Prior, FV = Future, N = periods, I = CAGR % per period.
        // All values stay positive; no sign-flip needed.
        case Key::SolveCAGR: {
            commitEntry();
            if (s_.PV == 0.0 || s_.n <= 0.0) { err_.raise(Error::E5); break; }
            double ratio = s_.FV / s_.PV;
            if (ratio <= 0.0) { err_.raise(Error::E5); break; }
            double r = std::pow(ratio, 1.0 / s_.n) - 1.0;
            s_.lastX = s_.X;
            s_.i = r * 100.0;
            liftIfNeeded();
            s_.X = r * 100.0;
            break;
        }
        case Key::SolveCAGRFuture: {
            commitEntry();
            if (s_.n <= 0.0) { err_.raise(Error::E5); break; }
            double r = s_.i / 100.0;
            if (1.0 + r <= 0.0) { err_.raise(Error::E5); break; }
            double fv = s_.PV * std::pow(1.0 + r, s_.n);
            s_.lastX = s_.X;
            s_.FV = fv;
            liftIfNeeded();
            s_.X = fv;
            break;
        }
        case Key::SolveCAGRPrior: {
            commitEntry();
            if (s_.n <= 0.0) { err_.raise(Error::E5); break; }
            double r = s_.i / 100.0;
            if (1.0 + r <= 0.0) { err_.raise(Error::E5); break; }
            double pv = s_.FV / std::pow(1.0 + r, s_.n);
            s_.lastX = s_.X;
            s_.PV = pv;
            liftIfNeeded();
            s_.X = pv;
            break;
        }
        case Key::SolveCAGRPeriods: {
            commitEntry();
            if (s_.PV == 0.0) { err_.raise(Error::E5); break; }
            double ratio = s_.FV / s_.PV;
            double r = s_.i / 100.0;
            if (ratio <= 0.0 || 1.0 + r <= 0.0 || r == 0.0) {
                err_.raise(Error::E5); break;
            }
            double n = std::log(ratio) / std::log(1.0 + r);
            s_.lastX = s_.X;
            s_.n = n;
            liftIfNeeded();
            s_.X = n;
            break;
        }

        case Key::Help:
        case Key::Sleep:
        case Key::None:
            changed = false;
            break;
    }

    if (consume_shift) shift_ = Shift::None;

    // Round-trip safety: snap inf/nan into Error 0.
    if (!std::isfinite(s_.X)) { err_.raise(Error::E0); s_.X = 0.0; }
    return changed;
}

}  // namespace hp12c
