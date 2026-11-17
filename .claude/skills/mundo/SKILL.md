---
name: mundo
description: Cómo se genera el mundo de SCAVENGERS THRIVE por capas (relieve, agua, regiones, vegetación, lugares e hitos, estaciones) y las reglas para cambiarlo sin romperlo, con los patrones de WorldClaw. Usar al tocar el terreno, los biomas, la hidrología, la dispersión de la vegetación o dónde caen pueblos, campamentos, ruinas y guaridas.
---

# El mundo, por capas

El mundo sale entero de la semilla y es siempre el mismo para la misma semilla (`docs/MUNDO.md`: lo fijo y lo
que cambia). Se arma por capas, y cada una lee solo las de abajo.

Es el orden de WorldClaw (Tencent Hunyuan, [arXiv 2608.05248](https://arxiv.org/abs/2608.05248)): plan →
terreno global → dispersión → objetos por región → revisión mirando imágenes.
- **WorldClaw** lo hace con agentes y Blender, antes de jugar.
- **Acá** va en tiempo real, en C11 y sin memoria por cuadro.

De WorldClaw se toman las ideas, no el código: su repositorio solo trae el README, sin código ni licencia.

| Capa | Qué decide | Dónde |
|---|---|---|
| 1. El plan | Las cinco regiones, sus alturas medias, sus bordes y lo que lleva cada una | `docs/MUNDO.md`, `region_altitude`, las tablas de `src/sim/world.c` |
| 2. El relieve | La altura: el borde fractal, las regiones mezcladas por peso, macizos, crestas y lomas | `world_height`, `world_region_weights`, `region_detail` (`src/sim/world.c`) |
| 3. El agua | Ríos, lagos, cascadas y el canal: bajan por la cota más baja y tallan su cauce en la altura | `src/sim/hydro.c`, `world_water*`, `world_river_*` (`docs/AGUA.md`) |
| 4. La dispersión | Pasto, stipa, flores, helechos, juncos, hierbas y árboles, por celda, según el peso de cada región y la cercanía del agua | `flora_cell`, `flora_density`, `flora_tree_kind` (`src/sim/flora.c`); el dibujo, en `src/world/vegetation.c`, `trees.c` y `terrain.c` |
| 5. Los lugares | Pueblos, tribus, guaridas, estructuras, ruinas y cuevas (en seco, en llano y separados), y su decorado | `settle_all` y `world_footprint_dry` (`world.c`), `place_check` (`placement.c`), `ancient.c`, `caves.c`, `camps.c`; el campamento inicial y sus objetos, en `src/world/camp.c` y `props.c` |
| 6. La estación | Cómo se ve todo lo anterior a lo largo del año: nieve, hielo, hojas, caudal | `climate.c`, `flora_tree_season`, `WaterSeason` |

## Reglas

- **Cada capa lee las de abajo y no las cambia:**
  - la dispersión no toca el relieve;
  - un pueblo no desvía un río;
  - si un lugar necesita llano, se busca un llano (`place_check`); no se aplana el mundo a su alrededor.
- **Determinista y local:** cada cosa sale de la semilla y de su celda o chunk, sin un azar compartido. Un chunk
  que se vuelve a cargar sale igual, en cualquier orden.
- **Sin memoria por cuadro:**
  - arreglos fijos (`FLORA_PER_CELL`, `CHUNK_TREES`, `*_MAX`); lo que no cabe se descarta;
  - los lugares se arman una sola vez, en `world_generate` (`settle_all`).
- **Las transiciones:** las regiones se mezclan por peso (`world_region_weights`) en la altura, la vegetación y
  los colores, así que nada cambia de golpe en una línea. Las fronteras se tuercen con ruido, pero ninguna región
  sale de su cuadrante.
- **El relieve, por región (`region_detail`):**
  - el altiplano, una meseta con cordilleras de agujas: la cresta, `1 − |fbm|`, al cubo y hasta 55 m;
  - los fiordos, una costa abrupta: la cresta al cuadrado, hasta 24 m;
  - el bosque, lomas amplias y redondeadas;
  - la estepa, lomas suaves.

  Los cauces se tallan siguiendo el agua. No hay erosión simulada; si se agrega:
  - va en las capas 2 y 3;
  - se calcula una vez, en memoria fija;
  - lleva su prueba en `tests/test_main.c`.
- **La dispersión, por región:** densidad × peso de la región × cercanía del agua (las cifras, en `flora.c`):
  - **la estepa central:** pasto (1,75 por m²), stipa lejos del agua (0,95), hierbas aromáticas y flores; matas en
    manchas y bosquecillos de pinos sueltos (`steppe_grove`, `steppe_scrub` en `terrain.c`); en la
    orilla, juncos tan juntos que no queda suelo pelado, y sauces;
  - **el bosque de coníferas:** pinos tupidos (un arce de cada seis árboles), pasto (1,15) y helechos;
  - **el altiplano glaciar:** pasto (0,6) y algunas flores; nieve sobre la línea de nieve, y los pinos, nevados;
    ríos trenzados en lechos de grava;
  - **el desierto de Ordos:** casi nada, salvo junto al agua (los oasis);
  - **los fiordos:** pasto (0,7) y helechos; sauces junto a lagos y ríos, no junto al mar.
- **Los lugares, en grupos:**
  - un reino es una capital amurallada con sus aldeas, al estilo de su región;
  - los pueblos quedan separados entre sí y lejos del campamento, y las guaridas, lejos de los pueblos;
  - nada cae en el agua ni sobre la nieve de todo el año (`world_footprint_dry`, `settle_all`). Lo que no cumple
    se corre al lugar válido más cercano, sin tocar el azar del resto.
- **La estación cambia cómo se ve, no dónde está.** El mismo árbol, en el mismo lugar, está verde, dorado, pelado
  o nevado; el río crece y se congela, pero su cauce no se mueve.
- **Los shaders:**
  - todo shader nuevo va en GLSL 330 y en ES 100 (el vértice en `precision highp float;`), con un respaldo sin
    shader;
  - en ES 2.0, `mediump` puede ser de 16 bits (GPU Mali o PowerVR, y Mesa): lo que va en coordenadas del mundo se
    deforma lejos del origen. El vértice va en `highp` (el shader por defecto de raylib también, desde
    `CMakeLists.txt`) y, donde se pueda, en las coordenadas locales del chunk o del modelo, como los árboles
    (`src/world/trees.c`).
- **Mirarlo:** cada capa que se toca, con los mapas y las vistas de `docs/MUNDO.md` («Cómo verlo»), en las
  regiones y estaciones que la regla toque, antes y después. Un cambio de reglas en `src/sim/` lleva su prueba.
