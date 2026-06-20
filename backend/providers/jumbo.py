"""Proveedor Jumbo (Cencosud) sobre plataforma VTEX.

Hereda toda la logica de `VTEXProvider`; solo define el host. La API publica
de busqueda es:
    https://www.jumbo.cl/api/catalog_system/pub/products/search/<termino>

Requiere que el host `www.jumbo.cl` este en el allowlist de egress del
entorno para poder consultarse en vivo.
"""
from __future__ import annotations

import os

from .vtex import VTEXProvider


class JumboProvider(VTEXProvider):
    name = "jumbo"
    retailer_name = "Jumbo"
    base_url = "https://www.jumbo.cl"

    def __init__(self, enabled: bool | None = None, **kwargs):
        if enabled is None:
            enabled = os.getenv("SCAVENGER_JUMBO_ENABLED", "1") == "1"
        super().__init__(enabled=enabled, **kwargs)
