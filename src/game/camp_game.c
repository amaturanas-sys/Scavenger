#include "camp_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/inventory_game.h"
#include "game/talents_game.h"
#include "raylib.h"
#include "sim/apparel.h"
#include "sim/clock.h"
#include "sim/lang.h"
#include "sim/world.h"
#include "ui/icons.h"

#define TALK_RANGE 3.0f
#define CART_ID "vehiculo.tierra.carreta_bueyes"
#define BURN_SECONDS 8.0f
#define LEAVE_SECONDS 14.0f  // s que se ve a los que salen alejarse (como los viajes)
#define LEAVE_SPEED 1.6f     // m/s
#define HERD_AWAY 28.0f      // m del fuego hasta donde pasta el ganado
#define PEOPLE_PAGE (DLG_OPTIONS - 4) // y dar una tarea, mas gente, informes y volver
#define FAT_ID "utileria.material.grasa"

typedef enum {
    CG_ROOT = 100,
    CG_BUILD,
    CG_ESCORT,
    CG_RECRUIT, // ya no se usa (reclutar es una tarea de la ventana de pobladores); queda por la numeracion
    CG_TRAIN_WHO,
    CG_TRAIN_ROLE,
    CG_TRAIN_CONFIRM,
    CG_STATUS,
    CG_DISSOLVE,
    CG_FOUND,
    CG_PEOPLE, // la ventana de pobladores: quien hace que
    CG_TASK,   // la tarea para los elegidos
    CG_NEWS,   // los ultimos informes de los que volvieron
} CampTalk;

static const Role TRAINABLE[] = { ROLE_SOLDIER, ROLE_SMITH, ROLE_DRUID, ROLE_GOLDSMITH, ROLE_HERDER, ROLE_HUNTER, ROLE_SCOUT };
#define N_TRAINABLE ((int)(sizeof(TRAINABLE) / sizeof(TRAINABLE[0])))

static float dist_xz(Vector3 a, float x, float z) { return sqrtf((a.x - x) * (a.x - x) + (a.z - z) * (a.z - z)); }

static const char *item_name(const GameActions *ga, const char *id) {
    const InvItem *it = inventory_find(ga->inv, id);
    return it ? T(it->name) : id;
}

static IconId role_icon(Role r) {
    switch (r) {
    case ROLE_SOLDIER: return ICON_SOLDADO;
    case ROLE_SMITH: return ICON_FABRICAR;
    case ROLE_DRUID: return ICON_DRUIDA;
    case ROLE_GOLDSMITH: return ICON_ORFEBRE;
    case ROLE_HERDER: return ICON_PASTOR;
    case ROLE_HUNTER: return ICON_CAZADOR;
    case ROLE_SCOUT: return ICON_EXPLORADOR;
    case ROLE_HEALER: return ICON_HIERBAS;
    case ROLE_COOK: return ICON_CARNE;
    case ROLE_LIEUTENANT: return ICON_GUARDIAN;
    case ROLE_BUILDER: return ICON_OBRAS;
    case ROLE_GUARD: return ICON_ESCUDO;
    default: return ICON_PERSONA;
    }
}

static IconId build_icon(BuildId b) {
    static const IconId I[BUILD_COUNT] = { ICON_REFUGIO, ICON_OBRAS, ICON_MURO_PIEDRA, ICON_HOGUERA, ICON_TOTEM,
                                           ICON_HORNO,   ICON_FUNDICION, ICON_FUNDICION, ICON_TORRE, ICON_CORRAL };
    return b >= 0 && b < BUILD_COUNT ? I[b] : ICON_OBRAS;
}

const char *cg_here_name(const GameActions *ga) { return ga->here >= 0 && ga->camps[ga->here].used ? ga->camps[ga->here].name : ""; }

// El campamento del que es guardian ese integrante, o -1.
static int camp_of_guardian(const GameActions *ga, int member) {
    for (int k = 0; k < CAMPS_MAX; k++)
        if (ga->camps[k].used && ga->camps[k].guardian == member) return k;
    return -1;
}

static int member_index(const Troop *troop, int id) {
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
        if (troop->members[i].id == id) return i;
    return -1;
}

bool cg_try_talk(GameActions *ga, const Troop *troop, const Player *p) {
    int best = -1;
    float bd = TALK_RANGE;
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
        const Member *m = &troop->members[i];
        if (m->status != STATUS_ACTIVE || m->health.down || ga->npcs[i].member_id != m->id) continue;
        if (camp_of_guardian(ga, m->id) < 0) continue;
        float d = dist_xz(ga->npcs[i].pos, p->pos.x, p->pos.z);
        if (d < bd) bd = d, best = i;
    }
    if (best < 0) return false;
    ga->talk_member = troop->members[best].id;
    ga->talk_mode = CG_ROOT;
    ga->dlg.cursor = 0;
    ga->dlg.open = true;
    return true;
}

void cg_basic_structure(GameActions *ga, float x, float z) {
    if (!camp_far_enough(ga->camps, CAMPS_MAX, x, z)) return;
    ga->found_pending = true;
    ga->found_x = x, ga->found_z = z;
}

// ¿Quien esta libre en el campamento k? (activo, de ese campamento, sin escolta, sin tarea, sin obra)
static bool idle_in(const GameActions *ga, const Troop *troop, int i, int k) {
    const Member *m = &troop->members[i];
    const Npc *n = &ga->npcs[i];
    return m->status == STATUS_ACTIVE && !m->health.down && m->camp == k && !n->escort && n->project < 0 && n->job < 0 &&
           !camp_member_busy(&ga->camps[k], m->id) && n->member_id == m->id;
}

static void set_mode(GameActions *ga, int mode) {
    ga->talk_mode = mode;
    ga->dlg.cursor = 0;
}

// Lo que cuesta, para people personas: lo que hay / lo que hace falta.
static const char *cost_text_n(GameActions *ga, const Stockpile *st, const Ingredient *mats, int people) {
    static char buf[160];
    int n = 0;
    buf[0] = '\0';
    for (const Ingredient *m = mats; m->id && n < (int)sizeof(buf); m++)
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s%s %d/%d", n ? ", " : "", item_name(ga, m->id), stock_count(st, m->id),
                      m->count * people);
    return n ? buf : T("nada");
}

static const char *cost_text(GameActions *ga, const Stockpile *st, const Ingredient *mats) { return cost_text_n(ga, st, mats, 1); }

static const char *RECRUIT_NAMES[] = { "Arslan", "Batu", "Chagatai", "Dorji", "Erdene", "Gansukh", "Khulan", "Mönkh",
                                       "Naran", "Oyun",   "Sarnai", "Tolui",   "Tumen", "Yesui",   "Zaya",  "Bolor" };

// ---------------------------------------------------------------- la tribu: quien hace que
static const char *task_doing(CampTaskKind k) {
    switch (k) {
    case TASK_RECRUIT: return T("buscando reclutas");
    case TASK_TRAIN: return T("aprendiendo un oficio");
    case TASK_SCOUT_RESOURCES: return T("explorando");
    case TASK_HERD: return T("pastoreando");
    case TASK_HUNT: return T("cazando");
    case TASK_GUARD: return T("de guardia");
    default: return "";
    }
}

static IconId task_icon(CampTaskKind k) {
    switch (k) {
    case TASK_RECRUIT: return ICON_RECLUTAR;
    case TASK_TRAIN: return ICON_ENTRENAR;
    case TASK_SCOUT_RESOURCES: return ICON_EXPLORADOR;
    case TASK_HERD: return ICON_PASTOR;
    case TASK_HUNT: return ICON_CAZADOR;
    case TASK_GUARD: return ICON_ESCUDO;
    default: return ICON_PERSONA;
    }
}

// Lo que esta haciendo ahora (para la lista de pobladores).
static const char *member_doing(const GameActions *ga, const Troop *troop, int i, int k) {
    const Member *m = &troop->members[i];
    const Npc *n = &ga->npcs[i];
    const CampTask *t = camp_task_of(&ga->camps[k], m->id);
    if (t) {
        int pct = (int)(100.0f * t->done / fmaxf(1.0f, t->work));
        if (t->kind == TASK_TRAIN) return TextFormat(T("aprende %s (%d%%)"), T(role_name(t->role)), pct);
        return TextFormat("%s (%d%%)", task_doing(t->kind), pct);
    }
    if (m->health.down) return T("malherido");
    if (n->escort) return T("te escolta");
    if (n->project >= 0) return T("en una obra");
    if (n->job >= 0) return T("trabajando");
    if (n->guarding) return T("hace la ronda");
    return T("sin tarea");
}

static int npicked(const GameActions *ga) { return (ga->people_pick[0] > 0) + (ga->people_pick[1] > 0); }
static bool is_picked(const GameActions *ga, int id) { return id > 0 && (ga->people_pick[0] == id || ga->people_pick[1] == id); }

// Elegir o soltar a alguien: hasta dos (el tercero reemplaza al segundo).
static void toggle_pick(GameActions *ga, int id) {
    if (ga->people_pick[0] == id) {
        ga->people_pick[0] = ga->people_pick[1], ga->people_pick[1] = 0;
    } else if (ga->people_pick[1] == id) {
        ga->people_pick[1] = 0;
    } else if (!ga->people_pick[0]) {
        ga->people_pick[0] = id;
    } else {
        ga->people_pick[1] = id;
    }
}

// "Batu" o "Batu y Khulan".
static const char *names_of(const Troop *troop, const int *ids, int n) {
    static char buf[2 * NAME_LEN + 8];
    buf[0] = '\0';
    for (int i = 0, k = 0; i < n; i++) {
        int mi = member_index(troop, ids[i]);
        if (mi < 0) continue;
        size_t l = strlen(buf);
        snprintf(buf + l, sizeof(buf) - l, "%s%s", k++ ? T(" y ") : "", troop->members[mi].name);
    }
    return buf;
}

// Copia sin cortar un caracter UTF-8 por la mitad.
static void copy_utf8(char *dst, size_t len, const char *src) {
    if (!len) return;
    size_t n = strlen(src);
    if (n >= len) {
        n = len - 1;
        while (n > 0 && ((unsigned char)src[n] & 0xC0) == 0x80) n--;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

// El informe de quien vuelve: completo a los informes del campamento (la ventana) y, en corto, al
// registro (brief; vacio: el mismo).
static void report(GameActions *ga, int k, const char *text, const char *brief, char *log, size_t len) {
    for (int i = GA_NEWS - 1; i > 0; i--) memcpy(ga->news[k].line[i], ga->news[k].line[i - 1], sizeof(ga->news[k].line[i]));
    copy_utf8(ga->news[k].line[0], sizeof(ga->news[k].line[0]), text);
    copy_utf8(log, len, brief && brief[0] ? brief : text);
}

// Los animales de la tribu que salen a pastar con el campamento k: herbivoros domados, sueltos
// (ni montados ni con el jugador) y en el campamento.
static bool herdable(const GameActions *ga, int i, const CampSite *c) {
    const Animal *a = &ga->animals[i];
    return animal_domestic(a) && !species_eats_meat(a->species) && !a->ridden && i != ga->mounted &&
           sqrtf((a->x - c->x) * (a->x - c->x) + (a->z - c->z) * (a->z - c->z)) < CAMP_RADIUS_M + 30.0f;
}

static int herd_size(const GameActions *ga, const CampSite *c) {
    int n = 0;
    for (int i = 0; i < ga->animal_count && i < GA_MAX_ANIMALS; i++) n += herdable(ga, i, c);
    return n;
}

// Cuantos de los que van saben el oficio de la tarea.
static int skilled_of(const Troop *troop, const int *ids, int n, CampTaskKind kind) {
    int s = 0;
    for (int i = 0; i < n; i++) {
        int mi = member_index(troop, ids[i]);
        s += mi >= 0 && role_suits(kind, troop->members[mi].role);
    }
    return s;
}

// ---------------------------------------------------------------- dialogo
static void build_dialog(GameActions *ga, const Troop *troop, const Props *props, const Terrain *t, int day, float now) {
    Dialog *d = &ga->dlg;
    int k = ga->talk_mode == CG_FOUND ? -1 : camp_of_guardian(ga, ga->talk_member);
    const Member *g = NULL;
    int gi = member_index(troop, ga->talk_member);
    if (gi >= 0) g = &troop->members[gi];
    CampSite *c = k >= 0 ? &ga->camps[k] : NULL;
    const char *speaker = c && g ? TextFormat(T("%s, guardián de %s"), g->name, c->name) : T("La tribu");
    char back[32];
    snprintf(back, sizeof(back), "%s", T("Volver"));
    switch (ga->talk_mode) {
    case CG_ROOT: {
        int idle = 0, people = 0;
        for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
            people += troop->members[i].status == STATUS_ACTIVE && troop->members[i].camp == k;
            idle += idle_in(ga, troop, i, k);
        }
        dlg_begin(d, speaker, ICON_GUARDIAN,
                  TextFormat(T("El campamento está en orden. Somos %d (%d sin tarea) y hay %d tareas en marcha. ¿Qué ordenas?"), people, idle,
                             c ? c->ntask : 0));
        dlg_option(d, ICON_OBRAS, T("Construir"), T("Una obra con el acopio de este campamento"), true, 1);
        dlg_option(d, ICON_TRIBU, T("Escolta"), T("Quién sale contigo del campamento"), true, 2);
        dlg_option(d, ICON_RECLUTAR, T("Pobladores"), T("Quién hace qué: buscar reclutas, explorar, pastorear, cazar o hacer guardia"), true, 3);
        dlg_option(d, ICON_ENTRENAR, T("Capacitar"), T("Enseñar un oficio a alguien sin tarea"), true, 4);
        dlg_option(d, ICON_MAPA, T("Estado"), T("Gente, tareas y acopio"), true, 5);
        dlg_option(d, ICON_DISOLVER, T("Disolver el campamento"), T("Se queman las estructuras y la gente te sigue"), true, 6);
        dlg_option(d, ICON_SALIR, T("Adiós"), "", true, 0);
        break;
    }
    case CG_BUILD:
        dlg_begin(d, speaker, ICON_GUARDIAN, T("¿Qué levantamos? La cuadrilla sale de la gente sin tarea de este campamento."));
        for (int b = 0; b < BUILD_COUNT; b++) {
            const BuildDef *bd = build_def((BuildId)b);
            bool ok = c && stock_has_all(&c->stock, bd->mats);
            dlg_option(d, build_icon((BuildId)b), T(bd->name), c ? cost_text(ga, &c->stock, bd->mats) : "", ok, 10 + b);
        }
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    case CG_ESCORT: {
        dlg_begin(d, speaker, ICON_GUARDIAN, T("¿Quién sale contigo? Los marcados ya te siguen. La escolta no trabaja en las obras."));
        for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
            const Member *m = &troop->members[i];
            if (m->status != STATUS_ACTIVE || (m->camp != k && !ga->npcs[i].escort) || m->id == ga->talk_member) continue;
            bool busy = c && camp_member_busy(c, m->id);
            DlgOption *o = dlg_option(d, role_icon(m->role), m->name,
                                      busy ? T("ocupado en una tarea") : ga->npcs[i].escort ? T("te sigue · clic: que se quede") : T(role_name(m->role)),
                                      !busy && !m->health.down, 100 + m->id);
            if (o) o->marked = ga->npcs[i].escort;
        }
        dlg_option(d, ICON_OK, T("Listo"), "", true, 0);
        break;
    }
    case CG_TRAIN_WHO:
        dlg_begin(d, speaker, ICON_GUARDIAN, T("¿A quién capacitamos? Mientras aprende no hace otra cosa."));
        for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
            if (idle_in(ga, troop, i, k) && troop->members[i].id != ga->talk_member)
                dlg_option(d, role_icon(troop->members[i].role), troop->members[i].name, T(role_name(troop->members[i].role)), true,
                           100 + troop->members[i].id);
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    case CG_TRAIN_ROLE: {
        int mi = member_index(troop, ga->talk_a);
        dlg_begin(d, speaker, ICON_GUARDIAN,
                  TextFormat(T("¿Qué oficio aprende %s? Un maestro del oficio en el campamento lo acelera mucho."), mi >= 0 ? troop->members[mi].name : ""));
        for (int r = 0; r < N_TRAINABLE; r++) {
            int teachers = 0;
            for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
                teachers += troop->members[i].status == STATUS_ACTIVE && troop->members[i].camp == k && troop->members[i].role == TRAINABLE[r];
            bool ok = c && stock_has_all(&c->stock, train_cost(TRAINABLE[r]));
            DlgOption *o = dlg_option(d, role_icon(TRAINABLE[r]), T(role_name(TRAINABLE[r])),
                                      TextFormat(T("%s · %.0f s · maestros: %d"), c ? cost_text(ga, &c->stock, train_cost(TRAINABLE[r])) : "",
                                                 train_work(TRAINABLE[r]), teachers),
                                      ok, 10 + r);
            if (o && teachers) snprintf(o->badge, sizeof(o->badge), "%d", teachers);
        }
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    }
    case CG_STATUS: {
        char txt[320];
        int n = 0, people = 0;
        for (int i = 0; i < troop->count && i < TROOP_MAX; i++) people += troop->members[i].status == STATUS_ACTIVE && troop->members[i].camp == k;
        n += snprintf(txt + n, sizeof(txt) - (size_t)n, T("Gente: %d. "), people);
        for (int j = 0; c && j < c->ntask && n < (int)sizeof(txt); j++) {
            const CampTask *ct = &c->task[j];
            int ids[2] = { ct->member, ct->member2 };
            int pct = (int)(100.0f * ct->done / fmaxf(1.0f, ct->work));
            if (ct->kind == TASK_TRAIN)
                n += snprintf(txt + n, sizeof(txt) - (size_t)n, T("%s aprende %s: %d%%. "), names_of(troop, ids, 1), T(role_name(ct->role)), pct);
            else
                n += snprintf(txt + n, sizeof(txt) - (size_t)n, "%s, %s: %d%%. ", names_of(troop, ids, ct->member2 ? 2 : 1), task_doing(ct->kind), pct);
        }
        n += snprintf(txt + n, sizeof(txt) - (size_t)n, "%s", T("Acopio: "));
        for (int e = 0, shown = 0; c && e < c->stock.n && shown < 8 && n < (int)sizeof(txt); e++)
            if (c->stock.e[e].count > 0)
                n += snprintf(txt + n, sizeof(txt) - (size_t)n, "%s%s %d", shown++ ? ", " : "", item_name(ga, c->stock.e[e].id), c->stock.e[e].count);
        dlg_begin(d, speaker, ICON_GUARDIAN, txt);
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    }
    case CG_DISSOLVE: {
        bool cart = false;
        for (int i = 0; c && i < props->count; i++)
            if (!strcmp(props->items[i].item->id, CART_ID) && dist_xz(props->items[i].pos, c->x, c->z) < CAMP_RADIUS_M) cart = true;
        dlg_begin(d, speaker, ICON_GUARDIAN,
                  cart ? T("¿Disolvemos el campamento? Quemaremos las estructuras, cargaremos en la carreta y en nuestras mochilas lo que quepa y te seguiremos.")
                       : T("¿Disolvemos el campamento? No hay carreta cerca: solo se llevará lo que quepa en las alforjas y en nuestras mochilas. El resto arderá."));
        dlg_option(d, ICON_DISOLVER, T("Disolver"), T("No se puede deshacer"), true, 1);
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    }
    case CG_FOUND: {
        dlg_begin(d, T("La tribu"), ICON_LUGAR,
                  T("Aquí puede nacer un campamento. Hace falta un guardián que lo administre: ¿a quién de tu escolta nombras?"));
        int n = 0;
        for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
            const Member *m = &troop->members[i];
            if (!ga->npcs[i].escort || m->status != STATUS_ACTIVE || m->health.down) continue;
            dlg_option(d, role_icon(m->role), m->name, TextFormat(T("Nombrar guardián a %s (se queda aquí)"), m->name), true, 100 + m->id);
            n++;
        }
        if (!n) d->text[0] = '\0', snprintf(d->text, sizeof(d->text), "%s", T("Aquí podría nacer un campamento, pero no tienes escolta para nombrar un guardián (Y)."));
        dlg_option(d, ICON_FALTA, T("No fundar"), "", true, 0);
        break;
    }
    case CG_PEOPLE: {
        // Lista: los del campamento (menos el guardian), con su oficio, salud, animo y tarea.
        int list[TROOP_MAX], nl = 0;
        for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
            const Member *m = &troop->members[i];
            if (m->status == STATUS_ACTIVE && m->camp == k && m->id != ga->talk_member && ga->npcs[i].member_id == m->id)
                list[nl++] = i;
        }
        // Los que estaban en la lista ya no pueden (salieron, cayeron): se sueltan.
        for (int j = 1; j >= 0; j--) {
            int mi = member_index(troop, ga->people_pick[j]);
            if (ga->people_pick[j] && (mi < 0 || !idle_in(ga, troop, mi, k))) {
                if (j == 0) ga->people_pick[0] = ga->people_pick[1];
                ga->people_pick[1] = 0;
            }
        }
        int pages = (nl + PEOPLE_PAGE - 1) / PEOPLE_PAGE;
        if (ga->people_page >= pages) ga->people_page = 0;
        char txt[512];
        int n = snprintf(txt, sizeof(txt), T("Somos %d en %s. Elige a uno o dos y dales una tarea: acompañados corren menos riesgo y rinden más."), nl + 1,
                         c ? c->name : "");
        if (c && ga->news[k].line[0][0] && n < (int)sizeof(txt)) snprintf(txt + n, sizeof(txt) - (size_t)n, T("\nÚltimo informe: %s"), ga->news[k].line[0]);
        dlg_begin(d, speaker, ICON_GUARDIAN, txt);
        for (int j = ga->people_page * PEOPLE_PAGE; j < nl && j < (ga->people_page + 1) * PEOPLE_PAGE; j++) {
            int i = list[j];
            const Member *m = &troop->members[i];
            const CampTask *ct = camp_task_of(&ga->camps[k], m->id);
            int hp = (int)(100.0f * fmaxf(0.0f, m->health.hp) / fmaxf(1.0f, m->health.hp_max));
            DlgOption *o = dlg_option(d, ct ? task_icon(ct->kind) : role_icon(m->role), m->name,
                                      TextFormat(T("%s · salud %d%% · ánimo %d · %s"), T(role_name(m->role)), hp, (int)m->morale,
                                                 member_doing(ga, troop, i, k)),
                                      idle_in(ga, troop, i, k), 100 + m->id);
            if (!o) continue;
            o->marked = is_picked(ga, m->id);
            if (ct) snprintf(o->badge, sizeof(o->badge), "%d%%", (int)(100.0f * ct->done / fmaxf(1.0f, ct->work)));
        }
        int np = npicked(ga);
        dlg_option(d, ICON_OK, T("Dar una tarea"), np ? TextFormat(T("A %s"), names_of(troop, ga->people_pick, np)) : T("Elige primero a quién"),
                   np > 0, 1);
        if (pages > 1) dlg_option(d, ICON_TRIBU, T("Más gente"), TextFormat(T("Página %d de %d"), ga->people_page + 1, pages), true, 2);
        if (c && ga->news[k].line[1][0]) dlg_option(d, ICON_MENSAJE, T("Informes"), T("Lo que contaron los últimos que volvieron"), true, 3);
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    }
    case CG_NEWS: {
        char txt[1024];
        int n = snprintf(txt, sizeof(txt), T("Lo que contaron los últimos que volvieron a %s:"), c ? c->name : "");
        for (int i = 0; c && i < GA_NEWS && ga->news[k].line[i][0] && n < (int)sizeof(txt); i++)
            n += snprintf(txt + n, sizeof(txt) - (size_t)n, "\n- %s", ga->news[k].line[i]);
        dlg_begin(d, speaker, ICON_GUARDIAN, txt);
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    }
    case CG_TASK: {
        int np = npicked(ga);
        const char *who = names_of(troop, ga->people_pick, np);
        dlg_begin(d, speaker, ICON_GUARDIAN,
                  np > 1 ? TextFormat(T("¿Qué hacen %s? Acompañados corren menos riesgo y rinden más."), who)
                         : TextFormat(T("¿Qué hace %s? Si va con alguien, corre menos riesgo y rinde más."), who));
        bool night = clock_is_night(now);
        static const CampTaskKind KINDS[] = { TASK_RECRUIT, TASK_SCOUT_RESOURCES, TASK_HERD, TASK_HUNT, TASK_GUARD };
        for (int j = 0; j < (int)(sizeof(KINDS) / sizeof(KINDS[0])); j++) {
            CampTaskKind kind = KINDS[j];
            int skilled = skilled_of(troop, ga->people_pick, np, kind);
            const Ingredient *cost = task_cost(kind, ROLE_NONE);
            bool pay = true;
            for (const Ingredient *m = cost; c && m->id; m++) pay &= stock_count(&c->stock, m->id) >= m->count * (np > 0 ? np : 1);
            bool ok = c && np > 0 && pay && c->ntask < CAMP_TASKS;
            int risk = (int)(100.0f * task_risk(kind, skilled, np, night) + 0.5f);
            const char *hint = "";
            const char *label = "";
            switch (kind) {
            case TASK_RECRUIT:
                label = T("Buscar reclutas");
                ok &= troop->count < TROOP_MAX;
                hint = troop->count >= TROOP_MAX ? T("La tribu está llena")
                                                 : TextFormat(T("Salida ~%.0f s · riesgo %d%% · llevan: %s"), task_work(kind, ROLE_NONE), risk,
                                                              c ? cost_text_n(ga, &c->stock, cost, np) : "");
                break;
            case TASK_SCOUT_RESOURCES:
                label = T("Explorar recursos");
                hint = TextFormat(T("Salida ~%.0f s · riesgo %d%% · hasta %.1f km · llevan: %s"),
                                  task_work(kind, ROLE_NONE), risk, scout_radius(skilled, np) / 1000.0f, c ? cost_text_n(ga, &c->stock, cost, np) : "");
                break;
            case TASK_HERD: {
                label = T("Pastorear");
                int herd = c ? herd_size(ga, c) : 0;
                ok &= herd > 0;
                hint = herd ? TextFormat(T("Junto al campamento · %.0f s · %d animales · leche"), task_work(kind, ROLE_NONE), herd)
                            : T("No hay ganado suelto en el campamento");
                break;
            }
            case TASK_HUNT: {
                label = T("Cazar");
                Region rg = c && t && t->world ? world_region(t->world, c->x, c->z) : REGION_STEPPE;
                hint = TextFormat(T("Salida ~%.0f s · riesgo %d%% · %s, %s · llevan: %s"), task_work(kind, ROLE_NONE), risk, region_name(rg),
                                  season_name(clock_season(day)), c ? cost_text_n(ga, &c->stock, cost, np) : "");
                break;
            }
            case TASK_GUARD:
                label = T("Guardia");
                hint = TextFormat(T("La ronda del campamento · %.0f s"), task_work(kind, ROLE_NONE));
                break;
            default: break;
            }
            if (c && c->ntask >= CAMP_TASKS) hint = T("Hay demasiadas tareas en marcha");
            DlgOption *o = dlg_option(d, task_icon(kind), label, hint, ok, 10 + kind);
            if (o && skilled) snprintf(o->badge, sizeof(o->badge), "%d", skilled); // cuantos saben el oficio
        }
        if (np == 1) dlg_option(d, ICON_ENTRENAR, T("Capacitar"), T("Aprender un oficio en el campamento"), true, 9);
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    }
    default: dlg_close(d); break;
    }
}

static void dissolve(GameActions *ga, Troop *troop, Props *props, int k, char *log, size_t len) {
    CampSite *c = &ga->camps[k];
    // A la carreta (si esta cerca) y a las alforjas de las monturas cercanas.
    Bag *dest[1 + GA_PACKS + TROOP_MAX];
    int nd = 0;
    for (int i = 0; i < props->count; i++)
        if (!strcmp(props->items[i].item->id, CART_ID) && dist_xz(props->items[i].pos, c->x, c->z) < CAMP_RADIUS_M) {
            dest[nd++] = &ga->cart;
            break;
        }
    for (int j = 0; j < GA_PACKS; j++) {
        int a = ga->pack_animal[j];
        if (a >= 0 && a < ga->animal_count && ga->animals[a].used &&
            dist_xz((Vector3){ ga->animals[a].x, 0, ga->animals[a].z }, c->x, c->z) < CAMP_RADIUS_M)
            dest[nd++] = &ga->packs[j];
    }
    // Y las mochilas de la gente del campamento, que se va con lo que pueda cargar.
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
        if (troop->members[i].camp == k && troop->members[i].status == STATUS_ACTIVE && troop->members[i].pack > PACK_NONE)
            dest[nd++] = &troop->members[i].bag;
    int loaded = 0, lost = 0;
    for (int e = 0; e < c->stock.n; e++) {
        int left = c->stock.e[e].count;
        for (int b = 0; b < nd && left > 0; b++) left -= bag_add(dest[b], ga->inv, c->stock.e[e].id, left, 1.0f);
        loaded += c->stock.e[e].count - left;
        lost += left;
    }
    stock_init(&c->stock);
    // La gente: sin casa, sigue al jugador.
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
        if (troop->members[i].camp == k) {
            troop->members[i].camp = -1;
            if (troop->members[i].status == STATUS_ACTIVE) ga->npcs[i].escort = true;
        }
    c->used = false; // deja de ser un campamento; x, z quedan para el fuego
    c->ntask = 0;
    ga->burn_camp = k;
    ga->burn_next = 0;
    ga->camps[k].founded_day = 0;
    if (nd) snprintf(log, len, T("Se disuelve %s: arden las estructuras; cargan %d cosas, se pierden %d."), c->name, loaded, lost);
    else snprintf(log, len, T("Se disuelve %s: arden las estructuras y se pierde todo el acopio (%d cosas): no había carreta ni alforjas cerca."), c->name, lost);
}

// Los elegidos salen a su tarea: pagan las provisiones del acopio; los que salen de viaje
// (reclutas, exploracion, caza) se ven alejarse y luego no se ven hasta que vuelven.
static void start_task(GameActions *ga, Troop *troop, const Terrain *t, int k, CampTaskKind kind, char *log, size_t len) {
    CampSite *c = &ga->camps[k];
    int ids[2] = { ga->people_pick[0], ga->people_pick[1] }, n = 0, mi[2];
    for (int j = 0; j < 2; j++) {
        int i = member_index(troop, ids[j]);
        if (ids[j] > 0 && i >= 0 && idle_in(ga, troop, i, k)) mi[n] = i, ids[n++] = ids[j];
    }
    if (!n) return;
    if (c->ntask >= CAMP_TASKS) {
        snprintf(log, len, "%s", T("Hay demasiadas tareas en marcha en este campamento."));
        return;
    }
    if (!task_pay(&c->stock, kind, ROLE_NONE, n)) {
        snprintf(log, len, T("No alcanza el acopio de %s para eso."), c->name);
        return;
    }
    if (!camp_add_task2(c, kind, ids[0], n > 1 ? ids[1] : 0, ROLE_NONE)) return;
    const char *who = names_of(troop, ids, n);
    for (int j = 0; j < n; j++) {
        Npc *np = &ga->npcs[mi[j]];
        np->project = np->job = -1;
        np->escort = false;
        if (!task_away(kind)) continue;
        // Sale hacia afuera del campamento, por su lado.
        float dx = np->pos.x - c->x, dz = np->pos.z - c->z, l = sqrtf(dx * dx + dz * dz);
        if (l < 0.5f) dx = cosf((float)mi[j] * 2.39996f), dz = sinf((float)mi[j] * 2.39996f), l = 1.0f;
        np->leave_dir = (Vector3){ dx / l, 0.0f, dz / l };
        np->leave_t = 0.0f;
    }
    if (kind == TASK_HERD) { // el pasto: a un lado del campamento, en seco
        float a0 = rng_float(&ga->rng) * 6.2831853f;
        ga->herd_at[k] = (Vector3){ c->x + cosf(a0) * HERD_AWAY, 0.0f, c->z + sinf(a0) * HERD_AWAY };
        for (int j = 0; j < 8 && t && t->world; j++) {
            float a = a0 + (float)j * 0.785398f, x = c->x + cosf(a) * HERD_AWAY, z = c->z + sinf(a) * HERD_AWAY;
            if (world_water(t->world, x, z, 0.0f, NULL) < terrain_height(t, x, z)) { // sin agua encima (-1e9: no hay)
                ga->herd_at[k] = (Vector3){ x, terrain_height(t, x, z), z };
                break;
            }
        }
    }
    switch (kind) {
    case TASK_RECRUIT: snprintf(log, len, n > 1 ? T("%s salen a buscar gente nueva.") : T("%s sale a buscar gente nueva."), who); break;
    case TASK_SCOUT_RESOURCES: snprintf(log, len, n > 1 ? T("%s salen a explorar los alrededores.") : T("%s sale a explorar los alrededores."), who); break;
    case TASK_HERD: snprintf(log, len, n > 1 ? T("%s sacan el ganado a pastar.") : T("%s saca el ganado a pastar."), who); break;
    case TASK_HUNT: snprintf(log, len, n > 1 ? T("%s salen a cazar.") : T("%s sale a cazar."), who); break;
    case TASK_GUARD: snprintf(log, len, n > 1 ? T("%s hacen guardia.") : T("%s hace guardia."), who); break;
    default: break;
    }
}

void cg_show_people(GameActions *ga, int a, int b) {
    ga->talk_mode = CG_PEOPLE;
    ga->people_pick[0] = a, ga->people_pick[1] = b;
    ga->people_page = 0;
    ga->dlg.cursor = 0;
}

bool cg_start_task(GameActions *ga, Troop *troop, const Terrain *t, int k, CampTaskKind kind, int a, int b, char *log, size_t len) {
    if (k < 0 || k >= CAMPS_MAX || !ga->camps[k].used) return false;
    ga->people_pick[0] = a, ga->people_pick[1] = b;
    start_task(ga, troop, t, k, kind, log, len);
    ga->people_pick[0] = ga->people_pick[1] = 0;
    const CampTask *ct = camp_task_of(&ga->camps[k], a);
    return ct && ct->kind == kind;
}

static void choose(GameActions *ga, Troop *troop, Props *props, const Terrain *t, int day, int v, char *log, size_t len) {
    int k = camp_of_guardian(ga, ga->talk_member);
    switch (ga->talk_mode) {
    case CG_ROOT:
        if (v == 0) dlg_close(&ga->dlg);
        else set_mode(ga, v == 1 ? CG_BUILD : v == 2 ? CG_ESCORT : v == 3 ? CG_PEOPLE : v == 4 ? CG_TRAIN_WHO : v == 5 ? CG_STATUS : CG_DISSOLVE);
        if (v == 3) ga->people_pick[0] = ga->people_pick[1] = 0, ga->people_page = 0;
        break;
    case CG_BUILD:
        if (v == 0) set_mode(ga, CG_ROOT);
        else if (ga_order_build(ga, (BuildId)(v - 10), k, props, log, len)) dlg_close(&ga->dlg);
        break;
    case CG_ESCORT:
        if (v == 0) {
            dlg_close(&ga->dlg);
        } else {
            int mi = member_index(troop, v - 100);
            if (mi >= 0) {
                Npc *n = &ga->npcs[mi];
                n->escort = !n->escort;
                if (!n->escort && troop->members[mi].camp < 0) troop->members[mi].camp = k; // sin casa: se queda aqui
                n->project = n->job = -1;
            }
        }
        break;
    case CG_TRAIN_WHO:
        if (v == 0) set_mode(ga, CG_ROOT);
        else ga->talk_a = v - 100, set_mode(ga, CG_TRAIN_ROLE);
        break;
    case CG_TRAIN_ROLE:
        if (v == 0) {
            set_mode(ga, is_picked(ga, ga->talk_a) ? CG_TASK : CG_TRAIN_WHO); // vuelve a donde lo eligio
        } else if (k >= 0) {
            Role r = TRAINABLE[v - 10];
            int mi = member_index(troop, ga->talk_a);
            if (mi >= 0 && stock_take_all(&ga->camps[k].stock, train_cost(r)) && camp_add_task(&ga->camps[k], TASK_TRAIN, ga->talk_a, r))
                snprintf(log, len, T("%s empieza a aprender el oficio de %s."), troop->members[mi].name, T(role_name(r)));
            dlg_close(&ga->dlg);
        }
        break;
    case CG_STATUS: set_mode(ga, CG_ROOT); break;
    case CG_PEOPLE:
        if (v == 0) set_mode(ga, CG_ROOT);
        else if (v == 1) set_mode(ga, CG_TASK);
        else if (v == 2) ga->people_page++, ga->dlg.cursor = 0;
        else if (v == 3) set_mode(ga, CG_NEWS);
        else if (v >= 100) toggle_pick(ga, v - 100);
        break;
    case CG_NEWS: set_mode(ga, CG_PEOPLE); break;
    case CG_TASK:
        if (v == 0) {
            set_mode(ga, CG_PEOPLE);
        } else if (v == 9) {
            ga->talk_a = ga->people_pick[0];
            set_mode(ga, CG_TRAIN_ROLE);
        } else if (v >= 10 && k >= 0) {
            start_task(ga, troop, t, k, (CampTaskKind)(v - 10), log, len);
            ga->people_pick[0] = ga->people_pick[1] = 0;
            set_mode(ga, CG_PEOPLE);
        }
        break;
    case CG_DISSOLVE:
        if (v == 0) set_mode(ga, CG_ROOT);
        else if (k >= 0) dissolve(ga, troop, props, k, log, len), dlg_close(&ga->dlg);
        break;
    case CG_FOUND: {
        ga->found_pending = false;
        if (v == 0) {
            dlg_close(&ga->dlg);
            break;
        }
        int mi = member_index(troop, v - 100);
        int nk = mi >= 0 ? camp_found(ga->camps, CAMPS_MAX, ga->found_x, ga->found_z, day) : -1;
        if (nk >= 0) {
            snprintf(ga->camps[nk].name, CAMP_NAME_LEN, T("Campamento %d"), nk + 1);
            ga->camps[nk].guardian = troop->members[mi].id;
            troop->members[mi].camp = nk;
            ga->npcs[mi].escort = false;
            // La escolta sin casa se queda en el campamento nuevo.
            for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
                if (troop->members[i].camp < 0) troop->members[i].camp = nk;
            snprintf(log, len, T("Nace %s. %s queda de guardián: háblale (F) para administrarlo."), ga->camps[nk].name, troop->members[mi].name);
            tg_xp(ga, XP_DISCOVER, log, len);
        }
        dlg_close(&ga->dlg);
        break;
    }
    default: dlg_close(&ga->dlg); break;
    }
}

// ---------------------------------------------------------------- tareas y fuego
// Vuelve de una salida: aparece en el borde del campamento, por donde se fue, y camina a su casa.
static void come_back(GameActions *ga, const Terrain *t, int mi, const CampSite *c, int j) {
    Npc *n = &ga->npcs[mi];
    float dx = n->leave_dir.x, dz = n->leave_dir.z, l = sqrtf(dx * dx + dz * dz);
    if (l < 0.1f) dx = 0.0f, dz = 1.0f, l = 1.0f;
    dx /= l, dz /= l;
    float r = CAMP_RADIUS_M - 6.0f, side = (float)j * 2.0f;
    n->pos = (Vector3){ c->x + dx * r - dz * side, 0.0f, c->z + dz * r + dx * side };
    if (t) n->pos.y = terrain_height(t, n->pos.x, n->pos.z);
    n->leave_t = 0.0f;
    n->home_camp = 0; // que sync_npcs le calcule la casa de nuevo
}

static const char *dist_text(float d) {
    if (d < 1000.0f) return TextFormat(T("%d m"), (int)(d / 10.0f + 0.5f) * 10);
    return TextFormat(T("%d,%d km"), (int)(d / 1000.0f), (int)(fmodf(d, 1000.0f) / 100.0f));
}

// La primera letra en minuscula (los nombres de las especies empiezan en mayuscula).
static const char *lower_first(const char *s) {
    static char buf[64];
    snprintf(buf, sizeof(buf), "%s", s);
    if (buf[0] >= 'A' && buf[0] <= 'Z') buf[0] = (char)(buf[0] - 'A' + 'a');
    return buf;
}

// Explorar recursos: lo que hay en el mundo para descubrir. Los rivales y las ciudadelas primero
// (son pocos); despues el agua (lagos y pozos) y los pastos de las orillas de los rios.
#define SCOUT_CANDS 256
static int scout_cands(const World *w, const CampSite *c, ScoutCand *out, const char **names) {
    int n = 0;
    for (int i = 0; i < w->tribe_count && n < SCOUT_CANDS; i++)
        if (w->tribes[i].attitude == TRIBE_RIVAL) names[n] = w->tribes[i].name, out[n++] = (ScoutCand){ w->tribes[i].x, w->tribes[i].z, FIND_RIVAL };
    for (int i = 0; i < w->settlement_count && n < SCOUT_CANDS; i++)
        if (w->settlements[i].kind == SETTLE_CAPITAL)
            names[n] = w->settlements[i].name, out[n++] = (ScoutCand){ w->settlements[i].x, w->settlements[i].z, FIND_CITADEL };
    for (int i = 0; i < w->lake_count && n < SCOUT_CANDS; i++) { // la orilla que da al campamento
        const Lake *lk = &w->lakes[i];
        float dx = c->x - lk->x, dz = c->z - lk->z, l = sqrtf(dx * dx + dz * dz);
        if (l < 1.0f) dx = 1.0f, dz = 0.0f, l = 1.0f;
        names[n] = NULL, out[n++] = (ScoutCand){ lk->x + dx / l * (lk->r + 4.0f), lk->z + dz / l * (lk->r + 4.0f), FIND_WATER };
    }
    for (int i = 0; i < w->site_count && n < SCOUT_CANDS; i++)
        if (w->sites[i].kind == SITE_WELL) names[n] = NULL, out[n++] = (ScoutCand){ w->sites[i].x, w->sites[i].z, FIND_WATER };
    for (int r = 0; r < w->river_count && n < SCOUT_CANDS; r++) { // vegas: a unos metros del cauce
        const River *rv = &w->rivers[r];
        if (rv->kind == RIVER_BRAIDED) continue; // grava del deshielo: no hay pasto
        for (int i = 12; i + 1 < rv->n && n < SCOUT_CANDS; i += 25) {
            float dx = rv->x[i + 1] - rv->x[i], dz = rv->z[i + 1] - rv->z[i], l = sqrtf(dx * dx + dz * dz);
            if (l < 0.01f) continue;
            float x = rv->x[i] - dz / l * 14.0f, z = rv->z[i] + dx / l * 14.0f;
            Region rg = world_region(w, x, z);
            if (rg == REGION_DESERT || rg == REGION_HIGHLAND) continue;
            names[n] = NULL, out[n++] = (ScoutCand){ x, z, FIND_PASTURE };
        }
    }
    return n;
}

static void scout_finish(GameActions *ga, Troop *troop, const Terrain *t, MemoryMap *mem, int k, const int *alive, int nalive, int skilled,
                         float now, Rng *rng, char *rep, size_t rlen, char *brief, size_t blen) {
    const CampSite *c = &ga->camps[k];
    char who[2 * NAME_LEN + 8];
    snprintf(who, sizeof(who), "%s", names_of(troop, alive, nalive));
    static ScoutCand cand[SCOUT_CANDS];
    static const char *cname[SCOUT_CANDS];
    static bool known[SCOUT_CANDS];
    int nc = t && t->world ? scout_cands(t->world, c, cand, cname) : 0;
    for (int i = 0; i < nc; i++) { // ya marcado en el mapa: no es nuevo
        known[i] = false;
        for (int m = 0; mem && m < mem->marker_count && !known[i]; m++)
            known[i] = (mem->markers[m].x - cand[i].x) * (mem->markers[m].x - cand[i].x) + (mem->markers[m].z - cand[i].z) * (mem->markers[m].z - cand[i].z) <
                       60.0f * 60.0f;
    }
    int pick[4];
    int np = scout_pick(cand, nc, known, c->x, c->z, scout_radius(skilled, nalive), scout_finds(skilled, nalive, rng), pick);
    int n = snprintf(rep, rlen, nalive > 1 ? T("%s vuelven de explorar: ") : T("%s vuelve de explorar: "), who), shown = 0;
    bool full = false;
    for (int j = 0; j < np && mem; j++) {
        const ScoutCand *f = &cand[pick[j]];
        int idx = memmap_add_marker(mem, f->x, f->z, find_danger(f->kind) ? MARKER_DANGER : MARKER_INTEREST, 30.0f);
        if (idx < 0) {
            full = true;
            continue;
        }
        memmap_reveal(mem, f->x, f->z, 45.0f, 90.0f, now);
        for (int i = 0; i < nalive; i++) { // los que fueron ya conocen el camino (src/sim/travel.h)
            Member *m = troop_find(troop, alive[i]);
            if (m) m->known = site_learn(m->known, SITE_MARK(idx));
        }
        const char *what = f->kind == FIND_WATER     ? T("agua")
                           : f->kind == FIND_PASTURE ? T("pastos junto al río")
                           : f->kind == FIND_RIVAL   ? TextFormat(T("un campamento rival (%s)"), cname[pick[j]] ? cname[pick[j]] : "?")
                                                     : TextFormat(T("la ciudadela de %s"), cname[pick[j]] ? cname[pick[j]] : "?");
        float dx = f->x - c->x, dz = f->z - c->z;
        if (n < (int)rlen)
            n += snprintf(rep + n, rlen - (size_t)n, "%s%s", shown ? "; " : "",
                          TextFormat(T("%s a %s al %s"), what, dist_text(sqrtf(dx * dx + dz * dz)), world_compass(dx, dz)));
        shown++;
    }
    if (n < (int)rlen)
        snprintf(rep + n, rlen - (size_t)n, "%s", !shown ? (full ? T("tu mapa ya no tiene lugar para más marcas.") : T("nada nuevo a la vista."))
                                                         : T(". Lo marcan en tu mapa (M)."));
    if (shown)
        snprintf(brief, blen, nalive > 1 ? T("%s vuelven de explorar: %d hallazgos, marcados en tu mapa (M).")
                                         : T("%s vuelve de explorar: %d hallazgos, marcados en tu mapa (M)."),
                 who, shown);
    else
        copy_utf8(brief, blen, rep);
}

static void hunt_finish(GameActions *ga, Troop *troop, const Terrain *t, int k, const int *alive, int nalive, int skilled, int day, Rng *rng,
                        char *rep, size_t rlen, char *brief, size_t blen) {
    CampSite *c = &ga->camps[k];
    char who[2 * NAME_LEN + 8];
    snprintf(who, sizeof(who), "%s", names_of(troop, alive, nalive));
    Region rg = t && t->world ? world_region(t->world, c->x, c->z) : REGION_STEPPE;
    HuntBag b = hunt_bag(region_habitat(rg), clock_season(day), skilled, nalive, rng);
    if (b.kills <= 0) {
        snprintf(rep, rlen, nalive > 1 ? T("%s vuelven de cazar con las manos vacías.") : T("%s vuelve de cazar con las manos vacías."), who);
        copy_utf8(brief, blen, rep);
        return;
    }
    const char *pelt = species_pelt(b.prey);
    stock_add(&c->stock, FRESH_MEAT_ID, b.meat);
    stock_add(&c->stock, pelt ? pelt : "utileria.material.pieles", b.hides);
    stock_add(&c->stock, "utileria.material.hueso", b.bones);
    stock_add(&c->stock, "utileria.material.tendones", b.sinew);
    if (b.fat > 0) stock_add(&c->stock, FAT_ID, b.fat);
    char prey[64];
    snprintf(prey, sizeof(prey), "%s", lower_first(T(species_def((Species)b.prey)->name)));
    int l = snprintf(rep, rlen,
                     nalive > 1 ? T("%s vuelven de cazar: %d presas (%s). Carne %d, pieles %d, huesos %d, tendones %d")
                                : T("%s vuelve de cazar: %d presas (%s). Carne %d, pieles %d, huesos %d, tendones %d"),
                     who, b.kills, prey, b.meat, b.hides, b.bones, b.sinew);
    if (l < (int)rlen) snprintf(rep + l, rlen - (size_t)l, "%s", b.fat > 0 ? TextFormat(T(", grasa %d."), b.fat) : ".");
    snprintf(brief, blen, nalive > 1 ? T("%s vuelven de cazar: %d presas (%s).") : T("%s vuelve de cazar: %d presas (%s)."), who, b.kills, prey);
}

static void recruit_finish(GameActions *ga, Troop *troop, int k, const int *alive, int nalive, int skilled, Rng *rng, char *rep, size_t rlen) {
    const CampSite *c = &ga->camps[k];
    char who[2 * NAME_LEN + 8];
    snprintf(who, sizeof(who), "%s", names_of(troop, alive, nalive));
    int found = recruit_found(skilled, nalive, rng), ids[2], n = 0;
    for (int i = 0; i < found; i++) {
        int nid = troop_recruit(troop, RECRUIT_NAMES[(unsigned)(troop->next_id * 7 + k) % (sizeof(RECRUIT_NAMES) / sizeof(RECRUIT_NAMES[0]))], 0);
        Member *nm = troop_find(troop, nid);
        if (!nm) break;
        nm->camp = k;
        nm->pack = PACK_SMALL; // llega con lo puesto y una mochila pequeña
        bag_init_pack(&nm->bag, PACK_SMALL);
        ids[n++] = nid;
    }
    if (!n) {
        snprintf(rep, rlen,
                 found        ? (nalive > 1 ? T("%s vuelven con gente, pero la tribu está llena.") : T("%s vuelve con gente, pero la tribu está llena."))
                 : nalive > 1 ? T("%s vuelven sin nadie: nadie quiso venir.")
                              : T("%s vuelve sin nadie: nadie quiso venir."),
                 who);
        return;
    }
    char joined[2 * NAME_LEN + 8];
    snprintf(joined, sizeof(joined), "%s", names_of(troop, ids, n));
    int l = snprintf(rep, rlen, nalive > 1 ? T("%s vuelven con gente nueva: ") : T("%s vuelve con gente nueva: "), who);
    if (l < (int)rlen) snprintf(rep + l, rlen - (size_t)l, n > 1 ? T("%s se unen a %s.") : T("%s se une a %s."), joined, c->name);
}

static void herd_finish(GameActions *ga, Troop *troop, int k, const int *ids, int n, bool night, Rng *rng, char *rep, size_t rlen, char *brief,
                        size_t blen) {
    CampSite *c = &ga->camps[k];
    int skilled = skilled_of(troop, ids, n, TASK_HERD), herd = 0, milk = 0, lost = -1, nlivestock = 0;
    bool attacked = rng_float(rng) < herd_loss_chance(n, skilled, night);
    for (int i = 0; i < ga->animal_count && i < GA_MAX_ANIMALS; i++) {
        if (!herdable(ga, i, c)) continue;
        Animal *a = &ga->animals[i];
        herd++;
        milk += herd_milk(a, n);
        a->home_x = c->x + cosf((float)i * 2.39996f) * 9.0f; // de vuelta al campamento
        a->home_z = c->z + sinf((float)i * 2.39996f) * 9.0f;
        bool stock = species_def(a->species)->cls == CLASS_LIVESTOCK;
        if (attacked && (lost < 0 || (stock && rng_range(rng, ++nlivestock) == 0))) lost = i; // las fieras van por el ganado chico
    }
    if (milk > 0) stock_add(&c->stock, MILK_ID, milk);
    char who[2 * NAME_LEN + 8];
    snprintf(who, sizeof(who), "%s", names_of(troop, ids, n));
    int len = snprintf(rep, rlen, n > 1 ? T("%s vuelven de pastorear: %d animales comieron y bebieron; leche %d.")
                                        : T("%s vuelve de pastorear: %d animales comieron y bebieron; leche %d."),
                       who, herd, milk);
    int bl = snprintf(brief, blen, n > 1 ? T("%s vuelven de pastorear: leche %d.") : T("%s vuelve de pastorear: leche %d."), who, milk);
    if (lost >= 0) {
        char what[64];
        snprintf(what, sizeof(what), "%s", lower_first(T(species_def(ga->animals[lost].species)->name)));
        animal_hurt(ga->animals, ga->animal_count, lost, rng, 9999.0f, WOUND_BITE, PART_RANDOM, false, -1);
        if (len < (int)rlen) snprintf(rep + len, rlen - (size_t)len, T(" Las fieras atacaron el rebaño: perdieron un animal (%s)."), what);
        if (bl < (int)blen) snprintf(brief + bl, blen - (size_t)bl, T(" Las fieras se llevaron un animal (%s)."), what);
    }
}

// Termino una tarea: la capacitacion da el oficio; las salidas vuelven (o no) con lo que traen; el
// pastoreo trae leche (y, a veces, una baja); la guardia termina su turno.
static void finish_task(GameActions *ga, Troop *troop, const Terrain *t, MemoryMap *mem, int k, const CampTask *d, int day, float now, Rng *rng,
                        char *log, size_t len) {
    CampSite *c = &ga->camps[k];
    int ids[2] = { d->member, d->member2 }, n = d->member2 > 0 ? 2 : 1;
    char rep[240], brief[128];
    rep[0] = brief[0] = '\0';
    if (d->kind == TASK_TRAIN) {
        int mi = member_index(troop, d->member);
        if (mi < 0) return;
        troop_assign_role(troop, d->member, d->role);
        snprintf(rep, sizeof(rep), T("%s ya es %s (%s)."), troop->members[mi].name, T(role_name(d->role)), c->name);
        report(ga, k, rep, NULL, log, len);
        return;
    }
    // Los que siguen en la tribu (alguien pudo desertar mientras tanto).
    int people[2], np = 0;
    for (int i = 0; i < n; i++) {
        const Member *m = troop_find(troop, ids[i]);
        if (m && m->status == STATUS_ACTIVE) people[np++] = ids[i];
    }
    if (!np) return;
    if (d->kind == TASK_HERD) {
        herd_finish(ga, troop, k, people, np, clock_is_night(now), rng, rep, sizeof(rep), brief, sizeof(brief));
        for (int i = 0; i < np; i++) {
            int mi = member_index(troop, people[i]);
            if (mi >= 0) ga->npcs[mi].home_camp = 0; // vuelve a su casa
        }
        report(ga, k, rep, brief, log, len);
        return;
    }
    if (d->kind == TASK_GUARD) {
        snprintf(rep, sizeof(rep), np > 1 ? T("%s terminan su turno de guardia.") : T("%s termina su turno de guardia."), names_of(troop, people, np));
        report(ga, k, rep, NULL, log, len);
        return;
    }
    // Las salidas: la suerte de cada uno; los que vuelven aparecen en el borde del campamento.
    Fate fate[2];
    task_fates(d->kind, skilled_of(troop, people, np, d->kind), np, clock_is_night(now), rng, fate);
    int alive[2], na = 0, hurt[2], nh = 0, dead[2], ndead = 0;
    for (int i = 0; i < np; i++) {
        int mi = member_index(troop, people[i]);
        if (mi < 0) continue;
        Member *m = &troop->members[mi];
        if (fate[i] == FATE_DEAD) {
            dead[ndead++] = people[i];
            continue;
        }
        if (fate[i] == FATE_WOUNDED) {
            health_hit(&m->health, rng, 25.0f + 20.0f * rng_float(rng), d->kind == TASK_HUNT ? WOUND_BITE : WOUND_CUT, PART_RANDOM);
            hurt[nh++] = people[i];
        }
        come_back(ga, t, mi, c, na);
        alive[na++] = people[i];
    }
    char lost_names[2 * NAME_LEN + 8];
    snprintf(lost_names, sizeof(lost_names), "%s", names_of(troop, dead, ndead));
    if (na) {
        int skilled = skilled_of(troop, alive, na, d->kind);
        if (d->kind == TASK_RECRUIT) recruit_finish(ga, troop, k, alive, na, skilled, rng, rep, sizeof(rep));
        else if (d->kind == TASK_SCOUT_RESOURCES) scout_finish(ga, troop, t, mem, k, alive, na, skilled, now, rng, rep, sizeof(rep), brief, sizeof(brief));
        else hunt_finish(ga, troop, t, k, alive, na, skilled, day, rng, rep, sizeof(rep), brief, sizeof(brief));
        if (!brief[0]) copy_utf8(brief, sizeof(brief), rep);
        // Lo que les paso, al final de los dos textos.
        char extra[96] = "";
        int l = 0;
        if (nh) l += snprintf(extra + l, sizeof(extra) - (size_t)l, nh > 1 ? T(" %s volvieron con heridas.") : T(" %s volvió con heridas."), names_of(troop, hurt, nh));
        if (ndead && l < (int)sizeof(extra)) snprintf(extra + l, sizeof(extra) - (size_t)l, T(" %s no volvió: la tribu está de luto."), lost_names);
        if (extra[0]) {
            size_t rl = strlen(rep), bl = strlen(brief);
            copy_utf8(rep + rl, sizeof(rep) - rl, extra);
            copy_utf8(brief + bl, sizeof(brief) - bl, extra);
        }
    } else {
        snprintf(rep, sizeof(rep), ndead > 1 ? T("No volvieron de su salida: %s. La tribu está de luto.") : T("No volvió de su salida: %s. La tribu está de luto."),
                 lost_names);
    }
    for (int i = 0; i < ndead; i++) troop_mourn(troop, dead[i], 3.0f);
    report(ga, k, rep, brief, log, len);
}

static void tick_tasks(GameActions *ga, Troop *troop, const Terrain *t, MemoryMap *mem, int day, float now, float dt, char *log, size_t len) {
    static Rng rng;
    static bool seeded;
    if (!seeded) rng_seed(&rng, 0xCA3B5u), seeded = true;
    for (int k = 0; k < CAMPS_MAX; k++) {
        CampSite *c = &ga->camps[k];
        if (!c->used || !c->ntask) continue;
        int idle = 0, ntrain = 0;
        for (int i = 0; i < troop->count && i < TROOP_MAX; i++) idle += idle_in(ga, troop, i, k);
        for (int j = 0; j < c->ntask; j++) ntrain += c->task[j].kind == TASK_TRAIN;
        float rates[CAMP_TASKS];
        for (int j = 0; j < c->ntask; j++) {
            if (c->task[j].kind != TASK_TRAIN) { // las salidas y los turnos duran lo que duran
                rates[j] = 1.0f;
                continue;
            }
            int teachers = 0;
            for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
                teachers += troop->members[i].status == STATUS_ACTIVE && troop->members[i].camp == k && troop->members[i].role == c->task[j].role;
            // Los ayudantes ociosos se reparten entre los que aprenden.
            rates[j] = task_rate(idle / ntrain, teachers);
        }
        CampTask done[CAMP_TASKS];
        int nd = camp_tick(c, dt, rates, done, CAMP_TASKS);
        for (int j = 0; j < nd; j++) finish_task(ga, troop, t, mem, k, &done[j], day, now, &rng, log, len);
    }
}

// Los que salen: se alejan del campamento y, pasado un rato, ya no se ven.
static void tick_trips(GameActions *ga, Troop *troop, const Terrain *t, float dt) {
    for (int k = 0; k < CAMPS_MAX; k++) {
        const CampSite *c = &ga->camps[k];
        for (int j = 0; c->used && j < c->ntask; j++) {
            if (!task_away(c->task[j].kind)) continue;
            int ids[2] = { c->task[j].member, c->task[j].member2 };
            for (int i = 0; i < 2; i++) {
                int mi = ids[i] > 0 ? member_index(troop, ids[i]) : -1;
                if (mi < 0) continue;
                Npc *n = &ga->npcs[mi];
                if (n->leave_t >= LEAVE_SECONDS) continue;
                n->leave_t += dt;
                n->pos.x += n->leave_dir.x * LEAVE_SPEED * dt;
                n->pos.z += n->leave_dir.z * LEAVE_SPEED * dt;
                if (t) n->pos.y = terrain_height(t, n->pos.x, n->pos.z);
                n->yaw = atan2f(n->leave_dir.x, n->leave_dir.z);
                n->moving = true;
            }
        }
    }
}

// El pastoreo se ve: los pastores rondan el pasto y el ganado pasta a su lado, come y bebe.
static void tick_herd(GameActions *ga, Troop *troop, float now, float dt) {
    for (int k = 0; k < CAMPS_MAX; k++) {
        const CampSite *c = &ga->camps[k];
        if (!c->used) continue;
        int herders[4], nh = 0;
        for (int j = 0; j < c->ntask; j++) {
            if (c->task[j].kind != TASK_HERD) continue;
            if (nh < 4) herders[nh++] = c->task[j].member;
            if (c->task[j].member2 > 0 && nh < 4) herders[nh++] = c->task[j].member2;
        }
        if (!nh) continue;
        Vector3 at = ga->herd_at[k];
        if (at.x == 0.0f && at.z == 0.0f) at = ga->herd_at[k] = (Vector3){ c->x + HERD_AWAY, 0.0f, c->z };
        for (int j = 0; j < nh; j++) {
            int mi = member_index(troop, herders[j]);
            if (mi < 0) continue;
            float a = now * 0.12f + (float)j * 3.14159f; // dan la vuelta al rebaño
            ga->npcs[mi].home = (Vector3){ at.x + cosf(a) * 6.0f, at.y, at.z + sinf(a) * 6.0f };
        }
        for (int i = 0; i < ga->animal_count && i < GA_MAX_ANIMALS; i++) {
            if (!herdable(ga, i, c)) continue;
            Animal *a = &ga->animals[i];
            a->home_x = at.x + cosf((float)i * 2.39996f) * 3.0f;
            a->home_z = at.z + sinf((float)i * 2.39996f) * 3.0f;
            herd_graze(a, dt, nh, ga->grass);
        }
    }
}

static bool is_structure(const Prop *pr) {
    return !strncmp(pr->item->id, "estructura.", 11) && strncmp(pr->item->id, "estructura.ruina.", 17) != 0;
}

static void tick_burn(GameActions *ga, Props *props, float dt) {
    if (ga->burn_camp < 0) return;
    const CampSite *c = &ga->camps[ga->burn_camp];
    static float t = 0.0f;
    t += dt;
    // Prende el fuego estructura por estructura (se ve arder); al rato, lo que quede son cenizas.
    for (int i = ga->burn_next; i < props->count && ga->ignite_n < 8; i++, ga->burn_next = i)
        if (is_structure(&props->items[i]) && dist_xz(props->items[i].pos, c->x, c->z) < CAMP_RADIUS_M)
            ga->ignite_at[ga->ignite_n++] = props->items[i].pos;
    if (t < BURN_SECONDS) return;
    for (int i = props->count - 1; i >= 0; i--) {
        Prop pr = props->items[i];
        if (!is_structure(&pr) || dist_xz(pr.pos, c->x, c->z) >= CAMP_RADIUS_M) continue;
        props_remove(props, i);
        props_add(props, "estructura.ruina.cenizas", pr.pos, pr.yaw);
    }
    t = 0.0f;
    ga->burn_camp = ga->burn_next = -1;
}

void cg_update(GameActions *ga, Troop *troop, Props *props, const Player *p, const Terrain *t, MemoryMap *mem, float now, float dt, char *log,
               size_t len) {
    (void)p;
    int day = clock_day(now);
    // El campamento inicial: su guardian es el lugarteniente.
    if (ga->camps[0].used && ga->camps[0].guardian < 0)
        for (int i = 0; i < troop->count; i++)
            if (troop->members[i].role == ROLE_LIEUTENANT && troop->members[i].status == STATUS_ACTIVE) ga->camps[0].guardian = troop->members[i].id;
    // El guardian murio o se fue: otro de su campamento ocupa su lugar.
    for (int k = 0; k < CAMPS_MAX; k++) {
        CampSite *c = &ga->camps[k];
        if (!c->used) continue;
        const Member *g = troop_find(troop, c->guardian);
        if (g && g->status == STATUS_ACTIVE && g->camp == k) continue;
        c->guardian = -1;
        for (int i = 0; i < troop->count && c->guardian < 0; i++) // uno que este en el campamento (no de salida)
            if (troop->members[i].status == STATUS_ACTIVE && troop->members[i].camp == k && !ga_away(ga, &troop->members[i]))
                c->guardian = troop->members[i].id;
    }
    tick_tasks(ga, troop, t, mem, day, now, dt, log, len);
    tick_trips(ga, troop, t, dt);
    tick_herd(ga, troop, now, dt);
    tick_burn(ga, props, dt);
    if (ga->found_pending && !ga->dlg.open) {
        ga->talk_mode = CG_FOUND;
        ga->talk_member = -1;
        ga->dlg.cursor = 0;
        ga->dlg.open = true;
    }
    if (!ga->dlg.open || ga->talk_mode < CG_ROOT || ga->talk_mode >= 200) return; // los >= 200: src/game/travel_game.c
    if (ga->talk_mode != CG_FOUND && camp_of_guardian(ga, ga->talk_member) < 0) { // ya no es guardian
        dlg_close(&ga->dlg);
        return;
    }
    build_dialog(ga, troop, props, t, day, now);
    int v = dlg_update(&ga->dlg);
    if (v == DLG_BACK) {
        if (ga->talk_mode == CG_ROOT) dlg_close(&ga->dlg);
        else if (ga->talk_mode == CG_FOUND) ga->found_pending = false, dlg_close(&ga->dlg);
        else if (ga->talk_mode == CG_TRAIN_ROLE) set_mode(ga, is_picked(ga, ga->talk_a) ? CG_TASK : CG_TRAIN_WHO);
        else set_mode(ga, ga->talk_mode == CG_TASK || ga->talk_mode == CG_NEWS ? CG_PEOPLE : CG_ROOT);
    } else if (v >= 0) {
        choose(ga, troop, props, t, day, v, log, len);
    }
    if (ga->dlg.open) build_dialog(ga, troop, props, t, day, now);
}
