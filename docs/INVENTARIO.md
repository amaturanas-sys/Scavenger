# Inventario de assets

> Generado por `tools/assets/inventario.py doc` a partir de [`assets/inventario.tsv`](../assets/inventario.tsv).
> No editar a mano: edita el TSV y regenera.

**735 objetos** · 330 con modelo · 405 pendientes.

Cada objeto se reemplaza dejando su archivo en la ruta que marca su id; el juego lo carga solo y, mientras falte, dibuja un marcador con sus medidas. Convenciones de importación: [assets/models/README.md](../assets/models/README.md).

## Avance por categoría

| Categoría | Total | Pendiente | Kiln | Importado | Refinado |
|---|---|---|---|---|---|
| [Mapa: terreno, vegetación, agua e hitos](#mapa) | 171 | 31 | 0 | 140 | 0 |
| [Estructuras](#estructura) | 96 | 37 | 1 | 58 | 0 |
| [Vehículos](#vehiculo) | 11 | 9 | 0 | 2 | 0 |
| [Animales](#animal) | 54 | 33 | 0 | 21 | 0 |
| [Armas](#arma) | 44 | 39 | 0 | 5 | 0 |
| [Proyectiles](#proyectil) | 6 | 6 | 0 | 0 | 0 |
| [Escudos](#escudo) | 6 | 4 | 0 | 2 | 0 |
| [Tótems](#totem) | 10 | 10 | 0 | 0 | 0 |
| [Armaduras (por piezas)](#armadura) | 48 | 44 | 0 | 4 | 0 |
| [Vestimenta (personalización)](#vestimenta) | 29 | 29 | 0 | 0 | 0 |
| [Accesorios: amuletos, tatuajes, arreos y mensajes](#accesorio) | 33 | 33 | 0 | 0 | 0 |
| [Maquinaria de asedio](#asedio) | 5 | 5 | 0 | 0 | 0 |
| [Utilería y consumibles](#utileria) | 175 | 81 | 0 | 94 | 0 |
| [Personajes y NPCs](#personaje) | 47 | 44 | 0 | 3 | 0 |

## Etiquetas

Formato `clave:valor`, separadas por comas. Una clave puede repetirse (p. ej., dos biomas).

| Clave | Valores |
|---|---|
| `bioma` | `bosque`, `ciudad`, `cualquiera`, `desierto`, `estepa`, `fiordo`, `glaciar` |
| `faccion` | `cualquiera`, `culto`, `imperio`, `neutral`, `nomada`, `salvaje` |
| `acto` | `1`, `2`, `3`, `4` |
| `uso` | libre |
| `talla` | `comun`, `gigante`, `grande`, `pequena` |
| `material` | libre |
| `rig` | `ave`, `cuadrupedo`, `humanoide`, `ninguno` |
| `especial` | `gran_guerrero`, `hito`, `legado`, `narrativo`, `rastro_padre` |
| `formato` | `modelo`, `textura`, `voxel` |
| `estilo` | libre |
| `requiere` | libre |
| `motivo` | libre |
| `manos` | `dos`, `escudo`, `una` |

<a id="mapa"></a>

## Mapa: terreno, vegetación, agua e hitos

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `mapa.suelo.estepa` | Suelo: Estepa | 4x0x4 | 0 | pendiente | `bioma:estepa` `formato:textura` | Textura de suelo para los chunks del terreno (paleta del bioma). |
| `mapa.suelo.desierto` | Suelo: Desierto | 4x0x4 | 0 | pendiente | `bioma:desierto` `formato:textura` | Textura de suelo para los chunks del terreno (paleta del bioma). |
| `mapa.suelo.bosque` | Suelo: Bosque de coníferas | 4x0x4 | 0 | pendiente | `bioma:bosque` `formato:textura` | Textura de suelo para los chunks del terreno (paleta del bioma). |
| `mapa.suelo.fiordo` | Suelo: Costa de fiordos | 4x0x4 | 0 | pendiente | `bioma:fiordo` `formato:textura` | Textura de suelo para los chunks del terreno (paleta del bioma). |
| `mapa.suelo.glaciar` | Suelo: Altiplano glaciar | 4x0x4 | 0 | pendiente | `bioma:glaciar` `formato:textura` | Textura de suelo para los chunks del terreno (paleta del bioma). |
| `mapa.roca.pequena` | Roca pequeña | 0.6x0.3x0.6 | 40 | importado | `bioma:cualquiera` `uso:decorado` `material:piedra` `formato:voxel` | Junto a los campamentos: se rompe para buscar gemas. Voxeles armados a mano (tools/voxeles/estructuras.py roca), celdas de 3 cm: el canto gris de la estepa, facetado como la roca de la foto de referencia del usuario, medio enterrado y con liquen ocre y verde arriba. |
| `mapa.roca.mediana` | Roca mediana | 1.5x1.08x1.38 | 80 | importado | `bioma:cualquiera` `uso:decorado` `material:piedra` `formato:voxel` | Junto a los campamentos: se rompe para buscar gemas. Voxeles armados a mano (tools/voxeles/estructuras.py roca_mediana), celdas de 6 cm: el canto gris facetado, como la roca pequeña. |
| `mapa.roca.grande` | Peñasco (escalable) | 4x3x4 | 160 | pendiente | `bioma:cualquiera` `uso:trepable` `material:piedra` |  |
| `mapa.relieve.acantilado` | Acantilado modular | 8x6x2 | 300 | pendiente | `bioma:fiordo` `bioma:glaciar` `uso:trepable` `material:piedra` |  |
| `mapa.relieve.duna` | Duna | 20x4x10 | 120 | pendiente | `bioma:desierto` `uso:decorado` |  |
| `mapa.relieve.grieta_glaciar` | Grieta de glaciar | 6x3x2 | 150 | pendiente | `bioma:glaciar` `uso:peligro` |  |
| `mapa.agua.oasis` | Oasis | 12x1x12 | 250 | pendiente | `bioma:desierto` `uso:recolectable` | Agua potable: clave en el desierto (acto III). |
| `mapa.agua.rio` | Tramo de río modular | 8x0.5x8 | 120 | pendiente | `bioma:cualquiera` `uso:navegable` |  |
| `mapa.agua.tempano` | Témpano de hielo | 3x1x3 | 60 | pendiente | `bioma:fiordo` `uso:decorado` |  |
| `mapa.vegetacion.hierba_alta` | Hierba alta (escondite) | 2x1x2 | 40 | pendiente | `bioma:estepa` `uso:escondite` | El jugador puede esconderse dentro. Hoy se dibuja con las hojas de la vegetacion (src/world/vegetation.c: vegetation_draw_prop), como las matas de la estepa de la foto de referencia del usuario: stipa con sus plumas y pasto, de 1 m, que se mecen con el viento y toman el color de la estacion. |
| `mapa.vegetacion.mata_hierbas` | Mata de hierbas curativas | 0.8x0.6x0.8 | 80 | pendiente | `bioma:estepa` `uso:recolectable` | F para recoger hierbas curativas. Hoy se dibuja con las hojas de la vegetacion (src/world/vegetation.c: vegetation_draw_prop), como las hierbas de la foto de referencia del usuario: salvia, menta y tomillo, con espigas moradas y amarillas mientras estan verdes. |
| `mapa.vegetacion.arbusto` | Arbusto estepario | 1x0.8x1 | 40 | pendiente | `bioma:estepa` `uso:decorado` | Hoy se dibuja con las hojas de la vegetacion (src/world/vegetation.c: vegetation_draw_prop): un arbusto de hojas anchas verde oscuro, pardo en el invierno. |
| `mapa.vegetacion.pino` | Pino | 3x10x3 | 120 | importado | `bioma:bosque` `uso:trepable` `formato:voxel` | Voxeles armados a mano (tools/voxeles/estructuras.py pino), simetrico: ocho pisos de ramas alrededor del tronco, 5 m de copa y el tronco a la vista hasta 2.3 m, con los colores de la hoja de tres vistas generada por el usuario con Canva. En el paisaje: alturas y tonos que varian, hasta 12 m. |
| `mapa.vegetacion.alerce` | Alerce | 3x12x3 | 120 | pendiente | `bioma:bosque` `uso:trepable` |  |
| `mapa.vegetacion.abedul` | Abedul | 2.5x8x2.5 | 100 | pendiente | `bioma:estepa` `bioma:bosque` `uso:decorado` |  |
| `mapa.vegetacion.roble` | Roble | 8.8x10x8.8 | 120 | importado | `bioma:bosque` `bioma:estepa` `uso:trepable` `formato:voxel` | Voxeles armados a mano (tools/voxeles/estructuras.py roble), celdas de 30 cm, simetrico como el pino: el tronco a la vista hasta los 4 m, ocho ramas abiertas y la copa de racimos en tres anillos, con el tronco y tres verdes de la hoja del arbol frondoso generada por el usuario con Canva. |
| `mapa.vegetacion.sauce` | Sauce llorón | 6.8x9x6.8 | 120 | importado | `bioma:estepa` `bioma:bosque` `uso:decorado` `formato:voxel` | Junto al agua. Voxeles armados a mano (tools/voxeles/estructuras.py sauce), celdas de 25 cm, simetrico como el pino: el tronco grueso a la vista, la cupula y las cortinas que cuelgan hasta 1.5 m del suelo, con el tronco y tres verdes de la hoja generada por el usuario con Canva. |
| `mapa.vegetacion.arce` | Arce | 8.03x11x8.03 | 120 | importado | `bioma:bosque` `uso:trepable` `formato:voxel` | En el bosque, menos que el pino (src/sim/flora.c: flora_tree_kind). Voxeles armados a mano (tools/voxeles/estructuras.py arce), celdas de 30 cm, simetrico como el pino, sin hoja de tres vistas: la corteza gris a la vista hasta los 3.5 m, cuatro ramas madre en V y la copa ovalada de racimos en tres anillos. En el mundo (src/world/trees.c), rojo y naranja en otoño y pelado en invierno. |
| `mapa.vegetacion.saxaul` | Saxaúl (arbusto del desierto) | 2x2x2 | 60 | pendiente | `bioma:desierto` `uso:recolectable` | Leña en el desierto. |
| `mapa.vegetacion.palmera` | Palmera datilera | 3x7x3 | 100 | pendiente | `bioma:desierto` `uso:recolectable` |  |
| `mapa.vegetacion.liquen` | Liquen y musgo de tundra | 1x0.1x1 | 20 | pendiente | `bioma:glaciar` `bioma:fiordo` `uso:decorado` |  |
| `mapa.hito.ovoo` | Ovoo (túmulo de piedras chamánico) | 3x2x3 | 120 | pendiente | `bioma:estepa` `uso:interactuable` `material:piedra` `especial:hito` | Punto de referencia; ofrendas. |
| `mapa.hito.kurgan` | Kurgán (túmulo funerario) | 14x3x14 | 200 | pendiente | `bioma:estepa` `especial:hito` `especial:rastro_padre` |  |
| `mapa.hito.tumba_helada` | Tumba helada del padre | 8x4x8 | 800 | pendiente | `bioma:glaciar` `acto:3` `especial:narrativo` `especial:legado` | Kurgán congelado al estilo de Pazyryk; prueba final del acto III. |
| `mapa.hito.campamento_abandonado` | Campamento abandonado del padre | 10x3x10 | 400 | pendiente | `acto:3` `especial:rastro_padre` |  |
| `mapa.hito.piedra_ciervo` | Piedra-ciervo | 1.36x3x0.44 | 150 | importado | `bioma:estepa` `material:piedra` `especial:rastro_padre` `formato:voxel` | Estela con ciervos de estilo animal. Voxeles armados a mano (tools/voxeles/estructuras.py piedra_ciervo): una losa de corte recto en celdas de 4 cm, con los tres ciervos y sus astas tallados como surcos en el frente y las grietas de la espalda, sacados de la hoja de tres vistas generada por el usuario con Canva (el ancho y el grueso, los de la hoja, por decision del usuario). |
| `mapa.hito.balbal` | Balbal (estatua de piedra) | 0.6x1.8x0.5 | 150 | pendiente | `bioma:estepa` `material:piedra` |  |
| `mapa.agua.coral` | Arrecife de coral | 1.25x0.8x1.3 | 120 | importado | `bioma:cualquiera` `uso:decorado` `uso:recolectable` `formato:voxel` | Bajo el agua de los lagos: coral rojo y perlas. Voxeles armados a mano (tools/voxeles/estructuras.py coral), celdas de 5 cm: una loma de roca clara con ramas de coral rojas y naranjas que se abren hacia arriba. |
| `mapa.vegetacion.arbusto_sorrel` | Arbusto Sorrel | 0.5x0.5x0.5 | 5000 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | Arbusto bajo poli de Poly Haven (shrub_sorrel_01, 3319 tris fuente). |
| `mapa.estructura.fogata_piedra` | Fogata de piedra | 1.0x0.5x1.0 | 5000 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | Fogata de piedra baja poli de Poly Haven (stone_fire_pit, 3887 tris fuente). |
| `mapa.textura.hierba_marchita` | Hierba marchita | 4x0x4 | 0 | pendiente | `bioma:estepa` `formato:textura` | Textura de hierba seca de Poly Haven (withered_grass). |
| `mapa.textura.hierba_escasa` | Hierba escasa | 4x0x4 | 0 | pendiente | `bioma:estepa` `formato:textura` | Textura de hierba escasa de Poly Haven (sparse_grass). |
| `mapa.textura.suelo_rocoso` | Suelo rocoso | 4x0x4 | 0 | pendiente | `bioma:estepa` `formato:textura` | Textura de terreno rocoso de Poly Haven (rocky_terrain). |
| `mapa.textura.suelo_rocoso_02` | Suelo rocoso 02 | 4x0x4 | 0 | pendiente | `bioma:estepa` `formato:textura` | Textura de terreno rocoso 02 de Poly Haven (rocky_terrain_02). |
| `mapa.textura.suelo_hierba` | Suelo con hierba | 4x0x4 | 0 | pendiente | `bioma:estepa` `formato:textura` | Textura de suelo con hierba de Poly Haven (grass_ground). |
| `mapa.hdri.pradera_abierta` | HDRI: Pradera abierta | 4x0x4 | 0 | importado | `bioma:estepa` `formato:textura` | HDRI de pradera abierta de Poly Haven (yellow_field). |
| `mapa.hdri.flores_silvestres` | HDRI: Flores silvestres | 4x0x4 | 0 | importado | `bioma:estepa` `formato:textura` | HDRI de campo de flores silvestres de Poly Haven (wildflower_field). |
| `mapa.hdri.veld_abierto` | HDRI: Veld abierto | 4x0x4 | 0 | importado | `bioma:estepa` `formato:textura` | HDRI de veld abierto de Poly Haven (veld_fire). |
| `mapa.vegetacion.pradera_libre` | Pradera libre (30 modelos) | 4x0x4 | 10000 | pendiente | `bioma:estepa` `rig:ninguno` `formato:modelo` | Pack de 30 modelos low poly de OpenGameArt (FREE Remastered Meadows). |
| `mapa.vegetacion_arbusto.bush_02b` | Arbusto | 3.95x4.31x4.76 | 799 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbusto.bush_02c` | Arbusto | 3.97x4.50x4.76 | 1499 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbusto.bush_02d` | Arbusto | 2.97x4.47x3.77 | 262 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbusto.bush_02f` | Arbusto | 4.30x5.79x6.69 | 300 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbusto.bush_dried_01` | Arbusto | 4.30x5.81x6.69 | 799 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbusto.bush_dried_02` | Arbusto | 1.91x1.60x2.75 | 1500 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real. |
| `mapa.vegetacion_arbusto.bush_dried_03` | Arbusto | 5.19x3.39x4.33 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbusto.bush_dried_04` | Arbusto | 4.42x3.78x6.08 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbusto.bush_dried_07` | Arbusto | 2.05x1.66x2.00 | 300 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca_acantilado.clif_01a` | Acantilado | 26.23x23.16x45.65 | 300 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca_acantilado.clif_01b` | Acantilado | 19.81x19.72x29.28 | 294 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca_acantilado.clif_01c` | Acantilado | 29.73x16.70x23.33 | 384 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). Cerrada (sin lados vacíos) y con bloques de vóxeles (tools/modelos/rocas_oga.py). |
| `mapa.roca_acantilado.clif_01d` | Acantilado | 48.44x22.79x76.82 | 400 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). Cerrada (sin lados vacíos) y con bloques de vóxeles (tools/modelos/rocas_oga.py). |
| `mapa.roca_acantilado.clif_01e` | Acantilado | 46.02x34.39x112.41 | 446 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). Cerrada (sin lados vacíos) y con bloques de vóxeles (tools/modelos/rocas_oga.py). |
| `mapa.vegetacion_hierba.grass_01` | Hierba | 0.31x1.04x0.40 | 84 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_hierba.grass_06` | Hierba | 0.79x0.73x0.83 | 230 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_hierba.grass_07` | Hierba | 0.25x0.70x0.40 | 138 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.terreno_colina.hill_01a` | Colina | 11.29x4.34x16.52 | 1740 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). Cerrada (sin lados vacíos) y con bloques de vóxeles (tools/modelos/rocas_oga.py). |
| `mapa.terreno_colina.hill_01b` | Colina | 15.82x4.88x20.19 | 1728 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). Cerrada (sin lados vacíos) y con bloques de vóxeles (tools/modelos/rocas_oga.py). |
| `mapa.terreno_colina.hill_01c` | Colina | 9.84x3.34x5.84 | 201 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.terreno_colina.hill_01c_1` | Colina | 8.28x3.81x5.24 | 281 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.terreno_colina.hill_01c_2` | Colina | 10.37x4.22x5.86 | 281 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.terreno_colina.hill_01c_3` | Colina | 10.49x0.91x13.53 | 3000 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.terreno_colina.hill_01d` | Colina | 6.47x3.71x6.26 | 1000 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). Cerrada (sin lados vacíos) y con bloques de vóxeles (tools/modelos/rocas_oga.py). |
| `mapa.terreno_colina.hill_half_01a` | Colina | 8.73x5.43x22.32 | 706 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). Cerrada (sin lados vacíos) y con bloques de vóxeles (tools/modelos/rocas_oga.py). |
| `mapa.terreno_colina.hill_half_01b` | Colina | 9.33x8.74x33.82 | 342 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.terreno_colina.hill_half_01c` | Colina | 9.42x4.52x19.06 | 362 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). Cerrada (sin lados vacíos) y con bloques de vóxeles (tools/modelos/rocas_oga.py). |
| `mapa.terreno_colina.hill_half_01d` | Colina | 5.98x3.53x13.40 | 250 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.terreno_colina.hill_low_01a` | Colina | 6.32x2.52x9.99 | 799 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.terreno_colina.hill_low_01b` | Colina | 8.73x3.44x14.71 | 300 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.terreno_colina.hill_low_01c` | Colina | 5.86x1.62x6.04 | 452 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). Cerrada (sin lados vacíos) y con bloques de vóxeles (tools/modelos/rocas_oga.py). |
| `mapa.terreno_colina.hill_low_01d` | Colina | 4.43x1.69x4.81 | 622 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.terreno_colina.hill_low_01e` | Colina | 5.04x5.75x6.59 | 249 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.terreno_colina.hill_low_01f` | Colina | 4.09x1.74x2.26 | 218 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_seta.mushroom_01` | Seta | 0.44x0.88x0.68 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_seta.mushroom_03` | Seta | 0.43x0.77x0.41 | 300 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_seta.mushroom_06` | Seta | 0.60x0.66x0.49 | 226 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_seta.mushroom_09` | Seta | 0.27x0.35x0.17 | 787 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real. |
| `mapa.vegetacion_seta.mushroom_10` | Seta | 0.69x0.91x0.63 | 204 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_planta.plant_01` | Planta | 1.18x0.82x0.97 | 210 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_planta.plant_02` | Planta | 2.05x0.54x1.85 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_planta.plant_03` | Planta | 1.08x0.71x0.75 | 238 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_planta.plant_04` | Planta | 1.02x0.68x0.90 | 299 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca.rock_01a` | Env Rock 01A | 10.87x16.16x23.10 | 300 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca.rock_01b` | Env Rock 01B | 10.23x5.96x11.76 | 299 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca.rock_01c` | Env Rock 01C | 13.70x8.29x17.03 | 376 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). Cerrada (sin lados vacíos) y con bloques de vóxeles (tools/modelos/rocas_oga.py). |
| `mapa.roca.rock_01d` | Env Rock 01D | 6.06x8.73x11.78 | 98 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca.rock_flat_01a` | Env Rock Flat 01A | 8.08x2.07x8.41 | 34 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca.rock_flat_01b` | Env Rock Flat 01B | 6.43x1.41x6.83 | 28 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca.rock_flat_01c` | Env Rock Flat 01C | 6.88x1.88x7.88 | 156 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). Cerrada (sin lados vacíos) y con bloques de vóxeles (tools/modelos/rocas_oga.py). |
| `mapa.roca.rock_flat_01d` | Env Rock Flat 01D | 5.99x1.68x6.14 | 36 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca.rock_sand_01a` | Env Rock Sand 01A | 13.33x4.59x10.28 | 110 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca.rock_sand_01c` | Env Rock Sand 01C | 7.09x9.92x15.43 | 202 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). Cerrada (sin lados vacíos) y con bloques de vóxeles (tools/modelos/rocas_oga.py). |
| `mapa.roca.rock_sand_01e` | Env Rock Sand 01E | 9.35x4.63x6.48 | 350 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). Cerrada (sin lados vacíos) y con bloques de vóxeles (tools/modelos/rocas_oga.py). |
| `mapa.roca.rock_sand_01e_1` | Env Rock Sand 01E 1 | 8.49x9.71x16.50 | 278 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca.rock_sand_01g` | Env Rock Sand 01G | 12.47x15.32x21.41 | 238 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca.rock_sand_01i` | Env Rock Sand 01I | 6.45x5.22x13.34 | 50 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca_piedra.stone_cobble_06` | Piedra | 1.71x0.28x1.62 | 233 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca_piedra.stone_cobble_06b` | Piedra | 0.63x0.17x0.63 | 59 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca_piedra.stone_cobble_06c` | Piedra | 0.33x0.15x0.35 | 20 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_01c` | Arbol | 17.23x8.26x13.40 | 490 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_01c_1` | Arbol | 46.15x21.91x19.78 | 568 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_01c_2` | Arbol | 10.07x13.40x10.42 | 2999 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_02a` | Arbol | 4.40x9.45x3.21 | 300 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_02a_1` | Arbol | 7.42x11.24x6.81 | 799 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_03a` | Arbol | 6.59x8.03x7.58 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_03a_1` | Arbol | 2.82x2.45x3.36 | 280 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_03a_2` | Arbol | 5.47x3.38x2.92 | 272 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_03a_3` | Arbol | 2.51x2.09x3.09 | 272 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_03a_4` | Arbol | 3.90x3.63x8.32 | 300 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_03a_5` | Arbol | 2.79x3.16x4.43 | 272 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_03a_6` | Arbol | 11.31x10.27x9.44 | 2999 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_03av2` | Arbol | 6.03x8.04x7.87 | 2999 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_04a` | Arbol | 6.40x9.24x7.76 | 668 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_04a_1` | Arbol | 6.30x9.87x5.90 | 3000 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_05d` | Arbol | 71.69x21.76x17.22 | 300 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_05d_1` | Arbol | 7.38x9.00x6.06 | 2998 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real. |
| `mapa.vegetacion_arbol.tree_06d` | Arbol | 18.70x11.86x9.83 | 602 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_06d_1` | Arbol | 21.03x11.91x9.93 | 1500 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_06d_2` | Arbol | 4.65x8.00x4.27 | 3000 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real. |
| `mapa.vegetacion_arbol.tree_08c` | Arbol | 29.61x13.67x13.24 | 746 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_08c_1` | Arbol | 34.28x18.63x14.63 | 3000 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_09d` | Arbol | 5.62x7.42x4.10 | 300 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_09d_1` | Arbol | 4.41x5.74x3.72 | 1500 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_11a` | Arbol | 5.57x7.94x6.55 | 78 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_11a_1` | Arbol | 5.60x10.42x7.41 | 80 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_11a_2` | Arbol | 9.88x12.99x18.15 | 80 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_11a_3` | Arbol | 2.30x1.79x2.59 | 80 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_11a_4` | Arbol | 5.40x7.65x6.54 | 80 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_11a_5` | Arbol | 5.40x7.65x6.54 | 80 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_11a_6` | Arbol | 7.94x10.29x9.28 | 675 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_11a_7` | Arbol | 13.87x8.38x10.44 | 1499 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_old_04` | Arbol | 6.19x7.20x6.96 | 300 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_old_04_1` | Arbol | 7.73x8.48x7.83 | 798 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_old_06` | Arbol | 6.80x7.72x7.01 | 799 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_arbol.tree_old_06_1` | Arbol | 5.91x7.00x6.95 | 2998 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real, reorientado. |
| `mapa.vegetacion_arbol.tree_old_06_2` | Arbol | 11.83x11.75x9.91 | 2998 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_tronco.trunk_01d` | Tronco | 6.58x5.95x21.15 | 135 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_tronco.trunk_01e` | Tronco | 8.81x17.99x37.91 | 264 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_tronco.trunk_02c` | Tronco | 5.15x4.80x4.05 | 282 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_tronco.trunk_02d` | Tronco | 3.23x3.22x3.25 | 86 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.vegetacion_tronco.trunk_02f` | Tronco | 5.92x8.49x13.62 | 170 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.downroad_2x2_seam_01a` | Downroad 2X2 Seam 01A | 8.00x2.02x16.00 | 302 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.downroad_2x2_seam_01b` | Downroad 2X2 Seam 01B | 8.00x3.66x16.00 | 300 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.downroad_2x2_seam_01c` | Downroad 2X2 Seam 01C | 8.00x0.61x8.77 | 300 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.downroad_2x2_seam_01d` | Downroad 2X2 Seam 01D | 8.00x0.61x8.77 | 300 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.downroad_4x4_bend_01a` | Downroad 4X4 Bend 01A | 8.00x2.24x8.00 | 418 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.downroad_4x4_bend_01b` | Downroad 4X4 Bend 01B | 8.00x2.24x8.00 | 418 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.downroad_4x4_bend_01c` | Downroad 4X4 Bend 01C | 8.00x2.24x8.00 | 418 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.downroad_4x4_bend_01d` | Downroad 4X4 Bend 01D | 8.00x2.24x8.00 | 482 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.downroad_4x4_bend_01e` | Downroad 4X4 Bend 01E | 8.00x2.24x8.00 | 546 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.road_2x2_bend_01a` | Road 2X2 Bend 01A | 4.00x0.41x4.00 | 269 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.road_2x2_bend_01b` | Road 2X2 Bend 01B | 4.00x0.44x4.00 | 253 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.road_2x2_end_01a` | Road 2X2 End 01A | 4.00x0.41x4.00 | 74 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.road_2x2_end_01b` | Road 2X2 End 01B | 4.00x0.47x4.00 | 71 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.road_2x2_seam_01a` | Road 2X2 Seam 01A | 4.00x0.41x4.00 | 150 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.road_2x2_seam_01b` | Road 2X2 Seam 01B | 4.00x0.52x4.00 | 180 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.road_2x2_seam_01c` | Road 2X2 Seam 01C | 4.00x0.41x4.00 | 154 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.road_2x4_seam_01a` | Road 2X4 Seam 01A | 8.00x0.84x16.00 | 300 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.road_4x4_bend_01a` | Road 4X4 Bend 01A | 8.00x0.53x8.00 | 455 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.camino.road_4x4_bend_01b` | Road 4X4 Bend 01B | 8.00x0.49x8.00 | 454 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca.swamp_rockbig_01a` | Swamp Rockbig 01A | 7.03x2.51x5.63 | 258 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca.swamp_rockbig_01b` | Swamp Rockbig 01B | 8.48x2.71x5.76 | 80 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca.swamp_rockbig_01c` | Swamp Rockbig 01C | 11.05x1.47x6.48 | 54 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca.swamp_rockbig_02a` | Swamp Rockbig 02A | 9.35x4.63x6.48 | 328 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). Cerrada (sin lados vacíos) y con bloques de vóxeles (tools/modelos/rocas_oga.py). |
| `mapa.roca.swamp_rockbig_02b` | Swamp Rockbig 02B | 9.99x7.67x17.57 | 300 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `mapa.roca.swamp_rockbig_02c` | Swamp Rockbig 02C | 12.83x7.84x19.27 | 692 | importado | `bioma:estepa` `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). Cerrada (sin lados vacíos) y con bloques de vóxeles (tools/modelos/rocas_oga.py). |

<a id="estructura"></a>

## Estructuras

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `estructura.vivienda.yurta_comun` | Yurta común | 4.5x2.6x4.5 | 300 | kiln | `faccion:nomada` `uso:habitable` `material:fieltro` | Ya existe (Kiln). Puerta hacia +X. |
| `estructura.vivienda.yurta_jefe` | Yurta del jefe | 7x3.4x7 | 600 | pendiente | `faccion:nomada` `uso:habitable` `material:fieltro` | Más grande, con estandarte y ornamento. |
| `estructura.vivienda.tienda_ligera` | Tienda ligera de viaje | 2.5x1.5x3 | 120 | pendiente | `faccion:nomada` `uso:habitable` `material:cuero` |  |
| `estructura.vivienda.choza_fieltro` | Choza de fieltro | 3x2x3 | 150 | pendiente | `faccion:nomada` `uso:habitable` `material:fieltro` |  |
| `estructura.vivienda.casa_adobe` | Casa de adobe | 6x3.5x5 | 250 | pendiente | `bioma:desierto` `faccion:neutral` `uso:habitable` `material:barro` |  |
| `estructura.vivienda.cabana_troncos` | Cabaña de troncos | 6x4x5 | 300 | pendiente | `bioma:bosque` `faccion:neutral` `uso:habitable` `material:madera` |  |
| `estructura.vivienda.casa_larga` | Casa larga de los fiordos | 6x5x18 | 500 | pendiente | `bioma:fiordo` `faccion:neutral` `uso:habitable` `material:madera` |  |
| `estructura.vivienda.refugio_nieve` | Refugio excavado en la nieve | 4x2x4 | 150 | pendiente | `bioma:glaciar` `uso:habitable` |  |
| `estructura.vivienda.casa_campesina` | Casa campesina | 6x4x7 | 300 | pendiente | `bioma:ciudad` `faccion:imperio` `uso:habitable` `material:madera` |  |
| `estructura.vivienda.casa_urbana` | Casa urbana de dos plantas | 8x7x8 | 500 | pendiente | `bioma:ciudad` `faccion:imperio` `uso:habitable` `material:piedra` |  |
| `estructura.vivienda.palacio` | Palacio imperial | 40x18x30 | 3000 | pendiente | `bioma:ciudad` `faccion:imperio` `acto:2` `especial:narrativo` |  |
| `estructura.campamento.fogata` | Fogata | 1.2x0.5x1.2 | 60 | pendiente | `faccion:nomada` `uso:interactuable` | Hoy es un primitivo procedural. |
| `estructura.campamento.corral` | Corral de cuerdas y estacas | 10x1.2x10 | 150 | pendiente | `faccion:nomada` `uso:decorado` `material:madera` |  |
| `estructura.campamento.secadero` | Secadero de carne | 3x2x1 | 80 | pendiente | `faccion:nomada` `uso:interactuable` `material:madera` |  |
| `estructura.campamento.forja` | Forja portátil | 2x1.5x2 | 300 | importado | `faccion:nomada` `uso:interactuable` `material:hierro` `formato:voxel` | Función: herrero. Horno cerrado con la llamarada adentro y un fuelle de acordeon al costado: voxeles armados a mano (tools/voxeles/estructuras.py forja), celdas de 6 cm, con la distribucion y el color de la hoja de tres vistas generada por el usuario con Canva. |
| `estructura.campamento.enfermeria` | Yurta enfermería | 4x2.6x4 | 350 | pendiente | `faccion:nomada` `uso:interactuable` `material:fieltro` | Función: curandero. |
| `estructura.campamento.jaula_prisioneros` | Jaula de prisioneros | 3x2x3 | 150 | pendiente | `faccion:cualquiera` `uso:interactuable` `material:madera` |  |
| `estructura.campamento.poste_ejecucion` | Poste de castigo | 0.5x2.5x0.5 | 50 | pendiente | `faccion:cualquiera` `uso:interactuable` `material:madera` |  |
| `estructura.campamento.empalizada` | Empalizada modular | 4x2.5x0.4 | 80 | pendiente | `faccion:nomada` `uso:destructible` `material:madera` |  |
| `estructura.campamento.atalaya` | Atalaya de madera | 2.55x6x6 | 200 | importado | `faccion:nomada` `uso:trepable` `material:madera` `formato:voxel` | Voxeles armados a mano (tools/voxeles/estructuras.py atalaya), celdas de 15 cm: la torre de 2.5 m con sus tres pisos, las cruces, la escalera de mano al frente y la escalera larga atras, con los colores de la hoja de tres vistas generada por el usuario con Canva. Con la escalera de atras ocupa 6 m de largo (las medidas de la hoja: el usuario las confirmo). Reemplaza a la del kit de edificios (src/sim/buildkit.c). |
| `estructura.imperio.muralla` | Muralla modular | 8x8x3 | 200 | pendiente | `bioma:ciudad` `faccion:imperio` `uso:trepable` `material:piedra` |  |
| `estructura.imperio.puerta` | Puerta de la ciudad | 12.3x10x24 | 600 | importado | `bioma:ciudad` `faccion:imperio` `uso:destructible` `material:madera` `formato:voxel` | Voxeles armados a mano (tools/voxeles/estructuras.py puerta_fortificada), celdas de 1/3 m, la version larga de la hoja de tres vistas generada por el usuario con Canva (la vista de lado): la portada con dos torres y el portico, el cuerpo que baja en cuatro tramos con sus frisos de ladrillo y la abertura de la escalera, y al final dos torres chicas con el muro de la puerta ciega. |
| `estructura.imperio.torre` | Torre de vigilancia | 5x14x5 | 400 | pendiente | `bioma:ciudad` `faccion:imperio` `uso:trepable` `material:piedra` |  |
| `estructura.imperio.cuartel` | Cuartel | 16x6x10 | 600 | pendiente | `bioma:ciudad` `faccion:imperio` |  |
| `estructura.imperio.granero` | Granero | 10x7x8 | 400 | pendiente | `bioma:ciudad` `faccion:imperio` `uso:interactuable` | Las cosechas marchitas (acto II). |
| `estructura.imperio.molino` | Molino | 6x10x6 | 500 | pendiente | `bioma:ciudad` `faccion:imperio` |  |
| `estructura.imperio.fundicion` | Fundición / taller industrial | 18x10x14 | 1200 | pendiente | `bioma:ciudad` `faccion:imperio` `especial:narrativo` | Capacidad industrial del imperio. |
| `estructura.imperio.cadalso` | Cadalso de ejecución pública | 6x3x6 | 250 | pendiente | `bioma:ciudad` `faccion:imperio` `acto:2` `especial:narrativo` | Escena de la huida del acto II. |
| `estructura.culto.altar` | Altar del culto | 12x6x12 | 1500 | pendiente | `bioma:ciudad` `faccion:culto` `acto:4` `especial:narrativo` `uso:destructible` | Objetivo del asalto final. |
| `estructura.culto.templo` | Templo del culto | 24x14x30 | 2500 | importado | `bioma:ciudad` `faccion:culto` `acto:4` `formato:voxel` | Voxeles armados a mano (tools/voxeles/estructuras.py templo): celdas de 0.5 m, la distribucion y el color de la hoja de tres vistas generada por el usuario con Canva. |
| `estructura.culto.jaula_sacrificio` | Jaula de cautivos para el sacrificio | 3x2.5x3 | 200 | pendiente | `faccion:culto` `uso:interactuable` `material:hierro` | Rescates del acto III. |
| `estructura.ruina.muro` | Muro en ruinas | 3.42x3x1.16 | 150 | importado | `bioma:cualquiera` `uso:decorado` `material:piedra` `formato:voxel` | Voxeles armados a mano (tools/voxeles/estructuras.py muro_en_ruinas), celdas de 5.3 cm: la base de dos hiladas de piedra y 12 hiladas de ladrillo trabado, la rotura en escalones y el barro del nucleo hundido donde se cayeron los ladrillos, con la forma y los colores de la hoja de tres vistas generada por el usuario con Canva. Reemplaza al del kit de edificios (src/sim/buildkit.c). En el mundo es el tramo que quedo de pie en los vestigios de los asentamientos antiguos (src/sim/ancient.h): de dos a cuatro por planta, sobre los muros de afuera. |
| `estructura.ruina.yurta_quemada` | Yurta quemada | 4.5x1.5x4.5 | 250 | pendiente | `faccion:nomada` `acto:1` `uso:decorado` | Restos de la masacre. |
| `estructura.ruina.escombros` | Escombros | 3x0.8x3 | 80 | pendiente | `bioma:cualquiera` `uso:decorado` | Lo que deja la lluvia torrencial de una estructura sin mantenimiento. |
| `estructura.ruina.cenizas` | Cenizas de una estructura | 3x0.5x3 | 80 | pendiente | `bioma:cualquiera` `uso:decorado` | Lo que deja el fuego. |
| `estructura.campamento.refugio` | Refugio de ramas y pieles | 3x2x3 | 150 | pendiente | `faccion:nomada` `uso:habitable` `uso:construible` | Construcción en grupo. |
| `estructura.campamento.hoguera` | Hoguera grande | 2.5x1.5x2.5 | 120 | pendiente | `faccion:nomada` `uso:interactuable` `uso:construible` | Construcción en grupo: calor y luz para todo el campamento. |
| `estructura.campamento.horno_cocina` | Horno de cocina de barro | 1.5x1.4x1.5 | 150 | pendiente | `faccion:nomada` `uso:interactuable` `uso:construible` `material:barro` | Construcción en grupo; el cocinero acelera la obra. |
| `estructura.campamento.horno_bronce` | Horno de fundición de bronce | 2x2.5x2 | 300 | pendiente | `uso:interactuable` `uso:construible` `material:barro` | Armas y armaduras de bronce. Requiere un herrero. |
| `estructura.campamento.horno_acero` | Horno de fundición de acero | 2.5x3.5x2.5 | 400 | pendiente | `uso:interactuable` `uso:construible` `material:piedra` | Armas y armaduras de acero. Requiere un herrero. |
| `estructura.campamento.taller_herrero` | Taller del herrero | 4x3x4 | 400 | pendiente | `uso:interactuable` `uso:construible` `material:piedra` | Obra de campamento grande: el temple, el acero wootz y las puntas de lanza de acero. Requiere un herrero. |
| `estructura.defensa.trinchera` | Trinchera | 1.2x0.8x4 | 80 | pendiente | `faccion:cualquiera` `uso:cobertura` | Se cava con pala (acción individual). |
| `estructura.defensa.muro_piedra` | Muro de piedra seca | 4x2x0.6 | 120 | pendiente | `faccion:nomada` `uso:destructible` `uso:construible` `material:piedra` | Construcción en grupo. |
| `estructura.puente.bridge_01` | Env Bridge 01 | 4.67x3.23x7.33 | 798 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.puente.bridge_02` | Env Bridge 02 | 4.35x2.60x6.23 | 748 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.puente.bridge_02_1` | Env Bridge 02 1 | 4.30x2.28x5.91 | 1499 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). |
| `estructura.puente.bridge_03` | Env Bridge 03 | 4.27x1.24x5.58 | 547 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.puente.bridge_05` | Env Bridge 05 | 6.63x4.77x11.18 | 2999 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.puente.bridge_06` | Env Bridge 06 | 6.00x5.63x21.36 | 1500 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.puente.bridge_hanging_01` | Env Bridge Hanging 01 | 6.00x13.75x52.59 | 800 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.puente.bridge_hanging_02` | Env Bridge Hanging 02 | 6.00x5.63x21.36 | 1500 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.puente.bridge_hanging_03` | Env Bridge Hanging 03 | 6.00x5.37x20.39 | 1500 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.puente.bridge_hanging_04` | Env Bridge Hanging 04 | 6.00x5.39x20.41 | 1499 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.puente.bridge_pier_04a` | Env Bridge Pier 04A | 2.10x8.96x9.91 | 800 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.puente.bridge_pier_04b` | Env Bridge Pier 04B | 2.06x0.48x2.24 | 800 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). |
| `estructura.puente.bridge_pier_04c` | Env Bridge Pier 04C | 3.35x2.06x3.58 | 800 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), reorientado. |
| `estructura.puente.bridge_pier_end_04a` | Env Bridge Pier End 04A | 3.35x2.05x3.54 | 799 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), reorientado. |
| `estructura.puente.bridge_pier_element_04a` | Env Bridge Pier Element 04A | 2.10x31.66x42.13 | 800 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.puente.bridge_pier_element_04b` | Env Bridge Pier Element 04B | 0.36x1.79x0.21 | 110 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.puente.bridge_pier_element_04c` | Env Bridge Pier Element 04C | 0.36x1.73x0.19 | 104 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.puente.bridge_pier_element_04d` | Env Bridge Pier Element 04D | 0.36x1.90x0.32 | 142 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_pin_01a` | Env Fence Pin 01A | 0.25x1.57x0.29 | 50 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_pin_01b` | Env Fence Pin 01B | 0.26x2.18x0.22 | 48 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_pin_01c` | Env Fence Pin 01C | 0.13x1.90x0.32 | 102 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_pin_02a` | Env Fence Pin 02A | 0.33x1.28x0.37 | 148 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_pin_03a` | Env Fence Pin 03A | 0.47x1.51x0.31 | 134 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_rock_01a` | Env Fence Rock 01A | 5.81x3.36x21.52 | 607 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_rock_01b` | Env Fence Rock 01B | 3.52x3.05x12.03 | 299 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_rock_01c` | Env Fence Rock 01C | 2.56x3.68x8.55 | 268 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_rock_01d` | Env Fence Rock 01D | 2.09x1.68x4.89 | 128 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_rock_01e` | Env Fence Rock 01E | 4.95x3.75x17.38 | 299 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_rock_01f` | Env Fence Rock 01F | 5.15x1.89x16.73 | 299 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_wood_01a` | Env Fence Wood 01A | 0.53x1.50x2.60 | 300 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real. |
| `estructura.cerca.fence_wood_01b` | Env Fence Wood 01B | 0.46x1.60x3.03 | 300 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real. |
| `estructura.cerca.fence_wood_01c` | Env Fence Wood 01C | 0.60x1.40x2.59 | 300 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real. |
| `estructura.cerca.fence_wood_02a` | Env Fence Wood 02A | 1.67x7.73x10.02 | 286 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_wood_02b` | Env Fence Wood 02B | 0.43x1.80x4.43 | 300 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real. |
| `estructura.cerca.fence_wood_02c` | Env Fence Wood 02C | 0.47x1.60x3.69 | 300 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real. |
| `estructura.cerca.fence_wood_02d` | Env Fence Wood 02D | 1.56x3.08x3.73 | 286 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_wood_02e` | Env Fence Wood 02E | 3.25x16.43x41.26 | 300 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_wood_03a` | Env Fence Wood 03A | 0.40x1.70x3.15 | 232 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real. |
| `estructura.cerca.fence_wood_03b` | Env Fence Wood 03B | 0.37x1.60x2.44 | 300 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real. |
| `estructura.cerca.fence_wood_03b_1` | Env Fence Wood 03B 1 | 1.48x0.36x1.09 | 124 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_wood_03c` | Env Fence Wood 03C | 1.78x8.91x14.60 | 288 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_wood_04a` | Env Fence Wood 04A | 0.94x1.20x2.65 | 680 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real. |
| `estructura.cerca.fence_wood_04b` | Env Fence Wood 04B | 2.79x1.76x3.20 | 630 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_wood_04c` | Env Fence Wood 04C | 2.79x12.23x14.03 | 630 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_wood_wine_01a` | Env Fence Wood Wine 01A | 2.87x6.53x11.71 | 800 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_wood_wine_01b` | Env Fence Wood Wine 01B | 2.96x7.24x13.82 | 798 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_wood_wine_02d` | Env Fence Wood Wine 02D | 1.61x3.31x4.14 | 800 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.fence_wood_wine_04a` | Env Fence Wood Wine 04A | 3.58x4.59x10.15 | 1500 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.portal.frostp_portal` | Frostp Portal | 2.01x0.81x2.21 | 1500 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.tree2a` | Tree2A | 11.20x5.45x5.13 | 798 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.wood_fence3` | Wood Fence3 | 0.36x1.60x3.71 | 300 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real. |
| `estructura.cerca.wood_fence3b` | Wood Fence3B | 2.67x11.35x26.28 | 300 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |
| `estructura.cerca.wood_fence3c` | Wood Fence3C | 2.66x8.65x17.64 | 300 | importado | `rig:ninguno` `formato:modelo` `uso:decorado` | OpenGameArt FREE Remastered Meadows |

<a id="vehiculo"></a>

## Vehículos

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `vehiculo.agua.bote_remos` | Bote de remos | 1.5x0.8x4 | 250 | pendiente | `bioma:fiordo` `uso:conducible` `material:madera` |  |
| `vehiculo.agua.canoa_pieles` | Canoa de pieles | 0.9x0.6x5 | 200 | pendiente | `bioma:fiordo` `bioma:bosque` `uso:conducible` `material:cuero` |  |
| `vehiculo.agua.balsa` | Balsa | 2.5x0.5x3 | 100 | pendiente | `bioma:cualquiera` `uso:conducible` `material:madera` |  |
| `vehiculo.agua.velero` | Velero | 3x8x9 | 600 | pendiente | `bioma:fiordo` `uso:conducible` `material:madera` |  |
| `vehiculo.agua.barco_carga` | Barco de carga | 6x14x20 | 1500 | pendiente | `bioma:fiordo` `uso:conducible` `faccion:neutral` |  |
| `vehiculo.agua.barco_guerra` | Barco de guerra con cañones | 8x18x28 | 2500 | pendiente | `bioma:fiordo` `uso:conducible` `faccion:imperio` |  |
| `vehiculo.tierra.carreta_bueyes` | Yunta de bueyes con carreta | 1.63x1.63x4.16 | 600 | importado | `faccion:nomada` `uso:conducible` `material:madera` `formato:voxel` | Mueve el campamento. Los bueyes: animal.ganado.buey. Voxeles armados a mano (tools/voxeles/estructuras.py carro_varas), celdas de 4.1 cm: el carro de la yunta (la caja con sus herrajes, dos ruedas de rayos de 1.3 m, la lanza con el tornapunta y el poste del yugo con el yugo), de la hoja del carro de varas generada por el usuario con Canva; los bueyes no van en el modelo. A la escala de la gente (1.70 m): las ruedas de 1.3 m, el piso de la caja a 1 m y sus tablas a la altura del pecho (antes, ruedas de 2 m y el piso a 1.5 m). |
| `vehiculo.tierra.kibitka` | Carro con yurta montada (kibitka) | 4x4x6 | 800 | pendiente | `faccion:nomada` `uso:conducible` |  |
| `vehiculo.tierra.carreta_mercancias` | Carreta de mercancías | 1.7x1.4x3.25 | 400 | importado | `faccion:neutral` `uso:conducible` `material:madera` `formato:voxel` | Voxeles armados a mano (tools/voxeles/estructuras.py carreta), celdas de 5 cm: la caja de tablas con sus postes, las cuatro ruedas de disco de 1 m, los ejes y la vara que los une, con tres tonos de madera de la hoja de tres vistas generada por el usuario con Canva. A la escala de la gente (1.70 m): el piso a 0.8 m y la baranda a 1.4 m (de la hoja salia de 2.43 x 4.64 m, con ruedas de 1.43 m). |
| `vehiculo.tierra.carruaje` | Carruaje imperial | 2.2x2.8x5 | 900 | pendiente | `faccion:imperio` `uso:conducible` |  |
| `vehiculo.tierra.trineo` | Trineo | 1.2x0.8x3 | 200 | pendiente | `bioma:glaciar` `uso:conducible` `material:madera` |  |

<a id="animal"></a>

## Animales

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `animal.montura.caballo_estepario` | Caballo estepario | 0.61x1.94x2.46 | 900 | pendiente | `faccion:nomada` `uso:montable` `rig:cuadrupedo` | Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen); falta el esqueleto y los clips. Medidas: 1.58 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. |
| `animal.montura.caballo_estepario_2` | Caballo estepario (variante 2) | 0.61x1.89x2.39 | 900 | pendiente | `faccion:nomada` `uso:montable` `rig:cuadrupedo` | Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen); falta el esqueleto y los clips. Medidas: 1.58 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. |
| `animal.montura.caballo_estepario_3` | Caballo estepario (variante 3) | 0.58x1.93x2.34 | 900 | pendiente | `faccion:nomada` `uso:montable` `rig:cuadrupedo` | Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen); falta el esqueleto y los clips. Medidas: 1.58 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. |
| `animal.montura.caballo_guerra` | Caballo de guerra | 0.74x2.22x2.78 | 1000 | pendiente | `uso:montable` `rig:cuadrupedo` | Admite barda (armadura de caballo). Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen); falta el esqueleto y los clips. Medidas: 1.75 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. |
| `animal.montura.camello_bactriano` | Camello bactriano | 0.8x2.2x3 | 1000 | importado | `bioma:desierto` `uso:montable` `rig:cuadrupedo` | Malla de IMPORTED/Animals/Models (n.º 17 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.montura.camello_bactriano_2` | Camello bactriano (variante 2) | 0.8x2.2x3 | 1000 | importado | `bioma:desierto` `uso:montable` `rig:cuadrupedo` | Malla de IMPORTED/Animals/Models (n.º 7 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.montura.dromedario` | Dromedario | 0.76x2.6x3.26 | 1000 | importado | `bioma:desierto` `uso:montable` `rig:cuadrupedo` | Medidas: 2 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. Malla de IMPORTED/Animals/Models (n.º 8 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.montura.elefante_guerra` | Elefante de guerra | 2x3.2x5 | 1500 | importado | `uso:montable` `rig:cuadrupedo` `talla:gigante` | Fuerza y asedio. Malla de IMPORTED/Animals/Models (n.º 18 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.montura.reno` | Reno de tiro | 0.6x1.3x2 | 800 | importado | `bioma:fiordo` `bioma:glaciar` `uso:montable` `rig:cuadrupedo` | Malla de IMPORTED/Animals/Models (n.º 16 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.ganado.oveja` | Oveja | 0.4x0.8x1.1 | 400 | pendiente | `faccion:nomada` `rig:cuadrupedo` |  |
| `animal.ganado.cabra` | Cabra | 0.4x0.8x1.1 | 400 | importado | `faccion:nomada` `rig:cuadrupedo` | Malla de IMPORTED/Animals/Models (n.º 10 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.ganado.yak` | Yak | 0.9x1.7x2.5 | 800 | importado | `bioma:glaciar` `rig:cuadrupedo` | Malla de IMPORTED/Animals/Models (n.º 9 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.ganado.buey` | Buey | 1.17x2.12x3.15 | 700 | importado | `rig:cuadrupedo` | Medidas: 1.88 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. Malla de IMPORTED/Animals/Models (n.º 11 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.ganado.vaca` | Vaca | 0.86x1.77x2.68 | 700 | pendiente | `faccion:nomada` `rig:cuadrupedo` | Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen); falta el esqueleto y los clips. Medidas: 1.62 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. La hoja la dibuja mirando a la derecha (--voltear-lado). |
| `animal.ganado.perro_pastor` | Perro pastor (mastín) | 0.35x0.94x1.24 | 500 | importado | `faccion:nomada` `rig:cuadrupedo` | Medidas: 0.78 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. Malla de IMPORTED/Animals/Models (n.º 20 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.salvaje.lobo` | Lobo | 0.35x1.06x1.37 | 500 | pendiente | `faccion:salvaje` `rig:cuadrupedo` | Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen); falta el esqueleto y los clips. Medidas: 0.85 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. |
| `animal.salvaje.lobo_2` | Lobo (variante 2) | 0.35x1.04x1.37 | 500 | pendiente | `faccion:salvaje` `rig:cuadrupedo` | Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen); falta el esqueleto y los clips. Medidas: 0.85 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. |
| `animal.salvaje.tigre` | Tigre | 0.45x1.27x2.09 | 700 | pendiente | `faccion:salvaje` `bioma:bosque` `rig:cuadrupedo` | Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen); falta el esqueleto y los clips. Medidas: 1.1 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. |
| `animal.salvaje.oso` | Oso pardo | 0.97x1.59x2.55 | 700 | importado | `faccion:salvaje` `bioma:bosque` `rig:cuadrupedo` | Medidas: 1.5 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. Malla de IMPORTED/Animals/Models (n.º 19 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.salvaje.oso_polar` | Oso polar | 0.95x1.69x3.02 | 700 | pendiente | `faccion:salvaje` `bioma:glaciar` `bioma:fiordo` `rig:cuadrupedo` | Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen); falta el esqueleto y los clips. Medidas: 1.6 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. |
| `animal.salvaje.ciervo` | Ciervo | 0.94x2.23x2.11 | 500 | importado | `faccion:salvaje` `bioma:bosque` `rig:cuadrupedo` | Medidas: 1.45 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. Malla de IMPORTED/Animals/Models (n.º 5 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.salvaje.cierva` | Cierva | 0.44x1.7x1.83 | 500 | importado | `faccion:salvaje` `bioma:bosque` `rig:cuadrupedo` | Medidas: 1.2 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. Malla de IMPORTED/Animals/Models (n.º 6 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.salvaje.ibice` | Íbice | 0.4x1.2x1.4 | 500 | importado | `faccion:salvaje` `bioma:glaciar` `rig:cuadrupedo` | Malla de IMPORTED/Animals/Models (n.º 13 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.salvaje.ibice_2` | Íbice (variante 2) | 0.4x1.2x1.4 | 500 | importado | `faccion:salvaje` `bioma:glaciar` `rig:cuadrupedo` | Malla de IMPORTED/Animals/Models (n.º 2 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.salvaje.zorro` | Zorro | 0.25x0.45x0.9 | 300 | importado | `faccion:salvaje` `rig:cuadrupedo` | Malla de IMPORTED/Animals/Models (n.º 15 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.salvaje.leopardo_nieves` | Leopardo de las nieves | 0.4x0.7x2 | 600 | importado | `faccion:salvaje` `bioma:glaciar` `rig:cuadrupedo` | Malla de IMPORTED/Animals/Models (n.º 1 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.ave.aguila` | Águila (cetrería) | 1.8x0.9x0.9 | 300 | pendiente | `rig:ave` `uso:equipable` | Puede acompañar al jugador. |
| `animal.ave.halcon` | Halcón (cetrería) | 1x0.5x0.5 | 250 | pendiente | `rig:ave` `uso:equipable` |  |
| `animal.ave.buitre` | Buitre | 2.4x1x1 | 300 | pendiente | `faccion:salvaje` `rig:ave` |  |
| `animal.montura.mula` | Mula | 0.6x1.5x2 | 800 | importado | `uso:montable` `rig:cuadrupedo` | Montura de carga: resiste el desierto. Malla de IMPORTED/Animals/Models (n.º 14 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.montura.burro` | Burro | 0.5x1.2x1.6 | 700 | pendiente | `bioma:desierto` `uso:montable` `rig:cuadrupedo` |  |
| `animal.ganado.becerro` | Becerro | 0.4x1x1.4 | 500 | pendiente | `faccion:nomada` `rig:cuadrupedo` | Se pastorea; carne y piel al sacrificarlo. |
| `animal.salvaje.perro_salvaje` | Perro asilvestrado | 0.3x0.84x1.07 | 500 | pendiente | `faccion:salvaje` `rig:cuadrupedo` | Caza en jauría; domable (pelea, lazo y comida). Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen); falta el esqueleto y los clips. Medidas: 0.65 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. |
| `animal.salvaje.puma` | Puma | 0.35x0.99x1.65 | 600 | importado | `faccion:salvaje` `rig:cuadrupedo` | Cazador solitario: acecha. Medidas: 0.8 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. Malla de IMPORTED/Animals/Models (n.º 4 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.salvaje.hiena` | Hiena | 0.35x0.85x1.3 | 500 | importado | `faccion:salvaje` `bioma:desierto` `rig:cuadrupedo` | Siempre hostil; en clan. Malla de IMPORTED/Animals/Models (n.º 3 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.salvaje.coyote` | Coyote | 0.26x0.76x1.01 | 400 | pendiente | `faccion:salvaje` `rig:cuadrupedo` | Siempre hostil; en pareja o grupo. Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen); falta el esqueleto y los clips. Medidas: 0.62 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. |
| `animal.salvaje.jabali` | Jabalí | 0.52x1.11x1.81 | 500 | pendiente | `faccion:salvaje` `rig:cuadrupedo` | Siempre hostil; embiste. Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen); falta el esqueleto y los clips. Medidas: 1.1 m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la imagen. |
| `animal.salvaje.antilope` | Antílope saiga | 0.35x0.8x1.3 | 500 | pendiente | `faccion:salvaje` `bioma:estepa` `rig:cuadrupedo` | Presa: manadas. |
| `animal.salvaje.reno` | Reno salvaje | 0.6x1.3x2 | 600 | importado | `faccion:salvaje` `bioma:glaciar` `rig:cuadrupedo` | Presa: manadas del frío. Malla de IMPORTED/Animals/Models (n.º 16 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.salvaje.gacela` | Gacela | 0.3x0.9x1.2 | 500 | importado | `faccion:salvaje` `bioma:desierto` `rig:cuadrupedo` | Presa: la más rápida. Malla de IMPORTED/Animals/Models (n.º 12 de la ficha), con su esqueleto de cuadrúpedo y sus clips (tools/modelos/animales.py). |
| `animal.salvaje.liebre` | Liebre | 0.15x0.3x0.5 | 200 | pendiente | `faccion:salvaje` `rig:cuadrupedo` | Presa solitaria; caza de halcón. |
| `animal.ave.cuervo` | Cuervo | 0.9x0.4x0.5 | 200 | pendiente | `faccion:salvaje` `rig:ave` `uso:equipable` | Domado, explora: revela el mapa. |
| `animal.acuatico.cocodrilo` | Cocodrilo | 0.6x0.5x3.5 | 700 | pendiente | `faccion:salvaje` `rig:cuadrupedo` | Embosca desde el agua; no se aleja de su orilla. |
| `animal.acuatico.tortuga_marina` | Tortuga marina | 0.9x0.4x1 | 400 | pendiente | `faccion:salvaje` `rig:cuadrupedo` | Presa lenta en tierra; huye al agua. |
| `animal.acuatico.peces` | Pez (banco) | 0.08x0.1x0.35 | 60 | pendiente | `faccion:salvaje` `rig:ninguno` | Bancos en los lagos; se pescan con lanza o flecha. |
| `animal.salvaje.vibora` | Víbora | 0.06x0.06x1 | 150 | pendiente | `faccion:salvaje` `rig:ninguno` | Muerde si te acercas: veneno. |
| `animal.salvaje.escorpion` | Escorpión | 0.1x0.05x0.2 | 100 | pendiente | `faccion:salvaje` `bioma:desierto` `rig:ninguno` | Pica si te acercas: veneno. |
| `animal.salvaje.arana` | Araña | 0.12x0.05x0.15 | 100 | pendiente | `faccion:salvaje` `rig:ninguno` | Muerde si te acercas: veneno leve. |
| `animal.insecto.abejas` | Abeja (enjambre) | 0.02x0.02x0.02 | 20 | pendiente | `faccion:salvaje` `rig:ninguno` | Defienden la colmena; el humo las calma. |
| `animal.insecto.colmena` | Colmena silvestre | 0.4x0.5x0.4 | 80 | pendiente | `faccion:salvaje` `uso:interactuable` | Con la antorcha (humo) se toma la miel. |
| `animal.insecto.avispas` | Avispa (enjambre) | 0.02x0.02x0.03 | 20 | pendiente | `faccion:salvaje` `rig:ninguno` | Atacan a quien se acerca al nido. |
| `animal.insecto.avispero` | Avispero | 0.3x0.4x0.3 | 80 | pendiente | `faccion:salvaje` |  |
| `animal.insecto.mosquitos` | Mosquito (nube) | 0.01x0.01x0.01 | 10 | pendiente | `faccion:salvaje` `rig:ninguno` | Junto al agua, con calor, al atardecer y de noche. |
| `animal.insecto.moscas` | Mosca (nube) | 0.01x0.01x0.01 | 10 | pendiente | `faccion:salvaje` `rig:ninguno` | Sobre los cadáveres. |

<a id="arma"></a>

## Armas

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `arma.corta.cuchillo` | Cuchillo de hueso y hierro | 0.05x0.3x0.03 | 60 | pendiente | `uso:equipable` `manos:una` |  |
| `arma.corta.daga` | Daga | 0.09x0.4x0.03 | 80 | importado | `uso:equipable` `manos:una` | Low-poly con textura (estilo PS2) desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma plana); las medidas, las de la imagen. |
| `arma.corta.sable` | Sable de la estepa | 0.52x0.9x0.03 | 120 | importado | `uso:equipable` `faccion:nomada` `manos:una` | Curvo y de un filo: a caballo corta limpio, no se traba y suma la carrera. Low-poly con textura (estilo PS2) desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma plana --girar 90); las medidas, las de la imagen. |
| `arma.corta.hacha_mano` | Hacha de mano | 0.24x0.6x0.04 | 100 | importado | `uso:equipable` `manos:una` | Low-poly con textura (estilo PS2) desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma plana); las medidas, las de la imagen. |
| `arma.corta.maza` | Maza | 0.27x0.7x0.27 | 100 | importado | `uso:equipable` `manos:una` | Low-poly con textura (estilo PS2) desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma revolucion); las medidas, las de la imagen. |
| `arma.larga.lanza` | Lanza ligera | 0.1x2.4x0.1 | 80 | pendiente | `uso:equipable` `manos:una` | A pie y a caballo; en ristre (L), la carga. |
| `arma.larga.guja` | Guja | 0.2x2.2x0.05 | 120 | pendiente | `uso:equipable` `manos:dos` |  |
| `arma.larga.pica` | Pica imperial | 0.1x4.5x0.1 | 80 | pendiente | `uso:equipable` `faccion:imperio` `manos:dos` |  |
| `arma.larga.alabarda` | Alabarda | 0.3x2.2x0.05 | 140 | pendiente | `uso:equipable` `faccion:imperio` `manos:dos` |  |
| `arma.larga.espada_larga` | Espada larga | 0.23x1.2x0.03 | 120 | importado | `uso:equipable` `manos:una` | Low-poly con textura (estilo PS2) desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma plana); las medidas, las de la imagen. |
| `arma.distancia.arco_compuesto` | Arco compuesto reflejo | 0.1x1.2x0.05 | 150 | pendiente | `uso:equipable` `requiere:proyectil` `faccion:nomada` `manos:dos` | Asta, madera y tendón: corto y potente, a caballo tira como a pie. Se hace en tres etapas. |
| `arma.distancia.arco_largo` | Arco largo | 0.08x1.8x0.05 | 120 | pendiente | `uso:equipable` `requiere:proyectil` `manos:dos` |  |
| `arma.distancia.ballesta` | Ballesta | 0.7x0.25x0.9 | 250 | pendiente | `uso:equipable` `requiere:proyectil` `manos:dos` |  |
| `arma.distancia.mosquete` | Mosquete de mecha | 0.1x0.25x1.5 | 300 | pendiente | `uso:equipable` `requiere:proyectil` `faccion:imperio` `manos:dos` |  |
| `arma.distancia.canon` | Cañón de campaña | 1.5x1.2x3 | 800 | pendiente | `uso:equipable` `requiere:proyectil` `faccion:imperio` |  |
| `arma.distancia.honda` | Honda | 0.05x0.6x0.05 | 40 | pendiente | `uso:equipable` `requiere:proyectil` `manos:una` |  |
| `arma.distancia.lazo` | Lazo | 0.5x0.1x0.5 | 80 | pendiente | `uso:equipable` `faccion:nomada` `manos:una` |  |
| `arma.distancia.boleadoras` | Boleadoras | 0.4x0.1x0.4 | 60 | pendiente | `uso:equipable` `manos:una` |  |
| `arma.especial.guja_hoja_ancha` | Guja de hoja ancha | 0.25x2.2x0.05 | 300 | pendiente | `uso:equipable` `especial:gran_guerrero` `manos:dos` | Arma especial de grandes guerreros (src/sim/champion.c). |
| `arma.especial.sable_damasquinado` | Sable de acero damasquinado | 0.08x0.95x0.03 | 300 | pendiente | `uso:equipable` `especial:gran_guerrero` `manos:una` | Arma especial de grandes guerreros (src/sim/champion.c). |
| `arma.especial.arco_cuerno` | Arco compuesto de cuerno | 0.1x1.2x0.05 | 300 | pendiente | `uso:equipable` `especial:gran_guerrero` `manos:dos` | Arma especial de grandes guerreros (src/sim/champion.c). |
| `arma.especial.maza_tigre` | Maza de bronce con cabeza de tigre | 0.15x0.8x0.15 | 300 | pendiente | `uso:equipable` `especial:gran_guerrero` `manos:una` | Arma especial de grandes guerreros (src/sim/champion.c). |
| `arma.especial.lazo_plomadas` | Lazo con plomadas | 0.5x0.1x0.5 | 300 | pendiente | `uso:equipable` `especial:gran_guerrero` `manos:una` | Arma especial de grandes guerreros (src/sim/champion.c). |
| `arma.especial.hacha_ceremonial` | Hacha ceremonial | 0.3x0.9x0.04 | 300 | pendiente | `uso:equipable` `especial:gran_guerrero` `manos:una` | Arma especial de grandes guerreros (src/sim/champion.c). |
| `arma.especial.lanza_doble_punta` | Lanza de caballería de doble punta | 0.1x2.8x0.1 | 300 | pendiente | `uso:equipable` `especial:gran_guerrero` `manos:dos` | Arma especial de grandes guerreros (src/sim/champion.c). |
| `arma.especial.ballesta_repeticion` | Ballesta de repetición | 0.6x0.35x0.9 | 300 | pendiente | `uso:equipable` `especial:gran_guerrero` `manos:dos` | Arma especial de grandes guerreros (src/sim/champion.c). |
| `arma.especial.espada_padre` | Espada ceremonial del padre | 0.12x1x0.04 | 500 | pendiente | `uso:equipable` `acto:3` `especial:legado` `especial:narrativo` `material:oro` `manos:una` | Hallada en la tumba helada; se empuña en el asalto final. |
| `arma.corta.sable_bronce` | Sable de bronce | 0.08x0.9x0.03 | 120 | pendiente | `uso:equipable` `manos:una` `material:bronce` | Se forja en el horno de bronce. |
| `arma.corta.sable_acero` | Sable de acero | 0.08x0.95x0.03 | 120 | pendiente | `uso:equipable` `manos:una` `material:hierro` | Se forja en el horno de acero. |
| `arma.larga.lanza_bronce` | Lanza de punta de bronce | 0.1x2.4x0.1 | 80 | pendiente | `uso:equipable` `manos:una` `material:bronce` | Se forja en el horno de bronce. |
| `arma.distancia.arco_simple` | Arco simple | 0.08x1.4x0.04 | 120 | pendiente | `uso:equipable` `requiere:proyectil` `manos:dos` | De una pieza de madera: a caballo se abre el tiro. |
| `arma.corta.akinakes` | Akinakes | 0.07x0.6x0.03 | 100 | pendiente | `uso:equipable` `faccion:nomada` `manos:una` `material:bronce` | La espada corta recta de los escitas: rápida en los combos. |
| `arma.corta.sagaris` | Sagaris de pico | 0.2x0.7x0.04 | 110 | pendiente | `uso:equipable` `faccion:nomada` `manos:una` | Hacha de pico: perfora la armadura y engancha escudos. |
| `arma.corta.sagaris_martillo` | Sagaris de martillo | 0.2x0.7x0.06 | 110 | pendiente | `uso:equipable` `faccion:nomada` `manos:una` | Hacha de martillo: aturde y engancha escudos. |
| `arma.corta.daga_anillo` | Daga de anillo | 0.05x0.4x0.03 | 80 | pendiente | `uso:equipable` `faccion:nomada` `manos:una` `material:hierro` | La daga sármata: para la mano torpe, las llaves, el rehén y la ejecución. |
| `arma.larga.lanza_pesada` | Lanza pesada | 0.1x4x0.1 | 90 | pendiente | `uso:equipable` `faccion:nomada` `manos:dos` | Cuatro metros de asta: solo a caballo. En ristre derriba y rompe la formación. |
| `arma.corta.sable_wootz` | Sable de acero wootz | 0.08x0.95x0.03 | 120 | pendiente | `uso:equipable` `manos:una` `material:hierro` | El acero de crisol de las caravanas: el mejor filo. Se forja en el taller. |
| `arma.corta.sable_acero_templado` | Sable de acero templado | 0.08x0.95x0.03 | 120 | pendiente | `uso:equipable` `manos:una` `material:hierro` | Filo duro y lomo flexible: pega más. |
| `arma.corta.sable_acero_fragil` | Sable de acero frágil (mal templado) | 0.08x0.95x0.03 | 120 | pendiente | `uso:equipable` `manos:una` `material:hierro` | El temple salió mal: puede quebrarse. |
| `arma.corta.sable_wootz_templado` | Sable wootz templado | 0.08x0.95x0.03 | 120 | pendiente | `uso:equipable` `manos:una` `material:hierro` | El mejor sable de la estepa. |
| `arma.corta.sable_wootz_fragil` | Sable wootz frágil (mal templado) | 0.08x0.95x0.03 | 120 | pendiente | `uso:equipable` `manos:una` `material:hierro` | El temple salió mal: puede quebrarse. |
| `arma.corta.daga_anillo_templada` | Daga de anillo templada | 0.05x0.4x0.03 | 80 | pendiente | `uso:equipable` `faccion:nomada` `manos:una` `material:hierro` | Filo duro y lomo flexible: pega más. |
| `arma.corta.daga_anillo_fragil` | Daga de anillo frágil (mal templada) | 0.05x0.4x0.03 | 80 | pendiente | `uso:equipable` `faccion:nomada` `manos:una` `material:hierro` | El temple salió mal: puede quebrarse. |
| `arma.larga.lanza_acero` | Lanza de punta de acero | 0.1x2.4x0.1 | 80 | pendiente | `uso:equipable` `manos:una` `material:hierro` | Se forja en el taller del herrero. |

<a id="proyectil"></a>

## Proyectiles

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `proyectil.flecha.comun` | Flecha | 0.02x0.8x0.02 | 20 | pendiente | `uso:municion` |  |
| `proyectil.flecha.silbadora` | Flecha silbadora (señales) | 0.03x0.8x0.03 | 30 | pendiente | `uso:municion` `faccion:nomada` |  |
| `proyectil.virote.comun` | Virote de ballesta | 0.02x0.4x0.02 | 20 | pendiente | `uso:municion` |  |
| `proyectil.bala.mosquete` | Bala de mosquete | 0.02x0.02x0.02 | 8 | pendiente | `uso:municion` |  |
| `proyectil.bala.canon` | Bala de cañón | 0.15x0.15x0.15 | 20 | pendiente | `uso:municion` |  |
| `proyectil.piedra.honda` | Piedra de honda | 0.05x0.05x0.05 | 8 | pendiente | `uso:municion` |  |

<a id="escudo"></a>

## Escudos

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `escudo.mano.mimbre` | Escudo redondo de mimbre | 0.6x0.6x0.08 | 80 | pendiente | `uso:equipable` `faccion:nomada` `material:madera` `manos:escudo` |  |
| `escudo.mano.cuero` | Escudo de cuero endurecido | 0.55x0.55x0.06 | 80 | pendiente | `uso:equipable` `faccion:nomada` `material:cuero` `manos:escudo` |  |
| `escudo.mano.umbo` | Escudo de madera con umbo | 0.8x0.8x0.18 | 100 | importado | `uso:equipable` `faccion:neutral` `material:madera` `manos:escudo` | Low-poly con textura (estilo PS2) desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma disco); las medidas, las de la imagen. |
| `escudo.grande.paves` | Pavés imperial | 0.7x1.5x0.15 | 120 | pendiente | `uso:equipable` `faccion:imperio` `material:madera` `manos:escudo` |  |
| `escudo.mano.lamina` | Escudo de láminas de hierro | 0.51x0.8x0.21 | 120 | importado | `uso:equipable` `faccion:imperio` `material:hierro` `manos:escudo` | Low-poly con textura (estilo PS2) desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma disco); las medidas, las de la imagen. La hoja lo dibuja de tablas, con borde y umbo de hierro. |
| `escudo.mano.culto` | Escudo ceremonial del culto | 0.7x0.9x0.08 | 200 | pendiente | `uso:equipable` `faccion:culto` `material:bronce` `manos:escudo` |  |

<a id="totem"></a>

## Tótems

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `totem.clan.poste_clan` | Poste tótem del clan | 0.6x4x0.6 | 200 | pendiente | `faccion:nomada` `material:madera` |  |
| `totem.clan.lobo` | Tótem del lobo | 0.6x3x0.6 | 250 | pendiente | `faccion:nomada` `material:madera` | Bonificación de manada (pendiente de diseño). |
| `totem.clan.ciervo` | Tótem del ciervo | 0.8x3x0.6 | 250 | pendiente | `faccion:nomada` `material:madera` |  |
| `totem.clan.aguila` | Tótem del águila | 1.2x3.5x0.6 | 250 | pendiente | `faccion:nomada` `material:madera` |  |
| `totem.clan.tigre` | Tótem del tigre | 0.8x3x0.8 | 250 | pendiente | `faccion:nomada` `material:madera` |  |
| `totem.clan.sulde` | Sulde (estandarte de crines) | 0.4x3x0.4 | 150 | pendiente | `faccion:nomada` `uso:equipable` `material:crin` | Espíritu del clan; cola de caballo. |
| `totem.clan.estandarte_tamga` | Estandarte con la tamga del padre | 1x3.5x0.1 | 150 | pendiente | `faccion:nomada` `especial:legado` | Signo de clan del padre. |
| `totem.culto.idolo_culto` | Ídolo del culto | 1x3x1 | 400 | pendiente | `faccion:culto` `acto:4` `material:bronce` | Símbolo del culto: pendiente de definir por el autor. |
| `totem.culto.craneos_culto` | Pila de cráneos del culto | 1x1.5x1 | 200 | pendiente | `faccion:culto` `uso:decorado` `material:hueso` |  |
| `totem.proteccion.guardian` | Tótem de protección | 0.8x3.5x0.8 | 250 | pendiente | `faccion:nomada` `uso:interactuable` `uso:construible` `material:madera` | Construcción en grupo; protege el campamento (efecto pendiente de diseño). |

<a id="armadura"></a>

## Armaduras (por piezas)

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `armadura.casco.laminar_cuero` | Casco laminar de cuero | 0.25x0.35x0.27 | 150 | importado | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | Low-poly con textura (estilo PS2) desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma secciones); las medidas, las de la imagen. |
| `armadura.cuello.laminar_cuero` | Gorjal / cofia laminar de cuero | 0.35x0.2x0.35 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` |  |
| `armadura.torso.laminar_cuero` | Coraza / peto laminar de cuero | 0.5x0.6x0.35 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` |  |
| `armadura.hombreras.laminar_cuero` | Hombreras (par) laminar de cuero | 0.6x0.2x0.3 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` |  |
| `armadura.brazales.laminar_cuero` | Brazales (par) laminar de cuero | 0.15x0.3x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` |  |
| `armadura.guantes.laminar_cuero` | Guanteletes (par) laminar de cuero | 0.12x0.2x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` |  |
| `armadura.faldar.laminar_cuero` | Faldar laminar de cuero | 0.5x0.4x0.4 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` |  |
| `armadura.grebas.laminar_cuero` | Grebas (par) laminar de cuero | 0.15x0.45x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` |  |
| `armadura.botas.laminar_cuero` | Botas (par) laminar de cuero | 0.15x0.35x0.3 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` |  |
| `armadura.casco.fieltro` | Casco de fieltro acolchado | 0.3x0.35x0.3 | 150 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:fieltro` |  |
| `armadura.cuello.fieltro` | Gorjal / cofia de fieltro acolchado | 0.35x0.2x0.35 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:fieltro` |  |
| `armadura.torso.fieltro` | Coraza / peto de fieltro acolchado | 0.5x0.6x0.35 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:fieltro` |  |
| `armadura.hombreras.fieltro` | Hombreras (par) de fieltro acolchado | 0.6x0.2x0.3 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:fieltro` |  |
| `armadura.brazales.fieltro` | Brazales (par) de fieltro acolchado | 0.15x0.3x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:fieltro` |  |
| `armadura.faldar.fieltro` | Faldar de fieltro acolchado | 0.5x0.4x0.4 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:fieltro` |  |
| `armadura.botas.fieltro` | Botas (par) de fieltro acolchado | 0.15x0.35x0.3 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:fieltro` |  |
| `armadura.casco.escamas_hierro` | Casco de escamas de hierro | 0.18x0.35x0.18 | 150 | importado | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` | Low-poly con textura (estilo PS2) desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma secciones); las medidas, las de la imagen. La hoja lo dibuja cónico, con nasal y carrilleras. |
| `armadura.cuello.escamas_hierro` | Gorjal / cofia de escamas de hierro | 0.35x0.2x0.35 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` |  |
| `armadura.torso.escamas_hierro` | Coraza / peto de escamas de hierro | 0.5x0.6x0.35 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` |  |
| `armadura.hombreras.escamas_hierro` | Hombreras (par) de escamas de hierro | 0.6x0.2x0.3 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` |  |
| `armadura.brazales.escamas_hierro` | Brazales (par) de escamas de hierro | 0.15x0.3x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` |  |
| `armadura.guantes.escamas_hierro` | Guanteletes (par) de escamas de hierro | 0.12x0.2x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` |  |
| `armadura.faldar.escamas_hierro` | Faldar de escamas de hierro | 0.5x0.4x0.4 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` |  |
| `armadura.grebas.escamas_hierro` | Grebas (par) de escamas de hierro | 0.15x0.45x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` |  |
| `armadura.botas.escamas_hierro` | Botas (par) de escamas de hierro | 0.15x0.35x0.3 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` `material:hierro` |  |
| `armadura.casco.malla` | Casco de malla | 0.26x0.35x0.28 | 150 | importado | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` | Low-poly con textura (estilo PS2) desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma secciones); las medidas, las de la imagen. La hoja lo dibuja como casco de anteojos (nasal y antiparras). |
| `armadura.cuello.malla` | Gorjal / cofia de malla | 0.35x0.2x0.35 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` |  |
| `armadura.torso.malla` | Coraza / peto de malla | 0.5x0.6x0.35 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` |  |
| `armadura.hombreras.malla` | Hombreras (par) de malla | 0.6x0.2x0.3 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` |  |
| `armadura.brazales.malla` | Brazales (par) de malla | 0.15x0.3x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` |  |
| `armadura.guantes.malla` | Guanteletes (par) de malla | 0.12x0.2x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` |  |
| `armadura.faldar.malla` | Faldar de malla | 0.5x0.4x0.4 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` |  |
| `armadura.grebas.malla` | Grebas (par) de malla | 0.15x0.45x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` |  |
| `armadura.botas.malla` | Botas (par) de malla | 0.15x0.35x0.3 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:neutral` `material:hierro` |  |
| `armadura.casco.culto` | Casco ritual del culto | 0.3x0.35x0.3 | 150 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.cuello.culto` | Gorjal / cofia ritual del culto | 0.35x0.2x0.35 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.torso.culto` | Coraza / peto ritual del culto | 0.5x0.6x0.35 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.hombreras.culto` | Hombreras (par) ritual del culto | 0.6x0.2x0.3 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.brazales.culto` | Brazales (par) ritual del culto | 0.15x0.3x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.guantes.culto` | Guanteletes (par) ritual del culto | 0.12x0.2x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.faldar.culto` | Faldar ritual del culto | 0.5x0.4x0.4 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.grebas.culto` | Grebas (par) ritual del culto | 0.15x0.45x0.15 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.botas.culto` | Botas (par) ritual del culto | 0.15x0.35x0.3 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.casco.mascara_culto` | Máscara ritual del culto | 0.25x0.3x0.2 | 200 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` `material:bronce` |  |
| `armadura.casco.oro_padre` | Casco de oro del padre | 0.3x0.4x0.3 | 300 | pendiente | `uso:equipable` `rig:humanoide` `especial:legado` `material:oro` `acto:1` | Lo lleva el padre en el prólogo. |
| `armadura.montura.barda_cuero` | Barda de cuero para caballo | 0.8x1.2x2 | 400 | pendiente | `uso:equipable` `rig:cuadrupedo` `faccion:nomada` `material:cuero` |  |
| `armadura.montura.barda_hierro` | Barda de hierro para caballo | 0.8x1.2x2 | 500 | pendiente | `uso:equipable` `rig:cuadrupedo` `faccion:imperio` `material:hierro` |  |
| `armadura.casco.bronce` | Casco de bronce | 0.17x0.35x0.26 | 150 | importado | `uso:equipable` `rig:humanoide` `material:bronce` | Se forja en el horno de bronce. Low-poly con textura (estilo PS2) desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma secciones --cresta); las medidas, las de la imagen. |

<a id="vestimenta"></a>

## Vestimenta (personalización)

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `vestimenta.torso.deel` | Deel (caftán cruzado) | 0.6x1.2x0.4 | 300 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.torso.deel_invierno` | Deel de invierno con piel | 0.7x1.3x0.5 | 350 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.cabeza.gorro_piel` | Gorro de piel | 0.3x0.25x0.3 | 100 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.cabeza.gorro_punta` | Gorro de fieltro de punta | 0.3x0.4x0.3 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.piernas.pantalon` | Pantalón de montar | 0.4x0.9x0.3 | 150 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.pies.botas_fieltro` | Botas de fieltro | 0.15x0.4x0.3 | 100 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.espalda.capa` | Capa de lana | 0.7x1.2x0.4 | 200 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.espalda.capa_piel` | Capa de piel de lobo | 0.8x1.2x0.5 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.cintura.cinturon` | Cinturón con placa de estilo animal | 0.4x0.08x0.3 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.cuello.bufanda` | Bufanda / velo contra la arena | 0.3x0.3x0.3 | 60 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.torso.tunica_campesina` | Túnica campesina | 0.6x1.1x0.4 | 200 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` | Personalización del personaje (Fase 3). |
| `vestimenta.torso.uniforme_imperial` | Uniforme imperial | 0.6x1.4x0.4 | 300 | pendiente | `uso:equipable` `rig:humanoide` `faccion:imperio` | Personalización del personaje (Fase 3). |
| `vestimenta.torso.tunica_culto` | Túnica del culto | 0.7x1.6x0.5 | 300 | pendiente | `uso:equipable` `rig:humanoide` `faccion:culto` | Personalización del personaje (Fase 3). |
| `vestimenta.torso.harapos` | Harapos de prisionero | 0.6x1.1x0.4 | 150 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` | Personalización del personaje (Fase 3). |
| `vestimenta.cabeza.sombrero` | Sombrero de ala ancha | 0.45x0.15x0.45 | 80 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:fieltro` | Da sombra: contra el sol y el calor. |
| `vestimenta.cabeza.gorro_lobo` | Gorro de cabeza de lobo | 0.3x0.3x0.35 | 120 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | Abriga y escarmienta: el enemigo se lo piensa. |
| `vestimenta.cuello.panuelo_desierto` | Pañuelo del desierto | 0.3x0.2x0.3 | 60 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:seda` | Cubre cara y cuello del sol y la arena. |
| `vestimenta.cuello.bufanda_piel` | Bufanda de piel de zorro | 0.3x0.2x0.3 | 60 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | De piel de coyote. |
| `vestimenta.torso.tunica_seda` | Túnica de seda blanca | 0.6x1.2x0.35 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:seda` | Fresca y blanca: refleja el sol. |
| `vestimenta.torso.tunica_lana` | Túnica de lana | 0.6x1.2x0.4 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:lana` |  |
| `vestimenta.espalda.abrigo_oso` | Abrigo de piel de oso | 0.9x1.3x0.6 | 300 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | El más abrigado; pesa. Escarmienta mucho. |
| `vestimenta.espalda.abrigo_tigre` | Abrigo de piel de tigre | 0.9x1.3x0.5 | 300 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | Escarmienta más que ninguno. |
| `vestimenta.espalda.capa_puma` | Capa de piel de puma | 0.8x1.2x0.5 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | Escarmienta. |
| `vestimenta.espalda.capa_hiena` | Capa de piel de hiena | 0.8x1.2x0.5 | 250 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | Escarmienta un poco. |
| `vestimenta.espalda.abrigo_reno` | Abrigo de piel de reno | 0.9x1.3x0.6 | 300 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | Muy abrigado. |
| `vestimenta.espalda.abrigo_cabra` | Abrigo de lana de cabra | 0.9x1.3x0.6 | 300 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:lana` |  |
| `vestimenta.espalda.manto_blanco` | Manto blanco del desierto | 0.8x1.3x0.4 | 220 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:seda` | Sombra para todo el cuerpo. |
| `vestimenta.pies.botas_piel` | Botas de piel de reno | 0.15x0.4x0.3 | 100 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | Abrigan y no se mojan. |
| `vestimenta.pies.sandalias` | Sandalias de cuero | 0.12x0.08x0.28 | 60 | pendiente | `uso:equipable` `rig:humanoide` `faccion:nomada` `material:cuero` | Frescas. |

<a id="accesorio"></a>

## Accesorios: amuletos, tatuajes, arreos y mensajes

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `accesorio.amuleto.lobo` | Amuleto del lobo | 0.08x0.06x0.01 | 80 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Placa de estilo animal; buffs en src/sim/loadout.c. |
| `accesorio.amuleto.ciervo` | Amuleto del ciervo | 0.08x0.06x0.01 | 80 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Placa de estilo animal; buffs en src/sim/loadout.c. |
| `accesorio.amuleto.aguila_ibice` | Amuleto del águila y el íbice | 0.08x0.06x0.01 | 80 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Placa de estilo animal; buffs en src/sim/loadout.c. |
| `accesorio.amuleto.tigre_dragon` | Amuleto del tigre y el dragón | 0.08x0.06x0.01 | 80 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Placa de estilo animal; buffs en src/sim/loadout.c. |
| `accesorio.amuleto.oso` | Amuleto del oso | 0.08x0.06x0.01 | 80 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Placa de estilo animal; buffs en src/sim/loadout.c. |
| `accesorio.amuleto.caballo` | Amuleto del caballo | 0.08x0.06x0.01 | 80 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Placa de estilo animal; buffs en src/sim/loadout.c. |
| `accesorio.tatuaje.lobo` | Tatuaje del lobo | 0.25x0.25x0 | 0 | pendiente | `uso:equipable` `formato:textura` `estilo:animal` | Calcomanía (PNG con alfa), no modelo. Permanente. |
| `accesorio.tatuaje.ciervo` | Tatuaje del ciervo | 0.25x0.25x0 | 0 | pendiente | `uso:equipable` `formato:textura` `estilo:animal` | Calcomanía (PNG con alfa), no modelo. Permanente. |
| `accesorio.tatuaje.grifo` | Tatuaje del grifo | 0.25x0.25x0 | 0 | pendiente | `uso:equipable` `formato:textura` `estilo:animal` | Calcomanía (PNG con alfa), no modelo. Permanente. |
| `accesorio.tatuaje.tamga` | Tatuaje de la tamga | 0.25x0.25x0 | 0 | pendiente | `uso:equipable` `formato:textura` `estilo:animal` | Calcomanía (PNG con alfa), no modelo. Permanente. |
| `accesorio.tatuaje.olas` | Tatuaje de olas | 0.25x0.25x0 | 0 | pendiente | `uso:equipable` `formato:textura` `estilo:animal` | Calcomanía (PNG con alfa), no modelo. Permanente. |
| `accesorio.arreo.silla_montar` | Silla de montar | 0.5x0.3x0.6 | 200 | pendiente | `uso:equipable` `faccion:nomada` `material:cuero` |  |
| `accesorio.arreo.brida` | Brida y riendas | 0.3x0.4x0.6 | 100 | pendiente | `uso:equipable` `material:cuero` |  |
| `accesorio.arreo.estribos` | Estribos (par) | 0.15x0.2x0.1 | 60 | pendiente | `uso:equipable` `material:hierro` |  |
| `accesorio.arreo.alforjas` | Alforjas | 0.6x0.4x0.3 | 120 | pendiente | `uso:equipable` `material:cuero` |  |
| `accesorio.arreo.placa_arreo_padre` | Placa de arreo de estilo animal del padre | 0.12x0.08x0.01 | 120 | pendiente | `uso:equipable` `especial:rastro_padre` `material:oro` `estilo:animal` | Rastros del padre (acto III). |
| `accesorio.mensaje.piedra_runica` | Piedra con mensaje cifrado | 0.5x0.8x0.2 | 120 | pendiente | `acto:3` `especial:rastro_padre` `material:piedra` | Mensajes en las antiguas lenguas de la estepa. |
| `accesorio.mensaje.tablilla` | Tablilla de madera cifrada | 0.2x0.3x0.02 | 60 | pendiente | `acto:3` `especial:rastro_padre` `material:madera` |  |
| `accesorio.anillo.bronce` | Anillo de bronce | 0.025x0.025x0.008 | 40 | pendiente | `uso:equipable` `material:bronce` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.anillo.plata` | Anillo de plata | 0.025x0.025x0.008 | 40 | pendiente | `uso:equipable` `material:plata` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.anillo.oro` | Anillo de oro | 0.025x0.025x0.008 | 40 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.brazalete.bronce` | Brazalete de bronce | 0.08x0.03x0.08 | 60 | pendiente | `uso:equipable` `material:bronce` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.brazalete.plata` | Brazalete de plata | 0.08x0.03x0.08 | 60 | pendiente | `uso:equipable` `material:plata` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.brazalete.oro` | Brazalete de oro | 0.08x0.03x0.08 | 60 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.collar.bronce` | Collar de bronce | 0.2x0.25x0.02 | 80 | pendiente | `uso:equipable` `material:bronce` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.collar.plata` | Collar de plata | 0.2x0.25x0.02 | 80 | pendiente | `uso:equipable` `material:plata` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.collar.oro` | Collar de oro | 0.2x0.25x0.02 | 80 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.aretes.bronce` | Aretes de bronce | 0.03x0.05x0.01 | 40 | pendiente | `uso:equipable` `material:bronce` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.aretes.plata` | Aretes de plata | 0.03x0.05x0.01 | 40 | pendiente | `uso:equipable` `material:plata` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.aretes.oro` | Aretes de oro | 0.03x0.05x0.01 | 40 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.hebilla.bronce` | Hebilla de cinturón de bronce | 0.08x0.06x0.015 | 60 | pendiente | `uso:equipable` `material:bronce` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.hebilla.plata` | Hebilla de cinturón de plata | 0.08x0.06x0.015 | 60 | pendiente | `uso:equipable` `material:plata` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |
| `accesorio.hebilla.oro` | Hebilla de cinturón de oro | 0.08x0.06x0.015 | 60 | pendiente | `uso:equipable` `material:oro` `estilo:animal` | Joya: la hace un orfebre con metal y piedra; un druida la encanta (src/sim/jewelry.c). |

<a id="asedio"></a>

## Maquinaria de asedio

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `asedio.maquina.ariete` | Ariete cubierto | 3x3x6 | 500 | pendiente | `faccion:imperio` `acto:1` `uso:conducible` `uso:destructible` | La maquinaria que aplasta al padre en el prólogo. |
| `asedio.maquina.torre_asedio` | Torre de asedio | 5x12x5 | 1000 | pendiente | `faccion:imperio` `acto:1` `uso:conducible` `uso:destructible` | La maquinaria que aplasta al padre en el prólogo. |
| `asedio.maquina.trebuchet` | Trebuchet | 5x10x8 | 900 | pendiente | `faccion:imperio` `acto:1` `uso:conducible` `uso:destructible` | La maquinaria que aplasta al padre en el prólogo. |
| `asedio.maquina.catapulta` | Catapulta | 3x3x4 | 600 | pendiente | `faccion:imperio` `acto:1` `uso:conducible` `uso:destructible` | La maquinaria que aplasta al padre en el prólogo. |
| `asedio.maquina.mantelete` | Mantelete | 2.5x2x0.3 | 100 | pendiente | `faccion:imperio` `acto:1` `uso:conducible` `uso:destructible` | La maquinaria que aplasta al padre en el prólogo. |

<a id="utileria"></a>

## Utilería y consumibles

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `utileria.objeto.caldero` | Caldero | 0.54x0.5x0.58 | 80 | importado | `faccion:nomada` `material:hierro` `formato:voxel` | Voxeles armados a mano (tools/voxeles/estructuras.py caldero), celdas de 2 cm: el caldero escita de bronce, la olla honda sobre su pie de cono hueco, las dos asas de pie sobre el borde y el guiso adentro, con la patina oscura y verde. |
| `utileria.objeto.odre` | Odre de cuero | 0.32x0.4x0.18 | 60 | importado | `uso:recolectable` `material:cuero` `formato:voxel` | Voxeles armados a mano (tools/voxeles/estructuras.py odre), celdas de 2 cm: la bolsa de cuero con su costura, el cuello atado con un cordel y la correa de colgar. |
| `utileria.objeto.cofre` | Cofre de botín | 0.8x0.5x0.5 | 100 | pendiente | `uso:interactuable` `material:madera` | Se abre y se cierra (assets/animaciones.tsv: abrir y cerrar), asi que va con un GLB animado. La forma ya esta en tools/voxeles/estructuras.py cofre (la tapa abombada, dos flejes de hierro y la cerradura de bronce al frente). |
| `utileria.objeto.barril_polvora` | Barril de pólvora | 0.6x0.8x0.6 | 80 | pendiente | `faccion:imperio` `uso:destructible` |  |
| `utileria.objeto.saco_grano` | Saco de grano | 0.5x0.6x0.4 | 60 | pendiente | `uso:recolectable` |  |
| `utileria.objeto.antorcha` | Antorcha | 0.1x0.7x0.1 | 40 | pendiente | `uso:equipable` `manos:una` |  |
| `utileria.objeto.cadenas` | Grilletes y cadenas | 0.4x0.1x0.4 | 80 | pendiente | `material:hierro` |  |
| `utileria.objeto.yunque` | Yunque | 0.6x0.5x0.3 | 80 | pendiente | `material:hierro` |  |
| `utileria.objeto.telar` | Telar de fieltro | 1.5x1.2x1 | 150 | pendiente | `faccion:nomada` `material:madera` |  |
| `utileria.objeto.tamboril_chaman` | Tambor chamánico | 0.5x0.1x0.5 | 100 | pendiente | `faccion:nomada` `material:cuero` |  |
| `utileria.objeto.vela` | Velas de sebo | 0.05x0.15x0.05 | 20 | pendiente | `uso:recolectable` | De la grasa animal: luz para las noches largas y bien de trueque. |
| `utileria.objeto.peine` | Peine de hueso | 0.1x0.02x0.08 | 20 | pendiente | `uso:recolectable` `material:hueso` | Labrado en hueso: bien de trueque. |
| `utileria.objeto.aljaba` | Aljaba de cuero | 0.15x0.7x0.15 | 60 | pendiente | `uso:recolectable` `material:cuero` | Para las flechas; se paga bien en la estepa. |
| `utileria.consumible.carne_seca` | Carne seca | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` |  |
| `utileria.consumible.airag` | Odre de airag (leche de yegua) | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` |  |
| `utileria.consumible.queso_seco` | Queso seco | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` |  |
| `utileria.consumible.hierbas` | Hierbas curativas | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` |  |
| `utileria.consumible.agua` | Agua cruda | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` |  |
| `utileria.consumible.pan` | Pan del imperio | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` |  |
| `utileria.consumible.carne_fresca` | Carne fresca | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` | De la caza o el sacrificio; se come antes que la seca. |
| `utileria.consumible.leche` | Leche | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` | Del ordeño diario del ganado. |
| `utileria.consumible.miel` | Miel silvestre | 0.15x0.15x0.15 | 40 | pendiente | `uso:recolectable` `uso:consumible` | De las colmenas, con humo. |
| `utileria.consumible.unguento` | Ungüento de hierbas y miel | 0.1x0.1x0.1 | 30 | pendiente | `uso:recolectable` `uso:consumible` | Cura más que las hierbas: corta el veneno y cierra las heridas. |
| `utileria.consumible.especias` | Especias | 0.15x0.1x0.15 | 20 | pendiente | `uso:recolectable` `uso:consumible` | De las caravanas del desierto: valen su peso en plata. |
| `utileria.herramienta.pala` | Pala | 0.25x1.2x0.05 | 60 | pendiente | `uso:equipable` `manos:dos` `material:hierro` | Para cavar trincheras. |
| `utileria.herramienta.picahielo` | Picahielo | 0.12x1.5x0.06 | 60 | pendiente | `uso:equipable` `manos:dos` `material:hierro` | Un asta con una punta de hierro en cincel: abre un agujero en el hielo para pescar (L → Acciones). |
| `utileria.herramienta.gancho_trepa` | Trepa (gancho con cuerda) | 0.3x0.4x0.3 | 80 | pendiente | `uso:equipable` `manos:una` `material:hierro` | Se lanza para escalar muros en los asedios. |
| `utileria.herramienta.escala_asedio` | Escala de asedio | 0.5x2.9x0.15 | 120 | pendiente | `uso:herramienta` `material:madera` | En dos tramos que se atan al pie de la muralla (12 kg). F junto a la muralla de una ciudad en guerra: se apoya y se sube por ella, de ida y de vuelta. |
| `utileria.herramienta.forja_movil` | Forja móvil (fuelle y hogar) | 0.6x0.5x0.6 | 120 | pendiente | `uso:herramienta` `material:hierro` | Fuelle de cuero y hogar de carbón (10 kg): en la mochila, las alforjas, la carreta o el acopio, repara y hace puntas, cuchillos y sables en cualquier sitio. |
| `utileria.objeto.lena` | Haz de leña | 0.46x0.22x0.82 | 40 | importado | `uso:recolectable` `material:madera` `formato:voxel` | Material de fogatas, hogueras y hornos. Voxeles armados a mano (tools/voxeles/estructuras.py lena), celdas de 2 cm: los restos de madera lavada de las fotos de referencia del usuario, dos ramas gruesas grises y pardas con su veta, nudos y los cortes claros, cruzadas sobre una mas fina. |
| `utileria.material.troncos` | Troncos | 0.4x0.4x2 | 40 | pendiente | `uso:recolectable` `uso:material` `material:madera` | Material del acopio de la tribu. |
| `utileria.material.piedra` | Piedras | 0.4x0.33x0.48 | 30 | importado | `uso:recolectable` `uso:material` `material:piedra` `formato:voxel` | Material del acopio de la tribu. Voxeles armados a mano (tools/voxeles/estructuras.py piedras), celdas de 2.5 cm: un monton de cinco piedras de campo grises y pardas, facetadas. |
| `utileria.material.barro` | Barro (adobe) | 0.36x0.3x0.4 | 30 | importado | `uso:recolectable` `uso:material` `material:barro` `formato:voxel` | Material del acopio de la tribu. Voxeles armados a mano (tools/voxeles/estructuras.py barro), celdas de 2 cm: la loma de barro rojizo, mas oscuro donde esta mojado, con dos adobes cortados al costado. |
| `utileria.material.pieles` | Pieles curtidas | 0.6x0.2x0.8 | 40 | pendiente | `uso:recolectable` `uso:material` `material:cuero` | Material del acopio de la tribu. |
| `utileria.material.cuerda` | Rollo de cuerda | 0.3x0.15x0.3 | 40 | pendiente | `uso:recolectable` `uso:material` | Material del acopio de la tribu. |
| `utileria.material.carbon` | Carbón vegetal | 0.4x0.3x0.4 | 30 | pendiente | `uso:recolectable` `uso:material` | Material del acopio de la tribu. |
| `utileria.material.cobre` | Mineral de cobre | 0.25x0.2x0.25 | 30 | pendiente | `uso:recolectable` `uso:material` `material:cobre` | Material del acopio de la tribu. |
| `utileria.material.estano` | Mineral de estaño | 0.25x0.2x0.25 | 30 | pendiente | `uso:recolectable` `uso:material` `material:estaño` | Material del acopio de la tribu. |
| `utileria.material.hierro` | Mineral de hierro | 0.25x0.2x0.25 | 30 | pendiente | `uso:recolectable` `uso:material` `material:hierro` | Material del acopio de la tribu. |
| `utileria.material.plumas` | Plumas | 0.2x0.05x0.3 | 20 | pendiente | `uso:recolectable` `uso:material` | De las aves cazadas: para emplumar flechas. |
| `utileria.material.hueso` | Huesos | 0.3x0.1x0.3 | 30 | pendiente | `uso:recolectable` `uso:material` `material:hueso` | Del despiece: puntas de virote. |
| `utileria.material.tendones` | Tendones | 0.2x0.05x0.3 | 20 | pendiente | `uso:recolectable` `uso:material` | Del despiece: cuerda. |
| `utileria.material.grasa` | Grasa animal | 0.2x0.1x0.2 | 20 | pendiente | `uso:recolectable` `uso:material` | De la caza (más en otoño): sebo para lámparas, ungüentos y para engrasar el cuero. |
| `utileria.material.cuero` | Cuero | 0.6x0.1x0.8 | 40 | pendiente | `uso:recolectable` `uso:material` `material:cuero` | De curtir y trabajar las pieles (talabartería): arreos, sillas, aljabas y armaduras. |
| `utileria.material.fieltro` | Fieltro | 0.6x0.1x0.8 | 40 | pendiente | `uso:recolectable` `uso:material` `material:fieltro` | De abatanar la lana de cabra: chozas, gorros y botas. |
| `utileria.material.madera_labrada` | Madera labrada | 0.3x0.2x1.5 | 40 | pendiente | `uso:recolectable` `uso:material` `material:madera` | Troncos desbastados y escuadrados: sillas, arcos y casas que duran. |
| `utileria.material.adobe` | Ladrillos de adobe | 0.4x0.2x0.3 | 30 | pendiente | `uso:recolectable` `uso:material` `material:barro` | Barro y paja secados al sol: casas que duran. |
| `utileria.material.puntas_hueso` | Puntas de hueso | 0.1x0.05x0.1 | 20 | pendiente | `uso:recolectable` `uso:material` `material:hueso` | Para las flechas, cuando no hay pedernal. |
| `utileria.material.piezas_arco` | Piezas de arco (asta y tendón) | 0.1x0.05x0.6 | 30 | pendiente | `uso:recolectable` `uso:material` `material:hueso` | Láminas de asta y tendón: el arco compuesto. |
| `utileria.material.alma_arco` | Alma de arco (madera) | 0.05x0.05x1.1 | 30 | pendiente | `uso:material` `material:madera` | La primera etapa del arco compuesto reflejo. |
| `utileria.material.cola_hueso` | Cola de hueso | 0.1x0.1x0.1 | 20 | pendiente | `uso:material` `material:hueso` | Huesos hervidos: pega el asta y el tendón del arco. |
| `utileria.material.arco_encolado` | Arco encolado (secando: faltan 2 días) | 0.1x0.05x1.1 | 40 | pendiente | `uso:material` `material:madera` | Se seca en el acopio de un campamento. |
| `utileria.material.arco_oreado` | Arco encolado (secando: falta 1 día) | 0.1x0.05x1.1 | 40 | pendiente | `uso:material` `material:madera` | Se seca en el acopio de un campamento. |
| `utileria.material.arco_seco` | Arco seco (sin terminar) | 0.1x0.05x1.1 | 40 | pendiente | `uso:material` `material:madera` | Falta la cola de hueso que lo termina. |
| `utileria.material.mineral_hierro` | Mineral de hierro | 0.3x0.2x0.3 | 30 | pendiente | `uso:recolectable` `uso:material` `material:hierro` | De las vetas del altiplano o del pantano: se funde en el horno de acero. |
| `utileria.material.acero_wootz` | Acero wootz (torta) | 0.12x0.04x0.12 | 20 | pendiente | `uso:material` `material:hierro` | El acero de crisol que traen las caravanas: el mejor sable. |
| `utileria.material.hoja_acero` | Hoja de sable de acero | 0.05x0.8x0.02 | 40 | pendiente | `uso:material` `material:hierro` | Hoja terminada de la capital: con la forja móvil se le pone empuñadura. |
| `utileria.material.puntas_hierro` | Puntas de flecha de hierro | 0.03x0.06x0.02 | 10 | pendiente | `uso:material` `material:hierro` | De la forja móvil: más flechas por tanda. |
| `utileria.material.pedernal` | Pedernal | 0.12x0.09x0.15 | 30 | importado | `uso:recolectable` `uso:material` `material:piedra` `formato:voxel` | Se recoge en la estepa: puntas de flecha. Voxeles armados a mano (tools/voxeles/estructuras.py pedernal), celdas de 1 cm: el nodulo casi negro de la foto de referencia del usuario, con facetas de lascado mas claras, la corteza blanca en una o dos caras y dos vetas blancas. |
| `utileria.gema.turquesa` | Turquesa | 0.03x0.02x0.03 | 20 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Piedra preciosa: rocas, corales, excavaciones y ríos. |
| `utileria.gema.cornalina` | Cornalina | 0.03x0.02x0.03 | 20 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Piedra preciosa: rocas, corales, excavaciones y ríos. |
| `utileria.gema.lapislazuli` | Lapislázuli | 0.03x0.02x0.03 | 20 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Piedra preciosa: rocas, corales, excavaciones y ríos. |
| `utileria.gema.ambar` | Ámbar | 0.03x0.02x0.03 | 20 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Piedra preciosa: rocas, corales, excavaciones y ríos. |
| `utileria.gema.jade` | Jade | 0.03x0.02x0.03 | 20 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Piedra preciosa: rocas, corales, excavaciones y ríos. |
| `utileria.gema.granate` | Granate | 0.03x0.02x0.03 | 20 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Piedra preciosa: rocas, corales, excavaciones y ríos. |
| `utileria.gema.perla` | Perla | 0.03x0.02x0.03 | 20 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Piedra preciosa: rocas, corales, excavaciones y ríos. |
| `utileria.gema.coral` | Coral rojo | 0.03x0.02x0.03 | 20 | pendiente | `uso:recolectable` `uso:material` `material:piedra` | Piedra preciosa: rocas, corales, excavaciones y ríos. |
| `utileria.material.bronce` | Lingote de bronce | 0.2x0.05x0.08 | 20 | pendiente | `uso:material` `material:bronce` | Metal de joyas y forja. |
| `utileria.material.plata` | Plata | 0.15x0.05x0.08 | 20 | pendiente | `uso:recolectable` `uso:material` `material:plata` | Metal de joyas. |
| `utileria.material.oro` | Oro | 0.12x0.04x0.07 | 20 | pendiente | `uso:recolectable` `uso:material` `material:oro` | Metal de joyas. |
| `utileria.consumible.forraje` | Forraje | 0.4x0.3x0.4 | 30 | pendiente | `uso:recolectable` `uso:consumible` `material:fibra` | Hierba seca para los herbívoros de la tribu cuando no hay pasto (se corta de la hierba alta; los pastores lo juntan). |
| `utileria.mochila.pequena` | Mochila pequeña | 0.35x0.4x0.2 | 80 | pendiente | `uso:equipable` `material:cuero` | 15 kg, 12 huecos; no frena. |
| `utileria.mochila.mediana` | Mochila mediana | 0.4x0.55x0.25 | 100 | pendiente | `uso:equipable` `material:cuero` | 25 kg, 18 huecos; frena un poco. |
| `utileria.mochila.grande` | Mochila grande | 0.5x0.75x0.3 | 120 | pendiente | `uso:equipable` `material:cuero` | 40 kg, 28 huecos; frena (se puede dejar en el suelo para pelear). |
| `utileria.piel.lobo` | Piel de lobo | 0.6x0.1x1.0 | 40 | pendiente | `uso:recolectable` `uso:material` `material:cuero` | Del despiece. Capas y gorros que escarmientan. |
| `utileria.piel.oso` | Piel de oso | 0.9x0.15x1.4 | 40 | pendiente | `uso:recolectable` `uso:material` `material:cuero` | Del despiece. |
| `utileria.piel.tigre` | Piel de tigre | 0.8x0.1x1.3 | 40 | pendiente | `uso:recolectable` `uso:material` `material:cuero` | Del despiece. |
| `utileria.piel.puma` | Piel de puma | 0.7x0.1x1.1 | 40 | pendiente | `uso:recolectable` `uso:material` `material:cuero` | Del despiece. |
| `utileria.piel.hiena` | Piel de hiena | 0.6x0.1x1.0 | 40 | pendiente | `uso:recolectable` `uso:material` `material:cuero` | Del despiece. |
| `utileria.piel.coyote` | Piel de coyote | 0.5x0.1x0.8 | 40 | pendiente | `uso:recolectable` `uso:material` `material:cuero` | Del despiece. |
| `utileria.piel.reno` | Piel de reno | 0.8x0.15x1.2 | 40 | pendiente | `uso:recolectable` `uso:material` `material:cuero` | Del despiece: abrigos y botas. |
| `utileria.piel.cabra` | Lana de cabra | 0.5x0.2x0.5 | 40 | pendiente | `uso:recolectable` `uso:material` `material:lana` | De las cabras (despiece y esquila de los pastores): fieltro, lana. |
| `utileria.material.seda` | Seda | 0.4x0.1x0.3 | 30 | pendiente | `uso:recolectable` `uso:material` `material:seda` | De las caravanas del imperio (botín). |
| `utileria.consumible.agua_hervida` | Agua hervida | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` | Hervir el agua (L, junto a un fuego) echa a los espíritus malditos. |
| `utileria.consumible.agua_vino` | Agua con vino | 0.2x0.15x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` | El vino purifica el agua: segura y apenas achispa. |
| `utileria.consumible.cerveza` | Cerveza | 0.2x0.25x0.2 | 40 | pendiente | `uso:recolectable` `uso:consumible` | Segura de beber; en exceso, torpe y fatigado. |
| `utileria.consumible.vino` | Vino | 0.12x0.3x0.12 | 40 | pendiente | `uso:recolectable` `uso:consumible` | De las caravanas del imperio. Seguro; emborracha pronto. |
| `utileria.objeto_generico.ea01_env_branch_01` | Ea01 Env Branch 01 | 2.41x0.97x4.77 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_branch_02` | Ea01 Env Branch 02 | 2.17x1.57x4.10 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_branch_03` | Ea01 Env Branch 03 | 2.06x1.90x4.40 | 798 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_branch_04` | Ea01 Env Branch 04 | 1.71x0.62x1.54 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). |
| `utileria.objeto_generico.ea01_env_road_bend_01a` | Ea01 Env Road Bend 01A | 12.00x1.04x12.09 | 229 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_bend_01b` | Ea01 Env Road Bend 01B | 12.00x1.04x12.09 | 229 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_bend_01c` | Ea01 Env Road Bend 01C | 12.09x0.73x12.00 | 231 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_bend_01d` | Ea01 Env Road Bend 01D | 12.09x0.73x12.00 | 231 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_cobble_02` | Ea01 Env Road Cobble 02 | 0.55x1.47x1.40 | 87 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_cobble_02b` | Ea01 Env Road Cobble 02B | 0.55x1.81x1.63 | 105 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_cobble_04` | Ea01 Env Road Cobble 04 | 4.13x28.76x29.14 | 299 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_cobble_04b` | Ea01 Env Road Cobble 04B | 4.09x0.64x4.08 | 300 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_cobble_stair_01b` | Ea01 Env Road Cobble Stair 01B | 4.41x5.42x12.66 | 283 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_cobble_stair_01c` | Ea01 Env Road Cobble Stair 01C | 3.90x3.95x9.91 | 299 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_seam_01` | Ea01 Env Road Seam 01 | 8.00x0.84x8.26 | 136 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_winding_01a` | Ea01 Env Road Winding 01A | 16.00x1.78x22.65 | 296 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_winding_01b` | Ea01 Env Road Winding 01B | 32.00x2.13x90.59 | 562 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_winding_01c` | Ea01 Env Road Winding 01C | 16.00x1.40x22.65 | 296 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_winding_02a` | Ea01 Env Road Winding 02A | 16.00x1.02x22.65 | 268 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_winding_02c` | Ea01 Env Road Winding 02C | 16.00x1.02x22.65 | 268 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_wooden_beam_01a` | Ea01 Env Road Wooden Beam 01A | 6.00x0.94x24.04 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_wooden_beam_01b` | Ea01 Env Road Wooden Beam 01B | 3.00x0.47x6.07 | 790 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_wooden_beam_02a` | Ea01 Env Road Wooden Beam 02A | 6.00x0.95x24.29 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_wooden_beam_02b` | Ea01 Env Road Wooden Beam 02B | 3.00x0.45x5.79 | 300 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_wooden_beam_03a` | Ea01 Env Road Wooden Beam 03A | 6.00x0.69x17.64 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_wooden_beam_03b` | Ea01 Env Road Wooden Beam 03B | 3.00x0.40x5.13 | 300 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_wooden_planks_01a` | Ea01 Env Road Wooden Planks 01A | 6.00x0.53x22.35 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_wooden_planks_01b` | Ea01 Env Road Wooden Planks 01B | 3.00x0.28x5.73 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_wooden_planks_02a` | Ea01 Env Road Wooden Planks 02A | 6.00x0.54x22.51 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_wooden_planks_02b` | Ea01 Env Road Wooden Planks 02B | 3.00x0.27x5.75 | 634 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_wooden_planks_03a` | Ea01 Env Road Wooden Planks 03A | 6.00x0.49x20.39 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_road_wooden_planks_03b` | Ea01 Env Road Wooden Planks 03B | 3.07x0.25x5.34 | 300 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). |
| `utileria.objeto_generico.ea01_env_treecrown_01` | Ea01 Env Treecrown 01 | 1.83x1.18x4.51 | 114 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_01_1` | Ea01 Env Treecrown 01 1 | 1.83x1.18x4.51 | 114 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_02` | Ea01 Env Treecrown 02 | 1.91x1.33x4.58 | 134 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_02_1` | Ea01 Env Treecrown 02 1 | 1.91x1.33x4.58 | 134 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_03` | Ea01 Env Treecrown 03 | 1.93x1.62x5.45 | 120 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_03_1` | Ea01 Env Treecrown 03 1 | 1.93x1.62x5.45 | 120 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_04` | Ea01 Env Treecrown 04 | 1.96x1.74x4.31 | 130 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_04_1` | Ea01 Env Treecrown 04 1 | 1.96x1.74x4.31 | 130 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_05` | Ea01 Env Treecrown 05 | 1.50x0.89x2.96 | 124 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_05_1` | Ea01 Env Treecrown 05 1 | 1.50x0.89x2.96 | 124 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_06` | Ea01 Env Treecrown 06 | 0.87x0.43x1.06 | 70 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_06_1` | Ea01 Env Treecrown 06 1 | 0.87x0.43x1.06 | 70 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_07` | Ea01 Env Treecrown 07 | 1.90x0.57x2.10 | 64 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_07_1` | Ea01 Env Treecrown 07 1 | 1.83x0.52x2.01 | 64 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_08` | Ea01 Env Treecrown 08 | 1.91x2.70x4.60 | 134 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_08_1` | Ea01 Env Treecrown 08 1 | 1.91x2.70x4.60 | 134 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_09` | Ea01 Env Treecrown 09 | 2.30x2.93x7.47 | 120 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_09_1` | Ea01 Env Treecrown 09 1 | 2.30x2.93x7.47 | 120 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_10` | Ea01 Env Treecrown 10 | 1.98x2.03x4.40 | 130 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_10_1` | Ea01 Env Treecrown 10 1 | 1.98x2.03x4.40 | 130 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_11` | Ea01 Env Treecrown 11 | 1.44x1.92x2.68 | 130 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_11_1` | Ea01 Env Treecrown 11 1 | 1.44x1.92x2.68 | 130 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_12` | Ea01 Env Treecrown 12 | 1.07x0.85x1.37 | 70 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_12_1` | Ea01 Env Treecrown 12 1 | 1.07x0.85x1.37 | 70 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_13` | Ea01 Env Treecrown 13 | 1.80x1.63x1.96 | 118 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treecrown_13_1` | Ea01 Env Treecrown 13 1 | 1.80x1.63x1.96 | 118 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treeroot_01a` | Ea01 Env Treeroot 01A | 2.45x2.00x3.55 | 300 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treeroot_01b` | Ea01 Env Treeroot 01B | 3.62x1.20x3.89 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treeroot_01c` | Ea01 Env Treeroot 01C | 2.43x2.24x2.69 | 1500 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). |
| `utileria.objeto_generico.ea01_env_treeroot_01d` | Ea01 Env Treeroot 01D | 3.05x4.30x10.42 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treeroot_01e` | Ea01 Env Treeroot 01E | 1.98x3.39x3.90 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treeroot_01f` | Ea01 Env Treeroot 01F | 3.25x1.83x5.02 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treeroot_01g` | Ea01 Env Treeroot 01G | 2.01x0.79x1.90 | 798 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treeroot_01h` | Ea01 Env Treeroot 01H | 3.70x1.73x5.43 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treeroot_01i` | Ea01 Env Treeroot 01I | 3.29x4.53x10.07 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.objeto_generico.ea01_env_treeroot_01j` | Ea01 Env Treeroot 01J | 2.64x1.95x5.14 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.generico.prop_forester_watchtower_01` | Objeto generico | 3.69x6.21x2.61 | 799 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.generico.prop_pin_01a` | Objeto generico | 0.17x0.28x0.15 | 42 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.generico.prop_plank_01a` | Objeto generico | 2.03x7.49x48.46 | 132 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.generico.prop_plank_2a` | Objeto generico | 2.92x2.54x29.53 | 84 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.generico.prop_plank_3a` | Objeto generico | 3.24x3.86x85.10 | 84 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.generico.prop_tool_stump_01a` | Objeto generico | 1.70x1.22x2.16 | 300 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.generico.prop_wehicule_cart_01` | Objeto generico | 1.69x1.90x4.12 | 1499 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real. |
| `utileria.generico.prop_wehicule_cart_broken_01a` | Objeto generico | 3.40x3.62x7.39 | 1499 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.generico.prop_wehicule_cart_broken_01b` | Objeto generico | 3.73x3.80x7.87 | 1499 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.generico.prop_wehicule_cart_whell_01a` | Objeto generico | 0.99x0.16x0.99 | 300 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.generico.props_campfire_02` | Objeto generico | 3.14x2.51x2.90 | 799 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.generico.props_container_barell_01a` | Objeto generico | 0.77x1.03x0.81 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py). |
| `utileria.generico.props_container_chest_01a` | Objeto generico | 0.66x0.75x1.02 | 800 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real. |
| `utileria.generico.props_container_chest_01b` | Objeto generico | 0.90x0.90x0.97 | 1232 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real, reorientado. |
| `utileria.generico.props_cross_02` | Objeto generico | 2.48x1.12x1.58 | 564 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.generico.props_tent_01a` | Objeto generico | 2.28x4.34x3.67 | 720 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.generico.props_tent_01b` | Objeto generico | 2.28x4.34x3.67 | 720 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.generico.props_bench_01` | Objeto generico | 1.70x1.31x4.35 | 248 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |
| `utileria.generico.props_bench_02` | Objeto generico | 0.79x0.95x2.65 | 212 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows. Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py), a escala real. |
| `utileria.generico.props_roadsign_03` | Objeto generico | 0.49x3.24x2.12 | 62 | importado | `rig:ninguno` `formato:modelo` | OpenGameArt FREE Remastered Meadows |

<a id="personaje"></a>

## Personajes y NPCs

| Id | Nombre | Medidas (m) | Tris | Estado | Etiquetas | Notas |
|---|---|---|---|---|---|---|
| `personaje.base.cuerpo_nino` | Cuerpo base: niño | 0.35x1.2x0.25 | 800 | pendiente | `rig:humanoide` `talla:pequena` | Malla base modular: ropa y armadura encajan encima. |
| `personaje.base.cuerpo_comun` | Cuerpo base: adulto común | 0.53x1.7x0.29 | 1200 | importado | `rig:humanoide` `talla:comun` | Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen --miembros); falta el esqueleto y los clips. Medidas: el alto del inventario; el ancho y el fondo, los de la imagen. Cuerpo humano estandar (tools/modelos/humanoide.py): la malla riggeada de IMPORTED/Humanoid (rigged-model.glb), en la pose de su piel, reducida, con el color de su textura y sus pesos (los de su esqueleto de Mixamo, en los huesos del maniqui); el esqueleto y los clips, de exported-model.glb (los nombres de la Universal Animation Library de Quaternius); los clips, con los nombres del juego. |
| `personaje.base.cuerpo_robusto` | Cuerpo base: adulto robusto | 0.57x1.85x0.33 | 1300 | pendiente | `rig:humanoide` `talla:grande` | Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen --miembros); falta el esqueleto y los clips. Medidas: el alto del inventario; el ancho y el fondo, los de la imagen. |
| `personaje.base.cuerpo_comun_2` | Cuerpo base: adulto común (variante 2) | 0.54x1.7x0.3 | 1200 | pendiente | `rig:humanoide` `talla:comun` | Malla base modular: ropa y armadura encajan encima. Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen --miembros); falta el esqueleto y los clips. Medidas: el alto del inventario (el adulto común); el ancho y el fondo, los de la imagen. |
| `personaje.base.cuerpo_comun_3` | Cuerpo base: adulto común (variante 3) | 0.51x1.7x0.29 | 1200 | pendiente | `rig:humanoide` `talla:comun` | Malla base modular: ropa y armadura encajan encima. Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen --miembros); falta el esqueleto y los clips. Medidas: el alto del inventario (el adulto común); el ancho y el fondo, los de la imagen. |
| `personaje.base.cuerpo_comun_4` | Cuerpo base: adulto común (variante 4) | 0.53x1.7x0.29 | 1200 | pendiente | `rig:humanoide` `talla:comun` | Malla base modular: ropa y armadura encajan encima. Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen --miembros); falta el esqueleto y los clips. Medidas: el alto del inventario (el adulto común); el ancho y el fondo, los de la imagen. |
| `personaje.base.cuerpo_mujer` | Cuerpo base: mujer adulta | 0.46x1.6x0.25 | 1200 | pendiente | `rig:humanoide` `talla:comun` | Malla base modular: ropa y armadura encajan encima. Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen --miembros); falta el esqueleto y los clips. Medidas: el alto del inventario (la mujer estándar); el ancho y el fondo, los de la imagen. |
| `personaje.base.cuerpo_mujer_2` | Cuerpo base: mujer adulta (variante 2) | 0.51x1.6x0.26 | 1200 | pendiente | `rig:humanoide` `talla:comun` | Malla base modular: ropa y armadura encajan encima. Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen --miembros); falta el esqueleto y los clips. Medidas: el alto del inventario (la mujer estándar); el ancho y el fondo, los de la imagen. |
| `personaje.base.cuerpo_mujer_3` | Cuerpo base: mujer adulta (variante 3) | 0.48x1.6x0.27 | 1200 | pendiente | `rig:humanoide` `talla:comun` | Malla base modular: ropa y armadura encajan encima. Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen --miembros); falta el esqueleto y los clips. Medidas: el alto del inventario (la mujer estándar); el ancho y el fondo, los de la imagen. |
| `personaje.base.cuerpo_mujer_4` | Cuerpo base: mujer adulta (variante 4) | 0.45x1.6x0.26 | 1200 | pendiente | `rig:humanoide` `talla:comun` | Malla base modular: ropa y armadura encajan encima. Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen --miembros); falta el esqueleto y los clips. Medidas: el alto del inventario (la mujer estándar); el ancho y el fondo, los de la imagen. |
| `personaje.base.cuerpo_mujer_5` | Cuerpo base: mujer adulta (variante 5) | 0.46x1.6x0.28 | 1200 | pendiente | `rig:humanoide` `talla:comun` | Malla base modular: ropa y armadura encajan encima. Malla low-poly con textura (estilo PS2) en arte/modelos/para_riggear/, desde una hoja de tres vistas generada por el usuario con Canva (tools/modelos/piezas.py --forma volumen --miembros); falta el esqueleto y los clips. Medidas: el alto del inventario (la mujer estándar); el ancho y el fondo, los de la imagen. |
| `personaje.base.cuerpo_gigante` | Cuerpo base: gigante (gran guerrero) | 0.75x2.3x0.45 | 1500 | importado | `rig:humanoide` `talla:gigante` `especial:gran_guerrero` | Talla x1.2–1.45 del común. Cuerpo humano estandar (tools/modelos/humanoide.py): la malla riggeada de IMPORTED/Humanoid (rigged-model.glb), en la pose de su piel, reducida, con el color de su textura y sus pesos (los de su esqueleto de Mixamo, en los huesos del maniqui); el esqueleto y los clips, de exported-model.glb (los nombres de la Universal Animation Library de Quaternius); los clips, con los nombres del juego. |
| `personaje.base.cuerpo_anciano` | Cuerpo base: anciano | 0.5x1.6x0.3 | 1200 | pendiente | `rig:humanoide` `talla:comun` |  |
| `personaje.narrativo.protagonista` | Protagonista ({NOMBRE}) | 0.5x1.75x0.3 | 2000 | importado | `rig:humanoide` `acto:2` `especial:narrativo` `uso:personalizable` | Personalizable: nombre, aspecto, ropa. Cuerpo humano estandar (tools/modelos/humanoide.py): la malla riggeada de IMPORTED/Humanoid (rigged-model.glb), en la pose de su piel, reducida, con el color de su textura y sus pesos (los de su esqueleto de Mixamo, en los huesos del maniqui); el esqueleto y los clips, de exported-model.glb (los nombres de la Universal Animation Library de Quaternius); los clips, con los nombres del juego. |
| `personaje.narrativo.padre` | El padre (jefe de la confederación) | 0.5x1.75x0.3 | 2000 | pendiente | `rig:humanoide` `acto:1` `especial:narrativo` `faccion:nomada` | Jugable en el prólogo, a caballo. |
| `personaje.narrativo.madre` | La madre | 0.5x1.75x0.3 | 2000 | pendiente | `rig:humanoide` `acto:1` `especial:narrativo` |  |
| `personaje.narrativo.medio_hermano` | El medio-hermano | 0.5x1.75x0.3 | 2000 | pendiente | `rig:humanoide` `acto:2` `especial:narrativo` `faccion:culto` | Antagonista. |
| `personaje.narrativo.rey` | El rey (anciano) | 0.5x1.75x0.3 | 2000 | pendiente | `rig:humanoide` `acto:2` `especial:narrativo` `faccion:imperio` |  |
| `personaje.npc.pastor` | Pastor | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:nomada` `motivo:civil` |  |
| `personaje.npc.jinete_arquero` | Jinete arquero | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:nomada` `motivo:combate` |  |
| `personaje.npc.guerrero_nomada` | Guerrero nómada | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:nomada` `motivo:combate` |  |
| `personaje.npc.chaman` | Chamán | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:nomada` `motivo:espiritual` |  |
| `personaje.npc.curandero` | Curandero | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:nomada` `motivo:servicio` |  |
| `personaje.npc.herrero` | Herrero | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:nomada` `motivo:servicio` |  |
| `personaje.npc.explorador` | Explorador | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:nomada` `motivo:servicio` |  |
| `personaje.npc.campesino` | Campesino | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `bioma:ciudad` `motivo:civil` |  |
| `personaje.npc.obrero` | Obrero de la fundición | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `bioma:ciudad` `motivo:civil` |  |
| `personaje.npc.burocrata` | Burócrata | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `bioma:ciudad` `motivo:civil` |  |
| `personaje.npc.lancero` | Soldado lancero | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `motivo:combate` |  |
| `personaje.npc.ballestero` | Soldado ballestero | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `motivo:combate` |  |
| `personaje.npc.mosquetero` | Mosquetero | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `motivo:combate` |  |
| `personaje.npc.artillero` | Artillero | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `motivo:combate` |  |
| `personaje.npc.oficial` | Oficial imperial | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `motivo:combate` |  |
| `personaje.npc.verdugo` | Verdugo | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:imperio` `motivo:castigo` |  |
| `personaje.npc.sacerdote_culto` | Sacerdote del culto | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:culto` `motivo:espiritual` |  |
| `personaje.npc.fanatico` | Fanático del culto | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:culto` `motivo:combate` |  |
| `personaje.npc.captor_culto` | Captor del culto | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:culto` `motivo:captura` `acto:3` |  |
| `personaje.npc.prisionero` | Prisionero | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:cualquiera` `motivo:rescate` |  |
| `personaje.npc.refugiado` | Refugiado | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:neutral` `motivo:rescate` |  |
| `personaje.npc.comerciante` | Comerciante de caravana | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:neutral` `bioma:desierto` `motivo:comercio` |  |
| `personaje.npc.nomada_desierto` | Nómada del desierto | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:neutral` `bioma:desierto` `motivo:civil` |  |
| `personaje.npc.cazador_bosque` | Cazador del bosque | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:neutral` `bioma:bosque` `motivo:civil` |  |
| `personaje.npc.pescador_fiordo` | Pescador de los fiordos | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:neutral` `bioma:fiordo` `motivo:civil` |  |
| `personaje.npc.bandido` | Bandido | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:salvaje` `motivo:combate` |  |
| `personaje.npc.mercenario` | Mercenario | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:neutral` `motivo:combate` |  |
| `personaje.npc.nino` | Niño | 0.35x1.2x0.25 | 1500 | pendiente | `rig:humanoide` `faccion:cualquiera` `talla:pequena` `motivo:civil` |  |
| `personaje.npc.anciano` | Anciano | 0.5x1.7x0.3 | 1500 | pendiente | `rig:humanoide` `faccion:cualquiera` `motivo:civil` |  |

