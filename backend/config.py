"""Configuracion central de SCAVENGER.

Los valores se pueden sobreescribir por variables de entorno para
facilitar el despliegue en distintos ambientes.
"""
from __future__ import annotations

import os
from pathlib import Path

# Rutas base del proyecto
BASE_DIR = Path(__file__).resolve().parent.parent
DATA_DIR = BASE_DIR / "data"
FRONTEND_DIR = BASE_DIR / "frontend"

# Archivo semilla con alimentos del retail chileno
SEED_FOODS_PATH = Path(os.getenv("SCAVENGER_SEED_FOODS", DATA_DIR / "chilean_foods.json"))

# Base de datos (SQLite por defecto, archivo en la raiz del proyecto)
DATABASE_URL = os.getenv("SCAVENGER_DATABASE_URL", f"sqlite:///{BASE_DIR / 'scavenger.db'}")

# Proveedor de datos de alimentos por defecto: local | jumbo | fatsecret
DEFAULT_FOOD_PROVIDER = os.getenv("SCAVENGER_FOOD_PROVIDER", "local")

# Parametros del motor de aprendizaje
# Peso con que las preferencias aprendidas modifican el "costo efectivo".
PREFERENCE_WEIGHT = float(os.getenv("SCAVENGER_PREFERENCE_WEIGHT", "0.35"))
# Tasa de aprendizaje al actualizar preferencias con feedback de saciedad.
LEARNING_RATE = float(os.getenv("SCAVENGER_LEARNING_RATE", "0.25"))

# Tolerancia por defecto sobre las calorias objetivo (+/-).
DEFAULT_KCAL_TOLERANCE = float(os.getenv("SCAVENGER_KCAL_TOLERANCE", "0.05"))

# Origenes permitidos por CORS (coma-separados). "*" = cualquiera (necesario
# para que la APK, de origen file://, consuma la API). Para un despliegue
# cerrado, define los origenes exactos, ej: "https://mi-app.com".
CORS_ORIGINS = os.getenv("SCAVENGER_CORS_ORIGINS", "*")

# Crear un usuario "Demo" en el arranque. Desactivar en produccion con "0".
SEED_DEMO = os.getenv("SCAVENGER_SEED_DEMO", "1") == "1"

# Refrescar el catalogo desde la semilla en cada arranque (refresh=True). Por
# defecto "0": el seed solo inserta alimentos nuevos y respeta los precios ya
# cargados (p.ej. los refrescados por scraping en la BD de produccion). Ponlo
# en "1" cuando quieras que un redeploy propague los datos reales recien
# commiteados en data/chilean_foods.json a una BD ya poblada.
SEED_REFRESH = os.getenv("SCAVENGER_SEED_REFRESH", "0") == "1"

# --- Scraping con cuota (Apify, plan gratuito) ---------------------------
# Apify factura por cuenta (credito compartido entre todos sus actors), asi que
# todos los scrapers de Apify comparten un mismo presupuesto bajo esta clave.
APIFY_PROVIDER_KEY = os.getenv("SCAVENGER_APIFY_PROVIDER_KEY", "apify")
# Tope mensual de unidades (consultas/resultados) para no salir del plan
# gratuito. ~100 alimentos/mes por defecto. Ajustar segun el modelo de precio
# real del actor (por-resultado vs por-corrida vs compute units).
APIFY_MONTHLY_BUDGET = int(os.getenv("SCAVENGER_APIFY_MONTHLY_BUDGET", "100"))
# Maximo de resultados a pedir por busqueda (para no pagar de mas por consulta).
APIFY_MAX_RESULTS = int(os.getenv("SCAVENGER_APIFY_MAX_RESULTS", "5"))
# Token de Apify. Nunca se commitea: va como secret/variable de entorno.
APIFY_TOKEN = os.getenv("SCAVENGER_APIFY_TOKEN", "")

# Vigencia (dias) de un precio recolectado antes de volver a consultarlo. Como
# el retail ajusta precios ~mensualmente, 30 dias evita gastar tokens en
# productos ya vistos. Pon 0 para forzar siempre el refresco.
PRICE_TTL_DAYS = float(os.getenv("SCAVENGER_PRICE_TTL_DAYS", "30"))
