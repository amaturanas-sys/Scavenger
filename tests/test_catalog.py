"""Pruebas del catalogo multi-cadena."""
import json

from backend import config
from backend.providers.local import LocalDatasetProvider


def _catalog() -> dict:
    with open(config.SEED_FOODS_PATH, encoding="utf-8") as fh:
        return json.load(fh)


def test_catalog_has_six_retailers():
    data = _catalog()
    assert len(data["retailers"]) == 6
    names = {r["name"] for r in data["retailers"]}
    assert {"Jumbo", "Lider", "Santa Isabel", "Tottus", "Unimarc", "Mayorista 10"} == names


def test_every_food_has_multiple_retailer_prices():
    data = _catalog()
    assert len(data["alimentos"]) >= 70
    for f in data["alimentos"]:
        prices = f["prices"]
        assert len(prices) >= 4, f"{f['id']} tiene pocas cadenas"
        assert all(p["price_clp"] > 0 for p in prices)
        # ids de cadena no se repiten dentro de un alimento
        ids = [p["retailer_id"] for p in prices]
        assert len(ids) == len(set(ids))


def test_local_provider_default_price_is_cheapest():
    records = LocalDatasetProvider(config.SEED_FOODS_PATH).fetch_foods()
    assert records
    for r in records:
        if r.prices:
            cheapest = min(p["price_clp"] for p in r.prices)
            assert r.price_clp == cheapest


def test_price_dispersion_between_retailers():
    """Debe existir diferencia de precio entre cadenas (comparacion util)."""
    data = _catalog()
    dispersos = 0
    for f in data["alimentos"]:
        precios = [p["price_clp"] for p in f["prices"]]
        if max(precios) > min(precios):
            dispersos += 1
    # La gran mayoria de los alimentos varia de precio entre cadenas.
    assert dispersos >= 0.8 * len(data["alimentos"])
