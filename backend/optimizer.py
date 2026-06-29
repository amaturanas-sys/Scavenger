"""Motor de optimizacion: la "dieta de costo minimo".

Resuelve un problema de programacion lineal (PL) que elige cuantos gramos
de cada alimento incluir para cubrir los requerimientos nutricionales al
menor costo posible, incorporando las preferencias aprendidas del usuario
y un enfasis opcional en la saciedad.

Es una variante del clasico "Stigler diet problem" resuelta con PuLP/CBC.
"""
from __future__ import annotations

from dataclasses import dataclass, field

import pulp

from .nutrition import KCAL_PER_G, Requirements

# Nutrientes (atributo en gramos/mg por 100 g) que el optimizador maneja.
MACROS = ("protein_g", "carb_g", "fat_g", "fiber_g")
MICROS = ("calcium_mg", "iron_mg", "potassium_mg", "vitamin_c_mg")
LIMIT_NUTRIENTS = ("sodium_mg",)


@dataclass
class FoodInput:
    """Representacion liviana de un alimento para el optimizador.

    Desacopla el motor del ORM para poder probarlo con datos planos.
    """

    id: str
    name: str
    category: str
    price_per_g: float
    serving_g: float
    max_servings_day: float
    satiety_index: float
    # Nutrientes por 100 g
    kcal: float
    protein_g: float
    carb_g: float
    fat_g: float
    fiber_g: float
    sodium_mg: float = 0.0
    calcium_mg: float = 0.0
    iron_mg: float = 0.0
    potassium_mg: float = 0.0
    vitamin_c_mg: float = 0.0
    brand: str = ""
    retailer: str = ""  # cadena mas conveniente para comprarlo

    @classmethod
    def from_orm(cls, food, price_per_g: float | None = None, retailer: str | None = None) -> "FoodInput":
        return cls(
            id=food.id,
            name=food.name,
            category=food.category,
            price_per_g=food.price_per_g if price_per_g is None else price_per_g,
            retailer=food.retailer if retailer is None else retailer,
            serving_g=food.serving_g,
            max_servings_day=food.max_servings_day,
            satiety_index=food.satiety_index,
            kcal=food.kcal,
            protein_g=food.protein_g,
            carb_g=food.carb_g,
            fat_g=food.fat_g,
            fiber_g=food.fiber_g,
            sodium_mg=food.sodium_mg,
            calcium_mg=food.calcium_mg,
            iron_mg=food.iron_mg,
            potassium_mg=food.potassium_mg,
            vitamin_c_mg=food.vitamin_c_mg,
            brand=getattr(food, "brand", ""),
        )


@dataclass
class OptimizeOptions:
    """Parametros que ajustan la busqueda del optimizador."""

    kcal_tolerance: float = 0.05         # banda +/- sobre las calorias objetivo
    protein_floor_ratio: float = 0.9     # proteina minima como fraccion del objetivo
    carb_band: tuple = (0.5, 1.3)        # banda de carbohidratos (min, max)
    fat_band: tuple = (0.6, 1.4)         # banda de grasa (min, max)
    fiber_floor_ratio: float = 0.7       # fibra minima como fraccion del objetivo
    micro_floor_ratio: float = 0.6       # micronutrientes minimos como fraccion
    preference_weight: float = 0.35      # cuanto pesan las preferencias en el costo
    satiety_emphasis: float = 0.0        # >0 favorece alimentos mas saciantes
    max_budget_clp: float | None = None  # tope duro de costo (opcional)
    # Objetivo a optimizar: "cost" minimiza el gasto; "satiety" maximiza la
    # saciedad sin pasar del presupuesto (aprovechar el presupuesto).
    objective: str = "cost"


@dataclass
class PlanItem:
    food_id: str
    name: str
    brand: str
    category: str
    retailer: str
    grams: float
    servings: float
    cost_clp: float
    kcal: float
    protein_g: float
    carb_g: float
    fat_g: float
    fiber_g: float
    satiety_contrib: float


@dataclass
class OptimizeResult:
    feasible: bool
    status: str
    items: list[PlanItem] = field(default_factory=list)
    totals: dict = field(default_factory=dict)
    warnings: list[str] = field(default_factory=list)

    def to_dict(self) -> dict:
        return {
            "feasible": self.feasible,
            "status": self.status,
            "items": [item.__dict__ for item in self.items],
            "totals": self.totals,
            "warnings": self.warnings,
        }


def _effective_cost_per_g(food: FoodInput, preference: float, opts: OptimizeOptions) -> float:
    """Costo por gramo ajustado por la preferencia del usuario.

    Una preferencia positiva (gusta) abarata el costo efectivo y hace que
    el alimento sea elegido con mayor probabilidad; una negativa lo encarece.
    """
    pref = max(-1.0, min(1.0, preference))
    factor = 1.0 - opts.preference_weight * pref
    return max(food.price_per_g * factor, 1e-6)


def _build_and_solve(
    foods: list[FoodInput],
    req: Requirements,
    preferences: dict[str, float],
    opts: OptimizeOptions,
) -> tuple[str, dict[str, float]]:
    """Construye y resuelve el PL. Devuelve (status, gramos por alimento)."""
    prob = pulp.LpProblem("scavenger_diet", pulp.LpMinimize)

    # Variable: gramos de cada alimento (continua, acotada por porciones/dia).
    grams = {}
    for f in foods:
        sp = f.serving_g or 100.0  # evita upBound=0 si serving_g viene en 0
        upper = max(f.max_servings_day * sp, sp)
        grams[f.id] = pulp.LpVariable(f"g_{f.id}", lowBound=0, upBound=upper)

    # Objetivo: minimizar costo efectivo - enfasis en saciedad.
    if opts.objective == "satiety":
        # Maximiza la saciedad total (sin pasar del presupuesto, ver cap abajo);
        # un termino minimo de costo desempata hacia lo mas economico.
        terms = []
        for f in foods:
            sat_density = f.satiety_index / 100.0
            c = _effective_cost_per_g(f, preferences.get(f.id, 0.0), opts)
            terms.append((-sat_density + 1e-4 * c) * grams[f.id])
        prob += pulp.lpSum(terms)
    else:
        # Minimiza el costo efectivo (ajustado por preferencias), con un sesgo
        # opcional hacia alimentos mas saciantes.
        cost_terms = []
        for f in foods:
            c = _effective_cost_per_g(f, preferences.get(f.id, 0.0), opts)
            sat_density = f.satiety_index / 100.0
            cost_terms.append((c - opts.satiety_emphasis * sat_density) * grams[f.id])
        prob += pulp.lpSum(cost_terms)

    def nutrient_sum(attr: str):
        # Los valores estan por 100 g; convertimos por gramo.
        return pulp.lpSum(getattr(f, attr) / 100.0 * grams[f.id] for f in foods)

    # Calorias dentro de una banda.
    prob += nutrient_sum("kcal") >= req.kcal * (1 - opts.kcal_tolerance), "kcal_min"
    prob += nutrient_sum("kcal") <= req.kcal * (1 + opts.kcal_tolerance), "kcal_max"

    # Macronutrientes.
    prob += nutrient_sum("protein_g") >= req.protein_g * opts.protein_floor_ratio, "protein_min"
    prob += nutrient_sum("carb_g") >= req.carb_g * opts.carb_band[0], "carb_min"
    prob += nutrient_sum("carb_g") <= req.carb_g * opts.carb_band[1], "carb_max"
    prob += nutrient_sum("fat_g") >= req.fat_g * opts.fat_band[0], "fat_min"
    prob += nutrient_sum("fat_g") <= req.fat_g * opts.fat_band[1], "fat_max"
    prob += nutrient_sum("fiber_g") >= req.fiber_g * opts.fiber_floor_ratio, "fiber_min"

    # Micronutrientes (minimos relajables).
    for micro in MICROS:
        target = req.micros.get(micro, 0.0)
        if target > 0:
            prob += nutrient_sum(micro) >= target * opts.micro_floor_ratio, f"{micro}_min"

    # Limites superiores (ej: sodio).
    for lim in LIMIT_NUTRIENTS:
        cap = req.limits.get(lim)
        if cap:
            prob += nutrient_sum(lim) <= cap, f"{lim}_max"

    # Tope de presupuesto (costo real, sin ajuste por preferencia).
    if opts.max_budget_clp is not None:
        prob += pulp.lpSum(f.price_per_g * grams[f.id] for f in foods) <= opts.max_budget_clp, "budget"

    prob.solve(pulp.PULP_CBC_CMD(msg=0))
    status = pulp.LpStatus[prob.status]
    solution = {f.id: (grams[f.id].value() or 0.0) for f in foods}
    return status, solution


def optimize(
    foods: list[FoodInput],
    req: Requirements,
    preferences: dict[str, float] | None = None,
    opts: OptimizeOptions | None = None,
) -> OptimizeResult:
    """Resuelve la dieta de costo minimo, relajando restricciones si es infactible."""
    preferences = preferences or {}
    opts = opts or OptimizeOptions()
    warnings: list[str] = []

    if not foods:
        return OptimizeResult(False, "Sin alimentos disponibles", warnings=["No hay alimentos que cumplan los filtros."])

    # Etapas de relajacion progresiva ante infactibilidad. El presupuesto es un
    # tope DURO ("sin pasarse") en los modos con tope, asi que se mantiene lo
    # mas posible: primero se relajan micronutrientes/fibra/banda calorica
    # conservando el tope, y solo como ULTIMO recurso se suelta.
    _very_relaxed = {"micro_floor_ratio": 0.0, "fiber_floor_ratio": 0.0,
                     "kcal_tolerance": max(opts.kcal_tolerance, 0.12),
                     "carb_band": (0.3, 1.6), "fat_band": (0.4, 1.7)}
    relax_stages = [
        opts,
        OptimizeOptions(**{**opts.__dict__, "micro_floor_ratio": opts.micro_floor_ratio * 0.5}),
        OptimizeOptions(**{**opts.__dict__, "micro_floor_ratio": 0.0, "fiber_floor_ratio": opts.fiber_floor_ratio * 0.5}),
        # Nutricion muy relajada pero AUN dentro del presupuesto (respeta el tope).
        OptimizeOptions(**{**opts.__dict__, **_very_relaxed}),
        # Ultimo recurso: si ni asi cabe, se suelta el tope y se MINIMIZA el costo
        # (objective="cost") para entregar la opcion mas economica posible, no una
        # que maximice saciedad sin techo. Se marca over_budget mas abajo.
        OptimizeOptions(**{**opts.__dict__, **_very_relaxed,
                           "max_budget_clp": None, "objective": "cost"}),
    ]

    status, solution, used = "Infeasible", {}, opts
    for i, stage in enumerate(relax_stages):
        status, solution = _build_and_solve(foods, req, preferences, stage)
        used = stage
        if status == "Optimal":
            if i > 0:
                warnings.append(
                    "No fue posible cumplir todas las metas estrictas; se relajaron "
                    "restricciones (micronutrientes/fibra/banda calorica) para entregar una opcion."
                )
            break

    if status != "Optimal":
        return OptimizeResult(False, status, warnings=["No se encontro una combinacion factible con los alimentos disponibles."])

    if used.max_budget_clp is None and opts.max_budget_clp is not None:
        warnings.append("El presupuesto indicado era insuficiente; se entrega la opcion mas economica posible.")

    return _build_result(foods, solution, preferences, warnings)


def _build_result(
    foods: list[FoodInput],
    solution: dict[str, float],
    preferences: dict[str, float],
    warnings: list[str],
) -> OptimizeResult:
    by_id = {f.id: f for f in foods}
    items: list[PlanItem] = []
    totals = {k: 0.0 for k in ("cost_clp", "kcal", "protein_g", "carb_g", "fat_g",
                               "fiber_g", "sodium_mg", "calcium_mg", "iron_mg",
                               "potassium_mg", "vitamin_c_mg", "satiety")}

    for fid, g in solution.items():
        if g < 0.5:  # ignora cantidades despreciables (< 0.5 g)
            continue
        f = by_id[fid]
        scale = g / 100.0
        cost = f.price_per_g * g
        sat = f.satiety_index / 100.0 * g
        items.append(PlanItem(
            food_id=fid, name=f.name, brand=f.brand, category=f.category, retailer=f.retailer,
            grams=round(g, 1), servings=round(g / f.serving_g, 2) if f.serving_g else 0.0,
            cost_clp=round(cost, 1), kcal=round(f.kcal * scale, 1),
            protein_g=round(f.protein_g * scale, 1), carb_g=round(f.carb_g * scale, 1),
            fat_g=round(f.fat_g * scale, 1), fiber_g=round(f.fiber_g * scale, 1),
            satiety_contrib=round(sat, 1),
        ))
        totals["cost_clp"] += cost
        totals["kcal"] += f.kcal * scale
        totals["protein_g"] += f.protein_g * scale
        totals["carb_g"] += f.carb_g * scale
        totals["fat_g"] += f.fat_g * scale
        totals["fiber_g"] += f.fiber_g * scale
        totals["sodium_mg"] += f.sodium_mg * scale
        totals["calcium_mg"] += f.calcium_mg * scale
        totals["iron_mg"] += f.iron_mg * scale
        totals["potassium_mg"] += f.potassium_mg * scale
        totals["vitamin_c_mg"] += f.vitamin_c_mg * scale
        totals["satiety"] += sat

    totals = {k: round(v, 1) for k, v in totals.items()}
    # Ordena de mayor a menor aporte calorico para presentar.
    items.sort(key=lambda it: it.kcal, reverse=True)
    return OptimizeResult(True, "Optimal", items=items, totals=totals, warnings=warnings)
