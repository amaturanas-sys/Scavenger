# Plan: herramientas de generación (edificios, animaciones y modelos) y ejecución del plan grande

> **Estado (18 de octubre de 2026):** en marcha.
> - **Parte A, el plan grande:** hecho (fases 0 a 8b, de la v0.4.4 a la v0.9.1).
> - **Paso 2, las tuberías (D.1 y C.2):** hecho en la **v0.9.3** (`tools/modelos`, las skills `modelos` y
>   `animaciones`, `--ver <id>`), sobre lo que armó Cline (abajo).
> - **Parte B, los edificios:** hecha. La investigación ([ARQUITECTURA_EDAD_DEL_HIERRO.md](ARQUITECTURA_EDAD_DEL_HIERRO.md))
>   y el kit (`src/sim/buildkit.c`), en la **v0.10.0**: las ciudades, las aldeas y las tribus se arman
>   con él; `--edificios` lo muestra. Lo que faltaba (el paso 5b), en la **v0.10.2**: las murallas con
>   sus torres y sus puertas acodadas, las ruinas y las obras de la tribu.
> - **Lo que sigue** espera al usuario (abajo): no queda nada del plan que se haga solo aquí.
> - **Del lado del usuario:** las imágenes de la D.2 (empezando por el estilo animal) y, con los cuerpos
>   riggeados, los videos de la C.1.
>
> Las tres herramientas necesitan una **GPU NVIDIA** que esta instancia de Claude Code no tiene (y su
> red no llega a huggingface.co). Por eso:
> - **la generación pesada** corre del lado del usuario: los espacios de Hugging Face desde la tablet
>   (TRELLIS.2, RigNet), o Codespaces con Cline, que sí llega a Hugging Face;
> - **la conversión a low-res, la integración y la revisión**, con los scripts del repo y el CI.

Pedido:
1. Ejecutar el plan de mejoras.
   - Se entiende `docs/PLAN_GRAN_ACTUALIZACION.md`: `PLAN_MEJORAS.md` ya se completó en la v0.2.0.
2. Integrar **BuildingGeneratorThreeJS** para edificios y estructuras más complejas y fieles a la cultura material de la temprana edad del hierro.
3. Integrar **GRAIL** para animar a las personas a partir de videos de referencia.
4. Integrar **TRELLIS.2** para generar, desde imágenes, los modelos que faltan, en versión low-res.

---

## Lo que dicen los tres repositorios (revisado)

| Repositorio | Qué es | Licencia | Requisitos | Lo útil para el juego |
|---|---|---|---|---|
| [BuildingGeneratorThreeJS](https://github.com/achrefelouafi/BuildingGeneratorThreeJS) | Generador procedural de edificios de **Hong Kong** para Three.js (TypeScript), portado de un grafo de nodos de Blender. Usa un *kit* de ~190 piezas instanciadas (`kit.glb` + `kit_manifest.json`) y una colocación por rejilla con semilla. | MIT | Node (`npm run dev`); Blender 4.2 para reexportar el kit | **El método, no el contenido:** un kit de piezas más reglas de colocación con semilla. Las piezas y las reglas hay que rehacerlas para la estepa y los reinos agrarios. El juego es C + raylib, así que se porta el algoritmo y no se ejecuta Three.js. |
| [GRAIL](https://github.com/NVlabs/GRAIL) | Tubería de datos para **robots humanoides**: genera videos con un modelo de video (Kling o MiniMax), reconstruye el movimiento 4D humano-objeto (GEM-SMPL) y lo lleva a un robot Unitree G1. | **NVIDIA License** (uso no comercial, investigación) | Docker con GPU, checkpoints, claves de API de video (Kling: de pago) | **Solo la etapa de video → movimiento humano 4D (SMPL)**, para sacar animaciones de videos de referencia. **Ojo con la licencia:** si el juego se vende, el uso de GRAIL y de lo que produce puede no estar permitido. Hay alternativas abiertas para la misma etapa (abajo). |
| [TRELLIS.2](https://github.com/microsoft/TRELLIS.2) | Imagen → modelo 3D texturizado (4B parámetros), con materiales PBR. | MIT | Linux, GPU NVIDIA de **24 GB** o más (A100/H100), CUDA 12.4; también hay **demo en Hugging Face** | Generar los ~400 modelos del inventario que siguen "pendiente" (398 de 400), desde imágenes, y bajarlos a low-res. |

---

## Parte A — Ejecutar el plan grande

Se sigue `docs/PLAN_GRAN_ACTUALIZACION.md` tal cual, por fases (de la 0 a la 8, más la 4b), cada una con su PR, CI y versión. Las partes B, C y D lo alimentan:
- **los edificios de la parte B** van a las fases 4b (obras en el campamento) y 8 (asentamientos agrarios);
- **las animaciones de la parte C**, a las fases 1, 2 y 6 (combate, espalda y rehenes, caballos);
- **los modelos de la parte D**, a todas.

**Orden recomendado (fase del plan grande y parte de este plan):**
1. **Fase 0, partidas por bloques:** es corta, va sola.
2. **Parte D.1, la tubería de TRELLIS:** mientras el usuario junta imágenes.
3. **Fases 1 y 2:** se juegan con las animaciones procedurales de hoy.
4. **Parte C, animaciones:** reemplaza las procedurales del combate cuando estén.
5. **Fases 3, 4 y 4b.**
6. **Parte B, el kit de edificios:** antes de la fase 8.
7. **Fases 5, 6, 7 y 8.**

---

## Parte B — Edificios fieles a la temprana edad del hierro (método del kit de BuildingGenerator)

### B.1 — Investigación (cultura material y arquitectura)

Un documento `docs/ARQUITECTURA_EDAD_DEL_HIERRO.md` con fuentes, medidas y materiales.

**Nómadas de la estepa:**
- **Yurta:** reja plegable, varas, corona y fieltro.
- **Tienda de pieles.**
- **Carretas-casa:** carros cubiertos de fieltro de los escitas, según Heródoto.
- **Kurganes y túmulos:** Pazyryk, Arzhan, Filippovka. Cámaras de troncos bajo un túmulo de piedra y tierra.
- **Estelas de ciervo:** las piedras de los ciervos.
- **Corrales.**

**Agricultores y reinos del hierro temprano:**
- **Casas de adobe y de piedra** sobre zócalo, con techo plano de barro y vigas; patios.
- **Ciudadelas** con muralla de adobe sobre zócalo de piedra, torres rectangulares salientes y puerta acodada. Modelos:
  - Sogdiana, Bactria;
  - Urartu (Teishebaini);
  - Gordion;
  - los *oppida* y fuertes de colina de Hallstatt y La Tène.
- **Casas largas** de postes y zarzo con barro, y techo de paja (Europa).
- **Graneros elevados** sobre postes.
- **Talleres:** hornos de fundición de tiro natural (*bloomery*), forjas y alfares.
- **Palacio o edificio administrativo:** sala hipóstila de columnas de madera, como en Urartu o en la Persia aqueménida temprana.
- **Templos de fuego.**

Cada tipo lleva un dibujo esquemático (planta y alzado), sus medidas típicas, sus materiales y la paleta de colores. De ahí salen las piezas del kit.

### B.2 — El kit de piezas (en C, sin Three.js)

> **Hecho en la v0.10.0** como `src/sim/buildkit.c` (en `src/sim/`, con tests, y no en `src/world/`):
> ocho tipos por región y cuatro variantes de cada uno, vistos con `--edificios tipo[:region]` (en vez
> de `--galeria-edificios`). Lo de adentro se rellena (la malla, con la mitad de caras) y, de lejos,
> se dibuja sin refinar. Lo que faltaba, en la **v0.10.2**: las murallas con sus torres y sus puertas
> acodadas, las ruinas (la fortaleza derruida y la ciudad enterrada) y las obras del campamento con el
> kit. El detalle, en [ARQUITECTURA_EDAD_DEL_HIERRO.md](ARQUITECTURA_EDAD_DEL_HIERRO.md).

- **`src/world/buildkit.c`:** un **kit de piezas** de vóxeles o de malla low-poly. Cada pieza es una unidad de rejilla de 1 m:
  - muro de adobe, de piedra o de zarzo;
  - esquina, puerta o ventana;
  - poste, viga o alero;
  - techo plano, a dos aguas de paja o de turba;
  - almena, torre y escalera;
  - fieltro y reja de yurta;
  - carreta.
- **Reglas de colocación con semilla:** el algoritmo de `src/generator.ts` portado a C. Se generan:
  - la planta (rectángulo, L o patio);
  - las alturas (1–2 pisos, torres);
  - las aberturas, siguiendo reglas;
  - los detalles al azar con probabilidad (vigas que asoman, pieles al sol, cántaros, leña).
- **Integración:**
  - los edificios de `voxstruct` (las casas por región y las ruinas) pasan a construirse con el kit;
  - las ciudades de la fase 8 crecen desde el palacio con calles, casas, campos y talleres;
  - las obras del campamento (fase 4b) usan las mismas piezas.
- **Herramienta de vista previa:** `--galeria-edificios` muestra todos los tipos con varias semillas para revisarlos, y saca capturas para `docs/`.
- **Tests:**
  - las piezas encajan sin huecos;
  - la puerta da a la calle;
  - el número de triángulos queda bajo el presupuesto por edificio.

**Costo:** alto (investigación + kit + reglas). Sin GPU: se hace entero aquí.

---

## Parte C — Animaciones desde videos de referencia

### C.1 — Qué videos tiene que grabar o conseguir el usuario

Cada clip de `docs/ANIMACIONES.md` (los cuerpos humanoides usan 62 clips) necesita un video. Pautas para que la reconstrucción funcione:
- **una persona**, de cuerpo entero siempre a la vista;
- **cámara fija**, de lado o en tres cuartos, a la altura de la cadera;
- **fondo liso**, buena luz y ropa ajustada (nada de ropa suelta que tape las articulaciones);
- **30 fps o más**, de 3 a 10 s por clip, empezando y terminando en posición neutra;
- los ciclos (caminar, correr) con 3 o 4 pasos completos.

**Lista para grabar** (se guardan en `arte/videos/<clip>.mp4`):

| Grupo | Clips |
|---|---|
| Moverse | caminar, correr, acechar (sigilo), acechar quieto, **agacharse** y caminar agachado, saltar, caer, aterrizar, **rodar**, nadar |
| Escalar | trepar por cuerda, coronar un muro, escalar roca |
| Armas a una mano | sable: 3 golpes en combo y un golpe pesado; hacha o sagaris; maza; daga: estocada y **apuñalar por la espalda** |
| Armas a dos manos y de asta | guja (golpes), lanza (estocada y golpe de asta), lanza larga (a caballo, se simula sobre un taburete) |
| Distancia | tensar y soltar el arco, ballesta (disparar y recargar), honda (voltear y soltar), lanzar (venablo, piedra) |
| Escudo | bloquear, golpe de escudo, carga con escudo, enganchar el escudo enemigo |
| MMA (llaves y palancas) | derribo a dos piernas, proyección de cadera, barrido, llave de brazo de pie, estrangulación por la espalda (**rehén**), agarre del cuello caminando (**escudo humano**) |
| Patadas | patada frontal, patada a la carrera |
| Recibir | recibir golpe (frente, espalda y lado), tambalearse, ser derribado, **abatido** (en el suelo, respirando), levantarse, morir |
| Vida diaria | tomar del suelo, cargar, construir, cavar, encender fuego, cocinar, curar o vendar, ensillar, montar y bajar del caballo, hablar, celebrar, protestar |

> **Consejo:** muchos ya están en bancos de movimiento abiertos (CMU, AMASS, Mixamo). Antes de grabar, conviene ver qué clips se pueden sacar de ahí: menos trabajo y licencias más claras.

### C.2 — La tubería (en una máquina con GPU)

1. **Video → movimiento SMPL**, con una de estas opciones:
   - **GRAIL**, etapa `recon_4dhoi`, con GEM-SMPL. Solo uso no comercial; además exige Docker, GPU y checkpoints.
   - **Una alternativa abierta** de la misma etapa (GVHMR, WHAM o similar, con su licencia revisada). Se recomienda si el juego puede venderse.
2. **SMPL → esqueleto del juego:** retarget con un script de Blender, `tools/anim/retarget.py`. Hace tres cosas:
   - corrige el pie que patina;
   - recorta y deja los ciclos en bucle;
   - baja a 30 fps.
3. **Exportar:** un GLB de animaciones por cuerpo, con los nombres de `docs/ANIMACIONES.md` (`idle`, `caminar`, `ataque_una_1`...). Lo carga `props_draw_item_anim`.
4. **Integración aquí:**
   - `tools/anim/check.py` en el CI: comprueba que están todos los clips obligatorios, la duración, los bucles y los huesos;
   - capturas por clip en `docs/`.

**La skill `.claude/skills/animaciones/SKILL.md`** documenta los pasos, para que cualquier sesión sepa:
- qué pedir al usuario;
- cómo recibir los videos y dónde guardarlos;
- cómo correr la tubería en la máquina con GPU;
- cómo integrar el resultado.

**Costo:** medio en código, alto en el material. La calidad depende de los videos.

---

## Parte D — Modelos desde imágenes con TRELLIS.2, en low-res

### D.1 — La tubería (hecha en la v0.9.3: `tools/modelos`)

1. **Imágenes:** el usuario hace 1 a 4 imágenes por objeto. Pautas:
   - el objeto entero, aislado, sobre fondo liso, en tres cuartos, de 1024 px o más;
   - mejor fotos de museo (Ermitage, Pazyryk, Museo Británico) o dibujos de concepto.
2. **Generación**, en el espacio de TRELLIS.2 en Hugging Face (desde la tablet) o en una GPU de 24 GB:
   un GLB de alta resolución con textura.
3. **Traer y bajar a low-res:** `python3 tools/modelos/modelos.py traer <id> --url <GLB> --girar 90`
   (o `--archivo`; Cline lo llama con `tools/fetch_pipeline_asset.py`). Sin Blender (`lowres.py`, con
   numpy, trimesh y fast-simplification):
   - **Reducción:** diezma hasta `tris_max` del inventario, sin respetar las costuras de la textura.
   - **Medidas:** escala por la medida más larga del inventario, gira el frente a +X (`--girar`,
     `--rotar`) y deja el origen en el centro de la base (o en el agarre, `--origen`).
   - **Color:** cada cara toma el color que más cubre de la textura debajo, de una paleta corta
     (24), con la luz del sol del terreno horneada: un facetado sin texturas, que raylib dibuja de
     una vez.
   - **La fuente** queda en `arte/modelos/fuentes/` (fuera de git) y su procedencia (url, sha256,
     opciones) en `arte/modelos/models_manifest.json`.
   - **Personas, animales y mecanismos:** su versión low-poly espera en `arte/modelos/para_riggear/`;
     el esqueleto lo pone **RigNet** (Hugging Face) y los clips la parte C; el GLB riggeado entra con
     `--riggeado`.
4. **Inventario:** el estado pasa de `pendiente` a `importado` en `assets/inventario.tsv` y en
   `docs/INVENTARIO.md`.
5. **Revisión y CI:** `./estepa --ver <id>` lo muestra de cerca con el jugador al lado;
   `tools/modelos/modelos.py check` revisa triángulos, medidas, base y texturas, y
   `tools/modelos/test_modelos.py` prueba la tubería con una fuente sintética (sin GPU ni red).

### D.2 — Qué imágenes pedir, por prioridad

Hay 398 objetos pendientes. No hace falta hacerlos todos: se empieza por lo que más se ve.

1. **Personas:** 29 PNJ (`personaje.npc`) y los cuerpos base. Junto con la parte C.
2. **Animales:** 19 salvajes y 8 monturas. Caballo, lobo, ciervo, liebre, águila y cuervo, primero.
3. **Armas y armaduras:**
   - las armas icónicas de la fase 7: arco compuesto, sable, sagaris, maza, daga de anillo, lanzas;
   - los cascos.
4. **Estructuras del campamento** (14) **y viviendas** (10): solo las que no salgan mejor con el kit de la parte B.
5. **Objetos y materiales:** el resto, a medida que se necesiten.

La skill `.claude/skills/modelos/SKILL.md` documenta cómo pedir las imágenes, generar, bajar a low-res e integrar.

**Costo:** bajo en código, medio en tiempo de GPU (≈20 s a 1 min por objeto en una H100). El cuello de botella son las imágenes.

---

## Lo que se necesita del usuario

1. **Una máquina con GPU NVIDIA** (24 GB para TRELLIS.2), o acceso a una nube de GPU, o usar la demo de Hugging Face (lenta, para pocos objetos). Opcional: conectar esa máquina a Claude Code como entorno propio, para que la sesión corra los scripts allí.
2. **La decisión sobre la licencia de GRAIL** (no comercial) frente a una alternativa abierta. Y, si se usa GRAIL con video generado, la clave de Kling (de pago).
3. **Los videos de la parte C.1**, empezando por el combate (fases 1 y 2).
4. **Las imágenes de la parte D.2**, empezando por personas, caballo y armas.

## Lo que armó Cline y cómo quedó

El usuario trabajó un tiempo con Cline en Codespaces (con los espacios de Hugging Face desde la tablet):

| De Cline | Cómo quedó |
|---|---|
| `tools/fetch_pipeline_asset.py` y las reglas de `.clinerules` (TRELLIS 2 + GRAIL + RigNet): bajar el GLB de un enlace y anotarlo en un manifiesto | **Integrado (v0.9.3).** El mismo comando pasa por `tools/modelos/modelos.py traer`: el modelo va a la ruta de su id (en `assets/models/<nombre>.glb` el CI fallaba), se baja a low-res, el inventario queda al día y el manifiesto (`arte/modelos/models_manifest.json`) guarda la procedencia. |
| **RigNet** para el esqueleto | **Adoptado:** reemplaza al esqueleto que la parte D ponía a mano en Blender. Las personas y los animales esperan en `arte/modelos/para_riggear/`. |
| La vegetación (PR #115) | **Reescrita (v0.9.2):** se caía en el primer cuadro; ahora con reglas puras y tests. |
| El campamento desde JSON (cJSON) y `anim_system` | **No se adoptó:** rompía la compilación y el dibujo del campamento. El campamento sale de la semilla del mundo, y el motor ya carga los clips de cada GLB por nombre (`src/world/props.c`). |

## Orden de trabajo y versiones

| Paso | Qué | Dónde | Versión | Estado |
|---|---|---|---|---|
| 1 | Fase 0 del plan grande | aquí | v0.4.4 | hecho |
| 2 | Tuberías y skills de modelos y animaciones (D.1, C.2) | aquí | v0.9.3 | hecho; la prueba con un objeto real espera la primera imagen |
| 3 | Fases 1 y 2 (combate) | aquí | v0.5.x | hecho |
| 4 | Primeras animaciones de combate y personas (C) | GPU → aquí | — | espera los cuerpos riggeados (D.2) y los videos (C.1) |
| 5 | Investigación y kit de edificios (B) | aquí | v0.10.0 | hecho: viviendas, palacios, talleres, graneros, templos y carretas en las ciudades, las aldeas y las tribus |
| 5b | Lo que falta de la B: murallas y puertas acodadas, ruinas y obras del campamento con el kit | aquí | v0.10.2 | hecho |
| 6 | Fases 3, 4, 4b | aquí | v0.6.x | hecho |
| 7 | Modelos por prioridad (D.2), en tandas | GPU → aquí | cada versión | espera las imágenes |
| 8 | Fases 5 a 8 | aquí | v0.7–v0.9.1 | hecho |

**La primera tanda de modelos (D.2)**, la que pidió la narrativa: las piezas de estilo animal, que
además no llevan esqueleto y entran directo al juego:
- `accesorio.arreo.placa_arreo_padre` (la placa de arreo del padre, de estilo animal);
- los amuletos de oro `accesorio.amuleto.*` (lobo, ciervo, águila e íbice, tigre y dragón, oso, caballo:
  el combate animal);
- `arma.corta.akinakes` (la espada corta).

Después, los cuerpos base (`personaje.base.*`) y el caballo, que abren la parte C.
