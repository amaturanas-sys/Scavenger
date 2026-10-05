#include "input.h"

#include "raylib.h"

static bool g_injected[IN_COUNT];
static int g_last_key;

bool input_ctrl(void) { return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL); }

void input_inject(InputAction a) {
    if ((unsigned)a < IN_COUNT) g_injected[a] = true;
}

void input_update(void) {
    for (int k = GetKeyPressed(); k; k = GetKeyPressed()) g_last_key = k;
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
