"""Proveedores de datos de alimentos (pluggables).

Cada proveedor implementa `FoodProvider` y normaliza su fuente al formato
interno de SCAVENGER. El dataset local funciona offline; los proveedores
Jumbo (scraping) y FatSecret (API) quedan listos para conectarse.
"""
from __future__ import annotations

from .base import FoodProvider, FoodRecord
from .fatsecret import FatSecretProvider
from .jumbo import JumboProvider
from .local import LocalDatasetProvider

PROVIDERS: dict[str, type[FoodProvider]] = {
    "local": LocalDatasetProvider,
    "jumbo": JumboProvider,
    "fatsecret": FatSecretProvider,
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
    "FatSecretProvider",
    "PROVIDERS",
    "get_provider",
]
