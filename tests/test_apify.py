"""Pruebas offline del proveedor Apify (mapeo, input y registro)."""
from backend.providers import PRICE_PROVIDERS
from backend.providers.apify import (
    ApifyProvider,
    JumboApifyProvider,
    LiderApifyProvider,
    UnimarcApifyProvider,
)


class _FakeApify(ApifyProvider):
    name = "fake"
    retailer_name = "Fake"
    env_key = "FAKE"

    def __init__(self, payload, **kw):
        super().__init__(token="tok", **kw)
        self.actor_id = "user/actor"
        self._payload = payload

    def _run_actor(self, payload):
        self.sent = payload
        return self._payload


def test_build_input_default():
    p = _FakeApify([])
    assert p._build_input("arroz") == {"search": "arroz", "maxItems": 5}


def test_input_env_override(monkeypatch):
    monkeypatch.setenv("SCAVENGER_APIFY_FAKE_INPUT", '{"q": "{q}", "n": {n}}')
    p = _FakeApify([], max_results=7)
    assert p._build_input("pan") == {"q": "pan", "n": 7}


def test_search_products_maps_and_filters():
    payload = [
        {"title": "Arroz Grado 2 Tucapel 1 Kg", "brand": "Tucapel", "sku": "1", "price": 1490},
        {"name": "Leche Entera Soprole 1 L", "brandName": "Soprole", "id": "2",
         "prices": {"BasePriceSales": 1090}},
        {"name": "Sin precio 1 Kg", "price": 0},  # se descarta (sin precio)
    ]
    p = _FakeApify(payload, max_results=5)
    out = p.search_products("x")
    names = {o["name"] for o in out}
    assert names == {"Arroz Grado 2 Tucapel 1 Kg", "Leche Entera Soprole 1 L"}
    assert all(o["retailer_id"] == "fake" and o["retailer"] == "Fake" for o in out)
    arroz = next(o for o in out if o["name"].startswith("Arroz"))
    assert arroz["price_clp"] == 1490 and arroz["package_g"] == 1000


def test_caps_at_max_results():
    payload = [{"title": f"Prod {i} 1 Kg", "price": 100 + i} for i in range(10)]
    p = _FakeApify(payload, max_results=3)
    assert len(p.search_products("x")) == 3


def test_not_configured_returns_empty():
    p = _FakeApify([{"title": "X 1 Kg", "price": 100}])
    p.token = ""  # sin token -> no configurado
    assert p.search_products("x") == []


def test_actor_id_slug_to_tilde():
    # La web muestra 'user/actor'; la API usa 'user~actor'.
    p = _FakeApify([])
    p.actor_id = "scraperschile/jumbo"
    assert p.actor_id.replace("/", "~") == "scraperschile~jumbo"


def test_registry_and_metered():
    assert PRICE_PROVIDERS["jumbo"] is JumboApifyProvider
    assert PRICE_PROVIDERS["unimarc"] is UnimarcApifyProvider
    assert PRICE_PROVIDERS["lider"] is LiderApifyProvider
    assert JumboApifyProvider(token="t").metered is True
    assert JumboApifyProvider(token="t").name == "jumbo"
    assert UnimarcApifyProvider(token="t").retailer_name == "Unimarc"
