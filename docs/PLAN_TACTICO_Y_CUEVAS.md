# Plan: el sistema táctico y las cuevas (v0.12 y v0.13)

> **Estado:** A1 (v0.12.0) hecha. Siguen A2, A3 y A4 (v0.12.x) y las cuevas, B1 a B3 (v0.13.x). Cada fase va en su propio PR, con CI en verde, y sale como una versión que se puede probar en la tablet.

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
| Barra rápida | 10 casillas: beber, vendar, antorcha, armas y habilidades | Comida desde la barra; lo demás ya está en A2 |
| Inventario | `I` abre una cuadrícula (flechas o WASD) | `Tab` lo abre; un número asigna lo elegido a esa casilla |
| Agarre (`G`) | Rehén o escudo humano por la espalda; `K` ejecuta, `L` empuja (v0.5.1) | `H` ejecuta, `I` empuja |
| Selector | `Tab` en combate, clic sobre el enemigo, `Ctrl+Q`/`Ctrl+E` | Los clics con arma a distancia, mirar siempre al objetivo, los animales |
| Detección | Instantánea al entrar en la vista del enemigo (de noche ve menos) | Los 5 s / 7 s / 3 s |
| Cobertura | Agacharse (`X`) junto a rocas y matas hace más difícil que te vean | `C`: pegarse al obstáculo, asomarse a disparar y volver |
| Lazo y gancho | Desde la columna de acciones; el gancho escala muros y murallas | Desde la barra; el gancho también en torres, y sube por la cuerda |
| Cuevas | Solo de adorno: las guaridas (una cueva entre rocas) y los farallones del desierto | Todo |

---

## Fase A — Máquinas de estado (v0.12.x)

### A1 — Las manos (v0.12.0) — hecha

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

### A2 — La barra rápida (v0.12.1)
- **Hecho:** 10 casillas: `1` a `9` y `0`.
- **Hecho:** `Tab` abre el inventario; con el cursor (flechas o WASD) y un número, lo elegido va a esa casilla.
- **Hecho:** el odre bebe; junto al agua o a un pozo, lo llena.
- **Hecho:** el lazo apunta y se lanza al animal elegido (el objetivo de A3, o el de delante).
- **Hecho:** el gancho de asedio apunta a lo alto de un muro o una torre; la cuerda te sube, como en una tirolesa.
- **Hecho:** la montura ensilla al animal domado que tengas cerca.
- **Pendiente:** las vendas y la comida desde la barra requieren una acción de consumo del jugador; no existe todavía una mecánica de hambre del jugador.

### A3 — El objetivo (v0.12.2)
- Con un arma a distancia en la mano: clic izquierdo o derecho elige al de delante; los clics siguientes pasan al de la izquierda o al de la derecha; mantener 1 s lo suelta. En la tableta, un toque elige y mantenerlo lo suelta.
- Con objetivo, el personaje lo mira siempre y se mueve de costado.
- También los animales (cazar).
- La línea de visión respeta el relieve, las rocas y los muros.
- Lo que tarda en descubrirte un enemigo que te ve: 5 s de día, 7 s de noche, 3 s si llevas la antorcha (menos de cerca). «!» cuando te descubre; «?» si te pierde de vista: va a buscarte donde te vio.

### A4 — A cubierto (v0.12.3)
- `C` junto a una roca, un muro o un árbol te pega a él; lejos de todo, `C` sigue siendo sigilo.
- Cubierto, no te ven del otro lado y el objetivo sigue elegido.
- Con el arco, mantener `H` te asoma y apunta al objetivo; al soltar disparas y vuelves a cubrirte.

---

## Fase B — Cuevas (v0.13.x)

### B1 — La boca (v0.13.0)
- **El generador** (`src/sim/caves.c`, desde la semilla del mundo): las cuevas en roca, laderas y farallones, nunca en zonas inundables. Las guaridas de hoy pasan a ser cuevas de animales.
- **El umbral:** un marco de roca con la textura del bioma y, detrás, la cara plana que finge el túnel y cambia de aspecto con el punto de vista del jugador.
- **La planta:** la Cisterna Magna en el centro y de 3 a 9 recámaras alrededor, a distintas alturas, unidas por pasillos; la bajada desde el umbral, la más empinada y sin vuelta.
- **Adentro es un espacio aparte** (el terreno no se puede ahuecar): oscuro, con la antorcha del jugador y las de las paredes.
- **Pruebas:** todas las recámaras se alcanzan, la bajada no se sube, siempre hay salida, nunca inundada.

### B2 — Lo que dejan los animales (v0.13.1)
- **Lo común a todas:** estalactitas, estalagmitas, telarañas e insectos, vetas minerales, sangre en las paredes o el suelo, huesos de presas y humanos, charcos, y lo que dejó alguien que murió ahí.
- **Tipo A (animales: osos, lobos, hienas, pumas, tigres):** nidos, huevos, presas, armas abandonadas y algún botín en la Cisterna.
- **Tipo F (abandonadas):** lo de las C, D o E en ruinas y abandono, habitadas por animales oportunistas.

### B3 — Los habitados (v0.13.2)
- **Tipos C, D y E (forajidos, tribus, imperio):** pinturas rupestres, restos de fogatas, antorchas encendidas en las paredes, paredes y columnas de ladrillo, cofres, ánforas, cerámica, esculturas, depósitos de armas y armaduras, el calabozo con cadenas y grilletes.
- **Los prisioneros:** al rescatarlos, una tirada decide si se unen a la tropa o te traicionan e intentan saquearte. Quién es cuenta: un forajido traiciona más que un pastor.
- **El tipo B (bestias terribles)** queda pendiente, como dice la hoja.

---

## Cómo se leyeron las hojas
- **La planta:** las recámaras forman un anillo alrededor de la Cisterna Magna (a → b → … → i). La bajada empinada va del umbral a la Cisterna y no se sube, así que se sale recorriendo el anillo hasta `a` o `i`, que dan al umbral.
- **La cara plana del umbral:** una superficie con la profundidad pintada que cambia con el punto de vista (paralaje), para que parezca un túnel hondo sin modelarlo; al entrar en ella se entra en la cueva.
- **Los 5 s, 7 s y 3 s:** el reloj junto a la línea de vista del enemigo de la hoja del selector («!» sobre él); el jugador se cubre con `C` antes de ser visto y el enemigo queda con «?».
- **A distancia, `H` dispara** al objetivo elegido con la caída calculada, como ya hacía.
