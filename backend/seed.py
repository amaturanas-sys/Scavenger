"""Carga el catalogo de alimentos a la base de datos desde un proveedor."""
from __future__ import annotations

import time
from collections.abc import Callable

from sqlalchemy.orm import Session

from .database import SessionLocal, init_db
from .models import Food, FoodPrice, User
from .providers import get_provider


def seed_foods(db: Session, provider_name: str = "local", refresh: bool = False) -> int:
    """Inserta/actualiza los alimentos del proveedor en la BD.

    Devuelve la cantidad de alimentos cargados.
    """
    provider = get_provider(provider_name)
    records = provider.fetch_foods()
    if not records:
        return 0

    count = 0
    for r in records:
        existing = db.get(Food, r.id)
        data = dict(
            name=r.name, brand=r.brand, category=r.category, retailer=r.retailer,
            package_g=r.package_g, price_clp=r.price_clp, serving_g=r.serving_g,
            max_servings_day=r.max_servings_day, satiety_index=r.satiety_index,
            kcal=r.kcal, protein_g=r.protein_g, carb_g=r.carb_g, fat_g=r.fat_g,
            fiber_g=r.fiber_g, sodium_mg=r.sodium_mg, calcium_mg=r.calcium_mg,
            iron_mg=r.iron_mg, potassium_mg=r.potassium_mg, vitamin_c_mg=r.vitamin_c_mg,
            tags=r.tags,
        )
        if existing is None:
            food = Food(id=r.id, ean=r.ean, **data)
            db.add(food)
            _sync_prices(db, food, r.prices)
            count += 1
        elif refresh:
            for k, v in data.items():
                setattr(existing, k, v)
            # Solo sobrescribe el EAN si el catalogo trae uno; preserva el que se
            # haya autoaprendido desde un scraping (no lo borra en cada refresh).
            if r.ean:
                existing.ean = r.ean
            _sync_prices(db, existing, r.prices)
            count += 1
    db.commit()
    return count


def _sync_prices(db: Session, food: Food, prices: list[dict]) -> None:
    """Reemplaza los precios por cadena de un alimento."""
    food.prices.clear()
    db.flush()
    for p in prices:
        food.prices.append(FoodPrice(
            retailer=p.get("retailer", ""),
            retailer_id=p.get("retailer_id", ""),
            price_clp=float(p.get("price_clp", 0)),
            package_g=float(p.get("package_g", food.package_g)),
        ))


def seed_demo_user(db: Session) -> bool:
    """Crea un usuario de demostracion si no existe ninguno.

    Hace que la app sea usable apenas se abre (perfil de ejemplo). Devuelve
    True si lo creo, False si ya habia usuarios.
    """
    if db.query(User).count() > 0:
        return False
    db.add(User(
        name="Demo", sex="M", age=30, weight_kg=75, height_cm=175,
        activity_level="moderado", goal="mantener", daily_budget_clp=4000,
        diet_tags=[], excluded_foods=[],
        preferred_retailers=["lider", "mayorista10", "jumbo"],
    ))
    db.commit()
    return True


def run_with_retries(
    fn: Callable,
    attempts: int = 8,
    delay: float = 3.0,
    sleeper: Callable[[float], None] = time.sleep,
    log: Callable[[str], None] = print,
):
    """Ejecuta fn() reintentando ante excepciones. Devuelve (ok, resultado_o_error).

    Util para tolerar una BD que aun no responde (HF/Supabase despertando).
    """
    last = None
    for i in range(1, attempts + 1):
        try:
            return True, fn()
        except Exception as exc:  # noqa: BLE001 - resiliencia de arranque
            last = exc
            log(f"[startup] intento {i}/{attempts} de inicializar la BD fallo: {exc}")
            if i < attempts:
                sleeper(delay)
    return False, last


def init_and_seed(
    provider_name: str = "local",
    attempts: int = 8,
    delay: float = 3.0,
    sleeper: Callable[[float], None] = time.sleep,
    log: Callable[[str], None] = print,
    seed_demo: bool = True,
    refresh: bool = False,
) -> bool:
    """Inicializa la BD y carga catalogo (+ usuario demo opcional), con reintentos.

    Con ``refresh=True`` actualiza los alimentos ya existentes desde la semilla
    (util para propagar datos reales recien commiteados a una BD ya poblada);
    por defecto solo inserta los nuevos y respeta los precios ya cargados.

    Devuelve True si lo logro; False si la BD no respondio tras los reintentos
    (el server arranca igual para no caerse; ver /api/health).
    """
    def _do():
        init_db()
        db = SessionLocal()
        try:
            seed_foods(db, provider_name, refresh=refresh)
            if seed_demo:
                seed_demo_user(db)
        finally:
            db.close()
        return True

    ok, err = run_with_retries(_do, attempts, delay, sleeper, log)
    if not ok:
        log(f"[startup] la BD no respondio tras {attempts} intentos: {err}. "
            f"El server arranca igual; revisa SCAVENGER_DATABASE_URL.")
    return ok


def run_seed(provider_name: str = "local", refresh: bool = False) -> int:
    """Inicializa la BD y carga el catalogo. Util para scripts/CLI."""
    init_db()
    db = SessionLocal()
    try:
        return seed_foods(db, provider_name, refresh)
    finally:
        db.close()


if __name__ == "__main__":
    import sys

    name = sys.argv[1] if len(sys.argv) > 1 else "local"
    n = run_seed(name, refresh=True)
    print(f"Alimentos cargados/actualizados: {n} (proveedor: {name})")
