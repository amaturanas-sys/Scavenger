"""Modelos ORM de SCAVENGER."""
from __future__ import annotations

from datetime import datetime

from sqlalchemy import (
    JSON,
    Boolean,
    DateTime,
    Float,
    ForeignKey,
    Integer,
    String,
    UniqueConstraint,
    func,
)
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
    # Codigo de barras (EAN-13). Permite match exacto contra el producto del
    # retail. Vacio por defecto; se autocompleta desde un match fuerte por nombre.
    ean: Mapped[str] = mapped_column(String, default="", index=True)

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
    # Micronutrientes adicionales (paso 3 del armado guiado). Vacios por defecto;
    # se completan al enriquecer la nutricion (FatSecret).
    zinc_mg: Mapped[float] = mapped_column(Float, default=0.0)
    vitamin_e_mg: Mapped[float] = mapped_column(Float, default=0.0)

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
    # Presupuesto mensual (si > 0 manda sobre el diario), comidas/dia y proteina
    # minima por plato: especificaciones que condicionan la recomendacion.
    monthly_budget_clp: Mapped[float] = mapped_column(Float, default=0.0)
    meals_per_day: Mapped[int] = mapped_column(Integer, default=4)
    min_protein_per_meal_g: Mapped[float] = mapped_column(Float, default=0.0)
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
    routines: Mapped[list["Routine"]] = relationship(
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


class ScraperUsage(Base):
    """Consumo mensual de un proveedor de scraping con cuota (p.ej. Apify).

    Permite mantenerse en el plan gratuito: una fila por (proveedor, mes UTC)
    con las unidades consumidas. Se comparte entre la app desplegada y los jobs
    de GitHub Actions porque ambos apuntan a la misma BD.
    """

    __tablename__ = "scraper_usage"
    __table_args__ = (UniqueConstraint("provider", "month", name="uq_provider_month"),)

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    provider: Mapped[str] = mapped_column(String, index=True, default="")
    month: Mapped[str] = mapped_column(String, index=True, default="")  # "YYYY-MM" (UTC)
    used: Mapped[int] = mapped_column(Integer, default=0)
    updated_at: Mapped[datetime] = mapped_column(
        DateTime(timezone=True), server_default=func.now(), onupdate=func.now()
    )


class PriceCache(Base):
    """Recuerdo del ultimo scraping de precio por (cadena, alimento).

    Permite NO volver a gastar cuota/tokens (p.ej. de Apify) en productos ya
    vistos mientras el dato siga fresco: los precios del retail cambian
    ~mensualmente, asi que con un TTL (def. 30 dias) basta refrescar cada
    producto una vez al mes. Guarda tambien los 'misses' (se busco y no hubo
    coincidencia) para no reintentarlos dentro del TTL.
    """

    __tablename__ = "price_cache"
    __table_args__ = (UniqueConstraint("retailer_id", "food_id", name="uq_cache_retailer_food"),)

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    retailer_id: Mapped[str] = mapped_column(String, index=True, default="")
    food_id: Mapped[str] = mapped_column(String, index=True, default="")
    matched: Mapped[bool] = mapped_column(Boolean, default=False)
    price_clp: Mapped[float] = mapped_column(Float, default=0.0)
    package_g: Mapped[float] = mapped_column(Float, default=0.0)
    retailer: Mapped[str] = mapped_column(String, default="")
    product_name: Mapped[str] = mapped_column(String, default="")
    # Epoch UTC (segundos) del ultimo fetch: comparacion de frescura sin lios de
    # zona horaria entre SQLite (naive) y Postgres (aware).
    fetched_epoch: Mapped[float] = mapped_column(Float, default=0.0)


class Routine(Base):
    """Comida fija que se repite en el calendario segun un preset semanal.

    El usuario puede dejar una comida (p.ej. el mismo desayuno) como rutina que
    se aplica automaticamente a ciertos dias segun un preset:
      - "L-V"   -> lunes a viernes
      - "finde" -> sabado y domingo
      - "todos" -> todos los dias
    El calendario precarga la rutina en cada dia que calce con su preset, sin
    necesidad de guardar una minuta por dia.
    """

    __tablename__ = "routines"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    user_id: Mapped[int] = mapped_column(ForeignKey("users.id"), index=True)
    # desayuno | snack 1 | almuerzo | cena | ...
    meal: Mapped[str] = mapped_column(String, default="")
    # L-V | finde | todos
    preset: Mapped[str] = mapped_column(String, default="todos")
    title: Mapped[str] = mapped_column(String, default="")
    # Items elegidos (misma forma que los candidatos del constructor).
    items: Mapped[list] = mapped_column(JSON, default=list)
    # Subtotales (kcal, proteina, costo, ...) de la comida.
    subtotal: Mapped[dict] = mapped_column(JSON, default=dict)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), server_default=func.now())

    user: Mapped[User] = relationship(back_populates="routines")


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
