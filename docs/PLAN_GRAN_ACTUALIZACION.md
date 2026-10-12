# Plan: la gran actualización (v0.5 a v0.9)

> **Estado:** fases 0 (v0.4.4), 1 (v0.5.0), 2 (v0.5.1), 3 (v0.6.0), 4 (v0.6.1) y 4b (v0.6.2) hechas; las demás, por hacer. Cada fase va en su propio PR, con CI en verde, y sale como una versión que se puede probar en la tablet. Las fases están ordenadas por dependencia: el combate primero, porque la tribu, la economía y los asentamientos lo usan.

Pedido (5 y 6 de octubre de 2026):
- combate más ágil, con selector de objetivo y cuatro botones (H J K L);
- un mapa de teclas nuevo: C sigilo, X agacharse, 1-9 empuñar y enfundar, F interactuar con todo;
- obras ubicadas en una vista aérea del campamento;
- golpes por la espalda;
- abatidos que despiertan y huyen;
- seguidores y guardias que pelean;
- la tribu administrada desde el jefe del campamento;
- economía de los campamentos;
- caballos;
- armas icónicas de la estepa y su forja;
- economía y ejército de los asentamientos agrarios.

---

## Antes de empezar: partidas guardadas con versión

Casi todas las fases agregan estado a estructuras que se guardan (`Combat`, `GameActions`, `Troop`, `Enemy`, `CampSite`). Hoy la partida se rechaza si cambia el tamaño de cualquiera de ellas (`layout_hash` en `src/game/save_game.c`). Con tantos cambios, las partidas se perderían en cada versión.

**Fase 0 — Formato de partida por bloques (v0.4.4) — hecha:** el formato, las reglas para cambiar un struct que se guarda y las partidas de referencia están en [PARTIDAS.md](PARTIDAS.md). Cada bloque lleva el esquema de sus campos, así que agregar, quitar o mover campos (a cualquier profundidad) no pierde partidas. La versión del bloque queda para las migraciones que el esquema no resuelve solo.
- **Formato:** la partida pasa a ser una lista de bloques `{etiqueta de 4 letras, versión, tamaño, datos}`, como los bloques opcionales que ya usan `death_game` (`DGT1`) y `hud_game` (`QBR1`).
- **Lectura:** cada módulo lee su bloque. Si la versión es vieja, la migra o completa con valores por defecto; si el bloque no está, arranca de cero.
- **Compatibilidad:** las partidas actuales (formato plano) se leen con el lector viejo y se reescriben en el nuevo al guardar.
- **Test:** guardar con la versión N y cargar con la N+1, con estructuras más grandes.
- **Costo:** bajo-medio. Destraba todo lo demás.

---

## Fase 1 — Combate de cuatro botones y selector de objetivo (v0.5.0) — hecha

**Cómo quedó** (lo que cambió respecto de lo planeado, o lo que el plan dejaba abierto):
- **Mapa de teclas en un solo lugar:** `src/sim/keymap.c` (C puro, con tests). El juego pregunta por acciones (`input_action_pressed`), no por teclas. Ctrl+letra queda para los atajos del sistema (Ctrl+H, Ctrl+P, Ctrl+M).
- **El golpe enemigo se anuncia:** el rival se detiene (el jinete no, para no perder la inercia del galope) y levanta el arma 0,4 s antes de pegar (más en el golpe pesado, menos con armas cortas). El parry gana en los últimos 0,25 s: un rombo dorado destella sobre el rival y en la casilla K. A destiempo, 0,6 s expuesto; entre parry y parry, 0,8 s.
- **Llave del parry:** sin armas o con daga es una prueba de fuerza; si el rival gana, el jugador queda desequilibrado.
- **Objetivo:** `Tab` en combate (algún enemigo vivo a menos de 25 m); fuera de combate, `Tab` sigue abriendo el menú. Un clic o un toque sobre un enemigo lo elige; un clic sobre el elegido es un golpe. Se guarda como índice + 1 (0: ninguno), así que no hizo falta migrar partidas.
- **Barra rápida:** Mayús+número pasa el arma a la otra mano; Alt+número (o el clic derecho, o una casilla vacía) elige qué va.
- **G sola** sigue siendo la mochila hasta la fase 2 (agarrar); Mayús+G es la tecla de la mochila. La ficha del gran guerrero pasó a **U** (en el inventario, U sigue cambiando de mochila).
- **Mayús+F** junto al ganado, quieto, lo sacrifica; si no, toma todo lo cercano.
- **HUD:** las cuatro casillas abajo a la derecha (cambian con el arma: arco, escudo); las habilidades, una fila más arriba y se pueden tocar. La columna suma Mochila y, con arco o ballesta en la mano, Encender la flecha. En tableta, los botones de pausa y controles subieron junto al minimapa.
- **Agacharse:** 2,2 m/s (1,2 con sigilo); visibilidad ×0,6 (×0,3 con sigilo; el sigilo solo, ×0,45 como antes) y ×0,4 más a cubierto (rocas y matas a 1,6 m, hierba alta a 1,3 m). Correr, saltar o montar lo cancelan.
- **Pruebas:** `--objetivo` (como pulsar Tab con enemigos cerca) y `--agachado`.


**Lo que hay:**
- **Golpes en `src/sim/melee.h`:** `MOVE_LIGHT`, `HEAVY`, `KICK`, `RUN_KICK`, `SHIELD_BASH`, `SHIELD_CHARGE`, `GRAPPLE`, `HOOK`.
- **Teclas repartidas:**
  - V: golpe;
  - J: patada;
  - Z: cubrirse, carga y golpe de escudo;
  - U: agarre;
  - O: gancho al escudo.
- **El blanco** es "el enemigo más cercano delante".

**Objetivo:** cuatro acciones claras, iguales con teclado, mando y toques.

| Botón | Tecla | Con arma cuerpo a cuerpo | Con arma a distancia |
|---|---|---|---|
| 1 Ataque | **H** / clic izq. | golpe (combo; mantener: pesado) | tensar y disparar al objetivo |
| 2 Bloqueo | **J** | con escudo, cubre el frente; sin escudo, para con el arma (menos eficaz, cansa) | bajar el arma (se cubre si lleva escudo) |
| 3 Parry / contra | **K** | en la ventana justa (≈0,25 s antes del golpe enemigo): engancha el escudo y lo arranca (`HOOK`), hace una llave y tumba (`GRAPPLE`) o desarma, según el arma | — |
| 4 Carga / patada | **L** | con escudo: carga que empuja (`SHIELD_CHARGE`); sin escudo: patada (`KICK` o `RUN_KICK`) | patada |

### Mapa de teclas nuevo (decidido el 6 de octubre de 2026)

Va en esta misma fase, porque los cuatro botones ocupan teclas que hoy hacen otras cosas.

| Tecla | Hace | Hoy hace |
|---|---|---|
| **C** | sigilo (alternar, como ahora) | igual |
| **X** | **agacharse** (nuevo): más bajo y lento, se esconde tras rocas, matas y la hierba alta, y es más difícil de ver que de pie; el sigilo suma | cambiar empuñadura |
| **H J K L** | los cuatro botones de combate (tabla de arriba) | enfundar · patada · animales · encender flecha |
| **1 a 9** | empuñar el arma de esa casilla de la barra rápida; la misma tecla otra vez **enfunda** (ya funciona así desde la v0.4.2) | igual |
| **F** | **interactuar** con todo: hablar con personas; tomar objetos y botín; usar estructuras (pozo, horno, guardián del campamento); rescatar; y lo que hoy hace la K con los animales (alimentar, ordeñar, despiezar, sacar miel) | hablar o tomar |
| **G** | agarrar: rehén o escudo humano (fase 2) | mochila |
| clic izq. | ataque (como H) | golpe |
| clic der. | bloqueo mientras se mantiene, si no se arrastra; **arrastrar** sigue girando la cámara | girar la cámara |

**Lo que se mueve a otro sitio:**
- **V** (golpe), **Z** (cubrirse), **U** (agarre) y **O** (gancho): quedan libres. El agarre y el gancho pasan a ser resultados del parry (K).
- **Enfundar** (H): con los números.
- **Cambiar empuñadura** (X): con los números. **Pasar el arma de mano** (Mayús+X): Mayús+número.
- **Animales** (K): con F. El sacrificio (Mayús+K) pasa a Mayús+F junto a un animal.
- **Encender la flecha** (L): sale de la columna de acciones del HUD o con Mayús+L.
- **Mochila** (G): Mayús+G y la columna de acciones.

**Cosas que revisar:**
- **Pruebas de reacción ante peligros:** usan J K L U I O. Solo aparecen atrapado (hielo, nieve, arenas), no peleando, así que no chocan; se revisa que no se disparen golpes a la vez.
- **Textos y documentos:** actualizar el instructivo, la pantalla de controles (F1), `input.c` (`input_keys_text`), las leyendas del HUD, `docs/` y las traducciones.
- **Tests de entrada:** cada acción responde a su tecla nueva y no a la vieja.

**Trabajo:**
- **`sim/melee`** (puro, con tests):
  - `parry_window(attacker)`: el enemigo anuncia el golpe con `windup` (≈0,4 s). El parry dentro de la ventana elige el resultado por el arma del jugador:
    - hacha o guja: arranca el escudo;
    - manos o daga: llave y derribo;
    - otras: desvío y apertura de 1 s.
  - Fuera de la ventana, el jugador queda expuesto.
  - **Bloqueo con arma:** absorbe el 50 % y gasta aguante.
- **Selector de objetivo (`combat_game`):**
  - Una tecla pasa al enemigo siguiente (propuesta: **Tab** en combate, con el menú de acciones en la columna del HUD; o la **Q/E** con Mayús). En tableta, se toca al enemigo.
  - Un anillo bajo el elegido y su barra de vida resaltada. Se pierde a 25 m o si muere.
  - Las armas a distancia apuntan al objetivo con la caída calculada: el arco no exige mirar con la cámara.
  - Sin objetivo, se usa el más cercano delante (como hoy).
- **HUD:** los 4 botones como casillas táctiles a la derecha, donde hoy están las habilidades; las habilidades suben una fila. Cada casilla muestra si su acción está disponible (por ejemplo, el parry destella en la ventana justa).
- **Animaciones:** usar las que existen (`bloquear`, `enganchar_escudo`, `derribo`, `patada`, `carga_escudo`). Faltan `parry` y `desvio`: se agregan a `docs/ANIMACIONES.md` y a los cuerpos procedurales.
- **Tests:**
  - el parry dentro de la ventana engancha o derriba;
  - fuera de ella, recibe el golpe;
  - el bloqueo con arma resta aguante;
  - el selector cicla y descarta a los muertos.

**Costo:** alto. Es la fase más grande de combate.

## Fase 2 — Espalda, ejecución silenciosa y rehenes (v0.5.1) — hecha

**Cómo quedó** (lo que cambió respecto de lo planeado, o lo que el plan dejaba abierto):
- **Reglas puras en `src/sim/stealth.c`**, con tests: el cono trasero, lo que ve un enemigo según de dónde vienes, la ejecución que solo se oye a 6 m, el rehén que tapa el tiro y cuándo un golpe tumba en vez de matar.
- **Ver y oír:** para que la espalda se pueda ganar, la vista de un enemigo ahora depende de dónde viene el jugador: de frente, entera; de lado, menos; por detrás solo oye (según el ruido: correr se oye, el sigilo casi nada). Un bandido nota al que llega en sigilo por detrás recién a ~1 m.
- **Notado** quiere decir que el enemigo lo tiene de objetivo; **aturdido**, desequilibrado o derribado. A caballo no se gana la espalda.
- **Letal:** mata una herida abierta (corte, mordida, quemadura) grave en la cabeza o el cuello, o muy grave en el pecho o el vientre, o quedarse casi sin sangre. Lo contundente nunca mata al tumbar. El abatido sigue sangrando en el suelo y puede morir.
- **Avisar a los suyos:** al despertar, los enemigos a menos de 30 m van a por el jugador 25 s aunque no lo vean. La huida dura 45 s.
- **Los suyos** son los del mismo bando (bandidos o culto): el culto no respeta a un rehén bandido. Retroceden a 4,5 m de cara al jugador.
- **Rematar** suma una acción de campamento nueva (`ACT_FINISH_DOWNED`), más leve que ejecutar a un prisionero; también cuenta rematarlo de un flechazo. El Kanato de Hierro lo aprueba un poco y la Dinastía de Jade lo reprueba.
- **Capturar:** el prisionero entra a la tribu con el nombre de su tipo y su número; lo que llevaba queda en el suelo como botín.
- **HUD:** sobre el enemigo con la espalda ganada, una daga; una casilla G a la izquierda de las cuatro (agarrar o soltar); K pasa a «Por la espalda», «Ejecutar al rehén» o «Rematar», L a «Empujar» y H a «Rematar» según el caso. Sobre los abatidos, una barra gris con lo que les falta para despertar.
- **G sola** agarra; la mochila queda en Mayús+G (y la columna).
- **Pruebas:** `--espalda` y `--rehen`.


- **Ganar la espalda:** el jugador está a menos de 2 m, en el cono trasero de 70° del enemigo, y no fue notado (sigilo, `noise` bajo) o el enemigo está aturdido.
  - Se marca con un ícono de daga sobre el enemigo.
  - **(3) Parry, K:** **ejecución silenciosa**. Muerte en el acto, sin alerta a más de 6 m y con animación `apunalar_espalda`.
  - **G (Agarrar):** **rehén / escudo humano**. Estado `EN_HOSTAGE`:
    - el enemigo camina delante del jugador, sujeto por el cuello;
    - los suyos no disparan (o fallan más) y retroceden;
    - se suelta con G, se ejecuta con (3) o se empuja con (4);
    - el jugador va lento y con una sola mano;
    - si recibe un golpe fuerte, lo suelta.
  - La mochila pasa a Mayús+G y a la columna de acciones del HUD (mapa de teclas de la fase 1).
- **Abatidos (knock out) contra muertos:**
  - **Hoy:** `EN_DEAD` es final.
  - **Estado nuevo `EN_DOWN`:** los golpes contundentes (maza, patada, escudo) y la vida que cae por debajo de 0 sin herida letal tumban al enemigo en vez de matarlo.
  - **Despertar:** el abatido despierta a los 30–90 s, herido y con miedo (`EN_FLEE`). Huye hacia su campamento o asentamiento y avisa a los suyos.
  - **Rematar:** (1) o (3) junto a un abatido lo remata, con su costo moral según los rasgos de la tribu, como ejecutar prisioneros en `troop`.
  - **Capturar:** la F junto a un abatido lo toma prisionero (ya existe `troop_take_prisoner`).
- **Tests:**
  - el cono trasero;
  - la ejecución silenciosa no alerta;
  - el rehén bloquea el disparo enemigo;
  - el abatido despierta y huye.

**Costo:** medio.

## Fase 3 — Seguidores, escolta y guardias que pelean (v0.6.0) — hecha

**Cómo quedó** (lo que cambió respecto de lo planeado, o lo que el plan dejaba abierto):
- **La escolta ya peleaba** (desde antes); ahora lo hace según la orden, con las reglas puras en `src/sim/squad.c` (con tests).
- **Órdenes:** `Y+1`, `Y+2`, `Y+3`. Para que no choquen, la `Y` sola llama o despide a la escolta al soltarla, y mientras está apretada los números no usan la barra rápida. Las casillas van en fila sobre la barra rápida, solo con la escolta fuera. La orden se guarda en la partida (cero: atacar, como hasta ahora).
- **Guardias:** nadie tenía `ROLE_GUARD` (no se podía capacitar), así que hacen la guardia los **soldados** que se quedan en el campamento (y `ROLE_GUARD` si alguien lo tiene), salvo el guardián y quien trabaja en una obra. Los soldados pegan un 30 % más fuerte.
- **Interceptar:** salen a 35 m de su punto de la ronda; si un enemigo entra al campamento (a menos de 40 m del centro), vuelven todos.
- **Asaltos (nuevo):** sin ellos los guardias no tenían a quién enfrentar, porque los enemigos solo aparecían lejos del campamento. Con el jugador a menos de 150 m de un campamento y tras 10 minutos de juego: 2,5 % por minuto de día y 6 % de noche. Los enemigos ahora atacan a cualquier integrante a la vista, no solo a la escolta.
- **Pobladores:** con un enemigo (o una fiera) a 30 m del campamento o de ellos, corren a la yurta, tienda o refugio más cercano y quedan escondidos (ni se ven ni se los ataca). Las yurtas del campamento inicial no son objetos del mundo; el juego le pasa sus posiciones al combate.
- **Rendimiento:** la gente del campamento a más de 150 m del jugador se actualiza cada medio segundo y no se dibuja. Los enemigos ya desaparecían a 160 m.
- **Pruebas:** `--escolta [atacar|defender|seguir]`, `--guardias` y `--asalto`.


**Lo que hay:**
- la escolta (Y) sigue al jugador y lo levanta si cae;
- los NPC tienen `health` y no pelean;
- `cb_draw_overlay` ya dibuja barras sobre los NPC.

**Trabajo:**
- **IA de combate para NPC (`sim/` puro):** elegir enemigo, acercarse, golpear con su arma y retirarse herido. Usa los mismos golpes y el mismo daño que el jugador.
- **Órdenes a la escolta (rueda o casillas del HUD, teclas Y+1..3):**
  - **Atacar:** van al objetivo del jugador o al más cercano.
  - **Defender:** se quedan junto al jugador y paran a quien se le acerque.
  - **Seguir / no atacar.**
- **Guardias del campamento:**
  - Los integrantes con `ROLE_GUARD` patrullan un anillo de 40–60 m alrededor del campamento, con puntos que rotan, y salen al encuentro de enemigos y fieras.
  - De noche llevan antorcha, la luz de `ga_lights`.
- **Pobladores:** los que no son guerreros huyen a las yurtas ante un ataque.
- **Rendimiento:** la IA de los NPC lejanos (más de 150 m) se simula sin dibujar y a menos frecuencia.
- **Tests:**
  - la escolta en modo atacar pelea;
  - en modo defender no se aleja;
  - el guardia intercepta a un enemigo que cruza el anillo.

**Costo:** alto.

## Fase 4 — Administrar la tribu desde el jefe del campamento (v0.6.1) — hecha

**Cómo quedó** (lo que cambió respecto de lo planeado, o lo que el plan dejaba abierto):
- **Nombres:** `TASK_RECRUIT` queda como está (es «explorar en busca de reclutas»); al final de `CampTaskKind` se sumaron `TASK_SCOUT_RESOURCES`, `TASK_HERD`, `TASK_HUNT` y `TASK_GUARD`. El acompañante es `CampTask.member2` (cero: va solo) y `CAMP_TASKS` pasó de 6 a 8. Las reglas son puras (`src/sim/camps.c`, con tests).
- **Ventana de pobladores:** es una página del diálogo del guardián (reemplaza a «Reclutar»): una casilla por persona con su oficio, y en la leyenda su salud, ánimo y tarea. Se marcan uno o dos y «Dar una tarea»; desde ahí también se capacita. Si hay más de 20 personas, «Más gente» pasa de página.
- **Salidas** (reclutas, exploración, caza): se resuelven por tiempo, como los viajes, pero sin `Journey` (no ocupan sus 8 lugares): quien está en una salida «no está en el mundo» (`ga_away`), igual que de viaje. Se los ve alejarse 14 s, y al volver aparecen en el borde del campamento por donde se fueron. Los que salen no aceleran con ayudantes: duran lo que duran.
- **Explorar** marca en el mapa sitios del mundo que todavía no están marcados: agua (orillas de lagos y pozos), pastos (vegas junto a los ríos que no son de grava), campamentos rivales y capitales («ciudadelas»). Los de peligro llevan `MARKER_DANGER`. Las marcas nuevas no alternan (`memmap_add_marker`) y los que fueron aprenden el camino.
- **Cazar** necesitó un objeto nuevo: `utileria.material.grasa`. La carne fresca, como la del sacrificio, se come primero o se seca al día siguiente.
- **Pastorear y guardia** se ven: los pastores rondan un pasto a 28 m del fuego y el ganado pasta con ellos; quien tiene un turno de guardia hace la ronda de la fase 3 aunque no sea soldado (y un soldado con otra tarea no hace guardia).
- **Informes:** en corto al registro y completo a la ventana (los últimos cuatro por campamento, en la partida). El texto de los diálogos admite saltos de línea.
- **Pruebas:** `--pobladores` (la ventana abierta, con alguien elegido) y `--tareas` (dos a cazar, uno a explorar, uno a pastorear y uno de guardia).

**Lo que hay:**
- el guardián del campamento (F) abre un diálogo con reclutar y capacitar (`CampTaskKind`: `TASK_RECRUIT`, `TASK_TRAIN`);
- los viajes (`sim/travel`, Mayús+Y) despachan gente y devuelven su suerte.

**Trabajo:**
- **Ventana de pobladores** (desde el jefe designado): lista con nombre, rol, salud, ánimo y tarea actual. Se elige uno o dos integrantes y se les asigna una tarea:
  - **Explorar en busca de reclutas:** el `TASK_RECRUIT` de hoy, como viaje con duración y riesgo.
  - **Explorar recursos:** viaje que revela en el mapa de memoria agua, pastizales, campamentos rivales y ciudadelas para asediar (marcas `MARKER_INTEREST`, `MARKER_DANGER`). El resultado depende del rol (`ROLE_SCOUT`) y de si va acompañado.
  - **Pastorear:** saca el ganado a pastar y a beber. Hace bajar el hambre y la sed de los animales de la tribu (`animal_domestic`) y da leche. Dos pastores rinden más y pierden menos animales ante las fieras.
  - **Cazar:** vuelve con carne, pieles, huesos, tendones y grasa, según la región y la estación. Riesgo de heridas; mejor con `ROLE_HUNTER` y acompañante.
  - **Guardia:** el de la fase 3.
- **Modelo:** en `sim/camps` agregar `TASK_SCOUT_RECRUITS`, `TASK_SCOUT_RESOURCES`, `TASK_HERD`, `TASK_HUNT` y `TASK_GUARD`, con un segundo integrante opcional. Resolución por tiempo, igual que `travel`: es simulación abstracta, sin NPC caminando por el mundo, salvo pastoreo y guardia, que sí se ven cerca del campamento.
- **Informe al volver:** en el registro y en la ventana.
- **Tests:** cada tarea produce lo esperado. Ir acompañado reduce el riesgo, y el pastoreo baja la sed del ganado.

**Costo:** medio.

## Fase 4b — Obras ubicadas en una vista aérea del campamento (v0.6.2) — hecha

**Cómo quedó** (lo que cambió respecto de lo planeado, o lo que el plan dejaba abierto):
- **Reglas puras** en `src/sim/placement.c` (con tests): la huella es un rectángulo girado con las medidas del inventario; el sitio se revisa en orden (radio, agua, pendiente, lo que pisa) y se dice el primer motivo. Los solapes se prueban con ejes separados (rectángulos girados, no cajas).
- **La cámara:** una vista en perspectiva desde arriba, un poco inclinada hacia el norte (el norte queda arriba), a unos 83 m del centro: entra el campamento entero (45 m de radio). Es fija; la huella no se aleja más de 48 m. De noche se ve como de día.
- **Fuera de todo campamento** (para fundar uno con un refugio o una tienda), la vista se centra en el jugador y no hay radio.
- **Lo que no se puede pisar:** las yurtas y la fogata del campamento inicial, los árboles, las estructuras, los tótems y los carros, y las obras en curso. No hay caminos en el campamento todavía; las cenizas no cuentan.
- **Muros:** cada tramo de 4 m es una obra (con su costo), así que `GA_MAX_PROJECTS` pasó de 8 a 24. Los muros pueden juntarse y cruzarse con otros muros (para hacer esquinas).
- **Tocar o hacer clic:** con el ratón, la obra va bajo el puntero y el clic la coloca. En la tablet, tocar otro lugar la mueve y tocarla la coloca. Las casillas de abajo a la derecha giran, alinean, aceptan y cancelan, con íconos nuevos.
- **El giro se guarda** en `BuildProject.yaw` y la estructura terminada lo conserva (antes todas miraban al mismo lado).
- **Ayuda:** una página nueva, «Las obras».
- **Prueba:** `--ubicar <obra>`.

**Lo que hay:** al mandar una obra (Tab → Obras), el proyecto se planta 6 m delante del jugador (`start_build`: `ground_ahead(t, p, 6.0f)` en `src/game/actions_game.c`). Hay que caminar hasta el sitio antes de ordenarla.

**Trabajo:**
- **Vista aérea:** al elegir una obra (desde el menú o desde el jefe del campamento de la fase 4), la cámara sube a una vista cenital del campamento: unos 60 m de radio, centrada en el campamento elegido, no en el jugador. Se ven las yurtas, la fogata, las obras en curso y el borde del campamento. Es como la vista orbital, pero local.
- **Fantasma de la obra:** su huella (el volumen del inventario o el modelo, translúcido) sigue al puntero, al dedo o a WASD.
  - **Q/E** la giran.
  - **Verde** si el sitio vale: dentro del radio del campamento, en seco, con poca pendiente y sin pisar yurtas, fuegos, otras obras ni caminos.
  - **Rojo** si no vale, con el motivo en la leyenda ("en el agua", "demasiado empinado", "encima de la yurta del jefe").
- **Confirmar:** Enter o un toque coloca la obra; Esc cancela. Los constructores (`NPC`) caminan hasta ese sitio, como hoy.
- **Ajustes útiles:**
  - las cercas y murallas se trazan con dos puntos, de un extremo al otro;
  - una rejilla opcional (Mayús) alinea las obras.
- **Tests (en `sim/` puro):**
  - la validación del sitio: agua, pendiente, solapes y radio;
  - el giro de la huella;
  - la obra queda donde se señaló.

**Costo:** medio. La cámara cenital y el fantasma son nuevos; el resto, la cola de obras y los constructores, ya existe.

## Fase 5 — Economía de los campamentos y artesanías (v0.7.0) — hecha

**Cómo quedó** (lo que cambió respecto de lo planeado, o lo que el plan dejaba abierto):
- **El bien de referencia es la plata** (`utileria.material.plata`), no las cabezas de ganado: los animales no se guardan en bolsas y la plata sí. Para que sirva de moneda pesa 50 g la unidad (antes, 4 kg como un lingote); el oro, igual. Las reglas son puras (`src/sim/trade.c`, con tests): el valor de cada bien (una tabla por id o prefijo; las joyas, por su metal), el del acopio, el precio por región (×0,7 donde abunda, ×1,5 donde escasea) y lo que se queda el mercader (paga el 80 %, cobra el 125 %).
- **Materias nuevas:** cuero, fieltro, madera labrada, adobe, puntas de hueso y piezas de arco; velas de sebo, peine, aljaba y especias. La grasa, el hueso, los tendones, el forraje, el airag y el queso seco ya estaban.
- **Recetas** (todas a mano, `CRAFT_*` al final del enum): cuero; silla, brida, alforjas, aljaba y casco, brazales y grebas laminares de cuero; puntas y flechas de hueso, peine, piezas de arco y el arco compuesto; velas, antorchas, queso seco, fieltro, madera labrada y adobe. Las botas y el gorro de fieltro ahora piden fieltro.
- **Producción por oficio** (`economy_daily_work`): cada día, una tanda por persona con oficio, de lo que sobra del acopio de su campamento y guardando una reserva (cazador: cuero y puntas; pastor: fieltro; constructor: madera labrada y adobe; cocinero: velas; curandero: ungüento; orfebre: peines). Sale en los informes del campamento y en el registro del día.
- **Obras duraderas:** casa de adobe, cabaña de troncos y choza de fieltro (`BUILD_*` al final). Abrigan a más gente y se desgastan menos (`structure_wear_scale`: adobe y piedra 0,4, troncos 0,6).
- **Comercio** (`src/game/trade_game.*`): `F` junto a tribus amigas o neutrales, aldeas, capitales, caravasares y caravanas abre un diálogo de vender y comprar. Se vende lo que se lleva (encima, alforjas, carreta, mochilas de la escolta), no lo puesto ni el acopio: lo que sobra en el campamento hay que cargarlo. Cada mercado vende lo suyo y lo de su región. Una pista abajo dice con quién se comercia.
- **Caravanas:** una por caravasar (hasta cuatro), entre él y la capital más cercana, al paso de los camellos; cada una sale a otra hora. Se dibujan con primitivas (camellos de cápsulas con jorobas y fardos, el mercader a pie y dos jinetes). Se detienen al comerciar. Asaltarlas es una opción del diálogo: aparece su escolta (un grupo nuevo de `cb_spawn_party_at`, «caravana»), baja la relación con el reino y, vencida, la carga queda en el suelo; descansa dos días y vuelve a salir de su caravasar.
- **Saqueo de campamentos rivales:** cuando sus guerreros ya salieron al paso (la emboscada de siempre), `F` junto a su fuego lo saquea si no queda nadie en pie a 40 m. Se rearma al alejarse 600 m, como la emboscada.
- **El valor a la vista:** el estado del guardián, el panel del acopio en el inventario y la leyenda de cada objeto dicen cuánto vale en plata.
- **Untar con grasa:** quedó como mantenimiento del cuero y el fieltro (si faltan pieles para repararlos, la grasa devuelve el 30 %). Untar las armas y que se gasten menos queda para la fase 7, con la forja.
- **Íconos nuevos:** monedas, balanza y camello. **Ayuda:** una página nueva, «Oficios y comercio».
- **Pruebas:** `--comerciar raiz|vender|comprar|cerca|caravana|asaltar|saquear`.

**Lo que hay:**
- el acopio (`Stockpile`) por campamento;
- la recolección diaria (`economy`);
- la forja (`CRAFT_*`) con herrero y horno;
- los recursos ya presentes: pieles, carne, hierbas, miel, leche, leña, piedra, mineral, lingote, cuerda.

**Trabajo:**
- **Materias primas nuevas en `assets/inventario.tsv`:**
  - cuero (de curtir pieles);
  - grasa animal;
  - huesos y tendones (ya hay tendones);
  - madera labrada;
  - barro/adobe;
  - forraje.
- **Cadenas de producción** (recetas en `economy`), con **valor** (precio en un bien de referencia: "cabezas de ganado" o plata):
  - **Talabartería:** pieles → cuero, y cuero → arreos, sillas, aljabas, armaduras de cuero.
  - **Hueso y asta:** puntas de flecha, peines, piezas del arco compuesto.
  - **Grasa:** velas y antorchas; untar cuero y armas (menos desgaste).
  - **Leche:** kumis/airag (ya existe el airag) y queso seco, que es comida de viaje.
  - **Fieltro:** lana → fieltro para yurtas, ropa y botas.
  - **Adobe y madera:** construcciones más duraderas del campamento.
  - **Forraje:** alimenta a los caballos en invierno.
- **Producción por oficio:** cada integrante con rol produce en el campamento según su oficio. Lo que no se usa se acumula y se puede **comerciar** con tribus amigas y caravanas (pendiente desde v0.2).
- **Saqueo** como fuente:
  - **caravanas**, nuevo: rutas entre caravasares y capitales, con escolta y carga valiosa;
  - **campamentos rivales;**
  - **ciudadelas**, en la fase 8.
- **Tests:** las recetas consumen y producen lo justo; el valor del acopio; el comercio a precio por región.

**Costo:** alto, sobre todo por el contenido (recetas, ids, iconos y traducciones).

## Fase 6 — Caballos: marcialidad y cría (v0.7.1) — hecha

La fase era grande y se partió en dos, como la 4: los caballos aquí (v0.7.1) y la migración en la fase 6b (v0.7.2).

**Cómo quedó** (lo que cambió respecto de lo planeado, o lo que el plan dejaba abierto):
- **Reglas puras** en `src/sim/riding.c` (con tests): la dispersión del tiro a caballo, la lanza en ristre, el cansancio de la montura y la maniobra de los arqueros a caballo. La cría, en `src/sim/animals.c`.
- **El rumbo del caballo:** antes el caballo miraba siempre hacia donde miraba el jinete; ahora mira hacia donde va, y el jinete se gira en la montura al apuntar. Así se puede galopar alejándose y tirar hacia atrás. Arrollar va por el rumbo del caballo.
- **Disparo parto:** no es un botón: es tirar al galope (desde 6 m/s) a más de 100° del rumbo. Abre más el tiro (×1,4 sin práctica) y, si acierta, el registro lo dice. El objetivo elegido (Tab) se pierde a 25 m, así que sirve contra el que persigue de cerca.
- **Carga con lanza:** L a caballo con una lanza la pone en ristre 3 s (antes, L a caballo no hacía nada). El primero delante del caballo recibe el golpe con el peso de la carrera. Se ve la lanza bajada.
- **Cuidado del caballo:** el aguante de la montura (`Animal.stamina`) se gasta al galope y su barra reemplaza a la del jugador. El hambre y la sed ya existían: ahora la cansan y la frenan. El forraje de invierno y el pastoreo de la fase 4 siguen igual.
- **Cría:** campos nuevos al final de `Animal` (sexo, velocidad y aguante heredados, días de cría y de preñez). En las partidas viejas el sexo se asigna solo. Hay preñez (3 días) antes del parto, y la cría tarda 4 días en ser adulta. Las monturas y el ganado se crían, menos la mula y el elefante. Lo heredado solo cuenta montado.
- **Arqueros a caballo:** `ENEMY_HORSE_ARCHER` al final del enum. Hostigan como pedía el plan y tiran peor hacia atrás. Salen de las tribus rivales, con los jinetes y en la escolta de las caravanas (en vez del arquero a pie). Los jinetes enemigos también miran hacia donde van.
- **Pruebas:** `--montado lanza|parto|agotado`, `--cria`, `--enemigos arqueros_caballo`. **Ayuda:** una página nueva, «A caballo».

**Lo que hay:**
- monturas domables (lazo, silla, montar con R);
- golpe a caballo con alcance;
- inercia del galope y arrollar;
- caballos sueltos de los jinetes derribados.

**Trabajo:**
- **Arquería montada:** disparar al galope hacia el objetivo de la fase 1 (el "disparo parto", hacia atrás), con la puntería según la velocidad y el talento.
- **Carga con lanza:** a galope tendido, la lanza derriba y hace daño por la velocidad.
- **Cuidado del caballo:** hambre, sed y cansancio (`Animal.stamina` ya existe). El forraje en invierno y el pastoreo de la fase 4.
- **Cría:** potros que crecen en el campamento. La calidad (velocidad, aguante) se hereda.
- **Enemigos montados:** los jinetes ya existen; se suman arqueros a caballo que hostigan y se alejan.
- **Tests:** el disparo a caballo pierde precisión con la velocidad.

**Costo:** alto.

## Fase 6b — Migración en caravana (v0.7.2) — hecha

**Cómo quedó** (lo que cambió respecto de lo planeado, o lo que el plan dejaba abierto):
- **Reglas puras** en `src/sim/camps.c` (con tests): qué vivienda se desarma (`camp_load_kind`), levantar y plantar (`camp_migrate`, `camp_settle`) y los pastos de cada región y estación (`pasture_quality`, `pasture_hunger`, `pasture_forage`). El rebaño de la caravana, en `src/sim/animals.c` (`FaunaCtx.caravan`).
- **La carga es abstracta:** no hace falta carreta ni alforjas. Las viviendas portátiles (yurtas, tiendas ligeras y chozas de fieltro) se cuentan en `CampSite.load` y las yurtas del campamento inicial se suman a la carga. Las obras fijas quedan en el sitio viejo. La carreta y las alforjas siguen para lo que lleva el jugador.
- **La columna** es la escolta de siempre: la gente del campamento pasa a escolta mientras dura la marcha (Y no la despide), y el rebaño sigue de más lejos y aunque el jugador esté quieto. Las salidas siguen y, al volver, alcanzan la columna; los despachados al campamento, también.
- **En marcha:** el acopio va en la columna (inventario, comida y agua). No hay asaltos ni descanso en el sitio viejo, y un desmayo se atiende donde estás. Si muere toda la columna, la caravana se pierde.
- **Plantar:** `F` junto al guardián, donde esté el jugador (seco, llano y a más de 70 m de los demás campamentos). Las viviendas se arman en corro con la puerta hacia un fogón nuevo. Si es el campamento inicial, el fuego de la tribu pasa a ese fogón.
- **La respuesta a la estación** son los pastos: el guardián avisa al cambiar la estación, los pastores juntan forraje con buen pasto y el ganado pasa hambre con pasto pobre.
- **Pruebas:** `--migrar marcha|plantar`. **Ayuda:** dos líneas en «La tribu y el campamento».

**Lo que hay:** campamentos con su acopio y su gente; fundar uno nuevo con una tienda o un refugio lejos; disolverlo (se queman las estructuras y la gente sigue al jugador); la carreta y las alforjas; la escolta que sigue al jugador.

**Trabajo:**
- **Migración en caravana:** mover el campamento entero.
  - Se levantan las yurtas, se cargan en carretas y caballos (ya hay `carreta` y alforjas) y la tribu viaja en columna detrás del jugador.
  - Al llegar se planta el campamento nuevo y el acopio viaja con ellos.
  - Es la respuesta a la estación: pastos de verano en el altiplano, de invierno en el llano.
- **Tests:** la migración conserva el acopio y a la gente.

**Costo:** medio.

## Fase 7 — Armas icónicas de la estepa (v0.8.0) — hecha

La fase era grande y se partió en dos, como la 4 y la 6: las armas aquí (v0.8.0) y la forja en la fase 7b (v0.8.1).

**Cómo quedó** (lo que cambió respecto de lo planeado, o lo que el plan dejaba abierto):
- **Los rasgos van en `WeaponStats`** (`src/sim/combat.h`, campos nuevos que no se guardan): `pierce` (el pico), `crush` (la maza), `stun` (el martillo), `clean_cut` (el sable), `mounted_only` (la lanza pesada) y `ring` (la daga de anillo). Los arcos llevan `RangedDef.saddle`: cuánto les pesa la montura. Las reglas, en `combat.c`, `melee.c`, `riding.c` y `stealth.c`, con tests.
- **Los ids:** el arco de antes es ahora `arco_simple`, y el id `arco_compuesto` pasó a ser el **reflejo**. En las partidas viejas, el arco que llevaba el jugador queda mejorado hasta que cambia de empuñadura: el equipo inicial trae el simple, y la empuñadura del arco toma el reflejo si lo tienes. El sable y la lanza de siempre se llaman ahora «sable de la estepa» y «lanza ligera». Son nuevos `arco_simple`, `akinakes`, `sagaris`, `sagaris_martillo`, `daga_anillo` y `lanza_pesada`.
- **Sable:** «no se traba» es una regla nueva: a caballo, las demás armas aprovechan la mitad de la carrera, y al galope se traban (el próximo golpe tarda 0,9 s más). El sable la aprovecha entera, más un 10 %.
- **Lanza pesada:** a pie no golpea. En ristre derriba más y, si no la paran, sigue hasta en 2 más en la línea de la carga, con la mitad de fuerza cada vez.
- **Sagaris y maza:** el pico pasa por la armadura como una punta (la malla peor). La maza tumba más y, en la cabeza, el casco para la mitad. El martillo aturde siempre que entra. Los dos sagaris enganchan escudos, en el golpe y en el parry.
- **Daga de anillo:** sin castigo en la mano torpe. Con ella en cualquier mano, las llaves son más firmes, el rehén no se suelta con un golpe cualquiera y la ejecución se oye a 3 m. La llevan los arqueros a caballo; los arqueros a pie pasan al arco simple.
- **El arco reflejo en tres etapas:** alma de madera; asta y tendón con cola de hueso, que seca 2 días en el acopio de un campamento (`CraftDef.to_stock`, `stock_dry_day`, el guardián avisa); y la cola de hueso que lo termina. Las armas de metal se forjan por ahora en los hornos que ya había; la forja móvil y el taller son de la 7b.
- **Dónde se consiguen:** recetas, botín, las tribus (arco reflejo, sagaris, akinakes, daga de anillo, lanza pesada) y las aldeas (arco simple, maza).
- **Pruebas:** `--montado pesada` (tres bandidos en fila); `--montado parto`, ahora con el arco reflejo. **Ayuda:** una página nueva, «Las armas de la estepa».

**Armas** (ids nuevos en el inventario, estadísticas en `weapon_stats` y `ranged_def`):

| Arma | Rasgos de juego |
|---|---|
| **Arco compuesto reflejo** (madera, asta, tendón) | El de hoy pasa a ser *arco simple*. El compuesto tiene más tensión y alcance, y es corto: se usa a caballo sin penalización. Se fabrica en 3 etapas (madera, asta y tendón, cola de hueso) con días de secado. |
| **Sable de la estepa** (curvo, un filo) | Corte limpio a caballo: no se traba y suma inercia. La versión temprana es la **akinakes**, espada corta recta de los escitas. |
| **Lanza ligera (2 m) / lanza pesada (4 m)** | La ligera sirve a pie y a caballo. La pesada, solo a caballo: con carga, derriba y rompe formaciones. |
| **Sagaris** (hacha con pico o martillo) | Perfora armadura (pico) o aturde (martillo). Engancha escudos en el parry. |
| **Maza** | Contundente: abate (KO) más de lo que mata, y es buena contra cascos. |
| **Daga de empuñadura de anillo** (sármata) | Arma de mano secundaria. Mejor en llaves, rehenes y ejecución silenciosa (fase 2). |

**Tests:** el disparo a caballo con el arco reflejo no pierde precisión; las recetas por etapas.

**Costo:** medio-alto. Es sobre todo contenido y datos; los modelos 3D son trabajo aparte en Nomad o Kiln.

## Fase 7b — La forja (v0.8.1) — hecha

**Cómo quedó** (lo que cambió respecto de lo planeado, o lo que el plan dejaba abierto):
- **Las reglas, en `src/sim/forge.*`** (con tests): los sitios de hierro, las estaciones de la forja (`forge_station_ok`) y la probabilidad del temple (`temper_chance`). En juego, `src/game/forge_game.*`: dibujar las vetas, descubrirlas al pasar, picarlas, la estación (`forge_station`) y la tirada del temple.
- **El hierro sale de la semilla** (`world_iron_sites`, con su propio generador): no cambia el mundo y no se guarda. Hasta 12 vetas a cielo abierto en el altiplano (a más de 700 m del centro y a 350 m una de otra) y un hierro de pantano por lago del bosque o de los fiordos. Al pasar a menos de 22 m, la veta queda marcada en el mapa; los exploradores la traen como hallazgo (`FIND_IRON`, que no es un peligro). `F` junto a ella pica (`ACTION_MINE`, 8 s): 2 de mineral, o 1 en el pantano. El herrero lo funde en el horno de acero (3 de mineral y 2 de carbón dan 1 hierro).
- **La forja móvil es un objeto** (`utileria.herramienta.forja_movil`, del horno de acero): cuenta si está a mano, como los ingredientes (bolsillos, mochila, alforjas, carreta, o el acopio del campamento o de la columna). «Se carga en un caballo» quedó en eso: va en las alforjas, la carreta o el acopio que viaja con la columna. Hace a mano puntas de flecha de hierro, cuchillos y el sable de acero con una hoja comprada, y repara lo que pide un horno. Lo que se forja en un horno sigue pidiendo el horno.
- **El taller** (`BUILD_WORKSHOP`) pide un campamento de 8 personas o más (`BuildDef.min_people`, nuevo; el menú lo muestra). Forja el sable wootz y la lanza de punta de acero, y templa. Lo de la «ciudad tomada» queda para cuando haya ciudades que tomar (fase 8).
- **El temple es una receta** (`CraftDef.fails`): sale el arma `_templado`/`_templada` o, si sale mal, `_fragil`. Las variantes son ids del inventario (las manos guardan solo el id), así que no cambia ningún struct que se guarde. Templada pega ×1,15; frágil ×1,1, pero se quiebra en un golpe que entra (5 %, 12 % si se lo paran) y se pierde. La probabilidad es 0,5 + 0,15 por herrero (hasta 2) + 0,25 × la destreza del clan, entre 0,3 y 0,95. El wootz pega ×1,4. La empuñadura del sable nunca toma uno frágil.
- **Comercio:** el wootz, en las caravanas; las hojas de sable, en las capitales. Lo templado vale más; lo frágil, menos que sin templar.
- **Untar las armas con grasa** (pendiente desde la fase 5) no entró: las armas todavía no se gastan con el uso.
- **Pruebas:** `--ir veta|pantano`, `--ubicar taller` y `--forja` (la forja móvil, hierro, carbón y un sable frágil; con `--ir veta`, pica). **Ayuda:** una página nueva, «La forja».

**Lo que hay:** las armas de la fase 7; los hornos de bronce y de acero con herrero; reparar con el horno; el hierro, el cobre y el estaño como materiales del altiplano y del comercio; la exploración que marca sitios en el mapa (fase 4); la migración (fase 6b).

**Forja:**
- **Forja móvil:** fuelle de cuero y hogar de carbón que se carga en un caballo. Repara y hace puntas de flecha y herramientas pequeñas en cualquier sitio, también durante la migración.
- **Taller permanente** (campamento grande o ciudad tomada): sables, puntas de lanza y armas de asta.
- **Hierro:** de minas a cielo abierto o hierro de pantano (vetas en el mapa, que se descubren al explorar en la fase 4), o comerciado o tributado como lingotes, **acero wootz** u hojas terminadas.
- **Temple:** un paso de forja con riesgo. Martillar y templar da filo duro y lomo flexible: más daño y menos rotura. Si sale mal, el arma es frágil. El resultado depende del herrero (rol y talento).
- **Tests:** el temple modifica las estadísticas; la forja móvil viaja con la caravana.

**Costo:** medio.

## Fase 8a — Asentamientos agrarios: ciudad y economía (v0.9.0) — hecha

La fase era muy grande y se partió en dos, como la 4, la 6 y la 7: la ciudad y su economía aquí (v0.9.0), y el ejército y el asedio en la fase 8b (v0.9.1).

**Cómo quedó** (lo que cambió respecto de lo planeado, o lo que el plan dejaba abierto):
- **La economía, en `src/sim/city.*`** (con tests): `City` por asentamiento, en `GameActions.cities`, y un día a la vez (`city_day`). La cosecha depende de la estación, la región y la suerte de la estación (de la semilla). Pasada su tierra, más gente no da más cosecha. Hay granero, crecimiento y hambre, impuestos según los escribas, talleres de armas, soldados que se arman en la armería y mercenarios que llegan con la plata de sobra y se van sin paga. Las ciudades se arman al empezar (o al cargar una partida vieja).
- **El trazado** (`city_layout`, de la semilla y de la gente): los campos y los corrales tienen huecos fijos, para que la ciudad crezca sin mover lo que ya había, y evitan el agua y las cuestas. Los edificios nuevos son `VB_PALACE`, `VB_FORGE`, `VB_BARRACKS` y `VB_CARPENTRY` (`VB_WORKSHOP` del plan es la carpintería), con los materiales de cada región. Los campos con surcos son cubos que siguen el terreno, no vóxeles. El dibujo dejó `world_game.c` y pasó a `src/game/city_game.c`.
- **La relación con la tribu, la parte de la 8a:**
  - el comercio, con los precios y el tesoro de la ciudad;
  - el tributo: se paga en la capital del kanato, una vez por estación; si una estación pasa sin pagarlo, la relación baja;
  - el saqueo de los campos, con la guardia que sale: `ENEMY_SOLDIER`, el soldado del reino, que es la base del ejército de la 8b. Sus bajas se descuentan de la ciudad;
  - el rencor por ciudad, que cierra su mercado.
  - El saqueo de las caravanas ya estaba (fase 5).
- **Con la relación ya pasa algo:** el kanato no comercia con quien la tiene en −30 o menos. En la 8b, además, enviará expediciones de castigo.
- **La tribu come grano:** un saco, diez raciones, después de la carne y la leche.
- **Pruebas:** `--ir kanato|campos`, `--comerciar ciudad|campos` y `--dron <m>`. **Ayuda:** una página nueva, «Ciudades y reinos».

**Lo que hay:**
- **Asentamientos en `world.h`:** `Settlement`, con `SETTLE_VILLAGE` y `SETTLE_CAPITAL`, reino, nombre y región.
- **Cómo se dibujan:** casas de vóxeles en anillo y murallas en la capital.
- **No tienen** población ni economía.

**Trabajo:**
- **Ciudad que crece desde el centro:**
  - **Centro:** el palacio o edificio administrativo del rey o gran jefe, con su corte.
  - **Anillo de casas.**
  - **Barrios:** fundición, cuartel y carpintería.
  - **Afuera:** campos de cultivo y corrales, con trabajadores que los atienden durante el día.
  - Se genera por semilla en `voxstruct` (edificios nuevos: `VB_PALACE`, `VB_BARRACKS`, `VB_FORGE`, `VB_WORKSHOP` y campos con surcos).
- **Economía (simulación abstracta por día):**
  - población y cosechas por estación;
  - impuestos (los escribas: la burocracia es un número de eficiencia);
  - producción en masa de armas y armaduras;
  - tesoro.
  - El tesoro paga soldados profesionales y **mercenarios**, que crecen con la riqueza.
- **Su ejército:**
  - **Infantería pesada:** más armadura (piezas de `armor` ya existentes), escudos grandes (paveses) y formación cerrada que se mueve junta.
  - **Arqueros y ballesteros.**
  - **Carros de guerra:** entidad nueva, dos caballos y conductor más arquero o lancero. Rápidos en llano, malos en terreno roto y vadeando.
- **Relación con la tribu:**
  - el tributo al reino, que ya existe (`Kingdom`, relación);
  - el comercio;
  - el saqueo de sus campos y caravanas;
  - el **asedio** de ciudadelas, con escalas, la trepa que ya existe, fuego y hambre. Tomar una ciudad da su taller permanente (fase 7).
  - Si la relación cae, envían expediciones de castigo con infantería y carros contra el campamento.
- **Tests:**
  - la economía de la ciudad crece con la paz y cae con el saqueo;
  - el tesoro limita el ejército;
  - el carro pierde velocidad en terreno roto.

**Costo:** muy alto. Se partió en 8a (ciudad y economía, arriba) y 8b (ejército y asedio).

## Fase 8b — Ejército y asedio (v0.9.1)

**Lo que hay:** las ciudades de la 8a con su economía, su tesoro y su guarnición (soldados y mercenarios, números); el soldado del reino (`ENEMY_SOLDIER`), que sale a defender los campos; la relación con el kanato (tributo y comercio) y el rencor de cada ciudad.

**Trabajo:** lo que quedó de la fase 8:
- **El ejército en el mundo**, que sale de la guarnición y del tesoro:
  - infantería pesada con paveses (`escudo.grande.paves` ya existe), en formación cerrada que se mueve junta;
  - arqueros y ballesteros;
  - carros de guerra: una entidad nueva, con dos caballos, un conductor y un arquero o lancero. Rápidos en el llano y lentos en terreno roto o vadeando.
- **Las expediciones de castigo:** si la relación con el kanato cae (o el rencor de una ciudad es mucho), sale una contra el campamento, con infantería y carros.
- **El asedio de las ciudadelas:** con escalas, la trepa que ya existe, el fuego y el hambre. Tomar una ciudad da su taller permanente (fase 7b).
- **Tests:** el tesoro limita el ejército que sale; el carro pierde velocidad en terreno roto; la expedición sale con la relación baja.

**Costo:** alto.

---

## Orden y versiones

| Versión | Fase | Depende de |
|---|---|---|
| v0.4.4 | 0 · partidas por bloques | — |
| v0.5.0 | 1 · cuatro botones, objetivo y mapa de teclas | 0 |
| v0.5.1 | 2 · espalda, rehenes, abatidos | 1 |
| v0.6.0 | 3 · escolta y guardias que pelean | 1, 2 |
| v0.6.1 | 4 · tareas de la tribu | 0 (y 3 para guardia) |
| v0.6.2 | 4b · obras en vista aérea | 4 (el jefe también las manda) |
| v0.7.0 | 5 · economía y artesanías | 4 |
| v0.7.1 | 6 · caballos y cría | 1, 4, 5 |
| v0.7.2 | 6b · migración en caravana | 4, 6 |
| v0.8.0 | 7 · armas de la estepa | 1, 5 |
| v0.8.1 | 7b · la forja | 6b, 7 |
| v0.9.0 | 8a · ciudad y economía | 3, 5, 6, 7 |
| v0.9.1 | 8b · ejército y asedio | 8a |

**Por sesión:** cada fase (o media fase, en las grandes) cabe en una sesión de trabajo, con PR, CI, versión y prueba en la tablet. La fase 0 conviene hacerla primero y sola: es corta y evita perder partidas en todas las demás.

**Fuera de este plan** (anotar para después):
- los modelos 3D nuevos de las armas, el carro y los edificios, que hasta tenerlos van con primitivas o vóxeles;
- las voces y los sonidos;
- el equilibrio fino de la economía, con jugadas largas en la tablet.
