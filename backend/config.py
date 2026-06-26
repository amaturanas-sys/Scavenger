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
