#pragma once

#include "../../include/state.h"

namespace hp12c {

// HP 12C operates on a 4-level stack X, Y, Z, T plus a LASTX register.
// All routines here operate directly on the State struct so persistence
// captures the whole machine.

class Stack {
public:
    static double  x(const State& s) { return s.X; }
    static double  y(const State& s) { return s.Y; }
    static double  z(const State& s) { return s.Z; }
    static double  t(const State& s) { return s.T; }
    static double  lastX(const State& s) { return s.lastX; }

    // Push X up; T is lost; X is replaced with `value`.
    // Used for ENTER and for placing computed values without lifting.
    static void enterValue(State& s, double value);

    // Lift the stack (Y<-X, Z<-Y, T<-Z) but keep X. Called before typing
    // a new number after most operations.
    static void lift(State& s);

    // Pop X off the stack: X<-Y, Y<-Z, Z<-T (T stays).
    static void drop(State& s);

    // Replace X (no lift). Used by unary results.
    static void setX(State& s, double v) { s.X = v; }

    // Save current X into LASTX, then run a unary op that writes a new X.
    // Returns true on success.
    template <typename UnaryOp>
    static bool unary(State& s, UnaryOp op) {
        double r;
        if (!op(s.X, r)) return false;
        s.lastX = s.X;
        s.X = r;
        return true;
    }

    // Save current X into LASTX, run binary op on (Y,X), drop the stack so
    // the result lands in X.
    template <typename BinaryOp>
    static bool binary(State& s, BinaryOp op) {
        double r;
        if (!op(s.Y, s.X, r)) return false;
        s.lastX = s.X;
        s.X = r;
        s.Y = s.Z;
        s.Z = s.T;
        // T stays per HP convention.
        return true;
    }

    // Roll down: X<-Y, Y<-Z, Z<-T, T<-original X.
    static void rollDown(State& s);
    // Roll up:   T<-Z, Z<-Y, Y<-X, X<-original T.
    static void rollUp(State& s);
    // Swap X and Y.
    static void swapXY(State& s);

    // CLx: X<-0; does NOT lift on next entry (entry mode resets).
    static void clearX(State& s);
};

}  // namespace hp12c
