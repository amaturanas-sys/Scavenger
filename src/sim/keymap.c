#include "sim/keymap.h"

#include <stdio.h>

#include "sim/lang.h"

static const KeyBind BINDS[KA_COUNT] = {
    [KA_ATTACK] = { 'H', 0, KM_CTRL }, // Ctrl+H: los controles
    [KA_BLOCK] = { 'J', 0, KM_CTRL },
    [KA_PARRY] = { 'K', 0, KM_CTRL },
    [KA_CHARGE] = { 'L', 0, KM_CTRL }, // con Mayus (corriendo) tambien: carga o patada a la carrera
    [KA_TARGET] = { KM_KEY_TAB, 0, KM_CTRL },
    [KA_SNEAK] = { 'C', 0, KM_CTRL },
    [KA_CROUCH] = { 'X', 0, KM_CTRL },
    [KA_JUMP] = { KM_KEY_SPACE, 0, KM_CTRL },
    [KA_INTERACT] = { 'F', 0, KM_CTRL },
    [KA_GRAB] = { 'G', 0, KM_SHIFT | KM_CTRL },
    [KA_BACKPACK] = { 'G', KM_SHIFT, KM_CTRL },
    [KA_LIGHT_ARROW] = { 'L', KM_SHIFT, KM_CTRL },
    [KA_THROW] = { 'T', 0, KM_CTRL },
    [KA_MOUNT] = { 'R', 0, KM_CTRL },
    [KA_BANDAGE] = { 'B', 0, KM_CTRL },
    [KA_DRINK] = { 'N', 0, KM_CTRL },
    [KA_ESCORT] = { 'Y', 0, KM_CTRL },
    [KA_MARK] = { 'M', 0, KM_CTRL }, // Ctrl+M: la vista orbital
    [KA_MENU] = { KM_KEY_TAB, 0, KM_CTRL },
    [KA_INVENTORY] = { 'I', 0, KM_CTRL },
    [KA_EQUIPMENT] = { 'P', 0, KM_CTRL }, // Ctrl+P: la pausa
    [KA_CARD] = { 'U', 0, KM_CTRL },
};

const KeyBind *keymap_bind(KeyAction a) { return (unsigned)a < KA_COUNT ? &BINDS[a] : &BINDS[0]; }

bool keymap_matches(KeyAction a, int key, unsigned mods) {
    if ((unsigned)a >= KA_COUNT) return false;
    const KeyBind *b = &BINDS[a];
    return b->key == key && (mods & b->need) == b->need && !(mods & b->forbid);
}

const char *keymap_text(KeyAction a) {
    static char buf[4][32];
    static int next;
    const KeyBind *b = keymap_bind(a);
    char key[16];
    if (b->key == KM_KEY_TAB) snprintf(key, sizeof(key), "Tab");
    else if (b->key == KM_KEY_SPACE) snprintf(key, sizeof(key), "%s", T("Espacio"));
    else snprintf(key, sizeof(key), "%c", (char)b->key);
    char *out = buf[next++ % 4];
    snprintf(out, sizeof(buf[0]), "%s%s", b->need & KM_SHIFT ? T("Mayús+") : "", key);
    return out;
}
