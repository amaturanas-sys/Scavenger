#include "travel_game.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/combat_game.h"
#include "game/input.h"
#include "raylib.h"
#include "sim/lang.h"
#include "ui/icons.h"

#define LEARN_RADIUS 35.0f  // m: quien pasa cerca de un sitio lo conoce
#define LEAVE_SECONDS 14.0f // s que se ve a los despachados alejarse
#define REPORT_RADIUS 40.0f // m: al llegar al destino, el jugador se entera de como les fue

typedef enum { TV_ROOT = 200, TV_WHO, TV_WHERE, TV_MSG_WHO, TV_MSG_WHERE, TV_MSG_COUNT } TravelTalk;

typedef struct {
    int id;
    float x, z;
    char name[48];
    bool camp;
} Site;

static float dist_xz(float ax, float az, float bx, float bz) { return sqrtf((ax - bx) * (ax - bx) + (az - bz) * (az - bz)); }

bool trv_away(const Member *m) { return m->journey > 0; }

int trv_count(const GameActions *ga) {
    int n = 0;
    for (int i = 0; i < JOURNEYS_MAX; i++) n += ga->journeys[i].used;
    return n;
}

static int sites(const GameActions *ga, const MemoryMap *mem, Site *out, int max) {
    int n = 0;
    for (int k = 0; k < CAMPS_MAX && n < max; k++) {
        if (!ga->camps[k].used) continue;
        out[n] = (Site){ SITE_CAMP(k), ga->camps[k].x, ga->camps[k].z, "", true };
        snprintf(out[n].name, sizeof(out[n].name), "%s", ga->camps[k].name);
        n++;
    }
    for (int i = 0; mem && i < mem->marker_count && i < SITES_MAX - 8 && n < max; i++) {
        const MapMarker *mk = &mem->markers[i];
        out[n] = (Site){ SITE_MARK(i), mk->x, mk->z, "", false };
        snprintf(out[n].name, sizeof(out[n].name), mk->kind == MARKER_DANGER ? T("Sitio de peligro %d") : T("Sitio marcado %d"), i + 1);
        n++;
    }
    return n;
}

static bool site_of(const GameActions *ga, const MemoryMap *mem, int id, Site *out) {
    Site all[CAMPS_MAX + 32];
    int n = sites(ga, mem, all, CAMPS_MAX + 32);
    for (int i = 0; i < n; i++)
        if (all[i].id == id) return *out = all[i], true;
    return false;
}

static int member_index(const Troop *troop, int id) {
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
        if (troop->members[i].id == id) return i;
    return -1;
}

static void set_mode(GameActions *ga, int mode) {
    ga->talk_mode = mode;
    ga->dlg.cursor = 0;
}

static bool picked(const GameActions *ga, int id) {
    for (int i = 0; i < ga->travel_npick; i++)
        if (ga->travel_pick[i] == id) return true;
    return false;
}

static void toggle_pick(GameActions *ga, int id) {
    for (int i = 0; i < ga->travel_npick; i++)
        if (ga->travel_pick[i] == id) {
            ga->travel_pick[i] = ga->travel_pick[--ga->travel_npick];
            return;
        }
    if (ga->travel_npick < JOURNEY_PEOPLE) ga->travel_pick[ga->travel_npick++] = id;
}

// Cuantos de los elegidos conocen el sitio.
static int knowers(const Troop *troop, const int *ids, int n, int site) {
    int k = 0;
    for (int i = 0; i < n; i++) {
        int mi = member_index(troop, ids[i]);
        if (mi >= 0 && site_known(troop->members[mi].known, site)) k++;
    }
    return k;
}

// Los que salen: dejan la escolta, se alejan hacia el destino y luego no se ven.
static void depart(GameActions *ga, Troop *troop, int j) {
    Journey *jr = &ga->journeys[j];
    float dx = jr->x1 - jr->x0, dz = jr->z1 - jr->z0, l = sqrtf(dx * dx + dz * dz);
    for (int i = 0; i < jr->n; i++) {
        int mi = member_index(troop, jr->people[i]);
        if (mi < 0) continue;
        Member *m = &troop->members[mi];
        Npc *n = &ga->npcs[mi];
        m->journey = j + 1;
        m->camp = -1;
        n->escort = false;
        n->project = n->job = -1;
        n->leave_t = 0.0f;
        n->leave_dir = l > 0.1f ? (Vector3){ dx / l, 0, dz / l } : (Vector3){ 0, 0, 1 };
    }
}

static TravelMode mode_now(const GameActions *ga) { return ga->mounted >= 0 ? TRAVEL_MOUNTED : TRAVEL_FOOT; }

// ---------------------------------------------------------------- dialogo
static void build_dialog(GameActions *ga, const Troop *troop, const MemoryMap *mem, const Player *p, bool night) {
    Dialog *d = &ga->dlg;
    char back[32];
    snprintf(back, sizeof(back), "%s", T("Volver"));
    Site all[CAMPS_MAX + 32];
    int ns = sites(ga, mem, all, CAMPS_MAX + 32);
    switch (ga->talk_mode) {
    case TV_ROOT: {
        int esc = 0;
        for (int i = 0; i < troop->count && i < TROOP_MAX; i++) esc += ga->npcs[i].escort && troop->members[i].status == STATUS_ACTIVE;
        dlg_begin(d, T("Órdenes a la escolta"), ICON_TRIBU,
                  TextFormat(T("Te siguen %d. Hay %d partidas de camino. Despacha a quien conozca el destino: si nadie lo conoce, se pierden."), esc,
                             trv_count(ga)));
        dlg_option(d, ICON_DESPACHAR, T("Despachar"), T("A un campamento o a un sitio marcado en el mapa"), esc > 0, 1);
        dlg_option(d, ICON_MENSAJERO, T("Mandar un mensajero"), T("A un campamento: vuelve con refuerzos (ida y vuelta)"), esc > 0, 2);
        dlg_option(d, ICON_SALIR, T("Cerrar"), "", true, 0);
        break;
    }
    case TV_WHO:
        dlg_begin(d, T("Órdenes a la escolta"), ICON_DESPACHAR, T("¿Quiénes van? Marca a los que salen y luego elige el destino."));
        for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
            const Member *m = &troop->members[i];
            if (!ga->npcs[i].escort || m->status != STATUS_ACTIVE || m->health.down) continue;
            DlgOption *o = dlg_option(d, ICON_PERSONA, m->name, picked(ga, m->id) ? T("va · clic: se queda") : T("se queda · clic: va"), true, 100 + m->id);
            if (o) o->marked = picked(ga, m->id);
        }
        dlg_option(d, ICON_LUGAR, T("Elegir destino"), TextFormat(T("%d elegidos"), ga->travel_npick), ga->travel_npick > 0, 1);
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    case TV_WHERE:
        dlg_begin(d, T("Órdenes a la escolta"), ICON_LUGAR, T("¿Adónde? Solo se puede ir donde alguno haya estado. Cuantos más lo conozcan, menos riesgo."));
        for (int s = 0; s < ns; s++) {
            int k = knowers(troop, ga->travel_pick, ga->travel_npick, all[s].id);
            float dist = dist_xz(p->pos.x, p->pos.z, all[s].x, all[s].z);
            float risk = journey_risk(dist, ga->travel_npick ? (float)k / (float)ga->travel_npick : 0.0f, ga->travel_npick, night);
            DlgOption *o = dlg_option(d, all[s].camp ? ICON_CAMPAMENTO : ICON_LUGAR, all[s].name,
                                      k ? TextFormat(T("%.0f m · ~%.0f s · lo conocen %d de %d · riesgo %d%%"), dist, journey_eta(dist, mode_now(ga)), k,
                                                     ga->travel_npick, (int)(risk * 100.0f + 0.5f))
                                        : T("ninguno de ellos ha estado allí"),
                                      k > 0 && dist > CAMP_RADIUS_M * 0.5f, 300 + s);
            if (o && k) snprintf(o->badge, sizeof(o->badge), "%d%%", (int)(risk * 100.0f + 0.5f));
        }
        if (!ns) snprintf(d->text, sizeof(d->text), "%s", T("No hay campamentos ni sitios marcados (M marca un sitio en el mapa)."));
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    case TV_MSG_WHO:
        dlg_begin(d, T("Órdenes a la escolta"), ICON_MENSAJERO, T("¿Quién lleva el mensaje? Tiene que conocer el campamento."));
        for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
            const Member *m = &troop->members[i];
            if (!ga->npcs[i].escort || m->status != STATUS_ACTIVE || m->health.down) continue;
            dlg_option(d, ICON_PERSONA, m->name, T(role_name(m->role)), true, 100 + m->id);
        }
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    case TV_MSG_WHERE: {
        int mi = member_index(troop, ga->talk_a);
        dlg_begin(d, T("Órdenes a la escolta"), ICON_MENSAJERO, T("¿A qué campamento va? El guardián mandará a los que estén libres."));
        for (int s = 0; s < ns; s++) {
            if (!all[s].camp) continue;
            bool knows = mi >= 0 && site_known(troop->members[mi].known, all[s].id);
            int idle = 0;
            for (int i = 0; i < troop->count && i < TROOP_MAX; i++)
                idle += troop->members[i].status == STATUS_ACTIVE && troop->members[i].camp == all[s].id && !ga->npcs[i].escort &&
                        troop->members[i].id != ga->camps[all[s].id].guardian && !trv_away(&troop->members[i]);
            float dist = dist_xz(p->pos.x, p->pos.z, all[s].x, all[s].z);
            dlg_option(d, ICON_CAMPAMENTO, all[s].name,
                       knows ? TextFormat(T("%.0f m · ida y vuelta ~%.0f s · %d libres allí"), dist, 2.0f * journey_eta(dist, mode_now(ga)), idle)
                             : T("no conoce ese campamento"),
                       knows && idle > 0 && dist > CAMP_RADIUS_M * 0.5f, 300 + s);
        }
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    }
    case TV_MSG_COUNT:
        dlg_begin(d, T("Órdenes a la escolta"), ICON_MENSAJERO, T("¿Cuántos refuerzos pides?"));
        for (int c = 1; c <= 4; c++) {
            DlgOption *o = dlg_option(d, ICON_TRIBU, TextFormat(T("%d refuerzos"), c), "", true, c);
            if (o) snprintf(o->badge, sizeof(o->badge), "%d", c);
        }
        dlg_option(d, ICON_TITULO, back, "", true, 0);
        break;
    default: dlg_close(d); break;
    }
}

static void choose(GameActions *ga, Troop *troop, const MemoryMap *mem, const Player *p, int v, char *log, size_t len) {
    Site all[CAMPS_MAX + 32];
    int ns = sites(ga, mem, all, CAMPS_MAX + 32);
    switch (ga->talk_mode) {
    case TV_ROOT:
        if (v == 0) dlg_close(&ga->dlg);
        else if (v == 1) ga->travel_npick = 0, set_mode(ga, TV_WHO);
        else set_mode(ga, TV_MSG_WHO);
        break;
    case TV_WHO:
        if (v == 0) set_mode(ga, TV_ROOT);
        else if (v == 1) set_mode(ga, TV_WHERE);
        else toggle_pick(ga, v - 100);
        break;
    case TV_WHERE: {
        if (v == 0) {
            set_mode(ga, TV_WHO);
            break;
        }
        int s = v - 300;
        if (s < 0 || s >= ns) break;
        int k = knowers(troop, ga->travel_pick, ga->travel_npick, all[s].id);
        int j = journey_start(ga->journeys, JOURNEYS_MAX, JOURNEY_DISPATCH, ga->travel_pick, ga->travel_npick, all[s].id, p->pos.x, p->pos.z,
                              all[s].x, all[s].z, mode_now(ga), (float)k / (float)ga->travel_npick);
        if (j < 0) {
            snprintf(log, len, "%s", T("Ya hay demasiadas partidas de camino."));
        } else {
            depart(ga, troop, j);
            snprintf(log, len, T("%d parten hacia %s y se pierden en el horizonte."), ga->travel_npick, all[s].name);
        }
        ga->travel_npick = 0;
        dlg_close(&ga->dlg);
        break;
    }
    case TV_MSG_WHO:
        if (v == 0) set_mode(ga, TV_ROOT);
        else ga->talk_a = v - 100, set_mode(ga, TV_MSG_WHERE);
        break;
    case TV_MSG_WHERE:
        if (v == 0) set_mode(ga, TV_MSG_WHO);
        else ga->talk_b = v - 300, set_mode(ga, TV_MSG_COUNT);
        break;
    case TV_MSG_COUNT: {
        if (v == 0) {
            set_mode(ga, TV_MSG_WHERE);
            break;
        }
        int s = ga->talk_b;
        if (s < 0 || s >= ns) break;
        int who = ga->talk_a;
        int j = journey_start(ga->journeys, JOURNEYS_MAX, JOURNEY_MESSENGER, &who, 1, all[s].id, p->pos.x, p->pos.z, all[s].x, all[s].z,
                              mode_now(ga), 1.0f);
        int mi = member_index(troop, who);
        if (j >= 0 && mi >= 0) {
            ga->journeys[j].request = v;
            depart(ga, troop, j);
            snprintf(log, len, T("%s parte con el mensaje hacia %s: volverá con %d más."), troop->members[mi].name, all[s].name, v);
        }
        dlg_close(&ga->dlg);
        break;
    }
    default: dlg_close(&ga->dlg); break;
    }
}

// ---------------------------------------------------------------- viajes
static void spawn_at(GameActions *ga, const Terrain *t, int mi, float x, float z, int k) {
    float a = (float)mi * 2.39996f, r = 2.0f + (float)(k % 3);
    Npc *n = &ga->npcs[mi];
    n->pos = (Vector3){ x + cosf(a) * r, 0, z + sinf(a) * r };
    n->pos.y = terrain_height(t, n->pos.x, n->pos.z);
    n->home = n->pos;
    n->leave_t = 0.0f;
    n->home_camp = 0; // que sync_npcs le calcule la casa de nuevo
}

static void apply_fates(GameActions *ga, Troop *troop, Journey *jr, Rng *rng) {
    for (int i = 0; i < jr->n; i++) {
        int mi = member_index(troop, jr->people[i]);
        if (mi < 0) continue;
        Member *m = &troop->members[mi];
        if (jr->fate[i] == FATE_DEAD) {
            m->journey = 0;
            troop_mourn(troop, m->id, 3.0f);
            continue;
        }
        if (jr->fate[i] == FATE_WOUNDED) health_hit(&m->health, rng, 25.0f + 20.0f * rng_float(rng), WOUND_CUT, PART_RANDOM);
        m->known = site_learn(m->known, jr->site);
    }
    (void)ga;
}

static void tick_journeys(GameActions *ga, Troop *troop, const MemoryMap *mem, const Player *p, const Terrain *t, bool night, float dt,
                          char *log, size_t len) {
    static Rng rng;
    static bool seeded;
    if (!seeded) rng_seed(&rng, 0x7A5E1u), seeded = true;
    for (int j = 0; j < JOURNEYS_MAX; j++) {
        Journey *jr = &ga->journeys[j];
        if (!jr->used) continue;
        jr->t += dt;
        Site dest;
        bool has_dest = site_of(ga, mem, jr->site, &dest);
        if (!has_dest) dest = (Site){ jr->site, jr->x1, jr->z1, "", false };
        if (!jr->arrived && jr->t >= jr->eta) { // llegaron (o no)
            jr->arrived = true;
            int alive = journey_resolve(jr, &rng, night);
            apply_fates(ga, troop, jr, &rng);
            if (jr->kind == JOURNEY_DISPATCH) {
                int k = 0;
                for (int i = 0; i < jr->n; i++) {
                    int mi = member_index(troop, jr->people[i]);
                    if (mi < 0 || jr->fate[i] == FATE_DEAD) continue;
                    Member *m = &troop->members[mi];
                    m->journey = 0;
                    m->camp = dest.camp && jr->site < CAMPS_MAX && ga->camps[jr->site].used ? jr->site : -1;
                    spawn_at(ga, t, mi, dest.x, dest.z, k++);
                }
            } else if (jr->kind == JOURNEY_MESSENGER) {
                int mi = member_index(troop, jr->people[0]);
                if (alive && mi >= 0 && jr->site < CAMPS_MAX && ga->camps[jr->site].used) {
                    // El guardian manda a los que esten libres; vuelven con el mensajero hacia el jugador.
                    int group[JOURNEY_PEOPLE] = { jr->people[0] }, n = 1;
                    for (int i = 0; i < troop->count && i < TROOP_MAX && n <= jr->request && n < JOURNEY_PEOPLE; i++) {
                        Member *m = &troop->members[i];
                        if (m->status != STATUS_ACTIVE || m->camp != jr->site || ga->npcs[i].escort || trv_away(m) ||
                            m->id == ga->camps[jr->site].guardian || m->health.down)
                            continue;
                        group[n++] = m->id;
                    }
                    int back = journey_start(ga->journeys, JOURNEYS_MAX, JOURNEY_REINFORCE, group, n, jr->site, dest.x, dest.z, p->pos.x, p->pos.z,
                                             jr->mode, 1.0f);
                    if (back >= 0) {
                        Journey *b = &ga->journeys[back];
                        for (int i = 0; i < b->n; i++) {
                            int k = member_index(troop, b->people[i]);
                            if (k >= 0) troop->members[k].journey = back + 1, troop->members[k].camp = -1, ga->npcs[k].leave_t = LEAVE_SECONDS;
                        }
                    }
                    jr->used = false; // el viaje de ida termina: sigue el de vuelta
                    continue;
                }
                if (mi >= 0) troop->members[mi].journey = 0;
            } else { // refuerzos: llegan junto al jugador, donde este ahora
                int ok = 0, hurt = 0, k = 0;
                for (int i = 0; i < jr->n; i++) {
                    int mi = member_index(troop, jr->people[i]);
                    if (mi < 0 || jr->fate[i] == FATE_DEAD) continue;
                    Member *m = &troop->members[mi];
                    m->journey = 0;
                    m->camp = -1;
                    spawn_at(ga, t, mi, p->pos.x - sinf(p->yaw) * 6.0f, p->pos.z - cosf(p->yaw) * 6.0f, k++);
                    ga->npcs[mi].escort = true;
                    ok++;
                    hurt += jr->fate[i] == FATE_WOUNDED;
                }
                if (ok) snprintf(log, len, T("Llegan los refuerzos: %d (%d heridos); %d se quedaron en el camino."), ok, hurt, jr->n - ok);
                else snprintf(log, len, "%s", T("Nadie volvió con los refuerzos: el camino se los llevó."));
                jr->used = false;
                continue;
            }
        }
        // El jugador se entera de como les fue al llegar al destino.
        if (jr->arrived && jr->kind == JOURNEY_DISPATCH && dist_xz(p->pos.x, p->pos.z, dest.x, dest.z) < REPORT_RADIUS) {
            int ok = 0, hurt = 0;
            for (int i = 0; i < jr->n; i++) ok += jr->fate[i] != FATE_DEAD, hurt += jr->fate[i] == FATE_WOUNDED;
            if (ok == jr->n && !hurt) snprintf(log, len, T("En %s: los %d despachados llegaron bien."), has_dest ? dest.name : "?", ok);
            else snprintf(log, len, T("En %s te enteras: llegaron %d de %d (%d heridos)."), has_dest ? dest.name : "?", ok, jr->n, hurt);
            jr->used = false;
        }
        // El mensajero que no llego: al pasar el doble del tiempo, se sabe.
        if (jr->arrived && jr->kind == JOURNEY_MESSENGER && jr->t > jr->eta * 2.2f) {
            snprintf(log, len, T("Tu mensajero a %s no vuelve. Algo le pasó en el camino."), has_dest ? dest.name : "?");
            jr->used = false;
        }
    }
}

// La gente conoce los sitios por donde pasa (y el campamento donde vive).
static void learn_sites(GameActions *ga, Troop *troop, const MemoryMap *mem) {
    Site all[CAMPS_MAX + 32];
    int ns = sites(ga, mem, all, CAMPS_MAX + 32);
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
        Member *m = &troop->members[i];
        if (m->status != STATUS_ACTIVE || trv_away(m) || ga->npcs[i].member_id != m->id) continue;
        if (m->camp >= 0 && m->camp < CAMPS_MAX) m->known = site_learn(m->known, SITE_CAMP(m->camp));
        for (int s = 0; s < ns; s++)
            if (dist_xz(ga->npcs[i].pos.x, ga->npcs[i].pos.z, all[s].x, all[s].z) < LEARN_RADIUS) m->known = site_learn(m->known, all[s].id);
    }
}

// Los que parten: se alejan hacia su destino y, pasado un rato, ya no se ven.
static void tick_leaving(GameActions *ga, Troop *troop, const Terrain *t, float dt) {
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
        Member *m = &troop->members[i];
        Npc *n = &ga->npcs[i];
        if (!trv_away(m) || n->leave_t >= LEAVE_SECONDS) continue;
        const Journey *jr = &ga->journeys[m->journey - 1];
        float speed = travel_speed(jr->mode) * 1.2f;
        n->leave_t += dt;
        n->pos.x += n->leave_dir.x * speed * dt;
        n->pos.z += n->leave_dir.z * speed * dt;
        n->pos.y = terrain_height(t, n->pos.x, n->pos.z) + (jr->mode == TRAVEL_MOUNTED ? 1.1f : 0.0f); // a caballo, encima
        n->yaw = atan2f(n->leave_dir.x, n->leave_dir.z);
        n->moving = true;
    }
}

void trv_update(GameActions *ga, Troop *troop, const MemoryMap *mem, const Player *p, const Terrain *t, bool night, bool input_ok,
                float dt, char *log, size_t len) {
    static float learn_t = 0.0f;
    if ((learn_t += dt) > 2.0f) learn_t = 0.0f, learn_sites(ga, troop, mem);
    tick_leaving(ga, troop, t, dt);
    tick_journeys(ga, troop, mem, p, t, night, dt, log, len);
    if (input_ok && !ga->dlg.open && input_action_pressed(KA_DISPATCH)) {
        ga->talk_mode = TV_ROOT;
        ga->talk_member = -1;
        ga->dlg.cursor = 0;
        ga->dlg.open = true;
    }
    if (!ga->dlg.open || ga->talk_mode < TV_ROOT) return;
    build_dialog(ga, troop, mem, p, night);
    int v = dlg_update(&ga->dlg);
    if (v == DLG_BACK) {
        if (ga->talk_mode == TV_ROOT) dlg_close(&ga->dlg);
        else set_mode(ga, ga->talk_mode == TV_WHERE ? TV_WHO : ga->talk_mode == TV_MSG_WHERE ? TV_MSG_WHO : ga->talk_mode == TV_MSG_COUNT ? TV_MSG_WHERE : TV_ROOT);
    } else if (v >= 0) {
        choose(ga, troop, mem, p, v, log, len);
    }
    if (ga->dlg.open) build_dialog(ga, troop, mem, p, night);
}

void trv_draw_world(const GameActions *ga, const Troop *troop, float time) {
    // Los despachados a caballo: el caballo bajo cada uno mientras se alejan.
    for (int i = 0; i < troop->count && i < TROOP_MAX; i++) {
        const Member *m = &troop->members[i];
        const Npc *n = &ga->npcs[i];
        if (!trv_away(m) || n->leave_t >= LEAVE_SECONDS || ga->journeys[m->journey - 1].mode != TRAVEL_MOUNTED) continue;
        cb_draw_horse((Vector3){ n->pos.x, n->pos.y - 1.1f, n->pos.z }, n->yaw, 8.0f, time + (float)i, false);
    }
}
