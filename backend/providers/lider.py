"""Proveedor Lider (Walmart Chile).

Lider NO usa VTEX (a diferencia de Jumbo/Santa Isabel): corre sobre la
plataforma propia de Walmart, que expone un BFF (backend-for-frontend) de
busqueda que responde JSON. Este adaptador implementa la misma interfaz que
los proveedores VTEX (`search_products(term) -> list[dict]`), de modo que el
refresco de precios (`backend/pricing.py`) funciona sin cambios.

Diseno tolerante al esquema
---------------------------
Como la respuesta exacta del BFF puede variar entre versiones del sitio, el
mapeo (`_map_product`) busca cada campo en varias ubicaciones conocidas
(nombre, marca, precio, id, ean) y el contenedor de productos
(`_extract_products`) prueba varias rutas. Asi, el adaptador resiste cambios
menores de esquema; si Walmart cambia algo mayor, basta ajustar las listas
de claves candidatas o el endpoint (ambos configurables por entorno).

Configuracion por entorno
-------------------------
  SCAVENGER_LIDER_BASE_URL     (def: https://apps.lider.cl)
  SCAVENGER_LIDER_SEARCH_PATH  (def: /supermercado/bff/products?term={q}&page=1)
  SCAVENGER_LIDER_ENABLED      (def: 1)

Requiere el host de Lider en el allowlist de egress del entorno para
consultarse en vivo (igual que Jumbo/Santa Isabel).
"""
from __future__ import annotations

import os
from urllib.parse import quote

from .base import FoodProvider, FoodRecord
from .vtex import BROWSER_UA, browser_headers, parse_package_grams

# Claves candidatas para cada campo (se usa la primera presente).
_NAME_KEYS = ("displayName", "name", "productName", "title", "description")
_BRAND_KEYS = ("brand", "brandName", "marca")
_ID_KEYS = ("sku", "productId", "id", "Id", "itemId")
_EAN_KEYS = ("gtin13", "gtin", "ean", "barcode")
_SIZE_KEYS = ("size", "netContent", "packageSize", "displayName", "name")
_AVAIL_KEYS = ("available", "isAvailable", "inStock", "hasStock")
# Rutas candidatas para el precio (cada una: lista de claves anidadas).
_PRICE_PATHS = (
    ("price",),
    ("sellPrice",),
    ("salePrice",),
    ("priceInfo", "price"),
    ("prices", "BasePriceSales"),
    ("prices", "price"),
    ("price", "BasePriceSales"),
    ("price", "salePrice"),
)
# Rutas candidatas para la lista de productos en la respuesta.
_PRODUCTS_PATHS = (
    ("products",),
    ("data", "products"),
    ("data", "search", "products"),
    ("results",),
    ("items",),
)


def _first(d: dict, keys, default=""):
    for k in keys:
        v = d.get(k)
        if v not in (None, ""):
            return v
    return default


def _to_clp(value) -> float:
    """Normaliza un precio a numero CLP, tolerando strings tipo '$1.490'."""
    if isinstance(value, (int, float)):
        return float(value)
    if isinstance(value, str):
        digits = "".join(c for c in value if c.isdigit())
        return float(digits) if digits else 0.0
    return 0.0


def _deep_price(product: dict) -> float:
    for path in _PRICE_PATHS:
        node = product
        ok = True
        for k in path:
            if isinstance(node, dict) and k in node:
                node = node[k]
            else:
                ok = False
                break
        if ok and not isinstance(node, dict):
            price = _to_clp(node)
            if price > 0:
                return price
    return 0.0


def _extract_products(data) -> list[dict]:
    if isinstance(data, list):
        return data
    if isinstance(data, dict):
        for path in _PRODUCTS_PATHS:
            node = data
            ok = True
            for k in path:
                if isinstance(node, dict) and k in node:
                    node = node[k]
                else:
                    ok = False
                    break
            if ok and isinstance(node, list):
                return node
    return []


def _map_product(raw: dict) -> dict | None:
    """Normaliza un producto del BFF de Lider al formato interno."""
    name = str(_first(raw, _NAME_KEYS))
    price = _deep_price(raw)
    if not name or price <= 0:
        return None

    # Disponibilidad: tolera flags como False, 0, "false"/"no" (JSON variado).
    available = _first(raw, _AVAIL_KEYS, default=True)
    if str(available).strip().lower() in ("false", "0", "no", "none", "off"):
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
        "link": str(raw.get("url", raw.get("link", ""))),
        "retailer": LiderProvider.retailer_name,
        "retailer_id": LiderProvider.name,
    }


class LiderProvider(FoodProvider):
    name = "lider"
    retailer_name = "Lider"

    def __init__(self, enabled: bool | None = None, user_agent: str = BROWSER_UA):
        if enabled is None:
            enabled = os.getenv("SCAVENGER_LIDER_ENABLED", "1") == "1"
        self.enabled = enabled
        self.user_agent = user_agent
        self.base_url = os.getenv("SCAVENGER_LIDER_BASE_URL", "https://apps.lider.cl")
        self.search_path = os.getenv(
            "SCAVENGER_LIDER_SEARCH_PATH",
            "/supermercado/bff/products?term={q}&page=1",
        )

    def _http_get_json(self, url: str):  # pragma: no cover - requiere red
        import httpx

        headers = browser_headers(referer=self.base_url)
        headers["User-Agent"] = self.user_agent
        with httpx.Client(timeout=20, headers=headers, follow_redirects=True) as client:
            resp = client.get(url)
            if resp.status_code == 403 and "allowlist" in resp.text.lower():
                raise PermissionError(
                    f"Host bloqueado por la politica de red del entorno: {self.base_url}. "
                    "Agregalo al allowlist de egress para permitir el scraping."
                )
            resp.raise_for_status()
            return resp.json()

    def search_raw(self, term: str):
        if not self.enabled:
            return []
        url = self.base_url + self.search_path.format(q=quote(term))
        return self._http_get_json(url)

    def search_products(self, term: str) -> list[dict]:
        out = []
        for raw in _extract_products(self.search_raw(term)):
            mapped = _map_product(raw)
            if mapped:
                out.append(mapped)
        return out

    def fetch_foods(self) -> list[FoodRecord]:
        return []

    def search(self, query: str) -> list[FoodRecord]:
        records = []
        for p in self.search_products(query):
            records.append(FoodRecord(
                id=f"{self.name}_{p['product_id']}",
                name=p["name"], category="otro", brand=p.get("brand", ""),
                retailer=self.retailer_name, price_clp=p["price_clp"],
                package_g=p.get("package_g") or 1000.0,
            ))
        return records
