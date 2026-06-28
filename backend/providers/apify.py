"""Proveedor de precios via Apify (actors que scrapean el retail chileno).

Apify aloja "actors" que ya resuelven el scraping de Jumbo / Unimarc / Lider y
los mantienen actualizados. Llamamos su API REST (run-sync-get-dataset-items)
con un token y recibimos los productos en JSON, que normalizamos al formato
interno. Asi tercerizamos la parte fragil (endpoints/anti-bot) y la app sigue
igual.

Es un proveedor *metered* (`metered = True`): cada consulta consume cuota del
plan gratuito, asi que el refresco lo combina con la memoria de precios
(`PriceCache`) y la cuota mensual (`usage`) para no exceder el credito.

Configuracion por entorno (sin tocar codigo):
  SCAVENGER_APIFY_TOKEN               token de Apify (obligatorio)
  SCAVENGER_APIFY_MAX_RESULTS         resultados por busqueda (def 5)
  SCAVENGER_APIFY_<CHAIN>_ACTOR       id del actor, ej: scraperschile/jumbo
  SCAVENGER_APIFY_<CHAIN>_INPUT       plantilla JSON del input, con {q} y {n}
donde <CHAIN> es JUMBO, UNIMARC o LIDER. El input por defecto es
{"search": "{q}", "maxItems": {n}} y se ajusta cuando conozcamos el actor.
"""
from __future__ import annotations

import json
import os

from .. import config
from .base import FoodProvider, FoodRecord
from .lider import _BRAND_KEYS, _EAN_KEYS, _ID_KEYS, _NAME_KEYS, _deep_price, _first
from .vtex import parse_package_grams

API_BASE = "https://api.apify.com/v2"
_SIZE_KEYS = ("size", "netContent", "packageSize", "format", "displayName", "name", "title")
_AVAIL_KEYS = ("availability_value", "availability", "inStock", "available", "isAvailable")
# Marcadores de "no disponible" (tolerante a varios formatos del actor).
_UNAVAILABLE = ("out", "agotado", "no disp", "sin stock", "false", "unavailable")


def _is_available(raw: dict) -> bool:
    for k in _AVAIL_KEYS:
        v = raw.get(k)
        if v in (None, ""):
            continue
        val = str(v).strip().lower()
        return not any(bad in val for bad in _UNAVAILABLE)
    return True  # sin dato de stock -> se asume disponible


class ApifyProvider(FoodProvider):
    """Cliente generico de un actor de Apify que devuelve productos."""

    metered = True
    name = "apify"
    retailer_name = "Apify"
    env_key = "APIFY"          # prefijo de las variables de entorno por cadena
    actor_id = ""              # ej: scraperschile/jumbo (se setea por entorno)
    # Input real de los actors scraperschile (Jumbo/Unimarc/Lider): buscan por
    # 'term' y limitan con 'pageSize'. Usamos {n} (=MAX_RESULTS) como pageSize
    # para pedir pocos resultados por busqueda y no gastar de mas. Override por
    # entorno con SCAVENGER_APIFY_<CHAIN>_INPUT si algun actor difiere.
    default_input = '{"term": "{q}", "maxPages": 1, "pageSize": {n}}'

    def __init__(self, enabled: bool = True, token: str | None = None,
                 max_results: int | None = None):
        self.enabled = enabled
        self.token = (token if token is not None
                      else os.getenv("SCAVENGER_APIFY_TOKEN", "")).strip()
        self.max_results = int(max_results) if max_results is not None else config.APIFY_MAX_RESULTS
        # Variables vacias (no definidas en Actions) -> usar el valor por defecto.
        self.actor_id = os.getenv(f"SCAVENGER_APIFY_{self.env_key}_ACTOR", "").strip() or self.actor_id
        self.input_tmpl = os.getenv(f"SCAVENGER_APIFY_{self.env_key}_INPUT", "").strip() or self.default_input

    @property
    def configured(self) -> bool:
        return bool(self.token and self.actor_id)

    # ---- construccion del input del actor ----
    def _build_input(self, term: str) -> dict:
        raw = self.input_tmpl.replace("{q}", term).replace("{n}", str(self.max_results))
        try:
            return json.loads(raw)
        except json.JSONDecodeError:
            return {"search": term, "maxItems": self.max_results}

    # ---- red (unico punto que toca la red; mockeable en pruebas) ----
    def _run_actor(self, payload: dict):  # pragma: no cover - requiere red
        import httpx

        # El id del actor usa '~' en la API (la web lo muestra con '/').
        actor = self.actor_id.replace("/", "~")
        url = (f"{API_BASE}/acts/{actor}/run-sync-get-dataset-items"
               f"?token={self.token}&limit={self.max_results}")
        with httpx.Client(timeout=180) as client:
            resp = client.post(url, json=payload)
            resp.raise_for_status()
            data = resp.json()
        return data if isinstance(data, list) else []

    # ---- API del proveedor de precios ----
    def search_products(self, term: str) -> list[dict]:
        if not self.enabled or not self.configured:
            return []
        items = self._run_actor(self._build_input(term)) or []
        out = []
        for raw in items[: self.max_results]:
            if isinstance(raw, dict):
                mapped = self._map(raw)
                if mapped:
                    out.append(mapped)
        return out

    def _map(self, raw: dict) -> dict | None:
        """Normaliza un item del dataset de Apify (tolerante al esquema)."""
        name = str(_first(raw, _NAME_KEYS))
        price = _deep_price(raw)
        if not name or price <= 0 or not _is_available(raw):
            return None
        grams = None
        for key in _SIZE_KEYS:
            grams = parse_package_grams(str(raw.get(key, "")))
            if grams:
                break
        return {
            "name": name,
            "brand": str(_first(raw, _BRAND_KEYS)),
            "price_clp": price,
            "package_g": grams,
            "ean": str(_first(raw, _EAN_KEYS)),
            "product_id": str(_first(raw, _ID_KEYS)),
            "retailer": self.retailer_name,
            "retailer_id": self.name,
        }

    def fetch_foods(self) -> list[FoodRecord]:
        return []


class JumboApifyProvider(ApifyProvider):
    name = "jumbo"
    retailer_name = "Jumbo"
    env_key = "JUMBO"
    actor_id = os.getenv("SCAVENGER_APIFY_JUMBO_ACTOR", "")


class UnimarcApifyProvider(ApifyProvider):
    name = "unimarc"
    retailer_name = "Unimarc"
    env_key = "UNIMARC"
    actor_id = os.getenv("SCAVENGER_APIFY_UNIMARC_ACTOR", "")


class LiderApifyProvider(ApifyProvider):
    name = "lider"
    retailer_name = "Lider"
    env_key = "LIDER"
    actor_id = os.getenv("SCAVENGER_APIFY_LIDER_ACTOR", "")
