"""Pruebas offline del scraping VTEX (parseo, matching y refresco)."""
import pytest
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from backend import pricing
from backend.database import Base
from backend.models import Food, FoodPrice
from backend.providers.vtex import (
    VTEXProvider,
    best_match,
    extract_offer,
    is_non_edible,
    parse_package_grams,
    score_match,
)
from backend.seed import seed_foods


# --- parse_package_grams --------------------------------------------------
@pytest.mark.parametrize("text,grams", [
    ("Arroz Grado 2 Tucapel 1 Kg", 1000),
    ("Leche Entera Soprole 1 L", 1000),
    ("Atun Robinson Crusoe 160 g", 160),
    ("Bebida 900 cc", 900),
    ("Aceite Vegetal 1,5 L", 1500),
    ("Yogur Pack 6 x 125 g", 750),
    ("Fideos 500 GR", 500),
    ("Arroz 6 x 1 kilo", 6000),
    ("Bebida 12 x 350 cc", 4200),
    ("Pack 2 x 1 kg", 2000),
    ("100 x 600 g", 600),  # multipack absurdo (60 kg) -> cae al unitario 600 g
])
def test_parse_package_grams_ok(text, grams):
    assert parse_package_grams(text) == grams


@pytest.mark.parametrize("text", ["Pechuga de pollo", "Arroz grado 2", ""])
def test_parse_package_grams_none(text):
    assert parse_package_grams(text) is None


# --- extract_offer --------------------------------------------------------
def _vtex_product(name, brand, price, available=True):
    return {
        "productId": "p1", "productName": name, "brand": brand, "link": "x",
        "items": [{
            "itemId": "i1", "name": name, "ean": "780",
            "unitMultiplier": 1, "measurementUnit": "un",
            "sellers": [{"commertialOffer": {"Price": price, "IsAvailable": available}}],
        }],
    }


def test_extract_offer_ok():
    p = _vtex_product("Arroz Grado 2 Tucapel 1 Kg", "Tucapel", 1490)
    offer = extract_offer(p)
    assert offer["price_clp"] == 1490
    assert offer["package_g"] == 1000
    assert offer["brand"] == "Tucapel"


def test_extract_offer_unavailable_or_no_price():
    assert extract_offer(_vtex_product("X 1 Kg", "Y", 0)) is None
    assert extract_offer(_vtex_product("X 1 Kg", "Y", 990, available=False)) is None
    assert extract_offer({"items": []}) is None


# --- matching -------------------------------------------------------------
def test_best_match_picks_right_product():
    products = [
        {"name": "Detergente Arroz Limpio 1 Kg", "brand": "Otro", "package_g": 1000},
        {"name": "Arroz Grado 2 Tucapel 1 Kg", "brand": "Tucapel", "package_g": 1000},
    ]
    m = best_match("Arroz grado 2", "Tucapel", products)
    assert m is not None and m["brand"] == "Tucapel"


def test_best_match_below_threshold_returns_none():
    products = [{"name": "Cloro Gel 1 L", "brand": "Z", "package_g": 1000}]
    assert best_match("Arroz grado 2", "Tucapel", products) is None


def test_score_brand_bonus():
    base = {"name": "Arroz Grado 2 1 Kg", "brand": "", "package_g": 1000}
    branded = {"name": "Arroz Grado 2 Tucapel 1 Kg", "brand": "Tucapel", "package_g": 1000}
    assert score_match("Arroz grado 2", "Tucapel", branded) >= score_match("Arroz grado 2", "Tucapel", base)


# --- filtro de no-comestibles --------------------------------------------
@pytest.mark.parametrize("name", [
    "Detergente Omo Matic 3 Kg", "Cloro Clorinda 900 cc", "Shampoo Sedal 400 ml",
    "Papel Higienico Confort 12 un", "Pañales Babysec G 40 un",
    "Alimento para Perros Master Dog 18 Kg", "Toallas Humedas Babysec",
    "Pasta Dental Colgate 90 g", "Pilas AA Energizer",
])
def test_is_non_edible_blocks(name):
    assert is_non_edible(name) is True


@pytest.mark.parametrize("name", [
    "Arroz Grado 2 Tucapel 1 Kg", "Leche Entera Soprole 1 L", "Sal de Mar 1 Kg",
    "Pan de Molde Ideal", "Atun Robinson Crusoe 160 g", "Pasta Fideos Carozzi 400 g",
    "Palta Hass Granel", "Aceite Vegetal Chef 1 L",
])
def test_is_non_edible_allows_food(name):
    assert is_non_edible(name) is False


def test_extract_offer_drops_non_edible():
    p = _vtex_product("Detergente Omo Matic 3 Kg", "Omo", 5990)
    assert extract_offer(p) is None


# --- sinonimos y plurales en el scoring ----------------------------------
def test_score_match_synonym_palta_aguacate():
    prod = {"name": "Aguacate Hass Maduro", "brand": "", "package_g": 200}
    # 'Palta' (catalogo) calza con 'Aguacate' (producto) por sinonimo.
    assert score_match("Palta Hass", "", prod) >= 0.9


def test_score_match_plural_singular():
    prod = {"name": "Lenteja Granel", "brand": "", "package_g": 1000}
    # 'Lentejas' (plural en el catalogo) calza con 'Lenteja' (singular) del producto.
    assert score_match("Lentejas", "", prod) >= 0.9


def test_best_match_uses_synonyms():
    products = [
        {"name": "Cloro Gel 1 L", "brand": "Z", "package_g": 1000},
        {"name": "Maiz Dulce en Conserva 300 g", "brand": "Wong", "package_g": 300},
    ]
    m = best_match("Choclo", "", products)  # choclo == maiz
    assert m is not None and "Maiz" in m["name"]


# --- VTEXProvider.search_products (red mockeada) --------------------------
class _FakeVTEX(VTEXProvider):
    name = "fake"
    retailer_name = "Fake"
    base_url = "https://fake.cl"

    def __init__(self, payload):
        super().__init__(enabled=True)
        self._payload = payload

    def _http_get_json(self, url):
        return self._payload


def test_provider_search_products_maps_offers():
    payload = [
        _vtex_product("Arroz Grado 2 Tucapel 1 Kg", "Tucapel", 1490),
        _vtex_product("Producto Agotado 1 Kg", "X", 0),  # se descarta
    ]
    prov = _FakeVTEX(payload)
    products = prov.search_products("arroz")
    assert len(products) == 1
    assert products[0]["retailer"] == "Fake"
    assert products[0]["price_clp"] == 1490


# --- refresh_retailer end-to-end (offline) --------------------------------
def _isolated_db():
    engine = create_engine("sqlite:///:memory:", connect_args={"check_same_thread": False})
    Base.metadata.create_all(engine)
    return sessionmaker(bind=engine)()


def test_refresh_retailer_updates_prices(monkeypatch):
    db = _isolated_db()
    seed_foods(db, "local", refresh=False)

    food = db.query(Food).filter(Food.id == "arroz_grado2").one()
    before = next((p.price_clp for p in food.prices if p.retailer_id == "jumbo"), None)

    # Proveedor falso: devuelve un producto barato y valido para cualquier termino.
    class FakeJumbo(_FakeVTEX):
        name = "jumbo"
        retailer_name = "Jumbo"

        def __init__(self, enabled=True):
            super().__init__([_vtex_product("Arroz Grado 2 Tucapel 1 Kg", "Tucapel", 777)])

        def search_products(self, term):  # responde igual a todo termino
            return super().search_products(term)

    monkeypatch.setitem(pricing.PRICE_PROVIDERS, "jumbo", FakeJumbo)

    res = pricing.refresh_retailer(db, "jumbo", use_cache=False, sleep_s=0, log=lambda *_: None)
    assert res.matched > 0 and res.updated > 0

    db.refresh(food)
    after = next((p.price_clp for p in food.prices if p.retailer_id == "jumbo"), None)
    assert after == 777 and after != before
    # El precio denormalizado (mas barato) se recomputa.
    assert food.price_clp <= 777
