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
