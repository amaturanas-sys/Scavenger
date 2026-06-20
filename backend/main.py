"""Aplicacion FastAPI de SCAVENGER.

Monta los routers de la API, sirve el frontend estatico y carga el catalogo
de alimentos en el arranque (seed idempotente).
"""
from __future__ import annotations

from contextlib import asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles

from . import config
from .database import SessionLocal, init_db
from .routers import feedback, foods, plans, users
from .seed import seed_demo_user, seed_foods


@asynccontextmanager
async def lifespan(app: FastAPI):
    # Inicializa la BD y carga el catalogo (idempotente).
    init_db()
    db = SessionLocal()
    try:
        seed_foods(db, config.DEFAULT_FOOD_PROVIDER, refresh=False)
        seed_demo_user(db)
    finally:
        db.close()
    yield


app = FastAPI(
    title="SCAVENGER",
    description="Meal prep costo-efectivo para Chile: minutas optimizadas por costo, "
                "nutricion y saciedad.",
    version="0.1.0",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(users.router)
app.include_router(foods.router)
app.include_router(plans.router)
app.include_router(feedback.router)


@app.get("/api/health", tags=["sistema"])
def health():
    return {"status": "ok", "app": "scavenger", "version": app.version}


# Frontend estatico (montado al final para no interferir con /api/*).
if config.FRONTEND_DIR.exists():
    @app.get("/", include_in_schema=False)
    def index():
        return FileResponse(config.FRONTEND_DIR / "index.html")

    app.mount("/", StaticFiles(directory=str(config.FRONTEND_DIR), html=True), name="frontend")
