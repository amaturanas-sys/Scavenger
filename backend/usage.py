"""Control de cuota mensual para proveedores de scraping con plan gratuito.

Lleva la cuenta del consumo del mes (persistido en la BD, compartido entre la
app desplegada y los jobs de GitHub Actions) y evita exceder un presupuesto
configurable, de modo que el uso de Apify (Jumbo / Lider / Santa Isabel /
Unimarc) se mantenga dentro del plan gratuito.

El medidor "real" lo lleva Apify; esto es un freno propio para no malgastar el
credito. La garantia dura de no pagar es NO cargar un metodo de pago en Apify
(sin tarjeta, Apify se detiene al agotar el credito en vez de cobrar).
"""
from __future__ import annotations

from datetime import datetime, timezone

from sqlalchemy.orm import Session

from .models import ScraperUsage


def current_month(now: datetime | None = None) -> str:
    """Mes calendario en UTC como 'YYYY-MM' (clave de la cuota mensual)."""
    now = now or datetime.now(timezone.utc)
    return now.strftime("%Y-%m")


def get_used(db: Session, provider: str, month: str | None = None) -> int:
    """Unidades ya consumidas por `provider` en el mes (0 si no hay registro)."""
    month = month or current_month()
    row = db.query(ScraperUsage).filter_by(provider=provider, month=month).one_or_none()
    return row.used if row else 0


def remaining(db: Session, provider: str, budget: int, month: str | None = None) -> int:
    """Unidades disponibles antes de tocar el presupuesto mensual."""
    return max(0, budget - get_used(db, provider, month))


def try_spend(
    db: Session, provider: str, units: int, budget: int, month: str | None = None
) -> int:
    """Reserva hasta `units` del presupuesto mensual sin pasarse.

    Devuelve cuanto se concedio efectivamente (0 si ya se agoto la cuota o si
    los argumentos no son positivos). Persiste el consumo de inmediato para que
    el conteo sea consistente entre procesos (app + Actions).
    """
    if units <= 0 or budget <= 0:
        return 0
    month = month or current_month()
    row = db.query(ScraperUsage).filter_by(provider=provider, month=month).one_or_none()
    used = row.used if row else 0
    grant = max(0, min(units, budget - used))
    if grant <= 0:
        return 0
    if row is None:
        row = ScraperUsage(provider=provider, month=month, used=0)
        db.add(row)
        db.flush()
    row.used = used + grant
    db.commit()
    return grant


def status(db: Session, provider: str, budget: int, month: str | None = None) -> dict:
    """Resumen legible de la cuota del mes (para mostrar en la app/logs)."""
    month = month or current_month()
    used = get_used(db, provider, month)
    return {
        "provider": provider,
        "month": month,
        "used": used,
        "budget": budget,
        "remaining": max(0, budget - used),
    }
