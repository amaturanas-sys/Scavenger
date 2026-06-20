"""Carga el catalogo de alimentos a la base de datos desde un proveedor."""
from __future__ import annotations

from sqlalchemy.orm import Session

from .database import SessionLocal, init_db
from .models import Food
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
            db.add(Food(id=r.id, **data))
            count += 1
        elif refresh:
            for k, v in data.items():
                setattr(existing, k, v)
            count += 1
    db.commit()
    return count


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
