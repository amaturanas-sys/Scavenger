"""Orquestador resiliente de datos reales: precios (scraping) + nutricion.

Pensado para correr en un job programado (GitHub Actions) contra la BD de
produccion (Supabase via SCAVENGER_DATABASE_URL). A diferencia de los CLI
individuales (``refresh_prices`` / ``enrich_nutrition``), tolera que una
cadena falle (red/esquema/403) sin abortar el resto: refresca cada cadena de
forma independiente y, si hay credenciales de FatSecret, completa la nutricion
faltante. Devuelve un resumen consolidado.

Uso:
    python3 -m backend.refresh_data                       # todas las cadenas + nutricion
    python3 -m backend.refresh_data --retailer jumbo --retailer lider
    python3 -m backend.refresh_data --limit 50 --no-cache --no-enrich

Requiere internet (los hosts de las cadenas/FatSecret accesibles). En el
entorno cloud administrado la red esta bloqueada; correr en un runner de
GitHub Actions (egress abierto) o localmente. Ver docs/DATOS_REALES.md.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass, field

from sqlalchemy.orm import Session

from . import config
from .database import SessionLocal, init_db
from .nutrition_enrich import enrich_foods
from .pricing import refresh_retailer
from .providers import PRICE_PROVIDERS
from .providers.fatsecret import FatSecretProvider
from .seed import seed_foods


@dataclass
class DataRefreshReport:
    """Resultado consolidado de un refresco de datos reales."""

    retailers: dict = field(default_factory=dict)  # retailer_id -> resumen | "ERROR: ..."
    enrich: str = ""
    ok_retailers: int = 0
    failed_retailers: int = 0

    @property
    def all_failed(self) -> bool:
        return self.failed_retailers > 0 and self.ok_retailers == 0

    def summary(self) -> str:
        lines = ["=== Refresco de datos reales ==="]
        for rid, msg in self.retailers.items():
            lines.append(f"  {rid}: {msg}")
        if self.enrich:
            lines.append(f"  nutricion: {self.enrich}")
        lines.append(f"Cadenas OK: {self.ok_retailers}, con error: {self.failed_retailers}")
        return "\n".join(lines)


def run_refresh(
    db: Session,
    retailers: list[str],
    limit: int | None = None,
    use_cache: bool = True,
    sleep_s: float = 0.3,
    enrich: bool = True,
    enrich_provider=None,
    refresh_one=refresh_retailer,
    enrich_fn=enrich_foods,
    log=print,
    budget: int | None = None,
    ttl_days: float | None = None,
) -> DataRefreshReport:
    """Refresca cada cadena de forma independiente y (opcional) la nutricion.

    Las dependencias de red (``refresh_one``, ``enrich_fn``,
    ``enrich_provider``) son inyectables para poder verificar la orquestacion
    offline. Una excepcion en una cadena (incluido ``SystemExit`` por host
    bloqueado) se registra y no detiene al resto.

    ``budget`` es la cuota mensual compartida para proveedores *metered* (Apify);
    si es None se toma de la config. Las cadenas comparten el mismo presupuesto,
    asi que se va consumiendo en orden hasta agotarse.

    ``ttl_days`` sobreescribe la frescura de la memoria de precios (`PriceCache`):
    con 0 se ignora la memoria y se vuelve a consultar/matchear todo (re-aplica
    el filtro de no-comestibles, sinonimos y autoaprendizaje de EAN). Si es None
    se usa el TTL de la config.
    """
    report = DataRefreshReport()
    budget = config.APIFY_MONTHLY_BUDGET if budget is None else budget
    for rid in retailers:
        try:
            res = refresh_one(db, rid, limit=limit, use_cache=use_cache,
                              sleep_s=sleep_s, enabled=True, log=log,
                              budget=budget, provider_key=config.APIFY_PROVIDER_KEY,
                              ttl_days=ttl_days)
            report.retailers[rid] = res.summary()
            report.ok_retailers += 1
        except (Exception, SystemExit) as exc:  # noqa: BLE001 - resiliencia del job
            report.retailers[rid] = f"ERROR: {exc}"
            report.failed_retailers += 1
            log(f"[refresh_data] cadena '{rid}' fallo: {exc}")

    if enrich:
        provider = enrich_provider or FatSecretProvider()
        if getattr(provider, "configured", False):
            try:
                res = enrich_fn(db, provider=provider, only_missing=True, limit=limit, log=log)
                report.enrich = res.summary()
            except (Exception, SystemExit) as exc:  # noqa: BLE001 - resiliencia del job
                report.enrich = f"ERROR: {exc}"
                log(f"[refresh_data] enriquecimiento FatSecret fallo: {exc}")
        else:
            report.enrich = "omitido (sin credenciales FatSecret)"

    log(report.summary())
    return report


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Refresca datos reales (precios por cadena + nutricion FatSecret) "
                    "de forma resiliente hacia la BD configurada.")
    parser.add_argument("--retailer", action="append", choices=sorted(PRICE_PROVIDERS),
                        help="Cadena a refrescar (repetible). Por defecto: todas.")
    parser.add_argument("--limit", type=int, default=None, help="Maximo de alimentos a procesar.")
    parser.add_argument("--no-cache", action="store_true", help="Ignora la cache en disco.")
    parser.add_argument("--force", action="store_true",
                        help="Ignora la memoria de precios (TTL=0) y re-consulta/re-matchea "
                             "todo: re-aplica filtro de no-comestibles, sinonimos y EAN. "
                             "Gasta cuota de Apify (acotada por el presupuesto mensual).")
    parser.add_argument("--sleep", type=float, default=0.3, help="Pausa entre consultas (s).")
    parser.add_argument("--no-enrich", action="store_true",
                        help="No completar nutricion via FatSecret.")
    args = parser.parse_args()

    retailers = args.retailer or list(PRICE_PROVIDERS)

    init_db()
    db = SessionLocal()
    try:
        # Asegura que el catalogo base exista antes de refrescar precios.
        seed_foods(db, "local", refresh=False)
        report = run_refresh(
            db, retailers, limit=args.limit, use_cache=not args.no_cache,
            sleep_s=args.sleep, enrich=not args.no_enrich,
            ttl_days=0 if args.force else None,
        )
    finally:
        db.close()

    # Falla el job solo si TODAS las cadenas fallaron (asi un fallo aislado de
    # una cadena no marca rojo todo el refresco programado).
    if report.all_failed:
        raise SystemExit("Todas las cadenas fallaron; revisa el acceso a red y las credenciales.")


if __name__ == "__main__":
    main()
