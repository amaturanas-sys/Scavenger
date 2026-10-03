# Inventario de modelos editable en Nomad Sculpt

Esta carpeta trae un **modelo de partida** (blockout low-poly) para cada objeto de [`assets/inventario.tsv`](../inventario.tsv), listo para abrir, esculpir y pintar en [Nomad Sculpt](https://nomadsculpt.com/) (iPad/Android), y un **atlas de materiales** compartido que puedes pintar a tu gusto.

Todo sale de [`tools/assets/nomad.py`](../../tools/assets/nomad.py). Los archivos de aquí **no los lee el juego**: el juego carga solo `assets/models/` y `assets/textures/`. Cuando un modelo esté listo, cópialo a su ruta del juego (ver más abajo).

| Archivo | Qué es |
|---|---|
| `modelos/<categoria>/<subcategoria>/<nombre>.glb` | Un modelo por id del inventario, en la misma estructura de carpetas que el juego. |
| [`INVENTARIO_MODELOS.md`](INVENTARIO_MODELOS.md) | Índice: archivo Nomad, destino en el juego, medidas, triángulos actuales/máximos, rig, celdas del atlas y estado. |
| [`inventario_modelos.tsv`](inventario_modelos.tsv) | Los mismos datos, para abrir en una hoja de cálculo. |
| [`atlas_materiales.png`](atlas_materiales.png) | El atlas de 256×256 px (8×8 celdas de 32 px). **Es tuyo: píntalo.** |
| [`atlas_materiales_guia.png`](atlas_materiales_guia.png) | Copia ×4 con la rejilla, el número y el nombre de cada celda, para tener al lado mientras pintas. |
| [`atlas_materiales.tsv`](atlas_materiales.tsv) | Lista de celdas: número, columna, fila, nombre y uso. |
| `hoja_modelos_*.png` | Hojas de miniaturas de todos los modelos (vista isométrica; la línea roja marca el frente, +X). |

> **Nada de esta carpeta se sobrescribe sin `--forzar`.** Si ya existe `atlas_materiales.png` o un `.glb`, el script lo deja como está: lo que pintes o esculpas aquí está a salvo. La guía, los TSV, el índice y las hojas sí se regeneran siempre.

## Convenciones (las mismas del juego)

- **Metros**, **+Y arriba**, **+X es el frente** (el rostro de un personaje, la puerta de una yurta, la proa de un barco, la cabeza de un caballo).
- Medidas del inventario `ancho x alto x largo`: **largo en X**, **alto en Y**, **ancho en Z** (igual que el marcador de `src/world/props.c`). Cada modelo de partida mide exactamente eso.
- **Origen** en el suelo (Y = 0), en el centro de la base. Los objetos con etiqueta `manos:*` (armas, escudos, antorcha, pala, gancho) tienen el origen en el **punto de agarre**.
- Low-poly y mate: cada modelo está por debajo de su `tris_max`. Ver [assets/models/README.md](../models/README.md).
- Cada pieza es un nodo con nombre (`cabeza`, `torso`, `brazo_izq`, `pata_delantera_der`, `hoja`, `empunadura`, `techo`...), así que en Nomad aparece como un objeto aparte. La izquierda del personaje es −Z y su derecha, +Z.
- Los 10 ids con etiqueta `formato:textura` (suelos y tatuajes) son **texturas 2D**: no tienen modelo; el índice los marca como «textura 2D».

## Pintar el atlas

1. Abre `atlas_materiales.png` en cualquier editor (Procreate, Krita, Photoshop, Aseprite, Pixelorama…). Ten a mano `atlas_materiales_guia.png`.
2. **Respeta la rejilla de 8×8 celdas de 32 px**: cada pieza de cada modelo usa una celda entera (con 2 px de margen). Pinta dentro de la celda lo que quieras: vetas, costuras, pelaje, motivos de fieltro. El tamaño debe seguir siendo 256×256.
3. Paleta y estilo: [docs/ESTILO_VISUAL.md](../../docs/ESTILO_VISUAL.md) (oro, turquesa, cornalina, cuero…). Pixel art sin degradados suaves; el juego usa filtro de punto.
4. Qué celda va en cada pieza: [`atlas_materiales.tsv`](atlas_materiales.tsv) y la columna «Celdas» del índice.
5. Para volver a dibujar la guía con tu atlas nuevo:
   ```bash
   python3 tools/assets/nomad.py atlas
   ```
6. Cada `.glb` lleva **su propia copia** del atlas embebida. Los modelos que regeneres a partir de ahora usarán el atlas nuevo (y su color medio por celda). Para regenerar los que **aún no tocaste**:
   ```bash
   python3 tools/assets/nomad.py modelos --forzar animal.ganado   # solo los ids que empiezan así
   ```
   Ojo: `--forzar` pisa lo esculpido en esos archivos. Para un modelo ya esculpido, cambia la textura dentro de Nomad.

## Flujo en Nomad Sculpt, paso a paso

1. **Elige el modelo** en [`INVENTARIO_MODELOS.md`](INVENTARIO_MODELOS.md) y copia su `.glb` de `modelos/` al iPad o al teléfono (AirDrop, Archivos/iCloud Drive, Google Drive, cable…).
2. **Importa** en Nomad: menú de archivos → *Import* → elige el `.glb` (glTF). Cada pieza llega como un objeto con su nombre, con su color de vértice y la textura del atlas.
3. **Esculpe.**
   - El modelo es un blockout: las piezas son mallas cerradas con aristas vivas (normales planas). Para esculpir con detalle, aplica **Voxel remesh** (une la pieza en una malla uniforme) y, si quieres más resolución, **Multiresolución**.
   - Respeta las medidas: puedes medir el objeto contra el original antes de remallar. No muevas el origen ni el frente (+X).
   - Para el juego, la malla final debe quedar **por debajo de `tris_max`** (columna «Tris / máx» del índice): usa **Decimate** antes de exportar o vuelve a un nivel bajo de multiresolución.
4. **Pinta.** Dos opciones:
   - **Color de vértice** (la pintura normal de Nomad): los modelos ya traen el color medio de cada celda en los vértices.
   - **Textura del atlas**: deja los UV como están y pinta el atlas (sección anterior). Sirve mientras no remalles; un remallado pierde los UV.
5. **Exporta**: menú de archivos → *Export* → **glTF (.glb)**. Activa los **colores de vértice** y, si conservaste los UV, la **textura**. Escala 1, sin cambiar ejes (Nomad y glTF usan +Y arriba).
6. **Llévalo al juego**: guarda el `.glb` en la ruta de la columna «Destino en el juego» (`assets/models/<categoria>/<subcategoria>/<nombre>.glb`, con ese nombre exacto).
7. En [`assets/inventario.tsv`](../inventario.tsv), cambia su `estado` a `importado` y valida:
   ```bash
   python3 tools/assets/inventario.py check && python3 tools/assets/inventario.py doc
   ```
8. Míralo a escala real junto a los demás:
   ```bash
   ./build/estepa --galeria
   ```

**Personajes, animales y objetos con mecanismo:** Nomad no hace rigging ni animación. Las piezas vienen separadas y con nombres de parte del cuerpo para facilitar el trabajo, pero el esqueleto (`rig:humanoide`, `rig:cuadrupedo`, `rig:ave`) y los clips de [docs/ANIMACIONES.md](../../docs/ANIMACIONES.md) se añaden después (por ejemplo en Blender) antes de marcarlos `importado`. Si no, `python3 tools/assets/animaciones.py check` falla en CI.

## Comandos

```bash
python3 tools/assets/nomad.py todo        # atlas (si falta) + modelos que falten + índice
python3 tools/assets/nomad.py verificar   # relee cada GLB: estructura, medidas (±5 %), tris_max y origen
python3 tools/assets/nomad.py hoja        # hojas de miniaturas hoja_modelos_*.png
```

`verificar` trata como avisos (no errores) las diferencias de medidas o de triángulos en un `.glb` que ya no es el generado (por ejemplo, uno exportado desde Nomad y guardado aquí). Más opciones en [tools/assets/README.md](../../tools/assets/README.md).

## Detalles técnicos

- glTF 2.0 binario, validado con el [glTF-Validator](https://github.com/KhronosGroup/glTF-Validator) de Khronos sin errores ni avisos.
- Un material mate (`metallicFactor` 0, `roughnessFactor` 1) con el atlas embebido como PNG y muestreo `NEAREST`.
- Atributos: posición, normal plana por cara, UV (cada pieza en su celda, con 2 px de margen) y `COLOR_0` (color medio de la celda, lineal).
- Según la especificación glTF, el color de vértice **multiplica** la textura: un visor que aplique las dos cosas verá los colores un poco más oscuros. Si te molesta, genera sin colores de vértice: `python3 tools/assets/nomad.py modelos --forzar --sin-colores <prefijo>`.
