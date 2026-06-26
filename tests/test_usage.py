"""Pruebas del control de cuota mensual de scraping (plan gratuito)."""
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from backend import usage
from backend.database import Base


def _db():
    engine = create_engine("sqlite:///:memory:", connect_args={"check_same_thread": False})
    Base.metadata.create_all(engine)
    return sessionmaker(bind=engine)()


def test_spend_within_budget_grants_full():
    db = _db()
    assert usage.try_spend(db, "apify", 30, budget=100, month="2026-06") == 30
    assert usage.get_used(db, "apify", "2026-06") == 30
    assert usage.remaining(db, "apify", 100, "2026-06") == 70


def test_spend_accumulates_and_caps_at_budget():
    db = _db()
    usage.try_spend(db, "apify", 80, budget=100, month="2026-06")
    # Solo quedan 20: pedir 50 concede 20.
    assert usage.try_spend(db, "apify", 50, budget=100, month="2026-06") == 20
    assert usage.get_used(db, "apify", "2026-06") == 100
    # Agotado: concede 0.
    assert usage.try_spend(db, "apify", 10, budget=100, month="2026-06") == 0


def test_month_rollover_resets_quota():
    db = _db()
    usage.try_spend(db, "apify", 100, budget=100, month="2026-06")
    assert usage.remaining(db, "apify", 100, "2026-06") == 0
    # Mes nuevo: presupuesto fresco.
    assert usage.remaining(db, "apify", 100, "2026-07") == 100
    assert usage.try_spend(db, "apify", 40, budget=100, month="2026-07") == 40


def test_providers_are_independent():
    db = _db()
    usage.try_spend(db, "apify", 100, budget=100, month="2026-06")
    # Otra clave de proveedor tiene su propia cuota.
    assert usage.try_spend(db, "otro", 10, budget=100, month="2026-06") == 10


def test_non_positive_args_grant_zero():
    db = _db()
    assert usage.try_spend(db, "apify", 0, budget=100, month="2026-06") == 0
    assert usage.try_spend(db, "apify", 10, budget=0, month="2026-06") == 0
    assert usage.try_spend(db, "apify", -5, budget=100, month="2026-06") == 0
    assert usage.get_used(db, "apify", "2026-06") == 0


def test_status_summary():
    db = _db()
    usage.try_spend(db, "apify", 25, budget=100, month="2026-06")
    s = usage.status(db, "apify", 100, "2026-06")
    assert s == {"provider": "apify", "month": "2026-06", "used": 25,
                 "budget": 100, "remaining": 75}


def test_current_month_format():
    from datetime import datetime, timezone
    assert usage.current_month(datetime(2026, 6, 1, tzinfo=timezone.utc)) == "2026-06"
    assert usage.current_month(datetime(2026, 12, 31, 23, 59, tzinfo=timezone.utc)) == "2026-12"
