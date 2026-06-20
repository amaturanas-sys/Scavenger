# 🍃 SCAVENGER

**Herramienta de _meal prep_ costo-efectiva para Chile.**

SCAVENGER genera pautas de alimentación (minutas diarias y semanales) que
**cumplen los requerimientos nutricionales** de cada usuario al **menor costo
posible**, usando precios del retail chileno (referencia tipo Jumbo) y valores
nutricionales declarados (referencia FatSecret Chile). Aprende de la
**saciedad** y las **preferencias** reportadas para afinar las sugerencias.

---

## ¿Qué hace?

1. **Perfil + requerimientos.** A partir de sexo, edad, peso, estatura,
   actividad y objetivo calcula las calorías (Mifflin-St Jeor + factor de
   actividad), macronutrientes, fibra y mínimos de micronutrientes.
2. **Optimización costo-efectiva.** Resuelve la _"dieta de costo mínimo"_
   (programación lineal, variante del problema de Stigler) para elegir cuántos
   gramos de cada alimento incluir y cubrir las metas gastando lo menos posible.
3. **Minutas por comida.** Distribuye la canasta diaria en desayuno, almuerzo,
   once y cena según afinidades por categoría y reparto calórico.
4. **Guardado + saciedad.** Permite guardar minutas y registrar un **puntaje de
   saciedad** (1–5) y de costo. Con ese feedback **aprende preferencias** por
   alimento que modifican el "costo efectivo" en futuras optimizaciones.
5. **Catálogo.** Explora alimentos con su nutrición por 100 g, precio por 100 g
   e índice de saciedad.

---

## Arquitectura

```
backend/                FastAPI + SQLAlchemy
  main.py               app, monta routers y sirve el frontend
  config.py             configuración por variables de entorno
  database.py           engine/sesión SQLite
  models.py             Food, User, Plan, Feedback, Preference
  schemas.py            esquemas Pydantic
  nutrition.py          requerimientos (Mifflin-St Jeor, macros, micros)
  optimizer.py          motor PL de dieta de costo mínimo (PuLP/CBC)
  planner.py            reparto de la canasta en comidas del día
  learning.py           aprendizaje de preferencias por saciedad/gusto
  services.py           orquestación (diario / semanal)
  seed.py               carga del catálogo a la BD
  providers/            fuentes de datos pluggables
    base.py             interfaz FoodProvider + FoodRecord
    local.py            dataset local (offline, por defecto)
    jumbo.py            scraper Jumbo (VTEX) — listo para conectar
    fatsecret.py        API FatSecret (OAuth) — listo para conectar
  routers/              users, foods, plans, feedback
data/
  chilean_foods.json    catálogo semilla (precios + nutrición referenciales)
frontend/               SPA sin framework (HTML/CSS/JS)
tests/                  pruebas de nutrición y optimizador
```

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

```bash
pip3 install -r requirements.txt
./scripts/run.sh              # http://localhost:8000  (docs en /docs)
```

El catálogo se carga automáticamente en el arranque (seed idempotente).
Para recargarlo/actualizarlo manualmente:

```bash
python3 -m backend.seed local   # o: jumbo / fatsecret
```

### Pruebas

```bash
python3 -m pytest -q
```

---

## API (resumen)

| Método | Ruta                              | Descripción                                  |
|--------|-----------------------------------|----------------------------------------------|
| POST   | `/api/users`                      | Crear usuario                                |
| PATCH  | `/api/users/{id}`                 | Actualizar perfil                            |
| GET    | `/api/users/{id}/requirements`    | Requerimientos nutricionales calculados      |
| GET    | `/api/foods`                      | Catálogo (filtros `q`, `category`)           |
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
| `SCAVENGER_JUMBO_ENABLED`      | `0`                    | Habilita scraping de Jumbo          |
| `SCAVENGER_FATSECRET_KEY/SECRET` | —                    | Credenciales FatSecret (OAuth)      |

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
