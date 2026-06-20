"""Capa de servicios: orquesta requerimientos, optimizacion y armado de minutas."""
from __future__ import annotations

from sqlalchemy.orm import Session

from . import config
from .learning import get_preferences
from .models import Food, User
from .nutrition import Requirements, compute_requirements
from .optimizer import FoodInput, OptimizeOptions, optimize
from .planner import distribute_into_meals


def user_requirements(user: User) -> Requirements:
    """Calcula los requerimientos diarios del usuario."""
    return compute_requirements(
        sex=user.sex, age=user.age, weight_kg=user.weight_kg,
        height_cm=user.height_cm, activity_level=user.activity_level, goal=user.goal,
    )


# Que etiquetas de un alimento satisfacen cada restriccion dietetica.
# Lo vegano tambien es apto para vegetarianos (jerarquia de dietas).
DIET_SATISFIERS = {
    "vegano": {"vegano"},
    "vegetariano": {"vegano", "vegetariano"},
    "sin_gluten": {"sin_gluten"},
}


def _food_satisfies_diet(food_tags: set[str], diet_tags: set[str]) -> bool:
    """True si el alimento cumple todas las restricciones dieteticas requeridas."""
    for req in diet_tags:
        satisfiers = DIET_SATISFIERS.get(req, {req})
        if not (food_tags & satisfiers):
            return False
    return True


def _eligible_foods(db: Session, user: User) -> list[Food]:
    """Filtra el catalogo segun restricciones dieteticas y exclusiones."""
    foods = db.query(Food).all()
    diet_tags = set(user.diet_tags or [])
    excluded = set(user.excluded_foods or [])
    result = []
    for f in foods:
        if f.id in excluded or f.category in excluded:
            continue
        if diet_tags and not _food_satisfies_diet(set(f.tags or []), diet_tags):
            continue
        result.append(f)
    return result


def build_options(
    user: User,
    satiety_emphasis: float = 0.0,
    use_budget: bool = False,
    kcal_tolerance: float | None = None,
) -> OptimizeOptions:
    return OptimizeOptions(
        kcal_tolerance=kcal_tolerance if kcal_tolerance is not None else config.DEFAULT_KCAL_TOLERANCE,
        preference_weight=config.PREFERENCE_WEIGHT,
        satiety_emphasis=satiety_emphasis,
        max_budget_clp=user.daily_budget_clp if use_budget else None,
    )


def generate_daily_plan(
    db: Session,
    user: User,
    satiety_emphasis: float = 0.0,
    use_budget: bool = False,
    extra_excluded: set[str] | None = None,
) -> dict:
    """Genera una minuta diaria optimizada para el usuario."""
    req = user_requirements(user)
    foods = _eligible_foods(db, user)
    if extra_excluded:
        foods = [f for f in foods if f.id not in extra_excluded]
    inputs = [FoodInput.from_orm(f) for f in foods]
    prefs = get_preferences(db, user.id) if user.id else {}
    opts = build_options(user, satiety_emphasis=satiety_emphasis, use_budget=use_budget)

    result = optimize(inputs, req, preferences=prefs, opts=opts)
    plan = distribute_into_meals(result)
    plan["requirements"] = req.to_dict()
    plan["budget_clp"] = user.daily_budget_clp
    plan["over_budget"] = plan["totals"].get("cost_clp", 0) > user.daily_budget_clp
    return plan


def generate_weekly_plan(
    db: Session,
    user: User,
    satiety_emphasis: float = 0.0,
    use_budget: bool = False,
) -> dict:
    """Genera 7 minutas diarias con variedad rotando alimentos protagonistas."""
    days = ["lunes", "martes", "miercoles", "jueves", "viernes", "sabado", "domingo"]
    daily_plans = []
    rotation: set[str] = set()
    total_cost = 0.0

    for day in days:
        plan = generate_daily_plan(
            db, user, satiety_emphasis=satiety_emphasis,
            use_budget=use_budget, extra_excluded=rotation,
        )
        # Para dar variedad, excluye el item proteico/caloricamente dominante
        # del dia siguiente (rotacion simple, acotada para no quedar sin opciones).
        items = sorted(
            (it for meal in plan["meals"] for it in meal["items"]),
            key=lambda it: it["kcal"], reverse=True,
        )
        for it in items[:1]:
            rotation.add(it["food_id"])
        if len(rotation) > 3:
            rotation = set(list(rotation)[-3:])

        daily_plans.append({"day": day, "plan": plan})
        total_cost += plan["totals"].get("cost_clp", 0.0)

    return {
        "days": daily_plans,
        "weekly_cost_clp": round(total_cost, 1),
        "avg_daily_cost_clp": round(total_cost / len(days), 1),
        "requirements": user_requirements(user).to_dict(),
    }
