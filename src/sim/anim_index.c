#include "sim/anim_index.h"

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
    if (s->climbing) return s->climb_top ? "trepar_cima" : "trepar_cuerda";
    if (s->mounted) return s->mount_speed > 6.0f ? "jinete_galope" : s->mount_speed > 0.3f ? "jinete_paso" : "jinete_idle";
    if (!s->grounded) return "caer";
    if (s->doing >= 0) return anim_for_action((ActionId)s->doing);
    if (s->hidden) return "acechar_idle";
    if (s->moving) {
        if (s->carrying) return "llevar";
        if (s->sneaking) return "acechar";
        return s->running ? "correr" : "caminar";
    }
    if (s->forging) return "forjar";
    if (s->building >= 0) return "construir";
    if (s->carrying) return "llevar";
    if (s->sneaking) return "acechar_idle";
    return anim_grip(s->grip, s->sheathed);
}

const char *anim_quadruped(const Animal *a) {
    if (a->ridden) return a->speed > 6.0f ? "galopar" : a->speed > 0.3f ? "caminar" : "montado_idle";
    if (a->fleeing || a->speed > 4.5f) return "galopar";
    if (a->speed > 2.0f) return "trotar";
    if (a->speed > 0.2f) return "caminar";
    // Quieto: los herbivoros pastan, el resto espera.
    return a->species == SPECIES_WOLF ? ANIM_IDLE : "pastar";
}
