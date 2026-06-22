# 🍃 SCAVENGER

**Herramienta de _meal prep_ costo-efectiva para Chile.**

SCAVENGER genera pautas de alimentación (minutas diarias y semanales) que
**cumplen los requerimientos nutricionales** de cada usuario al **menor costo
posible**, comparando precios entre las **principales cadenas de supermercado
de Chile** (Jumbo, Líder, Santa Isabel, Tottus, Unimarc y Mayorista 10) y
usando valores nutricionales declarados (referencia FatSecret Chile). Aprende
de la **saciedad** y las **preferencias** reportadas para afinar las sugerencias.

> **Catálogo actual:** ~84 alimentos × 6 cadenas. El usuario indica qué
> supermercados tiene cerca y SCAVENGER **compra cada producto en la cadena más
> barata** de esas. (En pruebas, la misma dieta cuesta ~30% menos comprando en
> mayoristas que restringido a cadenas premium.)

---

## ¿Qué hace?

1. **Perfil + requerimientos.** A partir de sexo, edad, peso, estatura,
   actividad y objetivo calcula las calorías (Mifflin-St Jeor + factor de
   actividad), macronutrientes, fibra y mínimos de micronutrientes. El usuario
   también marca las **cadenas de supermercado que tiene cerca**.
2. **Optimización costo-efectiva multi-cadena.** Para cada alimento toma el
   **precio más barato entre las cadenas elegidas** y resuelve la _"dieta de
   costo mínimo"_ (programación lineal, variante del problema de Stigler):
   elige cuántos gramos de cada alimento y **en qué supermercado comprarlo**
   para cubrir las metas gastando lo menos posible.
   - **Selector de presupuesto** (junto a los requerimientos, guía las
     decisiones): `Gastar lo mínimo` (tope que no se supera), `Aprovechar el
     presupuesto` (maximiza la **saciedad** sin pasarse del monto) o `Sin
     límite`. El monto es por día y se puede ajustar en cada generación.
3. **Minutas por comida.** Distribuye la canasta diaria en desayuno, almuerzo,
   once y cena según afinidades por categoría y reparto calórico.
3b. **Constructor de comidas (🎰 tragamonedas).** En vez de auto-generar, el
   usuario arma cada comida (desayuno/snack/almuerzo/cena) eligiendo un elemento
   por **rol** (proteínas, carbohidratos, grasas, vegetales, aderezos) en
   carretes deslizables. Cada candidato viene **pre-porcionado** para su rol, de
   modo que cualquier combinación suma valores cercanos a la meta de la comida
   («los macros calzan»). Hay un botón **Girar** para una combinación aleatoria,
   con suma de nutrición y costo en vivo. Se puede **fijar (🔒) un carrete** para
   que no gire mientras el resto sí, y soporta **snacks numerados** (Snack 1/2/3).
   (`backend/builder.py`, `POST /api/builder/slots|random|summary`.)
4. **Guardado + saciedad + historial.** Permite guardar minutas y registrar un
   **puntaje de saciedad** (1–5) y de costo. Con ese feedback **aprende
   preferencias** por alimento que modifican el "costo efectivo" en futuras
   optimizaciones, y muestra un **historial de saciedad** por usuario (promedios
   y evolución). (`GET /api/users/{id}/satiety-history`.)
5. **Catálogo.** Explora alimentos con su nutrición por 100 g, precio por 100 g
   e índice de saciedad.

---

## Arquitectura

```
backend/                FastAPI + SQLAlchemy
  main.py               app, monta routers y sirve el frontend
  config.py             configuración por variables de entorno
  database.py           engine/sesión SQLite
  models.py             Food, FoodPrice, User, Plan, Feedback, Preference
  schemas.py            esquemas Pydantic
  nutrition.py          requerimientos (Mifflin-St Jeor, macros, micros)
  optimizer.py          motor PL de dieta de costo mínimo (PuLP/CBC)
  planner.py            reparto de la canasta en comidas del día
  learning.py           aprendizaje de preferencias por saciedad/gusto
  services.py           orquestación (diario / semanal)
  seed.py               carga del catálogo a la BD
  pricing.py            refresco de precios por cadena (scraping)
  refresh_prices.py     CLI de scraping de precios (python -m)
  nutrition_enrich.py   completa nutrición de alimentos vía FatSecret
  enrich_nutrition.py   CLI de enriquecimiento nutricional (python -m)
  providers/            fuentes de datos pluggables
    base.py             interfaz FoodProvider + FoodRecord
    local.py            dataset local (offline, por defecto)
    vtex.py             cliente VTEX (parseo, matching, extraccion de oferta)
    jumbo.py            scraper Jumbo (VTEX)
    santa_isabel.py     scraper Santa Isabel (VTEX)
    lider.py            scraper Lider / Walmart (BFF propio, no VTEX)
    fatsecret.py        API FatSecret (OAuth): nutrición por 100 g
  routers/              users, foods, plans, feedback
data/
  foods_base.json       catálogo base autorado (nutrición + precio referencia)
  chilean_foods.json    catálogo generado (precios por cadena) ← lo usa la app
scripts/
  build_catalog.py      genera chilean_foods.json desde foods_base.json
  run.sh                arranca API + frontend
frontend/               SPA sin framework (HTML/CSS/JS)
tests/                  pruebas de nutrición, optimizador y catálogo
```

### Catálogo de alimentos y precios por cadena

- `data/foods_base.json` se **autora a mano**: cada alimento con su nutrición
  por 100 g y un `base_price_clp` de referencia.
- `scripts/build_catalog.py` genera `data/chilean_foods.json` con un **precio
  por cada cadena**, aplicando un modelo de posicionamiento
  (`precio = base × factor_cadena × ajuste_categoría × variación`). Los
  mayoristas quedan más baratos, las premium más caras, con variación por
  categoría (ferias/Santa Isabel mejores en frutas/verduras; mayoristas en
  abarrotes y carnes). Es reproducible: para **agregar alimentos o cadenas**,
  edita `foods_base.json` y vuelve a correr el script.

  ```bash
  python3 scripts/build_catalog.py   # regenera el catálogo
  ```

> Los precios por cadena parten como **estimaciones modeladas** y se
> reemplazan por **precios reales** con el scraper (ver más abajo).

### Conectar precios reales (scraping VTEX)

> 📋 Guía paso a paso para dejar **datos reales** (red + credenciales +
> comandos): [`docs/DATOS_REALES.md`](docs/DATOS_REALES.md).

Jumbo y Santa Isabel (Cencosud) exponen la misma API pública de catálogo VTEX.
El módulo `backend/pricing.py` recorre los alimentos del catálogo, busca cada
uno en la cadena, elige la mejor coincidencia (matching por nombre/marca),
parsea el **tamaño del envase** para normalizar a precio/100 g y actualiza su
`FoodPrice`. La nutrición autorada **no** se toca.

Cadenas con scraper hoy: **Jumbo** y **Santa Isabel** (VTEX) y **Líder /
Walmart** (BFF propio). Líder no es VTEX, por lo que tiene su propio adaptador
(`providers/lider.py`) hecho **tolerante al esquema**: el mapeo busca cada
campo (nombre, marca, precio, EAN) en varias ubicaciones conocidas y el
endpoint es configurable por entorno.

```bash
# Refresca precios reales de todas las cadenas con scraper
python3 -m backend.refresh_prices

# Solo una cadena, acotado y sin caché
python3 -m backend.refresh_prices --retailer lider --limit 20 --no-cache

# Vuelca el catálogo de la BD (ya con datos reales) al JSON versionable
python3 -m backend.export_catalog        # o: make export
```

> 💡 ¿Sin acceso a Claude Code on the web? Corre estos comandos **localmente**
> (tu internet no tiene allowlist) y commitea `data/chilean_foods.json`. Ver
> [`docs/DATOS_REALES.md`](docs/DATOS_REALES.md), **Opción A**.

**Requisito de red — allowlist de egress.** En Claude Code on the web la red
saliente está gobernada por la política del entorno. Para permitir el scraping,
agrega estos hosts al **allowlist de egress** del entorno (al crearlo o
editarlo; ver https://code.claude.com/docs/en/claude-code-on-the-web):

```
www.jumbo.cl
www.santaisabel.cl
apps.lider.cl
```

Si los hosts no están permitidos, el comando aborta con un mensaje claro
(`Host bloqueado por la política de red del entorno: ... Agrégalo al allowlist
de egress`). Las respuestas se cachean en `data/cache/` para no repetir
consultas.

> **Nota sobre Líder:** su BFF no pudo inspeccionarse en vivo desde este
> entorno (host bloqueado por la política de red). El adaptador está construido
> según la estructura conocida de la plataforma de Walmart Chile y es tolerante
> al esquema; si Walmart cambiara el endpoint o las claves, se ajustan vía
> `SCAVENGER_LIDER_BASE_URL` / `SCAVENGER_LIDER_SEARCH_PATH` o las listas de
> claves candidatas en `providers/lider.py`.

### Completar nutrición (FatSecret)

El scraping de precios trae productos sin valores nutricionales. El módulo
`backend/nutrition_enrich.py` los completa cruzando con **FatSecret** (Platform
API, OAuth 2.0, región CL): busca cada alimento, obtiene su detalle y
**normaliza la nutrición a valores por 100 g** (eligiendo la porción métrica
más cercana a 100 g y escalando). Solo rellena alimentos sin datos; no
sobreescribe la nutrición ya autorada (salvo `--all`).

```bash
# Completa solo los alimentos sin nutrición
python3 -m backend.enrich_nutrition

# Recalcula todos / acota
python3 -m backend.enrich_nutrition --all --limit 20
```

Requiere **credenciales** y los **hosts en el allowlist de egress**:

```
oauth.fatsecret.com
platform.fatsecret.com
```

```bash
export SCAVENGER_FATSECRET_KEY=...      # Consumer key
export SCAVENGER_FATSECRET_SECRET=...   # Consumer secret
```

Sin credenciales o con los hosts bloqueados, el comando aborta con un mensaje
claro.

### Proveedores de datos (pluggables)

El sistema es agnóstico a la fuente: todo se normaliza a `FoodRecord`.

- **`local`** (por defecto): dataset offline en `data/chilean_foods.json`.
- **`jumbo`**: scraping del catálogo VTEX de Jumbo (precios). Se activa con
  `SCAVENGER_JUMBO_ENABLED=1` cuando haya acceso a red.
- **`fatsecret`**: API de FatSecret para nutrición (región CL). Requiere
  `SCAVENGER_FATSECRET_KEY` y `SCAVENGER_FATSECRET_SECRET`.

> En esta primera versión el catálogo funciona **100% offline** con datos
> referenciales. Los scrapers/API quedan implementados como esqueletos listos
> para conectarse sin tocar el resto del sistema.

---

## Cómo correr

**Local (Python):**

```bash
pip3 install -r requirements.txt
./scripts/run.sh              # http://localhost:8000  (docs en /docs)
```

**Con Docker (un comando):**

```bash
docker build -t scavenger .
docker run --rm -p 8000:8000 scavenger   # http://localhost:8000
```

**Con Make (atajos):** `make install`, `make run`, `make test`, `make seed`,
`make refresh`, `make enrich`, `make docker-build`, `make docker-run`.

En el arranque se cargan automáticamente (idempotente) el **catálogo** y un
**usuario de demostración** ("Demo"), para que la app sea usable apenas se abre.
Para recargar/actualizar el catálogo manualmente:

```bash
python3 -m backend.seed local   # o: jumbo / fatsecret
```

### Pruebas

```bash
python3 -m pytest -q     # o: make test
```

---

## API (resumen)

| Método | Ruta                              | Descripción                                  |
|--------|-----------------------------------|----------------------------------------------|
| POST   | `/api/users`                      | Crear usuario                                |
| PATCH  | `/api/users/{id}`                 | Actualizar perfil                            |
| GET    | `/api/users/{id}/requirements`    | Requerimientos nutricionales calculados      |
| GET    | `/api/foods`                      | Catálogo con precios por cadena (filtros `q`, `category`) |
| GET    | `/api/foods/retailers`            | Cadenas de supermercado disponibles          |
| POST   | `/api/plans/generate`             | Generar minuta (diaria/semanal, no guarda)   |
| POST   | `/api/plans`                      | Guardar minuta                               |
| GET    | `/api/plans?user_id=`             | Listar minutas                               |
| POST   | `/api/plans/{id}/feedback`        | Registrar saciedad/costo → aprende           |

---

## Configuración (variables de entorno)

| Variable                       | Default                | Descripción                         |
|--------------------------------|------------------------|-------------------------------------|
| `SCAVENGER_DATABASE_URL`       | `sqlite:///scavenger.db` | Conexión de base de datos         |
| `SCAVENGER_FOOD_PROVIDER`      | `local`                | Proveedor de catálogo en el seed    |
| `SCAVENGER_PREFERENCE_WEIGHT`  | `0.35`                 | Peso de preferencias en el costo    |
| `SCAVENGER_LEARNING_RATE`      | `0.25`                 | Tasa de aprendizaje del feedback    |
| `SCAVENGER_KCAL_TOLERANCE`     | `0.05`                 | Banda ± sobre calorías objetivo     |
| `SCAVENGER_JUMBO_ENABLED`      | `1`                    | Habilita scraping de Jumbo          |
| `SCAVENGER_SANTA_ISABEL_ENABLED` | `1`                  | Habilita scraping de Santa Isabel   |
| `SCAVENGER_LIDER_ENABLED`      | `1`                    | Habilita scraping de Líder          |
| `SCAVENGER_LIDER_BASE_URL`     | `https://apps.lider.cl`| Host del BFF de Líder               |
| `SCAVENGER_LIDER_SEARCH_PATH`  | `/supermercado/bff/products?term={q}&page=1` | Ruta de búsqueda de Líder |
| `SCAVENGER_FATSECRET_KEY/SECRET` | —                    | Credenciales FatSecret (OAuth)      |
| `SCAVENGER_FATSECRET_REGION`   | `CL`                   | Región de FatSecret (localización)  |
| `SCAVENGER_FATSECRET_LANGUAGE` | `es`                   | Idioma de FatSecret                 |

---

## Notas y alcance

- Los **precios y valores nutricionales del dataset semilla son referenciales**
  para demostrar el flujo; con los proveedores conectados se reemplazan por
  datos en vivo.
- El optimizador acota porciones por alimento y **relaja restricciones de forma
  progresiva** (micronutrientes → fibra → banda calórica) si no hay solución
  factible, informando con _warnings_.
- El índice de saciedad usa una escala relativa estilo Holt (pan blanco = 100).

> ⚠️ SCAVENGER es una herramienta de apoyo a la planificación, **no** constituye
> indicación médica ni nutricional profesional.
