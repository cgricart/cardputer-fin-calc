#include "display.h"

#include <cstdio>

#include "../app/errors.h"
#include "../rpn/entry.h"
#include "../rpn/format.h"

#ifndef UNIT_TEST
  #include <M5Cardputer.h>
  #define LCD M5Cardputer.Display
#endif

namespace hp12c {

namespace {
#ifndef UNIT_TEST
constexpr int kW = 240;
constexpr int kH = 135;
constexpr int kStatusH = 14;
constexpr int kFooterH = 14;
constexpr int kStackRowH = 14;     // Y/Z/T preview rows
constexpr int kStackRows = 3;      // Y, Z, T

// Off-screen canvas: we draw the whole frame into this sprite, then push
// it to the LCD in one shot. Eliminates the black-flash from fillScreen().
LGFX_Sprite gCanvas(&LCD);
bool gCanvasReady = false;
#endif

FormatOpts fmtFromState(const State& s) {
    FormatOpts o;
    o.fix_digits  = s.fix_digits;
    o.comma_radix = (s.flags & kFlagCommaRad) != 0;
    return o;
}

}  // namespace

void Display::begin() {
#ifndef UNIT_TEST
    LCD.setRotation(1);
    LCD.fillScreen(TFT_BLACK);
    LCD.setTextWrap(false);

    gCanvas.setColorDepth(16);
    gCanvas.setPsram(true);             // 64 KB framebuffer lives in PSRAM
    gCanvasReady = gCanvas.createSprite(kW, kH);
    if (gCanvasReady) gCanvas.setTextWrap(false);
#endif
    dirty_ = true;
}

void Display::render(const State& s,
                     const NumberEntry& entry,
                     const ErrorState& err,
                     bool shift_f,
                     bool shift_g) {
#ifdef UNIT_TEST
    (void)s; (void)entry; (void)err; (void)shift_f; (void)shift_g;
    return;
#else
    if (!gCanvasReady) {
        LCD.fillScreen(TFT_BLACK);
    } else {
        gCanvas.fillSprite(TFT_BLACK);
    }
    auto& g = gCanvasReady ? static_cast<LovyanGFX&>(gCanvas)
                           : static_cast<LovyanGFX&>(LCD);

    // ── header bar (navy band, app name + live status indicators) ─
    g.fillRect(0, 0, kW, kStatusH, TFT_NAVY);
    g.setTextSize(1);
    g.setTextColor(TFT_WHITE, TFT_NAVY);
    g.setCursor(4, 3);
    g.print("Fin Calc - CGR");

    // Right-aligned status badges (only drawn when active).
    int x_right = kW - 4;
    auto badge = [&](const char* txt, uint16_t color) {
        int w = (int)__builtin_strlen(txt) * 6;
        x_right -= w;
        g.setCursor(x_right, 3);
        g.setTextColor(color, TFT_NAVY);
        g.print(txt);
        x_right -= 4;            // gap between badges
    };
    if (err.active()) {
        char buf[8];
        std::snprintf(buf, sizeof(buf), "E%d", (int)err.current());
        badge(buf, TFT_RED);
    }
    if (s.flags & kFlagBegin) badge("BEG", TFT_GREEN);
    if (shift_g)              badge("g",   TFT_CYAN);
    if (shift_f)              badge("f",   TFT_YELLOW);

    // ── stack preview Y/Z/T ───────────────────────────────────────
    FormatOpts opts = fmtFromState(s);
    g.setTextColor(TFT_DARKGREY, TFT_BLACK);
    int y = kStatusH + 2;
    auto row = [&](const char* label, double v) {
        g.setTextColor(TFT_DARKGREY, TFT_BLACK);
        g.setCursor(4, y);
        g.printf("%s ", label);
        std::string txt = formatNumber(v, opts);
        int char_w = 6;
        int x = kW - 4 - (int)txt.size() * char_w;
        g.setCursor(x, y);
        g.print(txt.c_str());
        y += kStackRowH;
    };
    row("T:", s.T);
    row("Z:", s.Z);
    row("Y:", s.Y);

    // ── X register, large ─────────────────────────────────────────
    g.setTextColor(TFT_WHITE, TFT_BLACK);
    g.setTextSize(3);
    std::string xtxt;
    if (entry.active()) xtxt = entry.buffer();
    else                xtxt = formatNumber(s.X, opts);
    int char_w = 18;
    int x_pos  = kW - 8 - (int)xtxt.size() * char_w;
    if (x_pos < 4) x_pos = 4;
    g.setCursor(x_pos, kStatusH + kStackRows * kStackRowH + 6);
    g.print(xtxt.c_str());

    // ── footer: modes (left) + battery (right) ────────────────────
    g.setTextSize(1);
    g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
    g.setCursor(2, kH - kFooterH + 2);
    g.printf("FIX %d   %s   %s",
        s.fix_digits,
        (s.flags & kFlagBegin) ? "BEG" : "END",
        (s.flags & kFlagDmy)   ? "D.MY" : "M.DY");

    int batt = M5Cardputer.Power.getBatteryLevel();
    if (batt >= 0 && batt <= 100) {
        bool charging = M5Cardputer.Power.isCharging();
        char b[12];
        std::snprintf(b, sizeof(b), "%s%d%%", charging ? "+" : "", batt);
        uint16_t color = TFT_GREEN;
        if (batt < 20)      color = TFT_RED;
        else if (batt < 50) color = TFT_YELLOW;
        if (charging)       color = TFT_CYAN;
        int w = (int)__builtin_strlen(b) * 6;
        g.setTextColor(color, TFT_BLACK);
        g.setCursor(kW - 4 - w, kH - kFooterH + 2);
        g.print(b);
    }

    if (gCanvasReady) gCanvas.pushSprite(0, 0);
    dirty_ = false;
#endif
}

void Display::splash() {
#ifdef UNIT_TEST
    return;
#else
    LCD.fillScreen(TFT_BLACK);
    LCD.setTextWrap(false);

    // Decorative top + bottom strips so the splash feels framed.
    LCD.fillRect(0, kH/2 - 30, kW, 2, TFT_NAVY);
    LCD.fillRect(0, kH/2 + 22, kW, 2, TFT_NAVY);

    LCD.setTextSize(2);
    LCD.setTextColor(TFT_WHITE, TFT_BLACK);
    const char* line1 = "Financial Calculator";
    int char_w = 12;
    int w1 = (int)__builtin_strlen(line1) * char_w;
    int x1 = (kW - w1) / 2;
    if (x1 < 0) x1 = 0;
    LCD.setCursor(x1, kH/2 - 22);
    LCD.print(line1);

    LCD.setTextColor(TFT_CYAN, TFT_BLACK);
    const char* line2 = "by CGR";
    int w2 = (int)__builtin_strlen(line2) * char_w;
    int x2 = (kW - w2) / 2;
    LCD.setCursor(x2, kH/2 + 4);
    LCD.print(line2);

    delay(1800);
    dirty_ = true;
#endif
}

void Display::flash(const char* msg, uint32_t ms) {
#ifdef UNIT_TEST
    (void)msg; (void)ms;
#else
    LCD.fillRect(0, kH/2 - 12, kW, 24, TFT_BLUE);
    LCD.setTextColor(TFT_WHITE, TFT_BLUE);
    LCD.setTextSize(2);
    int len = (int)strlen(msg);
    int x = (kW - len * 12) / 2;
    if (x < 4) x = 4;
    LCD.setCursor(x, kH/2 - 8);
    LCD.print(msg);
    delay(ms);
    dirty_ = true;
#endif
}

}  // namespace hp12c
