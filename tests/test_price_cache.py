"""Pruebas de la memoria de precios (cache persistente con TTL)."""
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from backend import pricing
from backend.database import Base
from backend.seed import seed_foods


def _db():
    engine = create_engine("sqlite:///:memory:", connect_args={"check_same_thread": False})
    Base.metadata.create_all(engine)
    db = sessionmaker(bind=engine)()
    seed_foods(db, "local", refresh=False)
    return db


def _matching_provider():
    class P:
        calls = 0

        def __init__(self, enabled=True):
            pass

        def search_products(self, term):
            P.calls += 1
            return [{"name": term, "brand": "", "price_clp": 777, "package_g": 1000,
                     "retailer": "Jumbo", "retailer_id": "jumbo"}]
    return P


def test_fresh_cache_avoids_second_fetch(monkeypatch):
    db = _db()
    P = _matching_provider()
    monkeypatch.setitem(pricing.PRICE_PROVIDERS, "jumbo", P)
    t = 1_000_000.0

    r1 = pricing.refresh_retailer(db, "jumbo", limit=3, use_cache=False, sleep_s=0,
                                  ttl_days=30, now=t, log=lambda *_: None)
    assert P.calls == 3 and r1.updated == 3 and r1.cached == 0

    # Dentro del TTL: se sirve de memoria, no se vuelve a consultar (sin token).
    r2 = pricing.refresh_retailer(db, "jumbo", limit=3, use_cache=False, sleep_s=0,
                                  ttl_days=30, now=t + 10 * 86400, log=lambda *_: None)
    assert P.calls == 3  # no hubo nuevas consultas
    assert r2.cached == 3 and r2.updated == 0


def test_expired_cache_refetches(monkeypatch):
    db = _db()
    P = _matching_provider()
    monkeypatch.setitem(pricing.PRICE_PROVIDERS, "jumbo", P)
    t = 1_000_000.0

    pricing.refresh_retailer(db, "jumbo", limit=2, use_cache=False, sleep_s=0,
                             ttl_days=30, now=t, log=lambda *_: None)
    assert P.calls == 2

    # Pasado el TTL (31 dias) se vuelve a consultar.
    r = pricing.refresh_retailer(db, "jumbo", limit=2, use_cache=False, sleep_s=0,
                                 ttl_days=30, now=t + 31 * 86400, log=lambda *_: None)
    assert P.calls == 4 and r.updated == 2 and r.cached == 0


def test_misses_are_remembered(monkeypatch):
    db = _db()

    class P:
        calls = 0

        def __init__(self, enabled=True):
            pass

        def search_products(self, term):
            P.calls += 1
            return []  # nunca encuentra coincidencia

    monkeypatch.setitem(pricing.PRICE_PROVIDERS, "jumbo", P)
    t = 1_000_000.0

    r1 = pricing.refresh_retailer(db, "jumbo", limit=2, use_cache=False, sleep_s=0,
                                  ttl_days=30, now=t, log=lambda *_: None)
    assert P.calls == 2 and r1.missed == 2

    # El 'no encontrado' tambien se recuerda: no se reintenta dentro del TTL.
    r2 = pricing.refresh_retailer(db, "jumbo", limit=2, use_cache=False, sleep_s=0,
                                  ttl_days=30, now=t + 5 * 86400, log=lambda *_: None)
    assert P.calls == 2 and r2.cached == 2


def test_ttl_zero_always_fetches(monkeypatch):
    db = _db()
    P = _matching_provider()
    monkeypatch.setitem(pricing.PRICE_PROVIDERS, "jumbo", P)
    t = 1_000_000.0

    pricing.refresh_retailer(db, "jumbo", limit=1, use_cache=False, sleep_s=0,
                             ttl_days=0, now=t, log=lambda *_: None)
    pricing.refresh_retailer(db, "jumbo", limit=1, use_cache=False, sleep_s=0,
                             ttl_days=0, now=t, log=lambda *_: None)
    assert P.calls == 2  # TTL=0 desactiva la memoria: siempre consulta


def test_cached_hit_preserves_price(monkeypatch):
    from backend.models import Food
    db = _db()
    P = _matching_provider()
    monkeypatch.setitem(pricing.PRICE_PROVIDERS, "jumbo", P)
    t = 1_000_000.0

    pricing.refresh_retailer(db, "jumbo", limit=5, use_cache=False, sleep_s=0,
                             ttl_days=30, now=t, log=lambda *_: None)
    pricing.refresh_retailer(db, "jumbo", limit=5, use_cache=False, sleep_s=0,
                             ttl_days=30, now=t + 1 * 86400, log=lambda *_: None)
    # Tras un hit de cache el precio de Jumbo sigue presente.
    foods = db.query(Food).order_by(Food.name).limit(5).all()
    jumbo_prices = [p.price_clp for f in foods for p in f.prices if p.retailer_id == "jumbo"]
    assert jumbo_prices and all(pr == 777 for pr in jumbo_prices)
