#include "decor_draw.h"

#include "raymath.h"
#include "sim/decor.h"

// Hasta donde se ve cada cosa (m): lo grande, en todo lo cargado; lo chico, de cerca.
static float reach(DecorKind kind) {
    switch (decor_norm(kind)->cls) {
    case DECOR_CLASS_RIDGE:
    case DECOR_CLASS_BIG: return 1e9f;
    case DECOR_CLASS_MEDIUM: return 170.0f;
    case DECOR_CLASS_FOOT: return 60.0f;
    default: return kind == DECOR_PUENTE || kind == DECOR_MUELLE || kind == DECOR_MUELLE_FIN ? 170.0f : 110.0f;
    }
}

void decor_draw(const Terrain *t, Props *p, const Inventory *inv, Camera3D cam) {
    static const InvItem *items[DECOR_KINDS];
    static const Inventory *loaded;
    if (loaded != inv) { // los ids, una vez
        for (int k = 0; k < DECOR_KINDS; k++) items[k] = inventory_find(inv, decor_norm((DecorKind)k)->id);
        loaded = inv;
    }
    for (int i = 0; i < CHUNK_SLOTS; i++) {
        const Chunk *ch = &t->slots[i];
        if (!ch->loaded) continue;
        for (int k = 0; k < ch->decor_count; k++) {
            const ChunkDecor *d = &ch->decor[k];
            const InvItem *item = items[d->kind];
            float dx = d->x - cam.position.x, dz = d->z - cam.position.z, r = reach((DecorKind)d->kind);
            if (!item || dx * dx + dz * dz > r * r) continue;
            Matrix m = MatrixMultiply(MatrixMultiply(MatrixScale(d->scale * d->stretch, d->scale, d->scale), MatrixRotateY(d->yaw)),
                                      MatrixTranslate(d->x, d->y, d->z));
            if (decor_climbable((DecorKind)d->kind)) props_draw_item_biome(p, item, m, d->biome);
            else props_draw_item_matrix(p, item, m);
        }
    }
}
