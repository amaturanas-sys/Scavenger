#include "sim/anim_index.h"

#include "sim/melee.h"

const char *anim_for_action(ActionId a) {
    switch (a) {
    case ACTION_TAKE: return "tomar";
    case ACTION_THROW: return "lanzar";
    case ACTION_CHANGE_GRIP: return "cambiar_empunadura";
    case ACTION_SHEATHE: return "enfundar";
    case ACTION_PLACE_FIRE:
    case ACTION_PLACE_TENT: return "agacharse_trabajar";
    case ACTION_DIG_TRENCH: return "cavar";
    case ACTION_THROW_GRAPPLE: return "lanzar_trepa";
    case ACTION_THROW_LASSO: return "lanzar_lazo";
    case ACTION_LIGHT_TORCH: return "encender";
    case ACTION_SADDLE: return "ensillar";
    case ACTION_FILL_WATER:
    case ACTION_BOIL: return "agacharse_trabajar";
    default: return ANIM_IDLE;
    }
}

const char *anim_grip(Grip g, bool sheathed) {
    if (sheathed) return ANIM_IDLE;
    switch (g) {
    case GRIP_ONE_HANDED: return "empunar_una";
    case GRIP_DUAL: return "empunar_doble";
    case GRIP_TWO_HANDED: return "empunar_dos_manos";
    case GRIP_WEAPON_SHIELD:
    case GRIP_SHIELD: return "empunar_escudo";
    default: return ANIM_IDLE;
    }
}

const char *anim_humanoid(const HumanoidState *s) {
    // De mayor a menor prioridad: lo que el cuerpo no puede dejar de hacer primero.
    if (s->dead) return "morir";
    if (s->down) return "abatido";
    if (s->knocked) return "derribado";
    if (s->held) return "rehen";
    if (s->hit) return "recibir_golpe";
    if (s->climbing) return s->climb_top ? "trepar_cima" : "trepar_cuerda";
    if (s->mounted) return s->mount_speed > 6.0f ? "jinete_galope" : s->mount_speed > 0.3f ? "jinete_paso" : "jinete_idle";
    if (!s->grounded) return "caer";
    if (s->doing >= 0) return anim_for_action((ActionId)s->doing);
    if (s->move > 1 && s->move <= MOVE_COUNT) return move_def((MeleeMove)(s->move - 1))->clip; // patada, escudo, agarre...
    if (s->attacking > 0) {
        static const char *one[] = { "ataque_una_1", "ataque_una_2", "ataque_una_3" };
        static const char *two[] = { "ataque_dos_1", "ataque_dos_2", "ataque_dos_3" };
        int k = (s->attacking - 1) % 3;
        if (s->spear) return "estocada_lanza";
        return s->grip == GRIP_TWO_HANDED ? two[k] : one[k];
    }
    if (s->swapping) return "cambiar_mano";
    if (s->holding) return "sujetar_rehen";
    if (s->ranged) return s->ranged == 2 ? "disparar_ballesta" : s->ranged == 3 ? "disparar_mosquete" : "disparar_arco";
    if (s->blocking) return "bloquear";
    if (s->hidden && !s->moving) return "acechar_idle";
    if (s->moving) {
        if (s->carrying) return "llevar";
        if (s->crouching) return "acechar"; // agachado (con sigilo, el mismo clip mas lento)
        if (s->limping) return "cojear";
        return s->running ? "correr" : "caminar"; // en sigilo de pie: caminar, mas despacio
    }
    if (s->forging) return "forjar";
    if (s->building >= 0) return "construir";
    if (s->carrying) return "llevar";
    if (s->crouching) return "acechar_idle";
    return anim_grip(s->grip, s->sheathed);
}

const char *anim_quadruped_fight(float speed, bool attacking, bool hit, bool dead) {
    if (dead) return "morir";
    if (hit) return "recibir_golpe";
    if (attacking) return "atacar";
    if (speed > 4.5f) return "galopar";
    if (speed > 2.0f) return "trotar";
    if (speed > 0.2f) return "caminar";
    return ANIM_IDLE;
}

const char *anim_quadruped(const Animal *a) {
    const SpeciesDef *d = species_def(a->species);
    if (a->state == ANIMAL_DEAD) return "morir";
    if (a->hit_anim > 0.0f) return "recibir_golpe";
    if (a->attack_anim > 0.0f) return "atacar";
    if (a->state == ANIMAL_BOUND) return "forcejear_lazo";
    if (a->ridden) return a->speed > 6.0f ? "galopar" : a->speed > 0.3f ? "caminar" : "montado_idle";
    if (a->mode == MODE_STALK) return "acechar";
    if (a->fleeing || a->speed > 4.5f) return "galopar";
    if (a->speed > 2.0f) return "trotar";
    if (a->speed > 0.2f) return "caminar";
    if (a->mode == MODE_EAT) return "comer_presa";
    if (a->mode == MODE_DRINK) return "pastar"; // la cabeza baja al agua
    // Quieto: los herbivoros pastan, los cazadores esperan.
    return d->hunt_max > 0.0f || d->cls == CLASS_HOSTILE ? ANIM_IDLE : "pastar";
}

const char *anim_bird(const Animal *a) {
    if (a->state == ANIMAL_DEAD) return "morir";
    if (a->attack_anim > 0.0f || (a->mode == MODE_CHASE && a->alt < 3.0f)) return "atacar_picado";
    if (a->alt < 0.4f) return a->speed > 0.3f ? "aterrizar" : "posado";
    if (a->alt < 2.0f) return "despegar";
    // En el aire: aletea al ir deprisa (y a ratos); si no, planea.
    return a->speed > 6.0f || ((int)(a->clock / 3.0f) % 3 == 0) ? "volar" : "planear";
}

const char *anim_animal(const Animal *a) { return species_def(a->species)->flier ? anim_bird(a) : anim_quadruped(a); }
