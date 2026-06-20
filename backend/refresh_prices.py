"""CLI para refrescar precios reales por cadena (scraping VTEX).

Uso:
    python3 -m backend.refresh_prices                 # jumbo y santa_isabel
    python3 -m backend.refresh_prices --retailer jumbo
    python3 -m backend.refresh_prices --limit 10 --no-cache

Requiere que los hosts de las cadenas esten en el allowlist de egress del
entorno (ej: www.jumbo.cl, www.santaisabel.cl). Si no lo estan, el comando
aborta con un mensaje claro.
"""
from __future__ import annotations

import argparse

from .database import SessionLocal, init_db
from .pricing import refresh_retailer
from .providers import PRICE_PROVIDERS
from .seed import seed_foods


def main() -> None:
    parser = argparse.ArgumentParser(description="Refresca precios por cadena (scraping VTEX).")
    parser.add_argument("--retailer", action="append", choices=sorted(PRICE_PROVIDERS),
                        help="Cadena a refrescar (repetible). Por defecto: todas.")
    parser.add_argument("--limit", type=int, default=None, help="Maximo de alimentos a procesar.")
    parser.add_argument("--no-cache", action="store_true", help="Ignora la cache en disco.")
    parser.add_argument("--sleep", type=float, default=0.3, help="Pausa entre consultas (s).")
    args = parser.parse_args()

    retailers = args.retailer or list(PRICE_PROVIDERS)

    init_db()
    db = SessionLocal()
    try:
        # Asegura que el catalogo base este cargado antes de actualizar precios.
        seed_foods(db, "local", refresh=False)
        for rid in retailers:
            refresh_retailer(db, rid, limit=args.limit, use_cache=not args.no_cache,
                             sleep_s=args.sleep, enabled=True)
    finally:
        db.close()


if __name__ == "__main__":
    main()
