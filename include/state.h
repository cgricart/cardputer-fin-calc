#pragma once

#include <cstdint>

namespace hp12c {

constexpr uint16_t kStateVersion = 1;
constexpr int kNumStorageRegs = 20;   // R0..R9, R.0..R.9
constexpr int kMaxCashFlows   = 21;   // CF0 + CF1..CF20

// Bit layout of State::flags
constexpr uint8_t kFlagBegin     = 1u << 0;  // 1 = BEG, 0 = END
constexpr uint8_t kFlagCommaRad  = 1u << 1;  // 1 = "1.234,56", 0 = "1,234.56"
constexpr uint8_t kFlagDmy       = 1u << 2;  // 1 = D.MY, 0 = M.DY
constexpr uint8_t kFlagCompound  = 1u << 3;  // C-flag: 1 = compound, 0 = simple in fractional period

struct State {
    uint16_t version;

    // RPN stack + LASTX
    double X, Y, Z, T, lastX;

    // Financial registers
    double n, i, PV, PMT, FV;

    // Storage
    double R[kNumStorageRegs];

    // Cash-flow registers (CF[0] is CF0, NPV initial outlay).
    double   CF[kMaxCashFlows];
    uint16_t Nj[kMaxCashFlows];
    uint8_t  cf_count;       // number of CFj entries, including CF0

    // Modes
    uint8_t fix_digits;      // 0..9
    uint8_t flags;           // see kFlag* above
};

void resetState(State& s);

}  // namespace hp12c
