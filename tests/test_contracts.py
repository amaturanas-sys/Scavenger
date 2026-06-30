"""Contratos de la API: FoodOut.ean y PlanOut.created_at (A1, A2)."""
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from backend.database import Base
from backend.models import Food, Plan, User
from backend.routers.foods import _to_out
from backend.schemas import PlanOut
from backend.seed import seed_foods


def _db():
    eng = create_engine("sqlite:///:memory:", connect_args={"check_same_thread": False})
    Base.metadata.create_all(eng)
    db = sessionmaker(bind=eng)()
    return db


def test_foodout_includes_ean():
    db = _db(); seed_foods(db, "local", refresh=False)
    food = db.query(Food).filter(Food.id == "arroz_grado2").one()
    food.ean = "7801234567895"; db.commit()
    out = _to_out(food)
    assert out.ean == "7801234567895"


def test_planout_includes_created_at():
    db = _db()
    db.add(User(name="U")); db.commit()
    plan = Plan(user_id=1, title="X", scope="diario", payload={"meals": []})
    db.add(plan); db.commit(); db.refresh(plan)
    out = PlanOut.model_validate(plan)
    assert out.created_at is not None
    # Serializa a ISO 8601 (lo que el frontend recorta a AAAA-MM-DD).
    assert "T" in out.model_dump(mode="json")["created_at"]
