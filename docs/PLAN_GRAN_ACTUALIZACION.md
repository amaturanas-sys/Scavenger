# Plan: la gran actualización (v0.5 a v0.9)

> **Estado:** plan, sin empezar. Cada fase va en su propio PR, con CI en verde, y sale como una versión que se puede probar en la tablet. Las fases están ordenadas por dependencia: el combate primero, porque la tribu, la economía y los asentamientos lo usan.

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

**Fase 0 — Formato de partida por bloques (v0.4.4):**
- **Formato:** la partida pasa a ser una lista de bloques `{etiqueta de 4 letras, versión, tamaño, datos}`, como los bloques opcionales que ya usan `death_game` (`DGT1`) y `hud_game` (`QBR1`).
- **Lectura:** cada módulo lee su bloque. Si la versión es vieja, la migra o completa con valores por defecto; si el bloque no está, arranca de cero.
- **Compatibilidad:** las partidas actuales (formato plano) se leen con el lector viejo y se reescriben en el nuevo al guardar.
- **Test:** guardar con la versión N y cargar con la N+1, con estructuras más grandes.
- **Costo:** bajo-medio. Destraba todo lo demás.

---

## Fase 1 — Combate de cuatro botones y selector de objetivo (v0.5.0)

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

## Fase 2 — Espalda, ejecución silenciosa y rehenes (v0.5.1)

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

## Fase 3 — Seguidores, escolta y guardias que pelean (v0.6.0)

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

## Fase 4 — Administrar la tribu desde el jefe del campamento (v0.6.1)

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

## Fase 4b — Obras ubicadas en una vista aérea del campamento (v0.6.2)

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

## Fase 5 — Economía de los campamentos y artesanías (v0.7.0)

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

## Fase 6 — Caballos: marcialidad y migración (v0.7.1)

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
- **Migración en caravana:** mover el campamento entero.
  - Se levantan las yurtas, se cargan en carretas y caballos (ya hay `carreta` y alforjas) y la tribu viaja en columna detrás del jugador.
  - Al llegar se planta el campamento nuevo y el acopio viaja con ellos.
  - Es la respuesta a la estación: pastos de verano en el altiplano, de invierno en el llano.
- **Enemigos montados:** los jinetes ya existen; se suman arqueros a caballo que hostigan y se alejan.
- **Tests:** el disparo a caballo pierde precisión con la velocidad; la migración conserva el acopio y a la gente.

**Costo:** alto.

## Fase 7 — Armas icónicas de la estepa y su forja (v0.8.0)

**Armas** (ids nuevos en el inventario, estadísticas en `weapon_stats` y `ranged_def`):

| Arma | Rasgos de juego |
|---|---|
| **Arco compuesto reflejo** (madera, asta, tendón) | El de hoy pasa a ser *arco simple*. El compuesto tiene más tensión y alcance, y es corto: se usa a caballo sin penalización. Se fabrica en 3 etapas (madera, asta y tendón, cola de hueso) con días de secado. |
| **Sable de la estepa** (curvo, un filo) | Corte limpio a caballo: no se traba y suma inercia. La versión temprana es la **akinakes**, espada corta recta de los escitas. |
| **Lanza ligera (2 m) / lanza pesada (4 m)** | La ligera sirve a pie y a caballo. La pesada, solo a caballo: con carga, derriba y rompe formaciones. |
| **Sagaris** (hacha con pico o martillo) | Perfora armadura (pico) o aturde (martillo). Engancha escudos en el parry. |
| **Maza** | Contundente: abate (KO) más de lo que mata, y es buena contra cascos. |
| **Daga de empuñadura de anillo** (sármata) | Arma de mano secundaria. Mejor en llaves, rehenes y ejecución silenciosa (fase 2). |

**Forja:**
- **Forja móvil:** fuelle de cuero y hogar de carbón que se carga en un caballo. Repara y hace puntas de flecha y herramientas pequeñas en cualquier sitio, también durante la migración.
- **Taller permanente** (campamento grande o ciudad tomada): sables, puntas de lanza y armas de asta.
- **Hierro:** de minas a cielo abierto o hierro de pantano (vetas en el mapa, que se descubren al explorar en la fase 4), o comerciado o tributado como lingotes, **acero wootz** u hojas terminadas.
- **Temple:** un paso de forja con riesgo. Martillar y templar da filo duro y lomo flexible: más daño y menos rotura. Si sale mal, el arma es frágil. El resultado depende del herrero (rol y talento).
- **Tests:** las recetas por etapas; el temple modifica las estadísticas; el arco compuesto a caballo no tiene penalización.

**Costo:** medio-alto. Es sobre todo contenido y datos; los modelos 3D son trabajo aparte en Nomad o Kiln.

## Fase 8 — Asentamientos agrarios: economía, ciudad y ejército (v0.9.0)

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

**Costo:** muy alto. Conviene partirla en 8a (ciudad y economía) y 8b (ejército y asedio).

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
| v0.7.1 | 6 · caballos y migración | 1, 4, 5 |
| v0.8.0 | 7 · armas y forja | 1, 5 |
| v0.9.0 | 8 · asentamientos agrarios | 3, 5, 6, 7 |

**Por sesión:** cada fase (o media fase, en las grandes) cabe en una sesión de trabajo, con PR, CI, versión y prueba en la tablet. La fase 0 conviene hacerla primero y sola: es corta y evita perder partidas en todas las demás.

**Fuera de este plan** (anotar para después):
- los modelos 3D nuevos de las armas, el carro y los edificios, que hasta tenerlos van con primitivas o vóxeles;
- las voces y los sonidos;
- el equilibrio fino de la economía, con jugadas largas en la tablet.
