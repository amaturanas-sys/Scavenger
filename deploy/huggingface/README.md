---
title: SCAVENGER
emoji: 🥗
colorFrom: gray
colorTo: green
sdk: docker
app_port: 8000
pinned: false
---

# SCAVENGER · backend (Hugging Face Space)

Backend de SCAVENGER (FastAPI) desplegado como Space tipo **Docker**. Sirve la
API y la interfaz web en la misma URL pública del Space.

Este archivo se usa **solo como README del Space** (su front-matter le indica a
Hugging Face que construya el `Dockerfile` del repo y exponga el puerto 8000).
Lo publica automáticamente el workflow `.github/workflows/huggingface.yml`.

## Variables del Space (Settings → Variables and secrets)

- `SCAVENGER_DATABASE_URL` (**secret**): cadena de conexión Postgres
  (Neon/Supabase), ej:
  `postgresql://usuario:clave@host.neon.tech/scavenger?sslmode=require`

Sin esa variable, el backend usa SQLite efímero (los datos se reinician en cada
rebuild del Space).

Ver `docs/DEPLOY.md` en el repositorio para el paso a paso completo.
