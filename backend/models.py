"""Modelos ORM de SCAVENGER."""
from __future__ import annotations

from datetime import datetime

from sqlalchemy import JSON, DateTime, Float, ForeignKey, Integer, String, UniqueConstraint, func
from sqlalchemy.orm import Mapped, mapped_column, relationship

from .database import Base


class Food(Base):
    """Alimento del retail con sus valores nutricionales y precio."""

    __tablename__ = "foods"

    id: Mapped[str] = mapped_column(String, primary_key=True)
    name: Mapped[str] = mapped_column(String, index=True)
    brand: Mapped[str] = mapped_column(String, default="")
    category: Mapped[str] = mapped_column(String, index=True)
    retailer: Mapped[str] = mapped_column(String, default="")

    package_g: Mapped[float] = mapped_column(Float, default=1000.0)
    price_clp: Mapped[float] = mapped_column(Float, default=0.0)
    serving_g: Mapped[float] = mapped_column(Float, default=100.0)
    max_servings_day: Mapped[float] = mapped_column(Float, default=3.0)
    satiety_index: Mapped[float] = mapped_column(Float, default=100.0)

    # Nutricion por 100 g
    kcal: Mapped[float] = mapped_column(Float, default=0.0)
    protein_g: Mapped[float] = mapped_column(Float, default=0.0)
    carb_g: Mapped[float] = mapped_column(Float, default=0.0)
    fat_g: Mapped[float] = mapped_column(Float, default=0.0)
    fiber_g: Mapped[float] = mapped_column(Float, default=0.0)
    sodium_mg: Mapped[float] = mapped_column(Float, default=0.0)
    calcium_mg: Mapped[float] = mapped_column(Float, default=0.0)
    iron_mg: Mapped[float] = mapped_column(Float, default=0.0)
    potassium_mg: Mapped[float] = mapped_column(Float, default=0.0)
    vitamin_c_mg: Mapped[float] = mapped_column(Float, default=0.0)

    tags: Mapped[list] = mapped_column(JSON, default=list)

    prices: Mapped[list["FoodPrice"]] = relationship(
        back_populates="food", cascade="all, delete-orphan"
    )

    @property
    def price_per_g(self) -> float:
        if self.package_g <= 0:
            return 0.0
        return self.price_clp / self.package_g

    @property
    def price_per_100g(self) -> float:
        return self.price_per_g * 100.0


class FoodPrice(Base):
    """Precio de un alimento en una cadena de supermercado especifica."""

    __tablename__ = "food_prices"
    __table_args__ = (UniqueConstraint("food_id", "retailer_id", name="uq_food_retailer"),)

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    food_id: Mapped[str] = mapped_column(ForeignKey("foods.id"), index=True)
    retailer: Mapped[str] = mapped_column(String, default="")
    retailer_id: Mapped[str] = mapped_column(String, index=True, default="")
    price_clp: Mapped[float] = mapped_column(Float, default=0.0)
    package_g: Mapped[float] = mapped_column(Float, default=1000.0)

    food: Mapped["Food"] = relationship(back_populates="prices")

    @property
    def price_per_g(self) -> float:
        return self.price_clp / self.package_g if self.package_g else 0.0


class User(Base):
    """Perfil del usuario y sus parametros para el calculo de requerimientos."""

    __tablename__ = "users"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    name: Mapped[str] = mapped_column(String, default="")
    sex: Mapped[str] = mapped_column(String, default="M")  # M | F
    age: Mapped[int] = mapped_column(Integer, default=30)
    weight_kg: Mapped[float] = mapped_column(Float, default=70.0)
    height_cm: Mapped[float] = mapped_column(Float, default=170.0)
    # sedentario | ligero | moderado | activo | muy_activo
    activity_level: Mapped[str] = mapped_column(String, default="moderado")
    # mantener | bajar | subir
    goal: Mapped[str] = mapped_column(String, default="mantener")

    daily_budget_clp: Mapped[float] = mapped_column(Float, default=4000.0)
    # Restricciones dieteticas: lista de tags requeridos (ej: ["vegano"]).
    diet_tags: Mapped[list] = mapped_column(JSON, default=list)
    # Categorias o ids de alimentos excluidos.
    excluded_foods: Mapped[list] = mapped_column(JSON, default=list)
    # Cadenas que el usuario tiene cerca (retailer_id). Vacio = todas.
    preferred_retailers: Mapped[list] = mapped_column(JSON, default=list)

    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())

    plans: Mapped[list["Plan"]] = relationship(back_populates="user", cascade="all, delete-orphan")
    preferences: Mapped[list["Preference"]] = relationship(
        back_populates="user", cascade="all, delete-orphan"
    )


class Plan(Base):
    """Minuta guardada (diaria o semanal)."""

    __tablename__ = "plans"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    user_id: Mapped[int] = mapped_column(ForeignKey("users.id"), index=True)
    title: Mapped[str] = mapped_column(String, default="")
    scope: Mapped[str] = mapped_column(String, default="diario")  # diario | semanal
    # Estructura completa del plan generado (comidas, items, totales).
    payload: Mapped[dict] = mapped_column(JSON, default=dict)
    total_cost_clp: Mapped[float] = mapped_column(Float, default=0.0)
    total_kcal: Mapped[float] = mapped_column(Float, default=0.0)
    satiety_score: Mapped[float] = mapped_column(Float, default=0.0)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())

    user: Mapped[User] = relationship(back_populates="plans")
    feedback: Mapped[list["Feedback"]] = relationship(
        back_populates="plan", cascade="all, delete-orphan"
    )


class Feedback(Base):
    """Retroalimentacion del usuario sobre una minuta (saciedad, costo, gusto)."""

    __tablename__ = "feedback"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    plan_id: Mapped[int] = mapped_column(ForeignKey("plans.id"), index=True)
    # Saciedad global 1 (mucho hambre) a 5 (muy saciado).
    satiety_score: Mapped[int] = mapped_column(Integer, default=3)
    # Satisfaccion con el costo 1 a 5.
    cost_score: Mapped[int] = mapped_column(Integer, default=3)
    notes: Mapped[str] = mapped_column(String, default="")
    # Ajustes por alimento: {food_id: rating -1..1} segun gusto declarado.
    food_ratings: Mapped[dict] = mapped_column(JSON, default=dict)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())

    plan: Mapped[Plan] = relationship(back_populates="feedback")


class Preference(Base):
    """Preferencia aprendida de un usuario por un alimento (peso -1..1)."""

    __tablename__ = "preferences"
    __table_args__ = (UniqueConstraint("user_id", "food_id", name="uq_user_food"),)

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    user_id: Mapped[int] = mapped_column(ForeignKey("users.id"), index=True)
    food_id: Mapped[str] = mapped_column(ForeignKey("foods.id"), index=True)
    weight: Mapped[float] = mapped_column(Float, default=0.0)
    times_used: Mapped[int] = mapped_column(Integer, default=0)

    user: Mapped[User] = relationship(back_populates="preferences")
