#pragma once

#include <cstdint>

namespace hp12c {

// Date math matching the HP 12C, where dates are entered as either
//   M.DDYYYY (e.g. 6.171789 = June 17, 1789)   when D.MY flag is OFF
//   D.MMYYYY (e.g. 17.061789 = 17 June 1789)   when D.MY flag is ON
//
// The internal representation here is plain (y, m, d).

struct Date {
    int y, m, d;
    bool valid() const;
};

// Parse the user-entered packed double into a Date according to D.MY flag.
bool parsePackedDate(double v, bool dmy_mode, Date& out);

// Pack back into M.DDYYYY or D.MMYYYY for display.
double packDate(const Date& d, bool dmy_mode);

// Day of week (0=Sunday..6=Saturday).
int dayOfWeek(const Date& d);

// Days between two dates by actual count and by 30/360.
// HP 12C `g DYS` returns: X = actual days, Y = 30/360 days.
bool daysActual(const Date& a, const Date& b, long& out);
bool days360   (const Date& a, const Date& b, long& out);

// Add a number of days to a date. Negative `days` goes backward.
Date addDays(const Date& base, long days);

}  // namespace hp12c
