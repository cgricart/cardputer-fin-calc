#include "stack.h"

namespace hp12c {

void Stack::enterValue(State& s, double value) {
    s.T = s.Z;
    s.Z = s.Y;
    s.Y = s.X;
    s.X = value;
}

void Stack::lift(State& s) {
    s.T = s.Z;
    s.Z = s.Y;
    s.Y = s.X;
}

void Stack::drop(State& s) {
    s.X = s.Y;
    s.Y = s.Z;
    s.Z = s.T;
    // T retains its value, matching HP behavior.
}

void Stack::rollDown(State& s) {
    double tmp = s.X;
    s.X = s.Y;
    s.Y = s.Z;
    s.Z = s.T;
    s.T = tmp;
}

void Stack::rollUp(State& s) {
    double tmp = s.T;
    s.T = s.Z;
    s.Z = s.Y;
    s.Y = s.X;
    s.X = tmp;
}

void Stack::swapXY(State& s) {
    double tmp = s.X;
    s.X = s.Y;
    s.Y = tmp;
}

void Stack::clearX(State& s) {
    s.X = 0.0;
}

}  // namespace hp12c
