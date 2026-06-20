"""Endpoints del catalogo de alimentos."""
from __future__ import annotations

from fastapi import APIRouter, Depends, Query
from sqlalchemy.orm import Session

from ..database import get_db
from ..models import Food
from ..schemas import FoodOut

router = APIRouter(prefix="/api/foods", tags=["alimentos"])


def _to_out(f: Food) -> FoodOut:
    return FoodOut(
        id=f.id, name=f.name, brand=f.brand, category=f.category, retailer=f.retailer,
        package_g=f.package_g, price_clp=f.price_clp, price_per_100g=round(f.price_per_100g, 1),
        serving_g=f.serving_g, satiety_index=f.satiety_index, kcal=f.kcal,
        protein_g=f.protein_g, carb_g=f.carb_g, fat_g=f.fat_g, fiber_g=f.fiber_g, tags=f.tags or [],
    )


@router.get("", response_model=list[FoodOut])
def list_foods(
    q: str | None = Query(None, description="Busqueda por nombre/marca/categoria"),
    category: str | None = None,
    db: Session = Depends(get_db),
):
    query = db.query(Food)
    if category:
        query = query.filter(Food.category == category)
    foods = query.order_by(Food.name).all()
    if q:
        ql = q.lower()
        foods = [f for f in foods if ql in f.name.lower() or ql in f.brand.lower() or ql in f.category.lower()]
    return [_to_out(f) for f in foods]


@router.get("/categories", response_model=list[str])
def list_categories(db: Session = Depends(get_db)):
    rows = db.query(Food.category).distinct().all()
    return sorted({r[0] for r in rows})
