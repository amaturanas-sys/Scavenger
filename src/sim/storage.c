#include "sim/storage.h"
#include "sim/lang.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static bool starts(const char *s, const char *p) { return !strncmp(s, p, strlen(p)); }

bool item_is_gear(const char *id) {
    return starts(id, "arma.") || starts(id, "escudo.") || starts(id, "armadura.");
}

bool item_storable(const InvItem *it) {
    if (!it || it->texture) return false;
    const char *id = it->id;
    if (starts(id, "estructura.") || starts(id, "vehiculo.") || starts(id, "mapa.") || starts(id, "personaje.") ||
        starts(id, "animal.") || starts(id, "totem."))
        return false;
    return fmaxf(it->w, fmaxf(it->h, it->l)) <= 3.0f;
}

float item_kg(const InvItem *it) {
    if (!it) return 1.0f;
    const char *id = it->id;
    // Materiales del acopio: por unidad.
    static const struct {
        const char *id;
        float kg;
    } FIXED[] = {
        { "utileria.material.troncos", 15.0f }, { "utileria.material.piedra", 10.0f }, { "utileria.material.barro", 8.0f },
        { "utileria.material.pieles", 3.0f },   { "utileria.material.cuerda", 1.0f },  { "utileria.material.carbon", 5.0f },
        { "utileria.objeto.lena", 6.0f },
    };
    for (size_t i = 0; i < sizeof(FIXED) / sizeof(FIXED[0]); i++)
        if (!strcmp(id, FIXED[i].id)) return FIXED[i].kg;
    if (starts(id, "utileria.material.")) return 4.0f;  // minerales y lingotes
    if (starts(id, "utileria.consumible.")) return 0.5f;
    if (starts(id, "proyectil.")) return 0.05f;
    if (starts(id, "accesorio.amuleto.")) return 0.05f;
    float vol = it->w * it->h * it->l;
    float kg = vol * 400.0f; // un objeto lleno de madera y cuero
    if (starts(id, "arma.")) kg = fminf(fmaxf(vol * 900.0f, 0.4f), 6.0f);
    else if (starts(id, "escudo.")) kg = fminf(fmaxf(vol * 300.0f, 1.5f), 7.0f);
    else if (starts(id, "armadura.")) kg = fminf(fmaxf(vol * 120.0f, 0.5f), 14.0f) * (strstr(id, "hierro") || strstr(id, "malla") ? 2.0f : 1.0f);
    return fminf(fmaxf(kg, 0.05f), 80.0f);
}

const char *bag_kind_name(BagKind k) {
    static const char *names[BAG_KIND_COUNT] = { N_("Bolsillos"), N_("Mochila"), N_("Alforjas"), N_("Carreta"), N_("Acopio del campamento") };
    return (unsigned)k < BAG_KIND_COUNT ? T(names[k]) : "?";
}

void bag_init(Bag *b, BagKind kind, float cap_kg) {
    memset(b, 0, sizeof(*b));
    b->kind = kind;
    switch (kind) {
    case BAG_POCKETS: b->cap_kg = 2.0f, b->max_size = 0.3f, b->slots = 6; break;
    case BAG_BACKPACK: b->cap_kg = 25.0f, b->max_size = 1.4f, b->slots = 16; break;
    case BAG_MOUNT: b->cap_kg = cap_kg, b->max_size = 2.0f, b->slots = 20; break;
    case BAG_CART: b->cap_kg = 400.0f, b->max_size = 3.0f, b->slots = BAG_SLOTS; break;
    default: b->cap_kg = 100000.0f, b->max_size = 3.0f, b->slots = BAG_SLOTS; break;
    }
    if (cap_kg > 0.0f && kind != BAG_MOUNT) b->cap_kg = cap_kg;
}

float bag_kg(const Bag *b, const Inventory *inv) {
    float kg = 0.0f;
    for (int i = 0; i < b->n; i++) kg += item_kg(inventory_find(inv, b->s[i].id)) * (float)b->s[i].count;
    return kg;
}

int bag_count(const Bag *b, const char *id) {
    int n = 0;
    for (int i = 0; i < b->n; i++)
        if (!strcmp(b->s[i].id, id)) n += b->s[i].count;
    return n;
}

int bag_add(Bag *b, const Inventory *inv, const char *id, int n, float condition) {
    return bag_add_var(b, inv, id, n, condition, 0);
}

int bag_add_var(Bag *b, const Inventory *inv, const char *id, int n, float condition, unsigned short var) {
    const InvItem *it = inventory_find(inv, id);
    if (!it || n <= 0 || !item_storable(it)) return 0;
    if (fmaxf(it->w, fmaxf(it->h, it->l)) > b->max_size) return 0; // no cabe
    float kg = item_kg(it), free_kg = b->cap_kg - bag_kg(b, inv);
    int fits = kg > 0.0f ? (int)floorf(free_kg / kg + 1e-4f) : n;
    if (fits < n) n = fits;
    if (n <= 0) return 0;
    bool gear = item_is_gear(id);
    int added = 0;
    // Lo apilable (o el equipo en perfecto estado) va con los suyos.
    for (int i = 0; i < b->n && added < n; i++) {
        BagSlot *s = &b->s[i];
        if (strcmp(s->id, id) != 0 || s->var != var) continue; // otra variante (una joya con otra piedra)
        if (gear && (condition < 0.999f || s->condition < 0.999f)) continue;
        s->count += n - added;
        added = n;
    }
    // Si no, un hueco nuevo (el equipo gastado, uno por hueco).
    while (added < n && b->n < b->slots) {
        BagSlot *s = &b->s[b->n++];
        snprintf(s->id, sizeof(s->id), "%s", id);
        s->var = var;
        s->condition = gear ? fminf(1.0f, fmaxf(0.0f, condition)) : 1.0f;
        int put = gear && condition < 0.999f ? 1 : n - added;
        s->count = put;
        added += put;
    }
    return added;
}

static void remove_slot(Bag *b, int i) {
    for (int k = i; k < b->n - 1; k++) b->s[k] = b->s[k + 1];
    b->n--;
}

int bag_take(Bag *b, const char *id, int n, float *condition) {
    int taken = 0;
    // Primero lo mas gastado (las piezas nuevas se guardan para despues).
    for (int pass = 0; pass < 2 && taken < n; pass++)
        for (int i = b->n - 1; i >= 0 && taken < n; i--) {
            BagSlot *s = &b->s[i];
            if (strcmp(s->id, id) != 0 || (pass == 0 && s->condition >= 0.999f)) continue;
            int t = s->count < n - taken ? s->count : n - taken;
            s->count -= t;
            taken += t;
            if (condition) *condition = s->condition;
            if (s->count <= 0) remove_slot(b, i);
        }
    return taken;
}

void bag_remove_slot(Bag *b, int slot, int n) {
    if (slot < 0 || slot >= b->n || n <= 0) return;
    b->s[slot].count -= n;
    if (b->s[slot].count <= 0) remove_slot(b, slot);
}

int bag_move_slot(Bag *from, int slot, Bag *to, const Inventory *inv, int n) {
    if (slot < 0 || slot >= from->n || n <= 0) return 0;
    BagSlot s = from->s[slot];
    if (n > s.count) n = s.count;
    int moved = bag_add_var(to, inv, s.id, n, s.condition, s.var);
    if (moved <= 0) return 0;
    from->s[slot].count -= moved;
    if (from->s[slot].count <= 0) remove_slot(from, slot);
    return moved;
}

float bag_speed_scale(float carried_kg, float limit_kg) {
    if (carried_kg <= limit_kg || limit_kg <= 0.0f) return 1.0f;
    return fmaxf(0.5f, 1.0f - (carried_kg - limit_kg) / (limit_kg * 2.0f));
}

float mount_capacity_kg(int species) {
    // Mismo orden que Species (src/sim/animals.h): caballo, mula, burro, buey, camello, elefante.
    static const float kg[] = { 50.0f, 80.0f, 60.0f, 90.0f, 120.0f, 200.0f };
    return species >= 0 && species < 6 ? kg[species] : 40.0f;
}
