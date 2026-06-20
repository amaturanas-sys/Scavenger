"""Proveedor FatSecret (valores nutricionales, region Chile).

Esqueleto listo para conectar. FatSecret expone una API REST (Platform API)
con autenticacion OAuth 2.0 (client credentials). Se requieren credenciales
(SCAVENGER_FATSECRET_KEY / SCAVENGER_FATSECRET_SECRET). Con region=CL se
priorizan alimentos del mercado chileno.

Mientras no haya credenciales/red, no devuelve datos (el sistema usa el
proveedor local).
"""
from __future__ import annotations

import os

from .base import FoodProvider, FoodRecord

TOKEN_URL = "https://oauth.fatsecret.com/connect/token"
API_URL = "https://platform.fatsecret.com/rest/server.api"


class FatSecretProvider(FoodProvider):
    name = "fatsecret"

    def __init__(self, key: str | None = None, secret: str | None = None):
        self.key = key or os.getenv("SCAVENGER_FATSECRET_KEY", "")
        self.secret = secret or os.getenv("SCAVENGER_FATSECRET_SECRET", "")

    @property
    def configured(self) -> bool:
        return bool(self.key and self.secret)

    def fetch_foods(self) -> list[FoodRecord]:
        # FatSecret es una API de busqueda; no entrega "todo el catalogo".
        return []

    def search(self, query: str) -> list[FoodRecord]:
        if not self.configured:
            return []
        return self._search_remote(query)

    def _get_token(self) -> str:  # pragma: no cover
        import httpx

        with httpx.Client(timeout=15) as client:
            resp = client.post(
                TOKEN_URL,
                data={"grant_type": "client_credentials", "scope": "basic"},
                auth=(self.key, self.secret),
            )
            resp.raise_for_status()
            return resp.json()["access_token"]

    def _search_remote(self, query: str) -> list[FoodRecord]:  # pragma: no cover
        """Busca alimentos en FatSecret y mapea la nutricion por 100 g.

        Implementacion de referencia (requiere credenciales y red).
        """
        import httpx

        token = self._get_token()
        params = {
            "method": "foods.search",
            "search_expression": query,
            "region": "CL",
            "format": "json",
        }
        records: list[FoodRecord] = []
        with httpx.Client(timeout=15, headers={"Authorization": f"Bearer {token}"}) as client:
            resp = client.get(API_URL, params=params)
            resp.raise_for_status()
            foods = resp.json().get("foods", {}).get("food", [])
            for f in foods if isinstance(foods, list) else [foods]:
                # food_description trae un resumen; para nutricion exacta por
                # 100 g se llamaria a foods.get con el food_id. Aqui se deja
                # el id para enriquecer en una segunda pasada.
                records.append(FoodRecord(
                    id=f"fatsecret_{f.get('food_id')}",
                    name=f.get("food_name", ""),
                    category="otro",
                    brand=f.get("brand_name", ""),
                ))
        return records
