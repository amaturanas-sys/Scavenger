"""Pruebas de rutinas (comidas fijas con preset semanal)."""
import pytest
from fastapi import HTTPException
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from backend.database import Base
from backend.models import Routine, User
from backend.routines import normalize_preset, preset_matches
from backend.routers import routines as routines_router
from backend.schemas import RoutineCreate


def _db():
    engine = create_engine("sqlite:///:memory:", connect_args={"check_same_thread": False})
    Base.metadata.create_all(engine)
    return sessionmaker(bind=engine)()


def test_preset_matches_weekdays():
    # lunes=0 ... domingo=6
    assert all(preset_matches("L-V", d) for d in range(0, 5))
    assert not any(preset_matches("L-V", d) for d in (5, 6))
    assert preset_matches("finde", 5) and preset_matches("finde", 6)
    assert not preset_matches("finde", 0)
    assert all(preset_matches("todos", d) for d in range(0, 7))


def test_normalize_preset_falls_back():
    assert normalize_preset("L-V") == "L-V"
    assert normalize_preset("cualquiera") == "todos"
    assert normalize_preset(None) == "todos"


def test_create_and_list_routines():
    db = _db()
    db.add(User(name="U")); db.commit()
    routines_router.create_routine(
        RoutineCreate(user_id=1, meal="desayuno", preset="L-V", title="Avena",
                      items=[{"food_id": "avena", "kcal": 300}],
                      subtotal={"kcal": 300, "cost_clp": 400}), db)
    routines_router.create_routine(
        RoutineCreate(user_id=1, meal="cena", preset="finde", title="Pizza",
                      items=[], subtotal={}), db)

    todas = routines_router.list_routines(user_id=1, db=db)
    assert len(todas) == 2

    # Lunes (weekday 0): solo la rutina L-V.
    lun = routines_router.list_routines(user_id=1, weekday=0, db=db)
    assert [r.meal for r in lun] == ["desayuno"]
    # Sabado (weekday 5): solo la de finde.
    sab = routines_router.list_routines(user_id=1, weekday=5, db=db)
    assert [r.meal for r in sab] == ["cena"]


def test_create_routine_normalizes_unknown_preset():
    db = _db()
    db.add(User(name="U")); db.commit()
    r = routines_router.create_routine(
        RoutineCreate(user_id=1, meal="almuerzo", preset="raro"), db)
    assert r.preset == "todos"


def test_create_routine_requires_existing_user():
    db = _db()
    with pytest.raises(HTTPException):
        routines_router.create_routine(RoutineCreate(user_id=99, meal="cena"), db)


def test_delete_routine():
    db = _db()
    db.add(User(name="U")); db.commit()
    r = routines_router.create_routine(RoutineCreate(user_id=1, meal="cena"), db)
    routines_router.delete_routine(r.id, db)
    assert db.query(Routine).count() == 0
    with pytest.raises(HTTPException):
        routines_router.delete_routine(r.id, db)
