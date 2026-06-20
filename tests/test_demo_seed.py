"""Pruebas del seed de usuario demo (idempotente)."""
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from backend.database import Base
from backend.models import User
from backend.seed import seed_demo_user


def _db():
    engine = create_engine("sqlite:///:memory:", connect_args={"check_same_thread": False})
    Base.metadata.create_all(engine)
    return sessionmaker(bind=engine)()


def test_seed_demo_user_creates_once_and_is_idempotent():
    db = _db()
    assert seed_demo_user(db) is True
    assert db.query(User).count() == 1

    demo = db.query(User).first()
    assert demo.name == "Demo"
    assert "mayorista10" in demo.preferred_retailers

    # Segunda llamada no crea otro usuario.
    assert seed_demo_user(db) is False
    assert db.query(User).count() == 1
