"""Pruebas de la lista de compras consolidada por cadena."""
import math

from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker

from backend.database import Base
from backend.models import Food
from backend.seed import seed_foods
from backend.shopping import build_shopping_list


def _db():
    engine = create_engine("sqlite:///:memory:", connect_args={"check_same_thread": False})
    Base.metadata.create_all(engine)
    db = sessionmaker(bind=engine)()
    seed_foods(db, "local", refresh=False)
    return db


def _cheapest(food: Food):
    fp = min(food.prices, key=lambda p: p.price_per_g)
    return fp.retailer, fp.package_g, fp.price_clp


def test_shopping_list_groups_by_retailer_and_counts_packages():
    db = _db()
    arroz = db.query(Food).filter(Food.id == "arroz_grado2").one()
    lentejas = db.query(Food).filter(Food.id == "lentejas").one()
    r_arroz, pkg_arroz, price_arroz = _cheapest(arroz)
    r_lentejas, pkg_lentejas, price_lentejas = _cheapest(lentejas)

    # 1.5 envases de arroz -> debe comprar 2; 0.4 de lentejas -> 1.
    grams_arroz = pkg_arroz * 1.5
    grams_lentejas = pkg_lentejas * 0.4
    payload = {"meals": [
        {"meal": "almuerzo", "items": [
            {"food_id": "arroz_grado2", "name": "Arroz", "retailer": r_arroz,
             "grams": grams_arroz, "cost_clp": arroz.price_per_g * grams_arroz},
            {"food_id": "lentejas", "name": "Lentejas", "retailer": r_lentejas,
             "grams": grams_lentejas, "cost_clp": lentejas.price_per_g * grams_lentejas},
        ]},
    ]}

    sl = build_shopping_list(db, payload)
    by_name = {r["retailer"]: r for r in sl["retailers"]}
    assert r_arroz in by_name

    arroz_item = next(i for r in sl["retailers"] for i in r["items"] if i["food_id"] == "arroz_grado2")
    assert arroz_item["packages"] == 2
    assert arroz_item["packages_cost_clp"] == round(2 * price_arroz, 1)
    assert arroz_item["needed_g"] == round(grams_arroz, 1)

    lentejas_item = next(i for r in sl["retailers"] for i in r["items"] if i["food_id"] == "lentejas")
    assert lentejas_item["packages"] == 1

    # El total por envases coincide con la suma de subtotales por cadena.
    assert sl["total_packages_clp"] == round(sum(r["subtotal_packages_clp"] for r in sl["retailers"]), 1)


def test_shopping_list_aggregates_same_food_across_meals():
    db = _db()
    arroz = db.query(Food).filter(Food.id == "arroz_grado2").one()
    r, pkg, _ = _cheapest(arroz)
    payload = {"meals": [
        {"meal": "almuerzo", "items": [{"food_id": "arroz_grado2", "name": "Arroz",
                                        "retailer": r, "grams": pkg * 0.6, "cost_clp": 100}]},
        {"meal": "cena", "items": [{"food_id": "arroz_grado2", "name": "Arroz",
                                    "retailer": r, "grams": pkg * 0.6, "cost_clp": 100}]},
    ]}
    sl = build_shopping_list(db, payload)
    items = [i for rr in sl["retailers"] for i in rr["items"] if i["food_id"] == "arroz_grado2"]
    # Un solo item agregado (no dos), con 1.2 envases -> 2 a comprar.
    assert len(items) == 1
    assert items[0]["packages"] == math.ceil(1.2)


def test_shopping_list_handles_weekly_payload():
    db = _db()
    arroz = db.query(Food).filter(Food.id == "arroz_grado2").one()
    r, pkg, _ = _cheapest(arroz)
    day_plan = {"meals": [{"meal": "almuerzo", "items": [
        {"food_id": "arroz_grado2", "name": "Arroz", "retailer": r, "grams": pkg, "cost_clp": 50}]}]}
    payload = {"days": [{"day": "lunes", "plan": day_plan}, {"day": "martes", "plan": day_plan}]}
    sl = build_shopping_list(db, payload)
    item = next(i for rr in sl["retailers"] for i in rr["items"] if i["food_id"] == "arroz_grado2")
    assert item["needed_g"] == round(2 * pkg, 1)  # dos dias agregados
    assert item["packages"] == 2
