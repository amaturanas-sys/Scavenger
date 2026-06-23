# SCAVENGER - imagen para correr la app (API + frontend) con un comando.
FROM python:3.11-slim

# Evita .pyc y fuerza salida sin buffer en logs.
ENV PYTHONDONTWRITEBYTECODE=1 \
    PYTHONUNBUFFERED=1

WORKDIR /app

# Instala dependencias primero (mejor cacheo de capas).
COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

# Copia el codigo de la aplicacion.
COPY backend ./backend
COPY data ./data
COPY frontend ./frontend

# Puerto configurable por entorno (HF Spaces / Render inyectan PORT; local: 8000).
ENV PORT=8000
EXPOSE 8000

# El catalogo y el usuario demo se cargan en el arranque (lifespan).
# Forma shell para expandir ${PORT}.
CMD ["sh", "-c", "uvicorn backend.main:app --host 0.0.0.0 --port ${PORT:-8000}"]
