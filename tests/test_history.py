"""Pruebas del historial de saciedad por usuario."""
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from backend.database import Base
from backend.models import Feedback, Plan, User
from backend.services import satiety_history


def _db():
    engine = create_engine("sqlite:///:memory:", connect_args={"check_same_thread": False})
    Base.metadata.create_all(engine)
    return sessionmaker(bind=engine)()


def test_satiety_history_empty():
    db = _db()
    db.add(User(name="U")); db.commit()
    h = satiety_history(db, 1)
    assert h["count"] == 0 and h["entries"] == [] and h["avg_satiety"] == 0.0


def test_satiety_history_aggregates():
    db = _db()
    db.add(User(name="U")); db.commit()
    for i, (sat, cost) in enumerate([(2, 5), (4, 3)], start=1):
        plan = Plan(user_id=1, title=f"Min {i}", scope="diario",
                    payload={"meals": []}, total_cost_clp=1000 * i)
        db.add(plan); db.flush()
        db.add(Feedback(plan_id=plan.id, satiety_score=sat, cost_score=cost))
    db.commit()

    h = satiety_history(db, 1)
    assert h["count"] == 2
    assert h["avg_satiety"] == 3.0      # (2 + 4) / 2
    assert h["avg_cost_score"] == 4.0   # (5 + 3) / 2
    assert [e["satiety_score"] for e in h["entries"]] == [2, 4]
    assert h["entries"][0]["title"] == "Min 1"
    assert h["entries"][1]["total_cost_clp"] == 2000
