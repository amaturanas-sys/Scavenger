# Modelos del juego

Cada objeto de [`assets/inventario.tsv`](../inventario.tsv) tiene una ruta fija, derivada de su id:

| Id | Archivo |
|---|---|
| `estructura.vivienda.yurta_comun` | `assets/models/estructura/vivienda/yurta_comun.glb` |
| `accesorio.tatuaje.lobo` (etiqueta `formato:textura`) | `assets/textures/accesorio/tatuaje/lobo.png` |

Mientras el archivo no existe, el juego dibuja un **marcador**: una caja del color de la categoría, con las medidas del inventario y una línea roja hacia el frente. Al dejar el modelo en su ruta, el juego lo usa solo; no hace falta tocar código.

Lista completa y avance: [docs/INVENTARIO.md](../../docs/INVENTARIO.md).

## Cómo importar un modelo

1. Exporta el modelo como **GLB** (glTF binario), con las texturas embebidas.
2. Guárdalo en la ruta de su id: `assets/models/<categoria>/<subcategoria>/<nombre>.glb`.
3. En `assets/inventario.tsv`, cambia su `estado` a `importado`. Cuando esté refinado, cámbialo a `refinado`.
4. Valida y regenera la documentación:
   ```bash
   python3 tools/assets/inventario.py check   # errores: nombres que no coinciden, estados incoherentes
   python3 tools/assets/inventario.py doc     # actualiza docs/INVENTARIO.md
   ```
5. Revísalo en el juego a escala real, junto a los demás:
   ```bash
   ./build/estepa --galeria
   ```
   La galería pone todos los objetos en filas por categoría. Arriba muestra el id, el nombre, las medidas y el estado del objeto más cercano; las etiquetas en turquesa son modelos ya cargados.

El CI valida el inventario y falla si un archivo no corresponde a ningún id o si `docs/INVENTARIO.md` está desactualizado.

## Convenciones

| Aspecto | Regla |
|---|---|
| Unidades | **Metros**. Las medidas del inventario son la referencia (`ancho x alto x largo`). |
| Ejes | **+Y arriba**, **+X adelante** (el frente del objeto, el rostro de un personaje, la proa de un barco). Es la convención de Kiln. |
| Origen | En el **suelo** (Y=0), en el centro de la base. Las armas y piezas equipables tienen el origen en el punto de agarre o de anclaje. |
| Polígonos | Sin pasar de `tris_max` del inventario. El estilo es low-poly: menos es mejor (Android). |
| Materiales | Mate (metalness 0), colores planos o texturas pequeñas (≤ 256 px) con filtro de punto. Paleta: [docs/ESTILO_VISUAL.md](../../docs/ESTILO_VISUAL.md). |
| Personajes y animales | Deben estar riggeados (`rig:humanoide`, `rig:cuadrupedo` o `rig:ave`). La ropa y la armadura se ajustan a los cuerpos base (`personaje.base.*`). |
| Nombres | Solo minúsculas, dígitos y `_`. El nombre del archivo es la última parte del id. |

## Animaciones

Los personajes, los animales y los objetos con mecanismo (vehículos, máquinas de asedio, puertas) traen sus animaciones dentro del mismo GLB. El índice [docs/ANIMACIONES.md](../../docs/ANIMACIONES.md), que sale de [`assets/animaciones.tsv`](../animaciones.tsv), dice qué clips necesita cada modelo.

| Aspecto | Regla |
|---|---|
| Nombre de cada clip | **Exactamente** el de la columna `clip` (p. ej. `caminar`, `galopar`, `construir`). El juego los busca por nombre. |
| Velocidad | **30 fps**. Los clips con `bucle: si` deben cerrar el ciclo: el último fotograma enlaza con el primero. |
| Esqueleto compartido | Todos los cuerpos humanos usan el mismo esqueleto, y todos los cuadrúpedos el suyo. La ropa, la armadura y las bardas se pesan (skinning) a ese esqueleto y no llevan clips propios. |
| Raíz | Los clips no desplazan la raíz: el juego mueve al personaje. El clip anima en el sitio. |
| Sin un clip | El juego usa `idle`. Un modelo `importado` o `refinado` sin sus clips obligatorios falla en CI. |

Para ver qué clips necesita un modelo:
```bash
python3 tools/assets/animaciones.py modelo animal.montura.caballo_estepario
python3 tools/assets/animaciones.py check   # comprueba los GLB importados
```

## Refinado: text-to-cad y Kiln

- **text-to-cad** ([earthtojake/text-to-cad](https://github.com/earthtojake/text-to-cad)), **activado** en `.claude/skills/cad`:
  - `python3 tools/assets/cad.py foto <id>` saca fotos del GLB (iso, frente, costado, arriba) en `build/cad/`.
  - `python3 tools/assets/cad.py visor` abre el **CAD Viewer** para revisar y medir contra las medidas del inventario.
  - `cadgen` sirve para las piezas de superficie dura (ballestas, cañones, maquinaria de asedio, placas de armadura); exporta GLB con `cadgen glb build`.
- **Kiln** ([matthew-kissinger/kiln](https://github.com/matthew-kissinger/kiln)):
  - Reescribe o ajusta el modelo como programa low-poly (`tools/assets/*.kiln.js`).
  - Su informe de QA verifica la exportación, el conteo de triángulos y el costo de render.
  - Ver [tools/assets/README.md](../../tools/assets/README.md).
