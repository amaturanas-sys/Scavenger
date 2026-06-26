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
from sqlalchemy import text

from . import config
from .database import engine
from .routers import builder, feedback, foods, plans, users
from .seed import init_and_seed


@asynccontextmanager
async def lifespan(app: FastAPI):
    # Inicializa la BD y carga el catalogo (idempotente), con reintentos para
    # tolerar una BD que aun despierta (HF Spaces/Supabase). Si falla, el server
    # arranca igual (no se cae) y /api/health reporta la BD como "down".
    app.state.db_ready = init_and_seed(config.DEFAULT_FOOD_PROVIDER)
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
app.include_router(builder.router)


@app.get("/api/health", tags=["sistema"])
def health():
    # Chequeo liviano de conectividad a la BD para diagnostico.
    db_ok = True
    try:
        with engine.connect() as conn:
            conn.execute(text("SELECT 1"))
    except Exception:  # noqa: BLE001
        db_ok = False
    return {
        "status": "ok",
        "app": "scavenger",
        "version": app.version,
        "db": "ok" if db_ok else "down",
    }


# Frontend estatico (montado al final para no interferir con /api/*).
if config.FRONTEND_DIR.exists():
    @app.get("/", include_in_schema=False)
    def index():
        return FileResponse(config.FRONTEND_DIR / "index.html")

    app.mount("/", StaticFiles(directory=str(config.FRONTEND_DIR), html=True), name="frontend")
