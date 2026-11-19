---
name: modelos
description: La tubería de modelos 3D de SCAVENGERS THRIVE (paso 0, la referencia con Perchance → TRELLIS.2 o piezas.py → low-poly → juego; RigNet para el esqueleto). Usar cuando el usuario quiera agregar o reemplazar un modelo, pase una URL de Hugging Face o un GLB, pregunte qué imágenes hacen falta o cómo pedirlas, o al revisar un modelo importado.
---

# La tubería de modelos

Lo que el juego necesita de un modelo está en `assets/inventario.tsv` (medidas, `tris_max`, `rig`) y en
`assets/models/README.md` (ejes, origen, estilo). La tubería lo deja así sola: `tools/modelos/`.

## Pasos

1. **Elegir el id.** `python3 tools/modelos/modelos.py estado` lista lo que falta por prioridad
   (personas, animales, armas y armaduras, estructuras, el resto). `estado <id>` dice sus medidas,
   su presupuesto de triángulos y si necesita esqueleto y clips.
2. **Paso 0, la imagen de referencia: Perchance, por defecto.**
   - **Dónde:** gratis y sin cuenta, en el navegador, por el usuario o por Cline en el Codespace:
     - [ai-text-to-image-generator](https://perchance.org/ai-text-to-image-generator), para objetos, animales y
       estructuras;
     - [ai-character-generator](https://perchance.org/ai-character-generator), para personas.

     Claude, en la nube, no llega a perchance.org.
   - **Qué pedir:** hay dos clases de imagen, según lo que venga después. Los prompts rinden más en inglés:
     - **La hoja de tres vistas, para `piezas.py` y los voxeles.** Una sola imagen ancha con el mismo sujeto de
       frente, de lado y de espalda, a la misma escala. Base:
       `turnaround reference sheet of a single <sujeto>, three views side by side: front view, side view facing
       left, back view, same scale, orthographic, full body, neutral pose, plain light gray background, no
       shadow, no text`. Si el lado mira a la derecha, `piezas.py --voltear-lado`.
     - **Una sola vista, para TRELLIS.2.** Nunca la hoja: TRELLIS hace un modelo por cada vista, y así el PR #164
       trajo tres lobos en una malla. Base: `a single <sujeto>, three-quarter front view, full body, isolated,
       centered, plain light gray background, no other objects, no text`.
   - **Pautas para las dos:**
     - el sujeto entero, sin recortes, sobre fondo liso, de 1024 px o más, y el estilo de la época
       (`docs/ESTILO_VISUAL.md`);
     - para objetos históricos siguen valiendo las fotos de museo (Ermitage, Pazyryk, Museo Británico); las de
       estilo animal (placas, arreos, amuletos), de frente;
     - nada que imite a gente real, marcas ni personajes con dueño.
   - **Guardarla en el repo, sin pasar por la tablet.** Con el enlace de la imagen o con el archivo subido al
     Codespace:
     `python3 tools/fetch_perchance_ref.py <id> --url <enlace> --vista hoja|unica --prompt "<el prompt>"`
     (o `--archivo`).
     - Queda en `arte/referencias/<id>/<vista>.<ext>`, en git, y conviene un JPEG de menos de 2 MB.
     - De dónde salió (el prompt, el enlace y el sha256) queda en `arte/referencias/referencias.json`.
     - Si el enlace es temporal o necesita la página abierta, el script lo dice: guardar la imagen y usar
       `--archivo`.
   - **La licencia:** según los resúmenes de sus términos, Perchance no reclama las imágenes ni limita su uso.
     Pero cada generador puede usar un modelo con su propia licencia: antes de publicar, leer
     perchance.org/terms-of-service y la página del generador. La imagen es solo la referencia; el juego lleva la
     conversión (`piezas.py`, voxeles o la malla de TRELLIS bajada a low-poly).
   - **Sin envoltorios no oficiales:** ni `eeemoon/perchance` ni `afrozthalam/perchance-api`. Manejan un
     navegador para pasar la verificación del sitio, y el segundo no tiene licencia.
3. **Generar el 3D** con la imagen de una sola vista, en el espacio de TRELLIS.2 en Hugging Face (desde la
   tablet: subir la imagen, generar, exportar el GLB), y copiar el enlace de descarga del GLB. Con una hoja de
   tres vistas, en cambio, el modelo sale con `piezas.py`, sin GPU.
   - **Con esqueleto, en AniGen** ([VAST-AI/AniGen](https://huggingface.co/spaces/VAST-AI/AniGen)): también pide
     una sola vista. `python3 tools/modelos/preparar_refs_anigen.py` las saca de las hojas de tres vistas de
     `arte/referencias/_raw_multiview/` y deja dos por id en `arte/referencias/<id>/`:
     - `unica_01`: la que pide el esqueleto (de frente las personas, de perfil a la izquierda los cuadrúpedos, de
       espaldas con las alas abiertas las aves);
     - `unica_02`: la que la complementa.

     Quedan sin fondo ni sombras, sobre blanco, negro o verde según el pelaje, y anotadas en
     `referencias.json` con su estado («lista» o «con reparos»). Necesita `pip install "rembg[cpu]"`;
     `--check` revisa lo hecho sin rembg.
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
