"""Aprendizaje adaptativo a partir del feedback del usuario.

Cada vez que el usuario califica una minuta (saciedad, costo y gusto por
alimento), se actualizan las preferencias por alimento. Estas preferencias
abaratan o encarecen el "costo efectivo" en el optimizador, de modo que
las sugerencias se adaptan progresivamente al perfil del usuario.
"""
from __future__ import annotations

from sqlalchemy.orm import Session

from . import config
from .models import Feedback, Plan, Preference


def _get_or_create_pref(db: Session, user_id: int, food_id: str) -> Preference:
    pref = (
        db.query(Preference)
        .filter(Preference.user_id == user_id, Preference.food_id == food_id)
        .one_or_none()
    )
    if pref is None:
        pref = Preference(user_id=user_id, food_id=food_id, weight=0.0, times_used=0)
        db.add(pref)
    return pref


def _clamp(x: float, lo: float = -1.0, hi: float = 1.0) -> float:
    return max(lo, min(hi, x))


def apply_feedback(db: Session, plan: Plan, feedback: Feedback) -> dict[str, float]:
    """Actualiza las preferencias del usuario segun el feedback de una minuta.

    Reglas:
      * Saciedad baja (<3): el usuario quedo con hambre -> se favorecen los
        alimentos mas saciantes de la minuta y se penalizan los menos
        saciantes, para que el sistema empuje hacia mayor saciedad.
      * Saciedad alta (>3): refuerza levemente lo consumido (funciono bien).
      * food_ratings explicitos (gusto -1..1) ajustan directamente el peso.
    """
    lr = config.LEARNING_RATE
    user_id = plan.user_id
    updated: dict[str, float] = {}
    cache: dict[str, Preference] = {}

    def pref_for(food_id: str) -> Preference:
        if food_id not in cache:
            cache[food_id] = _get_or_create_pref(db, user_id, food_id)
        return cache[food_id]

    items = []
    for meal in plan.payload.get("meals", []):
        items.extend(meal.get("items", []))
    if not items:
        items = plan.payload.get("items", [])

    # Agrega la saciedad por alimento (el planner divide un alimento en varias
    # comidas, por lo que un mismo food_id puede aparecer repetido).
    satiety_values: dict[str, float] = {}
    for it in items:
        satiety_values[it["food_id"]] = satiety_values.get(it["food_id"], 0.0) + it.get("satiety_contrib", 0.0)
    max_sat = max(satiety_values.values()) if satiety_values else 1.0
    max_sat = max_sat or 1.0

    hunger_gap = (3 - feedback.satiety_score) / 2.0  # >0 si quedo con hambre

    for fid in satiety_values:
        pref = pref_for(fid)
        delta = 0.0

        # Componente de saciedad: si hubo hambre, premia los mas saciantes.
        if hunger_gap > 0:
            rel_sat = satiety_values.get(fid, 0.0) / max_sat  # 0..1
            delta += lr * hunger_gap * (rel_sat - 0.5) * 2.0
        elif feedback.satiety_score > 3:
            # Quedo bien: refuerzo leve de lo consumido.
            delta += lr * 0.2

        pref.weight = _clamp(pref.weight + delta)
        pref.times_used += 1
        updated[fid] = pref.weight

    # Ajustes explicitos de gusto por alimento (tienen prioridad).
    for fid, rating in (feedback.food_ratings or {}).items():
        pref = pref_for(fid)
        pref.weight = _clamp(pref.weight + lr * float(rating))
        updated[fid] = pref.weight

    db.flush()
    return updated


def get_preferences(db: Session, user_id: int) -> dict[str, float]:
    """Devuelve el mapa {food_id: peso} de preferencias del usuario."""
    rows = db.query(Preference).filter(Preference.user_id == user_id).all()
    return {r.food_id: r.weight for r in rows}
