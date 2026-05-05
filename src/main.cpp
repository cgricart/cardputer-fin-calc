#ifndef UNIT_TEST

#include <M5Cardputer.h>

#include "../include/state.h"
#include "app/dispatcher.h"
#include "app/errors.h"
#include "mem/persistence.h"
#include "mem/registers.h"
#include "rpn/entry.h"
#include "ui/display.h"
#include "ui/keymap.h"
#include "ui/menu.h"

using namespace hp12c;

namespace {
State        g_state;
NumberEntry  g_entry;
ErrorState   g_err;
Persistence  g_persist;
Display      g_display;
Dispatcher   g_dispatch(g_state, g_entry, g_err);
Menu         g_menu;
}

void setup() {
    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setBrightness(140);

    g_display.begin();
    g_display.splash();

    g_persist.begin();
    if (!g_persist.load(g_state)) {
        resetState(g_state);
    }
    g_display.render(g_state, g_entry, g_err, false, false);
}

namespace {

bool fnHeldNow() {
    return M5Cardputer.Keyboard.isKeyPressed(KEY_FN);
}

}  // namespace

void loop() {
    M5Cardputer.update();

    bool any_change = false;
    bool menu_change = false;
    static bool s_last_fn   = false;
    static bool s_last_g    = false;
    static bool s_last_open = false;

    bool fn_now = fnHeldNow();
    if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
        Keyboard_Class::KeysState ks = M5Cardputer.Keyboard.keysState();

        // Menu open: route ALL input to the menu, then early-out.
        if (g_menu.isOpen()) {
            for (char c : ks.word) {
                if (g_menu.handleChar(c, fn_now, ks.shift, g_dispatch, g_state))
                    menu_change = true;
            }
            if (ks.del)   { g_menu.handleChar('\b', fn_now, ks.shift, g_dispatch, g_state); menu_change = true; }
            if (ks.enter) { g_menu.handleChar('\r', fn_now, ks.shift, g_dispatch, g_state); menu_change = true; }
            // The dispatcher may have run from a menu action; treat as state change too.
            if (!g_menu.isOpen()) any_change = true;
        } else {
            Shift override = Shift::None;
            if (fn_now)        override = Shift::F;
            else if (ks.shift) override = Shift::G;

            // Esc, or plain 'm' (no Fn / no Shift), opens the menu.
            bool opened_menu = false;
            bool no_modifier = !fn_now && !ks.shift;
            for (char c : ks.word) {
                if (c == 0x1b || (no_modifier && (c == 'm' || c == 'M'))) {
                    g_menu.open();
                    opened_menu = true;
                    menu_change = true;
                    break;
                }
            }
            if (!opened_menu) {
                for (char c : ks.word) {
                    char nc = c;
                    if (nc >= 'A' && nc <= 'Z') nc = (char)(nc + ('a' - 'A'));
                    Key k = keyForChar(nc);
                    if (k != Key::None) {
                        g_dispatch.handle(k, override);
                        any_change = true;
                    }
                }
                if (ks.del)   { g_dispatch.handle(Key::Backspace, override); any_change = true; }
                if (ks.enter) { g_dispatch.handle(Key::Enter, override);     any_change = true; }
                if (ks.tab)   { g_dispatch.handle(ks.shift ? Key::Rcl : Key::Sto, Shift::None); any_change = true; }
            }
        }
    }

    bool g_now = M5Cardputer.Keyboard.isPressed() && M5Cardputer.Keyboard.keysState().shift;
    bool annunciator_changed = (fn_now != s_last_fn) || (g_now != s_last_g);
    s_last_fn = fn_now;
    s_last_g  = g_now;

    bool open_changed = (g_menu.isOpen() != s_last_open);
    s_last_open = g_menu.isOpen();

    // Periodic refresh so the battery readout stays fresh while idle.
    static uint32_t s_last_refresh_ms = 0;
    uint32_t now_ms = millis();
    bool periodic = (now_ms - s_last_refresh_ms) > 30000;
    if (periodic) s_last_refresh_ms = now_ms;

    if (any_change) g_persist.markDirty();
    g_persist.tick(g_state, now_ms);

    if (g_menu.isOpen()) {
        if (menu_change || open_changed) g_menu.render(g_state);
    } else {
        if (any_change || annunciator_changed || open_changed || periodic) {
            g_display.render(g_state, g_entry, g_err,
                             g_dispatch.currentShift() == Shift::F || fn_now,
                             g_dispatch.currentShift() == Shift::G);
        }
    }

    delay(20);
}

#endif  // !UNIT_TEST
