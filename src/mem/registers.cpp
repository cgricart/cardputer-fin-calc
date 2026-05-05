#include "registers.h"

namespace hp12c {

int storageIndex(int user_index) {
    if (user_index < 0 || user_index >= kNumStorageRegs) return -1;
    return user_index;
}

bool storeRegister(State& s, int user_index, double value, StoOp op,
                   ErrorState& err) {
    int idx = storageIndex(user_index);
    if (idx < 0) { err.raise(Error::E6); return false; }
    double& r = s.R[idx];
    switch (op) {
        case StoOp::Replace: r  = value; break;
        case StoOp::Add:     r += value; break;
        case StoOp::Sub:     r -= value; break;
        case StoOp::Mul:     r *= value; break;
        case StoOp::Div:
            if (value == 0.0) { err.raise(Error::E0); return false; }
            r /= value;
            break;
    }
    return true;
}

bool recallRegister(const State& s, int user_index, double& out,
                    ErrorState& err) {
    int idx = storageIndex(user_index);
    if (idx < 0) { err.raise(Error::E6); return false; }
    out = s.R[idx];
    return true;
}

void clearFinancialRegs(State& s) {
    s.n = s.i = s.PV = s.PMT = s.FV = 0.0;
}

void clearStorageRegs(State& s) {
    for (int k = 0; k < kNumStorageRegs; ++k) s.R[k] = 0.0;
}

void clearStatsRegs(State& s) {
    // The 12C stores n, Σx, Σx², Σy, Σy², Σxy in R1..R6.
    for (int k = 1; k <= 6; ++k) s.R[k] = 0.0;
}

void clearCashFlows(State& s) {
    for (int k = 0; k < kMaxCashFlows; ++k) {
        s.CF[k] = 0.0;
        s.Nj[k] = 0;
    }
    s.cf_count = 0;
}

void resetState(State& s) {
    s.version = kStateVersion;
    s.X = s.Y = s.Z = s.T = s.lastX = 0.0;
    clearFinancialRegs(s);
    clearStorageRegs(s);
    clearCashFlows(s);
    s.fix_digits = 2;
    s.flags = 0;  // END mode, dot-radix, M.DY, simple-period
}

}  // namespace hp12c
