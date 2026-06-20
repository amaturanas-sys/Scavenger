"""Proveedor Jumbo (scraping de precios del retail chileno).

Esqueleto listo para conectar. Jumbo expone su catalogo a traves de una
API interna de busqueda (VTEX). Cuando se habilite el acceso a red, se
implementa `fetch_foods`/`search` consultando ese endpoint y mapeando la
respuesta a `FoodRecord`. Mientras tanto, no devuelve datos para no romper
el flujo (el sistema usa el proveedor local).

Notas de implementacion para conectar mas adelante:
  * Jumbo usa plataforma VTEX. Endpoint tipo:
      https://www.jumbo.cl/api/catalog_system/pub/products/search/<termino>
  * Hay que respetar robots.txt y los terminos de uso del sitio.
  * Conviene cachear resultados (data/cache/) y limitar la frecuencia.
  * Los precios vienen por producto/envase; el peso del envase suele estar
    en el nombre o en las especificaciones -> normalizar a price/100 g.
  * La nutricion NO viene en el catalogo de precios: se cruza por nombre
    con FatSecret (ver fatsecret.py) para completar `per_100g`.
"""
from __future__ import annotations

import os

from .base import FoodProvider, FoodRecord

JUMBO_SEARCH_URL = "https://www.jumbo.cl/api/catalog_system/pub/products/search/{query}"


class JumboProvider(FoodProvider):
    name = "jumbo"

    def __init__(self, enabled: bool | None = None):
        # Se activa explicitamente via env para evitar llamadas de red no deseadas.
        if enabled is None:
            enabled = os.getenv("SCAVENGER_JUMBO_ENABLED", "0") == "1"
        self.enabled = enabled

    def fetch_foods(self) -> list[FoodRecord]:
        # Sin un catalogo de terminos no hay "todo el catalogo"; se usa search().
        return []

    def search(self, query: str) -> list[FoodRecord]:
        if not self.enabled:
            # Acceso a red deshabilitado: el sistema cae al proveedor local.
            return []
        return self._search_remote(query)

    def _search_remote(self, query: str) -> list[FoodRecord]:  # pragma: no cover
        """Consulta el catalogo VTEX de Jumbo y normaliza la respuesta.

        Implementacion de referencia (requiere acceso a red habilitado).
        """
        import httpx

        url = JUMBO_SEARCH_URL.format(query=query)
        records: list[FoodRecord] = []
        with httpx.Client(timeout=15, headers={"User-Agent": "scavenger/0.1"}) as client:
            resp = client.get(url)
            resp.raise_for_status()
            for prod in resp.json():
                items = prod.get("items", [])
                if not items:
                    continue
                offer = items[0].get("sellers", [{}])[0].get("commertialOffer", {})
                price = float(offer.get("Price", 0) or 0)
                records.append(FoodRecord(
                    id=f"jumbo_{prod.get('productId')}",
                    name=prod.get("productName", ""),
                    category="otro",  # se clasifica en post-proceso
                    brand=prod.get("brand", ""),
                    retailer="Jumbo",
                    price_clp=price,
                    # package_g y nutricion se completan al cruzar con FatSecret.
                ))
        return records
