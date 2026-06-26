"""Pruebas de la cuota mensual aplicada en el refresco de precios (metered)."""
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from backend import pricing, usage
from backend.database import Base
from backend.seed import seed_foods


def _db():
    engine = create_engine("sqlite:///:memory:", connect_args={"check_same_thread": False})
    Base.metadata.create_all(engine)
    db = sessionmaker(bind=engine)()
    seed_foods(db, "local", refresh=False)
    return db


class _MeteredFake:
    metered = True

    def __init__(self, enabled=True):
        pass

    def search_products(self, term):
        return [{"name": term, "brand": "", "price_clp": 500, "package_g": 1000,
                 "retailer": "X", "retailer_id": "x"}]


def test_budget_limits_fetches_and_defers_rest(monkeypatch):
    db = _db()
    monkeypatch.setitem(pricing.PRICE_PROVIDERS, "x", _MeteredFake)
    res = pricing.refresh_retailer(db, "x", limit=5, use_cache=False, sleep_s=0,
                                   ttl_days=30, now=1_000_000.0, budget=2,
                                   provider_key="apify", log=lambda *_: None)
    assert res.updated == 2 and res.deferred == 3
    assert usage.get_used(db, "apify") == 2


def test_no_budget_means_unlimited(monkeypatch):
    db = _db()
    monkeypatch.setitem(pricing.PRICE_PROVIDERS, "x", _MeteredFake)
    res = pricing.refresh_retailer(db, "x", limit=5, use_cache=False, sleep_s=0,
                                   ttl_days=30, now=1_000_000.0, budget=None,
                                   log=lambda *_: None)
    assert res.deferred == 0 and res.updated == 5


def test_cache_hits_do_not_consume_budget(monkeypatch):
    db = _db()
    monkeypatch.setitem(pricing.PRICE_PROVIDERS, "x", _MeteredFake)
    t = 1_000_000.0
    pricing.refresh_retailer(db, "x", limit=3, use_cache=False, sleep_s=0, ttl_days=30,
                             now=t, budget=10, provider_key="apify", log=lambda *_: None)
    used1 = usage.get_used(db, "apify")
    assert used1 == 3
    # Dentro del TTL: se sirve de cache, no consume cuota.
    r2 = pricing.refresh_retailer(db, "x", limit=3, use_cache=False, sleep_s=0, ttl_days=30,
                                  now=t + 86400, budget=10, provider_key="apify",
                                  log=lambda *_: None)
    assert r2.cached == 3 and usage.get_used(db, "apify") == used1
