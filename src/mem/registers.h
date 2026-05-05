#pragma once

#include "state.h"
#include "../app/errors.h"

namespace hp12c {

// Map a user-visible storage register index (0..9 = R0..R9, 10..19 = R.0..R.9)
// to the State::R[] slot. Returns -1 if out of range.
int storageIndex(int user_index);

// STO/RCL helpers. STO arithmetic ops modify R[idx] in place.
enum class StoOp : uint8_t { Replace, Add, Sub, Mul, Div };

bool storeRegister(State& s, int user_index, double value, StoOp op,
                   ErrorState& err);
bool recallRegister(const State& s, int user_index, double& out,
                    ErrorState& err);

void clearFinancialRegs(State& s);
void clearStorageRegs(State& s);
void clearStatsRegs(State& s);     // R1..R6 are reused for sigma stats
void clearCashFlows(State& s);

}  // namespace hp12c
