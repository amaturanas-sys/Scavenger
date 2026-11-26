# Las fuentes de los assets

Lo que subió el usuario y de lo que salen los assets del juego (o saldrán), con la herramienta que lo lee. Antes vivía
en `IMPORTED/`, en la rama `cline/procesar-imagenes-animales`; quedó acá solo lo que se usa o se va a usar, y la rama
se borró.

Nada de esto va en el juego: `assets/` es lo que se empaqueta (el APK y el zip de Windows), `arte/` no.

| Carpeta | Qué es | Licencia | La lee |
|---|---|---|---|
| `sonidos/` | Los 55 sonidos de origen del juego, en sus paquetes: *Free Fantasy 200 SFX Pack* de TomMusic (con su `ReadMe.txt`), *400 Sounds Pack* de Chequered Ink y *96 Extra SFX* | Los de cada paquete: `assets/sonidos/CREDITOS.md` | `tools/sonido/sonidos.py` (a `assets/sonidos`) |
| `animaciones/UAL1`, `animaciones/UAL2` | La *Universal Animation Library* 1 y 2 de Quaternius (las versiones para Unreal y Godot, sin root motion) y el maniquí femenino de la 2 (`Mannequin_F.glb`) | CC0 (`License.txt`) | `tools/modelos/humanoide.py --ual` |
| `humanoide/` | El esqueleto con sus 50 clips (`exported-model.glb`) y la malla riggeada con Mixamo (`rigged-model.glb`) de los cuerpos humanos; en `imagenes/`, las referencias del cuerpo | Del usuario | `tools/modelos/humanoide.py` |
| `animales/modelos/` | Las 20 mallas de Hunyuan3D de la ficha de los animales (`white_mesh(n).glb`, `hy3d-model(n).glb`, `01-glb.glb`); las nueve que reemplazó `animales/mejores` (n.º 3, 8, 11, 12, 15 y 17 a 20) siguen acá, pero `FICHA` ya no las arma | Del usuario | `tools/modelos/animales.py` (`FICHA`) |
| `animales/rigs/` | Los tres rigs hechos de *Better animals* (`IMPORTED/Better animals/Modelos`, en la rama `cline/modelos-animales`): `caballo.glb` (un burro con el esqueleto de caballo, 56 huesos, y sus clips: Idle, Walk, Trot, Run, Eating, Rear, Kick, Death...), `hiena.glb` (49 huesos; Idle, Sneak, Run, Bite, Howl, Fetch, Death...) y `aguila.glb` (el esqueleto de un ave y sus clips Flap, Glide, Idle y Walk, sin malla) | Del usuario | `tools/modelos/rigs.py` |
| `animales/mejores/` | Las 17 mallas de Hunyuan3D de *Better animals* que reemplazan a las del juego, una por id (`<nombre>.glb`: la mejor de las carpetas `24_11` y `25_11`; `rigs.MEJORES` dice cuál era, con qué rig va y de dónde sale su color) | Del usuario | `tools/modelos/rigs.py` |
| `animales/imagenes/` | Los renders de referencia de los animales (61): el color de las mallas de la ficha y los de los animales que aún no tienen modelo (el águila, el buitre, el halcón, el cuervo, la saiga, el burro, el ternero, los peces, los insectos, la víbora, el escorpión...) | Del usuario | `tools/modelos/animales.py` (el color), `tools/procesar_lote_animales.py` (a `arte/referencias`) |
| `equipo/` | Las hojas de referencia del equipo (59, en cuadrícula de 3×2): cascos, corazas, cinturones, botas y guantes, escudos, arcos, espadas, lanzas y armas cortas | Del usuario | `tools/procesar_lote_animales.py` (los recortes, a `arte/referencias`) |
| `props/` | Las 28 mallas de Hunyuan3D del equipo (`modelos/`: armas, escudos, cascos, corazas, botas, la aljaba, la antorcha, la silla y herramientas) y los renders de los que salieron (`imagenes/`) | Del usuario | `tools/modelos/patron.py` |
| `patron/` | El patrón armado a mano (`model_equipped_01.glb`): el cuerpo de `humanoide/rigged-model.glb` en pose T con el casco, el chaleco, las botas, el escudo, la aljaba, el arco, el sable y la daga puestos, y la antorcha, la lanza y la alabarda al lado. Cada pieza es una malla de `props/modelos` escalada, girada y colocada: de ahí salen la medida, la holgura y el anclaje del equipo. `cascos_colocados.glb`: los seis cascos que el usuario colocó a mano sobre el mismo cuerpo (`Model equipped 02` a `07`, en `IMPORTED/References 3d` de la rama `cline/modelos-animales`), solo los cascos, llevados exactamente al marco del patrón (`patron.py referencias <carpeta>` lo rehace desde las originales). `jinete.glb`: la referencia del jinete (`Model horserider 01.glb`, en la misma carpeta), un caballo con su silla y estribos y un cuerpo en pose T sentado en ella: el caballo y el cuerpo, reducidos, y la silla entera, en el marco del juego y en metros. De ahí salen la silla de montar, dónde va el asiento en cada montura y adónde llegan los pies del jinete (`jinete.py --fuente <glb>` lo rehace desde la original) | Del usuario | `tools/modelos/patron.py`, `tools/modelos/jinete.py` (`jinete.glb`) |
| `opengameart/gltf.zip` | El paquete *FREE Remastered Meadows* de EmacEArt, del que salió el decorado | CC0 | `tools/importar_oga_masivo.py` |

Los 17 renders que dan el color a las mallas de la ficha están en su PNG original: el color de los vértices sale de
sus píxeles, y en JPEG algunos del borde de la silueta cambiaban hasta 90 niveles. Las demás imágenes (los otros renders
de animales, las hojas del equipo y las referencias del cuerpo) están en JPEG de calidad 95, sin submuestreo de color:
los 115 PNG originales pesaban 378 MB y así pesan 77, con un error medio de medio nivel de 255.

Para un modelo nuevo desde una imagen: `tools/modelos/hunyuan3d.py <imagen> <id>` (Hunyuan3D-2.1 en Hugging Face) y
después `tools/modelos/modelos.py traer`.

## Lo que no quedó

- **Los sonidos sin usar y la música:** el resto de los tres paquetes (850 archivos). El juego no lleva música.
- **Lo demás de la Universal Animation Library:** las versiones para Unity (FBX), las de root motion (`_RM`), el
  `.blend` y las imágenes de configuración. Se bajan de nuevo de quaternius.com.
- **Las mallas viejas del cuerpo** (`sample_*.glb`, las de antes de la v0.15.8) y una copia de `exported-model.glb`
  con los mismos clips.
- **Las texturas y los cielos de Poly Haven** (`withered_grass`, `grass_ground`, `sparse_grass`, `rocky_terrain`,
  `rocky_terrain_02`, `moon_rock_02`, `stone_fire_pit`, `shrub_sorrel_01`, `veld_fire`, `wildflower_field`,
  `yellow_field`): CC0, se bajan de nuevo de polyhaven.com con esos nombres. Los cielos del juego ya están convertidos
  en `assets/textures/mapa/hdri`.
- **Los modelos de Google Drive** (`Lupus 1` y `2`, `Caballo 1` a `4`): los caballos y los lobos del juego salieron
  de `arte/modelos/para_riggear`.
- **Las referencias de Canva** (`assets/referencia/Animals` y `Humanoids`): eran las mismas imágenes que
  `arte/referencias/_raw_multiview`.
- **Lo que hizo Cline con ellas:** sus referencias procesadas (las de `arte/referencias` son las del juego), su cuerpo
  humano (reemplazado desde la v0.15.4) y `procesar_imagenes_animales.py` (lo hace
  `tools/modelos/preparar_refs_anigen.py`).
