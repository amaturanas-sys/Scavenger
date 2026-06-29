"""Endpoints de rutinas (comidas fijas que se repiten en el calendario)."""
from __future__ import annotations

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.orm import Session

from ..database import get_db
from ..models import Routine, User
from ..routines import normalize_preset, preset_matches
from ..schemas import RoutineCreate, RoutineOut

router = APIRouter(prefix="/api/routines", tags=["rutinas"])


@router.post("", response_model=RoutineOut, status_code=201)
def create_routine(payload: RoutineCreate, db: Session = Depends(get_db)):
    if not db.get(User, payload.user_id):
        raise HTTPException(404, "Usuario no encontrado")
    routine = Routine(
        user_id=payload.user_id,
        meal=payload.meal,
        preset=normalize_preset(payload.preset),
        title=payload.title,
        items=payload.items,
        subtotal=payload.subtotal,
    )
    db.add(routine)
    db.commit()
    db.refresh(routine)
    return routine


@router.get("", response_model=list[RoutineOut])
def list_routines(
    user_id: int | None = None,
    weekday: int | None = None,
    db: Session = Depends(get_db),
):
    """Lista rutinas; si se pasa weekday (lunes=0..domingo=6) filtra por preset."""
    query = db.query(Routine)
    if user_id is not None:
        query = query.filter(Routine.user_id == user_id)
    routines = query.order_by(Routine.id).all()
    if weekday is not None:
        routines = [r for r in routines if preset_matches(r.preset, weekday)]
    return routines


@router.delete("/{routine_id}", status_code=204)
def delete_routine(routine_id: int, db: Session = Depends(get_db)):
    routine = db.get(Routine, routine_id)
    if not routine:
        raise HTTPException(404, "Rutina no encontrada")
    db.delete(routine)
    db.commit()
