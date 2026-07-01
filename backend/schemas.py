"""Esquemas Pydantic para la API de SCAVENGER."""
from __future__ import annotations

from datetime import datetime
from typing import Any

from pydantic import BaseModel, Field


# ----- Usuarios -----
class UserBase(BaseModel):
    name: str = ""
    sex: str = Field("M", description="M o F")
    age: int = Field(30, ge=10, le=110)
    weight_kg: float = Field(70, gt=20, le=400)
    height_cm: float = Field(170, gt=100, le=250)
    activity_level: str = "moderado"
    goal: str = "mantener"
    daily_budget_clp: float = Field(4000, ge=0)
    monthly_budget_clp: float = Field(0, ge=0, description="Si > 0, manda sobre el diario (mensual/30)")
    meals_per_day: int = Field(4, ge=1, le=8)
    min_protein_per_meal_g: float = Field(0, ge=0, description="Proteína mínima por plato")
    diet_tags: list[str] = []
    excluded_foods: list[str] = []
    preferred_retailers: list[str] = []


class UserCreate(UserBase):
    pass


class UserUpdate(BaseModel):
    name: str | None = None
    sex: str | None = None
    age: int | None = None
    weight_kg: float | None = None
    height_cm: float | None = None
    activity_level: str | None = None
    goal: str | None = None
    daily_budget_clp: float | None = None
    monthly_budget_clp: float | None = None
    meals_per_day: int | None = None
    min_protein_per_meal_g: float | None = None
    diet_tags: list[str] | None = None
    excluded_foods: list[str] | None = None
    preferred_retailers: list[str] | None = None


class UserOut(UserBase):
    id: int

    model_config = {"from_attributes": True}


# ----- Alimentos -----
class FoodPriceOut(BaseModel):
    retailer: str
    retailer_id: str
    price_clp: float
    price_per_100g: float


class FoodOut(BaseModel):
    id: str
    name: str
    brand: str
    category: str
    ean: str = ""
    retailer: str          # cadena mas economica
    package_g: float
    price_clp: float       # precio mas economico (envase)
    price_per_100g: float  # precio/100g mas economico
    price_max_per_100g: float
    serving_g: float
    satiety_index: float
    kcal: float
    protein_g: float
    carb_g: float
    fat_g: float
    fiber_g: float
    tags: list[str]
    prices: list[FoodPriceOut] = []

    model_config = {"from_attributes": True}


class RetailerOut(BaseModel):
    retailer_id: str
    retailer: str


# ----- Historial de saciedad -----
class SatietyHistoryEntry(BaseModel):
    plan_id: int
    title: str
    scope: str
    created_at: str | None = None
    satiety_score: int
    cost_score: int
    total_cost_clp: float


class SatietyHistoryOut(BaseModel):
    entries: list[SatietyHistoryEntry]
    count: int
    avg_satiety: float
    avg_cost_score: float


# ----- Generacion de planes -----
class GeneratePlanRequest(BaseModel):
    user_id: int
    scope: str = Field("diario", description="diario o semanal")
    satiety_emphasis: float = Field(0.0, ge=0, le=2, description=">0 favorece saciedad")
    # Modo de presupuesto: none | min_cost | target (por dia).
    budget_mode: str = Field("none", description="none | min_cost | target")
    # Monto de presupuesto seleccionado; si es None usa el del perfil.
    budget_clp: float | None = Field(None, ge=0)
    # Compatibilidad: si se envia y no hay budget_mode, equivale a min_cost.
    use_budget: bool | None = None


class SavePlanRequest(BaseModel):
    user_id: int
    title: str = ""
    scope: str = "diario"
    payload: dict[str, Any]


class PlanOut(BaseModel):
    id: int
    user_id: int
    title: str
    scope: str
    payload: dict[str, Any]
    total_cost_clp: float
    total_kcal: float
    satiety_score: float
    # Fecha de creacion: el calendario la usa como respaldo cuando la minuta no
    # trae payload.date (p.ej. la guardada desde la ruleta).
    created_at: datetime | None = None

    model_config = {"from_attributes": True}


# ----- Rutinas (comidas fijas) -----
class RoutineBase(BaseModel):
    meal: str = ""
    preset: str = Field("todos", description="L-V | finde | todos")
    title: str = ""
    items: list[dict[str, Any]] = []
    subtotal: dict[str, Any] = {}


class RoutineCreate(RoutineBase):
    user_id: int


class RoutineOut(RoutineBase):
    id: int
    user_id: int

    model_config = {"from_attributes": True}


# ----- Feedback -----
class FeedbackRequest(BaseModel):
    satiety_score: int = Field(3, ge=1, le=5)
    cost_score: int = Field(3, ge=1, le=5)
    notes: str = ""
    food_ratings: dict[str, float] = {}


class FeedbackOut(BaseModel):
    id: int
    plan_id: int
    satiety_score: int
    cost_score: int
    notes: str
    updated_preferences: dict[str, float] = {}


# ----- Lista de compras -----
class ShoppingListRequest(BaseModel):
    payload: dict[str, Any]


class ShoppingItemOut(BaseModel):
    food_id: str
    name: str
    brand: str
    category: str
    needed_g: float
    consumed_cost_clp: float
    package_g: float | None = None
    packages: int | None = None
    package_price_clp: float | None = None
    packages_cost_clp: float | None = None


class ShoppingRetailerOut(BaseModel):
    retailer: str
    retailer_id: str | None = None
    item_count: int
    items: list[ShoppingItemOut]
    subtotal_consumed_clp: float
    subtotal_packages_clp: float


class ShoppingListOut(BaseModel):
    retailers: list[ShoppingRetailerOut]
    total_consumed_clp: float
    total_packages_clp: float
    retailer_count: int
