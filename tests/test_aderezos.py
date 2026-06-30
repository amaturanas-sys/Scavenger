"""Pruebas del repertorio ampliado de aderezos y el orden por preferencias."""
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from backend.builder import build_slots
from backend.database import Base
from backend.models import Food, Preference, User
from backend.seed import seed_foods


def _db():
    engine = create_engine("sqlite:///:memory:", connect_args={"check_same_thread": False})
    Base.metadata.create_all(engine)
    db = sessionmaker(bind=engine)()
    seed_foods(db, "local", refresh=False)
    return db


def test_aderezo_repertoire_expanded():
    db = _db()
    ids = {f.id for f in db.query(Food).filter(Food.category == "otro").all()}
    for fid in ("mostaza", "mayonesa", "ketchup", "salsa_soya", "vinagre_tinto", "pebre", "salsa_bbq", "pesto"):
        assert fid in ids, f"falta el aderezo {fid}"


def test_builder_candidate_has_pref_default_zero():
    db = _db()
    db.add(User(name="U", preferred_retailers=[])); db.commit()
    u = db.query(User).first()
    data = build_slots(db, u, "almuerzo")
    ad = [s for s in data["slots"] if s["role"] == "aderezo"][0]["candidates"]
    assert ad and all("pref" in c for c in ad)
    assert all(c["pref"] == 0.0 for c in ad)


def test_builder_pref_reflects_preference():
    db = _db()
    db.add(User(name="U", preferred_retailers=[])); db.commit()
    u = db.query(User).first()
    db.add(Preference(user_id=u.id, food_id="mayonesa", weight=0.9)); db.commit()
    data = build_slots(db, u, "almuerzo")
    ad = [s for s in data["slots"] if s["role"] == "aderezo"][0]["candidates"]
    pref_of = {c["food_id"]: c["pref"] for c in ad}
    assert pref_of.get("mayonesa") == 0.9
    # El de mayor preferencia debe poder quedar primero al ordenar por 'pref'.
    top = max(ad, key=lambda c: c["pref"])
    assert top["food_id"] == "mayonesa"
