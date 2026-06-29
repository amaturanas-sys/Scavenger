"""Pruebas del selector de presupuesto (modos none / min_cost / target)."""
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from backend import config
from backend.database import Base
from backend.models import User
from backend.nutrition import compute_requirements
from backend.optimizer import FoodInput, OptimizeOptions, optimize
from backend.providers.local import LocalDatasetProvider
from backend.seed import seed_foods
from backend.services import generate_daily_plan


def _inputs():
    out = []
    for r in LocalDatasetProvider(config.SEED_FOODS_PATH).fetch_foods():
        out.append(FoodInput(
            id=r.id, name=r.name, category=r.category, price_per_g=r.price_clp / r.package_g,
            serving_g=r.serving_g, max_servings_day=r.max_servings_day,
            satiety_index=r.satiety_index, kcal=r.kcal, protein_g=r.protein_g,
            carb_g=r.carb_g, fat_g=r.fat_g, fiber_g=r.fiber_g, sodium_mg=r.sodium_mg,
            calcium_mg=r.calcium_mg, iron_mg=r.iron_mg, potassium_mg=r.potassium_mg,
            vitamin_c_mg=r.vitamin_c_mg, brand=r.brand,
        ))
    return out


def test_target_mode_uses_budget_for_more_satiety():
    inputs = _inputs()
    req = compute_requirements("M", 30, 80, 180, "moderado", "mantener")
    base = optimize(inputs, req)  # mas economico, sin tope
    budget = base.totals["cost_clp"] * 1.8

    target = optimize(inputs, req, opts=OptimizeOptions(max_budget_clp=budget, objective="satiety"))
    assert target.feasible
    # No se pasa del presupuesto y gana saciedad respecto a lo mas economico.
    assert target.totals["cost_clp"] <= budget + 1.0
    assert target.totals["satiety"] >= base.totals["satiety"] - 1e-6


def test_min_cost_mode_respects_cap_and_is_cheaper_than_target():
    inputs = _inputs()
    req = compute_requirements("M", 30, 80, 180, "moderado", "mantener")
    base = optimize(inputs, req)
    budget = base.totals["cost_clp"] * 1.8

    mincost = optimize(inputs, req, opts=OptimizeOptions(max_budget_clp=budget, objective="cost"))
    target = optimize(inputs, req, opts=OptimizeOptions(max_budget_clp=budget, objective="satiety"))
    assert mincost.totals["cost_clp"] <= budget + 1.0
    # Minimizar costo gasta <= que aprovechar el presupuesto.
    assert mincost.totals["cost_clp"] <= target.totals["cost_clp"] + 1.0


def _db_with_user(budget=6000):
    engine = create_engine("sqlite:///:memory:", connect_args={"check_same_thread": False})
    Base.metadata.create_all(engine)
    db = sessionmaker(bind=engine)()
    seed_foods(db, "local", refresh=False)
    u = User(name="B", sex="M", age=30, weight_kg=80, height_cm=180,
             activity_level="moderado", goal="mantener", daily_budget_clp=budget,
             preferred_retailers=[])
    db.add(u)
    db.commit()
    return db, u


def test_service_budget_modes_and_flags():
    db, u = _db_with_user(budget=6000)

    none = generate_daily_plan(db, u, budget_mode="none")
    mincost = generate_daily_plan(db, u, budget_mode="min_cost")
    target = generate_daily_plan(db, u, budget_mode="target")

    for plan in (mincost, target):
        assert plan["totals"]["cost_clp"] <= u.daily_budget_clp + 1.0
        assert plan["over_budget"] is False
    # 'none' no marca over_budget aunque no haya tope.
    assert none["over_budget"] is False
    # Aprovechar el presupuesto da >= saciedad que minimizar costo.
    assert target["totals"]["satiety"] >= mincost["totals"]["satiety"] - 1e-6
    # El presupuesto seleccionado puede sobrescribir al del perfil.
    custom = generate_daily_plan(db, u, budget_mode="min_cost", budget_clp=3000)
    assert custom["budget_clp"] == 3000


def test_low_budget_fallback_minimizes_cost_not_satiety():
    """B2: con presupuesto imposible, el último recurso minimiza costo aunque el
    modo sea 'target' (saciedad); no debe maximizar saciedad sin techo."""
    inputs = _inputs()
    req = compute_requirements("M", 30, 80, 180, "moderado", "mantener")
    tiny = 1.0  # 1 peso: imposible cumplir kcal -> fuerza el último recurso (sin tope)

    target_low = optimize(inputs, req, opts=OptimizeOptions(max_budget_clp=tiny, objective="satiety"))
    cost_low = optimize(inputs, req, opts=OptimizeOptions(max_budget_clp=tiny, objective="cost"))
    # Comportamiento viejo del fallback: maximizar saciedad SIN techo (mucho más caro).
    unbounded_sat = optimize(inputs, req, opts=OptimizeOptions(max_budget_clp=None, objective="satiety"))

    assert target_low.feasible and cost_low.feasible
    # Al soltar el tope, 'target' cae a minimizar costo: cuesta lo mismo que min_cost.
    assert abs(target_low.totals["cost_clp"] - cost_low.totals["cost_clp"]) <= 1.0
    # ...y mucho menos que maximizar saciedad sin techo (el bug que se corrige).
    assert target_low.totals["cost_clp"] < unbounded_sat.totals["cost_clp"]
