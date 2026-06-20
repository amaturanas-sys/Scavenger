"""Proveedor FatSecret (valores nutricionales, region Chile).

FatSecret expone la Platform API (REST) con autenticacion OAuth 2.0
(client credentials). Se usa para **completar la nutricion** de alimentos
nuevos (por ejemplo, productos traidos por scraping de precios que llegan
sin valores nutricionales): se busca el alimento, se obtiene su detalle y se
**normaliza a valores por 100 g**.

La logica pura (normalizacion a 100 g, seleccion de porcion) esta separada de
la red para ser verificable offline. Los unicos puntos que tocan la red son
`_request_token` y `_api_get`, faciles de mockear en pruebas.

Configuracion por entorno:
  SCAVENGER_FATSECRET_KEY / SCAVENGER_FATSECRET_SECRET   (credenciales OAuth)
  SCAVENGER_FATSECRET_REGION   (def: CL)
  SCAVENGER_FATSECRET_LANGUAGE (def: es)

Requiere los hosts de FatSecret en el allowlist de egress del entorno
(oauth.fatsecret.com, platform.fatsecret.com).
"""
from __future__ import annotations

import os
import time

from .base import FoodProvider, FoodRecord
from .vtex import best_match

TOKEN_URL = "https://oauth.fatsecret.com/connect/token"
API_URL = "https://platform.fatsecret.com/rest/server.api"

# Mapeo campo FatSecret -> campo interno (por 100 g).
_NUTRIENT_MAP = {
    "calories": "kcal",
    "protein": "protein_g",
    "carbohydrate": "carb_g",
    "fat": "fat_g",
    "fiber": "fiber_g",
    "sodium": "sodium_mg",
    "calcium": "calcium_mg",
    "iron": "iron_mg",
    "potassium": "potassium_mg",
    "vitamin_c": "vitamin_c_mg",
}


def _f(value, default=0.0) -> float:
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def _as_list(node) -> list:
    """FatSecret entrega un dict cuando hay un solo elemento y lista si hay varios."""
    if node is None:
        return []
    return node if isinstance(node, list) else [node]


def normalize_servings_to_100g(servings) -> dict | None:
    """Convierte las porciones de FatSecret a nutrientes por 100 g.

    Elige una porcion con unidad metrica en gramos/ml (preferentemente de
    100 g) y escala los nutrientes con factor 100 / cantidad. Devuelve None si
    ninguna porcion tiene cantidad metrica utilizable.
    """
    best = None
    best_amount = None
    for s in _as_list(servings):
        unit = str(s.get("metric_serving_unit", "")).lower()
        amount = _f(s.get("metric_serving_amount"), 0.0)
        if unit in ("g", "ml") and amount > 0:
            # Preferimos la porcion mas cercana a 100 g (factor cercano a 1).
            if best is None or abs(amount - 100) < abs(best_amount - 100):
                best, best_amount = s, amount
    if best is None:
        return None

    factor = 100.0 / best_amount
    per100 = {}
    for fs_key, internal in _NUTRIENT_MAP.items():
        per100[internal] = round(_f(best.get(fs_key)) * factor, 2)
    return per100


class FatSecretProvider(FoodProvider):
    name = "fatsecret"

    def __init__(self, key: str | None = None, secret: str | None = None):
        self.key = key or os.getenv("SCAVENGER_FATSECRET_KEY", "")
        self.secret = secret or os.getenv("SCAVENGER_FATSECRET_SECRET", "")
        self.region = os.getenv("SCAVENGER_FATSECRET_REGION", "CL")
        self.language = os.getenv("SCAVENGER_FATSECRET_LANGUAGE", "es")
        self._token = ""
        self._token_exp = 0.0

    @property
    def configured(self) -> bool:
        return bool(self.key and self.secret)

    # ---- red (mockeable en pruebas) ----
    def _request_token(self) -> dict:  # pragma: no cover - requiere red
        import httpx

        with httpx.Client(timeout=20) as client:
            resp = client.post(
                TOKEN_URL,
                data={"grant_type": "client_credentials", "scope": "basic"},
                auth=(self.key, self.secret),
            )
            self._raise_if_blocked(resp)
            resp.raise_for_status()
            return resp.json()

    def _api_get(self, params: dict):  # pragma: no cover - requiere red
        import httpx

        token = self._get_token()
        with httpx.Client(timeout=20, headers={"Authorization": f"Bearer {token}"}) as client:
            resp = client.get(API_URL, params=params)
            self._raise_if_blocked(resp)
            resp.raise_for_status()
            return resp.json()

    @staticmethod
    def _raise_if_blocked(resp):  # pragma: no cover - requiere red
        if resp.status_code == 403 and "allowlist" in resp.text.lower():
            raise PermissionError(
                "Host de FatSecret bloqueado por la politica de red del entorno. "
                "Agrega oauth.fatsecret.com y platform.fatsecret.com al allowlist de egress."
            )

    def _get_token(self) -> str:
        if self._token and time.time() < self._token_exp - 30:
            return self._token
        data = self._request_token()
        self._token = data.get("access_token", "")
        self._token_exp = time.time() + _f(data.get("expires_in"), 0.0)
        return self._token

    # ---- API ----
    def search_food(self, query: str) -> list[dict]:
        if not self.configured:
            return []
        data = self._api_get({
            "method": "foods.search", "search_expression": query,
            "region": self.region, "language": self.language,
            "max_results": 20, "format": "json",
        })
        return _as_list(data.get("foods", {}).get("food"))

    def get_food_nutrition(self, food_id: str) -> dict | None:
        """Devuelve {per_100g..., food_name, brand} para un food_id."""
        data = self._api_get({
            "method": "food.get.v2", "food_id": food_id,
            "region": self.region, "language": self.language, "format": "json",
        })
        food = data.get("food", {})
        per100 = normalize_servings_to_100g(food.get("servings", {}).get("serving"))
        if per100 is None:
            return None
        per100["food_name"] = food.get("food_name", "")
        per100["brand"] = food.get("brand_name", "")
        return per100

    def nutrition_for(self, query: str, brand: str = "") -> dict | None:
        """Busca un alimento y devuelve su nutricion por 100 g (o None)."""
        if not self.configured:
            return None
        foods = self.search_food(query)
        if not foods:
            return None
        candidates = [
            {"name": f.get("food_name", ""), "brand": f.get("brand_name", ""), "food_id": f.get("food_id")}
            for f in foods
        ]
        match = best_match(query, brand, candidates, threshold=0.4)
        if not match:
            return None
        return self.get_food_nutrition(match["food_id"])

    # ---- interfaz FoodProvider ----
    def fetch_foods(self) -> list[FoodRecord]:
        return []

    def search(self, query: str) -> list[FoodRecord]:
        per = self.nutrition_for(query)
        if not per:
            return []
        return [FoodRecord(
            id=f"fatsecret_{query}", name=per.get("food_name", query), category="otro",
            brand=per.get("brand", ""),
            kcal=per["kcal"], protein_g=per["protein_g"], carb_g=per["carb_g"],
            fat_g=per["fat_g"], fiber_g=per["fiber_g"], sodium_mg=per["sodium_mg"],
            calcium_mg=per["calcium_mg"], iron_mg=per["iron_mg"],
            potassium_mg=per["potassium_mg"], vitamin_c_mg=per["vitamin_c_mg"],
        )]
