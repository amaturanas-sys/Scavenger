"""Pruebas de las especificaciones del perfil (proteína mín/plato, migración)."""
from sqlalchemy import create_engine, inspect, text

from backend import database
from backend.builder import meal_target


class _Req:
    kcal = 2000.0
    protein_g = 100.0
    carb_g = 250.0
    fat_g = 60.0


def test_meal_target_protein_floor():
    # almuerzo = 0.35 del dia -> proteina base 35 g.
    base = meal_target(_Req(), "almuerzo")
    assert base["protein_g"] == 35.0
    # Con piso de 50 g por plato, el objetivo de proteina sube a 50.
    floored = meal_target(_Req(), "almuerzo", min_protein=50.0)
    assert floored["protein_g"] == 50.0
    # Si el piso es menor al base, no lo baja.
    assert meal_target(_Req(), "almuerzo", min_protein=10.0)["protein_g"] == 35.0


def test_ensure_columns_adds_missing(tmp_path):
    eng = create_engine(f"sqlite:///{tmp_path}/m.db")
    # Tabla 'users' vieja, sin las columnas nuevas.
    with eng.begin() as c:
        c.execute(text("CREATE TABLE users (id INTEGER PRIMARY KEY, name TEXT)"))
    database._ensure_columns(eng)
    cols = {col["name"] for col in inspect(eng).get_columns("users")}
    for new in ("meals_per_day", "monthly_budget_clp", "min_protein_per_meal_g"):
        assert new in cols
    # Idempotente: una segunda pasada no falla.
    database._ensure_columns(eng)
