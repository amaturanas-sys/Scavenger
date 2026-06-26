"""Proveedores de datos de alimentos (pluggables).

Cada proveedor implementa `FoodProvider` y normaliza su fuente al formato
interno de SCAVENGER. El dataset local funciona offline; los proveedores
Jumbo (scraping) y FatSecret (API) quedan listos para conectarse.
"""
from __future__ import annotations

from .apify import (
    ApifyProvider,
    JumboApifyProvider,
    LiderApifyProvider,
    UnimarcApifyProvider,
)
from .base import FoodProvider, FoodRecord
from .fatsecret import FatSecretProvider
from .jumbo import JumboProvider
from .lider import LiderProvider
from .local import LocalDatasetProvider
from .santa_isabel import SantaIsabelProvider
from .vtex import VTEXProvider

PROVIDERS: dict[str, type[FoodProvider]] = {
    "local": LocalDatasetProvider,
    "jumbo": JumboProvider,
    "santa_isabel": SantaIsabelProvider,
    "lider": LiderProvider,
    "fatsecret": FatSecretProvider,
}

# Proveedores de scraping de precios por cadena (retailer_id -> clase).
# Via Apify (actors mantenidos): los scrapers VTEX/HTTP directos quedaron
# obsoletos cuando las cadenas cambiaron sus APIs. Las clases VTEX/Lider siguen
# disponibles para sus pruebas unitarias y como fallback configurable.
PRICE_PROVIDERS: dict[str, type[FoodProvider]] = {
    "jumbo": JumboApifyProvider,
    "unimarc": UnimarcApifyProvider,
    "lider": LiderApifyProvider,
}


def get_provider(name: str) -> FoodProvider:
    """Instancia un proveedor por nombre (cae a 'local' si no existe)."""
    cls = PROVIDERS.get(name, LocalDatasetProvider)
    return cls()


__all__ = [
    "FoodProvider",
    "FoodRecord",
    "LocalDatasetProvider",
    "JumboProvider",
    "SantaIsabelProvider",
    "LiderProvider",
    "VTEXProvider",
    "FatSecretProvider",
    "ApifyProvider",
    "JumboApifyProvider",
    "UnimarcApifyProvider",
    "LiderApifyProvider",
    "PROVIDERS",
    "PRICE_PROVIDERS",
    "get_provider",
]
