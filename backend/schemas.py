"""Esquemas Pydantic para la API de SCAVENGER."""
from __future__ import annotations

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


# ----- Generacion de planes -----
class GeneratePlanRequest(BaseModel):
    user_id: int
    scope: str = Field("diario", description="diario o semanal")
    satiety_emphasis: float = Field(0.0, ge=0, le=2, description=">0 favorece saciedad")
    use_budget: bool = False


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
