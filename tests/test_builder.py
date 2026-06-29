"""Pruebas del constructor de comidas por roles."""
import random

from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from backend.builder import (
    MEAL_TEMPLATES,
    build_slots,
    meal_target,
    meal_type,
    random_meal,
    role_of,
    summarize,
)
from backend.database import Base
from backend.models import Food, User
from backend.seed import seed_foods
from backend.services import user_requirements


def _db_user(**kw):
    engine = create_engine("sqlite:///:memory:", connect_args={"check_same_thread": False})
    Base.metadata.create_all(engine)
    db = sessionmaker(bind=engine)()
    seed_foods(db, "local", refresh=False)
    defaults = dict(name="U", sex="M", age=30, weight_kg=80, height_cm=180,
                    activity_level="moderado", goal="mantener", daily_budget_clp=5000,
                    preferred_retailers=[])
    defaults.update(kw)
    u = User(**defaults)
    db.add(u)
    db.commit()
    return db, u


def test_role_mapping():
    assert role_of(_food("pechuga_pollo", "carne")) == "proteina"
    assert role_of(_food("arroz_grado2", "cereal")) == "carbohidrato"
    assert role_of(_food("aceite_oliva", "grasa")) == "grasa"
    assert role_of(_food("lechuga", "verdura")) == "vegetal"
    # Tuberculo (papa) se trata como carbohidrato pese a ser 'verdura'.
    assert role_of(_food("papa", "verdura")) == "carbohidrato"


def _food(fid, category):
    f = Food(id=fid, name=fid, category=category)
    return f


def test_meal_type_and_target_fractions():
    assert meal_type("snack2") == "snack"
    assert meal_type("Desayuno") == "desayuno"

    class R:  # requerimientos simulados
        kcal, protein_g, carb_g, fat_g = 2000, 120, 250, 60
    t = meal_target(R, "almuerzo")
    assert t["fraction"] == MEAL_TEMPLATES["almuerzo"]["fraction"]
    assert t["kcal"] == round(2000 * t["fraction"], 1)


def test_build_slots_has_candidates_with_portions():
    db, u = _db_user()
    data = build_slots(db, u, "almuerzo")
    roles = {s["role"] for s in data["slots"]}
    assert {"proteina", "carbohidrato", "vegetal"} <= roles
    for slot in data["slots"]:
        assert slot["candidates"], f"rol sin candidatos: {slot['role']}"
        for c in slot["candidates"]:
            assert c["grams"] > 0
            assert c["cost_clp"] >= 0
            assert "satiety_contrib" in c
        # ordenados de lo mas economico hacia arriba
        costs = [c["cost_clp"] for c in slot["candidates"]]
        assert costs == sorted(costs)


def test_protein_slot_portion_approaches_protein_target():
    db, u = _db_user()
    data = build_slots(db, u, "almuerzo")
    target = data["target"]
    prot_slot = next(s for s in data["slots"] if s["role"] == "proteina")
    # Al menos un candidato de proteina aporta cerca de la meta proteica.
    best = max(c["protein_g"] for c in prot_slot["candidates"])
    assert best >= target["protein_g"] * 0.6


def test_random_meal_picks_one_per_slot_and_summarizes():
    db, u = _db_user()
    rng = random.Random(42)
    res = random_meal(db, u, "almuerzo", rng=rng)
    slots = MEAL_TEMPLATES["almuerzo"]["slots"]
    assert len(res["selection"]) == len(slots)
    assert {c["role"] for c in res["selection"]} <= set(slots)
    assert res["totals"]["cost_clp"] > 0
    assert isinstance(res["fit_ok"], bool)


def test_numbered_snack_uses_snack_template():
    db, u = _db_user()
    data = build_slots(db, u, "snack 2")
    assert data["meal"] == "snack 2"
    assert data["meal_type"] == "snack"
    roles = {s["role"] for s in data["slots"]}
    assert roles == set(MEAL_TEMPLATES["snack"]["slots"])


def test_summarize_totals_and_fit():
    db, u = _db_user()
    req = user_requirements(u)
    target = meal_target(req, "almuerzo")
    items = [
        {"kcal": target["kcal"], "protein_g": target["protein_g"],
         "carb_g": target["carb_g"], "fat_g": target["fat_g"], "cost_clp": 800},
    ]
    s = summarize(items, target)
    assert s["fit"]["kcal"] == 1.0
    assert s["fit_ok"] is True
    assert s["totals"]["cost_clp"] == 800


def test_portion_grams_respects_absolute_max():
    """Una grasa con porción de referencia grande no debe superar ABS_MAX_G."""
    from backend.builder import ABS_MAX_G, _portion_grams

    class _Fat:
        serving_g = 200.0          # piso (lo) = 80 g, por encima del tope 50 g
        protein_g = 0.0
        carb_g = 0.0
        fat_g = 99.0
        max_servings_day = 3.0

    target = {"protein_g": 40.0, "carb_g": 150.0, "fat_g": 30.0}
    g = _portion_grams(_Fat(), "grasa", target)
    assert g <= ABS_MAX_G["grasa"], f"{g} excede el tope {ABS_MAX_G['grasa']}"
