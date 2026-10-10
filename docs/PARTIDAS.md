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

`tests/partidas/` guarda partidas de versiones anteriores, cada una con el resumen de su estado (`NOMBRE.txt`). Hay dos de cada tipo:
- **`v0.4.3-*.sav`:** el formato plano de antes. Se crearon con la v0.4.3 y su resumen se escribió con el lector de entonces.
- **`v0.4.4-*.sav`:** las mismas partidas en el formato por bloques. Tienen el mismo resumen.

| Partida | Qué tiene |
|---|---|
| `combate` | El jugador abatido con heridas, bandidos alrededor y bolsas de botín. |
| `fundar` | Un campamento por fundar, con su diálogo abierto, y la escolta. |
| `descanso` | Una marca en el mapa, órdenes a la escolta, un lugar de descanso, restos y una barra rápida personalizada. |

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

## El esquema congelado de las partidas planas

`src/game/save_v1.inc` describe con números fijos los structs de las partidas planas (formato 1, hasta la v0.4.3). Se volcó una sola vez, con `sf_dump_c`, desde el esquema de la v0.4.4, que era idéntico al de la v0.4.3, y **no se regenera**: describe archivos viejos, no los structs de hoy.

Se volcó en Linux x86-64. Android arm64 y Windows x64 tienen la misma disposición en memoria, porque ningún struct guardado usa `long`. Igual, el lector compara la huella de tamaños del encabezado con la del esquema congelado. Si no coincide pero sí coincide con la del build actual, lee con el esquema del build.
