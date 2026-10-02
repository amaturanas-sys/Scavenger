# Assets 3D con Kiln

Los modelos se escriben como programas JavaScript para [Kiln](https://github.com/matthew-kissinger/kiln) (MIT): se versiona el **programa** y el GLB es un derivado.

| Programa | Salida | Triángulos |
|---|---|---|
| `yurta_comun.kiln.js` | `assets/models/estructura/vivienda/yurta_comun.glb` | 296 |

## Instalar Kiln (una vez)

```bash
git clone --depth 1 https://github.com/matthew-kissinger/kiln ~/kiln
cd ~/kiln && npm install --omit=dev
node scripts/create-workspace.mjs ~/kiln-workspace --harness claude
```

## Exportar un asset

```bash
cd ~/kiln-workspace
node kiln.mjs render /ruta/al/repo/tools/assets/yurta_comun.kiln.js --render cpu \
  --out /ruta/al/repo/assets/models/estructura/vivienda/yurta_comun.glb --views yurt_vistas.png
```

`--views` genera una hoja con 6 vistas para revisar el modelo; el reporte de QA de Kiln valida la exportación y el costo de render.

## Convenciones
- Marco de Kiln: +X adelante, +Y arriba, Y=0 es el suelo. En el juego, la "puerta" o frente de un objeto mira a +X.
- Materiales mate (`metalness` 0) mientras sea posible: render por CPU y estética low-poly.
- Presupuesto orientativo: props < 1.000 triángulos, personajes < 3.000.

# text-to-cad (activado)

El skill **CAD** de [text-to-cad](https://github.com/earthtojake/text-to-cad) (MIT, cadgen 0.7.6) está en `.claude/skills/cad`: Claude Code lo carga solo al trabajar en este repositorio. Sirve para:
- **Revisar un GLB importado:** fotos desde varias cámaras y el CAD Viewer para medirlo contra las medidas del inventario.
- **Piezas de superficie dura** (ballestas, cañones, maquinaria de asedio, placas de armadura): se modelan en Python (build123d) y se exportan a GLB.

```bash
python3 tools/assets/cad.py instalar                              # una vez: crea .venv-cad con cadgen y Chromium
python3 tools/assets/cad.py foto estructura.vivienda.yurta_comun  # fotos en build/cad/ (iso, right, front, top)
python3 tools/assets/cad.py foto asedio.maquina.ariete iso back    # cámaras a elección
python3 tools/assets/cad.py visor                                 # CAD Viewer sobre assets/models
```

cadgen respeta el +Y arriba de los GLB. Como en el juego el frente de un objeto es +X, la cámara `right` muestra **el frente** (la puerta de la yurta, el rostro de un personaje) y `front` muestra su costado.

Para actualizar el skill: copiar de nuevo `skills/cad` del repositorio de text-to-cad a `.claude/skills/cad` (sin la carpeta `agents/`).
