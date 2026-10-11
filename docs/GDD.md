# ESTEPA — Documento de diseño (GDD)

> Título provisional. Documento vivo: cada sistema se detalla a medida que se definan las propiedades pendientes.

## Visión

RPG de mundo abierto en 3D con gráficos pixel/low-res, ambientado en las estepas de las tribus nómadas (hunos, mongoles). El jugador lidera una **tropa guerrillera**: acampa, recluta, delega, juzga y sobrevive. Lo que hace dentro de su campamento pesa tanto como lo que hace en el campo de batalla.

**Plataformas:** Windows y Android. **Tecnología:** C11 + raylib (ver [ARQUITECTURA.md](ARQUITECTURA.md)).

### Inspiraciones
- **Outward:** supervivencia exigente, campamentos, combate con peso, el mundo no se adapta al jugador.
- **Freedom Fighters:** mando de una escuadra guerrillera; reclutar y dar órdenes en tiempo real.
- Historia de las estepas: yurtas, monturas, tributo a los reinos sedentarios, lealtad tribal.

## Pilares

1. **La política del campamento es el núcleo.** Cada decisión del líder mueve la moral de la tropa y la relación con el reino al que rinde tributo.
2. **Combate físico y cercano.** Grappling y combos primero; las armas a distancia complementan.
3. **Movimiento nómada.** Moverse, esconderse, trepar y montar son tan importantes como pelear.
4. **Liviano por diseño.** El estilo low-res es una decisión estética *y* de rendimiento: debe correr en Android de gama baja.

## Sistemas

### 0. Menú de entrada, partidas guardadas e interfaz (implementado — `src/game/title_menu.*`, `src/game/save_game.*`)
- **Menú de entrada:**
  - Opciones: nueva partida, cargar partida, instructivo y salir.
  - El fondo son estelas de piedra en la estepa. Encima, el **ciervo de oro con astas de cabezas de ave**.
  - Los bordes son frisos de triángulos escalonados y espirales, con turquesas engastadas en las esquinas, como la orfebrería escita.
  - Las láminas (lobo enroscado, tigre, grifo y ciervo, águila e íbice, reno de plata) ilustran el instructivo y los huecos vacíos.
- **Pausa** (`Esc`): continuar, guardar, cargar, instructivo, salir al título o del juego. La partida queda congelada y oscurecida detrás.
- **Partidas guardadas:**
  - Tres huecos. Cada uno muestra una **minifoto** de la partida al pausar, la **fecha** real, el día de juego con la estación y la hora, y la tribu.
  - Sobrescribir pide confirmar.
  - Se guarda todo: jugador, tribu y grandes guerreros, acopio, obras, animales, enemigos, fuego, objetos del mundo con su mantenimiento y el mapa recorrido con sus marcas.
  - El archivo es versionado: una partida de otra versión del juego no se carga, se avisa. Se escribe aparte y se renombra, así nunca queda a medias.
- **Instructivo:** seis láminas con los controles y las reglas: la estepa, la tribu, cuerpo a cuerpo, armas a distancia y armadura, fauna y clima.
- **HUD limpio:**
  - Arriba a la izquierda, solo lo esencial: hora y estación, tribu, barras de ánimo y lealtad, y lo que se empuña. El riesgo de rebelión aparece solo si existe.
  - El registro de sucesos va abajo y se desvanece a los 8 s.
  - Los controles se ven con `F1`. La versión, los fps y la relación con el reino están en la pausa.

### 1. Tropa y política (implementado en Fase 0 — `src/sim/troop.*`)
- **Integrantes** con moral (0-100), lealtad al líder (0-100), una **función delegada** (explorador, cazador, cocinero, herrero, guardia, curandero, lugarteniente) y **rasgos** (compasivo, sanguinario, ambicioso, devoto, leal).
- **Acciones del campamento:** reclutar, tomar prisioneros, liberar prisioneros, desterrar, ejecutar (integrante o prisionero), repartir botín.
- Cada acción afecta la moral según los **rasgos** de cada integrante (una ejecución hunde a los compasivos y apenas toca a los sanguinarios). Los valores están en una tabla de balance editable.
- **Reino tributario:** el jugador elige a qué reino rinde tributo. Cada reino tiene **valores propios**: el *Kanato de Hierro* aplaude la mano dura; la *Dinastía de Jade* castiga las ejecuciones y premia la clemencia. La misma acción sube la relación con uno y la baja con el otro.
- **Deserción:** probabilidad diaria por integrante cuando su moral o lealtad caen bajo 40.
- **Rebelión:** riesgo de tropa cuando la moral media cae bajo 45; los ambiciosos desleales lo agravan y uno de ellos encabeza la revuelta.
- Simulación determinista (misma semilla ⇒ mismo resultado), cubierta por tests.

- **Grandes guerreros** (implementado — `src/sim/champion.*`): personajes únicos que **aparecen por azar** (4 % por encuentro; escasos, pero sin límite).
  - **Historia:** mezcla azarosa de origen, circunstancia del exilio, modo de vida actual y aspiración.
  - **Dones:** de 1 a 3 sorteados, entre mayor talla, rapidez, fuerza, aguante, arma especial, sanación, puntería y monta. Cada don se traduce en multiplicadores de combate.
  - **Rasgo de tropa:** lo fija su aspiración. Por ejemplo, quien sueña con un clan propio es ambicioso.
  - **En la tropa:** pueden ser integrantes o prisioneros, y siguen la moral de la tribu (desertar, rebelarse, ser desterrados). Si uno deserta, la moral de todos cae; y tienden a encabezar las rebeliones.
  - Ver [NARRATIVA.md](NARRATIVA.md#reputación-y-grandes-guerreros).
- **Afinidad con los pueblos del mundo:** las decisiones secundarias y la conducta del jugador mueven la hostilidad o afinidad de cada pueblo hacia la tribu.

**Pendiente de definir:** consecuencias jugables de una rebelión (duelo por el liderazgo, escisión de la tropa), cómo se ganan nuevos reclutas en el mundo, economía del tributo.

### Mapa de memoria y brújula (implementado — `src/sim/memory_map.*`, `src/ui/minimap.*`)
- **Minimapa circular** en la esquina superior derecha que gira con la vista, como una brújula: arriba es hacia donde mira el jugador, y una "N" en el aro marca el norte.
- **Empieza negro:** el personaje no conoce el mundo. Las zonas se **iluminan poco a poco** a medida que el jugador las recorre.
- **Frecuencia = nitidez:** cada zona acumula familiaridad. Lo recorrido a menudo brilla más, como la memoria espacial real.
- **Olvido en días de juego:** el brillo se apaga con los días (un día de juego = 30 minutos reales; `src/sim/clock.h`). Una zona vista de pasada se borra en una o dos semanas de juego; lo familiar dura mucho más (repetición espaciada).
- **Todo el mundo:** el minimapa siempre se centra en el jugador y muestra a su alrededor (150 m) la geografía ya explorada: regiones, relieve sombreado, ríos, lagos, mar, deshielo y nieve de las cumbres, y los pueblos, tribus, guaridas y ruinas vistos. Lo no explorado queda a oscuras, y lo olvidado se va apagando. La memoria se guarda por páginas que solo se crean al explorar, así que crece con lo recorrido, no con el tamaño del mundo.
- **Días automáticos:** el reloj de juego avanza los días solo; `Enter` salta al amanecer siguiente y `Ctrl+N` adelanta dos minutos (prueba).
- **Marcas:** el jugador marca sitios de interés (`M`) o de peligro (`Shift+M`). Pulsar de nuevo cerca de una marca la quita. Las marcas fuera de alcance quedan en el borde, señalando su dirección.
- Los parámetros (radio de vista, velocidad de aprendizaje y de olvido, brillo de una sola pasada) están en `memmap_default_params` y son de balance.

**Pendiente:** guardar el mapa con la partida y marcas con nombre.

### Día, noche y estaciones (implementado — `src/sim/clock.*`, `src/world/sky.*`)
- **Ciclo de 30 minutos reales** (día + noche). Cada día de juego empieza al amanecer.
- **Estaciones:** primavera, verano, otoño e invierno, de 7 días cada una (año de 28 días). La partida empieza el primer día de la primavera.
- **Duración de la luz según la estación:** en pleno invierno la noche alcanza **20 minutos** y el día 10; en pleno verano, al revés. En los equinoccios (mitad de la primavera y del otoño) son 15 y 15. El cambio es gradual día a día.
- **Alba y ocaso:** penumbra de 90 segundos de juego con luz cálida; el sol bajo también entibia los colores.
- **La noche se ve:** la escena se oscurece y azulea, aparecen las estrellas y los fuegos alumbran a su alrededor (fogata del campamento, fogatas, hogueras, hornos y la antorcha encendida en la mano).
- **Logo:** el lobo y el tigre en su placa de bronce (`arte/logo/`, `tools/assets/logo.py`): icono de la app en Android y Windows, y el menú principal.
- **HUD de rol:** bajo el minimapa, una placa con las constantes (vida, calor del cuerpo, sed y **aguante**); abajo a la izquierda, la **barra rápida 1-9** (cada casilla guarda un arma, un objeto o una habilidad; vacía, su tecla abre un selector; Mayús+número la cambia); en el borde izquierdo, una **columna de acciones** (menú, beber, vendar, fogata, tienda, lazo, trepa, hervir agua). Responde al ratón y a los toques. El aguante lo gastan correr, saltar y golpear; agotado, no se corre.
- **HUD:** el panel dice el día, la estación, la fase y los minutos que faltan para el próximo cambio; bajo el minimapa, una barra muestra el reparto de luz (oro) y noche (lapislázuli) con la hora actual.
- **Prueba:** `./build/estepa --dia 25 --minuto 14` arranca en una noche de pleno invierno.

**Pendiente:** efectos jugables de la noche (visión y sigilo, fieras, frío).

### Clima y estaciones (implementado — `src/sim/climate.*`, `src/world/terrain.*`, `src/world/weather.*`)
Todo sale de una función pura del instante y la semilla (`climate_at`): igual en cada máquina, sin estado que guardar.
- **Temperatura continental:** de unos −18 °C a mitad del invierno a +24 °C a mitad del verano; sube de día y baja de noche. La partida empieza al final del invierno: el deshielo llega hacia el día 5.
- **Tiempo atmosférico** en bloques de 7,5 minutos con transición suave: despejado, nublado, lluvia, tormenta eléctrica, nevada y ventisca. Cada estación tiene sus probabilidades (verano seco con tormentas, otoño lluvioso, nevadas en invierno) y con helada la precipitación es nieve. El tiempo tiende a durar más de un bloque.
- **Suelo según la estación:** pasto verde al final de la primavera, seco y amarillo al final del verano, ocre en otoño y dormido con helada. La lluvia lo oscurece y embarra. La nieve lo cubre en manchas que crecen hasta tapar todo en invierno; cuesta más en las pendientes. Las yurtas y las copas de los árboles se nevan.
- **Lagos y ríos:** cada lago y cada río tiene su nivel (ver [El mundo](#el-mundo-implementado--srcsimworld-en-juego-srcgameworld_game-y-srcworldworldview)). Crecen hasta 3 m con el deshielo de primavera y las lluvias (los ríos, la mitad) y bajan al final del verano, dejando a la vista el lecho seco y una orilla de barro. En invierno se congelan; el mar, no.
- **Cordilleras y glaciares:** las montañas de cada región (las más altas en el altiplano). La nieve permanente baja en invierno (glaciares extensos) y sube en verano (solo las cumbres). Arriba hace más frío: unos 7 °C menos cada 100 m sobre el llano.
- **Atmósfera:** lluvia y nieve como partículas ancladas al mundo, ventisca casi horizontal que blanquea la vista, relámpagos en las tormentas, cielo gris y luz apagada con nubes; las nubes tapan las estrellas.
- **HUD:** bajo la barra del ciclo, el tiempo y la temperatura (p. ej. «nevada · −12 °C»).
- **Modelos por estación:** un modelo puede traer variantes junto al base (`yurta_comun@invierno.glb`, `@primavera`, `@verano`, `@otono`); el juego usa la de la estación si existe.
- **Prueba:** `--dia N --minuto M` elige el momento, `--pos X Z` el lugar e `--ir lugar` un sitio del mundo (p. ej. `--dia 23 --ir altiplano`: glaciares en invierno; `--dia 7 --lago`: un lago crecido).

**Pendiente:** cosechas y pasto para el ganado según la estación, ríos.

### El mundo (implementado — `src/sim/world.*`, en juego `src/game/world_game.*` y `src/world/worldview.*`)
Detalle, reglas y parámetros para afinar: [MUNDO.md](MUNDO.md).
- **Una gran tierra casi redonda** de 4,8 km de radio medio, de borde fractal (no una circunferencia neta), con **cinco regiones fijas**: la estepa en el centro, rodeada de ríos de meandros; el bosque de coníferas al noroeste; el desierto al noreste; el altiplano glaciar al sureste; la costa de fiordos al suroeste.
- **Altitud media estandarizada:** costa ~12 m, desierto ~28 m, estepa ~40 m, bosque ~55 m, altiplano ~70 m (con cumbres que guardan el glaciar).
- **La frontera:**
  - la costa termina en el mar, con fiordos estrechos que entran tierra adentro;
  - el desierto termina en un gran cañón y, detrás, un muro de siete estratos;
  - el altiplano termina en un muro de hielo glaciar, agrietado; sus ríos bajan trenzados por lechos de grava, con lagos turquesa de deshielo;
  - el bosque termina en un canal sigmoideo, del que salen ríos tributarios hacia adentro;
  - Nadie pasa: la frontera invisible sigue el borde fractal (un aviso dice por qué).
- **Cada partida, otro mundo** (la semilla): cambian el trazado de los ríos, la forma y el sitio de los lagos y las montañas, las fronteras entre regiones (dentro de su sector) y dónde caen los pueblos y las guaridas. Las reglas no cambian. La semilla viaja en la partida guardada; `--semilla N` la fija.
- **Pueblos y reinos:** cada región tiene su reino vecino (la estepa, el Kanato de Hierro al que la tribu rinde tributo; Principado de los Pinos Negros; Señorío del Glaciar; Jarlazgo de la Costa Helada; Reino de los Oasis), una capital amurallada y dos aldeas, al estilo de su región. Al llegar, quedan en el mapa.
- **Guaridas:** de las fieras de cada región (lobos y tigres en la estepa; osos, lobos y pumas en el bosque; lobos y pumas en el altiplano; osos en la costa; hienas y coyotes en el desierto). Cerca, aparecen sus dueños; quedan en el mapa como peligro.
- **Fauna por región:** cada región tiene su hábitat (estepa, bosque, frío, costa, desierto).
- **Árboles:** bosque cerrado de coníferas; pinos dispersos en la costa y las faldas del altiplano; pocos en la estepa (más junto al agua); ninguno en el desierto.
- **Texturas del suelo:** atlas low-res de 4×4 celdas (`assets/terrain/texturas.png`, `tools/assets/texturas_terreno.py`) inspirado en fotos de la estepa y del Altai; la estación tiñe cada celda.
- **Horizonte lejano:** una malla gruesa hasta ~2,4 km bajo los chunks cargados, que se pierde en la bruma del color del cielo.
- **Cielo:** degradado del cenit al horizonte; el sol cruza del este al oeste por el sur; la luna, con fases (14 días), recorre el mismo arco; las estrellas giran alrededor del polo norte.
- **Nubes:** una capa a altura fija (80 m sobre el llano del campamento) que el viento arrastra; la cobertura sale del clima. Las cumbres altas atraviesan la capa y llevan su gorro de nubes; dentro de una nube, niebla.
- **Vista orbital (F5)** del mundo entero y **mapa general** (`--mapa-mundo archivo.png`).

### Peligros del clima y del terreno (implementado — `src/sim/hazards.*`, `src/game/hazards_game.*`)
- **Frío:** el jugador tiene **calor corporal** (barra bajo el minimapa: abrigado, fresco, frío, helado, hipotermia). Lo baja la sensación térmica: temperatura + abrigo de pieles − viento − ropa mojada. Lo sube estar junto a un fuego (fogata del campamento, fogatas, hogueras, hornos); las yurtas cortan el viento y la antorcha da algo de calor. La lluvia empapa y el fuego seca. Con frío el cuerpo se entumece (más lento) y con **hipotermia** el jugador se desmaya: la tribu lo lleva junto al fuego (moral −3). Una noche de pleno invierno a la intemperie congela en unos minutos.
- **Leña:** en las noches heladas cada fuego del campamento gasta leña del acopio (el doble con frío extremo). Si falta, la tribu pasa frío (moral −4).
- **Barro:** la lluvia y el deshielo frenan la marcha en suelo blando (hasta −40 %); no en la arena ni bajo la nieve.
- **Agua:** sin hielo, los lagos se vadean (más lento, la ropa se moja) o se nadan.
- **Lagos helados:** con el lago congelado se camina sobre el hielo, pero la capa puede romperse: más riesgo corriendo, a caballo y con hielo recién formado. El agujero queda abierto hasta el día siguiente.
- **Desierto:** su región del mundo: dunas y mesetas (arena dorada y roca rojiza, sin barro, casi sin nieve).
- **Socavones ocultos:** cada día aparecen en sitios al azar distintos: **socavones de nieve** en los glaciares y **arena movediza** en el desierto. No se ven; al acercarse solo una pista sutil (un cerco de grietas o de arena húmeda). Pisarlo atrapa.
- **Minijuego de rescate (coordinación mano-ojo):** al caer, el juego pide una serie de teclas (J K L U I O) que hay que pulsar en orden y a tiempo; cada acierto acorta el tiempo de la siguiente y los errores (o las demoras) se cuentan. Ganar: sales (empapado si fue el hielo). Perder: sales a duras penas, helado, y se hunde lo que cargabas o tu arma.
- **Escolta (Y):** dos integrantes libres te acompañan. También pueden romper el hielo o caer en un socavón: entonces corre el tiempo (unos 40–60 s) y hay que acercarse y pulsar **F** para sacarlos con el minijuego (si falla, se puede reintentar con menos tiempo). Salvarlo sube la moral (+3); si no llegas, muere (moral −6). Si caes tú con la escolta cerca, te tienden la lanza: la serie es más corta y con más margen.
- **Prueba:** `--trampa hielo|nieve|arena|rescate` arranca en cada caso (p. ej. `--dia 25 --lago --trampa rescate`; arena: `--dia 12 --ir desierto --trampa arena`).

**Pendiente:** ropa y abrigo como equipo, que los NPCs del campamento también sufran el frío individualmente, animales que caen al hielo.

### Fuego, rayos y lluvia torrencial (implementado — `src/sim/fire.*`, `src/game/disasters_game.*`)
- **Fuego:**
  - Arde en focos que queman pasto (unos 10 s), árboles (40 s) o estructuras (55 s).
  - Un foco caliente prende puntos vecinos con combustible: más lejos y más fácil a favor del viento, y con calor y suelo seco. El pasto mojado o nevado casi no arde; el suelo pisado del campamento, el agua y el desierto pelado, nada.
  - Lo quemado deja **tierra calcinada** que no vuelve a arder en 3 días. Los árboles quedan chamuscados o calcinados; las estructuras, reducidas a cenizas.
  - Pisar el fuego quema (heridas de **quemadura**, que no sangran y casi no para la armadura). Los animales también se queman.
- **Incendios forestales:** solo en verano, con calor, el pasto seco y sin lluvia; empiezan a 60–140 m del jugador. Si llegan al campamento, avisa.
- **Rayos:** en las tormentas cae uno cada unos 20 s, hasta 120 m del jugador. Lo alto los atrae: un árbol o una estructura a menos de 25 m de donde iba a caer. El árbol queda chamuscado; la estructura se daña (puede arder hasta los cimientos) y quizá prende. Hiere a quien esté a menos de 4 m.
- **Mantenimiento y lluvia torrencial:**
  - Las estructuras se desgastan cada día (más si llovió).
  - La tribu repara cada día las peores con troncos del acopio: cada constructor, dos; el resto entre todos, una.
  - Con lluvia torrencial (tormenta), las descuidadas (condición < 35 %) pueden **derrumbarse** y quedan escombros. El registro avisa cuántas están en riesgo.
- **La lluvia apaga todo fuego:** incendios, la antorcha, las fogatas y los hornos (dejan de dar luz y calor hasta que escampa y la tribu los vuelve a encender), y las flechas encendidas, en la mano o en vuelo.
- **Flechas encendidas** (`L` junto a un fuego, con flechas o virotes):
  - La flecha arde 25 s.
  - Al alcanzar a alguien suma una quemadura.
  - Donde cae prende el pasto, los árboles o las estructuras.
- **Prueba:** `--incendio` prende el pasto 12 m delante (en verano y seco se propaga).

### Salud, heridas y combate (implementado — `src/sim/health.*`, `src/sim/combat.*`, `src/game/combat_game.*`)
- **Para todos:** el jugador, cada integrante de la tribu y cada enemigo (personas y fieras) tienen **vida, sangre y heridas**.
- **Heridas:** corte, golpe, fractura, mordida y congelación, en una parte del cuerpo (cabeza, torso, brazos, piernas; el torso recibe más) y con gravedad leve, moderada o grave. La misma herida sin tratar empeora en vez de duplicarse; un golpe fuerte en un brazo o una pierna lo rompe.
- **Sangrado:** cortes y mordidas sangran hasta vendarlos (los leves coagulan solos). Sin sangre se cae **abatido** y, si se acaba, se muere.
- **Efectos:** heridas en las piernas frenan (y se cojea), en los brazos debilitan los golpes; poca sangre afecta a todo.
- **Curar:** las heridas sanan con el tiempo, más rápido vendadas y en reposo; las fracturas solo sanan entablilladas. `B` venda con **hierbas curativas** del acopio (a uno mismo o a un compañero cercano; a uno abatido lo levanta). El **curandero** del campamento atiende al jugador cuando está cerca y, cada amanecer, trata y cura a toda la tribu; los grandes guerreros aguantan más (vida según talla y aguante).
- **Muerte y regreso:** abatido, su escolta lo levanta y lo venda. Si muere (desangrado, o abatido sin nadie que lo socorra), en el suelo quedan una calavera y unos huesos, y vuelve a su **último lugar de descanso**: el último campamento, tienda o casa de la tribu donde se quedó unos segundos (moral −8, pierde lo que cargaba). Los compañeros mueren para siempre (luto: moral −6).
- **Combate cuerpo a cuerpo** (`src/sim/melee.*`). Las mismas reglas valen para el jugador, la escolta y los enemigos. El jugador pelea con **cuatro botones** (v0.5.0): `H` ataque, `J` bloqueo, `K` parry y `L` carga o patada, con teclado o con las cuatro casillas de abajo a la derecha (tableta):
  - **Golpes y combos:** `H` (o clic izquierdo) golpea con el arma empuñada. El daño, el alcance y la cadencia dependen del arma: dagas rápidas, mazas que rompen huesos, lanzas de largo alcance; el bronce, el acero y las armas especiales cortan más. Tres golpes seguidos encadenan un **combo** (el tercero remata). Las armas cortas encadenan más rápido; con las largas (lanza, espada, guja) el remate puede tumbar.
  - **Golpe pesado:** mantener `H` medio segundo. Más daño, desequilibra y rompe la guardia del escudo (algo de daño pasa).
  - **Bloqueo:** mantener `J` (o el clic derecho sin arrastrar; arrastrar sigue girando la cámara). Con escudo cubre de frente (para casi todos los golpes ligeros). Sin escudo se para con el arma: aguanta la mitad del golpe y gasta aguante; agotado no se puede, y contra flechas no sirve. `J` + `H` con escudo da un **golpe de escudo** que aturde (escudo contra escudo, los dos se tambalean).
  - **Parry:** `K`. El rival anuncia el golpe: se detiene (el jinete no, que sigue al galope) y levanta el arma 0,4 s antes de pegar (más en el pesado, menos con armas cortas). En los últimos 0,25 s, un rombo dorado destella sobre él y en la casilla; el parry ahí le gana según el arma del jugador:
    - hacha o guja: **engancha el escudo** y se lo arranca (sin escudo, puede desarmarlo);
    - sin armas o con daga: **llave y derribo**, que lo desarma; es un pulso de fuerza, vida y peso de la armadura, y si falla, el que agarra queda desequilibrado;
    - las demás: **desvío**, que lo deja abierto un segundo.
    A destiempo (o sin golpe que parar) el jugador queda **expuesto** 0,6 s: ni se cubre ni ataca.
  - **Carga o patada:** `L`. Con escudo, **carga** que empuja y arrolla al primero que encuentra. Sin escudo, **patada**: aparta y desequilibra; contra un escudo en guardia solo le quita la guardia. **A la carrera, con inercia**, tumba incluso al que se cubre con escudo.
  - **Objetivo:** `Tab` en combate pasa al enemigo siguiente, en orden alrededor del jugador desde el que tiene delante (`Mayús+Tab`, al anterior); un clic o un toque sobre un enemigo lo elige. Un anillo dorado lo marca y su barra de vida se resalta. Los golpes van a él (el jugador se vuelve hacia él) y las armas a distancia le apuntan solas, con la caída calculada. Se pierde a 25 m o si muere; sin objetivo, se golpea al más cercano delante. Fuera de combate, `Tab` abre el menú de acciones.
  - **La espalda** (v0.5.1, `src/sim/stealth.*`): de frente te ven; de lado, menos; por detrás solo te oyen (el ruido: correr se oye, el sigilo casi nada). A menos de 2 m, en el cono trasero de 70° de uno que no te notó (o aturdido), aparece una daga sobre él:
    - `K`: **ejecución silenciosa**, muerte en el acto; solo la oyen los que están a menos de 6 m.
    - `G`: **rehén**. Va delante del jugador, sujeto por el cuello. Los de su bando (bandidos o culto) no se acercan a menos de 4,5 m ni atacan; el arquero no tira si el rehén le tapa el blanco, y si tira, falla más. El jugador va lento y con una sola mano (sin escudo ni armas a dos manos). `G` lo suelta, `K` lo ejecuta delante de los suyos y `L` lo empuja al suelo; un golpe fuerte (o caer, o montar) lo suelta.
  - **Abatidos:** las mazas, las patadas, los escudos y las heridas que no son mortales tumban sin matar (mata una herida abierta grave en la cabeza o el cuello, muy grave en el pecho o el vientre, o desangrarse). El abatido queda en el suelo, con una barra gris de lo que le falta para despertar; a los 30-90 s se levanta herido, huye y avisa a los suyos (los de cerca van a por el jugador aunque no lo vean). `F` lo toma prisionero (lo que lleva queda en el suelo, como botín); `H` o `K` lo rematan, y a la tribu le pesa como una ejecución menor (los compasivos y los devotos lo reprueban, los sanguinarios lo celebran; el reino también opina).
  - **En el suelo** (derribado unos 2,5 s) no se cubre ni ataca, y recibe más daño. **Desequilibrado**, no se puede cubrir. La armadura pesada cuesta más de tumbar.
  - **Lo que sueltan los enemigos** (escudos, armas) queda en el suelo. Si te lo arrancan a ti, su número (1 a 9) vuelve a empuñar.
- **Armas de mano:**
  - Los números (1 a 9) empuñan lo que haya en esa casilla de la barra rápida; la misma tecla otra vez enfunda.
  - `Mayús+número` pasa el arma a la otra mano. Con el escudo o un arma a dos manos no se puede. Con la izquierda sola se golpea peor.
  - Con un arma en cada mano, el combo alterna derecha e izquierda.
- **IA:** los bandidos y los captores llevan escudo a veces y se cubren cuando vas a golpear (no mientras anuncian su golpe). Contra tu escudo en guardia patean (a la carrera si vienen corriendo), agarran o enganchan (los fanáticos llevan hacha); contra tu arma en guardia patean o golpean pesado. Si estás en el suelo, rematan. La escolta pelea igual (los grandes guerreros, con guja).
- **Enemigos:** bandidos, arqueros, fanáticos y captores del culto. Ven al jugador según la distancia (acechando cuesta más), persiguen, golpean y huyen malheridos (los fanáticos nunca). Lejos del campamento aparecen bandidos o el culto de día. Se desangran también. Las fieras (lobos, osos, hienas...) son fauna: ver «Fauna».
- **Escolta** (v0.6.0, `src/sim/squad.*`): dos integrantes (`Y`) pelean a tu lado con las mismas reglas que el jugador y pueden caer heridos. Órdenes con `Y+1..3` o las casillas sobre la barra rápida:
  - **atacar:** al objetivo del jugador (si está a menos de 30 m de él) o al enemigo más cercano (14 m);
  - **defender:** solo contra quien se acerca a menos de 4 m del jugador; no se alejan de su lado;
  - **seguir:** no pelean.
  Con menos del 30 % de vida se retiran detrás del jugador. Los soldados (y los guardias) pegan un 30 % más fuerte.
- **La guardia del campamento:** los soldados que se quedan en su campamento (no el guardián, ni quien trabaja en una obra) hacen la ronda: un anillo de 40 a 60 m con puntos que rotan 30°. Salen al encuentro de los enemigos y las fieras que cruzan el anillo (a 35 m de ellos) y vuelven a defender si alguien entra al campamento. De noche llevan antorcha (luz).
- **Asaltos:** con el jugador a menos de 150 m de un campamento (y tras los primeros 10 minutos), cada minuto hay un 2,5 % de día y un 6 % de noche de que una banda (bandidos o el culto) salga a 70 m y vaya hacia él. Los enemigos atacan a cualquier integrante a la vista; los pobladores corren a las yurtas, tiendas y refugios y quedan escondidos (no se los ve ni se los ataca) mientras haya peligro a 30 m.
- **Rendimiento:** la IA de la gente del campamento a más de 150 m del jugador corre cada medio segundo y no se dibuja.
- **Peligros que hieren:** caer al hielo golpea, un socavón de nieve puede romper una pierna y la hipotermia congela manos y pies.
- **HUD:** vida y sangre bajo el calor; `P` abre el panel de heridas (tuyas y de la tribu); barras de vida sobre enemigos y compañeros heridos.
- **Animación:**
  - Clips nuevos de salud: `abatido`, `cojear` y `vendar`.
  - Clips nuevos cuerpo a cuerpo: `ataque_pesado`, `patada`, `patada_carrera`, `golpe_escudo`, `carga_escudo`, `enganchar_escudo`, `derribado` y `cambiar_mano`; con los cuatro botones, `parry` y `desvio`; con la espalda y los abatidos, `apunalar_espalda`, `sujetar_rehen`, `rehen` y `rematar`.
  - Se usan además `ataque_*`, `estocada_lanza`, `bloquear`, `agarre`, `recibir_golpe` y `morir`.
  - Sin modelos, el cuerpo articulado muestra la patada, el agarre, el escudo, el parry y al jugador agachado.
- **Prueba:** `9` hace aparecer bandidos delante (`Shift+9` lobos, `Ctrl+9` culto); `--enemigos bandidos|culto|arqueros|lobos|<animal>` (p. ej. `tigre`, `jabali`), `--heridas`, `--objetivo` (como pulsar Tab), `--espalda` (a la espalda del primero) y `--rehen` (con él de rehén) al arrancar.

**Pendiente:** combate montado, captores que se llevan prisioneros, botín de los enemigos.

### Zonas del cuerpo, armas a distancia y armadura (implementado — `src/sim/body.*`, `src/sim/ballistics.*`, `src/sim/armor.*`)
- **Cuerpo por zonas:** cabeza, cuello, tórax, abdomen, pelvis, brazos y antebrazos, muslos y piernas, para el jugador y todos los humanos. Cada zona multiplica el daño distinto (cuello ×2.2, cabeza ×1.8, abdomen ×1.25, tórax ×1.15, pelvis ×1, muslo ×0.85, brazo ×0.7, pierna ×0.6, antebrazo ×0.55) y sangra distinto (cuello, vientre y muslo, más). Las fieras usan las mismas zonas con nombres de animal.
- **Modelado simple y articulado:** mientras no hay modelos, cada humano se dibuja con una cápsula por zona; brazos y piernas se balancean al andar, el brazo derecho golpea, los brazos tensan el arco, y el cuerpo se tiende al caer. La misma geometría decide dónde impacta un proyectil.
- **Armas a distancia:** arco compuesto, arco largo, ballesta, honda y mosquete (y las especiales). La velocidad de salida sale de la **potencia del arma y la masa del proyectil** (v = √(2E/m)); en vuelo actúan la gravedad y la resistencia del aire (más fuerte para lo liviano), así cada tiro describe su curva y cae con la distancia. El daño sale de la energía que llega al blanco. Mantener `H` (o clic) tensa el arco (más tensión, más alcance y precisión) y al soltar dispara; la ballesta y el mosquete disparan al pulsar y luego recargan. Con objetivo, la mira se calcula sola; sin él, la cámara alza o baja la mira. Se dibuja la **curva** que hará el proyectil y dónde caerá. `J` baja el arma y `L` patea. Consume munición del acopio (flechas, virotes, piedras); las flechas se clavan en el suelo.
- **Arqueros enemigos:** guardan distancia y calculan el ángulo del tiro (con más error a mayor distancia). La escolta también puede recibir flechas.
- **Armadura por piezas:** casco (cabeza), gorjal (cuello), coraza (tórax y abdomen), hombreras (brazos), brazales y guanteletes (antebrazos), faldar (pelvis y muslos), grebas y botas (piernas). Cada material para distinto el filo, el golpe y la punta (fieltro, cuero laminar, bronce, hierro, acero, oro; la malla para menos las flechas) y tiene su **durabilidad y dureza**: cada impacto la gasta y, rota, ya no protege. Un corte que la armadura casi detiene llega como golpe. Las piezas pesan y frenan. El campamento las remienda cada día (mucho más con herrero).
- **Quién lleva qué:** el jugador empieza con cuero laminar; la tribu con fieltro y cuero, los grandes guerreros con escamas de hierro; los bandidos con fieltro y el culto con bronce. `P` muestra las piezas y su estado.
- **Prueba:** `Alt+9` arqueros; `--enemigos arqueros`; `--apuntar`.

### Acciones (núcleo implementado — `src/sim/actions.*`, en juego `src/game/actions_game.*`)
Todas las acciones las puede hacer el jugador y también los NPCs (la IA que las use llega en la Fase 1). Los objetos que usan o producen son ids del [inventario de assets](INVENTARIO.md).

**Manos y empuñadura.** Cada arma declara cómo se empuña (etiqueta `manos:una|dos|escudo` del inventario):

| Empuñadura | Ejemplo |
|---|---|
| A una mano | sable |
| Una en cada mano | sable y daga, o un arma y una antorcha |
| A dos manos | guja, arco, ballesta (ocupa ambas manos y suelta el escudo) |
| Arma y escudo | sable o lanza con escudo (el escudo siempre va en la izquierda) |
| Solo escudo / desarmado | — |

- **Enfundar y desenfundar:** guardar las armas libera las manos.
- **Tomar y lanzar objetos:** para tomar un objeto hace falta una mano libre, o tener las armas enfundadas.

**Acciones individuales** (una persona; tienen duración):

| Acción | Requiere | Resultado |
|---|---|---|
| Tomar / lanzar | mano libre / un objeto en la mano | el objeto vuela con física y cae al suelo |
| Cambiar empuñadura | — | recorre las combinaciones del equipo |
| Enfundar / desenfundar | un arma | — |
| Instalar fogata | — | `estructura.campamento.fogata` |
| Instalar tienda de campaña | — | `estructura.vivienda.tienda_ligera` |
| Cavar trinchera | pala | `estructura.defensa.trinchera` |
| Lanzar trepa | gancho con cuerda; un muro a tiro | se engancha para escalar en un asedio (trepar: Fase 1) |
| Lanzar lazo | lazo; un animal salvaje | intento de doma (doma: Fase 2) |
| Encender antorcha | antorcha | luz en la mano izquierda |
| Instalar montura | silla de montar; una montura | ensillar (monturas: Fase 2) |

**Construcciones en grupo.** Requieren varias personas de la tribu. Aquí importa tener miembros hábiles:

| Obra | Cuadrilla | Requiere | Rinde el doble |
|---|---|---|---|
| Refugio | 2–4 | — | constructor |
| Muro de empalizada | 3–6 | — | constructor |
| Muro de piedra | 4–8 | — | constructor |
| Hoguera | 2–4 | — | cazador |
| Tótem de protección | 2–4 | — | curandero |
| Horno de cocina | 2–3 | — | cocinero |
| Horno de fundición (bronce) | 3–5 | herrero | herrero |
| Horno de fundición (acero) | 4–6 | herrero | herrero |
| Torre de vigilancia | 3–6 | — | constructor |
| Corral | 2–4 | — | cazador |

- **Sin la cuadrilla mínima, o sin el oficio requerido, la obra no avanza.** Más gente que el máximo no acelera.
- **Ritmo de cada trabajador:**
  - 1 por persona;
  - ×2 si tiene la función adecuada;
  - grandes guerreros: ×1,5 con fuerza, ×1,25 con aguante y ×1,2 con talla;
  - ×0,5 si su moral está por debajo de 30.
- Los prisioneros no trabajan. El jugador suma una persona si está cerca de la obra.
- Nueva función de tropa: **constructor**.
- **La cuadrilla camina a la obra:** cada obra elige a sus trabajadores, sin repetir gente entre obras. La obra solo avanza con los que ya llegaron, y espera mientras no se junte el mínimo. El panel muestra cuántos hay en la obra (p. ej. «3/4»).

### Campamento: acopio, comida y efectos (implementado — `src/sim/economy.*`)
- **Acopio de la tribu:** materiales y comida compartidos.
  - Materiales: leña, troncos, piedra, barro, pieles, cuerda, carbón y minerales de cobre, estaño y hierro.
  - Las acciones y las obras consumen sus materiales al empezar. Si falta algo, se avisa qué y cuánto.
  - Los recursos que el jugador toma del suelo (`F`) van directo al acopio.
  - `I` muestra el acopio.
- **Recolección diaria por función:**

  | Función | Trae al acopio cada día |
  |---|---|
  | Sin función | leña y troncos |
  | Cazador | carne seca y pieles |
  | Explorador | piedra y barro |
  | Herrero | carbón |
  | Constructor | cuerda |

- **Comida:** cada integrante come una ración por día. Con cocinero se ahorra una ración cada tres bocas. Si falta comida, la moral de todos cae.
- **Efectos diarios de lo construido** (dentro de 45 m del campamento):
  - Hoguera: +3 de moral.
  - Fogatas: +1 cada una, hasta +2.
  - Horno de cocina: +2 de moral.
  - Tótem de protección: el riesgo de rebelión baja a la mitad.
  - Torre de vigilancia: revela 80 m del mapa de memoria cada día.
  - Refugios, tiendas y yurtas: dan techo.
- **Los NPCs actúan solos:** usan las mismas acciones que el jugador. Si no hay fogata, un integrante libre instala una; si faltan techos, otro instala una tienda.
- **Forja:**

  | Horno | Se forja |
  |---|---|
  | Bronce | sable, lanza y casco |
  | Acero | sable, coraza de escamas y escudo de láminas |

  - Hace falta el horno construido, un herrero y los materiales. Varios herreros forjan más rápido.
  - Lo forjado va al acopio. El jugador empuña el mejor sable que haya (acero, luego bronce, luego común).

### Inventario, equipo y amuletos (implementado — `src/sim/storage.*`, `src/sim/loadout.*`, `src/game/inventory_game.*`)
- **Contenedores:** cada uno con peso máximo, huecos y talla máxima de objeto.
  - **Bolsillos:** 2 kg, solo lo pequeño.
  - **Mochila:** 25 kg; un tronco no entra.
  - **Alforjas:** las tiene cada montura ensillada, según la especie: caballo 50 kg, mula 80, burro 60, buey 90, camello 120, elefante 200. Se usan montado o a menos de 8 m.
  - **Carreta de la tribu:** 400 kg, junto al campamento; se usa a menos de 5 m.
  - **Acopio y armería del campamento:** dentro del campamento. Los materiales y la comida van al acopio; las armas, escudos, armaduras y amuletos, a la armería.
- **Peso:** cada objeto pesa según su tipo, sus medidas y su material. Con más de 20 kg encima se anda más lento (hasta la mitad).
- **Las piezas de equipo guardan su estado** (gastadas o rotas) y no se apilan con las nuevas; al sacar se usa primero lo gastado.
- **Lo que se gasta en el campo sale de lo que llevas** (o tienes cerca): flechas, virotes y piedras, hierbas para vendar, carne para domar. Lo que se consigue (caza, pesca, leche, miel) va a la mochila, a los bolsillos o a lo que haya cerca; lo que no cabe se queda. Al salir llevas munición, hierbas, algo de carne seca y dos amuletos.
- **`I` inventario:** dos columnas. Izquierda y derecha eligen la columna, `A`/`D` el contenedor, arriba y abajo el objeto. `Enter` pasa uno al otro lado; `Mayús+Enter`, todos. Muestra el peso, los huecos, el estado de cada pieza y cuánto llevas encima.
- **`P` equipo:** una figura con las nueve piezas de armadura coloreadas por material y su desgaste.
  - Por pieza: protección contra corte, golpe y flechas, cobertura, estado y cuánto frena.
  - `Enter` pone o cambia una pieza que tengas a mano; la que llevabas va a la mochila, o donde quepa. `Supr` la quita.
  - **Tres amuletos**, cada uno con sus efectos:
    - lobo: sigilo y agarre;
    - ciervo: aguante y monta;
    - águila e íbice: puntería;
    - tigre y dragón: agarre;
    - oso: aguante y agarre;
    - caballo: monta y carisma.
  - Al lado, la vida, la sangre, el veneno y las heridas.
- **Efecto de los amuletos:**
  - el sigilo hace que los enemigos te vean de más cerca;
  - la puntería reduce la dispersión;
  - el agarre da fuerza a las llaves;
  - el aguante sana más rápido;
  - la monta da velocidad montado;
  - el carisma sube el ánimo de la tribu cada día.

### Fabricar y reparar (implementado — `src/sim/economy.*`, en juego `src/game/actions_game.c` y `src/game/inventory_game.*`)
- **El menú Tab tiene cuatro pestañas:** Acciones, Obras, Fabricar y Reparar (A/D o flechas laterales para cambiar). Cada receta muestra sus ingredientes con lo que tienes a mano (encima, en las alforjas o la carreta cercanas, o en el acopio si estás en el campamento) y marca lo que falta.
- **A mano, sin taller, lo hace el jugador:**
  - flechas: leña, plumas y pedernal (5 por tanda);
  - virotes: leña y hueso (4);
  - piedras de honda (8);
  - cuerda de tendones;
  - ungüento: hierbas y miel (cierra las heridas, corta el veneno y devuelve algo de vida al vendar con B);
  - coraza laminar de cuero (pieles y cuerda) y botas de fieltro (pieles).
- **Forjado:** las armas y armaduras de metal piden su horno y un herrero, y las hace la tribu.
- **Ingredientes recolectables:**
  - matas de hierbas y pedernal en el campo (F; Mayús+F recoge todo lo cercano);
  - plumas de las aves, hueso y tendones del resto de animales al despiezarlos (F);
  - miel de las colmenas.
- **Reparar:** la pestaña lista las piezas gastadas que llevas puestas y las de tus contenedores cercanos, con su estado.
  - El fieltro y el cuero se remiendan con pieles (y cuerda), en cualquier sitio.
  - El bronce, el hierro y el acero piden herrero, su horno en el campamento y metal con carbón.
  - Cada reparación devuelve parte del estado (60 % el cuero, 80 % el metal).

### Mochilas y alimentación de los animales (implementado — `src/sim/storage.*`, `src/sim/animals.*`)
- **Mochilas, tres tamaños:**

  | Tamaño | Carga | Huecos | Paso |
  |---|---|---|---|
  | Pequeña | 15 kg | 12 | ×1 |
  | Mediana | 25 kg | 18 | ×0,97 |
  | Grande | 40 kg | 28 | ×0,9, aun vacía |

  - Se cambia de mochila con `U` sobre otra en el inventario; lo que llevas tiene que caber.
  - `Mayús+G` (o la casilla de la columna) la deja en el suelo y la recoge al momento: útil para pelear más ligero.
  - Junto a la carreta, o a una mula, burro, camello, caballo o buey ensillado, `Mayús+G` la carga en ellos; la mochila sigue al animal o al carro.
- **La gente también lleva mochila** (`Member.pack`, `Member.bag`):
  - las de la escolta que está al lado son contenedores en el inventario;
  - una mochila grande frena a la escolta;
  - al disolver un campamento, la gente carga lo que quepa.
- **Animales de la tribu (domados y ensillados):**
  - el hambre sube con el tiempo;
  - los herbívoros pastan solos si hay pasto (sin nieve);
  - los carnívoros con hambre cazan presas de su talla cerca;
  - si no, hay que alimentarlos con `F`: forraje para los herbívoros, carne para los carnívoros;
  - los pastores alimentan con el acopio a los que están en su campamento y juntan forraje cada día;
  - con mucha hambre adelgazan, y al final abandonan a la tribu y vuelven a ser salvajes;
  - aviso en el registro y un cuenco sobre la cabeza del animal.

### Campamentos y guardián (implementado — `src/sim/camps.*`, en juego `src/game/camp_game.*`)
- **Fundar:** un campamento nace al levantar una estructura básica (tienda o refugio) a más de 70 m de los demás. La escolta nombra a un guardián, que se queda.
- **Cada campamento tiene:**
  - su acopio (en el juego, el del campamento donde está el jugador);
  - su gente (`Member.camp`: cada NPC vive alrededor del fuego de su campamento);
  - sus tareas.
- **Obras:** las levantan los del campamento que no tienen tarea ni van de escolta. Si la obra está fuera de todo campamento, trabaja la escolta cercana.
- **Guardián (diálogo con opciones en iconos):**
  - construir: la obra se ubica sola en un sitio libre del campamento;
  - escolta: quién sale con el jugador; los marcados ya lo siguen;
  - reclutar: alguien sin tarea sale con carne seca y vuelve con un integrante nuevo;
  - capacitar: un oficio para alguien sin tarea, con su coste y tiempo (soldado, herrero, druida, orfebre, pastor, cazador, explorador);
  - estado;
  - disolver.
- **Recursos humanos:**
  - quien se capacita o recluta queda ocupado y no trabaja en obras ni sale de escolta;
  - la gente ociosa del campamento acelera las tareas (+25 % cada uno, hasta ×2);
  - cada maestro del oficio suma +60 %.
- **Disolver:**
  - las estructuras arden y quedan cenizas;
  - el acopio se carga en la carreta y en las alforjas cercanas, y lo que no cabe se pierde;
  - la gente queda sin casa y sigue al jugador;
  - la escolta sin casa se queda en el próximo campamento que se funde (o en el campamento del guardián al dejar de escoltar).
- **Comida:** cada campamento alimenta a su gente con su acopio. La escolta come del campamento más cercano al jugador.

### Ropa y clima (implementado — `src/sim/apparel.*`, en juego `src/game/apparel_game.*`)
- **Capas:** cabeza, cuello y cara, cuerpo, capa o abrigo, pies. Cada prenda tiene abrigo (grados de sensación térmica), sombra [0, 1], protección de lluvia [0, 1], peso (frena) y escarmiento.
- **Sensación térmica** = temperatura del lugar + abrigo + refugio y fuego − viento (el abrigo corta hasta la mitad) − ropa mojada + sol × (1 − sombra) × 9.
- **Sol:** fuerte con el sol alto y sin nubes, más en el desierto. El desierto suma hasta +11° a mediodía y resta hasta −7° de noche.
- **Calor** (`HeatStress`, como el frío al revés): sube por encima de 27° de sensación térmica; baja a la sombra (yurtas), en el agua o mojado. Acalorado, sofocado (frena), agotado y golpe de calor (desmayo: la tribu lo lleva a la sombra). También da más sed (para el agua).
- **Escarmiento** (pieles de lobo, oso, tigre, puma, hiena; hasta 50 %): el enemigo que ve de cerca a quien las lleva tira una vez y puede retroceder unos segundos; herido, huye antes. Los fanáticos, al 30 %.
- **Materiales:** el despiece da la piel de la especie (`species_pelt`) además de pieles curtidas; los pastores esquilan las cabras del campamento cada día; la seda sale del botín de bandidos y jinetes. 20 recetas a mano.
- **NPCs:** cada 4 s eligen prenda por capa (`garment_pick`: lo que falta de abrigo, la sombra si hay sol, el peso) entre lo puesto, su mochila y el acopio de su campamento. Si con su ropa la sensación térmica queda bajo 2° o sobre 33°, pasan frío o calor (aviso sobre la cabeza) y baja su moral.

### El agua (implementado — `src/sim/water.*`, en juego `src/game/water_game.*`)
- **Sed** [0, 100]: de llena a seca en 25 minutos sin calor; el calor (`heat_thirst_scale`, hasta ×2.5), correr y la fiebre la aceleran. Con sed (< 40) y reseco (< 15): más lento y menos aguante. A cero, 20 s y desmayo (la tribu da de beber).
- **Bebidas** (`DrinkDef`): agua cruda, hervida, agua con vino, leche, airag, cerveza, vino; cuánta sed quitan y cuánto emborrachan.
- **Espíritus malditos:** cada trago de agua cruda tira `water_spirit_chance` (15 % + hasta 25 % con calor + 10 % en agua quieta). Se incuban 60–120 s y luego dan fiebre: frena, cansa, más sed. Se curan solos en unos 10 minutos (el doble de rápido descansando); las hierbas los cortan a la mitad (y casi del todo si aún se incuban).
- **Hervir** (acción junto a un fuego, una leña, hasta 4 tragos) y **el alcohol** purifican.
- **Borrachera** [0, 2]: alegre (> 0.3), borracho (> 0.8), ebrio perdido (> 1.4). Torpeza (golpes −35 %, puntería ×2.5 de dispersión), aguante a la mitad, se tambalea. Se pasa con el tiempo.
- **La tribu** (`water_daily`): una ración por persona y día del acopio (lo seguro primero). Si falta y hay agua a menos de 60 m del campamento: hervida mientras haya leña (una cada cuatro), cruda si no (fiebre: −20 de vida y −6 de ánimo a quien le toque). Sin agua: −15 de vida y −12 de ánimo. La vida no baja del 30 %.
- **Animales:** los de la tribu tienen sed (de 0 a 2); junto al agua beben solos; `F` con agua o los pastores con el acopio. Con sed > 1 adelgazan; > 2, se van.

### Órdenes a la escolta: despachar y mensajeros (implementado — `src/sim/travel.*`, en juego `src/game/travel_game.*`)
- **Despachar (`Mayús+Y`):** a un campamento o a un sitio marcado en el mapa. Al menos uno de los despachados tiene que conocer el destino (`Member.known`: un bit por sitio; se aprende al pasar a menos de 35 m).
- **Riesgo de un percance** (`journey_risk`):
  - sin nadie que conozca el camino, 90 %;
  - crece con la distancia y de noche (×1.4);
  - baja con la fracción del grupo que conoce el destino (hasta la mitad) y con el tamaño del grupo;
  - si hay percance, cada uno tira su suerte: llega bien, herido o no llega.
- **Sin simular el camino:** los despachados caminan (o cabalgan) hacia el destino unos segundos y desaparecen en el horizonte; el viaje es un temporizador (distancia / velocidad, a pie 3.5 m/s, a caballo 8 m/s). Al llegar aparecen en el destino, pero el jugador solo se entera del resultado al ir allí.
- **Mensajero:** va a un campamento; los que llegan vuelven con refuerzos (gente sin tarea del campamento, no el guardián) hasta la posición del jugador. Tarda la ida y la vuelta. Si el mensajero no llega, el jugador se entera porque no vuelve.
- **Hasta 8 partidas de camino a la vez**, de hasta 8 personas cada una.

### Nivel, tatuajes y joyas (implementado — `src/sim/talents.*`, `src/sim/jewelry.*`, en juego `src/game/talents_game.*`, `src/game/gems_game.*`)
- **Nivel (1 a 20):** la experiencia sale de pelear, cazar, fabricar, construir y domar. Cada nivel pide más que el anterior.
- **Tatuajes, permanentes:** los hace un druida de la tribu (`F` junto a él).
  - **Cinco motivos**, cada uno una progresión:
    - Lobo: sigilo y la furia de la manada.
    - Ciervo: aguante y carrera.
    - Grifo: vista y puntería.
    - Tamga, el sello del clan: mando y oficio.
    - Olas: vida y carga.
  - **Grados:** el grado I pide nivel 1; el II, nivel 3 y el grado I del mismo motivo. El III pide nivel 6 y se bifurca en dos habilidades específicas: una rama activa (aullido, galope, vuelo del grifo, grito del clan, marea) o una pasiva más fuerte. Elegir una cierra la otra.
  - **Zonas:** ocho zonas del cuerpo (cabeza, cuello, pecho, espalda, brazos, manos, piernas), cada una con un solo tatuaje.
    - La zona favorece unos atributos (×1,5) y no le van otros (×0,75); además suma un +5 % propio (la cabeza, vista; las piernas, velocidad...).
    - El mismo motivo rinde distinto según dónde se haga.
  - **Sin vuelta atrás:** no hay forma de quitarlo, así que cada tatuaje es una decisión definitiva.
  - **Tinta:** carbón y hierbas; en el tercer grado, también miel.
- **Joyas (amuletos), cambiables:**
  - **Nueve huecos:** cuatro anillos (dos por mano), dos brazaletes, collar, aretes y hebilla de cinturón.
  - **Las hace el orfebre** con metal (bronce ×1, plata ×1,3, oro ×1,6) y una piedra. Cada tipo pesa distinto (anillo ×0,6 … collar ×1,2).
  - **Las encanta el druida**, por hierbas y miel. Hasta entonces no hacen nada.
  - **Cada piedra da un pasivo o un activo:**

    | Piedra | Pasivo | Activo |
    |---|---|---|
    | Turquesa | aguante | marea |
    | Cornalina | cuerpo a cuerpo | aullido |
    | Lapislázuli | vista | vuelo del grifo |
    | Ámbar | carga | calor del ámbar |
    | Jade | sigilo | sombra |
    | Granate | vida | sangre de granate |
    | Perla | carisma | grito del clan |
    | Coral | monta | galope |

  - Los colgantes de animales de antes van en el collar, ya encantados.
- **Habilidades activas:** `F2`–`F4`, con duración y espera.
- **Atributos:** aguante, agarre, sigilo, puntería, carisma, monta, cuerpo a cuerpo, velocidad, vida, vista (descubre el mapa más lejos), carga y oficio (fabricar más rápido).
- **Piedras y metales:**
  - las rocas se rompen a golpes y sueltan piedras, mineral y a veces una gema;
  - los arrecifes de coral de los lagos dan coral y perlas (se agotan);
  - las trincheras desentierran lapislázuli, ámbar, plata y oro;
  - en la orilla se criba ("Cribar en el agua") para sacar piedras rodadas y oro;
  - el lingote de bronce se funde en el horno de bronce.

### Combate montado y botín (implementado — `src/sim/combat.*`, en juego `src/game/combat_game.*`)
- **A caballo solo se golpea con el arma** (V; mantener: golpe pesado). Patadas, agarres, ganchos y cargas con escudo son a pie.
- **El golpe montado:**
  - llega 0,9 m más lejos;
  - suma la inercia del galope: ×1 parado, hasta ×1,8 a 12 m/s;
  - con lanza por encima de 6 m/s, un golpe que no se para derriba el 60 % de las veces.
- **Arrollar:** al galope el caballo arrolla al que esté delante y lo tumba. Otro jinete solo cae el 30 % de las veces.
- **Contra el jinete:**
  - la montura suma 80 kg contra los derribos;
  - el 35 % de los golpes que pasan los recibe el caballo;
  - un derribo tira al jinete del caballo.
- **A distancia montado:** la velocidad dispersa el tiro; la monta (amuleto del caballo) lo corrige.
- **Jinetes bandidos:** rápidos, con sable y cuero.
  - Aparecen en grupo o acompañando a los bandidos.
  - Derribados, caen del caballo, que queda suelto y se puede domar con el lazo.
- **Botín:** cada enemigo abatido deja una bolsa en el suelo. Contiene:
  - a veces su arma y su escudo (si aún los tenía);
  - flechas y su arco (arqueros), cuerda (captores), hierbas y ungüento (culto), carne seca;
  - rara vez un amuleto;
  - la mitad de sus piezas de armadura no rotas, con su desgaste.
- **Recoger el botín:** F junto a la bolsa guarda lo que quepa; el resto se queda en ella. Las bolsas desaparecen a los 15 minutos.

### Fauna (implementado — `src/sim/animals.*`, en juego `src/game/fauna_game.*`)
- **Cinco clases de animales:**
  - **Monturas** (caballo, mula, burro, buey, camello, elefante): presas que se doman con el lazo (probabilidad propia de cada especie) y se montan con silla. Montado vas más rápido: caballo ×2,4, mula ×1,8, camello ×1,7, elefante ×1,6, burro ×1,5, buey ×1,3.
  - **Depredadores domables** (lobo, perro asilvestrado, tigre, puma, halcón, cuervo): hostiles. Se doman peleando hasta debilitarlos (menos del 40 % de vida; una marca dorada lo indica), echándoles el lazo (quedan atados a una estaca y dejan de sangrar) y dándoles carne con `F` antes de 60 s. Si no comen, se sueltan. Domados, defienden al jugador de fieras y enemigos. El halcón caza liebres para la tribu; el cuervo vuela en círculos y revela el mapa.
  - **Siempre hostiles** (oso, hiena, coyote, jabalí): atacan a las personas que ven y no se doman.
  - **Presas salvajes** (antílope saiga, reno, gacela, ciervo, liebre, íbice): se cazan y no se doman.
  - **Ganado** (cabra, becerro): de la tribu desde el principio.
- **Relaciones entre animales:**
  - **Sociales** (manadas, rebaños, jaurías, clanes): pastan juntos y vuelven al grupo si se alejan. Los cazadores sociales comparten la presa, la rodean y la cansan: correr agota (la presa se cansa antes que la manada). Después comen juntos del cadáver.
  - **Solitarios** (tigre, puma, oso, liebre): los cazadores acechan despacio y saltan de cerca; acechando, la presa los nota a un tercio de la distancia. Dos adultos de la misma especie en el mismo territorio pelean con golpes de advertencia; el que queda peor se va lejos (rivalidad).
  - **Huida:** las presas huyen de los cazadores que notan (de noche notan menos) y la alarma se contagia a su grupo. De las personas solo huyen si les hicieron daño, a ellas o a su grupo (durante 60 s). Depredadores y hostiles no huyen: contraatacan a quien los hiere y solo escapan con la vida crítica (menos del 20 %). Bueyes y elefantes heridos embisten en vez de huir.
  - Los animales atacan a cualquier persona: jugador, tribu y enemigos. La armadura y las zonas del cuerpo cuentan igual que en el combate. Nadie entra al agua profunda salvo las aves.
- **Aparición:** por bioma (estepa, desierto, frío de alta montaña), a 70–110 m del jugador y nunca junto al campamento. Hay unos 16 animales salvajes alrededor; de noche, el doble de cazadores. Los que quedan lejos desaparecen.
- **Caza y despiece:** golpes y proyectiles hieren por zonas de fiera (cada especie con su talla; las aves, en el aire). Con `F` junto a un cadáver se despieza: carne fresca y pieles (menos si otros se lo comieron).
- **Ganado:** sigue al jugador que camina cerca (pastoreo) y se queda donde lo dejas. `F` ordeña la cabra una vez al día; `Mayús+F` (quieto, junto al animal) sacrifica (carne y piel).
- **Acuáticos y anfibios:**
  - El cocodrilo espera medio sumergido junto a la orilla. Embosca a quien se acerca (personas o presas), pero no se aleja más de 9 m de su guarida.
  - La tortuga marina es una presa lenta en tierra: huye hacia el agua, donde los cazadores que no nadan no la alcanzan.
  - Solo nadan las especies acuáticas y las aves. El resto se queda en la orilla.
- **Pequeños y venenosos:**
  - La víbora, el escorpión (desierto) y la araña muerden si te acercas a pocos metros y no persiguen lejos de su guarida.
  - Su mordedura deja **veneno**: quita vida poco a poco, y mientras dura no se recupera. Las hierbas (`B`) lo cortan a la mitad. El HUD dice «envenenado».
- **Enjambres** (`src/sim/swarms.*`; cada celda de 40 m del mundo tiene siempre el mismo enjambre):
  - **Peces:** bancos que nadan bajo la superficie de los lagos (bajo el hielo no se ven) y huyen de quien se mete en el agua. Se pescan con un golpe hacia el agua (la lanza acierta más) o con una flecha que cae entre ellos, y dan carne fresca.
  - **Abejas:** tranquilas alrededor de su colmena salvo que la golpees o metas la mano sin humo. Con la antorcha encendida, el humo las calma y `F` toma la miel (una vez al día). De noche están en la colmena.
  - **Avispas:** se enfadan con solo acercarte a menos de 4 m del avispero. Pican más fuerte y con más veneno.
  - **Mosquitos:** junto al agua, con calor, al atardecer y de noche. Encuentran a quien pase cerca; el fuego de la antorcha los espanta.
  - **Moscas:** zumban sobre los cadáveres sin despiezar.
  - Bajo el agua honda, los insectos te pierden.
- **Comida:** la tribu come primero la carne fresca y la leche, y después la carne seca. Lo fresco que sobra al final del día se seca (la mitad de la carne) o se cuaja en queso (la mitad de la leche).

### Trepar y esconderse (implementado — `src/game/actions_game.*`)
- **Trepa:** se lanza a un muro (empalizada, muro o muralla) que esté delante. El jugador sube por la cuerda, pasa por encima y baja del otro lado.
- **Esconderse:** acechando dentro de la hierba alta, el jugador queda oculto y no hace ruido (el HUD lo indica).

### 2. Personaje y equipo (núcleo implementado — `src/sim/loadout.*`)
- Personaje y vestuario personalizables (fase 3).
- **Árbol de habilidades** con buffs **activos** (con enfriamiento) y **pasivos**.
- Los nodos del árbol dependen de **amuletos** (se equipan y cambian; 3 espacios) y **tatuajes** (permanentes: la API no permite quitarlos).
- Atributos iniciales: regeneración de aguante, fuerza de agarre, sigilo, arquería, carisma, monta.

### 3. Combate (fase 1)
- Centrado en **grappling y combos**: agarres, derribos, proyecciones, encadenamientos.
- Armas **cortas** (cuchillo, sable) y **largas** (lanza, guja).
- Armas **a distancia**: arcos, ballestas, mosquetes, cañones.

### 4. Movimiento (caminar, correr, sigilo, agacharse y saltar implementados)
- **Implementado:** caminar, correr, sigilo (`C`: más lento, emite menos ruido), agacharse (`X`: más bajo y lento, 2,2 m/s; con sigilo, 1,2 m/s) y saltar (`Espacio`; agachado, se levanta). Correr o montar cancelan el agacharse. El "ruido" de cada postura queda expuesto para la IA de detección.
- **Esconderse:** agachado tras rocas o matas, o con sigilo (o agachado) en la hierba alta. Lo que ven los enemigos se multiplica: de pie ×1; sigilo ×0,45; agachado ×0,6; agachado con sigilo ×0,3; a cubierto, ×0,4 más.
- **Pendiente:** esconderse en interiores, trepar árboles y muros.
- **Monturas:** caballo (velocidad), camello (resistencia, desierto), elefante (fuerza, asedio); cada una con habilidades propias.

### 5. Vehículos (fase 2)
Botes, veleros, barcos, carruajes y yunta de bueyes con carreta (transporte del campamento).

### 6. Mundo
- Estepa procedural por chunks con streaming alrededor del jugador (implementado).
- Campamento inicial con yurtas y fogata (implementado).
- Pendiente: biomas (desierto, montaña, ríos), asentamientos de los reinos, clima.

## Narrativa
Historia principal con protagonista de **nombre elegido por el jugador**: hijo de un jefe nómade exiliado, traicionado por su medio-hermano, que sobrevive en cinco climas bajo la filosofía *Scavengers Thrive* y regresa a la ciudad de su infancia. El prólogo se juega con el padre, a caballo, durante la masacre. Hay **tres finales** (asedio, asesinato o exilio del medio-hermano) que cambian el mundo, y la partida continúa después. Argumento completo en [NARRATIVA.md](NARRATIVA.md). Los diálogos y las escenas los define el autor.

## Dirección de arte
- 3D low-poly con sombreado plano, renderizado a 640×360 y escalado sin suavizado.
- Paleta terrosa de estepa; acentos saturados en objetos importantes (puertas de las yurtas, fuego, estandartes).
- **Interfaz:** orfebrería de estilo animal (placas de oro y plata, granulado, turquesa, cuero). Ver [ESTILO_VISUAL.md](ESTILO_VISUAL.md). El arte del menú (`assets/ui/`) parte de las imágenes de referencia: estelas, placas de oro y plata con ciervos, grifos, tigres y lobos. Se reduce a la resolución y la paleta del juego.
- **Animaciones:** cada modelo animado tiene su lista de clips en [ANIMACIONES.md](ANIMACIONES.md); el juego elige el clip según lo que hace el personaje o el animal.
- **Inventario de assets:** todos los modelos del juego están listados en [INVENTARIO.md](INVENTARIO.md), con id, medidas, presupuesto y estado.
- Los modelos se generan por código con **Kiln** (`tools/assets/*.kiln.js` → GLB). Ver [ROADMAP.md](ROADMAP.md).

## Preguntas abiertas
- Título definitivo.
- Preguntas de la narrativa: ver [NARRATIVA.md](NARRATIVA.md#preguntas-abiertas-para-el-autor).
- ¿Muerte permanente del líder o escenarios de derrota a lo Outward?
- Escala del mapa y número de reinos.
