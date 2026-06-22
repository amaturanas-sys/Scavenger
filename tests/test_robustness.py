"""Pruebas de robustez para bugs corregidos en la revisión exhaustiva."""
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from backend.database import Base
from backend.learning import apply_feedback
from backend.models import Feedback, Food, FoodPrice, Plan, User
from backend.seed import seed_foods
from backend.services import _price_in_retailers
from backend.shopping import build_shopping_list


def _db():
    engine = create_engine("sqlite:///:memory:", connect_args={"check_same_thread": False})
    Base.metadata.create_all(engine)
    db = sessionmaker(bind=engine)()
    seed_foods(db, "local", refresh=False)
    return db


def test_shopping_list_skips_items_without_food_id():
    db = _db()
    payload = {"meals": [{"meal": "almuerzo", "items": [
        {"name": "Sin id", "retailer": "Jumbo", "grams": 100, "cost_clp": 500},  # sin food_id
        {"food_id": "arroz_grado2", "name": "Arroz", "retailer": "Mayorista 10",
         "grams": 200, "cost_clp": 220},
    ]}]}
    sl = build_shopping_list(db, payload)  # no debe lanzar KeyError
    ids = [i["food_id"] for r in sl["retailers"] for i in r["items"]]
    assert ids == ["arroz_grado2"]


def test_apply_feedback_ignores_items_without_food_id():
    db = _db()
    db.add(User(name="U")); db.commit()
    plan = Plan(user_id=1, title="M", scope="diario", payload={"meals": [{"meal": "almuerzo", "items": [
        {"name": "Sin id", "satiety_contrib": 10},  # sin food_id
        {"food_id": "lentejas", "name": "Lentejas", "satiety_contrib": 50},
    ]}]})
    db.add(plan); db.flush()
    fb = Feedback(plan_id=plan.id, satiety_score=2, cost_score=4)
    db.add(fb); db.flush()
    updated = apply_feedback(db, plan, fb)  # no debe lanzar KeyError
    assert "lentejas" in updated


def test_price_without_preferred_retailers_picks_cheapest():
    db = _db()
    food = db.query(Food).filter(Food.id == "arroz_grado2").one()
    cheapest = min(p.price_per_g for p in food.prices)
    ppg, retailer = _price_in_retailers(food, preferred=set())
    assert abs(ppg - cheapest) < 1e-9


def test_nutrition_requirements_never_negative():
    from backend.nutrition import compute_requirements
    # Inputs degenerados (que la API ya valida) no deben dar macros negativos.
    r = compute_requirements("M", 200, 1, 1, "sedentario", "bajar")
    for v in (r.kcal, r.protein_g, r.carb_g, r.fat_g, r.fiber_g):
        assert v >= 0


def test_planner_conserves_mass_in_meals():
    from backend.optimizer import OptimizeResult, PlanItem
    from backend.planner import distribute_into_meals
    # Item con poca masa (1 g) que repartido caeria por debajo de 0.5 g por comida.
    item = PlanItem(food_id="x", name="X", brand="", category="cereal", retailer="J",
                    grams=1.0, servings=0.1, cost_clp=10, kcal=3.6, protein_g=0.1,
                    carb_g=0.8, fat_g=0.0, fiber_g=0.0, satiety_contrib=1.0)
    res = OptimizeResult(True, "Optimal", items=[item],
                         totals={"kcal": 3.6, "cost_clp": 10})
    plan = distribute_into_meals(res)
    placed = sum(i["grams"] for meal in plan["meals"] for i in meal["items"])
    assert abs(placed - 1.0) < 1e-6  # no se pierde masa
    meal_kcal = sum(m["subtotal"]["kcal"] for m in plan["meals"])
    assert abs(meal_kcal - 3.6) < 0.2  # los subtotales reconcilian con el total


def test_recompute_cheapest_ignores_zero_price():
    from backend.pricing import _recompute_cheapest
    db = _db()
    food = db.query(Food).filter(Food.id == "lentejas").one()
    # Inyecta un precio espurio en 0; no debe ser elegido como el mas barato.
    food.prices.append(FoodPrice(retailer="Falsa", retailer_id="falsa", price_clp=0, package_g=1000))
    db.flush()
    _recompute_cheapest([food])
    assert food.price_clp > 0
