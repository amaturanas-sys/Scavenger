#include "sim/inventory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *STATE_NAMES[INV_ESTADO_COUNT] = { "pendiente", "kiln", "importado", "refinado" };

const char *inventory_state_name(InvState s) { return s >= 0 && s < INV_ESTADO_COUNT ? STATE_NAMES[s] : "?"; }

// Copia el campo [start, end) en dst (truncando) y devuelve el inicio del siguiente.
static const char *field(const char *p, const char *eol, char *dst, size_t len) {
    const char *end = p;
    while (end < eol && *end != '\t') end++;
    size_t n = (size_t)(end - p);
    if (n >= len) n = len - 1;
    memcpy(dst, p, n);
    dst[n] = '\0';
    return end < eol ? end + 1 : eol;
}

static bool valid_id(const char *id) {
    int dots = 0;
    if (!id[0]) return false;
    for (const char *c = id; *c; c++) {
        if (*c == '.') {
            if (c == id || c[1] == '\0' || c[1] == '.') return false;
            dots++;
        } else if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_')) {
            return false;
        }
    }
    return dots == 2;
}

// "ancho x alto x largo" en metros. Se separa por 'x' antes de convertir:
// sscanf("%f") o strtof sobre el texto completo leerian "0x4" como hexadecimal.
static bool parse_dims(const char *s, float *w, float *h, float *l) {
    float v[3];
    for (int i = 0; i < 3; i++) {
        char num[24];
        size_t n = strcspn(s, "x");
        if (n == 0 || n >= sizeof(num) || (i < 2) != (s[n] == 'x')) return false;
        memcpy(num, s, n);
        num[n] = '\0';
        char *end;
        v[i] = strtof(num, &end);
        if (*end) return false;
        s += n + (i < 2);
    }
    *w = v[0];
    *h = v[1];
    *l = v[2];
    return true;
}

static bool parse_line(const char *p, const char *eol, InvItem *it) {
    char id[INV_ID_LEN], name[96], tags[512], dims[48], tris[16], state[24];
    p = field(p, eol, id, sizeof(id));
    p = field(p, eol, name, sizeof(name));
    p = field(p, eol, tags, sizeof(tags));
    p = field(p, eol, dims, sizeof(dims));
    p = field(p, eol, tris, sizeof(tris));
    field(p, eol, state, sizeof(state));
    if (!valid_id(id)) return false;

    memset(it, 0, sizeof(*it));
    snprintf(it->id, sizeof(it->id), "%s", id);
    snprintf(it->name, sizeof(it->name), "%s", name);
    size_t cat = strcspn(id, ".");
    if (cat >= sizeof(it->category)) cat = sizeof(it->category) - 1;
    memcpy(it->category, id, cat);
    it->category[cat] = '\0';
    if (!parse_dims(dims, &it->w, &it->h, &it->l)) it->w = it->h = it->l = 1.0f;
    it->tris_max = atoi(tris);
    it->state = INV_PENDIENTE;
    for (int s = 0; s < INV_ESTADO_COUNT; s++)
        if (!strcmp(state, STATE_NAMES[s])) it->state = (InvState)s;
    it->texture = strstr(tags, "formato:textura") != NULL;
    return true;
}

int inventory_parse(Inventory *inv, const char *text) {
    inv->items = NULL;
    inv->count = 0;
    int cap = 0;
    for (const char *p = text; p && *p;) {
        const char *eol = strchr(p, '\n');
        if (!eol) eol = p + strlen(p);
        const char *end = eol;
        if (end > p && end[-1] == '\r') end--; // archivos con CRLF (Windows)
        if (*p != '#' && end > p) {
            InvItem it;
            if (parse_line(p, end, &it)) { // la cabecera ("id") no pasa valid_id
                if (inv->count == cap) {
                    cap = cap ? cap * 2 : 128;
                    InvItem *grown = realloc(inv->items, (size_t)cap * sizeof(*grown));
                    if (!grown) break;
                    inv->items = grown;
                }
                inv->items[inv->count++] = it;
            }
        }
        p = *eol ? eol + 1 : NULL;
    }
    return inv->count;
}

void inventory_free(Inventory *inv) {
    free(inv->items);
    inv->items = NULL;
    inv->count = 0;
}

const InvItem *inventory_find(const Inventory *inv, const char *id) {
    for (int i = 0; i < inv->count; i++)
        if (!strcmp(inv->items[i].id, id)) return &inv->items[i];
    return NULL;
}

int inventory_path(const InvItem *item, char *buf, size_t len) {
    char rel[INV_ID_LEN];
    snprintf(rel, sizeof(rel), "%s", item->id);
    for (char *c = rel; *c; c++)
        if (*c == '.') *c = '/';
    return snprintf(buf, len, item->texture ? "assets/textures/%s.png" : "assets/models/%s.glb", rel);
}
