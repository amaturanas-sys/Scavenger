"""Endpoints de generacion y guardado de minutas."""
from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session

from ..database import get_db
from ..models import Plan, User
from ..schemas import (
    GeneratePlanRequest,
    PlanOut,
    SavePlanRequest,
    ShoppingListOut,
    ShoppingListRequest,
)
from ..services import generate_daily_plan, generate_weekly_plan
from ..shopping import build_shopping_list

router = APIRouter(prefix="/api/plans", tags=["minutas"])


@router.post("/shopping-list", response_model=ShoppingListOut)
def shopping_list_from_payload(req: ShoppingListRequest, db: Session = Depends(get_db)):
    """Lista de compras consolidada por cadena para una minuta (no guardada)."""
    return build_shopping_list(db, req.payload)


@router.post("/generate")
def generate_plan(payload: GeneratePlanRequest, db: Session = Depends(get_db)):
    """Genera una minuta (no la guarda) optimizada de lo mas economico hacia arriba."""
    user = db.get(User, payload.user_id)
    if not user:
        raise HTTPException(404, "Usuario no encontrado")

    if payload.scope == "semanal":
        return {
            "scope": "semanal",
            "data": generate_weekly_plan(
                db, user, satiety_emphasis=payload.satiety_emphasis, use_budget=payload.use_budget
            ),
        }
    return {
        "scope": "diario",
        "data": generate_daily_plan(
            db, user, satiety_emphasis=payload.satiety_emphasis, use_budget=payload.use_budget
        ),
    }


@router.post("", response_model=PlanOut, status_code=201)
def save_plan(payload: SavePlanRequest, db: Session = Depends(get_db)):
    user = db.get(User, payload.user_id)
    if not user:
        raise HTTPException(404, "Usuario no encontrado")

    totals = (payload.payload or {}).get("totals", {})
    plan = Plan(
        user_id=payload.user_id,
        title=payload.title or "Minuta",
        scope=payload.scope,
        payload=payload.payload,
        total_cost_clp=totals.get("cost_clp", 0.0),
        total_kcal=totals.get("kcal", 0.0),
        satiety_score=totals.get("satiety", 0.0),
    )
    db.add(plan)
    db.commit()
    db.refresh(plan)
    return plan


@router.get("", response_model=list[PlanOut])
def list_plans(user_id: int | None = None, db: Session = Depends(get_db)):
    query = db.query(Plan)
    if user_id is not None:
        query = query.filter(Plan.user_id == user_id)
    return query.order_by(Plan.created_at.desc()).all()


@router.get("/{plan_id}", response_model=PlanOut)
def get_plan(plan_id: int, db: Session = Depends(get_db)):
    plan = db.get(Plan, plan_id)
    if not plan:
        raise HTTPException(404, "Minuta no encontrada")
    return plan


@router.get("/{plan_id}/shopping-list", response_model=ShoppingListOut)
def get_plan_shopping_list(plan_id: int, db: Session = Depends(get_db)):
    """Lista de compras consolidada por cadena para una minuta guardada."""
    plan = db.get(Plan, plan_id)
    if not plan:
        raise HTTPException(404, "Minuta no encontrada")
    return build_shopping_list(db, plan.payload or {})


@router.delete("/{plan_id}", status_code=204)
def delete_plan(plan_id: int, db: Session = Depends(get_db)):
    plan = db.get(Plan, plan_id)
    if not plan:
        raise HTTPException(404, "Minuta no encontrada")
    db.delete(plan)
    db.commit()
