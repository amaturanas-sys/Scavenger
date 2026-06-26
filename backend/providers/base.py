"""Interfaz comun de los proveedores de datos de alimentos."""
from __future__ import annotations

import abc
from dataclasses import asdict, dataclass, field


@dataclass
class FoodRecord:
    """Formato interno normalizado de un alimento.

    Todos los proveedores deben entregar sus datos en este formato para que
    el resto del sistema sea agnostico a la fuente (local, Jumbo, FatSecret).
    Los valores nutricionales son por 100 g.
    """

    id: str
    name: str
    category: str
    brand: str = ""
    retailer: str = ""
    package_g: float = 1000.0
    price_clp: float = 0.0
    # Precios por cadena: [{retailer, retailer_id, price_clp, package_g}, ...]
    prices: list = field(default_factory=list)
    serving_g: float = 100.0
    max_servings_day: float = 3.0
    satiety_index: float = 100.0
    kcal: float = 0.0
    protein_g: float = 0.0
    carb_g: float = 0.0
    fat_g: float = 0.0
    fiber_g: float = 0.0
    sodium_mg: float = 0.0
    calcium_mg: float = 0.0
    iron_mg: float = 0.0
    potassium_mg: float = 0.0
    vitamin_c_mg: float = 0.0
    tags: list = field(default_factory=list)

    def to_dict(self) -> dict:
        return asdict(self)


class FoodProvider(abc.ABC):
    """Contrato que deben cumplir todas las fuentes de alimentos."""

    name: str = "base"
    # True si cada consulta consume cuota de pago (p.ej. Apify): el refresco
    # entonces respeta el presupuesto mensual antes de consultar.
    metered: bool = False

    @abc.abstractmethod
    def fetch_foods(self) -> list[FoodRecord]:
        """Devuelve el catalogo completo de alimentos normalizado."""

    def search(self, query: str) -> list[FoodRecord]:
        """Busqueda simple por nombre/categoria (puede sobreescribirse)."""
        q = query.lower().strip()
        return [
            r for r in self.fetch_foods()
            if q in r.name.lower() or q in r.category.lower() or q in r.brand.lower()
        ]
