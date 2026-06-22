"""Prueba del exportador de catalogo (round-trip BD -> JSON -> proveedor)."""
from pathlib import Path

from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from backend.database import Base
from backend.export_catalog import export_catalog
from backend.models import Food
from backend.providers.local import LocalDatasetProvider
from backend.seed import seed_foods


def _db():
    engine = create_engine("sqlite:///:memory:", connect_args={"check_same_thread": False})
    Base.metadata.create_all(engine)
    db = sessionmaker(bind=engine)()
    seed_foods(db, "local", refresh=False)
    return db


def test_export_roundtrips_through_local_provider(tmp_path: Path):
    db = _db()
    n_db = db.query(Food).count()

    out = tmp_path / "catalogo.json"
    n_exported = export_catalog(db, out)
    assert n_exported == n_db
    assert out.exists()

    # El proveedor local debe poder releer lo exportado.
    records = LocalDatasetProvider(out).fetch_foods()
    assert len(records) == n_db

    # Un alimento conocido conserva nutricion y precios por cadena.
    arroz = next(r for r in records if r.id == "arroz_grado2")
    assert arroz.kcal == 358
    assert len(arroz.prices) >= 4
    # El precio por defecto sigue siendo el mas barato.
    assert arroz.price_clp == min(p["price_clp"] for p in arroz.prices)


def test_export_reflects_db_edits(tmp_path: Path):
    db = _db()
    food = db.query(Food).filter(Food.id == "arroz_grado2").one()
    food.kcal = 999.0
    fp = food.prices[0]
    fp.price_clp = 12345.0
    db.commit()

    out = tmp_path / "c.json"
    export_catalog(db, out)
    records = LocalDatasetProvider(out).fetch_foods()
    arroz = next(r for r in records if r.id == "arroz_grado2")
    assert arroz.kcal == 999.0
    assert any(p["price_clp"] == 12345.0 for p in arroz.prices)
