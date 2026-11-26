# Modelos del juego

Cada objeto de [`assets/inventario.tsv`](../inventario.tsv) tiene una ruta fija, derivada de su id:

| Id | Archivo |
|---|---|
| `estructura.vivienda.yurta_comun` | `assets/models/estructura/vivienda/yurta_comun.glb` |
| `accesorio.tatuaje.lobo` (etiqueta `formato:textura`) | `assets/textures/accesorio/tatuaje/lobo.png` |

Mientras el archivo no existe, el juego dibuja un **marcador**: una caja del color de la categoría, con las medidas del inventario y una línea roja hacia el frente. Al dejar el modelo en su ruta, el juego lo usa solo; no hace falta tocar código.

Lista completa y avance: [docs/INVENTARIO.md](../../docs/INVENTARIO.md).

## Desde una imagen (TRELLIS.2) o un GLB cualquiera: la tubería

`tools/modelos/modelos.py` hace todo lo de abajo de una vez (skill [`modelos`](../../.claude/skills/modelos/SKILL.md)):

```bash
python3 tools/modelos/modelos.py traer <id> --url <enlace del GLB> --fuente trellis2 --girar 90
python3 tools/modelos/modelos.py traer <id> --archivo modelo.glb --girar 90
./build/estepa --ver <id> --screenshot ver.png   # desde build/: el modelo de cerca, con el jugador al lado
```

Baja la fuente (queda en `arte/modelos/fuentes/`, fuera de git), la lleva al estilo del juego
(`tris_max`, colores planos por cara desde la textura con una paleta corta, la luz del sol horneada,
las medidas del inventario, frente +X, origen en el centro de la base), la deja en la ruta de su id
como `importado` y anota de dónde salió en `arte/modelos/models_manifest.json`. Las personas, los
animales y los mecanismos esperan en `arte/modelos/para_riggear/` hasta tener esqueleto y clips; el cuerpo
humano (`tools/modelos/humanoide.py`, con clips de la Universal Animation Library de Quaternius con `--ual`, y el cuerpo
de mujer desde su maniquí femenino), los animales de `arte/fuentes/animales/modelos` (`tools/modelos/animales.py`), los
cuadrúpedos de `para_riggear` (`animales.py --para-riggear`: el coyote, el jabalí, el lobo 2, el oso polar, el perro
salvaje y el tigre, rehechos en vóxeles) y los 17 de *Better animals* (`tools/modelos/rigs.py`: los caballos, los
burros, los camellos, el elefante, el buey, la vaca, el perro pastor, el lobo, el oso, la hiena, el zorro y la gacela,
con el esqueleto y los clips del rig de referencia del caballo o de la hiena) ya los tienen; la malla de pocas caras de
los de `para_riggear` queda ahí, como fuente. Las monturas llevan además el hueso `asiento` (sin pesos): ahí pone el juego la silla de montar
y la pelvis del jinete (`tools/modelos/jinete.py`, desde la referencia del jinete).
`python3 tools/modelos/modelos.py check` (también en el CI) revisa triángulos, medidas, base y texturas.

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

## El equipo de los humanoides: el patrón

El equipo de Props (cascos, corazas, botas, armas, escudos, la aljaba y la antorcha) sale de
`tools/modelos/patron.py`, guiado por el patrón que armó el usuario a mano
(`arte/fuentes/patron/model_equipped_01.glb`: el cuerpo de `rigged-model.glb` en su pose T con las piezas
puestas). Cada pieza del patrón es una malla de `arte/fuentes/props/modelos` con una transformación afín exacta
(0 mm): de ahí salen la escala, el lugar y la holgura a la piel (de 4 a 7 mm como mínimo; menos del 1 % de sus
vértices adentro). El usuario colocó a mano seis cascos más sobre el mismo cuerpo (`Model equipped 02` a `07`,
en `IMPORTED/References 3d` de la rama `cline/modelos-animales`); `arte/fuentes/patron/cascos_colocados.glb`
guarda solo esos cascos, llevados exactamente al marco del patrón (`patron.py referencias <carpeta>` lo rehace
desde las originales). Las piezas que no están colocadas se ponen con su mismo criterio: los cascos, con el
patrón de los colocados (la fila «Cascos»); las corazas, con el borde de arriba donde el del chaleco y lo más
justas que no se metan en el torso.

| Qué | Cómo va |
|---|---|
| Corazas y botas | Con piel: los 56 huesos del cuerpo (su pose de reposo) y los pesos de la piel de abajo, sin clips. Un archivo por cuerpo: `nombre.glb` (el común), `nombre@mujer.glb`, `nombre@gigante.glb` y `nombre@protagonista.glb`. El juego las mueve con el clip y el cuadro del cuerpo (`props_draw_worn`). A 5 mm de la piel como mínimo: lo que se mete sale por la normal de la piel, y en las botas, la piel que asoma por una cara grande (la punta del pie por la puntera) empuja esa cara (en la coraza no: el brazo, en la sisa, le sacaba aletas). Reducidas de dos maneras (sin lo de adentro antes o después de reducir), saneadas (sin aletas ni caras al revés) y con los agujeros y las rajas tapados, sin tocar la boca de la bota, el cuello, las sisas ni la cintura; queda la que menos se aparta de la de Props (p95 y desvío máximo: sin púas). |
| Cascos, armas, escudos, aljaba, antorcha | Rígidos, en su marco: las armas con la empuñadura en el origen, la hoja hacia +Y y el filo hacia +Z; los escudos mirando a +X; los cascos con el origen en el centro del cráneo del común (a la altura de las cejas, entre la frente y la nuca y de oreja a oreja), de frente a +X. Van en los anclajes de `src/world/anclajes.inc` (generado: el hueso y la matriz de cada anclaje en cada cuerpo), que siguen a su hueso (`props_draw_socket`), sin corrimientos sueltos en el código. |
| Cascos | Siete los colocó el usuario a mano sobre el cuerpo del patrón: el del padre, en el patrón, y seis más, en `cascos_colocados.glb`. Cada uno va donde lo puso, con su escala por eje y su giro. Solo se agrandó lo justo, de 0 a 3 %, donde la cabeza del juego (de menos resolución que la de las referencias: hasta 2 mm más afuera) asomaba por él. Lo que hizo el usuario, medido en los siete: no los giró más que para que miren adelante (solo inclinó dos, 5°); escaló el alto y el ancho por igual y estiró el fondo de 0 a 25 % (la cabeza es un 28 % más honda que ancha y los cascos reconstruidos salen redondos: así el hueco de la banda de la frente queda parejo, adelante más atrás igual a los dos costados); los apoyó en la cabeza (la piel entra de 1 a 4,5 mm en su grosor y no asoma por afuera, con 2 a 18 mm de hueco en la banda y el techo de 40 a 85 mm sobre la coronilla; la máscara, a 16); el borde de adelante, de 4 a 8 mm sobre las cejas (el del padre, 16). El que no está colocado (el del culto) y los que vengan siguen esa regla (`calzar_casco`): de frente por su simetría, el fondo estirado hasta el hueco parejo, el borde 6 mm sobre las cejas y la escala menor con la que se apoya (la piel entra hasta 3 mm) sin que la cabeza asome. Probada contra los colocados, la regla queda a 6–8 mm de lo que hizo el usuario en los cascos abiertos de metal (escamas, malla, bronce); los especiales piden su mano: el del padre (más alto), la máscara (pegada a la cara), el laminar (inclinado) y el gorro de piel (sin abertura de la cara y con forro). Reducido, sin el forro ni lo que queda más de 6 mm adentro de la cabeza, se verifica que no asome. En cada cuerpo, `GEAR_HEAD` lleva el origen a su centro del cráneo con su cabeza por eje (fondo, alto y ancho contra los del común: la de la mujer es un 6 % más angosta y un 6 % más alta) y cada casco va a su escala en esa cabeza (`GEAR_HELMET_FIT`): la menor con el mismo apoyo que en el común, sin que asome. `python3 tools/modelos/patron.py cascos` lo verifica en los 4 cuerpos (contacto, piel que asoma, techo y borde) y `--ver armadura.casco.<nombre> --desde frente` lo muestra puesto. Triángulos: cada casco, el mínimo con el que la reducción no deforma la forma ni los detalles (la distancia de ida y vuelta a la malla de Props, p99, de 1,1 mm o menos, y el desvío medio de la normal, de 8,5° o menos: ahí quedan el águila, los remaches, el rollo de piel y los grabados, mirados de cerca): bronce 1.400, escamas y máscara 1.700, malla 2.000, laminar 2.100, padre 2.400, gorro 2.900 y culto 3.400. |
| Guardado | El casco en la cabeza; el escudo, la aljaba y lo largo (el arco, la lanza) a la espalda; lo de la mano derecha enfundado al costado izquierdo (el sable del patrón) y lo de la izquierda (la daga) al derecho. |
| Empuñado | En la mano derecha (la izquierda: el arco, la antorcha) y el escudo embrazado en el antebrazo izquierdo; lo guardado deja de dibujarse. |
| Medidas | Las armas y los objetos, las del inventario (del patrón, el anclaje, el eje y el calce); lo que se viste, la de su caja puesta en el cuerpo común (el inventario se actualiza solo). |
| Polígonos y normales | `tris_max` del inventario, reducido con cuádricas sin centrar ni escalar a ciegas. Las corazas y las botas se reducen de dos maneras, abiertas (sin lo que mira al cuerpo) y cerradas (con su cara de adentro, que se quita después), y queda la que más se parece a la de Props: abierta, a veces se plegaba y dejaba rendijas por donde se veía la piel; cerrada, a veces pierde un lado. Lo plano y fino (la lanza, la alabarda, el arco con su cuerda), con la silueta de su malla extruida a su grosor, porque las cuádricas lo aplastaban. Normales suaves entre caras de menos de 40° y cortadas en las aristas más duras (también las de los cuerpos y los animales: `patron.py normales`). |
| Los cuerpos | `rigged-model.glb` es una cáscara de unos 4 mm, con la capa de adentro mirando al hueco del cuerpo. `humanoide.py` se queda con la de afuera antes de reducir (`lowres.capa_de_afuera`) y tapa los agujeritos que deja la reducción: todo el `tris_max` va a la piel que se ve (antes, la mitad era la capa de adentro) y la holgura de las prendas se mide contra esa piel. |
| Textura | El render de Props (`arte/fuentes/props/imagenes`) proyectado de frente (≤ 256 px, filtro de punto), sin la luz grande del render: la luz la pone el juego. Lo claro del borde de la silueta (el fondo blanco del render pegado al pelo o al brillo del canto) y lo de afuera toman el color de lo de adentro, alisado; las caras que la proyección ve de canto (los costados), un solo color: el de sus vecinas en la malla que sí se ven de frente (el que cubre más superficie entre ellas). Lo fino no gana nunca: ni el brillo del canto ni las puntas claras del pelo, ni el borde oscuro de la silueta, ni una correa. Así no quedan manchas claras, barridos ni triángulos negros. |

`python3 tools/modelos/patron.py ver` dibuja cada cuerpo con el equipo del patrón, guardado y empuñado
(`build/patron/`); en el juego, `./estepa --ver personaje.base.cuerpo_comun --patron guardado|empunado
[--patron-atras]`.

La luz de los cuerpos, los animales y el equipo la pone un shader (`src/world/model_light.c`, GLSL 330 y ES 100):
la del cielo y la del suelo (hemisférica) y la del sol (direccional), con la pose de cada cuadro. Sin el shader
(OpenGL 1.1 o 2.1), se ven como antes, con la luz horneada en el color.

## Variantes por estación

Un modelo puede traer variantes estacionales junto al base, con el sufijo `@estacion`:

| Archivo | Cuándo se usa |
|---|---|
| `yurta_comun.glb` | Siempre que no haya variante para la estación |
| `yurta_comun@invierno.glb` | En invierno (p. ej. techo nevado) |
| `arbusto@otono.glb` | En otoño (hojas ocres) |

Estaciones válidas: `primavera`, `verano`, `otono`, `invierno` (sin tilde en el nombre del archivo). Las variantes no van en el inventario; el validador comprueba que el modelo base exista. El suelo, el agua, la nieve y los glaciares ya cambian solos con la estación.

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
