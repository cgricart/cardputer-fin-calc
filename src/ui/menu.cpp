#include "menu.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "../app/dispatcher.h"
#include "../app/errors.h"
#include "../fin/cashflow.h"
#include "../mem/registers.h"
#include "../rpn/format.h"

#ifndef UNIT_TEST
  #include <M5Cardputer.h>
  #define LCD M5Cardputer.Display
#endif

namespace hp12c {

// ── Form types (in `hp12c` ns so menu.h's forward decl matches) ──────
enum class TargetKind : uint8_t {
    None,
    StackX, StackY,
    RegN, RegI, RegPV, RegPMT, RegFV,
    FixDigits,
    AppendCF,        // each Enter calls appendCashFlow(state, value, 1)
    FlagBegin,       // 1 = BEG, 0 = END
};

struct FormField {
    const char* label;
    TargetKind  target;
    bool        loop          = false;  // stay on this field after Enter
    bool        is_loop_count = false;  // value caps the upcoming loop's iters
};

// A Form may end in a single looping field. Convention: only the LAST
// field may have loop=true. Loop iterations apply per-Enter; Esc on a
// loop field ends the loop and dispatches run_key/run_shift.
struct Form {
    const char* title;
    FormField   fields[5];
    uint8_t     field_count;
    Key         run_key;     // Key::None means apply fields only, no dispatch
    Shift       run_shift;
    bool        clear_cfs_on_open = false;
};

namespace {

// ── Form catalog (file-local) ────────────────────────────────────────
// ── TVM solver forms ─────────────────────────────────────────────────
// Each form gathers the four KNOWN variables (plus BEG/END), pre-filling
// from current registers so the user can edit only what changed. On Run
// the dispatcher solves for the missing variable and shows it in X.

const Form kFormSolveN = {"Solve n",
    {{"i / period (%)", TargetKind::RegI},
     {"PV",             TargetKind::RegPV},
     {"PMT",            TargetKind::RegPMT},
     {"FV",             TargetKind::RegFV},
     {"BEG=1 END=0",    TargetKind::FlagBegin}}, 5,
    Key::SolveN, Shift::None, /*clear_cfs=*/false};

const Form kFormSolveI = {"Solve i",
    {{"n (periods)",  TargetKind::RegN},
     {"PV",           TargetKind::RegPV},
     {"PMT",          TargetKind::RegPMT},
     {"FV",           TargetKind::RegFV},
     {"BEG=1 END=0",  TargetKind::FlagBegin}}, 5,
    Key::SolveI, Shift::None, false};

const Form kFormSolvePV = {"Solve PV",
    {{"n (periods)",      TargetKind::RegN},
     {"i / period (%)",   TargetKind::RegI},
     {"PMT",              TargetKind::RegPMT},
     {"FV",               TargetKind::RegFV},
     {"BEG=1 END=0",      TargetKind::FlagBegin}}, 5,
    Key::SolvePV, Shift::None, false};

const Form kFormSolvePMT = {"Solve PMT",
    {{"n (periods)",    TargetKind::RegN},
     {"i / period (%)", TargetKind::RegI},
     {"PV",             TargetKind::RegPV},
     {"FV",             TargetKind::RegFV},
     {"BEG=1 END=0",    TargetKind::FlagBegin}}, 5,
    Key::SolvePMT, Shift::None, false};

const Form kFormSolveFV = {"Solve FV",
    {{"n (periods)",    TargetKind::RegN},
     {"i / period (%)", TargetKind::RegI},
     {"PV",             TargetKind::RegPV},
     {"PMT",            TargetKind::RegPMT},
     {"BEG=1 END=0",    TargetKind::FlagBegin}}, 5,
    Key::SolveFV, Shift::None, false};

// NPV: clear cash flows, ask for rate + count up front, then loop exactly
// `count` cash flows. After the last CF the dispatcher runs NPV
// automatically. Esc on any field cancels (no run).
const Form kFormNPV = {"NPV",
    {{"rate i (%)",      TargetKind::RegI},
     {"# of cash flows", TargetKind::None, /*loop=*/false, /*is_loop_count=*/true},
     {"cash flow",       TargetKind::AppendCF, /*loop=*/true}}, 3,
    Key::KeyPV, Shift::F, /*clear_cfs_on_open=*/true};

// IRR: same idea but no rate.
const Form kFormIRR = {"IRR",
    {{"# of cash flows", TargetKind::None, false, true},
     {"cash flow",       TargetKind::AppendCF, true}}, 2,
    Key::KeyFV, Shift::F, /*clear_cfs_on_open=*/true};

// Append CFj: open-ended loop with no count cap, no dispatch. Esc to stop.
// (Useful when you want to add more cash flows to an existing list.)
const Form kFormCFj = {"Append CFj",
    {{"cash flow", TargetKind::AppendCF, true}}, 1,
    Key::None, Shift::None, /*clear_cfs_on_open=*/false};

const Form kFormNj = {"Set Nj",
    {{"repeats", TargetKind::StackX}}, 1,
    Key::KeyYx, Shift::G};

const Form kFormAmort = {"AMORT",
    {{"periods", TargetKind::StackX}}, 1,
    Key::KeyN, Shift::F};

const Form kFormSigma = {"Add point",
    {{"x value", TargetKind::StackX}, {"y value", TargetKind::StackY}}, 2,
    Key::KeySigma, Shift::None};

const Form kFormSigmaR = {"Remove pt",
    {{"x value", TargetKind::StackX}, {"y value", TargetKind::StackY}}, 2,
    Key::KeySigma, Shift::F};

const Form kFormPredY = {"Predict y",
    {{"x value", TargetKind::StackX}}, 1,
    Key::KeyInv, Shift::F};

const Form kFormPredX = {"Predict x",
    {{"y value", TargetKind::StackX}}, 1,
    Key::KeyInv, Shift::G};

const Form kFormDate = {"Date+days",
    {{"base date M.DDYYYY", TargetKind::StackY},
     {"days",               TargetKind::StackX}}, 2,
    Key::KeyDelta, Shift::F};

const Form kFormDys = {"Days btw",
    {{"date1 M.DDYYYY", TargetKind::StackY},
     {"date2 M.DDYYYY", TargetKind::StackX}}, 2,
    Key::KeyDelta, Shift::G};

const Form kFormPrice = {"PRICE",
    {{"settle M.DDYYYY",   TargetKind::StackY},
     {"maturity M.DDYYYY", TargetKind::StackX},
     {"coupon %",          TargetKind::RegPMT},
     {"yield %",           TargetKind::RegI}}, 4,
    Key::KeyPctT, Shift::F};

const Form kFormSolveYTM = {"Solve YTM",
    {{"settle M.DDYYYY",   TargetKind::StackY},
     {"maturity M.DDYYYY", TargetKind::StackX},
     {"coupon %",          TargetKind::RegPMT},
     {"clean price (/100)",TargetKind::RegPV}}, 4,
    Key::SolveYTM, Shift::None, false};

const Form kFormSolveCoupon = {"Solve coupon",
    {{"settle M.DDYYYY",   TargetKind::StackY},
     {"maturity M.DDYYYY", TargetKind::StackX},
     {"yield %",           TargetKind::RegI},
     {"clean price (/100)",TargetKind::RegPV}}, 4,
    Key::SolveCoupon, Shift::None, false};

// ── Margin / markup forms ────────────────────────────────────────────
// Each form has 2 fields (Y first, X second) so the dispatcher can read
// both directly from the stack. The convention used:
//   Markup: pct = (price - cost) / cost      (cost is the base)
//   Margin: pct = (price - cost) / price     (price is the base; markdown)

const Form kFormMarkupPct = {"Markup %",
    {{"cost",  TargetKind::StackY},
     {"price", TargetKind::StackX}}, 2,
    Key::SolveMarkupPct, Shift::None, false};

const Form kFormMarkupPrice = {"Markup price",
    {{"cost",     TargetKind::StackY},
     {"markup %", TargetKind::StackX}}, 2,
    Key::SolveMarkupPrice, Shift::None, false};

const Form kFormMarkupCost = {"Markup cost",
    {{"price",    TargetKind::StackY},
     {"markup %", TargetKind::StackX}}, 2,
    Key::SolveMarkupCost, Shift::None, false};

const Form kFormMarkupDollar = {"Markup $",
    {{"cost",  TargetKind::StackY},
     {"price", TargetKind::StackX}}, 2,
    Key::SolveDollar, Shift::None, false};

const Form kFormMarginPct = {"Margin %",
    {{"cost",  TargetKind::StackY},
     {"price", TargetKind::StackX}}, 2,
    Key::SolveMarginPct, Shift::None, false};

const Form kFormMarginPrice = {"Margin price",
    {{"cost",     TargetKind::StackY},
     {"margin %", TargetKind::StackX}}, 2,
    Key::SolveMarginPrice, Shift::None, false};

const Form kFormMarginCost = {"Margin cost",
    {{"price",    TargetKind::StackY},
     {"margin %", TargetKind::StackX}}, 2,
    Key::SolveMarginCost, Shift::None, false};

const Form kFormMarginDollar = {"Margin $",
    {{"cost",  TargetKind::StackY},
     {"price", TargetKind::StackX}}, 2,
    Key::SolveDollar, Shift::None, false};

// ── Growth forms ─────────────────────────────────────────────────────
// "Change %" uses the Markup math (single-period growth). CAGR is
// compound and uses dedicated dispatcher cases that route through the
// TVM registers (PV=Prior, FV=Future, N=periods, I=CAGR %).

const Form kFormChangePct = {"Change %",
    {{"prior",  TargetKind::StackY},
     {"future", TargetKind::StackX}}, 2,
    Key::SolveMarkupPct, Shift::None, false};

const Form kFormChangeFuture = {"Solve future",
    {{"prior",    TargetKind::StackY},
     {"change %", TargetKind::StackX}}, 2,
    Key::SolveMarkupPrice, Shift::None, false};

const Form kFormChangePrior = {"Solve prior",
    {{"future",   TargetKind::StackY},
     {"change %", TargetKind::StackX}}, 2,
    Key::SolveMarkupCost, Shift::None, false};

const Form kFormCAGR = {"Solve CAGR",
    {{"prior",   TargetKind::RegPV},
     {"future",  TargetKind::RegFV},
     {"periods", TargetKind::RegN}}, 3,
    Key::SolveCAGR, Shift::None, false};

const Form kFormCAGRFuture = {"Solve future",
    {{"prior",   TargetKind::RegPV},
     {"CAGR %",  TargetKind::RegI},
     {"periods", TargetKind::RegN}}, 3,
    Key::SolveCAGRFuture, Shift::None, false};

const Form kFormCAGRPrior = {"Solve prior",
    {{"future",  TargetKind::RegFV},
     {"CAGR %",  TargetKind::RegI},
     {"periods", TargetKind::RegN}}, 3,
    Key::SolveCAGRPrior, Shift::None, false};

const Form kFormCAGRPeriods = {"Solve n",
    {{"prior",   TargetKind::RegPV},
     {"future",  TargetKind::RegFV},
     {"CAGR %",  TargetKind::RegI}}, 3,
    Key::SolveCAGRPeriods, Shift::None, false};

const Form kFormSL = {"SL depr",
    {{"cost",     TargetKind::RegPV},
     {"salvage",  TargetKind::RegFV},
     {"life",     TargetKind::RegN},
     {"year",     TargetKind::StackX}}, 4,
    Key::Div, Shift::F};

const Form kFormFix = {"FIX",
    {{"digits 0-9", TargetKind::FixDigits}}, 1,
    Key::None, Shift::None};

// ── Pages ────────────────────────────────────────────────────────────
enum PageId : uint8_t {
    P_Root = 0,
    P_Financial,
    P_Solve,
    P_Margin,
    P_MarkUp,
    P_MarkDown,
    P_Growth,
    P_GrowthChange,
    P_GrowthCagr,
    P_Stats,
    P_Dates,
    P_Bonds,
    P_Depr,
    P_Modes,
    P_Memory,
    P_Help,
    kPageCount,
};

struct Item {
    const char* label;
    const char* shortcut;
    int8_t      nav;            // PageId or -1
    Key         key;
    Shift       shift;
    const Form* form = nullptr; // optional form
};

constexpr int kMaxItems = 12;

struct Page {
    const char* title;
    Item        items[kMaxItems];
    uint8_t     count;
    bool        is_help;
};

const Page kPages[kPageCount] = {
    // P_Root — pure navigation.
    {"Fin Calc - CGR", {
        {"Financial",     nullptr, P_Financial, Key::None, Shift::None},
        {"Margin",        nullptr, P_Margin,    Key::None, Shift::None},
        {"Growth",        nullptr, P_Growth,    Key::None, Shift::None},
        {"Statistics",    nullptr, P_Stats,     Key::None, Shift::None},
        {"Dates",         nullptr, P_Dates,     Key::None, Shift::None},
        {"Bonds",         nullptr, P_Bonds,     Key::None, Shift::None},
        {"Depreciation",  nullptr, P_Depr,      Key::None, Shift::None},
        {"Modes",         nullptr, P_Modes,     Key::None, Shift::None},
        {"Memory",        nullptr, P_Memory,    Key::None, Shift::None},
        {"Help / Keys",   nullptr, P_Help,      Key::None, Shift::None},
    }, 10, false},

    // P_Financial
    {"Financial", {
        {"Solve TVM",    nullptr, P_Solve, Key::None,    Shift::None},
        {"NPV...",       "Fn+p",  -1,      Key::KeyPV,   Shift::F, &kFormNPV},
        {"IRR...",       "Fn+v",  -1,      Key::KeyFV,   Shift::F, &kFormIRR},
        {"Append CFj...","Sh+v",  -1,      Key::KeyFV,   Shift::G, &kFormCFj},
        {"Set Nj...",    "Sh+y",  -1,      Key::KeyYx,   Shift::G, &kFormNj},
        {"AMORT...",     "Fn+n",  -1,      Key::KeyN,    Shift::F, &kFormAmort},
        {"Round X",      "Fn+a",  -1,      Key::KeyPMT,  Shift::F},
        {"BEG mode",     "Sh+a",  -1,      Key::KeyPMT,  Shift::G},
        {"END mode",     "Sh+p",  -1,      Key::KeyPV,   Shift::G},
    }, 9, false},

    // P_Solve — TVM solver wizards.
    {"Solve TVM", {
        {"Solve n...",   nullptr, -1, Key::SolveN,   Shift::None, &kFormSolveN},
        {"Solve i...",   nullptr, -1, Key::SolveI,   Shift::None, &kFormSolveI},
        {"Solve PV...",  nullptr, -1, Key::SolvePV,  Shift::None, &kFormSolvePV},
        {"Solve PMT...", nullptr, -1, Key::SolvePMT, Shift::None, &kFormSolvePMT},
        {"Solve FV...",  nullptr, -1, Key::SolveFV,  Shift::None, &kFormSolveFV},
    }, 5, false},

    // P_Margin — pick a convention.
    {"Margin", {
        {"Mark Up",   nullptr, P_MarkUp,   Key::None, Shift::None},
        {"Mark Down", nullptr, P_MarkDown, Key::None, Shift::None},
    }, 2, false},

    // P_MarkUp  — % is based on COST.
    {"Mark Up", {
        {"Solve %...",     nullptr, -1, Key::SolveMarkupPct,   Shift::None, &kFormMarkupPct},
        {"Solve Price...", nullptr, -1, Key::SolveMarkupPrice, Shift::None, &kFormMarkupPrice},
        {"Solve Cost...",  nullptr, -1, Key::SolveMarkupCost,  Shift::None, &kFormMarkupCost},
        {"Solve $...",     nullptr, -1, Key::SolveDollar,      Shift::None, &kFormMarkupDollar},
    }, 4, false},

    // P_MarkDown  — % is based on PRICE (gross margin).
    {"Mark Down", {
        {"Solve %...",     nullptr, -1, Key::SolveMarginPct,   Shift::None, &kFormMarginPct},
        {"Solve Price...", nullptr, -1, Key::SolveMarginPrice, Shift::None, &kFormMarginPrice},
        {"Solve Cost...",  nullptr, -1, Key::SolveMarginCost,  Shift::None, &kFormMarginCost},
        {"Solve $...",     nullptr, -1, Key::SolveDollar,      Shift::None, &kFormMarginDollar},
    }, 4, false},

    // P_Growth — pick simple change vs. compound.
    {"Growth", {
        {"Change %",  nullptr, P_GrowthChange, Key::None, Shift::None},
        {"CAGR",      nullptr, P_GrowthCagr,   Key::None, Shift::None},
    }, 2, false},

    // P_GrowthChange — single-period change (Future = Prior * (1 + r)).
    {"Change %", {
        {"Solve %...",      nullptr, -1, Key::SolveMarkupPct,   Shift::None, &kFormChangePct},
        {"Solve Future...", nullptr, -1, Key::SolveMarkupPrice, Shift::None, &kFormChangeFuture},
        {"Solve Prior...",  nullptr, -1, Key::SolveMarkupCost,  Shift::None, &kFormChangePrior},
    }, 3, false},

    // P_GrowthCagr — compound (Future = Prior * (1 + r)^n).
    {"CAGR", {
        {"Solve CAGR...",   nullptr, -1, Key::SolveCAGR,        Shift::None, &kFormCAGR},
        {"Solve Future...", nullptr, -1, Key::SolveCAGRFuture,  Shift::None, &kFormCAGRFuture},
        {"Solve Prior...",  nullptr, -1, Key::SolveCAGRPrior,   Shift::None, &kFormCAGRPrior},
        {"Solve n...",      nullptr, -1, Key::SolveCAGRPeriods, Shift::None, &kFormCAGRPeriods},
    }, 4, false},

    // P_Stats
    {"Statistics", {
        {"Add point...", "s",    -1, Key::KeySigma, Shift::None, &kFormSigma},
        {"Remove pt...", "Fn+s", -1, Key::KeySigma, Shift::F,    &kFormSigmaR},
        {"Mean x",       "Sh+s", -1, Key::KeySigma, Shift::G},
        {"Std dev",      "Sh+x", -1, Key::KeySwap,  Shift::G},
        {"Predict y...", "Fn+r", -1, Key::KeyInv,   Shift::F,    &kFormPredY},
        {"Predict x...", "Sh+r", -1, Key::KeyInv,   Shift::G,    &kFormPredX},
        {"Clear stats",  "Sh+c", -1, Key::KeyClear, Shift::G},
    }, 7, false},

    // P_Dates
    {"Dates", {
        {"Date + days...", "Fn+d", -1, Key::KeyDelta, Shift::F, &kFormDate},
        {"Days between...","Sh+d", -1, Key::KeyDelta, Shift::G, &kFormDys},
    }, 2, false},

    // P_Bonds
    {"Bonds", {
        {"Solve PRICE...",  "Fn+b",  -1, Key::KeyPctT,    Shift::F,    &kFormPrice},
        {"Solve YTM...",    nullptr, -1, Key::SolveYTM,   Shift::None, &kFormSolveYTM},
        {"Solve coupon...", nullptr, -1, Key::SolveCoupon,Shift::None, &kFormSolveCoupon},
    }, 3, false},

    // P_Depr
    {"Depreciation", {
        {"SL...", "Fn+/", -1, Key::Div, Shift::F, &kFormSL},
    }, 1, false},

    // P_Modes
    {"Modes", {
        {"BEG mode",  "Sh+a", -1, Key::KeyPMT, Shift::G},
        {"END mode",  "Sh+p", -1, Key::KeyPV,  Shift::G},
        {"FIX...",    "Fn+#", -1, Key::None,   Shift::None, &kFormFix},
    }, 3, false},

    // P_Memory
    {"Memory", {
        {"Clear fin",     "Fn+e",  -1, Key::KeyClearFin, Shift::F},
        {"Clear storage", "Sh+e",  -1, Key::KeyClearFin, Shift::G},
        {"Clear stats",   "Sh+c",  -1, Key::KeyClear,    Shift::G},
        {"Clear X",       "Fn+c",  -1, Key::KeyClear,    Shift::F},
        {"STO  Tab+#",    "Tab+#", -1, Key::None,        Shift::None},
        {"RCL  sTab+#",   "sTab#", -1, Key::None,        Shift::None},
    }, 6, false},

    // P_Help — content rendered separately.
    {"Help", {}, 0, true},
};

// ── Help text (size-2, ≤19 chars per line) ───────────────────────────
const char* kHelpText =
    "ENTRY\n"
    " 0-9 .  digits\n"
    " Enter  push\n"
    " Bksp   del / CLx\n"
    " h      CHS\n"
    " e      EEX\n"
    "\n"
    "STACK\n"
    " x      x<>y\n"
    " o      roll down\n"
    " Fn+o   roll up\n"
    " Sh+Ent LASTx\n"
    "\n"
    "MATH\n"
    " + - * /  ops\n"
    " t      %\n"
    " Fn+t   LN\n"
    " Sh+t   e^x\n"
    " d      delta %\n"
    " b      %T\n"
    " y      y^x\n"
    " Fn+y   sqrt\n"
    " r      1/x\n"
    "\n"
    "TVM\n"
    " n      n\n"
    " i      i\n"
    " p      PV\n"
    " a      PMT\n"
    " v      FV\n"
    " Sh+n   x 12\n"
    " Sh+i   /12\n"
    " Fn+a   round X\n"
    " Sh+a   BEG\n"
    " Sh+p   END\n"
    " Solve  via menu:\n"
    "  Fin > Solve TVM\n"
    "\n"
    "CASH FLOWS\n"
    " Fn+p   NPV\n"
    " Fn+v   IRR\n"
    " Sh+v   add CFj\n"
    " Sh+y   set Nj\n"
    " Fn+n   AMORT\n"
    "\n"
    "STATS\n"
    " s      sigma+\n"
    " Fn+s   sigma-\n"
    " Sh+s   mean x\n"
    " Sh+x   std dev\n"
    " Fn+r   pred y\n"
    " Sh+r   pred x\n"
    "\n"
    "DATES\n"
    " Fn+d   date+days\n"
    " Sh+d   days btw\n"
    "\n"
    "MARGIN (menu)\n"
    " Mkup % = (P-C)/C\n"
    " Margn % = (P-C)/P\n"
    " Solve %, Price,\n"
    "  Cost, or $\n"
    "\n"
    "GROWTH (menu)\n"
    " Change %: simple\n"
    "  F = P*(1+r)\n"
    " CAGR: compound\n"
    "  F = P*(1+r)^n\n"
    " Solve any one\n"
    "\n"
    "BONDS\n"
    " Fn+b   PRICE\n"
    " Solve via menu:\n"
    "  Bonds > YTM\n"
    "  Bonds > coupon\n"
    "\n"
    "DEPRECIATION\n"
    " Fn+/   SL\n"
    "\n"
    "MEMORY\n"
    " Tab+#  STO R0-9\n"
    " sTab+# RCL\n"
    " Fn+e   clr fin\n"
    " Sh+e   clr stor\n"
    " Fn+c   CLx\n"
    " Sh+c   clr stats\n"
    "\n"
    "MODES\n"
    " Fn+#   FIX 0-9\n"
    "\n"
    "MENU\n"
    " Esc/m  open/close\n"
    " ; .    up/down\n"
    " Enter  select/run\n"
    " Bksp   back\n";

#ifndef UNIT_TEST

constexpr int kW = 240;
constexpr int kH = 135;

LGFX_Sprite gMenuCanvas(&LCD);
bool gMenuCanvasReady = false;

void ensureCanvas() {
    if (gMenuCanvasReady) return;
    gMenuCanvas.setColorDepth(16);
    gMenuCanvas.setPsram(true);
    gMenuCanvasReady = gMenuCanvas.createSprite(kW, kH);
    if (gMenuCanvasReady) gMenuCanvas.setTextWrap(false);
}

LovyanGFX& canvas() {
    ensureCanvas();
    if (gMenuCanvasReady) {
        gMenuCanvas.fillSprite(TFT_BLACK);
        return static_cast<LovyanGFX&>(gMenuCanvas);
    }
    LCD.fillScreen(TFT_BLACK);
    return static_cast<LovyanGFX&>(LCD);
}

void pushFrame() {
    if (gMenuCanvasReady) gMenuCanvas.pushSprite(0, 0);
}

void drawTitle(LovyanGFX& g, const char* title, const char* right_label = nullptr) {
    constexpr int title_h = 22;
    g.fillRect(0, 0, kW, title_h, TFT_NAVY);
    g.setTextSize(2);
    g.setTextColor(TFT_WHITE, TFT_NAVY);
    g.setCursor(6, 4);
    g.print(title);
    if (right_label) {
        int w = (int)std::strlen(right_label) * 12;
        g.setCursor(kW - 6 - w, 4);
        g.print(right_label);
    }
}

void drawList(const Page& page, int sel) {
    auto& g = canvas();
    drawTitle(g, page.title);

    constexpr int row_h    = 22;
    constexpr int top      = 24;
    constexpr int footer_h = 10;
    const int     visible  = (kH - top - footer_h) / row_h;
    int scroll = sel - visible / 2;
    if (scroll < 0) scroll = 0;
    int max_scroll = page.count - visible;
    if (max_scroll < 0) max_scroll = 0;
    if (scroll > max_scroll) scroll = max_scroll;

    int y = top;
    for (int i = scroll; i < page.count && i < scroll + visible; ++i) {
        bool focus = (i == sel);
        if (focus) {
            g.fillRect(0, y - 1, kW, row_h, TFT_DARKCYAN);
            g.setTextColor(TFT_WHITE, TFT_DARKCYAN);
        } else {
            g.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
        }
        g.setTextSize(2);
        g.setCursor(6, y + 3);
        g.print(focus ? "> " : "  ");
        g.print(page.items[i].label);
        y += row_h;
    }

    if (page.count > visible) {
        g.setTextSize(1);
        g.setTextColor(TFT_DARKGREY, TFT_BLACK);
        if (scroll > 0)              { g.setCursor(kW - 8, top + 2);                   g.print('^'); }
        if (scroll < max_scroll)     { g.setCursor(kW - 8, top + visible*row_h - 6);   g.print('v'); }
    }

    g.setTextSize(1);
    g.setTextColor(TFT_DARKGREY, TFT_BLACK);
    g.setCursor(2, kH - 9);
    g.print(";/. nav  Enter sel  Bksp back");

    pushFrame();
}

int totalHelpLines() {
    int n = 1;
    for (const char* p = kHelpText; *p; ++p) if (*p == '\n') ++n;
    return n;
}

void drawHelp(int scroll_lines) {
    auto& g = canvas();
    drawTitle(g, "Keys");

    constexpr int line_h = 18;
    constexpr int top    = 24;
    constexpr int bottom = kH - 11;
    int row = 0;
    int y   = top;
    const char* p = kHelpText;
    while (*p) {
        const char* line_start = p;
        while (*p && *p != '\n') ++p;
        int len = (int)(p - line_start);
        if (row >= scroll_lines && y + line_h <= bottom) {
            bool indented = len >= 1 && line_start[0] == ' ';
            g.setTextSize(2);
            g.setTextColor(indented ? TFT_LIGHTGREY : TFT_YELLOW, TFT_BLACK);
            g.setCursor(4, y);
            for (int i = 0; i < len; ++i) g.print(line_start[i]);
            y += line_h;
        }
        ++row;
        if (*p == '\n') ++p;
    }

    g.setTextSize(1);
    g.setTextColor(TFT_DARKGREY, TFT_BLACK);
    g.setCursor(2, kH - 9);
    g.print(";/. scroll    Bksp back");

    pushFrame();
}

void drawForm(const Form& form, int field_idx, int loop_count, int loop_max,
              const NumberEntry& entry,
              const double* values, const State& s) {
    auto& g = canvas();
    const FormField& f = form.fields[field_idx];

    // Title counter:
    //   loop, capped       → "k/N"  (current iter / total)
    //   loop, no cap       → "+k"
    //   non-loop           → "i/N"  (field number / total fields)
    char counter[16];
    if (f.loop) {
        if (loop_max > 0)
            std::snprintf(counter, sizeof(counter), "%d/%d", loop_count + 1, loop_max);
        else
            std::snprintf(counter, sizeof(counter), "+%d", loop_count);
    } else {
        std::snprintf(counter, sizeof(counter), "%d/%d", field_idx + 1, form.field_count);
    }
    drawTitle(g, form.title, counter);

    // Field label (with loop counter suffix when looping).
    g.setTextSize(2);
    g.setTextColor(TFT_YELLOW, TFT_BLACK);
    g.setCursor(8, 34);
    if (f.loop) {
        char lbuf[40];
        std::snprintf(lbuf, sizeof(lbuf), "%s #%d", f.label, loop_count);
        g.print(lbuf);
    } else {
        g.print(f.label);
    }

    // Field value (live entry buffer or stored value).
    g.setCursor(8, 64);
    g.setTextColor(TFT_WHITE, TFT_BLACK);
    g.print("> ");
    if (entry.active()) {
        g.print(entry.buffer().c_str());
        g.setTextColor(TFT_GREEN, TFT_BLACK);
        g.print('_');
    } else {
        FormatOpts opts;
        opts.fix_digits  = s.fix_digits;
        opts.comma_radix = (s.flags & kFlagCommaRad) != 0;
        std::string txt = formatNumber(values[field_idx], opts);
        g.print(txt.c_str());
    }

    // Footer hint
    g.setTextSize(1);
    g.setTextColor(TFT_DARKGREY, TFT_BLACK);
    g.setCursor(2, kH - 9);
    if (f.loop) {
        if (loop_max > 0) g.print("Enter add   Esc cancel");
        else              g.print("Enter add   Esc done    m=menu");
    } else if (field_idx + 1 < form.field_count) {
        g.print(";/. fields  Enter next  Bksp del");
    } else {
        g.print(";/. fields  Enter RUN   Bksp del");
    }

    pushFrame();
}

#endif  // !UNIT_TEST

double readTarget(const State& s, TargetKind k) {
    switch (k) {
        case TargetKind::StackX:    return s.X;
        case TargetKind::StackY:    return s.Y;
        case TargetKind::RegN:      return s.n;
        case TargetKind::RegI:      return s.i;
        case TargetKind::RegPV:     return s.PV;
        case TargetKind::RegPMT:    return s.PMT;
        case TargetKind::RegFV:     return s.FV;
        case TargetKind::FixDigits: return (double)s.fix_digits;
        case TargetKind::FlagBegin: return (s.flags & kFlagBegin) ? 1.0 : 0.0;
        case TargetKind::None:      return 0.0;
        case TargetKind::AppendCF:  return 0.0;
    }
    return 0.0;
}

void writeTarget(State& s, TargetKind k, double v) {
    switch (k) {
        case TargetKind::StackX:    s.X   = v; break;
        case TargetKind::StackY:    s.Y   = v; break;
        case TargetKind::RegN:      s.n   = v; break;
        case TargetKind::RegI:      s.i   = v; break;
        case TargetKind::RegPV:     s.PV  = v; break;
        case TargetKind::RegPMT:    s.PMT = v; break;
        case TargetKind::RegFV:     s.FV  = v; break;
        case TargetKind::FixDigits: {
            int d = (int)std::round(v);
            if (d < 0) d = 0;
            if (d > 9) d = 9;
            s.fix_digits = (uint8_t)d;
            break;
        }
        case TargetKind::AppendCF:
            appendCashFlow(s, v, 1);
            break;
        case TargetKind::FlagBegin:
            if (v != 0.0) s.flags |= kFlagBegin;
            else          s.flags &= ~kFlagBegin;
            break;
        case TargetKind::None: break;
    }
}

}  // namespace

// ── Menu class methods ──────────────────────────────────────────────

void Menu::open() {
    open_ = true;
    page_ = P_Root;
    sel_  = 0;
    mode_ = Mode::List;
    form_ = nullptr;
}

void Menu::close() {
    open_ = false;
    mode_ = Mode::List;
    form_ = nullptr;
    entry_.reset();
}

void Menu::openForm(const Form* form, State& s) {
    form_        = form;
    field_idx_   = 0;
    loop_count_  = 0;
    loop_max_    = 0;
    mode_        = Mode::Form;
    entry_.reset();
    if (form->clear_cfs_on_open) clearCashFlows(s);
    for (int i = 0; i < form->field_count; ++i) {
        if (form->fields[i].loop || form->fields[i].is_loop_count)
            field_vals_[i] = 0.0;
        else
            field_vals_[i] = readTarget(s, form->fields[i].target);
    }
}

void Menu::commitEntryToField() {
    if (entry_.active()) {
        field_vals_[field_idx_] = entry_.value();
        entry_.reset();
    }
}

void Menu::applyForm(State& s) {
    if (!form_) return;
    // Loop fields apply per-iteration during the loop; skip them here.
    for (int i = 0; i < form_->field_count; ++i) {
        if (form_->fields[i].loop) continue;
        writeTarget(s, form_->fields[i].target, field_vals_[i]);
    }
}

bool Menu::handleFormChar(char c, Dispatcher& dispatcher, State& state) {
    if (!form_) { mode_ = Mode::List; return true; }

    // 'm' anywhere → jump to main menu (cancels form).
    if (c == 'm' || c == 'M') {
        entry_.reset();
        mode_ = Mode::List;
        form_ = nullptr;
        page_ = P_Root;
        sel_  = 0;
        return true;
    }

    const FormField& cur = form_->fields[field_idx_];

    // Esc always cancels the form (never runs). Returns to current sub-menu.
    if (c == 0x1b) {
        entry_.reset();
        mode_ = Mode::List;
        form_ = nullptr;
        return true;
    }

    if (c == '\b') {
        if (entry_.active()) {
            entry_.backspace();
        } else if (!cur.loop && field_idx_ > 0) {
            --field_idx_;
        } else {
            mode_ = Mode::List;
            form_ = nullptr;
        }
        return true;
    }

    // Field navigation only on non-loop fields.
    if (!cur.loop) {
        if (c == ';')  { commitEntryToField(); if (field_idx_ > 0) --field_idx_; return true; }
        if (c == '\t') { commitEntryToField(); if (field_idx_ + 1 < form_->field_count) ++field_idx_; return true; }
    }

    if (c == '\r' || c == '\n') {
        commitEntryToField();
        if (cur.loop) {
            // Apply this iteration's value (e.g. appendCashFlow) and stay.
            writeTarget(state, cur.target, field_vals_[field_idx_]);
            ++loop_count_;
            field_vals_[field_idx_] = 0.0;
            entry_.reset();
            // Auto-run when the loop count cap is reached.
            if (loop_max_ > 0 && loop_count_ >= loop_max_) {
                applyForm(state);
                if (form_->run_key != Key::None) {
                    dispatcher.handle(form_->run_key, form_->run_shift);
                }
                close();
            }
            return true;
        }
        // Non-loop: capture loop cap if this is the count field.
        if (cur.is_loop_count) {
            int v = (int)field_vals_[field_idx_];
            loop_max_ = (v > 0) ? v : 0;
        }
        // Advance, or run if we're at the end with no loop following.
        if (field_idx_ + 1 < form_->field_count) {
            ++field_idx_;
        } else {
            applyForm(state);
            if (form_->run_key != Key::None) {
                dispatcher.handle(form_->run_key, form_->run_shift);
            }
            close();
        }
        return true;
    }

    if (c >= '0' && c <= '9') { entry_.digit(c); return true; }
    if (c == '.')              { entry_.dot();   return true; }
    if (c == 'h' || c == 'H')  { entry_.chs();   return true; }
    if (c == 'e' || c == 'E')  { entry_.eex();   return true; }
    return true;
}

bool Menu::handleChar(char c, bool fn_held, bool shift_held,
                      Dispatcher& dispatcher, State& state) {
    (void)fn_held; (void)shift_held;
    if (!open_) return false;

    if (mode_ == Mode::Form) {
        return handleFormChar(c, dispatcher, state);
    }

    const Page& page = kPages[page_];

    // 'm' anywhere in the menu always jumps to the main menu.
    if (c == 'm' || c == 'M') {
        page_ = P_Root;
        sel_  = 0;
        return true;
    }

    if (c == 0x1b) {                 // Esc
        if (page_ == P_Root) close();
        else { page_ = P_Root; sel_ = 0; }
        return true;
    }
    if (c == '\b') {                 // Backspace = back
        if (page_ == P_Root) close();
        else { page_ = P_Root; sel_ = 0; }
        return true;
    }

    if (page.is_help) {
#ifndef UNIT_TEST
        int total = totalHelpLines();
        constexpr int visible = 5;
        if (c == ';')      { if (sel_ > 0) --sel_; }
        else if (c == '.') { if (sel_ < total - visible) ++sel_; }
#else
        (void)c;
#endif
        return true;
    }

    if (c == ';') { if (sel_ > 0) --sel_; return true; }
    if (c == '.') { if (sel_ < page.count - 1) ++sel_; return true; }

    if (c == '\r' || c == '\n') {
        const Item& it = page.items[sel_];
        if (it.nav >= 0) {
            page_ = it.nav;
            sel_  = 0;
        } else if (it.form) {
            openForm(it.form, state);
        } else if (it.key != Key::None) {
            dispatcher.handle(it.key, it.shift);
            close();
        } else {
            close();
        }
        return true;
    }
    return true;
}

void Menu::render(const State& s) const {
#ifndef UNIT_TEST
    if (!open_) return;
    if (mode_ == Mode::Form && form_) {
        drawForm(*form_, field_idx_, loop_count_, loop_max_, entry_, field_vals_, s);
    } else if (kPages[page_].is_help) {
        drawHelp(sel_);
    } else {
        drawList(kPages[page_], sel_);
    }
#else
    (void)s;
#endif
}

}  // namespace hp12c
