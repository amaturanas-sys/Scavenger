"""Proveedor Santa Isabel (Cencosud) sobre plataforma VTEX.

Comparte la misma API de catalogo que Jumbo. Requiere `www.santaisabel.cl`
en el allowlist de egress del entorno.
"""
from __future__ import annotations

import os

from .vtex import VTEXProvider


class SantaIsabelProvider(VTEXProvider):
    name = "santa_isabel"
    retailer_name = "Santa Isabel"
    base_url = "https://www.santaisabel.cl"

    def __init__(self, enabled: bool | None = None, **kwargs):
        if enabled is None:
            enabled = os.getenv("SCAVENGER_SANTA_ISABEL_ENABLED", "1") == "1"
        super().__init__(enabled=enabled, **kwargs)
