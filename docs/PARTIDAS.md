# Partidas guardadas

Desde la v0.4.4 la partida es una **lista de bloques**, uno por módulo, y cada bloque lleva el **esquema** de su struct: el nombre, el tipo, la posición y el tamaño de cada campo. Así una versión nueva del juego carga las partidas de las anteriores aunque los structs hayan cambiado, que es lo que pasará en casi todas las fases del [plan grande](PLAN_GRAN_ACTUALIZACION.md).

Antes (hasta la v0.4.3) la partida era plana: los structs uno tras otro, tal como estaban en memoria, con una huella de sus tamaños. Si cambiaba un solo tamaño, la partida no se podía cargar. Esas partidas se siguen leyendo con su esquema congelado y se reescriben en el formato nuevo la próxima vez que se guardan.

## El archivo

| Parte | Contenido |
|---|---|
| Encabezado | Igual en los dos formatos. Lleva la marca `ESTP`, el formato (1 plano, 2 por bloques), la huella de los tamaños (solo el formato 1), la fecha, el día, la tribu, la hora del juego y la estación. La lista de huecos lo lee sin cargar la partida. |
| Bloques | `{etiqueta de 4 letras, versión, tamaño, datos}`. Los datos son el esquema, la cantidad de elementos y los elementos. |
| `FIN.` | La marca de fin. Si no está, el archivo quedó a medias. |

| Bloque | Qué guarda | Si falta |
|---|---|---|
| `PART` | El último gran guerrero, la cámara y el azar de la partida (`SavedMisc`). | Dañada. |
| `JUGA` | El jugador (`Player`). | Dañada. |
| `REIN` | El reino tributario (`Kingdom`). | Dañada. |
| `TROP` | La tropa (`Troop`). | Dañada. |
| `ACCI` | Campamentos, acopio, obras, NPC, animales, bolsas, joyas, viajes… (`GameActions`). | Dañada. |
| `COMB` | Salud del jugador, enemigos y proyectiles (`Combat`). | Dañada. |
| `PELI` | Calor del cuerpo, trampas y escolta (`Hazards`). | Dañada. |
| `DESA` | Fuego, rayos y árboles quemados (`Disasters`). | Dañada. |
| `OBJE` | Los objetos del mundo, por id (`SavedProp`). | Sin objetos. |
| `MEMO` | Las páginas del mapa de memoria (`MemoryPage`). | Mapa sin recorrer. |
| `MARC` | Las marcas del mapa (`MapMarker`). | Sin marcas. |
| `MUER` | Lugar de descanso, muertes y restos (`DeathSave`). | Sin restos. Se descansa en el campamento. |
| `BARR` | La barra rápida (`QbSlot`). | La barra de serie. |

Los números del formato van en little-endian. Los datos de los structs van como están en memoria, sin el relleno ni los punteros, que se escriben en cero; todas las plataformas del juego son little-endian. Una partida es la misma en Windows, Linux y Android.

## Cómo se lee una partida de otra versión

- **Mismo esquema:** se copia tal cual.
- **Otro esquema:** campo por campo, buscando cada campo **por nombre**.
  - Un campo nuevo queda con su valor por defecto, que es cero.
  - Un campo que ya no existe se ignora.
  - Un campo que se movió se encuentra igual.
  - Un arreglo más grande se completa con ceros; uno más chico se recorta.
  - Un texto más corto se recorta y sigue terminando en cero.
  - Un número que cambió de tipo se convierte por su valor. Por ejemplo, un `int` que pasó a `float`, un `unsigned char` que pasó a `int`, o un valor que ya no entra se recorta al máximo.
  - Un struct anidado se convierte igual, a cualquier profundidad.
- **Un bloque que falta:** ese módulo arranca de cero. Si el bloque es obligatorio, la partida está dañada.
- **Un bloque de una versión más nueva que la del build:** la partida no se carga y se avisa ("La partida es de una versión más nueva del juego").
- **Bloques desconocidos** (de una versión más nueva): se ignoran.

El motor es C puro (`src/sim/save_format.c`) y tiene tests en `tests/test_main.c`. Cubren structs que crecen, se achican y se reordenan, números que cambian de tipo, archivos dañados byte por byte y esquemas congelados.

## Cambiar un struct que se guarda

Es lo que hace casi cada fase del plan grande. Los pasos:

1. Cambiar el struct: agregar, quitar o mover campos, o cambiar el tamaño de un arreglo.
2. Correr `python3 tools/save/esquema.py`, que regenera `src/game/save_schema.inc`. CI falla si quedó viejo.
3. Si el campo nuevo **no debe valer cero** al cargar una partida vieja, o si un campo cambió de significado o de unidades:
   - subir la versión del bloque en `BLOCKS` (`src/game/save_game.c`);
   - agregar el caso en `migrate()`, por ejemplo `if (L->version[B_COMBAT] < 2) L->cb.lock_target = -1;`.

Reglas:
- **No renombrar campos guardados.** Para el lector, un nombre nuevo es un campo nuevo y el valor viejo se pierde. Si hace falta renombrar uno, se deja el viejo una versión más y se copia en `migrate()`.
- **Los enums crecen al final.** Se guardan como números: un valor insertado en el medio cambia el significado de los que siguen.
- **Achicar un arreglo:** subir la versión y recortar su contador en `migrate()` (si no, el contador puede apuntar fuera).
- **Sin `long` ni `size_t`.** Cambian de tamaño entre Windows y Linux; el generador los rechaza. Usar `int32_t`, `int64_t` o `float`.
- **Los punteros no se guardan.** `apply()` los vuelve a enlazar al cargar.
- **Los objetos del inventario se guardan por su id** (texto). Un id de `assets/inventario.tsv` que cambia de nombre deja de encontrarse.
- **Un struct nuevo que se guarda:** agregar su raíz en `ROOTS` (`tools/save/esquema.py`) y su bloque en `BLOCKS`, como opcional (las partidas viejas no lo traen).

## Las partidas de referencia

`tests/partidas/` guarda partidas de versiones anteriores, cada una con el resumen de su estado (`NOMBRE.txt`):
- **`v0.4.3-*.sav`:** el formato plano de antes. Se crearon con la v0.4.3 y su resumen se escribió con el lector de entonces.
- **`v0.4.4-*.sav`:** las mismas partidas en el formato por bloques. Tienen el mismo resumen.
- **`v0.5.0-*.sav`:** las mismas, con los structs de la v0.5.0 (agacharse, objetivo, parry y golpe anunciado), y una nueva, `parry`.
- **`v0.5.1-rehen.sav`:** una sola, con lo de la v0.5.1 (rehén, abatido y alerta).
- **`v0.6.0-asalto.sav`:** un asalto al campamento, con la escolta en «defender».
- **`v0.6.1-tribu.sav`:** la tribu trabajando, con la ventana de pobladores abierta.
- **`v0.6.2-obras.sav`:** obras ubicadas con su giro.
- **`v0.7.0-caravana.sav`:** el asalto a una caravana, con su escolta peleando.
- **`v0.7.1-caballos.sav`:** a caballo, con arqueros a caballo alrededor y la cría en el campamento.
- **`v0.7.2-migracion.sav`:** el campamento inicial en marcha, con la tribu en columna.
- **`v0.8.0-lanza.sav`:** una carga con la lanza pesada que rompió una fila de bandidos.
- **`v0.8.1-forja.sav`:** junto a una veta de hierro, con la forja móvil, el mineral recién picado y una receta de la forja a medio hacer.
- **`v0.9.0-campos.sav`:** los campos del kanato recién saqueados, con su guardia saliendo de la ciudad.
- **`v0.9.1-asedio.sav`:** una expedición de castigo del kanato que marcha en formación sobre el campamento, con la capital en guerra a medio asediar y una aldea tomada.

| Partida | Qué tiene |
|---|---|
| `combate` | El jugador abatido con heridas, bandidos alrededor y bolsas de botín. |
| `fundar` | Un campamento por fundar, con su diálogo abierto, y la escolta. |
| `descanso` | Una marca en el mapa, órdenes a la escolta, un lugar de descanso, restos y una barra rápida personalizada. |
| `parry` (v0.5.0) | El jugador agachado, con un bandido elegido de objetivo y otro anunciando el golpe, dentro de la ventana del parry. |
| `rehen` (v0.5.1) | El jugador con un bandido de rehén, otro abatido en el suelo y un jinete alertado. |
| `asalto` (v0.6.0) | Fanáticos asaltando el campamento: un soldado de guardia, pobladores escondidos en las yurtas y la escolta en «defender». |
| `tribu` (v0.6.1) | Dos de caza (fuera, no se ven), uno pastoreando, los informes del explorador y de la guardia, dos marcas nuevas en el mapa y la ventana de pobladores abierta con uno elegido. |
| `obras` (v0.6.2) | Un refugio girado 45° y un muro de empalizada de dos tramos, con sus cuadrillas trabajando. |
| `caravana` (v0.7.0) | El asalto a una caravana del desierto: la caravana detenida, su escolta (dos jinetes, un bandido y un arquero) peleando con el jugador y el mercado de la caravana como último con quien se habló. |
| `caballos` (v0.7.1) | El jugador a caballo tras una carga con lanza (la espera de la próxima), arqueros a caballo alejándose al galope, una yegua preñada, un semental y una cría en el campamento. |
| `migracion` (v0.7.2) | El campamento inicial levantado y en marcha: las cinco yurtas cargadas, la gente en columna detrás del jugador y el rebaño siguiéndolo. |
| `lanza` (v0.8.0) | El jugador a caballo con la lanza pesada, justo después de una carga que alcanzó a dos bandidos de una fila de tres (el primero cayó muerto). |
| `forja` (v0.8.1) | El jugador junto a una veta del altiplano, ya marcada en el mapa, con el sable de acero frágil empuñado. En la mochila (la grande), la forja móvil y 2 de mineral recién picado, y las puntas de flecha de hierro a medio hacer (`CRAFT_IRON_POINTS`, una receta nueva). |
| `asedio` (v0.9.1) | La relación con el kanato en −50: su capital en guerra (la ciudad 0), con una puerta quemada, una escala apoyada en un tramo de la muralla y cercada el día anterior; una aldea (la ciudad 1) ya tomada por la tribu. Una expedición de castigo de la capital marcha sobre el campamento inicial: diez hombres en formación cerrada (cuatro infantes pesados con pavés delante, dos soldados, dos ballesteros y dos arqueros) y dos carros de guerra que ya cargan contra el jugador. |
| `campos` (v0.9.0) | El jugador en un campo del kanato que acaba de saquear el último día de la primavera: el campo quemado, 5 sacos de grano en el suelo y cuatro soldados del reino (`ENEMY_SOLDIER`, de la guardia de la ciudad 0) que salen por la puerta. Ya empezó el verano: la economía de las ciudades corrió un día y la revisión del tributo quedó anotada. |

`estepa --probar-partidas tests/partidas` (CI, en el job de Linux) hace esto con cada una:
- la carga;
- compara su resumen (`save_summary`, en `src/game/save_check.c`) con el esperado;
- la guarda en el formato de hoy, la vuelve a cargar y a guardar: tiene que dar el mismo resumen y los mismos bytes;
- la vuelve a cargar campo por campo (sin copiar structs enteros) y tiene que dar lo mismo.

Si el resumen no coincide, el obtenido queda en `NOMBRE.obtenido.txt` para compararlo.

- **Al cerrar una fase que cambia structs:** agregar una partida de referencia de esa versión. Se puede crear desde cualquier partida con `estepa --convertir-partida vieja.sav tests/partidas/vX.Y.Z-nombre.sav`, y su resumen va en `vX.Y.Z-nombre.txt`.
- **Al ampliar el resumen** (por ejemplo, con un campo nuevo): `--rehacer-resumenes` reescribe los esperados. Hay que revisar el diff: solo deben aparecer líneas o valores nuevos, nunca cambiar los viejos.

En la v0.4.4 se probó una versión futura simulada:
- campos nuevos en medio de `Enemy`, `Member`, `Player`, `Wound` y `MemoryCell`;
- un campo nuevo al principio de `Combat`;
- 24 enemigos en vez de 16.

Las seis partidas de referencia cargaron con el mismo resumen.

La v0.5.0 agregó campos de verdad (`Player.crouching`; `Enemy.windup` y `windup_move`; `Combat.target_lock`, `parry_cd` y `exposed`) sin migraciones: las partidas viejas cargan con esos campos en cero. El objetivo se guarda como índice + 1, así el cero de las partidas viejas significa «ninguno». El resumen sumó dos líneas (`agachado`, `elegido ... parry ... expuesto`) y la del golpe anunciado de cada enemigo, si lo hay.

La v0.5.1 agregó dos estados de enemigo al final de `EnemyState` (`EN_DOWN`, abatido, y `EN_HOSTAGE`, rehén), `Enemy.alert`, `Combat.hostage` (índice + 1) y una acción de campamento al final de `CampAction` (`ACT_FINISH_DOWNED`), que agranda `Kingdom.stance`: las partidas viejas cargan con el valor nuevo en cero. El resumen sumó `rehen`, y por enemigo, su alerta y lo que le falta para despertar si está abatido.

La v0.6.0 agregó `GameActions.escort_order` (cero: atacar, como antes) y a cada `Npc` su guardia, su ronda, su alarma y dónde se esconde. El resumen sumó la orden a la escolta y, por NPC, esos datos si está de guardia o escondido.

La v0.6.1 agregó tareas al final de `CampTaskKind` y `CampTask.member2`, el acompañante (cero: va solo). `CAMP_TASKS` pasó de 6 a 8: el arreglo crece y las partidas viejas cargan con los huecos en cero. `GameActions` sumó tres cosas:
- los elegidos de la ventana de pobladores;
- el pasto de cada campamento;
- sus informes (`CampNews`, un struct para que el esquema no tenga arreglos de tres dimensiones).

El texto de los diálogos creció de 320 a 1024. El resumen sumó, por campamento, sus tareas, el pasto y los informes, y los elegidos de la ventana.

La v0.6.2 agregó `BuildProject.yaw` (cero: como antes, sin girar) y `GA_MAX_PROJECTS` pasó de 8 a 24 (el arreglo crece). El resumen suma el giro de cada obra si no es cero.

La v0.7.0 agregó al final de `CraftId` y `BuildId` las recetas y obras nuevas, y a `GameActions` el comercio y las caravanas: el último mercado (`market_kind`, `market_idx`, `market_region`), la demora y el descanso de cada caravana, el asalto en curso (`caravan_raid`, índice + 1, y dónde) y los campamentos rivales saqueados (`tribe_looted`, un bit por tribu). Todo en cero en las partidas viejas: las caravanas salen a su hora y nada está saqueado. El resumen suma esas líneas solo si no son cero. La plata y el oro pesan menos (50 g la unidad): las partidas viejas cargan igual, solo cambia lo que pesa lo que llevan.

La v0.7.1 agregó, al final de cada struct, sin migraciones:
- a `Animal`: el sexo, la velocidad y el aguante heredados, los días de cría y de preñez. Un sexo en cero (partidas viejas) se asigna solo al primer paso de la fauna (`fauna_update`), con rasgos de ±10 %;
- a `Enemy`: el rumbo del caballo, el sentido en que rodea y si se aleja;
- a `Combat`: la lanza en ristre y su espera;
- a `Shot`: si es un disparo parto.

Además, `ENEMY_HORSE_ARCHER` va al final de `EnemyKind`. El resumen suma la línea de la cría de cada animal, la de la montura de cada enemigo, la lanza en ristre y los disparos partos, solo si no son cero.

La v0.7.2 agregó, al final de `CampSite`, si el campamento está en marcha (`migrating`), lo que lleva cargado (`load`, cuántas viviendas de cada clase: yurta común, yurta del jefe, tienda ligera y choza de fieltro) y si dejó su primer sitio (`moved`: migró o, el inicial, se disolvió; así se sabe que las yurtas del campamento inicial ya no están allí). A `GameActions`, `start_yurts`, cuántas yurtas del campamento inicial quedan en pie (lo copia el juego en cada cuadro). Todo en cero en las partidas viejas: los campamentos están plantados donde siempre. El resumen suma, por campamento, la línea de la marcha solo si algo de eso no es cero.

La v0.8.0 no cambió ningún struct que se guarda: los rasgos de las armas (`WeaponStats`, `RangedDef.saddle`) se calculan del id. Sí cambió lo que es un id: `arma.distancia.arco_compuesto` pasó a ser el arco compuesto reflejo (más fuerte, sin castigo a caballo), y el arco de antes es `arma.distancia.arco_simple`. Las partidas viejas cargan igual: el arco que ya tenían queda mejorado.

La v0.8.1 tampoco cambió ningún struct que se guarda. Lo nuevo va al final de sus enums: `ACTION_MINE`, `BUILD_WORKSHOP`, las recetas de la forja y del temple en `CraftId`, y `FIND_IRON` en `FindKind`. `BuildDef.min_people` y `CraftDef.fails` son datos de las recetas y no se guardan. El temple es un id del inventario (`_templado`, `_templada`, `_fragil`) y las vetas salen de la semilla del mundo, así que tampoco se guardan.

La v0.9.0 agregó, al final de cada struct, sin migraciones:
- a `GameActions`, las ciudades (`cities`, un `City` por asentamiento del mundo: gente, granero, tesoro, armería, campos, descontento, soldados, mercenarios, rencor, el último saqueo y su campo) y el tributo (`tribute_season`, la estación pagada, y `tribute_checked`, la última revisada, las dos más uno);
- a `Enemy`, `city`: de qué asentamiento es su guardia, más uno.

En las partidas viejas todo queda en cero. Una ciudad con la gente en cero se arma sola en el primer cuadro (`ct_update`), y la primera revisión del tributo solo se anota: no castiga. `ENEMY_SOLDIER` va al final de `EnemyKind` y `TD_CITY` al final de los modos del diálogo del mercado. El resumen suma, solo si no son cero, la línea de cada ciudad, la del tributo y la de la guardia de cada enemigo.

La v0.9.1 agregó, también al final y sin migraciones:
- a `City`, el asedio: `taken` (el día en que la tribu la tomó), `gates_burnt` (las puertas quemadas, un bit por puerta), `besieged` (el último día que pasó cercada), `ladders` (las escalas apoyadas, un bit por tramo de la muralla) y `expedition` (el día de su última expedición de castigo);
- a `Enemy`, `formation` (su formación más uno), `slot` (su lugar en ella; −1, fuera de filas) y `punitive` (es de la expedición de castigo en marcha);
- a `Combat`, `formations` (cuatro `Formation`: el centro del frente, la meta, el rumbo, las columnas, cuántos formaron, la ciudad, si se rompió y el enemigo que ven sus hombres);
- a `GameActions`, la expedición en marcha (`expedition_city`, `expedition_camp`, `expedition_army`, un `ArmyComp` con cuántos de cada clase salieron, `expedition_lost`, `expedition_out` y `expedition_sacked`) y `vassal_season`, la última estación en que pagaron tributo las ciudades tomadas (más uno).

Además, `CB_MAX_ENEMIES` pasó de 16 a 24: el arreglo crece y las partidas viejas cargan con los huecos en cero. Los enemigos nuevos (`ENEMY_HEAVY_INFANTRY`, `ENEMY_ROYAL_ARCHER`, `ENEMY_CROSSBOWMAN` y `ENEMY_CHARIOT`) van al final de `EnemyKind`, y la receta de la escala (`CRAFT_SIEGE_LADDER`), al final de `CraftId`. El resumen suma, solo si no son cero, la línea del asedio de cada ciudad, la de la expedición, la del tributo de las tomadas, la de la formación de cada enemigo y la de cada formación.

La v0.10.3 duplicó los árboles del campamento: `WORLD_MAX_TREES` pasó de 80 a 160, así que crece
`Disasters.tree_burn` (lo quemado de cada árbol). Los primeros 80 árboles salen iguales de la
semilla; en las partidas viejas, los nuevos cargan sanos (en cero). La gente con la que arranca cada
ciudad cambió (`city_start`), pero eso solo cuenta en las partidas nuevas: las guardadas traen la suya.

## El esquema congelado de las partidas planas

`src/game/save_v1.inc` describe con números fijos los structs de las partidas planas (formato 1, hasta la v0.4.3). Se volcó una sola vez, con `sf_dump_c`, desde el esquema de la v0.4.4, que era idéntico al de la v0.4.3, y **no se regenera**: describe archivos viejos, no los structs de hoy.

Se volcó en Linux x86-64. Android arm64 y Windows x64 tienen la misma disposición en memoria, porque ningún struct guardado usa `long`. Igual, el lector compara la huella de tamaños del encabezado con la del esquema congelado. Si no coincide pero sí coincide con la del build actual, lee con el esquema del build.
