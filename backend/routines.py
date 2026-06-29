"""Logica de presets de rutinas (comidas fijas que se repiten en el calendario).

Una rutina se aplica a los dias cuyo dia de semana calza con su preset. El dia
de semana sigue la convencion de Python (`date.weekday()`): lunes=0 ... domingo=6.
"""
from __future__ import annotations

# Preset -> conjunto de dias de semana (lunes=0 ... domingo=6).
PRESET_WEEKDAYS: dict[str, set[int]] = {
    "L-V": {0, 1, 2, 3, 4},
    "finde": {5, 6},
    "todos": {0, 1, 2, 3, 4, 5, 6},
}

PRESET_LABELS = {
    "L-V": "Lunes a viernes",
    "finde": "Fin de semana",
    "todos": "Todos los días",
}


def normalize_preset(preset: str | None) -> str:
    """Devuelve un preset valido; cae a 'todos' si es desconocido."""
    return preset if preset in PRESET_WEEKDAYS else "todos"


def preset_matches(preset: str | None, weekday: int) -> bool:
    """True si el preset aplica al dia de semana dado (lunes=0 ... domingo=6)."""
    return weekday in PRESET_WEEKDAYS[normalize_preset(preset)]
