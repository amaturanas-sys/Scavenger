# El agua: ríos, lagos, hielo y mar

El diseño del usuario (seis páginas a mano, noviembre de 2026), pasado a reglas del juego y repartido en cinco
fases. Ya están la **fase 1 (v0.11.0)**: el agua baja por donde debe y los lagos llenan sus depresiones; y la
**fase 2 (v0.11.1)**: cada agua cambia con las estaciones (se seca, se hiela, el mar se congela en la costa y el
glaciar avanza y retrocede). Las demás, abajo, con lo que hace falta para cada una.

![El agua](mundo/agua.png)

*Un lago del altiplano · el desborde de un lago glaciar, por donde sale su río · una cascada en la estepa · un
río de meandros en la estepa · un río trenzado del deshielo · un arroyo junto al glaciar (`--ir lago`,
`desborde`, `cascada`, `meandro`, `trenzado`, `arroyo`).*

## El diseño, página por página

| Página | La regla | Estado |
|---|---|---|
| **1. El nivel del mar es la base** | Las alturas medias bajan del altiplano al desierto, la estepa, el bosque y la costa, hasta el mar. | **Hecho (v0.11.0).** Altiplano ~82 m, desierto ~61, estepa ~40, bosque ~33 y costa ~26 (las medias medidas, con su relieve; `ALTITUDE[]` en `src/sim/world.c`). |
| **2. Agua sobre el nivel del mar** | Los lagos están en las depresiones del altiplano, la estepa y el bosque. | **Hecho.** Cada depresión del relieve es un lago (también en la costa). El desierto solo tiene oasis. |
| **3. Aguas abajo y estaciones** | En verano el glaciar retrocede y los cauces se llenan, y los ríos alimentan los lagos. En invierno los cauces quedan secos (la «huella») y los lagos se hielan. | **Hecho (v0.11.1).** Los ríos del glaciar corren de mediados de la primavera a mediados del otoño; con helada se secan y queda su cauce de grava con unos charcos helados. La lengua del glaciar retrocede en verano. |
| **4. Cambios de altura** | El agua siempre necesita una superficie por donde deslizarse y siempre cae. Sin superficie, es una cascada. | **Hecho.** El nivel de un río nunca sube. Donde cae 3 m o más con una pendiente de más del 25 %, hay una cascada: una cortina de agua con espuma al pie. Su aspecto fino es de la fase 4. |
| **El mundo: un disco de cinco biomas** | Cada bioma tiene su borde y, entre biomas, transiciones con cordilleras y ríos. A tiene muros de hielo. B tiene un gran canal con acantilados y ríos sinuosos, y del otro lado, paredes de roca. D tiene un cañón con un muro de estratos. F tiene la costa con fiordos y un mar accesible hasta un límite. | **En parte.** Los bordes de A, B, D y F ya existían. Las **cordilleras** entre el altiplano y el desierto, y entre el altiplano y la costa, están hechas. El muro de roca tras el canal y el mar con su límite son de la fase 5. Tu croquis tiene la misma vecindad que el juego (A frente a B, D frente a F): no se movieron regiones. |
| **5. El invierno** | a) Los lagos se cubren de hielo, que se rompe (peligro) o se perfora para pescar. b) Unos ríos se hielan y otros se secan. c) Hay hielo en la costa y témpanos en el mar. d) Los glaciares retroceden en verano. | **b, c y d: hecho (v0.11.1).** a) Cada lago y cada río tiene su hielo, que se cruza y se rompe bajo el jugador (`src/sim/hazards.h`, con rescate); el agujero para pescar es de la fase 3. |
| **6. Las texturas** | a) Vaivén en lagos y lagunas. b) Torrente en ríos y cascadas. c) Espuma en las orillas y donde el terreno o alguien toca el agua. d) Olas que entran y se retiran. | **Fase 4.** Hoy hay una textura de ondas que «respira» y espuma en orillas y rápidos. |
| **7. La hondura** | En lo bajo se camina por el fondo, en lo hondo se nada, en invierno se camina sobre el hielo y el hielo se rompe. | **El jugador, ya** (vadea hasta 1,2 m, nada más hondo; cruza el hielo que aguanta y los cauces secos). **Fase 3:** los NPC y los animales, que hoy rodean el agua honda (el hielo que aguanta y los cauces secos ya los cruzan). |
| **8. El trazado de los ríos** | Desde donde nace, el río baja por la cota más baja; los ríos se ramifican como raíces. | **Hecho.** Priority-Flood más la suma del agua (abajo). |
| **9. El agua necesita un contenedor** | El nivel de un lago es la altura de su punto de salida más bajo (el desborde), y de ahí sale su río. | **Hecho.** |

## La fase 1 (v0.11.0): el agua baja

Todo está en `src/sim/hydro.c` (la hidrología, genérica) y en `make_water()` de `src/sim/world.c` (el
mundo). Se calcula una vez por semilla, al generar el mundo: unos 110 a 150 ms en una PC.

1. **La grilla.** Son 288 × 288 celdas de 40 m, que cubren todo el mundo y algo de afuera. Cada celda tiene:
   - su altura sin agua (`world_land`), con la cuenca del lago del campamento;
   - qué es: tierra o una salida (el mar de los fiordos, el canal del bosque, el cañón del desierto o
     afuera del mundo);
   - cuánta agua recibe. La lluvia por región: bosque y costa 1, altiplano 0,75, estepa 0,55 y desierto 0
     (en el desierto no nace ningún río). El **deshielo** suma 14 en el pie del muro de hielo y sobre la nieve
     de todo el año.
2. **Priority-Flood** (Barnes, Lehman y Mulla, 2014; está en RichDEM, `github.com/r-barnes/richdem`). Desde
   las salidas se recorre el terreno de lo más bajo a lo más alto:
   - la celda que alcanza a otra es adonde corre el agua de esa otra;
   - en una depresión, el agua sube hasta su punto de desborde;
   - en lo llano, el agua sube 1 mm por celda, así que siempre corre.
3. **La suma del agua.** Al revés del orden del llenado, cada celda pasa su agua (y su deshielo) a la de
   abajo. Es el mismo principio que usa mapgen4 (`github.com/redblobgames/mapgen4`) para sus ríos.
4. **Los lagos.** Son las celdas cubiertas, vecinas entre sí, cuyo desborde está a la misma altura (±25 cm).
   - Su nivel es el de su desborde.
   - Quedan los de 4 celdas o más, 1,5 m de hondo o más, redondos (no un barranco largo), fuera del desierto
     y lejos del campamento.
   - El lago es un círculo de orilla irregular: crece hasta tapar la depresión.
   - Su nivel queda 40 cm debajo de lo más bajo de su orilla, porque la grilla es gruesa y un bajo entre dos
     celdas no tiene que desbordarlo. El río de salida corta hasta el desborde.
5. **Los ríos.** Nacen donde se juntan al menos 450 celdas de lluvia.
   - El orden: primero el de más agua. Aguas arriba se sigue el brazo con más agua; los otros brazos son
     afluentes que terminan en el punto exacto de su río.
   - Del desborde de cada lago sale su río. El que entra a un lago termina adentro.
   - Si un arroyo chico no se traza (no hay lugar), el que le llegaba sigue por su cauce hasta un río trazado.
6. **Cómo se ven.** Puntos cada 20 m, las esquinas de la grilla redondeadas y la clase según la región y el
   agua:
   - **arroyo** (2 a 3,5 m), por debajo de 2600;
   - **trenzado** (30 a 80 m de lecho de grava), en el altiplano y la costa, desde 9000;
   - **tributario** (6 a 12 m), en el bosque;
   - **de meandros** (6 a 12 m), en lo demás.

   Los meandros van y vienen a los lados en lo llano, sin subir una loma. El nivel queda un poco bajo la orilla
   y nunca sube; no baja del mar ni del canal donde termina.
7. **Los tramos.** Cada río se parte en tramos de una clase y de hasta 96 puntos (`River`). Cada tramo
   guarda:
   - de dónde viene (`RIVER_FROM_RAIN`, `_GLACIER`, `_LAKE`);
   - dónde termina (el mar, el canal, el cañón, un lago, otro río o el tramo siguiente);
   - su agua (`flow`) y la parte que es deshielo (`melt`).
8. **Las cascadas** (`Waterfall`): donde el nivel cae 3 m o más con pendiente. Lo que se hunde en el lago o
   en el río donde termina no cuenta.

**En cada semilla** salen unos 25 a 30 lagos y 130 a 180 tramos de río, con 13 a 22 cascadas. El río más
grande nace del glaciar y llega al mar o al canal. Con esta grilla, `world_height` cuesta lo mismo que antes
(1,4 a 1,6 µs por punto).

**Las pruebas** (`tests/test_main.c`):
- `test_hydro_basics`: la hidrología sobre un terreno de prueba.
  - el agua nunca sube y nada se pierde;
  - el pozo es un lago con el nivel de su desborde, que es lo más bajo de su borde;
  - los valles se juntan;
  - del lago sale un río.
- `test_water_flows_down`: el agua de tres mundos.
  - cada lago tiene su nivel bajo su orilla y un río que sale por su desborde;
  - cada río llega a una salida;
  - el más grande lleva deshielo;
  - las cascadas caen de verdad;
  - hay lagos en la estepa, el bosque y el altiplano, y ninguno en el desierto.
- `test_world_regions_borders_and_sites`: el orden de las alturas, la boca de los tributarios en el canal y
  los tramos que siguen sin saltos.

**Las partidas guardadas.** La semilla es la misma, pero el agua cambió de lugar. Si al cargar el jugador
queda en agua honda, aparece en la orilla más cercana.

## La fase 2 (v0.11.1): las estaciones del agua

Todo es una función pura del clima de ese día (`src/sim/climate.h`) y del mundo: `world_water_now` en
`src/sim/world.c` dice el agua de un punto en una estación (`WaterSeason`), con su hielo, y el juego la consulta
en cada cuadro (`terrain_water`, `terrain_ice`). El agua «completa», la de los cauces llenos, sigue siendo
`world_water`: con ella se generan el mundo, los pueblos y la flora, y no se construye en un cauce aunque hoy
esté seco (`terrain_water_full`).

1. **El clima del agua** (`Climate`):
   - `melt`, el deshielo del glaciar que corre ahora: nada con helada, todo con calor;
   - `temp_late`, la temperatura de dos días antes: la que sienten los lagos grandes y los ríos grandes, que
     se enfrían y se entibian despacio;
   - `sea_ice` y `floes`, el hielo de la costa y los témpanos: siguen la temperatura de tres días antes (el mar
     es lo más lento). Los témpanos llegan antes que el hielo y se van después;
   - `glacier`, cuánto avanzó el frente del glaciar: lo más al empezar la primavera, lo menos al empezar el
     otoño (va detrás del frío).
2. **Los cauces** (`ChannelState`: con agua, seco o helado). Cada tramo corre la parte de su agua que no es
   deshielo, más el deshielo de ahora (`world_river_share`).
   - Con menos del 20 %, el cauce se seca: queda la grava (o los cantos rodados) con unos charcos en lo hondo,
     que se hielan antes que el río.
   - Con menos del 60 %, el nivel baja hacia lo hondo del cauce (en el trenzado, hasta sus canales). Cerca de su
     boca, un tramo baja con el agua que lo recibe, así no queda colgado sobre ella. El río que sale de un lago
     nunca queda más alto que el lago.
   - En el invierno se secan los ríos del glaciar (de 70 a 110 de los 130 a 160 tramos de cada semilla); los
     de la lluvia y los lagos se hielan. Los del glaciar corren de nuevo desde mediados de la primavera.
3. **El hielo** de cada agua (`world_lake_ice`, `world_river_ice`; aguanta desde 0,5, `ICE_WALKABLE`):
   - **la altura:** 0,08 grados menos por metro sobre el campamento (más que en la realidad, para que se note);
   - **el tamaño:** un lago chico sigue el aire de hoy; uno grande, el de dos días antes. Los chicos (el del
     campamento, los oasis) se hielan de uno a tres días antes que los grandes, y los del altiplano antes que
     los de la costa;
   - **los ríos:** los arroyos y los charcos se hielan primero; los ríos grandes y el canal, al final.
4. **El mar** se hiela desde la orilla, donde es bajo (`world_sea_ice`): con más frío, más lejos (hasta 17 m
   de hondo), con el borde irregular. Mar adentro sigue abierto, y los témpanos flotan a la deriva. El hielo de
   la costa se camina y se rompe como el de un lago.
5. **El glaciar** (`world_glacier_reach`): su lengua avanza desde el pie del muro de hielo, en lóbulos, hasta
   unos 400 m al final del invierno, y retrocede en el verano. Deja una morrena de limo y grava, sin pasto ni
   árboles. Arriba, la nieve de todo el año sube y baja como antes (`Climate.snowline`).
6. **Cómo se ve:**
   - las mallas del agua de cada chunk se rehacen cuando la estación cambia lo suficiente (unos centímetros de
     crecida o un cuarto de grado). Lo que no cambia con la estación (qué agua llega a cada vértice,
     `WaterBodies`) se guarda al crear el chunk, así que rehacerlas cuesta menos de 0,1 ms por chunk;
   - cada vértice lleva el color de su agua mezclado con el de su hielo. El hielo que aguanta va en su propia
     malla, con la textura `assets/terrain/hielo.png` (placas con grietas, de `tools/assets/texturas_terreno.py`)
     y sin el brillo de las ondas;
   - las cascadas de un río seco desaparecen; las de uno helado son de hielo, sin espuma;
   - los témpanos del mar son placas de vóxeles (`VF_FLOE`) que salen con el frío y se mecen a la deriva;
   - el horizonte lejano usa la misma agua.
7. **En el juego:** el jugador, la tribu, los enemigos y los animales cruzan los cauces secos y el hielo que
   aguanta (y lo rompen como antes). Los peces no se ven ni se pescan donde el agua se fue. Las construcciones,
   los árboles del campamento y el pasto que puede quemarse miran el agua completa.

![Las estaciones del agua](mundo/estaciones.png)

*El mismo lugar en dos momentos del año: un río de meandros del glaciar en otoño y ya seco · un lago glaciar
antes y después de helarse · el mar al romperse su hielo en la primavera y helado en invierno · la lengua del
glaciar en la primavera y retirada en el verano (`--ir meandro`, `lago`, `mar`, `lengua`, con `--dia`).*

**Las pruebas** (`tests/test_main.c`, `test_water_seasons`):
- el clima del agua: el deshielo, el hielo del mar (después de los témpanos) y el glaciar, sin saltos;
- la estación llena es el agua de siempre (`world_water`), en todo el mundo;
- en verano todos los cauces llevan agua; en invierno los del glaciar se secan (sin agua o con charcos helados)
  y los de la lluvia y los lagos se hielan;
- en cada estación, ningún tramo sube ni queda más colgado sobre el agua que lo recibe que con los cauces llenos,
  y el río de un lago no queda más alto que el lago;
- los lagos: ninguno helado en verano, todos en el invierno; los altos se hielan antes que los bajos, y los chicos
  antes que los grandes;
- el mar se hiela junto a la costa y no mar adentro;
- el glaciar cubre siempre el pie del muro, más lejos solo en invierno, y nunca fuera del altiplano.

## Las fases que siguen

Cada una va con su PR, el CI verde, su release, sus capturas y sus pruebas.

- **Fase 3, v0.11.2, «Hielo, nado y pesca».**
  - Los NPC y los animales vadean, nadan, cruzan el hielo y caen al romperse (hoy rodean el agua honda).
  - Se puede hacer un agujero en el hielo para pescar, con una herramienta nueva del inventario.
- **Fase 4, v0.11.3, «Cómo se ve el agua».** Un shader, también en GLSL ES 100 para Android, con:
  - el vaivén de los lagos;
  - la corriente de los ríos, que sigue su cauce;
  - la cortina y la bruma de las cascadas;
  - la espuma en las orillas, las rocas y alrededor de quien está en el agua;
  - las olas que entran y se retiran en las playas.
- **Fase 5, v0.11.4, «Los bordes».**
  - Del otro lado del canal del bosque, un muro de roca y estratos levantados: una vista que no se alcanza.
  - El mar abierto se puede nadar hasta un límite. Más allá, el tiburón. El jugador nunca muere: lo muerde,
    lo deja herido y despierta en la playa, con **secuelas** que no se curan solas (menos aguante y
    velocidad). Solo las cura un **curandero** o un **druida** del campamento.
