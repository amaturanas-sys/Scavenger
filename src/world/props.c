#include "props.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "sim/anim_index.h"
#include "raymath.h"
#include "rlgl.h"
#include "vegetation.h"
#include "voxmodel.h"
#include "voxstruct.h"

static Color g_tint = { 255, 255, 255, 255 }; // la luz de alrededor (props_set_tint)

void props_init(Props *p, const Inventory *inv) {
    memset(p, 0, sizeof(*p));
    p->inv = inv;
    p->season = -1;
}

void props_set_season(Props *p, int season) { p->season = season; }

const char *props_season_suffix(int season) {
    static const char *names[] = { "primavera", "verano", "otono", "invierno" };
    return season >= 0 && season < 4 ? names[season] : NULL;
}

void props_unload(Props *p) {
    for (int i = 0; i < p->cached; i++) {
        if (p->cache[i].anims) UnloadModelAnimations(p->cache[i].anims, p->cache[i].anim_count);
        if (p->cache[i].loaded) UnloadModel(p->cache[i].model);
    }
    p->cached = 0;
    p->count = 0;
}

int props_add(Props *p, const char *id, Vector3 pos, float yaw) {
    const InvItem *it = p->inv ? inventory_find(p->inv, id) : NULL;
    if (!it || p->count >= PROPS_MAX) return -1;
    p->items[p->count] = (Prop){ it, pos, yaw, false, { 0, 0, 0 }, 1.0f };
    return p->count++;
}

void props_remove(Props *p, int index) {
    if (index < 0 || index >= p->count) return;
    p->items[index] = p->items[--p->count];
}

bool props_takeable(const InvItem *item) { return fmaxf(fmaxf(item->w, item->h), item->l) <= 1.2f && !item->texture; }

int props_nearest(const Props *p, Vector3 from, float radius, bool takeable_only) {
    int best = -1;
    float best_d = radius;
    for (int i = 0; i < p->count; i++) {
        const Prop *pr = &p->items[i];
        if (pr->flying || (takeable_only && !props_takeable(pr->item))) continue;
        float d = Vector2Distance((Vector2){ from.x, from.z }, (Vector2){ pr->pos.x, pr->pos.z });
        if (d <= best_d) {
            best_d = d;
            best = i;
        }
    }
    return best;
}

// Indice en la cache del id y la variante (cargando el modelo y sus clips una sola vez), o -1.
// Las rocas de cada region (v0.15.2): la misma roca con la piedra de su region y, en lo que mira arriba, lo que la
// cubre: liquen seco en la estepa, musgo en el bosque, nieve en el altiplano, liquen mojado en los fiordos y polvo de
// arena en el desierto, con vetas. La luz horneada y los bloques de voxeles siguen: el brillo de cada cara, sobre el medio.
#define BIOME_VARIANT 100
#define BODY_VARIANT 200 // + 1 la mujer, 2 el gigante, 3 el protagonista: "<nombre>@<cuerpo>.glb" (props_draw_worn)
static const char *const BODY_SUFFIX[] = { NULL, "mujer", "gigante", "protagonista" };
static void recolor_biome(Model *m, int biome) {
    static const float STONE[5][3] = { { 132, 128, 118 }, { 100, 106, 98 }, { 150, 156, 168 }, { 84, 90, 100 }, { 198, 136, 88 } };
    static const float COVER[5][3] = { { 156, 150, 96 }, { 70, 104, 54 }, { 238, 242, 248 }, { 104, 124, 92 }, { 218, 182, 128 } };
    static const float AMOUNT[5] = { 0.5f, 0.85f, 0.95f, 0.6f, 0.6f };
    if (biome < 0 || biome > 4) return;
    for (int i = 0; i < m->meshCount; i++) {
        Mesh *me = &m->meshes[i];
        if (!me->colors || !me->normals || me->vertexCount <= 0) continue;
        float lum = 0.0f, ymin = 1e9f, ymax = -1e9f;
        for (int v = 0; v < me->vertexCount; v++) {
            const unsigned char *c = &me->colors[v * 4];
            lum += 0.3f * c[0] + 0.59f * c[1] + 0.11f * c[2];
            ymin = fminf(ymin, me->vertices[v * 3 + 1]), ymax = fmaxf(ymax, me->vertices[v * 3 + 1]);
        }
        lum = fmaxf(lum / (float)me->vertexCount, 1.0f);
        int group = me->indices || me->vertexCount % 3 ? 1 : 3; // facetada (sin indices): cara por cara
        for (int v = 0; v < me->vertexCount; v += group) {
            float y = 0.0f;
            for (int g = 0; g < group; g++) y += me->vertices[(v + g) * 3 + 1] / (float)group;
            float up = me->normals[v * 3 + 1], cover = AMOUNT[biome] * fminf(fmaxf((up - 0.5f) / 0.3f, 0.0f), 1.0f);
            float band = biome == 4 && ((int)((y - ymin) / fmaxf(ymax - ymin, 0.01f) * 9.0f) & 1) ? 0.88f : 1.0f; // las vetas
            for (int g = 0; g < group; g++) {
                unsigned char *c = &me->colors[(v + g) * 4];
                float shade = fminf(fmaxf((0.3f * c[0] + 0.59f * c[1] + 0.11f * c[2]) / lum, 0.45f), 1.5f) * band;
                for (int j = 0; j < 3; j++) c[j] = (unsigned char)fminf(255.0f, (STONE[biome][j] * (1.0f - cover) + COVER[biome][j] * cover) * shade);
            }
        }
        UpdateMeshBuffer(*me, 3, me->colors, me->vertexCount * 4, 0);
    }
}

static int load_slot(Props *p, const InvItem *item, int variant) {
    for (int i = 0; i < p->cached; i++)
        if (p->cache[i].item == item && p->cache[i].variant == variant) return i;
    if (p->cached >= MODEL_CACHE_MAX) return -1;
    char path[160];
    inventory_path(item, path, sizeof(path));
    const char *suffix = variant >= 0 && variant < BIOME_VARIANT ? props_season_suffix(variant)
                         : variant > BODY_VARIANT && variant < BODY_VARIANT + 4 ? BODY_SUFFIX[variant - BODY_VARIANT] : NULL;
    if (suffix) { // "dir/nombre.glb" -> "dir/nombre@invierno.glb", "dir/nombre@mujer.glb"
        char *dot = strrchr(path, '.');
        if (!dot) return -1;
        char ext[8];
        snprintf(ext, sizeof(ext), "%s", dot);
        snprintf(dot, sizeof(path) - (size_t)(dot - path), "@%s%s", suffix, ext);
    }
    const char *full = platform_asset_path(path);
    int slot = p->cached++;
    p->cache[slot].item = item;
    p->cache[slot].variant = variant;
    p->cache[slot].loaded = false;
    p->cache[slot].anims = NULL;
    p->cache[slot].anim_count = 0;
    p->cache[slot].posed = false;
    if (item->texture) return slot;
    if (!platform_asset_exists(full)) return slot; // en Android, dentro del APK
    if (item->voxel) { // un .vox: a la altura de sus medidas, sin animaciones
        p->cache[slot].loaded = vm_load(full, item->h, &p->cache[slot].model);
        platform_note_asset(full, p->cache[slot].loaded);
        return slot;
    }
    p->cache[slot].model = LoadModel(full);
    p->cache[slot].loaded = p->cache[slot].model.meshCount > 0;
    platform_note_asset(full, p->cache[slot].loaded);
    if (p->cache[slot].loaded && variant >= BIOME_VARIANT && variant < BODY_VARIANT)
        recolor_biome(&p->cache[slot].model, variant - BIOME_VARIANT);
    if (p->cache[slot].loaded) p->cache[slot].anims = LoadModelAnimations(full, &p->cache[slot].anim_count);
    return slot;
}

// Entrada de cache a usar: la variante de la estacion si existe, si no el modelo base.
static int cache_slot(Props *p, const InvItem *item) {
    if (props_season_suffix(p->season)) {
        int v = load_slot(p, item, p->season);
        if (v >= 0 && p->cache[v].loaded) return v;
    }
    return load_slot(p, item, -1);
}

// Modelo del id, cargado una sola vez. NULL si todavia no fue importado.
static Model *cached_model(Props *p, const InvItem *item) {
    int slot = cache_slot(p, item);
    return slot >= 0 && p->cache[slot].loaded ? &p->cache[slot].model : NULL;
}

bool props_has_model(Props *p, const InvItem *item) { return item && cached_model(p, item) != NULL; }

// raylib muestrea los clips de un GLB cada 17 ms (GLTF_ANIMDELAY en rmodels.c): unos 59 cuadros por segundo.
#define ANIM_FPS (1000.0f / 17.0f)

// El clip del modelo con ese nombre (si no lo trae, idle; si tampoco, el primero), o NULL si no esta animado.
static ModelAnimation *anim_clip(Props *p, const InvItem *item, const char *clip, Model **model) {
    *model = cached_model(p, item);
    int slot = cache_slot(p, item);
    if (!*model || slot < 0 || p->cache[slot].anim_count <= 0) return NULL;
    ModelAnimation *anims = p->cache[slot].anims, *chosen = NULL, *idle = NULL;
    for (int i = 0; i < p->cache[slot].anim_count; i++) {
        if (clip && !strcmp(anims[i].name, clip)) chosen = &anims[i];
        if (!strcmp(anims[i].name, "idle")) idle = &anims[i];
    }
    if (!chosen) chosen = idle ? idle : &anims[0];
    return chosen->frameCount > 0 ? chosen : NULL;
}

// El cuadro a los t segundos del clip: los que se repiten, en vuelta; los de una vez, quietos en el ultimo.
static int anim_frame(const ModelAnimation *a, float t) {
    int f = (int)(fmaxf(t, 0.0f) * ANIM_FPS);
    return anim_loops(a->name) ? f % a->frameCount : (f < a->frameCount ? f : a->frameCount - 1);
}

void props_draw_item_anim(Props *p, const InvItem *item, Vector3 pos, float yaw, const char *clip, float time) {
    Model *m;
    ModelAnimation *a = anim_clip(p, item, clip, &m);
    if (a) UpdateModelAnimation(*m, *a, (int)(time * ANIM_FPS) % a->frameCount);
    props_draw_item(p, item, pos, yaw, 1.0f);
}

void props_draw_item_track(Props *p, const InvItem *item, Vector3 pos, float yaw, const char *clip, float time, float fit,
                           AnimTrack *track) {
    Model *m;
    ModelAnimation *a = anim_clip(p, item, clip, &m);
    bool again = fit > track->left + 0.05f; // otro golpe con el mismo clip (la lanza, el mismo golpe de la serie)
    track->left = fit;
    if (again || !track->clip || !clip || strcmp(track->clip, clip) != 0) {
        track->clip = clip, track->start = time, track->rate = 1.0f;
        // Un golpe entero en lo que el juego le da (0,4 s el del jugador; los clips duran de 0,5 a 2 s).
        if (a && fit > 0.0f && !anim_loops(a->name)) track->rate = Clamp((float)a->frameCount / ANIM_FPS / fit, 0.5f, 5.0f);
    }
    if (a) UpdateModelAnimation(*m, *a, anim_frame(a, (time - track->start) * track->rate));
    props_draw_item(p, item, pos, yaw, 1.0f);
}

bool props_bone_point(Props *p, const InvItem *item, Vector3 pos, float yaw, const AnimTrack *track, float time, const char *bone,
                      Vector3 *out) {
    Model *m;
    ModelAnimation *a = track && track->clip ? anim_clip(p, item, track->clip, &m) : NULL;
    if (!a) return false;
    for (int b = 0; b < m->boneCount && b < a->boneCount; b++) {
        if (strcmp(m->bones[b].name, bone) != 0) continue;
        Transform t = a->framePoses[anim_frame(a, (time - track->start) * track->rate)][b];
        Vector3 q = Vector3RotateByAxisAngle(Vector3Transform(t.translation, m->transform), (Vector3){ 0, 1, 0 }, yaw);
        *out = Vector3Add(q, pos);
        return true;
    }
    return false;
}

// La variante de la prenda para ese cuerpo (o la del comun), o -1.
static int worn_slot(Props *p, const InvItem *wear, const InvItem *body) {
    int k = strstr(body->id, "cuerpo_mujer") ? 1 : strstr(body->id, "cuerpo_gigante") ? 2 : strstr(body->id, "protagonista") ? 3 : 0;
    int slot = k ? load_slot(p, wear, BODY_VARIANT + k) : -1;
    if (slot < 0 || !p->cache[slot].loaded) slot = load_slot(p, wear, -1);
    return slot >= 0 && p->cache[slot].loaded ? slot : -1;
}

void props_draw_worn(Props *p, const InvItem *wear, const InvItem *body, Vector3 pos, float yaw, const AnimTrack *track,
                     float time) {
    if (!wear || !body || !track || !track->clip) return;
    Model *bm;
    ModelAnimation *a = anim_clip(p, body, track->clip, &bm);
    int slot = a ? worn_slot(p, wear, body) : -1;
    if (slot < 0) return;
    Model *m = &p->cache[slot].model;
    if (m->boneCount != a->boneCount || m->meshCount < 1 || !m->meshes[0].boneMatrices) return; // no es una prenda
    UpdateModelAnimation(*m, *a, anim_frame(a, (time - track->start) * track->rate));
    p->cache[slot].posed = true;
    DrawModelEx(*m, pos, (Vector3){ 0, 1, 0 }, yaw * RAD2DEG, (Vector3){ 1, 1, 1 }, g_tint);
}

bool props_is_worn(Props *p, const InvItem *item) {
    int slot = item ? cache_slot(p, item) : -1;
    return slot >= 0 && p->cache[slot].loaded && p->cache[slot].model.boneCount > 0 && p->cache[slot].anim_count == 0;
}

// Una prenda fuera del cuerpo (en el suelo, en una mano, en la galeria): de vuelta a su pose de piel, y lo que hay
// que moverla para que apoye en el origen (esta donde iria en el cuerpo: un casco, a 1,6 m).
static Vector3 worn_rest(Props *p, const InvItem *item, Model *m) {
    int slot = cache_slot(p, item);
    if (slot < 0 || m->boneCount <= 0 || p->cache[slot].anim_count > 0) return (Vector3){ 0 };
    if (p->cache[slot].posed) {
        for (int i = 0; i < m->meshCount; i++) {
            const Mesh *me = &m->meshes[i];
            if (!me->animVertices) continue;
            rlUpdateVertexBuffer(me->vboId[0], me->vertices, me->vertexCount * 3 * (int)sizeof(float), 0);
            if (me->normals) rlUpdateVertexBuffer(me->vboId[2], me->normals, me->vertexCount * 3 * (int)sizeof(float), 0);
        }
        p->cache[slot].posed = false;
    }
    BoundingBox bb = GetMeshBoundingBox(m->meshes[0]);
    for (int i = 1; i < m->meshCount; i++) {
        BoundingBox b = GetMeshBoundingBox(m->meshes[i]);
        bb.min = Vector3Min(bb.min, b.min), bb.max = Vector3Max(bb.max, b.max);
    }
    return (Vector3){ -(bb.min.x + bb.max.x) * 0.5f, -bb.min.y, -(bb.min.z + bb.max.z) * 0.5f };
}

// Color del marcador segun la categoria del inventario (igual que en la galeria).
static Color category_color(const char *cat) {
    if (!strcmp(cat, "mapa")) return (Color){ 120, 140, 80, 255 };
    if (!strcmp(cat, "animal")) return (Color){ 160, 115, 75, 255 };
    if (!strcmp(cat, "utileria")) return (Color){ 165, 135, 100, 255 };
    if (!strcmp(cat, "totem")) return (Color){ 125, 75, 45, 255 };
    return (Color){ 190, 160, 120, 255 };
}


void props_set_tint(Color tint) { g_tint = tint; }
Color props_get_tint(void) { return g_tint; }

bool props_draw_item_biome(Props *p, const InvItem *item, Matrix transform, int biome) {
    int slot = load_slot(p, item, BIOME_VARIANT + biome);
    if (slot < 0 || !p->cache[slot].loaded) return props_draw_item_matrix(p, item, transform);
    Model *m = &p->cache[slot].model;
    Matrix saved = m->transform;
    m->transform = MatrixMultiply(saved, transform);
    DrawModel(*m, (Vector3){ 0, 0, 0 }, 1.0f, g_tint);
    m->transform = saved;
    return true;
}

bool props_draw_item_matrix(Props *p, const InvItem *item, Matrix transform) {
    Model *m = cached_model(p, item);
    if (!m) return false;
    Vector3 off = worn_rest(p, item, m);
    Matrix saved = m->transform;
    m->transform = MatrixMultiply(MatrixMultiply(saved, MatrixTranslate(off.x, off.y, off.z)), transform);
    DrawModel(*m, (Vector3){ 0, 0, 0 }, 1.0f, g_tint);
    m->transform = saved;
    return true;
}

void props_draw_item(Props *p, const InvItem *item, Vector3 pos, float yaw, float grow) {
    Model *m = cached_model(p, item);
    if (m && grow >= 1.0f) {
        Vector3 off = Vector3RotateByAxisAngle(worn_rest(p, item, m), (Vector3){ 0, 1, 0 }, yaw);
        DrawModelEx(*m, Vector3Add(pos, off), (Vector3){ 0, 1, 0 }, yaw * RAD2DEG, (Vector3){ 1, 1, 1 }, g_tint);
        return;
    }
    if (!m && grow >= 1.0f && vegetation_draw_prop(item->id, item->w, item->l, pos, yaw)) return; // las matas, con sus hojas
    if (!m && grow >= 1.0f && voxs_draw_item(item, pos, yaw)) return; // las obras de la tribu, con el kit
    // Marcador: caja con las medidas del inventario (largo en X local, ancho en Z), girada con yaw.
    float h = (item->texture || item->h <= 0.0f ? 0.03f : item->h) * fmaxf(grow, 0.05f);
    Color c = grow < 1.0f ? (Color){ 150, 120, 80, 255 } : category_color(item->category);
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(yaw * RAD2DEG, 0, 1, 0);
    Vector3 size = { fmaxf(item->l, 0.05f), h, fmaxf(item->w, 0.05f) };
    Vector3 center = { 0, h * 0.5f, 0 };
    DrawCubeV(center, size, c);
    DrawCubeWiresV(center, size, ColorBrightness(c, -0.5f));
    if (grow < 1.0f) { // andamio: el volumen final, en alambre
        Vector3 full = { size.x, fmaxf(item->h, 0.05f), size.z };
        DrawCubeWiresV((Vector3){ 0, full.y * 0.5f, 0 }, full, (Color){ 90, 70, 50, 255 });
    }
    rlPopMatrix();
}

void props_draw_ghost(Props *p, const InvItem *item, Vector3 pos, float yaw, Color tint) {
    Model *m = cached_model(p, item);
    if (m) {
        Vector3 off = Vector3RotateByAxisAngle(worn_rest(p, item, m), (Vector3){ 0, 1, 0 }, yaw);
        DrawModelEx(*m, Vector3Add(pos, off), (Vector3){ 0, 1, 0 }, yaw * RAD2DEG, (Vector3){ 1, 1, 1 }, tint);
        return;
    }
    rlPushMatrix();
    rlTranslatef(pos.x, pos.y, pos.z);
    rlRotatef(yaw * RAD2DEG, 0, 1, 0);
    Vector3 size = { fmaxf(item->l, 0.05f), fmaxf(item->h, 0.05f), fmaxf(item->w, 0.05f) };
    DrawCubeV((Vector3){ 0, size.y * 0.5f, 0 }, size, tint);
    DrawCubeWiresV((Vector3){ 0, size.y * 0.5f, 0 }, size, ColorAlpha(tint, 1.0f));
    rlPopMatrix();
}

bool props_is_fire(const InvItem *item, float *scale) {
    if (!item) return false;
    bool bon = !strcmp(item->id, "estructura.campamento.hoguera");
    if (!bon && strcmp(item->id, "estructura.campamento.fogata")) return false;
    if (scale) *scale = bon ? 2.0f : 1.0f;
    return true;
}

void props_draw(Props *p) {
    for (int i = 0; i < p->count; i++) {
        const InvItem *it = p->items[i].item;
        if (props_is_fire(it, NULL) && !props_has_model(p, it)) continue; // los dibuja hearth.c
        props_draw_item(p, it, p->items[i].pos, p->items[i].yaw, 1.0f);
    }
}
