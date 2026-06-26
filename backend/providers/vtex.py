"""Proveedor generico para tiendas sobre plataforma VTEX.

Varias cadenas chilenas usan VTEX y exponen la misma API publica de
busqueda de catalogo:

    https://{host}/api/catalog_system/pub/products/search/{termino}?_from=0&_to=49

Jumbo y Santa Isabel (Cencosud) comparten exactamente este formato, por lo
que ambos proveedores heredan de `VTEXProvider`.

Este modulo separa la logica pura (parseo de tamano de envase, extraccion
de oferta, matching de producto) de la llamada de red, de modo que todo es
verificable offline con fixtures. La unica funcion que toca la red es
`_http_get_json`, facil de mockear en pruebas.
"""
from __future__ import annotations

import re
import unicodedata

from .base import FoodProvider, FoodRecord

# Palabras vacias que no aportan al matching de nombres.
_STOPWORDS = {"de", "del", "la", "el", "los", "las", "y", "con", "sin", "en", "al", "un", "una"}

# User-Agent de navegador real: muchos retailers chilenos rechazan clientes que
# no parezcan un navegador (responden 403/404/410). Configurable por entorno.
import os as _os

BROWSER_UA = _os.getenv(
    "SCAVENGER_SCRAPER_UA",
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/124.0.0.0 Safari/537.36",
)


def browser_headers(referer: str = "") -> dict:
    """Cabeceras tipo navegador (es-CL) para no ser bloqueado como bot."""
    h = {
        "User-Agent": BROWSER_UA,
        "Accept": "application/json, text/plain, */*",
        "Accept-Language": "es-CL,es;q=0.9,en;q=0.8",
    }
    if referer:
        h["Referer"] = referer
        h["Origin"] = referer.rstrip("/")
    return h


def strip_accents(text: str) -> str:
    return "".join(c for c in unicodedata.normalize("NFD", text) if unicodedata.category(c) != "Mn")


def normalize(text: str) -> str:
    return strip_accents((text or "").lower()).strip()


def tokens(text: str) -> set[str]:
    raw = re.split(r"[^a-z0-9]+", normalize(text))
    return {t for t in raw if t and t not in _STOPWORDS and len(t) > 1}


# --- Parseo de tamano de envase a gramos ---------------------------------
# Para liquidos se asume densidad ~1 (1 ml ~ 1 g), suficiente para normalizar
# el precio por 100 g/ml en la comparacion.
_UNIT_TO_G = {
    "kg": 1000.0, "kilo": 1000.0, "kilos": 1000.0, "k": 1000.0,
    "g": 1.0, "gr": 1.0, "grs": 1.0, "gramos": 1.0,
    "l": 1000.0, "lt": 1000.0, "lts": 1000.0, "litro": 1000.0, "litros": 1000.0,
    "ml": 1.0, "cc": 1.0,
}
_SIZE_RE = re.compile(r"(\d+(?:[.,]\d+)?)\s*(kg|kilos|kilo|k|gramos|grs|gr|g|litros|litro|lts|lt|l|ml|cc)\b")
# Multipack tipo "6 x 200 g" o "pack 6 un 200g".
_MULTI_RE = re.compile(r"(\d+)\s*(?:x|un|u|unid|unidades)\s*(\d+(?:[.,]\d+)?)\s*(kg|kilos|kilo|k|gramos|grs|gr|g|litros|litro|lts|lt|l|ml|cc)\b")


def parse_package_grams(text: str) -> float | None:
    """Extrae el tamano del envase en gramos desde un texto de producto.

    Soporta unidades simples ("1 Kg", "500 g", "1,5 L", "900 cc") y
    multipacks ("6 x 200 g"). Devuelve None si no logra inferirlo.
    """
    if not text:
        return None
    t = normalize(text)

    m = _MULTI_RE.search(t)
    if m:
        count = float(m.group(1))
        size = float(m.group(2).replace(",", "."))
        unit = _UNIT_TO_G.get(m.group(3))
        if unit:
            grams = count * size * unit
            if 1 <= grams <= 50000:  # descarta valores absurdos
                return grams

    m = _SIZE_RE.search(t)
    if m:
        size = float(m.group(1).replace(",", "."))
        unit = _UNIT_TO_G.get(m.group(2))
        if unit:
            grams = size * unit
            # Descarta valores absurdos (ej: "grado 2" mal interpretado).
            if 1 <= grams <= 50000:
                return grams
    return None


# --- Extraccion de oferta desde un producto VTEX -------------------------
def extract_offer(product: dict) -> dict | None:
    """Normaliza un producto VTEX a {name, brand, price_clp, package_g, ...}.

    Devuelve None si el producto no esta disponible o sin precio.
    """
    items = product.get("items") or []
    if not items:
        return None
    item = items[0]
    sellers = item.get("sellers") or []
    if not sellers:
        return None
    offer = sellers[0].get("commertialOffer", {})
    price = float(offer.get("Price") or 0)
    available = offer.get("IsAvailable", True)
    if price <= 0 or not available:
        return None

    name = product.get("productName", "") or item.get("name", "")
    brand = product.get("brand", "")

    # Intenta inferir el tamano del envase desde varios campos.
    grams = parse_package_grams(item.get("name", "")) or parse_package_grams(name)
    if grams is None:
        mult = item.get("unitMultiplier")
        unit = normalize(item.get("measurementUnit", ""))
        if mult and unit in _UNIT_TO_G:
            grams = float(mult) * _UNIT_TO_G[unit]

    return {
        "name": name,
        "brand": brand,
        "price_clp": price,
        "package_g": grams,
        "ean": item.get("ean", ""),
        "product_id": product.get("productId", ""),
        "link": product.get("link", ""),
    }


# --- Matching de producto contra un alimento del catalogo ----------------
def score_match(food_name: str, food_brand: str, product: dict) -> float:
    """Puntaje 0..1 de cuan bien un producto VTEX corresponde a un alimento."""
    fn = tokens(food_name)
    pn = tokens(product.get("name", ""))
    if not fn or not pn:
        return 0.0
    overlap = len(fn & pn) / len(fn)  # cobertura de los tokens buscados
    score = overlap
    if food_brand and normalize(food_brand) in normalize(product.get("name", "") + " " + product.get("brand", "")):
        score += 0.25
    if product.get("package_g"):
        score += 0.1  # preferimos productos con tamano conocido
    return min(score, 1.0)


def best_match(food_name: str, food_brand: str, products: list[dict], threshold: float = 0.5) -> dict | None:
    """Elige el producto mejor puntuado por sobre el umbral (None si ninguno)."""
    best, best_score = None, 0.0
    for p in products:
        s = score_match(food_name, food_brand, p)
        if s > best_score:
            best, best_score = p, s
    return best if best_score >= threshold else None


# --- Proveedor VTEX -------------------------------------------------------
class VTEXProvider(FoodProvider):
    """Cliente generico de busqueda de catalogo VTEX."""

    name = "vtex"
    retailer_name = "VTEX"
    base_url = ""  # ej: https://www.jumbo.cl

    def __init__(self, enabled: bool = True, page_size: int = 24, user_agent: str = BROWSER_UA):
        self.enabled = enabled
        self.page_size = page_size
        self.user_agent = user_agent

    # ---- red (unico punto que toca la red; mockeable en pruebas) ----
    def _http_get_json(self, url: str):  # pragma: no cover - requiere red
        import httpx

        headers = browser_headers(referer=self.base_url)
        headers["User-Agent"] = self.user_agent
        with httpx.Client(timeout=20, headers=headers, follow_redirects=True) as client:
            resp = client.get(url)
        ct = resp.headers.get("content-type", "")
        snippet = " ".join(resp.text[:200].split())
        diag = f"status={resp.status_code} ct='{ct}' final='{resp.url}' body[:200]={snippet!r}"
        if resp.status_code == 403 and "allowlist" in resp.text.lower():
            raise PermissionError(
                f"Host bloqueado por la politica de red del entorno: {self.base_url}. "
                "Agregalo al allowlist de egress para permitir el scraping."
            )
        if resp.status_code >= 400:
            raise RuntimeError(f"HTTP {resp.status_code} | {diag}")
        try:
            return resp.json()
        except Exception as exc:  # respuesta no-JSON (HTML, challenge anti-bot, etc.)
            raise RuntimeError(f"respuesta no-JSON | {diag}") from exc

    # Endpoints candidatos VTEX (se prueban en orden; el primero con productos gana):
    #  1) busqueda legacy por path, 2) legacy por query ?ft=, 3) Intelligent Search.
    _SEARCH_TEMPLATES = (
        "/api/catalog_system/pub/products/search/{q}?_from=0&_to={to}",
        "/api/catalog_system/pub/products/search?ft={q}&_from=0&_to={to}",
        "/api/io/_v/api/intelligent-search/product_search/trade-policy/1"
        "?query={q}&locale=es-CL&hideUnavailableItems=false",
    )

    def _candidate_urls(self, term: str) -> list[str]:
        from urllib.parse import quote

        q = quote(term)
        to = self.page_size - 1
        return [self.base_url + t.format(q=q, to=to) for t in self._SEARCH_TEMPLATES]

    @staticmethod
    def _products_from(data) -> list[dict]:
        """Extrae la lista de productos tanto del formato legacy (lista) como
        del de Intelligent Search ({'products': [...]})."""
        if isinstance(data, list):
            return data
        if isinstance(data, dict):
            for key in ("products", "data"):
                v = data.get(key)
                if isinstance(v, list):
                    return v
        return []

    def search_raw(self, term: str) -> list[dict]:
        """Productos VTEX crudos. Prueba varios endpoints y registra diagnostico
        (status/content-type/url/cuerpo) para saber que devuelve cada tienda."""
        if not self.enabled or not self.base_url:
            return []
        last_exc = None
        for url in self._candidate_urls(term):
            try:
                data = self._http_get_json(url)
            except Exception as exc:  # noqa: BLE001 - diagnostico de scraping
                print(f"[{self.name}][diag] FALLO {url} | {exc}")
                last_exc = exc
                continue
            prods = self._products_from(data)
            print(f"[{self.name}][diag] OK    {url} -> productos={len(prods)}")
            if prods:
                return prods
        if last_exc is not None:
            raise last_exc
        return []

    def search_products(self, term: str) -> list[dict]:
        """Productos normalizados (con precio y tamano) para un termino."""
        out = []
        for prod in self.search_raw(term):
            offer = extract_offer(prod)
            if offer:
                offer["retailer"] = self.retailer_name
                offer["retailer_id"] = self.name
                out.append(offer)
        return out

    def fetch_foods(self) -> list[FoodRecord]:
        # VTEX es busqueda; no entrega "todo el catalogo".
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
