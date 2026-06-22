"""Endpoints del constructor de comidas (tragamonedas por roles)."""
from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException
from pydantic import BaseModel
from sqlalchemy.orm import Session

from ..builder import (
    MEAL_TEMPLATES,
    ROLE_LABELS,
    build_slots,
    meal_target,
    random_meal,
    summarize,
)
from ..database import get_db
from ..models import User
from ..services import user_requirements

router = APIRouter(prefix="/api/builder", tags=["constructor"])


class BuilderRequest(BaseModel):
    user_id: int
    meal: str = "almuerzo"


class SummaryRequest(BaseModel):
    user_id: int
    meal: str = "almuerzo"
    items: list[dict] = []


def _get_user(db: Session, user_id: int) -> User:
    user = db.get(User, user_id)
    if not user:
        raise HTTPException(404, "Usuario no encontrado")
    return user


@router.get("/meals")
def list_meals():
    """Plantillas de comida disponibles (carretes y fracción de la meta diaria)."""
    return [
        {"meal": name, "fraction": tpl["fraction"],
         "slots": [{"role": r, "label": ROLE_LABELS.get(r, r)} for r in tpl["slots"]]}
        for name, tpl in MEAL_TEMPLATES.items()
    ]


@router.post("/slots")
def slots(req: BuilderRequest, db: Session = Depends(get_db)):
    """Candidatos pre-porcionados por rol (carretes) para una comida."""
    return build_slots(db, _get_user(db, req.user_id), req.meal)


@router.post("/random")
def random(req: BuilderRequest, db: Session = Depends(get_db)):
    """Una combinación aleatoria (un candidato por carrete) + totales."""
    return random_meal(db, _get_user(db, req.user_id), req.meal)


@router.post("/summary")
def summary(req: SummaryRequest, db: Session = Depends(get_db)):
    """Suma de una selección y su ajuste contra la meta de la comida."""
    user = _get_user(db, req.user_id)
    target = meal_target(user_requirements(user), req.meal)
    return {"meal": req.meal, "target": target, **summarize(req.items, target)}
