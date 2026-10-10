#include "sim/save_format.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define SF_MAX_TYPES 128
#define SF_NO_SUB 0xFFFFu
#define SF_MAX_TYPE_SIZE (64u << 20) // 64 MB: un struct mas grande es un archivo dañado

static bool g_slow;
void sf_set_slow(bool slow) { g_slow = slow; }

// ---------------------------------------------------------------- tipos de un esquema
typedef struct {
    const SfType *t[SF_MAX_TYPES];
    int n;
    bool overflow;
} TypeList;

static int type_index(const TypeList *l, const SfType *t) {
    for (int i = 0; i < l->n; i++)
        if (l->t[i] == t) return i;
    return -1;
}

// En postorden: cada struct despues de los que contiene.
static void collect(TypeList *l, const SfType *t) {
    if (type_index(l, t) >= 0) return;
    for (unsigned i = 0; i < t->nfields; i++)
        if (t->fields[i].kind == SF_STRUCT && t->fields[i].sub) collect(l, t->fields[i].sub);
    if (l->n < SF_MAX_TYPES) l->t[l->n++] = t;
    else l->overflow = true;
}

// Sin relleno ni punteros: todos sus bytes son campos del esquema (se copia de una vez).
static bool dense(const SfType *t) {
    unsigned used = 0;
    for (unsigned i = 0; i < t->nfields; i++) {
        const SfField *f = &t->fields[i];
        if (f->kind == SF_STRUCT && (!f->sub || !dense(f->sub))) return false;
        used += f->size * f->count;
    }
    return used == t->size;
}

// Copia solo los campos del esquema (dst en cero): el relleno y los punteros quedan en cero,
// asi el mismo estado da siempre los mismos bytes.
static void pack(const SfType *t, const unsigned char *src, unsigned char *dst) {
    if (dense(t)) {
        memcpy(dst, src, t->size);
        return;
    }
    for (unsigned i = 0; i < t->nfields; i++) {
        const SfField *f = &t->fields[i];
        if (f->kind == SF_STRUCT && f->sub)
            for (unsigned k = 0; k < f->count; k++) pack(f->sub, src + f->offset + (size_t)k * f->size, dst + f->offset + (size_t)k * f->size);
        else
            memcpy(dst + f->offset, src + f->offset, (size_t)f->size * f->count);
    }
}

// ---------------------------------------------------------------- escritura
// Los numeros del formato van en little-endian; los datos de los structs, como estan en memoria
// (todas las plataformas del juego son little-endian).
static void put_bytes(SfWriter *w, const void *p, size_t n) {
    if (w->ok && n && fwrite(p, 1, n, w->f) != n) w->ok = false;
}

static void put_u8(SfWriter *w, unsigned v) {
    unsigned char b = (unsigned char)v;
    put_bytes(w, &b, 1);
}

static void put_u16(SfWriter *w, unsigned v) {
    unsigned char b[2] = { (unsigned char)v, (unsigned char)(v >> 8) };
    put_bytes(w, b, 2);
}

static void put_u32(SfWriter *w, uint32_t v) {
    unsigned char b[4] = { (unsigned char)v, (unsigned char)(v >> 8), (unsigned char)(v >> 16), (unsigned char)(v >> 24) };
    put_bytes(w, b, 4);
}

static void put_str(SfWriter *w, const char *s) {
    size_t n = strlen(s);
    if (n > 255) n = 255;
    put_u8(w, (unsigned)n);
    put_bytes(w, s, n);
}

// El esquema de un bloque: sus tipos con la raiz primero (cada struct antes que los que contiene).
static void put_schema(SfWriter *w, const SfType *root) {
    TypeList l = { 0 };
    collect(&l, root);
    if (l.overflow) {
        w->ok = false;
        return;
    }
    for (int i = 0, j = l.n - 1; i < j; i++, j--) {
        const SfType *x = l.t[i];
        l.t[i] = l.t[j], l.t[j] = x;
    }
    put_u16(w, (unsigned)l.n);
    for (int i = 0; i < l.n; i++) {
        const SfType *t = l.t[i];
        put_str(w, t->name);
        put_u32(w, t->size);
        put_u16(w, t->nfields);
        for (unsigned k = 0; k < t->nfields; k++) {
            const SfField *f = &t->fields[k];
            put_str(w, f->name);
            put_str(w, f->type);
            put_u8(w, (unsigned)f->kind);
            put_u32(w, f->offset);
            put_u32(w, f->size);
            put_u32(w, f->count);
            put_u16(w, f->kind == SF_STRUCT && f->sub ? (unsigned)type_index(&l, f->sub) : SF_NO_SUB);
        }
    }
}

void sf_write_begin(SfWriter *w, FILE *f) {
    memset(w, 0, sizeof(*w));
    w->f = f;
    w->ok = f != NULL;
    w->start = -1;
}

void sf_block_begin(SfWriter *w, const char *tag, uint32_t version, const SfType *type) {
    if (w->start >= 0) sf_block_end(w);
    put_bytes(w, tag, 4);
    put_u32(w, version);
    put_u32(w, 0); // el tamaño, al cerrar
    w->start = w->ok ? ftell(w->f) : -1;
    put_schema(w, type);
    w->count_at = w->ok ? ftell(w->f) : -1;
    put_u32(w, 0); // la cantidad de elementos, al cerrar
    w->count = 0;
    w->type = type;
    free(w->buf);
    w->buf = malloc(type->size);
    if (!w->buf || w->start < 0 || w->count_at < 0) w->ok = false;
}

void sf_put(SfWriter *w, const void *elem) {
    if (!w->ok || !w->buf) return;
    memset(w->buf, 0, w->type->size);
    pack(w->type, elem, w->buf);
    put_bytes(w, w->buf, w->type->size);
    w->count++;
}

static void patch_u32(SfWriter *w, long at, uint32_t v) {
    if (w->ok && fseek(w->f, at, SEEK_SET) != 0) w->ok = false;
    put_u32(w, v);
}

void sf_block_end(SfWriter *w) {
    if (w->start < 0) return;
    long end = w->ok ? ftell(w->f) : -1;
    if (end < w->start) w->ok = false;
    patch_u32(w, w->start - 4, (uint32_t)(end - w->start));
    patch_u32(w, w->count_at, w->count);
    if (w->ok && fseek(w->f, end, SEEK_SET) != 0) w->ok = false;
    free(w->buf);
    w->buf = NULL;
    w->start = -1;
}

void sf_block(SfWriter *w, const char *tag, uint32_t version, const SfType *type, const void *elems, int n) {
    sf_block_begin(w, tag, version, type);
    for (int i = 0; i < n; i++) sf_put(w, (const unsigned char *)elems + (size_t)i * type->size);
    sf_block_end(w);
}

bool sf_write_end(SfWriter *w) {
    if (w->start >= 0) sf_block_end(w);
    put_bytes(w, "FIN.", 4);
    put_u32(w, 0);
    put_u32(w, 0);
    free(w->buf);
    w->buf = NULL;
    return w->ok;
}

// ---------------------------------------------------------------- lectura
static uint32_t rd32(const unsigned char *p) {
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

bool sf_parse(SfFile *file, const void *buf, size_t len) {
    const unsigned char *p = buf, *end = p + len;
    file->count = 0;
    while (p && (size_t)(end - p) >= 12) {
        SfBlock b;
        memcpy(b.tag, p, 4);
        b.tag[4] = '\0';
        b.version = rd32(p + 4);
        b.size = rd32(p + 8);
        p += 12;
        if (b.size > (size_t)(end - p)) return false; // cortado
        b.data = p;
        p += b.size;
        if (!memcmp(b.tag, "FIN.", 4)) return p == end;
        if (file->count >= SF_MAX_BLOCKS) return false;
        file->blocks[file->count++] = b;
    }
    return false; // sin la marca de fin: el archivo quedo a medias
}

const SfBlock *sf_find(const SfFile *file, const char *tag) {
    for (int i = 0; i < file->count; i++)
        if (!memcmp(file->blocks[i].tag, tag, 4)) return &file->blocks[i];
    return NULL;
}

typedef struct {
    const unsigned char *p, *end;
    bool ok;
} Cursor;

static unsigned get_u8(Cursor *c) {
    if (!c->ok || c->end - c->p < 1) return c->ok = false, 0;
    return *c->p++;
}

static unsigned get_u16(Cursor *c) {
    if (!c->ok || c->end - c->p < 2) return c->ok = false, 0;
    unsigned v = (unsigned)c->p[0] | (unsigned)c->p[1] << 8;
    c->p += 2;
    return v;
}

static uint32_t get_u32(Cursor *c) {
    if (!c->ok || c->end - c->p < 4) return c->ok = false, 0;
    uint32_t v = rd32(c->p);
    c->p += 4;
    return v;
}

static const unsigned char *get_str(Cursor *c, unsigned *len) {
    *len = get_u8(c);
    if (!c->ok || (size_t)(c->end - c->p) < *len) return c->ok = false, NULL;
    const unsigned char *s = c->p;
    c->p += *len;
    return s;
}

// Un esquema leido de un bloque: sus tipos, campos y nombres.
typedef struct {
    SfType *types;
    SfField *fields;
    char *names;
    int ntypes;
} Schema;

static void schema_free(Schema *s) {
    free(s->types);
    free(s->fields);
    free(s->names);
    memset(s, 0, sizeof(*s));
}

static const char *take_str(Cursor *c, char **out) {
    unsigned len;
    const unsigned char *s = get_str(c, &len);
    if (!s) return "";
    char *r = *out;
    memcpy(r, s, len);
    r[len] = '\0';
    *out += len + 1;
    return r;
}

static bool size_ok(unsigned kind, unsigned size) {
    switch (kind) {
    case SF_INT:
    case SF_UINT: return size == 1 || size == 2 || size == 4 || size == 8;
    case SF_FLOAT: return size == 4 || size == 8;
    case SF_BOOL:
    case SF_TEXT: return size == 1;
    case SF_BYTES:
    case SF_STRUCT: return size >= 1;
    default: return false;
    }
}

// Un campo coherente: dentro de su struct, y si es un struct, uno que viene despues en la lista
// (asi no hay ciclos) y del tamaño que dice.
static bool field_ok(const Schema *s, unsigned self, const SfField *f, unsigned kind, unsigned sub) {
    const SfType *t = &s->types[self];
    if (!size_ok(kind, f->size) || f->count < 1) return false;
    if ((uint64_t)f->offset + (uint64_t)f->size * f->count > t->size) return false;
    if (kind != SF_STRUCT) return sub == SF_NO_SUB;
    return sub > self && sub < (unsigned)s->ntypes && s->types[sub].size == f->size;
}

static bool parse_schema(Cursor *c, Schema *s) {
    memset(s, 0, sizeof(*s));
    // Primero se mide (cuantos campos y letras), despues se llena.
    Cursor scan = *c;
    unsigned ntypes = get_u16(&scan), len;
    if (!scan.ok || ntypes < 1 || ntypes > SF_MAX_TYPES) return false;
    size_t nfields = 0, chars = 0;
    for (unsigned i = 0; i < ntypes && scan.ok; i++) {
        get_str(&scan, &len), chars += len + 1;
        get_u32(&scan);
        unsigned nf = get_u16(&scan);
        nfields += nf;
        for (unsigned k = 0; k < nf && scan.ok; k++) {
            get_str(&scan, &len), chars += len + 1;
            get_str(&scan, &len), chars += len + 1;
            get_u8(&scan), get_u32(&scan), get_u32(&scan), get_u32(&scan), get_u16(&scan);
        }
    }
    if (!scan.ok) return false;
    s->types = calloc(ntypes, sizeof(SfType));
    s->fields = calloc(nfields ? nfields : 1, sizeof(SfField));
    s->names = malloc(chars);
    unsigned *kinds = malloc((nfields ? nfields : 1) * 2 * sizeof(unsigned)); // tipo de campo y struct contenido
    if (!s->types || !s->fields || !s->names || !kinds) {
        free(kinds);
        schema_free(s);
        return false;
    }
    s->ntypes = (int)ntypes;
    char *out = s->names;
    size_t fi = 0;
    get_u16(c);
    for (unsigned i = 0; i < ntypes; i++) {
        SfType *t = &s->types[i];
        t->name = take_str(c, &out);
        t->size = get_u32(c);
        t->nfields = get_u16(c);
        t->fields = &s->fields[fi];
        for (unsigned k = 0; k < t->nfields; k++, fi++) {
            SfField *f = &s->fields[fi];
            f->name = take_str(c, &out);
            f->type = take_str(c, &out);
            kinds[2 * fi] = get_u8(c);
            f->kind = kinds[2 * fi] < SF_KIND_COUNT ? (SfKind)kinds[2 * fi] : SF_BYTES;
            f->offset = get_u32(c);
            f->size = get_u32(c);
            f->count = get_u32(c);
            kinds[2 * fi + 1] = get_u16(c);
        }
    }
    bool ok = c->ok;
    fi = 0;
    for (unsigned i = 0; ok && i < ntypes; i++) {
        SfType *t = &s->types[i];
        ok = t->size >= 1 && t->size <= SF_MAX_TYPE_SIZE;
        for (unsigned k = 0; ok && k < t->nfields; k++, fi++) {
            SfField *f = &s->fields[fi];
            ok = field_ok(s, i, f, kinds[2 * fi], kinds[2 * fi + 1]);
            if (ok && f->kind == SF_STRUCT) f->sub = &s->types[kinds[2 * fi + 1]];
        }
    }
    free(kinds);
    if (!ok) schema_free(s);
    return ok;
}

// El esquema y los datos de un bloque (count elementos de la raiz, exactos).
static bool open_block(const SfBlock *b, Schema *s, const unsigned char **data, uint32_t *count) {
    Cursor c = { b->data, b->data + b->size, true };
    if (!parse_schema(&c, s)) return false;
    *count = get_u32(&c);
    size_t left = (size_t)(c.end - c.p);
    if (!c.ok || *count > INT_MAX || (uint64_t)*count * s->types[0].size != left) {
        schema_free(s);
        return false;
    }
    *data = c.p;
    return true;
}

int sf_count(const SfBlock *b) {
    Schema s;
    const unsigned char *data;
    uint32_t count;
    if (!b || !open_block(b, &s, &data, &count)) return -1;
    schema_free(&s);
    return (int)count;
}

int sf_read(const SfBlock *b, const SfType *live, void *dst, int max) {
    Schema s;
    const unsigned char *data;
    uint32_t count;
    if (!b || !open_block(b, &s, &data, &count)) return -1;
    int n = (int)count < max ? (int)count : max;
    if (n > 0) sf_convert(&s.types[0], data, n, live, dst);
    schema_free(&s);
    return (int)count;
}

// ---------------------------------------------------------------- conversion
bool sf_same(const SfType *a, const SfType *b) {
    if (a == b) return true;
    if (!a || !b || a->size != b->size || a->nfields != b->nfields) return false;
    for (unsigned i = 0; i < a->nfields; i++) {
        const SfField *x = &a->fields[i], *y = &b->fields[i];
        if (x->kind != y->kind || x->offset != y->offset || x->size != y->size || x->count != y->count || strcmp(x->name, y->name))
            return false;
        if (x->kind == SF_STRUCT ? !sf_same(x->sub, y->sub) : strcmp(x->type, y->type) != 0) return false;
    }
    return true;
}

static const SfField *find_field(const SfType *t, const char *name) {
    for (unsigned i = 0; i < t->nfields; i++)
        if (!strcmp(t->fields[i].name, name)) return &t->fields[i];
    return NULL;
}

// Un numero cualquiera (entero, float o bool), para pasar de un tipo a otro.
typedef struct {
    bool real;
    int64_t i;
    double f;
} Num;

static Num get_num(const unsigned char *p, const SfField *f) {
    Num v = { false, 0, 0.0 };
    if (f->kind == SF_FLOAT) {
        v.real = true;
        if (f->size == 4) {
            float x;
            memcpy(&x, p, 4);
            v.f = x;
        } else {
            memcpy(&v.f, p, 8);
        }
    } else if (f->kind == SF_BOOL) {
        v.i = p[0] != 0;
    } else if (f->kind == SF_INT) {
        int8_t a;
        int16_t b;
        int32_t c;
        switch (f->size) {
        case 1: memcpy(&a, p, 1), v.i = a; break;
        case 2: memcpy(&b, p, 2), v.i = b; break;
        case 4: memcpy(&c, p, 4), v.i = c; break;
        default: memcpy(&v.i, p, 8); break;
        }
    } else {
        uint16_t b;
        uint32_t c;
        uint64_t d;
        switch (f->size) {
        case 1: v.i = p[0]; break;
        case 2: memcpy(&b, p, 2), v.i = b; break;
        case 4: memcpy(&c, p, 4), v.i = c; break;
        default: memcpy(&d, p, 8), v.i = d > (uint64_t)INT64_MAX ? INT64_MAX : (int64_t)d; break;
        }
    }
    return v;
}

// Al tipo del destino: los enteros se recortan a su rango y los float se truncan.
static void put_num(unsigned char *p, const SfField *f, Num v) {
    if (f->kind == SF_FLOAT) {
        double x = v.real ? v.f : (double)v.i;
        if (f->size == 4) {
            float y = (float)x;
            memcpy(p, &y, 4);
        } else {
            memcpy(p, &x, 8);
        }
        return;
    }
    if (f->kind == SF_BOOL) {
        p[0] = (unsigned char)(v.real ? v.f != 0.0 : v.i != 0);
        return;
    }
    int64_t x = v.i;
    if (v.real) x = isnan(v.f) ? 0 : v.f >= 9.2e18 ? INT64_MAX : v.f <= -9.2e18 ? INT64_MIN : (int64_t)v.f;
    if (f->kind == SF_INT) {
        int64_t lo = f->size == 1 ? INT8_MIN : f->size == 2 ? INT16_MIN : f->size == 4 ? INT32_MIN : INT64_MIN;
        int64_t hi = f->size == 1 ? INT8_MAX : f->size == 2 ? INT16_MAX : f->size == 4 ? INT32_MAX : INT64_MAX;
        x = x < lo ? lo : x > hi ? hi : x;
        int8_t a = (int8_t)x;
        int16_t b = (int16_t)x;
        int32_t c = (int32_t)x;
        switch (f->size) {
        case 1: memcpy(p, &a, 1); break;
        case 2: memcpy(p, &b, 2); break;
        case 4: memcpy(p, &c, 4); break;
        default: memcpy(p, &x, 8); break;
        }
    } else {
        uint64_t hi = f->size == 1 ? UINT8_MAX : f->size == 2 ? UINT16_MAX : f->size == 4 ? UINT32_MAX : UINT64_MAX;
        uint64_t y = x < 0 ? 0 : (uint64_t)x > hi ? hi : (uint64_t)x;
        uint8_t a = (uint8_t)y;
        uint16_t b = (uint16_t)y;
        uint32_t c = (uint32_t)y;
        switch (f->size) {
        case 1: memcpy(p, &a, 1); break;
        case 2: memcpy(p, &b, 2); break;
        case 4: memcpy(p, &c, 4); break;
        default: memcpy(p, &y, 8); break;
        }
    }
}

static void convert_elem(const SfType *old, const unsigned char *src, const SfType *live, unsigned char *dst);

static void convert_field(const SfField *of, const unsigned char *src, const SfField *lf, unsigned char *dst) {
    unsigned n = of->count < lf->count ? of->count : lf->count;
    if (of->kind == SF_STRUCT || lf->kind == SF_STRUCT) {
        if (of->kind != lf->kind || !of->sub || !lf->sub) return;
        if (!g_slow && of->size == lf->size && sf_same(of->sub, lf->sub)) {
            memcpy(dst, src, (size_t)n * lf->size);
            return;
        }
        for (unsigned k = 0; k < n; k++) convert_elem(of->sub, src + (size_t)k * of->size, lf->sub, dst + (size_t)k * lf->size);
        return;
    }
    if (of->kind == SF_TEXT || lf->kind == SF_TEXT) {
        if (of->kind != lf->kind) return;
        memcpy(dst, src, n);
        if (of->count > lf->count) dst[lf->count - 1] = '\0'; // recortado: sigue terminando en cero
        return;
    }
    if (of->kind == SF_BYTES || lf->kind == SF_BYTES) {
        if (of->kind == lf->kind && of->size == lf->size && !strcmp(of->type, lf->type)) memcpy(dst, src, (size_t)n * lf->size);
        return;
    }
    if (of->kind == lf->kind && of->size == lf->size) {
        memcpy(dst, src, (size_t)n * lf->size);
        return;
    }
    for (unsigned k = 0; k < n; k++) put_num(dst + (size_t)k * lf->size, lf, get_num(src + (size_t)k * of->size, of));
}

static void convert_elem(const SfType *old, const unsigned char *src, const SfType *live, unsigned char *dst) {
    for (unsigned i = 0; i < live->nfields; i++) {
        const SfField *lf = &live->fields[i];
        const SfField *of = find_field(old, lf->name);
        if (of) convert_field(of, src + of->offset, lf, dst + lf->offset);
    }
}

void sf_convert(const SfType *old, const void *src, int n, const SfType *live, void *dst) {
    const unsigned char *s = src;
    unsigned char *d = dst;
    if (n <= 0) return;
    if (!g_slow && sf_same(old, live)) {
        memcpy(d, s, (size_t)n * live->size);
        return;
    }
    for (int i = 0; i < n; i++) convert_elem(old, s + (size_t)i * old->size, live, d + (size_t)i * live->size);
}

// ---------------------------------------------------------------- esquemas congelados
void sf_dump_c(FILE *out, const SfType *const *roots, int n, const char *prefix) {
    static const char *KIND[SF_KIND_COUNT] = { "SF_INT", "SF_UINT", "SF_FLOAT", "SF_BOOL", "SF_TEXT", "SF_BYTES", "SF_STRUCT" };
    TypeList l = { 0 };
    for (int i = 0; i < n; i++) collect(&l, roots[i]); // los contenidos primero: el C los necesita definidos antes
    for (int i = 0; i < l.n; i++) {
        const SfType *t = l.t[i];
        fprintf(out, "static const SfField %s_F_%s[] = {\n", prefix, t->name);
        for (unsigned k = 0; k < t->nfields; k++) {
            const SfField *f = &t->fields[k];
            fprintf(out, "    { \"%s\", \"%s\", %s, %u, %u, %u, ", f->name, f->type, KIND[f->kind], f->offset, f->size, f->count);
            if (f->kind == SF_STRUCT && f->sub) fprintf(out, "&%s_T_%s },\n", prefix, f->sub->name);
            else fprintf(out, "NULL },\n");
        }
        fprintf(out, "};\nstatic const SfType %s_T_%s = { \"%s\", %u, %u, %s_F_%s };\n\n", prefix, t->name, t->name, t->size, t->nfields,
                prefix, t->name);
    }
}
