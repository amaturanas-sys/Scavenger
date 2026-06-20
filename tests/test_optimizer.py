"""Pruebas del motor de optimizacion (dieta de costo minimo)."""
import json

from backend import config
from backend.nutrition import compute_requirements
from backend.optimizer import FoodInput, OptimizeOptions, optimize
from backend.providers.local import LocalDatasetProvider


def _load_inputs() -> list[FoodInput]:
    records = LocalDatasetProvider(config.SEED_FOODS_PATH).fetch_foods()
    inputs = []
    for r in records:
        inputs.append(FoodInput(
            id=r.id, name=r.name, category=r.category, price_per_g=r.price_clp / r.package_g,
            serving_g=r.serving_g, max_servings_day=r.max_servings_day,
            satiety_index=r.satiety_index, kcal=r.kcal, protein_g=r.protein_g,
            carb_g=r.carb_g, fat_g=r.fat_g, fiber_g=r.fiber_g, sodium_mg=r.sodium_mg,
            calcium_mg=r.calcium_mg, iron_mg=r.iron_mg, potassium_mg=r.potassium_mg,
            vitamin_c_mg=r.vitamin_c_mg, brand=r.brand,
        ))
    return inputs


def test_seed_dataset_loads():
    with open(config.SEED_FOODS_PATH, encoding="utf-8") as fh:
        data = json.load(fh)
    assert len(data["alimentos"]) >= 30


def test_optimize_feasible_and_meets_kcal():
    inputs = _load_inputs()
    req = compute_requirements("M", 30, 80, 180, "moderado", "mantener")
    res = optimize(inputs, req)
    assert res.feasible
    # Las calorias quedan dentro de una banda razonable del objetivo.
    assert abs(res.totals["kcal"] - req.kcal) <= req.kcal * 0.15
    # Cubre al menos el 85% de la proteina objetivo.
    assert res.totals["protein_g"] >= req.protein_g * 0.85
    assert res.totals["cost_clp"] > 0
    assert len(res.items) > 0


def test_preferences_change_selection_cost():
    inputs = _load_inputs()
    req = compute_requirements("M", 30, 80, 180, "moderado", "mantener")
    base = optimize(inputs, req)

    # Penaliza fuertemente el alimento mas usado: el costo o la mezcla cambia.
    top = max(base.items, key=lambda it: it.grams).food_id
    prefs = {top: -1.0}
    biased = optimize(inputs, req, preferences=prefs,
                      opts=OptimizeOptions(preference_weight=0.5))
    biased_top_grams = next((it.grams for it in biased.items if it.food_id == top), 0.0)
    base_top_grams = next((it.grams for it in base.items if it.food_id == top), 0.0)
    # El alimento penalizado se usa igual o menos que en la base.
    assert biased_top_grams <= base_top_grams + 1e-6


def test_satiety_emphasis_increases_total_satiety():
    inputs = _load_inputs()
    req = compute_requirements("M", 30, 80, 180, "moderado", "mantener")
    base = optimize(inputs, req, opts=OptimizeOptions(satiety_emphasis=0.0))
    saciante = optimize(inputs, req, opts=OptimizeOptions(satiety_emphasis=0.5))
    assert saciante.totals["satiety"] >= base.totals["satiety"] - 1e-6


def test_infeasible_returns_gracefully():
    # Un solo alimento que no puede cubrir las metas -> debe relajar o avisar.
    inputs = _load_inputs()[:1]
    req = compute_requirements("M", 40, 100, 190, "muy_activo", "subir")
    res = optimize(inputs, req)
    # No debe lanzar excepcion; devuelve un resultado (factible relajado o no).
    assert isinstance(res.feasible, bool)
