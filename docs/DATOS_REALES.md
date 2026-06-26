# Cargar datos reales en SCAVENGER

Checklist para reemplazar las **estimaciones modeladas** por **precios y
nutrición reales** (scraping de cadenas + FatSecret). El código ya está listo
y probado; lo único pendiente es **acceso a internet** y, para FatSecret, las
**credenciales**.

Hay dos caminos. **El más simple es el local** (opción A): no requiere Claude
Code on the web ni tocar configuración de red.

---

## Opción A — Correr localmente (recomendado, sin Claude Code web)

El bloqueo de red es exclusivo del entorno cloud administrado. En **tu propio
computador** (o cualquier servidor con internet normal) no existe esa
restricción: los comandos pegan directo a Jumbo/Líder/FatSecret.

```bash
git clone <url-del-repo> && cd Scavenger
pip install -r requirements.txt          # o usa Docker (ver README)

# Credenciales de FatSecret (gratis en https://platform.fatsecret.com/)
export SCAVENGER_FATSECRET_KEY=<tu_consumer_key>
export SCAVENGER_FATSECRET_SECRET=<tu_consumer_secret>

make refresh    # precios reales: Jumbo, Santa Isabel, Líder  -> BD local
make enrich     # nutrición real vía FatSecret                -> BD local
make export     # vuelca la BD a data/chilean_foods.json (para versionar)
make run        # http://localhost:8000 con datos reales
```

Luego revisa los datos, corre `make test`, y commitea `data/chilean_foods.json`
con los datos reales (ver sección **Verificar y commitear**).

---

## Opción B — En el entorno cloud (Claude Code on the web)

Solo si usas el entorno cloud administrado. Requiere editar el acceso de red,
que **solo se hace por la interfaz web** (el terminal `/remote-env` no cambia la
red). Si no puedes acceder a la web, usa la **Opción A**.

> El acceso a red se fija **al iniciar la sesión**: tras cambiarlo hay que
> **abrir una sesión nueva** para que tome efecto (la sesión en curso no lo
> recoge).

---

### B.1 — Habilitar los hosts en el allowlist de egress

En la web/app de Claude Code:

1. Abre el **selector de entorno** (ícono de nube, donde inicias la sesión) y
   edita el entorno que usas con este repo.
2. En **Network access**, cambia de *Trusted* a **Custom**.
3. En **Allowed domains**, agrega una por línea:

   ```
   *.jumbo.cl
   *.santaisabel.cl
   *.lider.cl
   *.fatsecret.com
   ```

4. Marca **"Also include default list of common package managers"** para que
   `pip`/`git` sigan funcionando.
5. Guarda.

| Cadena / servicio | Host(s)                                   | Para qué |
|-------------------|-------------------------------------------|----------|
| Jumbo (VTEX)      | `www.jumbo.cl`                            | precios  |
| Santa Isabel (VTEX) | `www.santaisabel.cl`                    | precios  |
| Líder / Walmart   | `apps.lider.cl`                           | precios  |
| FatSecret         | `oauth.fatsecret.com`, `platform.fatsecret.com` | nutrición |

### B.2 — Credenciales de FatSecret (solo para `enrich`)

1. Crea una app gratuita en la [FatSecret Platform API](https://platform.fatsecret.com/).
2. En **Environment variables** del entorno, agrega:

   ```
   SCAVENGER_FATSECRET_KEY=<tu_consumer_key>
   SCAVENGER_FATSECRET_SECRET=<tu_consumer_secret>
   ```

   (Opcional) `SCAVENGER_FATSECRET_REGION=CL`, `SCAVENGER_FATSECRET_LANGUAGE=es`.

### B.3 — Iniciar una sesión nueva

La sesión actual **no** recoge el cambio de red. Abre una **sesión nueva** sobre
este repositorio.

---

## Cargar los datos (ambas opciones)

```bash
make refresh    # precios reales: Jumbo, Santa Isabel, Líder   -> BD
make enrich     # nutrición real vía FatSecret                 -> BD
make export     # vuelca la BD a data/chilean_foods.json       (para versionar)
```

Equivalente directo:

```bash
python3 -m backend.refresh_prices                      # todas las cadenas
python3 -m backend.refresh_prices --retailer lider --limit 20 --no-cache
python3 -m backend.enrich_nutrition                    # solo nutrición faltante
python3 -m backend.enrich_nutrition --all --limit 20   # recalcula por tandas
python3 -m backend.export_catalog                      # BD -> data/chilean_foods.json
```

- Los resultados de scraping se cachean en `data/cache/` (no se versiona).
- `refresh` actualiza los precios por cadena en la BD; `enrich` rellena la
  nutrición faltante (salvo `--all`). `export` deja todo en
  `data/chilean_foods.json` para commitearlo y compartirlo.

## Opción C — Llevar datos reales al backend desplegado (producción)

El backend desplegado (Hugging Face Space + Postgres/Supabase) **no** scrapea
desde el Space (su egress es restringido). Hay dos formas de poblar la BD de
producción con datos reales:

### C.1 — Commitear `chilean_foods.json` y refrescar en el deploy

1. Genera los datos reales (Opción A o B) y commitea `data/chilean_foods.json`.
2. En los **secrets/variables del Space**, define temporalmente
   `SCAVENGER_SEED_REFRESH=1`. Por defecto el seed solo **inserta** alimentos
   nuevos y **respeta** los precios ya cargados; con `=1`, el próximo arranque
   **actualiza** los alimentos existentes desde el JSON commiteado.
3. Redespliega (merge a `main` dispara `huggingface.yml`). Tras confirmar que la
   BD quedó con los datos reales, vuelve a poner `SCAVENGER_SEED_REFRESH=0`
   (o quítalo) para no sobrescribir en cada reinicio.

### C.2 — Scraping programado directo a la BD (GitHub Actions)

El workflow **`Refresh real data (prices + nutrition)`**
(`.github/workflows/refresh-data.yml`) corre en un runner de GitHub (egress
abierto) y escribe **directo** en la BD de producción. Es resiliente: si una
cadena falla (red/esquema/403), las demás siguen.

1. Define el secret **`SCAVENGER_DATABASE_URL`** del repo con la cadena de
   conexión de producción (Supabase *Session pooler*, IPv4). Opcional para
   nutrición: `SCAVENGER_FATSECRET_KEY` / `SCAVENGER_FATSECRET_SECRET`.
2. Ejecútalo manualmente desde **Actions → Refresh real data → Run workflow**
   (puedes acotar `retailers`, `limit` y `enrich`), o deja el `schedule`
   semanal. Equivale a `make refresh-data` apuntando a la BD de producción.

> Nunca pegues la cadena de conexión real en el chat ni la commitees: va solo
> como **secret** del repositorio/Space.

---

## Verificar y commitear

1. Revisa el matching de productos, los tamaños de envase parseados y los
   precios (`GET /api/foods`, `GET /api/foods/{q}` o la pestaña **Catálogo**).
2. Si Líder devuelve un esquema distinto al esperado, ajusta el endpoint o las
   claves candidatas:
   - `SCAVENGER_LIDER_BASE_URL`, `SCAVENGER_LIDER_SEARCH_PATH`, o
   - las listas de claves en `backend/providers/lider.py`.
3. Corre la suite: `make test`.
4. Commitea `data/chilean_foods.json` (ya con datos reales) y abre un PR.

---

## Notas y límites realistas

- **Líder**: su BFF no pudo inspeccionarse en vivo durante el desarrollo (host
  bloqueado), así que puede requerir un **ajuste menor de mapeo** la primera vez.
  El adaptador es tolerante al esquema (varias claves candidatas) y configurable
  por entorno.
- **FatSecret**: el plan gratuito limita región y volumen de consultas. Conviene
  correr `enrich` **por tandas** con `--limit`.
- **Scraping responsable**: respeta los términos de uso de cada sitio y usa la
  caché para no repetir consultas. El refresco aplica una pausa (`--sleep`)
  entre llamadas.
- **Nada de esto cambia la arquitectura**: los proveedores son pluggables y el
  resto del sistema (optimizador, minutas, lista de compras) es agnóstico a la
  fuente de datos.
