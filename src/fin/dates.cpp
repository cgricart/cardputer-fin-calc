#include "dates.h"

#include <cmath>

namespace hp12c {

namespace {

bool isLeap(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

int daysInMonth(int y, int m) {
    static const int dim[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (m < 1 || m > 12) return 0;
    int d = dim[m-1];
    if (m == 2 && isLeap(y)) d = 29;
    return d;
}

// Convert (y,m,d) to a serial day number (proleptic Gregorian, day 0 = 0000-03-01).
// Algorithm from Hatcher / Howard Hinnant — handles wide year ranges.
long toSerial(const Date& dt) {
    int y = dt.y;
    int m = dt.m;
    int d = dt.d;
    if (m <= 2) { y -= 1; m += 12; }
    long era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);            // 0..399
    unsigned doy = (153 * (m - 3) + 2) / 5 + d - 1;      // 0..365
    unsigned doe = yoe * 365 + yoe/4 - yoe/100 + doy;    // 0..146096
    return era * 146097 + (long)doe - 719468;           // days since 1970-01-01
}

Date fromSerial(long days) {
    long z = days + 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;
    int y = (int)(yoe + era * 400);
    unsigned doy = doe - (365*yoe + yoe/4 - yoe/100);
    unsigned mp  = (5*doy + 2) / 153;
    unsigned d   = doy - (153*mp + 2)/5 + 1;
    unsigned m   = mp < 10 ? mp + 3 : mp - 9;
    if (m <= 2) y += 1;
    return {y, (int)m, (int)d};
}

}  // namespace

bool Date::valid() const {
    if (m < 1 || m > 12) return false;
    if (d < 1 || d > daysInMonth(y, m)) return false;
    return true;
}

bool parsePackedDate(double v, bool dmy_mode, Date& out) {
    // Pull integer part as the leading field, fractional part scaled by 1e6
    // for the rest. We use rounding to dodge IEEE float artifacts on .ddyyyy.
    long sign = v < 0 ? -1 : 1;
    double av = std::fabs(v);
    long  ipart = (long)std::floor(av);
    double fpart = av - ipart;
    long  packed = (long)std::llround(fpart * 1000000.0);  // ddyyyy or mmyyyy

    int leading = (int)ipart * (int)sign;
    int dd_or_mm = (int)(packed / 10000);
    int year     = (int)(packed % 10000);

    if (dmy_mode) {
        out.d = leading;
        out.m = dd_or_mm;
        out.y = year;
    } else {
        out.m = leading;
        out.d = dd_or_mm;
        out.y = year;
    }
    return out.valid();
}

double packDate(const Date& d, bool dmy_mode) {
    int leading = dmy_mode ? d.d : d.m;
    int second  = dmy_mode ? d.m : d.d;
    return (double)leading + ((double)second * 10000.0 + (double)d.y) / 1000000.0;
}

int dayOfWeek(const Date& d) {
    long s = toSerial(d);
    // 1970-01-01 was a Thursday (=4 if Sun=0).
    long w = ((s % 7) + 7 + 4) % 7;
    return (int)w;
}

bool daysActual(const Date& a, const Date& b, long& out) {
    if (!a.valid() || !b.valid()) return false;
    out = toSerial(b) - toSerial(a);
    return true;
}

bool days360(const Date& a, const Date& b, long& out) {
    if (!a.valid() || !b.valid()) return false;
    int d1 = a.d, d2 = b.d;
    // HP 12C uses the 30/360 European-ish rule per its manual:
    //   if d1 = 31, set d1 = 30
    //   if d2 = 31 and d1 = 30, set d2 = 30
    if (d1 == 31) d1 = 30;
    if (d2 == 31 && d1 == 30) d2 = 30;
    out = (b.y - a.y) * 360L + (b.m - a.m) * 30L + (d2 - d1);
    return true;
}

Date addDays(const Date& base, long days) {
    return fromSerial(toSerial(base) + days);
}

}  // namespace hp12c
