"""Pruebas del match por EAN (código de barras) y su autoaprendizaje."""
import pytest
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from backend import pricing
from backend.database import Base
from backend.models import Food
from backend.providers.vtex import VTEXProvider, best_match, looks_like_ean
from backend.seed import seed_foods


@pytest.mark.parametrize("value,ok", [
    ("7801234567890", True), ("12345678", True), ("1234567", False),
    ("780abc", False), ("", False), ("123456789012345", False), (None, False),
])
def test_looks_like_ean(value, ok):
    assert looks_like_ean(value) is ok


def test_best_match_ean_beats_name():
    products = [
        {"name": "Arroz Grado 2 Tucapel 1 Kg", "brand": "Tucapel", "package_g": 1000, "ean": "7801111111118"},
        {"name": "Producto Sin Relacion XYZ", "brand": "Z", "package_g": 500, "ean": "7809999999993"},
    ]
    # El nombre calza con el primero, pero el EAN del alimento es el del segundo:
    # gana el match exacto por código de barras.
    m = best_match("Arroz grado 2", "Tucapel", products, food_ean="7809999999993")
    assert m is not None and m["ean"] == "7809999999993"


def test_best_match_invalid_ean_falls_back_to_name():
    products = [{"name": "Arroz Grado 2 Tucapel 1 Kg", "brand": "Tucapel", "package_g": 1000, "ean": "999"}]
    # '999' no es un EAN válido -> se ignora y se matchea por nombre.
    m = best_match("Arroz grado 2", "Tucapel", products, food_ean="123")
    assert m is not None and m["name"].startswith("Arroz")


def _isolated_db():
    engine = create_engine("sqlite:///:memory:", connect_args={"check_same_thread": False})
    Base.metadata.create_all(engine)
    db = sessionmaker(bind=engine)()
    seed_foods(db, "local", refresh=False)
    return db


class _FakeJumbo(VTEXProvider):
    """Proveedor falso: devuelve un producto fijo para cualquier término."""
    name = "jumbo"
    retailer_name = "Jumbo"
    base_url = "https://fake.cl"
    _product: dict = {}

    def __init__(self, enabled=True):
        super().__init__(enabled=True)

    def search_products(self, term):
        return [dict(self._product)]


def test_refresh_autolearns_ean(monkeypatch):
    db = _isolated_db()
    food = db.query(Food).filter(Food.id == "arroz_grado2").one()
    assert not food.ean

    class FakeJumbo(_FakeJumbo):
        _product = {"name": "Arroz Grado 2 Tucapel 1 Kg", "brand": "Tucapel",
                    "price_clp": 777, "package_g": 1000, "ean": "7801234567895",
                    "retailer": "Jumbo", "retailer_id": "jumbo"}

    monkeypatch.setitem(pricing.PRICE_PROVIDERS, "jumbo", FakeJumbo)
    # ttl_days=0 desactiva la memoria para que el match corra en cada alimento.
    pricing.refresh_retailer(db, "jumbo", use_cache=False, sleep_s=0, ttl_days=0, log=lambda *_: None)

    db.refresh(food)
    # El match por nombre es fuerte -> se autoaprende el EAN real del retail.
    assert food.ean == "7801234567895"


def test_refresh_matches_by_ean_despite_bad_name(monkeypatch):
    db = _isolated_db()
    food = db.query(Food).filter(Food.id == "arroz_grado2").one()
    food.ean = "7801234567895"  # EAN ya conocido (p.ej. aprendido antes)
    db.commit()

    class FakeJumbo(_FakeJumbo):
        # Nombre sin relación, pero mismo EAN: debe matchear igual.
        _product = {"name": "ZZZ Oferta Random", "brand": "X", "price_clp": 555,
                    "package_g": 1000, "ean": "7801234567895",
                    "retailer": "Jumbo", "retailer_id": "jumbo"}

    monkeypatch.setitem(pricing.PRICE_PROVIDERS, "jumbo", FakeJumbo)
    pricing.refresh_retailer(db, "jumbo", use_cache=False, sleep_s=0, ttl_days=0, log=lambda *_: None)

    db.refresh(food)
    after = next((p.price_clp for p in food.prices if p.retailer_id == "jumbo"), None)
    assert after == 555  # matcheó por EAN pese al nombre sin relación


# --- gate de autoaprendizaje reforzado (B1) ---------------------------------
class _FoodLike:
    def __init__(self, name, brand=""):
        self.name = name
        self.brand = brand


def test_can_learn_ean_blocks_single_token_without_brand():
    from backend.pricing import _can_learn_ean
    m = {"name": "Salsa de Tomate Pomarola 200 g", "brand": "Pomarola", "ean": "7801234567777"}
    # 'Tomate' (1 token, sin marca) NO debe aprender el EAN de una salsa de tomate.
    assert _can_learn_ean(_FoodLike("Tomate"), m) is False


def test_can_learn_ean_allows_multitoken_full_coverage():
    from backend.pricing import _can_learn_ean
    m = {"name": "Arroz Grado 2 Tucapel 1 Kg", "brand": "Tucapel"}
    assert _can_learn_ean(_FoodLike("Arroz grado 2", "Tucapel"), m) is True


def test_can_learn_ean_allows_single_token_with_brand_match():
    from backend.pricing import _can_learn_ean
    m = {"name": "Quinoa Real Tucapel 500 g", "brand": "Tucapel"}
    assert _can_learn_ean(_FoodLike("Quinoa", "Tucapel"), m) is True


def test_can_learn_ean_requires_full_coverage_even_with_brand():
    from backend.pricing import _can_learn_ean
    m = {"name": "Arroz Grado 2 Tucapel", "brand": "Tucapel"}
    # 'integral' no aparece -> cobertura < 1 -> no aprende aunque la marca calce.
    assert _can_learn_ean(_FoodLike("Arroz integral", "Tucapel"), m) is False


def test_refresh_does_not_autolearn_single_token(monkeypatch):
    db = _isolated_db()
    tomate = db.query(Food).filter(Food.id == "tomate").one_or_none()
    if tomate is None:
        return  # catálogo sin 'tomate'; el gate ya está cubierto por los unit tests
    assert not tomate.ean

    class FakeJumbo(_FakeJumbo):
        _product = {"name": "Salsa de Tomate Pomarola 200 g", "brand": "Pomarola",
                    "price_clp": 690, "package_g": 200, "ean": "7801234567777",
                    "retailer": "Jumbo", "retailer_id": "jumbo"}

    monkeypatch.setitem(pricing.PRICE_PROVIDERS, "jumbo", FakeJumbo)
    pricing.refresh_retailer(db, "jumbo", use_cache=False, sleep_s=0, ttl_days=0, log=lambda *_: None)
    db.refresh(tomate)
    assert not tomate.ean   # no aprendió el EAN equivocado
