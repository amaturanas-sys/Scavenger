"""Pruebas offline del adaptador Lider (Walmart Chile)."""
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from backend import pricing
from backend.database import Base
from backend.models import Food
from backend.providers import PRICE_PROVIDERS
from backend.providers.lider import (
    LiderProvider,
    _deep_price,
    _extract_products,
    _map_product,
)
from backend.seed import seed_foods


def test_lider_registered_as_price_provider():
    assert PRICE_PROVIDERS.get("lider") is LiderProvider


# --- extraccion del contenedor de productos -------------------------------
def test_extract_products_shapes():
    assert _extract_products({"products": [1, 2]}) == [1, 2]
    assert _extract_products({"data": {"products": [3]}}) == [3]
    assert _extract_products([{"a": 1}]) == [{"a": 1}]
    assert _extract_products({"nada": True}) == []


# --- precio anidado / formateado ------------------------------------------
def test_deep_price_variants():
    assert _deep_price({"price": 1390}) == 1390
    assert _deep_price({"prices": {"BasePriceSales": "$1.090"}}) == 1090
    assert _deep_price({"priceInfo": {"price": 2490.0}}) == 2490
    assert _deep_price({"foo": 1}) == 0.0


# --- mapeo de producto ----------------------------------------------------
def test_map_product_ok():
    raw = {
        "displayName": "Arroz Grado 2 Tucapel 1 Kg", "brand": "Tucapel",
        "sku": "12345", "gtin13": "7800", "price": 1390, "available": True, "url": "/p/1",
    }
    m = _map_product(raw)
    assert m["price_clp"] == 1390
    assert m["package_g"] == 1000
    assert m["brand"] == "Tucapel"
    assert m["retailer"] == "Lider"
    assert m["retailer_id"] == "lider"


def test_map_product_skips_unavailable_and_priceless():
    assert _map_product({"displayName": "X 1 Kg", "price": 990, "available": False}) is None
    assert _map_product({"displayName": "Y 500 g"}) is None
    assert _map_product({"price": 1000}) is None  # sin nombre


# --- search_products con red mockeada -------------------------------------
class _FakeLider(LiderProvider):
    def __init__(self, payload, **kw):
        super().__init__(enabled=True, **kw)
        self._payload = payload

    def _http_get_json(self, url):
        return self._payload


def test_search_products_maps_and_filters():
    payload = {"products": [
        {"displayName": "Arroz Grado 2 Tucapel 1 Kg", "brand": "Tucapel", "sku": "1", "price": 1390, "available": True},
        {"displayName": "Agotado 1 Kg", "price": 500, "available": False},
        {"name": "Leche Entera Soprole 1 L", "brandName": "Soprole", "productId": "2", "prices": {"BasePriceSales": 1090}},
    ]}
    prov = _FakeLider(payload)
    products = prov.search_products("x")
    names = {p["name"] for p in products}
    assert "Arroz Grado 2 Tucapel 1 Kg" in names
    assert "Leche Entera Soprole 1 L" in names
    assert all(p["retailer_id"] == "lider" for p in products)
    assert len(products) == 2  # el agotado se descarta


# --- refresco end-to-end (offline) ----------------------------------------
def _isolated_db():
    engine = create_engine("sqlite:///:memory:", connect_args={"check_same_thread": False})
    Base.metadata.create_all(engine)
    return sessionmaker(bind=engine)()


def test_refresh_lider_updates_prices(monkeypatch):
    db = _isolated_db()
    seed_foods(db, "local", refresh=False)
    food = db.query(Food).filter(Food.id == "arroz_grado2").one()

    class FakeLider(_FakeLider):
        def __init__(self, enabled=True):
            super().__init__({"products": [
                {"displayName": "Arroz Grado 2 Tucapel 1 Kg", "brand": "Tucapel", "sku": "1",
                 "price": 1234, "available": True},
            ]})

    monkeypatch.setitem(pricing.PRICE_PROVIDERS, "lider", FakeLider)
    res = pricing.refresh_retailer(db, "lider", use_cache=False, sleep_s=0, log=lambda *_: None)
    assert res.matched > 0 and res.updated > 0

    db.refresh(food)
    lider_price = next((p.price_clp for p in food.prices if p.retailer_id == "lider"), None)
    assert lider_price == 1234
