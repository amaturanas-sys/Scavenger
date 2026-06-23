# Desplegar el backend gratis (Hugging Face Spaces + Postgres)

Objetivo: que la app (web y **APK**) corra contra un **backend público y
gratuito**, sin depender de tu PC. Usamos:

- **Hugging Face Spaces** (Docker) para alojar el backend → URL pública estable.
- **Postgres gratuito** (Neon o Supabase) para que los datos **persistan**.

GitHub no aloja servidores persistentes; lo que hacemos es **desplegar desde
GitHub** a estos servicios con un workflow automático.

```
GitHub (push a main) ──▶ workflow ──▶ HF Space (Docker)  ──▶  https://usuario-scavenger.hf.space
                                            │
                                            └── SCAVENGER_DATABASE_URL ──▶ Postgres (Neon/Supabase)
APK / navegador  ──────────────── HTTP /api ───────────────▶  (la misma URL del Space)
```

## 1. Base de datos Postgres (gratis, persistente)

Elige una (ambas sin tarjeta):

- **Neon** (https://neon.tech): crea un proyecto → copia la *connection string*.
  Queda como:
  `postgresql://usuario:clave@ep-xxx.neon.tech/neondb?sslmode=require`
- **Supabase** (https://supabase.com): Project → Settings → Database →
  *Connection string* (modo "URI", usa el puerto del pooler).

> Si la cadena trae `&channel_binding=require`, quítalo (deja solo
> `?sslmode=require`).

## 2. Crear el Space en Hugging Face

1. https://huggingface.co → **New Space**.
2. **SDK: Docker** (Blank). Nombre, ej: `scavenger`. Visibilidad: pública.
3. En el Space → **Settings → Variables and secrets** → agrega un **secret**:
   - `SCAVENGER_DATABASE_URL` = la cadena de Postgres del paso 1.

## 3. Conectar GitHub → Space (despliegue automático)

1. Crea un **token de escritura** de Hugging Face:
   https://huggingface.co/settings/tokens (rol *write*).
2. En tu repo de GitHub → **Settings → Secrets and variables → Actions**:
   - **Secret** `HF_TOKEN` = el token de Hugging Face.
   - **Variable** `HF_SPACE_ID` = `tu-usuario/scavenger` (el id del Space).
3. Lanza el deploy: haz un push a `main` (o **Actions → Deploy backend to
   Hugging Face Space → Run workflow**).

El workflow empuja el repo al Space; Hugging Face construye el `Dockerfile` y
publica el backend. En el primer arranque se crean las tablas y se carga el
catálogo + usuario demo en Postgres (idempotente).

Tu backend queda en: `https://TU-USUARIO-scavenger.hf.space`
- Ábrelo en el navegador: es la **web completa** funcionando con Postgres.
- La API está en `…hf.space/api/...`.

## 4. Apuntar la APK al backend público

1. En GitHub → **Settings → Secrets and variables → Actions → Variables**:
   - **Variable** `SCAVENGER_API_BASE` = `https://TU-USUARIO-scavenger.hf.space`
2. Lanza **Actions → Android APK → Run workflow** (o push que toque
   `frontend/`/`android/`). El build **hornea** esa URL en la APK.
3. Descarga la APK desde **Releases → `android-latest`** e instálala: ya viene
   apuntando al backend público (puedes cambiar la URL con el botón ⚙️).

## Notas

- **HTTPS gratis**: el Space ya entrega `https://`, así que la APK no necesita
  permitir tráfico en claro para este backend (sí lo permite igual para pruebas
  con un backend local por `http://`).
- **CORS**: el backend responde con `Access-Control-Allow-Origin: *`.
- **Robustez de conexión**: el engine usa `pool_pre_ping`/`pool_recycle` para
  tolerar el cierre de conexiones ociosas de Postgres administrado.
- **Sin Postgres**: si no defines `SCAVENGER_DATABASE_URL`, el Space usa SQLite
  efímero (datos se reinician en cada rebuild). Sirve para una demo rápida.
- **Costo**: HF Spaces (Docker, CPU básico) y Neon/Supabase tienen planes
  gratuitos suficientes para un prototipo. Los Spaces gratuitos pueden
  "dormir" tras inactividad prolongada y despertar al primer request.
