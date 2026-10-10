#include "input.h"

#include <math.h>

#include "raylib.h"

#define DRAG_PIXELS 6.0f // lo que se mueve el raton con el boton derecho antes de girar la camara

static bool g_injected[IN_COUNT];
static int g_last_key;
static bool g_right_drag; // el boton derecho ya arrastra (gira la camara)
static float g_right_moved;

bool input_ctrl(void) { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); }

void input_inject(InputAction a) {
    if ((unsigned)a < IN_COUNT) g_injected[a] = true;
}

void input_update(void) {
    for (int k = GetKeyPressed(); k; k = GetKeyPressed()) g_last_key = k;
    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) g_right_drag = false, g_right_moved = 0.0f;
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        Vector2 d = GetMouseDelta();
        g_right_moved += fabsf(d.x) + fabsf(d.y);
        if (g_right_moved > DRAG_PIXELS) g_right_drag = true;
    } else {
        g_right_drag = false;
    }
}

unsigned input_mods(void) {
    unsigned m = 0;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) m |= KM_SHIFT;
    if (input_ctrl()) m |= KM_CTRL;
    if (IsKeyDown(KEY_LEFT_ALT) || IsKeyDown(KEY_RIGHT_ALT)) m |= KM_ALT;
    return m;
}

static bool action_key(KeyAction a, bool (*test)(int)) {
    const KeyBind *b = keymap_bind(a);
    return test(b->key) && keymap_matches(a, b->key, input_mods());
}

bool input_action_pressed(KeyAction a) { return action_key(a, IsKeyPressed); }
bool input_action_down(KeyAction a) { return action_key(a, IsKeyDown); }
bool input_action_released(KeyAction a) { return action_key(a, IsKeyReleased); }

bool input_right_hold(void) { return IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !g_right_drag; }

bool input_right_drag(float *dx, float *dy) {
    if (!g_right_drag || !IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) return false;
    Vector2 d = GetMouseDelta();
    *dx = d.x, *dy = d.y;
    return true;
}

int input_last_key(void) { return g_last_key; }

static bool ctrl_key(int key) { return input_ctrl() && IsKeyPressed(key); }

bool input_pressed(InputAction a) {
    if ((unsigned)a >= IN_COUNT) return false;
    if (g_injected[a]) {
        g_injected[a] = false;
        return true;
    }
    switch (a) {
    case IN_PAUSE: return IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACK) || ctrl_key(KEY_P);
    case IN_BACK: return IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_BACK) || IsKeyPressed(KEY_BACKSPACE);
    case IN_HELP: return IsKeyPressed(KEY_F1) || ctrl_key(KEY_H);
    case IN_ORBITAL: return IsKeyPressed(KEY_F5) || ctrl_key(KEY_M);
    case IN_ABILITY1: return IsKeyPressed(KEY_F2) || ctrl_key(KEY_ONE);
    case IN_ABILITY2: return IsKeyPressed(KEY_F3) || ctrl_key(KEY_TWO);
    case IN_ABILITY3: return IsKeyPressed(KEY_F4) || ctrl_key(KEY_THREE);
    case IN_DIAG: return ctrl_key(KEY_D);
    default: return false;
    }
}

const char *input_keys_text(InputAction a) {
    switch (a) {
    case IN_PAUSE: return "Esc / Atrás / Ctrl+P";
    case IN_BACK: return "Esc / Atrás / Retroceso";
    case IN_HELP: return "F1 / Ctrl+H";
    case IN_ORBITAL: return "F5 / Ctrl+M";
    case IN_ABILITY1: return "F2 / Ctrl+1";
    case IN_ABILITY2: return "F3 / Ctrl+2";
    case IN_ABILITY3: return "F4 / Ctrl+3";
    case IN_DIAG: return "Ctrl+D";
    default: return "";
    }
}

static bool g_debug;
void input_set_debug(bool on) { g_debug = on; }
bool input_debug(void) { return g_debug; }
