#include "lang.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *key, *val;
} Entry;

typedef struct {
    Entry *e;
    int cap, n;
} Table;

static Table g_tab[LANG_COUNT];
static Lang g_lang = LANG_ES;
static int g_missing;

static uint32_t hash(const char *s) {
    uint32_t h = 2166136261u;
    for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
    return h;
}

static void table_free(Table *t) {
    for (int i = 0; i < t->cap; i++) free(t->e[i].key), free(t->e[i].val);
    free(t->e);
    memset(t, 0, sizeof(*t));
}

static void table_put(Table *t, char *key, char *val) {
    if ((t->n + 1) * 2 > t->cap) { // crece al 50 % de ocupacion
        Table bigger = { calloc((size_t)(t->cap ? t->cap * 2 : 512), sizeof(Entry)), t->cap ? t->cap * 2 : 512, 0 };
        for (int i = 0; i < t->cap; i++)
            if (t->e[i].key) table_put(&bigger, t->e[i].key, t->e[i].val);
        free(t->e);
        *t = bigger;
    }
    uint32_t i = hash(key) & (uint32_t)(t->cap - 1);
    while (t->e[i].key) {
        if (!strcmp(t->e[i].key, key)) { // repetida: gana la ultima
            free(t->e[i].val), free(key);
            t->e[i].val = val;
            return;
        }
        i = (i + 1) & (uint32_t)(t->cap - 1);
    }
    t->e[i].key = key, t->e[i].val = val;
    t->n++;
}

// Copia un campo deshaciendo \n, \t y \\.
static char *unescape(const char *s, size_t len) {
    char *out = malloc(len + 1), *o = out;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '\\' && i + 1 < len) {
            char c = s[++i];
            *o++ = c == 'n' ? '\n' : c == 't' ? '\t' : c;
        } else {
            *o++ = s[i];
        }
    }
    *o = '\0';
    return out;
}

int lang_load(Lang l, const char *text) {
    if (l <= LANG_ES || l >= LANG_COUNT || !text) return 0;
    table_free(&g_tab[l]);
    int n = 0;
    for (const char *line = text; *line;) {
        const char *end = strchr(line, '\n');
        size_t len = end ? (size_t)(end - line) : strlen(line);
        if (len && line[len - 1] == '\r') len--;
        const char *tab = memchr(line, '\t', len);
        if (line[0] != '#' && tab && tab > line && (size_t)(tab - line) + 1 < len) {
            const char *val = tab + 1;
            size_t vlen = len - (size_t)(val - line);
            const char *tab2 = memchr(val, '\t', vlen); // columnas extra (notas): se ignoran
            if (tab2) vlen = (size_t)(tab2 - val);
            if (vlen) table_put(&g_tab[l], unescape(line, (size_t)(tab - line)), unescape(val, vlen)), n++;
        }
        if (!end) break;
        line = end + 1;
    }
    return n;
}

void lang_free(void) {
    for (int l = 0; l < LANG_COUNT; l++) table_free(&g_tab[l]);
}

void lang_set(Lang l) { g_lang = l >= 0 && l < LANG_COUNT ? l : LANG_ES; }
Lang lang_get(void) { return g_lang; }
const char *lang_name(Lang l) { return l == LANG_EN ? "English" : "Español"; }
const char *lang_code(Lang l) { return l == LANG_EN ? "en" : "es"; }
int lang_missing(void) { return g_missing; }

const char *tr(const char *es) {
    if (!es || g_lang == LANG_ES || !es[0]) return es;
    const Table *t = &g_tab[g_lang];
    if (!t->cap) return es;
    uint32_t i = hash(es) & (uint32_t)(t->cap - 1);
    while (t->e[i].key) {
        if (!strcmp(t->e[i].key, es)) return t->e[i].val;
        i = (i + 1) & (uint32_t)(t->cap - 1);
    }
    g_missing++;
    return es;
}
