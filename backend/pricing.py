"""Refresco de precios por cadena mediante scraping (VTEX).

Por cada alimento del catalogo busca el producto en la cadena indicada,
elige la mejor coincidencia y actualiza su `FoodPrice`. Mantiene la
nutricion autorada intacta (el scraping de precios no la toca). Cachea las
respuestas en disco para no repetir consultas.
"""
from __future__ import annotations

import json
import re
import time
from dataclasses import dataclass, field
from pathlib import Path

from sqlalchemy.orm import Session

from . import config
from .models import Food, FoodPrice
from .providers import PRICE_PROVIDERS
from .providers.vtex import best_match

CACHE_DIR = config.DATA_DIR / "cache"


@dataclass
class RefreshResult:
    retailer_id: str
    matched: int = 0
    missed: int = 0
    updated: int = 0
    misses: list[str] = field(default_factory=list)

    def summary(self) -> str:
        return (f"[{self.retailer_id}] {self.matched} con precio, {self.missed} sin "
                f"coincidencia, {self.updated} FoodPrice actualizados.")


def _slug(term: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", term.lower()).strip("_")[:60] or "x"


def _cache_path(retailer_id: str, term: str) -> Path:
    return CACHE_DIR / retailer_id / f"{_slug(term)}.json"


def _cached_search(provider, retailer_id: str, term: str, use_cache: bool) -> list[dict]:
    path = _cache_path(retailer_id, term)
    if use_cache and path.exists():
        try:
            return json.loads(path.read_text(encoding="utf-8"))
        except (json.JSONDecodeError, OSError):
            pass
    products = provider.search_products(term)
    if use_cache:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(json.dumps(products, ensure_ascii=False), encoding="utf-8")
    return products


def _search_term(food: Food) -> str:
    """Termino de busqueda para un alimento (marca + nombre, sin 'Granel')."""
    brand = "" if (food.brand or "").lower() in ("granel", "") else food.brand
    return f"{food.name} {brand}".strip()


def refresh_retailer(
    db: Session,
    retailer_id: str,
    limit: int | None = None,
    use_cache: bool = True,
    sleep_s: float = 0.3,
    enabled: bool = True,
    log=print,
) -> RefreshResult:
    """Actualiza los precios de una cadena para todos los alimentos."""
    cls = PRICE_PROVIDERS.get(retailer_id)
    if cls is None:
        raise ValueError(f"No hay proveedor de precios para '{retailer_id}'. "
                         f"Disponibles: {sorted(PRICE_PROVIDERS)}")
    provider = cls(enabled=enabled)
    result = RefreshResult(retailer_id=retailer_id)

    foods = db.query(Food).order_by(Food.name).all()
    if limit:
        foods = foods[:limit]

    for food in foods:
        term = _search_term(food)
        try:
            products = _cached_search(provider, retailer_id, term, use_cache)
        except PermissionError as exc:
            # Host bloqueado por el allowlist de egress: abortamos con guia clara.
            raise SystemExit(str(exc)) from exc

        match = best_match(food.name, food.brand, products)
        if not match or not match.get("package_g"):
            result.missed += 1
            result.misses.append(food.id)
            continue

        result.matched += 1
        _upsert_price(db, food, retailer_id, match)
        result.updated += 1
        if sleep_s and not (use_cache and _cache_path(retailer_id, term).exists()):
            time.sleep(sleep_s)

    _recompute_cheapest(foods)
    db.commit()
    log(result.summary())
    return result


def _upsert_price(db: Session, food: Food, retailer_id: str, match: dict) -> None:
    existing = next((p for p in food.prices if p.retailer_id == retailer_id), None)
    if existing is None:
        existing = FoodPrice(food_id=food.id, retailer_id=retailer_id)
        food.prices.append(existing)
    existing.retailer = match.get("retailer", retailer_id)
    existing.price_clp = float(match["price_clp"])
    existing.package_g = float(match["package_g"])


def _recompute_cheapest(foods: list[Food]) -> None:
    """Actualiza el precio/cadena denormalizado de cada alimento (el mas barato)."""
    for food in foods:
        if not food.prices:
            continue
        cheapest = min(food.prices, key=lambda p: p.price_per_g)
        food.price_clp = cheapest.price_clp
        food.package_g = cheapest.package_g
        food.retailer = cheapest.retailer
