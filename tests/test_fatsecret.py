"""Pruebas offline del proveedor FatSecret y el enriquecimiento nutricional."""
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from backend.database import Base
from backend.models import Food
from backend.nutrition_enrich import enrich_foods
from backend.providers.fatsecret import FatSecretProvider, normalize_servings_to_100g
from backend.seed import seed_foods


def _serving(amount, unit="g", **vals):
    base = {"metric_serving_amount": str(amount), "metric_serving_unit": unit}
    base.update({k: str(v) for k, v in vals.items()})
    return base


# --- normalizacion a 100 g ------------------------------------------------
def test_normalize_serving_100g_factor1():
    s = _serving(100, calories=130, protein=2.7, carbohydrate=28, fat=0.3,
                 fiber=0.4, sodium=1, calcium=10, iron=0.2, potassium=35, vitamin_c=0)
    per = normalize_servings_to_100g(s)
    assert per["kcal"] == 130 and per["protein_g"] == 2.7 and per["carb_g"] == 28


def test_normalize_serving_scaled_from_50g():
    s = _serving(50, calories=65, protein=1.35, carbohydrate=14, fat=0.15)
    per = normalize_servings_to_100g(s)
    assert per["kcal"] == 130 and per["protein_g"] == 2.7 and per["carb_g"] == 28


def test_normalize_prefers_serving_closest_to_100g():
    servings = [
        _serving(30, calories=300),   # factor 3.33 -> 1000
        _serving(100, calories=120),  # factor 1 -> 120 (elegida)
    ]
    assert normalize_servings_to_100g(servings)["kcal"] == 120


def test_normalize_returns_none_without_metric_serving():
    assert normalize_servings_to_100g({"metric_serving_unit": "serving"}) is None
    assert normalize_servings_to_100g(None) is None


# --- proveedor con red mockeada -------------------------------------------
class _FakeFatSecret(FatSecretProvider):
    def __init__(self, search_payload, get_payload):
        super().__init__(key="k", secret="s")
        self._sp = search_payload
        self._gp = get_payload

    def _get_token(self):
        return "token"

    def _api_get(self, params):
        if params.get("method") == "foods.search":
            return self._sp
        return self._gp


def _fake_provider():
    search = {"foods": {"food": [
        {"food_id": "1", "food_name": "Arroz Grado 2", "brand_name": "Tucapel"},
        {"food_id": "2", "food_name": "Cloro", "brand_name": "X"},
    ]}}
    get = {"food": {"food_id": "1", "food_name": "Arroz Grado 2", "brand_name": "Tucapel",
                    "servings": {"serving": _serving(100, calories=358, protein=7,
                                                     carbohydrate=79, fat=0.8, fiber=1.3)}}}
    return _FakeFatSecret(search, get)


def test_nutrition_for_matches_and_normalizes():
    per = _fake_provider().nutrition_for("Arroz grado 2", "Tucapel")
    assert per is not None
    assert per["kcal"] == 358 and per["carb_g"] == 79
    assert per["food_name"] == "Arroz Grado 2"


def test_nutrition_for_unconfigured_returns_none():
    prov = FatSecretProvider(key="", secret="")
    assert prov.nutrition_for("arroz") is None


# --- enriquecimiento end-to-end (offline) ---------------------------------
def _isolated_db():
    engine = create_engine("sqlite:///:memory:", connect_args={"check_same_thread": False})
    Base.metadata.create_all(engine)
    return sessionmaker(bind=engine)()


def test_enrich_fills_only_missing(monkeypatch):
    db = _isolated_db()
    seed_foods(db, "local", refresh=False)

    # Simula un alimento sin nutricion (todo en cero).
    food = db.query(Food).filter(Food.id == "arroz_grado2").one()
    food.kcal = food.protein_g = food.carb_g = food.fat_g = 0.0
    db.commit()

    res = enrich_foods(db, provider=_fake_provider(), only_missing=True, log=lambda *_: None)
    assert res.enriched == 1 and res.skipped > 0

    db.refresh(food)
    assert food.kcal == 358 and food.carb_g == 79
