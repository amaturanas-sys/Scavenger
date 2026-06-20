"""Endpoints de feedback de saciedad y aprendizaje de preferencias."""
from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session

from ..database import get_db
from ..learning import apply_feedback
from ..models import Feedback, Plan
from ..schemas import FeedbackOut, FeedbackRequest

router = APIRouter(prefix="/api/plans", tags=["feedback"])


@router.post("/{plan_id}/feedback", response_model=FeedbackOut, status_code=201)
def submit_feedback(plan_id: int, payload: FeedbackRequest, db: Session = Depends(get_db)):
    """Registra el feedback de una minuta y actualiza las preferencias del usuario."""
    plan = db.get(Plan, plan_id)
    if not plan:
        raise HTTPException(404, "Minuta no encontrada")

    feedback = Feedback(
        plan_id=plan_id,
        satiety_score=payload.satiety_score,
        cost_score=payload.cost_score,
        notes=payload.notes,
        food_ratings=payload.food_ratings,
    )
    db.add(feedback)
    db.flush()

    updated = apply_feedback(db, plan, feedback)
    # Refleja la saciedad reportada en la minuta para historico.
    plan.satiety_score = payload.satiety_score
    db.commit()
    db.refresh(feedback)

    return FeedbackOut(
        id=feedback.id, plan_id=plan_id, satiety_score=feedback.satiety_score,
        cost_score=feedback.cost_score, notes=feedback.notes, updated_preferences=updated,
    )


@router.get("/{plan_id}/feedback", response_model=list[FeedbackOut])
def list_feedback(plan_id: int, db: Session = Depends(get_db)):
    plan = db.get(Plan, plan_id)
    if not plan:
        raise HTTPException(404, "Minuta no encontrada")
    return [
        FeedbackOut(
            id=fb.id, plan_id=plan_id, satiety_score=fb.satiety_score,
            cost_score=fb.cost_score, notes=fb.notes, updated_preferences={},
        )
        for fb in plan.feedback
    ]
