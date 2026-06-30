"""Pruebas del puntaje de micronutrientes (paso 3 del armado guiado)."""
from sqlalchemy import create_engine, inspect, text

from backend import database
from backend.builder import micro_score


class _Rich:
    fiber_g = 10.0; vitamin_c_mg = 40.0; vitamin_e_mg = 6.0
    iron_mg = 7.0; zinc_mg = 5.0; calcium_mg = 400.0


class _Plain:
    fiber_g = 0.0; vitamin_c_mg = 0.0; vitamin_e_mg = 0.0
    iron_mg = 0.0; zinc_mg = 0.0; calcium_mg = 0.0


def test_micro_score_rewards_richness():
    rich = micro_score(_Rich(), 100)
    plain = micro_score(_Plain(), 100)
    assert plain == 0.0
    assert 0 < rich <= 100
    assert rich > plain


def test_micro_score_scales_and_caps():
    f = _Rich()
    assert micro_score(f, 200) >= micro_score(f, 100)   # más porción, más aporte
    # Cada micro se topa al 100% RDA, así el promedio nunca pasa de 100.
    assert micro_score(f, 100000) <= 100.0


def test_micro_score_missing_attrs_safe():
    class _Partial:
        fiber_g = 5.0  # sin el resto de atributos
    assert micro_score(_Partial(), 100) >= 0.0


def test_ensure_columns_adds_micros(tmp_path):
    eng = create_engine(f"sqlite:///{tmp_path}/m.db")
    with eng.begin() as c:
        c.execute(text("CREATE TABLE foods (id TEXT PRIMARY KEY, name TEXT, category TEXT)"))
    database._ensure_columns(eng)
    cols = {col["name"] for col in inspect(eng).get_columns("foods")}
    assert "zinc_mg" in cols and "vitamin_e_mg" in cols
