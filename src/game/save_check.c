// Pruebas de las partidas guardadas: un resumen legible del estado (valores clave de cada
// bloque) y las partidas de referencia de tests/partidas, que toda version nueva tiene que
// seguir cargando igual (docs/PARTIDAS.md).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/death_game.h"
#include "game/hud_game.h"
#include "game/save_game.h"
#include "sim/save_format.h"

static void sum_health(FILE *o, const Health *h) {
    fprintf(o, "vida %.3f/%.3f sangre %.3f veneno %.3f heridas %d%s%s", h->hp, h->hp_max, h->blood, h->venom, h->wound_count,
            h->down ? " abatido" : "", h->dead ? " muerto" : "");
    for (int i = 0; i < h->wound_count && i < WOUNDS_MAX; i++)
        fprintf(o, " [%d %d %.3f%s%s]", (int)h->wounds[i].kind, (int)h->wounds[i].part, h->wounds[i].severity,
                h->wounds[i].bleeding ? " sangra" : "", h->wounds[i].treated ? " vendada" : "");
}

static void sum_armor(FILE *o, const Armor *a) {
    for (int i = 0; i < SLOT_COUNT; i++)
        if (a->slot[i].id[0]) fprintf(o, " %s %.3f", a->slot[i].id, a->slot[i].durability);
}

static void sum_bag(FILE *o, const char *name, const Bag *b) {
    fprintf(o, "  %s: tipo %d carga %.2f casillas %d n %d:", name, (int)b->kind, b->cap_kg, b->slots, b->n);
    for (int i = 0; i < b->n && i < BAG_SLOTS; i++) fprintf(o, " %s x%d %.3f", b->s[i].id, b->s[i].count, b->s[i].condition);
    fputc('\n', o);
}

static int page_order(const void *a, const void *b) {
    const MemoryPage *p = *(const MemoryPage *const *)a, *q = *(const MemoryPage *const *)b;
    return p->px != q->px ? (p->px < q->px ? -1 : 1) : p->pz != q->pz ? (p->pz < q->pz ? -1 : 1) : 0;
}

void save_summary(const GameState *g, FILE *o) {
    fprintf(o, "partida: dia %d tiempo %.3f campeon %d camara %.4f rng %u\n", *g->day, *g->world_time, *g->last_champion, *g->cam_yaw,
            (unsigned)g->rng->state);
    const Player *p = g->player;
    fprintf(o, "jugador: (%.3f, %.3f, %.3f) rumbo %.4f vy %.3f postura %d sigilo %d suelo %d escala %.3f alzado %.3f\n", p->pos.x, p->pos.y,
            p->pos.z, p->yaw, p->vy, (int)p->stance, p->sneaking, p->grounded, p->speed_scale, p->draw_lift);
    fprintf(o, "  agachado %d\n", p->crouching); // v0.5.0
    fprintf(o, "reino: %s relacion %.3f\n", g->overlord->name, g->overlord->relation);

    const Troop *t = g->troop;
    fprintf(o, "tropa: %d integrantes, proximo id %d, %d campeones, rebelion %.3f\n", t->count, t->next_id, t->champion_count, t->rebellion_scale);
    for (int i = 0; i < t->count && i < TROOP_MAX; i++) {
        const Member *m = &t->members[i];
        fprintf(o, "  %d %s funcion %d estado %d rasgos %u moral %.3f lealtad %.3f campeon %d campamento %d mochila %d viaje %d conoce %llx ",
                m->id, m->name, (int)m->role, (int)m->status, m->traits, m->morale, m->loyalty, m->champion, m->camp, m->pack, m->journey,
                (unsigned long long)m->known);
        sum_health(o, &m->health);
        sum_armor(o, &m->armor);
        fputc('\n', o);
        if (m->bag.n) sum_bag(o, "  bolsa", &m->bag);
    }
    for (int i = 0; i < t->champion_count && i < TROOP_MAX; i++) fprintf(o, "  campeon %s, %s\n", t->champions[i].name, t->champions[i].epithet);

    const GameActions *ga = g->ga;
    fprintf(o, "acciones: rng %u preset %d antorcha %d haciendo %d (%.3f) objetivo %d aqui %d montado %d grupo %d forja %d (%.3f)\n",
            (unsigned)ga->rng.state, ga->preset, ga->torch_lit, ga->doing, ga->timer, ga->target, ga->here, ga->mounted, ga->next_group,
            ga->crafting, ga->craft_timer);
    fprintf(o, "  manos: [%s] [%s] enfundado %d lleva [%s]\n", ga->hands.right.id, ga->hands.left.id, ga->hands.sheathed, ga->hands.carried);
    for (int k = 0; k < CAMPS_MAX; k++) {
        const CampSite *c = &ga->camps[k];
        if (!c->used) continue;
        fprintf(o, "  campamento %d %s (%.2f, %.2f) guardian %d fundado %d tareas %d acopio %d:", k, c->name, c->x, c->z, c->guardian,
                c->founded_day, c->ntask, c->stock.n);
        for (int i = 0; i < c->stock.n && i < STOCK_MAX; i++) fprintf(o, " %s x%d", c->stock.e[i].id, c->stock.e[i].count);
        fputc('\n', o);
    }
    for (int i = 0; i < ga->project_count && i < GA_MAX_PROJECTS; i++)
        fprintf(o, "  obra %d (%.2f, %.2f) avance %.3f%s\n", (int)ga->projects[i].def, ga->projects[i].x, ga->projects[i].z,
                ga->projects[i].progress, ga->projects[i].done ? " hecha" : "");
    int animals = 0;
    for (int i = 0; i < ga->animal_count && i < GA_MAX_ANIMALS; i++) {
        const Animal *a = &ga->animals[i];
        if (!a->used) continue;
        animals++;
        fprintf(o, "  animal %d especie %d estado %d modo %d (%.2f, %.2f) alto %.2f grupo %d hambre %.3f sed %.3f ", i, (int)a->species,
                (int)a->state, (int)a->mode, a->x, a->z, a->alt, a->group, a->hunger, a->thirst);
        sum_health(o, &a->h);
        fputc('\n', o);
    }
    fprintf(o, "  animales: %d en uso de %d\n", animals, ga->animal_count);
    for (int i = 0; i < TROOP_MAX; i++) {
        const Npc *n = &ga->npcs[i];
        if (n->member_id)
            fprintf(o, "  npc %d integrante %d (%.2f, %.2f) obra %d trabajo %d escolta %d casa %d\n", i, n->member_id, n->pos.x, n->pos.z,
                    n->project, n->job, n->escort, n->home_camp);
    }
    int swarms = 0;
    for (int i = 0; i < GA_MAX_SWARMS; i++) swarms += ga->swarms[i].used;
    fprintf(o, "  enjambres: %d\n", swarms);
    sum_bag(o, "bolsillos", &ga->pockets);
    sum_bag(o, "mochila", &ga->backpack);
    sum_bag(o, "carreta", &ga->cart);
    sum_bag(o, "armeria", &ga->armory);
    for (int i = 0; i < GA_PACKS; i++) {
        char name[32];
        snprintf(name, sizeof(name), "alforja %d (animal %d)", i, ga->pack_animal[i]);
        if (ga->packs[i].n) sum_bag(o, name, &ga->packs[i]);
    }
    for (int i = 0; i < GA_LOOT; i++) {
        char name[48];
        snprintf(name, sizeof(name), "botin (%.2f, %.2f)", ga->loot_pos[i].x, ga->loot_pos[i].z);
        if (ga->loot_age[i] >= 0.0f && ga->loot[i].n) sum_bag(o, name, &ga->loot[i]);
    }
    fprintf(o, "  mochila: tamano %d donde %d animal %d\n", ga->pack_size, ga->pack_where, ga->pack_anchor);
    fprintf(o, "  nivel %d xp %.3f agua %.3f ebrio %.3f maldicion %.3f region %d vistos %x %llx %x %x %llx\n", ga->prog.level, ga->prog.xp,
            ga->hydro.water, ga->hydro.drunk, ga->hydro.curse, ga->last_region, (unsigned)ga->seen_settle, (unsigned long long)ga->seen_dens,
            (unsigned)ga->seen_tribes, (unsigned)ga->raided_tribes, (unsigned long long)ga->seen_sites);
    fprintf(o, "  ropa:");
    for (int i = 0; i < WEAR_COUNT; i++) fprintf(o, " %d", ga->outfit.g[i]);
    fprintf(o, " tatuajes:");
    for (int i = 0; i < TZ_COUNT; i++) fprintf(o, " %d", ga->tattoos.node[i]);
    fprintf(o, " joyas:");
    for (int i = 0; i < JS_COUNT; i++) fprintf(o, " [%s]", ga->jewels.slot[i].id);
    fputc('\n', o);
    for (int i = 0; i < JOURNEYS_MAX; i++) {
        const Journey *j = &ga->journeys[i];
        if (j->used) fprintf(o, "  viaje %d tipo %d gente %d destino %d (%.3f de %.3f)\n", i, (int)j->kind, j->n, j->site, j->t, j->eta);
    }
    if (ga->dlg.open) fprintf(o, "  dialogo: %s, %d opciones\n", ga->dlg.speaker, ga->dlg.n);

    const Combat *cb = g->cb;
    fprintf(o, "combate: semilla %u rng %u ", cb->seed, (unsigned)cb->rng.state);
    sum_health(o, &cb->player);
    sum_armor(o, &cb->armor);
    fputc('\n', o);
    fprintf(o, "  elegido %d parry %.3f expuesto %.3f\n", cb->target_lock, cb->parry_cd, cb->exposed); // v0.5.0
    fprintf(o, "  rehen %d\n", cb->hostage); // v0.5.1
    for (int i = 0; i < CB_MAX_ENEMIES; i++) {
        const Enemy *e = &cb->enemies[i];
        if (!e->used) continue;
        fprintf(o, "  enemigo %d tipo %d estado %d (%.2f, %.2f) objetivo %d ", i, (int)e->kind, (int)e->state, e->pos.x, e->pos.z, e->target);
        sum_health(o, &e->h);
        sum_armor(o, &e->armor);
        fputc('\n', o);
        if (e->windup > 0.0f) fprintf(o, "    anuncia el golpe %d: %.3f s\n", e->windup_move, e->windup); // v0.5.0
        if (e->alert > 0.0f) fprintf(o, "    alerta %.3f s\n", e->alert);                                  // v0.5.1
        if (e->state == EN_DOWN) fprintf(o, "    abatido: despierta en %.3f s\n", e->timer);               // v0.5.1
    }
    int shots = 0;
    for (int i = 0; i < CB_MAX_SHOTS; i++) shots += cb->shots[i].p.alive || cb->shots[i].stuck;
    fprintf(o, "  proyectiles: %d, caballos sueltos %d\n", shots, cb->loose_n);

    const Hazards *hz = g->hz;
    fprintf(o, "peligros: semilla %u rng %u calor %.3f mojado %.3f sensacion %.3f trampa %d agujeros %d victima %d escolta %d\n", hz->seed,
            (unsigned)hz->rng.state, hz->warmth.heat, hz->warmth.wet, hz->feels, (int)hz->trap, hz->hole_count, hz->victim, hz->escort_on);

    const Disasters *dz = g->dz;
    int fires = 0;
    double burnt = 0.0;
    for (int i = 0; i < FIRE_MAX; i++) fires += dz->fire.cells[i].used;
    for (int i = 0; i < WORLD_MAX_TREES; i++) burnt += dz->tree_burn[i];
    fprintf(o, "desastres: rng %u fuegos %d quemaduras %d arboles %.3f\n", (unsigned)dz->rng.state, fires, dz->fire.scorch_next, burnt);

    fprintf(o, "objetos: %d\n", g->props->count);
    for (int i = 0; i < g->props->count; i++) {
        const Prop *pr = &g->props->items[i];
        fprintf(o, "  %s (%.3f, %.3f, %.3f) rumbo %.4f estado %.3f\n", pr->item->id, pr->pos.x, pr->pos.y, pr->pos.z, pr->yaw, pr->condition);
    }

    const MemoryMap *mem = g->mem;
    const MemoryPage **pages = calloc((size_t)(mem->capacity > 0 ? mem->capacity : 1), sizeof(*pages));
    int np = 0;
    for (int i = 0; pages && i < mem->capacity; i++)
        if (memmap_page_slot(mem, i)) pages[np++] = memmap_page_slot(mem, i);
    if (pages) qsort(pages, (size_t)np, sizeof(*pages), page_order);
    fprintf(o, "memoria: %d paginas, %d marcas\n", np, mem->marker_count);
    for (int i = 0; i < np; i++) {
        double light = 0.0, fam = 0.0, last = 0.0;
        for (int c = 0; c < MEMMAP_PAGE * MEMMAP_PAGE; c++)
            light += pages[i]->cells[c].light, fam += pages[i]->cells[c].familiarity, last += pages[i]->cells[c].t_last;
        fprintf(o, "  pagina (%d, %d) luz %.3f familiaridad %.3f instantes %.1f\n", pages[i]->px, pages[i]->pz, light, fam, last);
    }
    free(pages);
    for (int i = 0; i < mem->marker_count && i < MEMMAP_MAX_MARKERS; i++)
        fprintf(o, "  marca %d (%.2f, %.2f)\n", (int)mem->markers[i].kind, mem->markers[i].x, mem->markers[i].z);

    const DeathSave *d = dg_state();
    fprintf(o, "muerte: descanso %d %s (%.2f, %.2f) muertes %d restos %d proximo %d\n", d->has_rest, d->rest_name, d->rest_x, d->rest_z, d->deaths,
            d->count, d->next);
    for (int i = 0; i < d->count && i < DG_REMAINS_MAX; i++) fprintf(o, "  restos (%.2f, %.2f, %.2f)\n", d->remains[i].x, d->remains[i].y, d->remains[i].z);
    const QbSlot *qb = hud_slots();
    fprintf(o, "barra:");
    for (int k = 0; k < HUD_QUICK_SLOTS; k++) fprintf(o, " [%d %d %s]", qb[k].kind, qb[k].grip, qb[k].id);
    fputc('\n', o);
}

static char *read_text(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *s = n >= 0 ? malloc((size_t)n + 1) : NULL;
    if (s) s[fread(s, 1, (size_t)n, f)] = '\0';
    fclose(f);
    return s;
}

// El resumen como texto (malloc), pasando por un archivo: si no es el esperado, queda alli para
// compararlo.
static char *summary_text(const GameState *g, const char *path) {
    FILE *f = fopen(path, "wb");
    if (!f) return NULL;
    save_summary(g, f);
    fclose(f);
    return read_text(path);
}

static bool same_bytes(const char *a, const char *b) {
    FILE *fa = fopen(a, "rb"), *fb = fopen(b, "rb");
    bool same = fa && fb;
    while (same) {
        int x = fgetc(fa), y = fgetc(fb);
        same = x == y;
        if (x == EOF) break;
    }
    if (fa) fclose(fa);
    if (fb) fclose(fb);
    return same;
}

// Primera linea en que difieren dos textos (para el informe).
static void report_diff(const char *want, const char *got) {
    int line = 1;
    const char *a = want, *b = got;
    while (*a && *a == *b) {
        if (*a == '\n') line++, want = a + 1, got = b + 1;
        a++, b++;
    }
    printf("    linea %d\n    esperado: %.*s\n    obtenido: %.*s\n", line, (int)strcspn(want, "\n"), want, (int)strcspn(got, "\n"), got);
}

static int compare_names(const void *a, const void *b) { return strcmp(*(const char *const *)a, *(const char *const *)b); }

// Una partida de referencia: carga, su resumen es el esperado, y la ida y vuelta por el formato de
// hoy (guardar, cargar, guardar) da el mismo resumen y los mismos bytes, tambien leyendo campo por
// campo en vez de copiar los structs enteros.
static bool check_one(const char *path, GameState *g, bool rewrite) {
    char base[1024], expected[1100], got_path[1100], there[1100], back[1100], err[256];
    snprintf(base, sizeof(base), "%.*s", (int)(strlen(path) - 4), path);
    snprintf(expected, sizeof(expected), "%s.txt", base);
    snprintf(got_path, sizeof(got_path), "%s.obtenido.txt", base);
    snprintf(there, sizeof(there), "%s.ida.tmp", base);
    snprintf(back, sizeof(back), "%s.vuelta.tmp", base);
    if (!save_read_file(path, g, err, sizeof(err))) {
        printf("  FALLA: no carga (%s)\n", err);
        return false;
    }
    char *got = summary_text(g, got_path), *want = read_text(expected);
    bool ok = got != NULL;
    if (rewrite && got) {
        FILE *f = fopen(expected, "wb");
        if (f) fputs(got, f), fclose(f);
        printf("  resumen escrito en %s\n", GetFileName(expected));
    } else if (!want || !got || strcmp(want, got) != 0) {
        printf("  FALLA: el resumen no es el esperado (%s; el obtenido queda en %s)\n", GetFileName(expected), GetFileName(got_path));
        if (want && got) report_diff(want, got);
        ok = false;
    } else {
        printf("  ok: carga y su resumen es el esperado\n");
    }
    if (ok) {
        bool trip = save_write_file(there, g, 0, err, sizeof(err)) && save_read_file(there, g, err, sizeof(err));
        char *again = trip ? summary_text(g, got_path) : NULL;
        trip = again && !strcmp(again, got) && save_write_file(back, g, 0, err, sizeof(err)) && same_bytes(there, back);
        free(again);
        printf("  %s ida y vuelta (guardar, cargar, guardar)\n", trip ? "ok:" : "FALLA:");
        sf_set_slow(true);
        bool slow = save_read_file(path, g, err, sizeof(err));
        sf_set_slow(false);
        char *fields = slow ? summary_text(g, got_path) : NULL;
        slow = fields && !strcmp(fields, got) && save_write_file(back, g, 0, err, sizeof(err)) && same_bytes(there, back);
        free(fields);
        printf("  %s campo por campo, lo mismo que de una vez\n", slow ? "ok:" : "FALLA:");
        ok = trip && slow;
        remove(there), remove(back);
    }
    if (ok) remove(got_path);
    free(got), free(want);
    return ok;
}

int save_selftest(const char *dir, GameState *g, bool rewrite) {
    FilePathList files = LoadDirectoryFilesEx(dir, ".sav", false);
    qsort(files.paths, files.count, sizeof(files.paths[0]), compare_names);
    int fails = 0;
    for (unsigned i = 0; i < files.count; i++) {
        printf("%s\n", GetFileName(files.paths[i]));
        if (!check_one(files.paths[i], g, rewrite)) fails++;
    }
    if (!files.count) printf("no hay partidas de referencia en %s\n", dir), fails++;
    UnloadDirectoryFiles(files);
    printf("%s\n", fails ? "PARTIDAS: FALLAN" : "PARTIDAS: OK");
    return fails;
}
