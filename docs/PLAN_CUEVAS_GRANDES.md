# Plan: las cuevas grandes (v0.14)

Pedido del usuario (4 de noviembre de 2026), con once imágenes de referencia:

> «Es necesario incrementar las dimensiones de las recámaras y la cisterna magna, y que el techo de ellas sea más alto
> (al menos 3 veces la altura de una persona). Los pasillos también requieren mejoras: incrementa su longitud, haz que
> tengan trayecto tortuoso. Los objetos en las cuevas como estalactitas, columnas, cofres, rocas, fuentes, y otros
> elementos geológicos deberán ser renderizados con voxels. Los demás elementos deben ser refinados según las imágenes
> de referencia que se adjuntan aquí. Para agilizar el gameplay y prevenir que se sature el motor de gráficos, los
> interiores de las cavernas cargan por separado una vez que el jugador se aproxima a la entrada y acciona "explorar",
> esto permite que el mundo exterior y la geometría de los túneles y recámaras de la cueva no interfieran entre sí.»

El reparto: **Claude Code puso la base (v0.14.0)**. Sobre ella, **Copilot hace las tareas de abajo en el Codespace**,
una por rama y por PR, en orden. Claude verifica cada PR, lo fusiona y publica una versión en cada hito.

---

## 1. La base (v0.14.0, hecha)

### La planta (`src/sim/caves.c`, C puro, con sus pruebas)

- **Las salas:**
  - las recámaras tienen de 6,5 a 9,5 m de radio y la Cisterna Magna, de 13 a 17 (antes, 3,5 a 5,5 y 7 a 9);
  - la pared es recta hasta 6 m (la Cisterna, 12) y después viene la bóveda hasta 9 m (18);
  - sobre todo el suelo de cada sala, el techo está a más de tres personas (`CAVE_PERSON_HEIGHT`, 1,8 m). La prueba lo
    mide punto por punto.
- **Los pasillos:**
  - miden 10 m o más (los de la boca, hasta unos 70 m) y dan hasta tres vueltas;
  - el eje tiene 24 tramos (`CaveCorridor.px` y `pz`) y no pisa salas ajenas ni otros pasillos;
  - la bajada sigue recta y es la más empinada.
- **El suelo** de cada sala sigue a su pared despareja. La pared, el perfil de la bóveda, la roca sobre cada punto y el
  lugar de cada objeto salen de la planta: `cave_wall_radius`, `cave_room_profile`, `cave_rock_above`,
  `cave_object_at` y `cave_door_at`. Lo que se dibuja y lo que se choca salen de lo mismo.
- **El decorado** (`cave_decor`, `CaveDecor`) es la roca misma: no se toma ni se guarda, y sale de la cueva cada vez
  que se carga.
  - Por ahora son rocas (`CDEC_ROCK`).
  - Lo que tapa el paso va solo junto a la pared: del centro de cada sala se llega a todas sus puertas, y la prueba lo
    comprueba.
- **Las partidas viejas siguen valiendo.** La planta nueva usa los mismos números al azar, en el mismo orden, que la de
  la v0.13, y las vueltas salen de otro hilo de azar. Así cada objeto sigue siendo el mismo (lo tomado se guarda por
  su número, en `GameActions.cave_done`). La prueba compara la «huella» de los objetos con la de la v0.13.

### Explorar y la carga aparte (`src/game/`)

- **Junto a la boca** (en el umbral o en el túnel oscuro) aparece «F: explorar la cueva». Al pulsar F:
  - un cuadro muestra «Explorando la cueva...»;
  - se arma la escena (`cave_scene.c`) y se entra.
- **Al salir** caminando por la boca, la escena se libera. Ya no se entra caminando. Los guardias en pie y la montura
  siguen cerrando el paso.
- **Mientras se está adentro, el mundo de afuera no se dibuja.** La cueva es otro espacio: sus túneles y salas no se
  cruzan con el terreno.
- **La escena:**
  - las mallas de la roca: el vestíbulo, cada sala y cada pasillo por su eje, en trozos;
  - el decorado y los modelos de voxeles.
- **La luz sigue siendo por vértice**, pero solo se calcula en los trozos a menos de 48 m del ojo (`CS_VIEW`) y con
  las llamas que los alcanzan. Nada se arma por cuadro.
- **En la consola**, cada carga anota sus trozos, sus triángulos, los modelos de voxeles y los milisegundos (la línea
  `CUEVA`).

### Los voxeles de adentro (`src/world/voxcave.c`)

- **Ya están** las estalactitas, las estalagmitas y las rocas (`VC_STALACTITE`, `VC_STALAGMITE`, `VC_ROCK`), con la
  roca de cada región y la calcita clara.
- **Cada modelo** se arma al cargar la cueva (`vc_preload`, desde `cp_preload` o `cs_load`) y se libera al salir
  (`vc_unload`).
- **La receta** de cada uno está en `build_kind`: es la plantilla para las tareas.

### Quién es quién

| Archivo | Qué es | Quién lo toca |
|---|---|---|
| `src/sim/caves.c` y `.h` | La planta, el decorado y sus reglas (C puro). | Las tareas que agregan decorado. Siempre con su prueba en `tests/test_main.c`. |
| `src/world/voxcave.c` y `.h` | Los modelos de voxeles de adentro. | Casi todas las tareas. |
| `src/game/cave_props.c` | Cómo se ve cada objeto (cofres, celdas, pinturas, antorchas...). | Las tareas de los objetos. |
| `src/game/cave_scene.c` | La roca (salas y pasillos), la luz, el decorado y la boca vista desde adentro. | Las tareas de los túneles, del lago y de la boca. |
| `src/game/cave_game.c` | Lo que se juega: entrar, moverse, F, los prisioneros, la cámara, el plano. | Ninguna tarea, salvo para agregar su lugar de prueba a `DECOR_SPOTS` (en `cv_test_enter`). Lo demás lo toca Claude. |

### Cómo mirar

Desde `build/`, con `xvfb-run -a ./estepa --screenshot x.png --frames 40` y esto:

| Lo que se quiere ver | Opciones |
|---|---|
| Una sala, con la antorcha | `--cueva forajidos:0 --antorcha` (la Cisterna; `:1`, `:2`..., las recámaras) |
| Un pasillo con sus vueltas | `--cueva tribu:pasillo3 --antorcha` |
| Ante un objeto | `--cueva imperio:columna --antorcha` (también `cofre`, `celda`, `pintura`, `fogata`, `antorcha`, `veta`, `huesos`, `estalactita`, `estalagmita`...) |
| Ante algo del decorado | `--cueva forajidos:roca --antorcha` (cada tarea que agrega un decorado suma su nombre a `DECOR_SPOTS`) |
| Desde más lejos y arriba | `--dron 7 --camara 0.15` |
| Explorar desde la boca | `--ir boca-abandonada --explorar`: «Explorando...» a los 8 cuadros y adentro desde los 10 |

El presupuesto se lee en la consola, en la línea `CUEVA`.

### Los presupuestos (los mide Claude al verificar)

- **La roca de una cueva:** hasta 20 000 triángulos (la base usa unos 12 600).
- **Un modelo de voxeles:** hasta 1 500 triángulos, sin refinar.
- **Todos los modelos armados en una cueva:** hasta 40 000 triángulos.
- **La carga:** hasta 150 ms en la PC (la base tarda de 2 a 3).
- **Nada se arma, se carga ni se reserva por cuadro:**
  - los modelos y las texturas, en `cp_preload` o `cs_load`;
  - lo que se libera, en `cs_unload`.

---

## 2. Las referencias

Las once imágenes del pedido **no están en el repositorio**: el repositorio es público y la mayoría son fotos de
terceros. Para cada tarea hay que **adjuntar en el chat de Copilot la imagen que indica** (el usuario las tiene). De
cada una, qué tomar (no se copia la foto: se toma la forma, la composición y la paleta) y sus colores dominantes:

| # | Qué es | Qué tomar | Paleta | Tarea |
|---|---|---|---|---|
| 1 | Cráneos y huesos sobre una losa, junto a la roca, con luz cálida | Cráneos de a dos o tres; huesos largos cruzados encima de una losa caída; polvo y piedritas alrededor | `#1E1609` `#947350` `#B49065` `#64492E` `#8E6234` `#C9B294` | C7 |
| 2 | Una pila de huesos y tres cráneos en fila sobre un saliente | La pila de huesos largos; los cráneos alineados sobre el saliente de roca; el ocre del hueso sobre la roca gris | `#C6AC8F` `#584B44` `#A48D77` `#8B7766` `#CEA679` | C7 |
| 3 | Una antorcha encendida | La llama en gota, con una lengua que sube; el núcleo casi blanco y el borde rojo anaranjado; la cabeza envuelta, oscura | `#260202` `#5F1005` `#9D2904` `#D34607` `#F7DFBF` `#7A4139` | C8 |
| 4 | Pintura levantina: ciervos y arqueros | Siluetas planas de rojo oscuro sobre ocre; ciervos a la carrera, con astas; arqueros de cuerpo de palito; las grietas de la roca | `#EA9961` `#4E1A0F` `#A4582F` `#C16E3F` `#D38D5B` `#F8BB8D` | C9 |
| 5 | La Cueva de las Manos | Manos en negativo (soplado) rojas, blancas y ocres; guanacos en fila; un zigzag rojo; una espiral | `#CB9770` `#B4896D` `#A56749` `#CFA588` `#754D3B` `#D38732` | C9 |
| 6 | Un calabozo | Cadenas colgadas de argollas; grilletes; cuencos de barro; una bola de hierro; paja en el suelo; muro de piedras irregulares | `#8E7257` `#614B39` `#A68B6F` `#88603C` `#C4AF95` `#C2A076` | C10 |
| 7 | Túnel de bóveda de ladrillo y una puerta de tablas al fondo | La bóveda de medio punto; la puerta de tablas con luz entre ellas; escombros en el suelo | `#110E0F` `#2D2828` `#473C38` `#5C5656` `#DEDDDE` `#837978` | C11, C12 |
| 8 | Un arco de piedra y una escalera que sube, en penumbra | El arco labrado; los escalones de piedra; un banco de piedra; casi todo oscuro y la luz desde arriba | `#070708` `#1F2020` `#2E2E2D` `#5E584F` `#46413B` `#9C8873` | C13 |
| 9 | Túnel de bóveda de sillares, suelo de losas, niebla verdosa | Los sillares de la bóveda; las losas del suelo; la niebla al fondo y la luz lejana | `#061216` `#122B2E` `#17282C` `#2A5453` `#305051` `#4B8D89` | C11 |
| 10 | Una sala con estalactitas y un lago que las refleja | Las estalactitas en racimos y cortinas; el agua quieta que las refleja; la luz fría azulada con un foco cálido | `#090C11` `#1C232C` `#354352` `#314C63` `#476E8E` `#85939B` | C1, C5, C6 |
| 11 | La boca vista desde adentro | La abertura encandilada; los rayos de luz que entran; el suelo de tierra con relieve; lo demás, negro | `#070606` `#2C2521` `#59473C` `#FEF9F3` `#B57E57` `#ECC29B` | C14 |

---

## 3. Las reglas de todas las tareas

Son las de `.github/copilot-instructions.md` (el detalle está en `docs/GUIA_IA.md`). Además, en estas tareas:

1. **Rama y PR:** una tarea por rama, `copilot/cuevas-cN-<nombre>`, desde `main` al día, con un PR por tarea. No se
   tocan `VERSION`, las novedades del `README.md` ni `docs/ROADMAP.md`.
2. **Lo nuevo de la roca es decorado (`CaveDecor`), no un objeto (`CaveObject`).** Los objetos se guardan por su
   número: uno nuevo, o uno que cambia de lugar en la lista, rompe las partidas y la prueba de la huella. Si algo nuevo
   tiene que tomarse, se para y se pregunta.
3. **Las reglas del decorado van en `src/sim/caves.c`** (dónde va, si tapa el paso) **y su prueba, en
   `tests/test_main.c`.** Las pruebas solo crecen. Los tipos (`CaveDecorKind`, `VoxCaveKind`) crecen al final.
4. **Los modelos y las texturas se arman al cargar** (`vc_preload` en `cp_preload` o `cs_load`) y se liberan al salir:
   los modelos, en `cs_unload` (`vc_unload`), y lo demás que arme `cave_props.c`, en `cp_unload`. Nunca en un `draw`.
5. **Se mira antes y después:** con las opciones de «Cómo mirar», el antes y el después lado a lado (`montage`),
   abiertos. En el PR van la captura y la línea `CUEVA` de la consola, antes y después.
6. **Los textos**, si hay, con `T()` o `N_()` y su traducción en `assets/i18n/en.tsv`.
7. **El pedido en el chat:** con el agente `pulir`, «Haz la tarea Cn de `docs/PLAN_CUEVAS_GRANDES.md`», con la
   imagen que indica la tarea adjunta.

---

## 4. Las tareas (en este orden)

Los hitos de versión son tres: v0.14.1 al terminar C4, v0.14.2 al terminar C10 y v0.14.3 al terminar C16.

### C1 — Las estalactitas y las estalagmitas, en racimos (referencia 10)

- **Qué:**
  - dos decorados nuevos en `cave_decor`: `CDEC_STALACTITES` (un racimo colgado de la bóveda, de 3 a 6 por sala, más
    junto a la pared) y `CDEC_STALAGMITES` (un racimo en el suelo, junto a la pared; tapa el paso);
  - sus modelos en `voxcave.c`: cada racimo es un solo modelo, con 3 a 5 conos de largos distintos;
  - las estalactitas sueltas que ya hay, con más variación (cortinas, gotas) y las vetas húmedas.
- **Archivos:** `src/sim/caves.c` y `.h`, `tests/test_main.c` (el racimo del techo no baja de 2,5 m sobre el suelo; el
  del suelo cumple las reglas del decorado que tapa), `src/world/voxcave.c` y `.h`, `src/game/cave_scene.c` (dibujar
  los dos kinds).
- **Se prueba:** `--cueva animal:racimo --antorcha --dron 7 --camara 0.15` (el lugar `racimo`, en `DECOR_SPOTS`) y
  `--cueva tribu:estalagmita --antorcha`.
- **Hecho cuando:** cada sala tiene racimos; desde el centro se llega a todas las puertas (la prueba); cada modelo
  tiene menos de 1 500 triángulos.

### C2 — Las columnas de voxeles

- **Qué:**
  - `COBJ_COLUMN` pasa a voxeles (`VC_COLUMN`), de la mampostería del tipo de cueva: sillares en el imperio, piedra
    apilada en los forajidos y la tribu, y rota y caída en las abandonadas;
  - un decorado `CDEC_COLUMN` para las de fieras y las abandonadas: una columna natural, donde una estalactita se
    juntó con una estalagmita. Va del suelo a la bóveda (`cave_rock_above`) y tapa el paso.
- **Archivos:** `voxcave.c` y `.h`, `cave_props.c` (la columna del objeto; su choque sigue en `cave_game.c` y no se
  toca), `caves.c` y `.h` con su prueba (`CDEC_COLUMN`), `cave_scene.c`.
- **Se prueba:** `--cueva imperio:columna --antorcha --dron 7 --camara 0.15` y `--cueva animal:pilar --antorcha --dron 7`
  (el lugar `pilar`, la columna natural, en `DECOR_SPOTS`).
- **Hecho cuando:** la columna llega justo a la bóveda, sin pasarla ni quedar corta.

### C3 — Los cofres de voxeles

- **Qué:** `COBJ_CHEST` pasa a voxeles, con dos modelos (`VC_CHEST` cerrado y `VC_CHEST_OPEN` abierto y vacío, para
  cuando ya se tomó), de madera con flejes y cerradura de hierro:
  - los del imperio, con esquinas de bronce;
  - los de la tribu, más bajos, de cuero sobre madera.
- **Archivos:** `voxcave.c` y `.h`, `cave_props.c`.
- **Se prueba:** `--cueva forajidos:cofre --antorcha`, y lo mismo con `--cueva-f` (abierto).
- **Hecho cuando:** se ve cerrado y, después de F, abierto; el choque (`object_blocks`) sigue igual.

### C4 — Las rocas: variantes y escombro

- **Qué:**
  - más variantes de `VC_ROCK`: canto rodado, bloque caído anguloso y laja;
  - un decorado `CDEC_RUBBLE`: un montón de piedras chicas que no tapa el paso, en todas las cuevas y más en las
    abandonadas (el techo se viene abajo).
- **Archivos:** `voxcave.c` y `.h`, `caves.c` y `.h` con su prueba, `cave_scene.c`.
- **Se prueba:** `--cueva abandonada:escombro --antorcha --dron 7 --camara 0.15` (el lugar `escombro`, en `DECOR_SPOTS`) y
  `--cueva forajidos:roca --antorcha`.
- **Hecho cuando:** las salas se ven pedregosas sin tapar las puertas. **Hito: v0.14.1** (lo publica Claude).

### C5 — Las fuentes (referencia 10)

- **Qué:** un decorado `CDEC_SPRING`, en algunas cuevas: una pila de roca junto a la pared, con un hilo de agua que
  cae de una grieta, y el agua de la pila animada (un plano con ondas en `cave_scene.c`, sin shader nuevo). No tapa el
  paso, salvo la pila misma.
- **Archivos:** `caves.c` y `.h` con su prueba, `voxcave.c` (la pila), `cave_scene.c` (el agua y el hilo).
- **Se prueba:** `--cueva animal:fuente --antorcha` (el lugar `fuente` se agrega a `DECOR_SPOTS`), en una cueva que
  tenga fuente.
- **Hecho cuando:** el agua se mueve y el hilo cae de la pared a la pila.

### C6 — El lago de la Cisterna (referencia 10)

- **Qué:** en algunas Cisternas (más en los fiordos y el bosque), un lago:
  - es un decorado `CDEC_POOL` en una parte de la sala, lejos de las puertas y de los objetos;
  - el agua es quieta y oscura, y refleja las estalactitas (un falso reflejo: los modelos de arriba, dibujados al
    revés bajo el agua con poca luz);
  - lo hondo tapa el paso.
- **Archivos:** `caves.c` y `.h` con su prueba (no tapa ninguna puerta ni objeto, y se cruza la sala por otro lado),
  `cave_scene.c`.
- **Se prueba:** `--cueva animal:lago --antorcha --dron 9 --camara 0.25` (el lugar `lago`, en `DECOR_SPOTS`).
- **Hecho cuando:** se ve el reflejo y no se camina sobre el agua.

### C7 — El osario (referencias 1 y 2)

- **Qué:** `COBJ_BONES` deja de ser tres palitos y una esfera:
  - en las de fieras, restos esparcidos;
  - en las abandonadas y las habitadas, una pila de huesos largos con dos o tres cráneos encima, sobre una losa o un
    saliente.

  Al tomarlos desaparecen, como ahora. Puede ser de voxeles finos (celdas de 0,03 a 0,05 m) o de primitivas: lo que
  quede mejor dentro del presupuesto.
- **Archivos:** `cave_props.c` (y `voxcave.c` si es de voxeles).
- **Se prueba:** `--cueva abandonada:huesos --antorcha` y `--cueva animal:huesos --antorcha`.
- **Hecho cuando:** se reconoce un cráneo desde 3 m.

### C8 — Las antorchas (referencia 3)

- **Qué:** la antorcha de la pared (`COBJ_TORCH`):
  - el soporte de hierro y el palo;
  - la cabeza envuelta y oscura;
  - la llama en gota con su lengua, de dos o tres capas (el núcleo claro y el borde rojo), que tiembla con el tiempo;
  - apagada, la cabeza carbonizada.

  La luz (`cs_gather_lights`) no cambia.
- **Archivos:** `cave_props.c`.
- **Se prueba:** `--cueva forajidos:antorcha --antorcha` y `--cueva abandonada:antorcha` (apagada).
- **Hecho cuando:** la llama se lee como tal a 5 m, sin parecer una esfera.

### C9 — Las pinturas (referencias 4 y 5)

- **Qué:** cada pintura (`COBJ_PAINTING`) pasa a ser una textura armada al cargar la cueva (una imagen chica, de 128 ×
  64 píxeles, dibujada por código: nada de archivos de fotos), según quién vive allí:
  - la tribu: una cacería al modo levantino, con ciervos a la carrera y arqueros;
  - las abandonadas: manos en negativo, guanacos en fila, un zigzag y una espiral;
  - los forajidos: rayas de conteo;
  - el imperio: un relieve con soldados en fila.

  Se dibuja sobre la pared con la luz del lugar. La textura se arma en `cp_preload`, se libera en `cp_unload` y se
  olvida en `cp_forget_gpu` (Android).
- **Archivos:** `cave_props.c`.
- **Se prueba:** `--cueva tribu:pintura --antorcha` y `--cueva abandonada:pintura --antorcha`.
- **Hecho cuando:** se distinguen las figuras y la pared sigue viéndose como roca alrededor.

### C10 — Las celdas (referencia 6)

- **Qué:** la celda (`COBJ_CELL`):
  - cadenas colgadas de argollas en la pared, con grilletes;
  - cuencos de barro en el suelo y una bola de hierro;
  - paja en el suelo de la celda y un parche de muro de piedras irregulares detrás.

  Los barrotes y la puerta abierta del liberado siguen.
- **Archivos:** `cave_props.c` (y `voxcave.c` si algo es de voxeles).
- **Se prueba:** `--cueva forajidos:celda --antorcha` y `--cueva imperio:dialogo --cueva-f --antorcha`.
- **Hecho cuando:** se ve como un calabozo, sin tapar al prisionero. **Hito: v0.14.2** (lo publica Claude).

### C11 — Los túneles labrados (referencias 7 y 9)

- **Qué:** en las cuevas del imperio (y en el vestíbulo de las habitadas), los pasillos que salen de la boca se ven
  labrados:
  - una bóveda de sillares o ladrillo (el perfil del túnel, más redondo, con hiladas en los colores);
  - el suelo de losas.

  Es solo el color y la forma de la malla del pasillo en `build_corridor`. Lo que se choca no cambia: el ancho y el
  alto siguen siendo los de `src/sim/caves.h`.
- **Archivos:** `cave_scene.c`.
- **Se prueba:** `--cueva imperio:pasillo1 --antorcha`, `--cueva imperio:pasillo2 --antorcha`.
- **Hecho cuando:** se nota la mano del hombre en el imperio y la roca natural en las demás; la roca sigue por debajo
  de 20 000 triángulos.

### C12 — La puerta del calabozo (referencia 7)

- **Qué:** en las habitadas, en la puerta de la recámara del calabozo (la de las celdas), una puerta de tablas abierta
  contra la pared, con luz que se filtra entre las tablas si hay antorchas adentro. Es un decorado `CDEC_DOOR` que no
  tapa el paso.
- **Archivos:** `caves.c` y `.h` con su prueba (va en una puerta de esa sala y no la tapa), `voxcave.c`,
  `cave_scene.c`.
- **Se prueba:** `--cueva forajidos:puerta --antorcha --dron 7 --camara 0.15` (el lugar `puerta`, en `DECOR_SPOTS`).
- **Hecho cuando:** la puerta queda junto a la abertura, sin cerrarla.

### C13 — Las escaleras talladas (referencia 8)

- **Qué:** en las cuevas del imperio, los pasillos de subida (los que no son la bajada) con más de 0,3 de pendiente
  muestran escalones de piedra en el suelo, y donde entran a una sala, un arco labrado. Solo se ve: se sigue
  caminando por la rampa de `cave_floor`.
- **Archivos:** `cave_scene.c` (y `voxcave.c` para el arco).
- **Se prueba:** el pasillo del imperio más empinado (`--cueva imperio:pasilloN --antorcha`).
- **Hecho cuando:** los escalones siguen la pendiente sin que el jugador flote ni se hunda más de un escalón.

### C14 — La boca vista desde adentro (referencia 11)

- **Qué:** `draw_mouth` (`cave_scene.c`):
  - de día, la abertura encandilada, con la forma del arco;
  - unos rayos de luz que entran (planos translúcidos aditivos, del arco hacia el vestíbulo) y motas de polvo;
  - de noche, la luz azul de la luna, apenas.
- **Archivos:** `cave_scene.c`.
- **Se prueba:** `--cueva abandonada:boca` de día, y con `--minuto` de noche.
- **Hecho cuando:** desde el vestíbulo, la salida se ve como en la referencia, sin tapar al jugador.

### C15 — El rendimiento (para Android)

- **Qué:** con el diagnóstico en pantalla (`draw_diagnostics` en `src/main.c`), las cifras de la escena (`cs_stats`):
  los trozos dibujados, los triángulos y los modelos. Con eso, el usuario mide en la tableta. Si hace falta, se ajustan
  `CS_VIEW` y lo fino de los anillos.
- **Archivos:** `src/main.c` (solo `draw_diagnostics`; nada más de ese archivo), `cave_scene.c`.
- **Se prueba:** el diagnóstico adentro de una cueva grande (`--cueva imperio:0`).
- **Hecho cuando:** las cifras se ven en pantalla; el PR dice cuántos cuadros por segundo da la PC con OpenGL por
  software, antes y después.

### C16 — Las capturas y los documentos

- **Qué:** las capturas de las cuevas para la documentación:
  - en `tools/capturas.sh`: un pasillo, una estalagmita, el lago y una pintura;
  - las de `docs/capturas/cueva_*`, de nuevo;
  - `docs/CAPTURAS.md`;
  - en este plan, cada tarea marcada como hecha, con su PR.
- **Archivos:** `tools/capturas.sh` (solo agregar líneas), `docs/capturas/`, `docs/CAPTURAS.md`, este documento.
- **Hecho cuando:** cada captura está abierta y su texto dice lo que se ve. **Hito: v0.14.3** (lo publica Claude).

---

## 5. Estado

| Tarea | Estado | PR |
|---|---|---|
| Base (planta, explorar, escena, voxeles) | hecha (v0.14.0) | la de Claude |
| C1 a C16 | pendientes | |
