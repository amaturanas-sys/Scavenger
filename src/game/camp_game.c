#include "camp_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/inventory_game.h"
#include "game/talents_game.h"
#include "raylib.h"
#include "sim/lang.h"
#include "ui/icons.h"

#define TALK_RANGE 3.0f
#define CART_ID "vehiculo.tierra.carreta_bueyes"
#define BURN_SECONDS 8.0f

typedef enum {
    CG_ROOT = 100,
    CG_BUILD,
    CG_ESCORT,
    CG_RECRUIT,
    CG_TRAIN_WHO,
    CG_TRAIN_ROLE,
    CG_TRAIN_CONFIRM,
    CG_STATUS,
    CG_DISSOLVE,
    CG_FOUND,
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

static const char *cost_text(GameActions *ga, const Stockpile *st, const Ingredient *mats) {
    static char buf[160];
    int n = 0;
    buf[0] = '\0';
    for (const Ingredient *m = mats; m->id && n < (int)sizeof(buf); m++)
        n += snprintf(buf + n, sizeof(buf) - (size_t)n, "%s%s %d/%d", n ? ", " : "", item_name(ga, m->id), stock_count(st, m->id), m->count);
    return n ? buf : T("nada");
}

static const char *RECRUIT_NAMES[] = { "Arslan", "Batu", "Chagatai", "Dorji", "Erdene", "Gansukh", "Khulan", "Mönkh",
                                       "Naran", "Oyun",   "Sarnai", "Tolui",   "Tumen", "Yesui",   "Zaya",  "Bolor" };

// ---------------------------------------------------------------- dialogo
static void build_dialog(GameActions *ga, const Troop *troop, const Props *props) {
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
        dlg_option(d, ICON_RECLUTAR, T("Reclutar"), T("Mandar a alguien a buscar gente nueva"), true, 3);
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
    case CG_RECRUIT: {
        int who = -1;
        for (int i = 0; i < troop->count && i < TROOP_MAX && who < 0; i++)
            if (idle_in(ga, troop, i, k) && troop->members[i].id != ga->talk_member) who = i;
        bool ok = who >= 0 && c && stock_has_all(&c->stock, recruit_cost()) && troop->count < TROOP_MAX;
        dlg_begin(d, speaker, ICON_GUARDIAN,
                  TextFormat(T("Alguien sin tarea puede salir a buscar gente por los campamentos de la estepa. Lleva: %s. Tarda unos %.0f s."),
                             c ? cost_text(ga, &c->stock, recruit_cost()) : "", recruit_work()));
        dlg_option(d, ICON_RECLUTAR, T("Que salga a reclutar"),
                   who < 0 ? T("No hay nadie sin tarea") : TextFormat(T("Irá %s"), troop->members[who].name), ok, 1);
        dlg_option(d, ICON_TITULO, back, "", true, 0);
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
        for (int t = 0; c && t < c->ntask && n < (int)sizeof(txt); t++) {
            int mi = member_index(troop, c->task[t].member);
            n += snprintf(txt + n, sizeof(txt) - (size_t)n, "%s%s %d%%. ",
                          c->task[t].kind == TASK_RECRUIT ? T("Reclutando: ") : TextFormat(T("%s aprende %s: "), mi >= 0 ? troop->members[mi].name : "",
                                                                                          T(role_name(c->task[t].role))),
                          mi >= 0 && c->task[t].kind == TASK_RECRUIT ? troop->members[mi].name : "", (int)(100.0f * c->task[t].done / c->task[t].work));
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
                  cart ? T("¿Disolvemos el campamento? Quemaremos las estructuras, cargaremos en la carreta lo que quepa y te seguiremos.")
                       : T("¿Disolvemos el campamento? No hay carreta cerca: solo se llevará lo que quepa en las alforjas. El resto arderá."));
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
    default: dlg_close(d); break;
    }
}

static void dissolve(GameActions *ga, Troop *troop, Props *props, int k, char *log, size_t len) {
    CampSite *c = &ga->camps[k];
    // A la carreta (si esta cerca) y a las alforjas de las monturas cercanas.
    Bag *dest[1 + GA_PACKS];
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

static void choose(GameActions *ga, Troop *troop, Props *props, int day, int v, char *log, size_t len) {
    int k = camp_of_guardian(ga, ga->talk_member);
    switch (ga->talk_mode) {
    case CG_ROOT:
        if (v == 0) dlg_close(&ga->dlg);
        else set_mode(ga, v == 1 ? CG_BUILD : v == 2 ? CG_ESCORT : v == 3 ? CG_RECRUIT : v == 4 ? CG_TRAIN_WHO : v == 5 ? CG_STATUS : CG_DISSOLVE);
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
    case CG_RECRUIT:
        if (v == 0) {
            set_mode(ga, CG_ROOT);
        } else if (k >= 0) {
            for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
                if (idle_in(ga, troop, i, k) && troop->members[i].id != ga->talk_member) {
                    if (stock_take_all(&ga->camps[k].stock, recruit_cost()) && camp_add_task(&ga->camps[k], TASK_RECRUIT, troop->members[i].id, ROLE_NONE))
                        snprintf(log, len, T("%s sale a buscar gente nueva para %s."), troop->members[i].name, ga->camps[k].name);
                    break;
                }
            dlg_close(&ga->dlg);
        }
        break;
    case CG_TRAIN_WHO:
        if (v == 0) set_mode(ga, CG_ROOT);
        else ga->talk_a = v - 100, set_mode(ga, CG_TRAIN_ROLE);
        break;
    case CG_TRAIN_ROLE:
        if (v == 0) {
            set_mode(ga, CG_TRAIN_WHO);
        } else if (k >= 0) {
            Role r = TRAINABLE[v - 10];
            int mi = member_index(troop, ga->talk_a);
            if (mi >= 0 && stock_take_all(&ga->camps[k].stock, train_cost(r)) && camp_add_task(&ga->camps[k], TASK_TRAIN, ga->talk_a, r))
                snprintf(log, len, T("%s empieza a aprender el oficio de %s."), troop->members[mi].name, T(role_name(r)));
            dlg_close(&ga->dlg);
        }
        break;
    case CG_STATUS: set_mode(ga, CG_ROOT); break;
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
static void tick_tasks(GameActions *ga, Troop *troop, float dt, char *log, size_t len) {
    for (int k = 0; k < CAMPS_MAX; k++) {
        CampSite *c = &ga->camps[k];
        if (!c->used || !c->ntask) continue;
        int idle = 0;
        for (int i = 0; i < troop->count && i < TROOP_MAX; i++) idle += idle_in(ga, troop, i, k);
        float rates[CAMP_TASKS];
        for (int t = 0; t < c->ntask; t++) {
            int teachers = 0;
            for (int i = 0; c->task[t].kind == TASK_TRAIN && i < troop->count && i < TROOP_MAX; i++)
                teachers += troop->members[i].status == STATUS_ACTIVE && troop->members[i].camp == k && troop->members[i].role == c->task[t].role;
            // Los ayudantes ociosos se reparten entre las tareas en marcha.
            rates[t] = task_rate(idle / c->ntask, teachers);
        }
        CampTask done[CAMP_TASKS];
        int nd = camp_tick(c, dt, rates, done, CAMP_TASKS);
        for (int j = 0; j < nd; j++) {
            int mi = member_index(troop, done[j].member);
            if (done[j].kind == TASK_TRAIN && mi >= 0) {
                troop_assign_role(troop, done[j].member, done[j].role);
                snprintf(log, len, T("%s ya es %s (%s)."), troop->members[mi].name, T(role_name(done[j].role)), c->name);
            } else if (done[j].kind == TASK_RECRUIT) {
                int nid = troop_recruit(troop, RECRUIT_NAMES[(unsigned)(troop->next_id * 7 + k) % (sizeof(RECRUIT_NAMES) / sizeof(RECRUIT_NAMES[0]))], 0);
                Member *nm = troop_find(troop, nid);
                if (nm) {
                    nm->camp = k;
                    snprintf(log, len, T("%s vuelve con %s: se une a %s."), mi >= 0 ? troop->members[mi].name : "", nm->name, c->name);
                } else {
                    snprintf(log, len, "%s", T("Volvió sin nadie: la tribu está llena."));
                }
            }
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

void cg_update(GameActions *ga, Troop *troop, Props *props, const Player *p, int day, float dt, char *log, size_t len) {
    (void)p;
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
        for (int i = 0; i < troop->count && c->guardian < 0; i++)
            if (troop->members[i].status == STATUS_ACTIVE && troop->members[i].camp == k) c->guardian = troop->members[i].id;
    }
    tick_tasks(ga, troop, dt, log, len);
    tick_burn(ga, props, dt);
    if (ga->found_pending && !ga->dlg.open) {
        ga->talk_mode = CG_FOUND;
        ga->talk_member = -1;
        ga->dlg.cursor = 0;
        ga->dlg.open = true;
    }
    if (!ga->dlg.open || ga->talk_mode < CG_ROOT) return;
    if (ga->talk_mode != CG_FOUND && camp_of_guardian(ga, ga->talk_member) < 0) { // ya no es guardian
        dlg_close(&ga->dlg);
        return;
    }
    build_dialog(ga, troop, props);
    int v = dlg_update(&ga->dlg);
    if (v == DLG_BACK) {
        if (ga->talk_mode == CG_ROOT) dlg_close(&ga->dlg);
        else if (ga->talk_mode == CG_FOUND) ga->found_pending = false, dlg_close(&ga->dlg);
        else set_mode(ga, ga->talk_mode == CG_TRAIN_ROLE ? CG_TRAIN_WHO : CG_ROOT);
    } else if (v >= 0) {
        choose(ga, troop, props, day, v, log, len);
    }
    if (ga->dlg.open) build_dialog(ga, troop, props);
}
