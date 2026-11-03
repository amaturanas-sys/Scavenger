# Plan: el sistema táctico y las cuevas (v0.12 y v0.13)

> **Estado:** hecho. Las siete fases se hicieron en siete PR apilados: A1 en el #138 (la v0.12.0, que no llegó a publicarse
> sola) y de A2 a B3 del #139 al #144. La revisión final los corrigió y completó en la rama del #138, y salen juntas en la
> **v0.13.0**. Lo que queda pendiente está marcado como tal en cada fase.

Pedido (2 de noviembre de 2026): cinco hojas del cuaderno del usuario y un documento que las resume («actualización maestra»). Las hojas están en [`docs/hojas/`](hojas/):

| Hoja | Qué dice |
|---|---|
| [Sistema de equipamiento y combate](hojas/equipamiento_combate.jpg) | La fila numérica elige lo que se tiene en la mano; Tab cambia lo que va en ella (el cursor y un número). H I J K según la espada y el escudo; 1 enfunda la espada, 2 guarda el escudo. |
| [Otros ítems, sigilo y armas de rango](hojas/barra_sigilo_rango.jpg) | Beber, el lazo, el gancho de asedio y la montura desde la barra. G agarra por la espalda: H ejecuta, I empuja. Con armas de rango, el clic elige al de delante y cambia a la izquierda o la derecha; mantenerlo 1 s lo suelta. |
| [Selector de objetivos](hojas/selector_objetivos.jpg) | El personaje mira siempre al objetivo. 5 s de día, 7 s de noche, 3 s con luz. C para cubrirse antes de ser visto; disparar el arco desde la cobertura. |
| [Cuevas y mazmorras](hojas/cuevas_planta.jpg) | El umbral de roca y la cara plana que finge el túnel. La Cisterna Magna en el centro y las recámaras alrededor, a distintas alturas; la bajada desde el umbral, empinada y sin vuelta. |
| [Los tipos de cueva](hojas/cuevas_tipos.jpg) | Animales, bestias terribles (pendiente), forajidos, tribus, imperios y abandonadas; lo que hay en cada una; los prisioneros que se unen o traicionan. |

---

## Lo decidido (2 de noviembre de 2026)

1. **Las teclas de las hojas:** `H` ataque, `I` parry, `J` bloqueo, `K` carga o patada; `Tab` abre el inventario; el menú de acciones pasa a `L`, que quedó libre. El objetivo siguiente pasa a `O` (de objetivo; `Mayús+O`, el anterior; `Ctrl+Q` y `Ctrl+E` siguen).
2. **`C` contextual (A4):** junto a un obstáculo te cubre; lejos de todo, sigue siendo sigilo.
3. **Los 5 s, 7 s y 3 s (A3):** lo que tarda en descubrirte un enemigo que te ve: más de noche, menos si llevas la antorcha. Da tiempo a cubrirse, como en la hoja.
4. **El clic para elegir objetivo (A3):** solo con un arma a distancia en la mano. Con arma de mano, el clic sigue golpeando y bloqueando.
5. **Las cuevas, en C desde la semilla del mundo** (`src/sim/caves.c`), no con un script de Python que exporta JSON fijo leído con cJSON. Así cada mundo tiene las suyas (como los ríos), no se suma una biblioteca ni archivos que empaquetar en Android, y las reglas de la planta se prueban en `sim_tests`. Python queda, si hace falta, para dibujar los planos en `docs/`.
6. **Dónde va el código:** las reglas en `src/sim/` (C puro, con pruebas en `tests/test_main.c`), el dibujo en `src/game/` y `src/world/`, como el resto del juego. `src/entities/` solo dibuja personajes. Sin `malloc` en el bucle: arreglos fijos.

## Lo que ya había (antes de la v0.12.0)

| Hoja | En el juego | Qué faltaba |
|---|---|---|
| Cuatro botones | `H` golpe · `J` bloqueo · `K` parry · `L` carga o patada (v0.5.0) | Pasar a H I J K, según la espada y el escudo |
| 1 enfunda, 2 escudo | Los números empuñan y enfundan, pero el arma y el escudo iban juntos («sable y escudo» en la casilla 1) | La espada y el escudo por separado |
| Barra rápida | 9 casillas: beber, vendar, antorcha, armas y habilidades | La décima casilla, asignar desde el inventario, el odre, el lazo, el gancho y la montura |
| Inventario | `I` abre una cuadrícula (flechas o WASD) | `Tab` lo abre; un número asigna lo elegido a esa casilla |
| Agarre (`G`) | Rehén o escudo humano por la espalda; `K` ejecuta, `L` empuja (v0.5.1) | `H` ejecuta, `I` empuja |
| Selector | `Tab` en combate, clic sobre el enemigo, `Ctrl+Q`/`Ctrl+E` | Los clics con arma a distancia, mirar siempre al objetivo, los animales |
| Detección | Instantánea al entrar en la vista del enemigo (de noche ve menos) | Los 5 s / 7 s / 3 s |
| Cobertura | Agacharse (`X`) junto a rocas y matas hace más difícil que te vean | `C`: pegarse al obstáculo, asomarse a disparar y volver |
| Lazo y gancho | Desde la columna de acciones; el gancho escala muros y murallas | Desde la barra; el gancho también en torres, y sube por la cuerda |
| Cuevas | Solo de adorno: las guaridas (una cueva entre rocas) y los farallones del desierto | Todo |

---

## Fase A — Máquinas de estado (v0.12.x)

### A1 — Las manos (v0.12.0, publicada con la v0.13.0) — hecha

**Cómo quedó:**
- **Las teclas** (`src/sim/keymap.c`): `H` ataque, `I` parry, `J` bloqueo, `K` carga o patada; `Tab` inventario, `L` menú de acciones, `O` objetivo (`Mayús+O`, el anterior). Ya ninguna tecla hace dos cosas según el contexto (antes, `Tab` era objetivo en combate y menú fuera, y `Mayús+L` carga o flecha). `V` y `Z` siguen libres. El minijuego de los peligros (`J K L U I O`) no choca: atrapado, el resto de la entrada no cuenta.
- **Cada mano por separado** (`src/sim/actions.c`): `Hands.right_away` y `Hands.left_away` guardan lo de una mano mientras lo de la otra sigue fuera. En cero (las partidas de antes), las dos manos siguen a `sheathed`, como siempre. `hands_out` dice si lo de una mano está fuera; `hands_drawn` da lo que se tiene de verdad en las manos; `hands_toggle_hand` guarda o saca solo una mano; `hands_slot_of` dice en qué mano está un objeto.
  - Empuñar algo saca solo eso (la antorcha en la izquierda no desenfunda la espada).
  - Enfundar todo sigue existiendo: con algo fuera lo guarda todo, y otra vez lo saca todo (las empuñaduras de la barra y la acción del menú).
  - Pasar el arma a la otra mano lleva consigo si estaba guardada.
- **La barra de serie** (partidas nuevas): `1` la espada (el mejor sable que tengas), `2` el escudo, `3` el odre, `4` el arco, `5` la lanza y el escudo, `6` la antorcha. Una casilla con un arma o un escudo lo pone en su mano; si ya está en ella, lo guarda o lo saca sin tocar la otra. Las partidas de antes conservan su barra: su casilla «sable y escudo» enfunda los dos, y `Alt+número` la cambia.
- **Lo que hacen los cuatro botones:**

  | En la mano | `H` | `I` | `J` | `K` |
  |---|---|---|---|---|
  | La espada sola | golpe | parry (el del arma) | parar con el arma | patada |
  | El escudo solo (la espada enfundada) | puñetazo | parry con el escudo | cubrirse con el escudo | carga con escudo |
  | La espada y el escudo | golpe | parry con la espada | cubrirse con el escudo | carga con escudo |
  | Nada | puñetazo | la llave | — | patada |
  | El arco (o ballesta, honda) | tensar y disparar | — | bajar el arma | patada |

- **El parry con el escudo** (`PARRY_SHIELD`, `hands_parry_kind` en `src/sim/melee.c`): solo con el escudo fuera. Lo desvía con el escudo y lo aturde con el canto (la mitad del daño de un golpe de escudo, sin tumbarlo). Con el arma fuera, el parry es el del arma aunque lleve escudo, como dice la hoja.
- **La espalda, el rehén y los abatidos:** `H` por la espalda (la ejecución silenciosa), `H` ejecuta al rehén, `I` lo empuja (cae aturdido), `H` remata al abatido. Con un rehén, `K` no hace nada: las manos están ocupadas.
- **El arco** (a dos manos) guarda solos la espada y el escudo, como en la hoja; después, el escudo aparta el arco.
- **El dibujo:** la espada guardada va al cinto y el escudo a la espalda; la animación sigue a lo que está fuera; la antorcha guardada se apaga.
- **El HUD:** las cuatro casillas en el orden `H I J K`, con «Puñetazo» y «Parry con escudo»; la columna de acciones muestra `L` para el menú; la pantalla de controles (`F1`), el instructivo, los avisos y el README con las teclas nuevas.
- **Pruebas:** `test_keymap_four_buttons` (las teclas nuevas; ninguna tecla hace dos cosas) y `test_hands_each_hand` (cada mano, las partidas de antes, el arco, pasar de mano, el parry con el escudo). En el juego: `--manos escudo|espada|nada`.

### A2 — La barra rápida (v0.13.0) — hecha

- **10 casillas,** `1` a `9` y `0` (la casilla 10 es la tecla `0`; los textos dicen «0», no «10»). Las partidas de antes
  (9 casillas) cargan con la décima vacía.
- **Asignar desde el inventario:** `Tab` lo abre; con lo elegido (flechas o WASD), un número lo pone en esa casilla. En la
  tableta, la fila de las diez casillas al pie del inventario se toca. Solo entra lo que se usa desde la barra (beber,
  vendar, la antorcha, lo que se empuña, el lazo, el gancho y la montura); lo demás avisa «no se usa desde la barra
  rápida».
- **El odre:** junto a un pozo o al agua, lo llena con la acción de siempre (`ACTION_FILL_WATER`, 3 s y sus reglas); en
  otro lado, bebe. La columna «Beber» y `N` beben siempre.
- **El lazo** va al animal salvaje elegido (A3) si está a tiro; si no, al de delante.
- **El gancho** lanza la trepa al muro que tengas delante (los muros de la tribu y la muralla de una ciudad), como desde
  el menú. **La montura** ensilla al animal domado que tengas cerca.
- **Pendiente:** la comida desde la barra (el jugador todavía no tiene hambre); el gancho en torres y la cuerda como
  tirolesa, que dibuja la hoja.

### A3 — El objetivo (v0.13.0) — hecha

- **Qué se elige:** enemigos y animales **salvajes** (los de la tribu, el ganado y las monturas, no). `Combat.target_lock`:
  el enemigo `i` es `i + 1`, el animal `i` es `-i - 1`, `0` es ninguno (las partidas de antes cargan igual).
- **`O`** (`Mayús+O`, el anterior) y `Ctrl+Q`/`Ctrl+E` recorren a los que ves, de izquierda a derecha según la cámara.
- **El clic o el toque sobre una silueta** la elige, con cualquier arma (en la tableta es la forma de elegir); ese clic no
  golpea. **Con un arma a distancia en la mano** (decisión 4), además: el clic izquierdo pasa al de la izquierda, el
  derecho al de la derecha, y mantener cualquiera 1 s (`TARGET_RELEASE_HOLD`) suelta el objetivo. Con arma de mano, el
  clic fuera de una silueta golpea y el derecho bloquea; con arco, el disparo es `H` o su casilla.
- **Mirar siempre al objetivo:** el personaje lo mira y camina de costado; corriendo, a caballo o trepando, no.
- **Se pierde** al morir, a más de 25 m (`TARGET_RANGE`) o tras 6 s sin verlo (`TARGET_UNSEEN_S`; el relieve, una roca o
  un muro tapan). **Cubierto (A4) no se pierde.**
- **La línea de visión** mira el relieve y, a menos de 3 m del suelo, las peñas del mundo (roquedales, farallones, torres
  de arenisca, seracs). El obstáculo de la cobertura (A4) tapa aparte.
- **Lo que tarda en reconocerte** un enemigo que te ve (decisión 3): 5 s de día, 7 de noche, 3 con la antorcha; de cerca,
  menos (`stealth_recognition_seconds`: a 2 m, la décima parte). Mientras, «?» sobre él; al reconocerte, «!». Si te
  cubres (A4) antes de que te reconozca, va adonde te vio por última vez y, si allí no te encuentra, desiste.
- **Pendiente:** nada de la hoja.

### A4 — A cubierto (v0.13.0) — hecha

- **`C` contextual** (decisión 2): junto a un obstáculo te pega a él (agachado y en sigilo, «OCULTO»); lejos de todo,
  sigue siendo sigilo. Sirven los obstáculos **del mundo** (`terrain_cover_near`: troncos, rocas grandes, matas altas y
  las peñas) y los que se ponen (vegetación, rocas, ruinas, defensas y viviendas).
- **No te ven desde el otro lado:** el obstáculo tapa la línea entre quien mira y tú (`stealth_cover_blocks`, una prueba
  de segmento contra el disco del obstáculo).
- **Moverte o `C` te saca;** golpear cuerpo a cuerpo, también.
- **Con el arco:** `H` te asoma y apunta al objetivo; al soltar disparas y, si sigues junto al obstáculo, vuelves a
  cubrirte.
- **Al cargar una partida** no se está a cubierto (decisión de la revisión: el obstáculo puede no estar).

---

## Fase B — Cuevas (v0.13.0) — hecha

### B1 — La boca y la planta

Cada guarida del mundo tiene su cueva (`src/sim/caves.c`, desde la semilla y su índice: sin archivos ni bibliotecas).
En las coordenadas de la cueva, la boca está en (0, 0) y +z va hacia adentro.

- **Afuera,** la boca es un peñasco con un túnel que se oscurece hacia el fondo (`src/game/world_game.c`). Cerca de la
  boca no crecen árboles (12 m) y la cámara no se mete en la roca. **Al caminar hacia adentro por la boca, se entra.**
- **El vestíbulo:** tras la boca, una cámara chica y llana (3 m de radio). De su borde salen, cada uno por su lado, los
  tres pasillos de la boca: las dos salidas (a las recámaras `a` e `i`, los extremos del anillo) y la bajada.
- **La Cisterna Magna:** la sala más honda (7 a 9 m de radio, el doble de alto). **La bajada** llega a ella desde el
  vestíbulo: es la más empinada (1,1 a 1,3 de pendiente; todos los demás pasillos, 0,8 o menos) y **no se sube**
  (`cave_step_ok`).
- **El anillo:** de 3 a 9 recámaras alrededor de la Cisterna y más altas (las de los extremos, a 4 m bajo la boca; las del
  fondo, 1,5 m sobre la Cisterna), unidas una tras otra. La Cisterna sube al anillo por el fondo (con un anillo par, a sus
  dos recámaras más hondas; con uno impar, a la del medio), así que de ella se sale recorriendo el anillo hasta `a` o `i`.
- **Las reglas** (`cave_layout_valid`, probadas en `test_caves` con tres semillas): las salas no se pisan ni pisan el
  vestíbulo; todo se alcanza desde la boca y de todo se vuelve a ella; la bajada es la única sin vuelta y la más
  empinada (las demás, 0,8 o menos); los tres pasillos de la boca no se pisan a 2 m del vestíbulo; lo de la pared no
  tapa pasillos; la boca, en seco; determinista.
- **Adentro** (`src/game/cave_game.c`) es un espacio aparte: su cámara (que no sale de la roca ni sube sobre la bóveda),
  su oscuridad y su luz (por vértice: las antorchas y la fogata encendidas, la boca de día, la antorcha del jugador y un
  poco alrededor de él). El plano de la cueva reemplaza al minimapa y muestra las salas que ya viste. Afuera el mundo
  espera: el combate, la fauna, los peligros, los desastres y las ciudades; el reloj, la sed, la salud (sangra y sana),
  el inventario, la barra rápida y la antorcha siguen. Para el mundo, el jugador queda en la boca: **allí se guarda la
  partida y allí aparece al cargarla.** Se sale caminando por la boca, sin girar de golpe.

### B2 — Lo que hay adentro

Cada cosa es un objeto con su sala y su lugar (`CaveObject`, hasta 64 por cueva: lo que se toma o se rescata, primero).
Lo de la pared (celdas, antorchas, vetas, depósitos, pinturas, telarañas) no tapa la entrada de un pasillo. Qué es cada
uno (la decisión de la revisión: nada declarado que no se vea o no se use):

| Objeto | Dónde | Qué es |
|---|---|---|
| Estalactitas, estalagmitas | todas | se ven (las estalagmitas cortan el paso) |
| Telarañas, insectos, sangre, charcos | todas | se ven (los insectos se mueven) |
| Huesos | todas (más en las de fieras) | **se toman** (F): huesos |
| Vetas | todas | **se toman**: mineral de hierro o cobre |
| Nidos | fieras y abandonadas | **se toman**: plumas (los huevos quedan, a la vista: la fiera cría) |
| Presas | fieras | se ven (la carroña) |
| Armas abandonadas | fieras y abandonadas | **se toman**: una daga, un hacha de mano, una lanza o un cuchillo |
| Botín | la Cisterna de las de fieras y abandonadas | **se toma**: gemas o una joya de plata |
| Antorchas, fogata | las habitadas (encendidas) y las abandonadas (apagadas) | dan luz; las apagadas **se encienden** con la antorcha propia (F) |
| Columnas de ladrillo, pinturas | las habitadas (y las ruinas de las abandonadas) | las columnas cortan el paso; las pinturas **se miran** (F: lo que cuentan) |
| Cofres | las habitadas (alguno en las abandonadas) | **se abren**: plata, granates y flechas (forajidos); pieles, queso y carne seca, turquesas (tribu); oro, jade y lapislázuli (imperio) |
| Ánforas | las habitadas (alguna en las abandonadas) | **se toman**: airag (en el imperio, un saco de grano) |
| Depósitos de armas | las habitadas | **se toman**: un hacha de mano, una lanza o un casco (en el imperio, cascos de cuero y de bronce) |
| Celdas | las habitadas (vacías en las abandonadas) | con un **prisionero** (B3) |

Lo tomado, encendido o rescatado queda en `GameActions.cave_done` (un bit por objeto), que se guarda.

### B3 — Los habitados

- **Los tipos** (desde la semilla): de fieras (40 %), abandonadas (15 %), de forajidos (20 %), de una tribu (12 %) y
  almacenes del imperio (13 %). Las fieras de la guarida solo rondan las de fieras y las abandonadas.
- **Los guardias:** al acercarte a menos de 38 m de una habitada, una vez por día salen a la boca (forajidos con un
  arquero; guerreros de la tribu; la guardia imperial con un ballestero) y pelean con el combate de siempre. **Mientras
  sigan en pie junto a la boca, no te dejan entrar.** Adentro no hay combate: los enemigos de la cueva son los que salen a
  la boca (decisión de la revisión: el combate del juego vive en el terreno de afuera).
- **Los prisioneros:** en las celdas del calabozo (una recámara del fondo), un pastor, un cazador, un herrero o un
  forajido. F abre el diálogo: lo que dice, si se lo ve sincero o con «mirada de lobo», y dos opciones: romper las
  cadenas o dejarlo. Liberado, **se une a la tribu** con su oficio (pastor, cazador, herrero, soldado) y más lealtad, y te
  espera en la boca; o **te traiciona** (`cave_betray_percent`: 10 % el pastor, 18 % el cazador, 14 % el herrero, 55 % el
  forajido; +15 en una cueva de forajidos, −5 en la de una tribu; entre 5 y 90): te golpea, te roba hasta tres de algo de
  la mochila (o de los bolsillos) y huye. Con la tribu llena, se va por su cuenta.
- **Las antorchas y la oscuridad:** las habitadas están encendidas; las de fieras y las abandonadas, a oscuras: sin
  antorcha se ve apenas alrededor (la entrada lo avisa). Afuera, de noche, las antorchas de la boca de las habitadas
  alumbran.
- **El tipo B (bestias terribles)** queda pendiente, como dice la hoja.

### Pruebas en el juego

`--cueva tipo[:lugar]` (tipo: `animal`, `abandonada`, `forajidos`, `tribu`, `imperio`; lugar: una sala o `boca`, `celda`,
`dialogo`, `nido`, `huevos`, `presa`, `botin`, `arma`, `cofre`, `anfora`, `deposito`, `pintura`, `fogata`, `antorcha`,
`veta`, `columna`), `--cueva-f` (F en el cuadro 20; con el diálogo abierto, romper las cadenas), `--antorcha` (encendida
en la mano), `--caminar adelante|atras` e `--ir boca-<tipo>`. Las capturas: [CAPTURAS.md](CAPTURAS.md#las-cuevas).

---

## Cómo se leyeron las hojas
- **La planta:** las recámaras forman un anillo alrededor de la Cisterna Magna (a → b → … → i). La bajada empinada va del umbral a la Cisterna y no se sube, así que se sale recorriendo el anillo hasta `a` o `i`, que dan al umbral.
- **La cara plana del umbral:** una superficie con la profundidad pintada que cambia con el punto de vista (paralaje), para que parezca un túnel hondo sin modelarlo; al entrar en ella se entra en la cueva.
- **Los 5 s, 7 s y 3 s:** el reloj junto a la línea de vista del enemigo de la hoja del selector («!» sobre él); el jugador se cubre con `C` antes de ser visto y el enemigo queda con «?».
- **A distancia, `H` dispara** al objetivo elegido con la caída calculada, como ya hacía.
