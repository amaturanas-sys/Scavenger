---
name: modelos
description: La tubería de modelos 3D de SCAVENGERS THRIVE (imagen → TRELLIS.2 → low-poly → juego; RigNet para el esqueleto). Usar cuando el usuario quiera agregar o reemplazar un modelo, pase una URL de Hugging Face o un GLB, pregunte qué imágenes hacen falta, o al revisar un modelo importado.
---

# La tubería de modelos

Lo que el juego necesita de un modelo está en `assets/inventario.tsv` (medidas, `tris_max`, `rig`) y en
`assets/models/README.md` (ejes, origen, estilo). La tubería lo deja así sola: `tools/modelos/`.

## Pasos

1. **Elegir el id.** `python3 tools/modelos/modelos.py estado` lista lo que falta por prioridad
   (personas, animales, armas y armaduras, estructuras, el resto). `estado <id>` dice sus medidas,
   su presupuesto de triángulos y si necesita esqueleto y clips.
2. **Las imágenes las pone el usuario** (no se generan acá: no hay GPU y este entorno no llega a
   huggingface.co). Pautas para la foto o el dibujo:
   - el objeto entero, aislado, sobre fondo liso, en tres cuartos, de 1024 px o más;
   - mejor fotos de museo (Ermitage, Pazyryk, Museo Británico) o dibujos de concepto; las de
     estilo animal (placas, arreos, amuletos) de frente, como se ven.
3. **Generar el 3D** en el espacio de TRELLIS.2 en Hugging Face (desde la tablet: subir la imagen,
   generar, exportar el GLB) y copiar el enlace de descarga del GLB.
4. **Traerlo** (en Codespaces con Cline, o acá con el archivo):
   ```bash
   python3 tools/modelos/modelos.py traer <id> --url <enlace del GLB> --fuente trellis2 --girar 90
   python3 tools/modelos/modelos.py traer <id> --archivo ruta/al.glb --fuente trellis2 --girar 90
   ```
   Lo que hace: guarda la fuente en `arte/modelos/fuentes/` (fuera de git), la baja al estilo del
   juego (`lowres.py`: diezma a `tris_max`, colores planos por cara desde la textura con una paleta
   corta, la luz del sol horneada como el terreno, a las medidas del inventario, origen en el
   centro de la base), la escribe en la ruta de su id, la marca `importado`, anota la procedencia
   en `arte/modelos/models_manifest.json` (url, sha256, opciones) y corre los chequeos.
   - `--girar G`: el frente tiene que quedar hacia **+X**. Si la imagen era de frente, suele ser 90.
   - `--rotar z:90`: giros previos (una espada que salió acostada y va de pie).
   - `--origen x,y,z`: el agarre de un arma o el anclaje de una pieza (m, del modelo ya escalado).
   - `--reemplazar`: pisa un modelo que ya estaba.
   - `bajar <id> <fuente.glb>`: solo la conversión, en `build/modelos/`, para probar opciones.
   Cline conoce el mismo paso como `python3 tools/fetch_pipeline_asset.py --url ... --name <id o nombre> --category ...`.
5. **Mirarlo en el juego**, a escala, con el jugador al lado:
   ```bash
   cmake --build build && cmake -E copy_directory assets build/assets
   cd build && xvfb-run -a ./estepa --ver <id> --frames 40 --screenshot ver.png
   ```
   La cámara lo mira de tres cuartos desde su frente (+X). Si el frente no da a la cámara, repetir
   con otro `--girar`. Mostrarle la captura al usuario.
6. **Commit** del GLB, `assets/inventario.tsv`, `docs/INVENTARIO.md` y el manifiesto, con los chequeos:
   ```bash
   python3 tools/modelos/modelos.py check
   python3 tools/assets/inventario.py check && python3 tools/assets/inventario.py doc --verificar
   python3 tools/assets/animaciones.py check
   ```

## Personas, animales y mecanismos

Los que llevan esqueleto (`rig:humanoide`, `rig:cuadrupedo`, `rig:ave`) o clips de mecanismo
(`docs/ANIMACIONES.md`) no entran al juego sin sus clips obligatorios (el CI falla). `traer` deja su
versión low-poly en `arte/modelos/para_riggear/<id>.glb`:
1. riggearla (RigNet en Hugging Face, o Blender) con el esqueleto compartido de su tipo;
2. animarla con los clips de `docs/ANIMACIONES.md` (skill `animaciones`);
3. traer el GLB riggeado: `traer <id> --archivo riggeado.glb --fuente rignet --riggeado` (no se
   convierte: se valida y se copia; tiene que venir ya en metros, +Y arriba, frente +X, origen en los pies).

## Dependencias

`pip install -r tools/modelos/requirements.txt` (numpy, scipy, trimesh, fast-simplification,
pillow). El chequeo (`modelos.py check`) no las necesita. Pruebas: `python3 tools/modelos/test_modelos.py`
(también en el CI).
