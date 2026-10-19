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

**Pendiente:** cosechas y ríos. El pasto para el ganado según la estación ya está (v0.7.2, en «Campamentos y guardián»).

### La vegetación (v0.9.2 — `src/sim/flora.*`, en juego `src/world/vegetation.*`)
Las matas que crecen alrededor del jugador, sobre el suelo de cada región, al ritmo del clima. Llegó con el PR #115 (de Cline), reescrita para que funcione en el juego y en Android.
- **Qué crece y dónde:** pasto en todas partes (en el desierto, solo en los oasis); la stipa, el pasto pluma, en la estepa seca; flores en primavera (tulipanes y lirios en la estepa; azules, blancas y amarillas en el altiplano); helechos en el bosque y la costa; juncos en las orillas de lagos y ríos (también en el agua baja). Siempre las mismas para el mismo mundo: por celdas de 2 m, como el suelo, y sobre su facetado.
- **Dónde no:** en el agua honda, en la tierra pelada y la roca de las cuestas, en el lecho de cantos de los ríos, en la playa negra, en el frente del glaciar, en los pueblos y sus campos, junto a las estructuras y a los campamentos de las tribus; en los campamentos de la tribu el pasto está pisoteado. En lo quemado por un incendio no hay nada, y vuelve a crecer cuando se borra la tierra calcinada.
- **Con la estación:** el color es el del suelo de su región (verde, seco, ocre o dormido) y las puntas, más claras al sol. Crece en primavera, llega a lo más alto a comienzos del verano, se seca de pie en otoño y el invierno la aplasta. La stipa echa sus plumas plateadas de fines de la primavera a mediados del verano; los juncos llevan su espiga parda del verano al invierno y sus tallos secos quedan de pie. La nieve tapa las matas donde cubre el suelo; la stipa y los juncos asoman, escarchados.
- **El viento:** las matas se inclinan a favor del viento (el de la lluvia y el humo) y se mecen en olas que cruzan el pasto; con ventisca, casi se acuestan. Al pasar, el jugador (y su caballo) las aparta.
- **Rendimiento:** una ventana de 60 m que sigue al jugador; al moverse solo se siembran las celdas nuevas, las cercanas primero. Se dibujan hasta 26 m (18 m en Android), con menos hojas a lo lejos y creciendo de a poco en el borde; son triángulos de color sin texturas ni memoria por cuadro.
- **Prueba:** `--ir estepa --dia 10` (verano: stipa con plumas y flores), `--dia 17` (otoño), `--dia 24` (invierno: tallos en la nieve), `--ir meandro`, `--ir canal` o `--ir bosque` (juncos y helechos); `--montado lanza` galopa por el pasto. Al terminar una captura (`--screenshot`) dice cuántas matas hay sembradas y cuántas se dibujaron.

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
- **Armas a distancia:** arco simple, arco compuesto reflejo, arco largo, ballesta, honda y mosquete (y las especiales). La velocidad de salida sale de la **potencia del arma y la masa del proyectil** (v = √(2E/m)); en vuelo actúan la gravedad y la resistencia del aire (más fuerte para lo liviano), así cada tiro describe su curva y cae con la distancia. El daño sale de la energía que llega al blanco. Mantener `H` (o clic) tensa el arco (más tensión, más alcance y precisión) y al soltar dispara; la ballesta y el mosquete disparan al pulsar y luego recargan. Con objetivo, la mira se calcula sola; sin él, la cámara alza o baja la mira. Se dibuja la **curva** que hará el proyectil y dónde caerá. `J` baja el arma y `L` patea. Consume munición del acopio (flechas, virotes, piedras); las flechas se clavan en el suelo.
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
- **Ubicar la obra** (v0.6.2, `src/sim/placement.*`, en juego `src/game/place_game.*`): al elegir una obra (Tab → Obras, o el guardián), la cámara sube sobre el campamento (a la vista entran unos 50 m de radio; fuera de todo campamento, alrededor del jugador). La obra se ve como un fantasma translúcido con su huella:
  - sigue al puntero, al dedo o a WASD; Q/E la giran de a 15°; Mayús (o la casilla) alinea su centro a una rejilla de 2 m desde el centro del campamento;
  - **verde** si el sitio vale; **roja** si no, con el motivo abajo: fuera del campamento (alguna esquina a más de 45 m del centro), en el agua, demasiado empinado (desnivel de más de 0,4 m y más del 30 % del largo) o encima de algo (una yurta, la fogata, un árbol, otra estructura u otra obra; la huella lleva 0,2 m de margen);
  - **cercas y muros** (empalizada y muro de piedra): se marca un extremo y después el otro; van por tramos de 4 m sin huecos (si el largo no da justo, se enciman un poco) y cada tramo es una obra con su costo. Los muros pueden juntarse y cruzarse con otros muros;
  - Enter, un clic sobre la obra (en la tablet: tocar otro lugar la mueve, tocarla la coloca) o la casilla de aceptar la colocan y se paga el acopio; Esc cancela sin gastar. El giro queda en la obra y en la estructura terminada.

### Campamento: acopio, comida y efectos (implementado — `src/sim/economy.*`)
- **Acopio de la tribu:** materiales y comida compartidos.
  - Materiales: leña, troncos, piedra, barro, pieles, cuerda, carbón y minerales de cobre, estaño y hierro; y los trabajados (v0.7.0): cuero, fieltro, madera labrada, adobe, puntas de hueso y piezas de arco, con la grasa, el hueso y los tendones de la caza.
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

- **Producción por oficio** (v0.7.0, `economy_daily_work`): además de lo que traen, cada día los del campamento con oficio trabajan lo que sobra del acopio, una tanda por persona. Cada receta guarda una reserva de lo que usa (no se come el acopio entero) y lo hecho sale en los informes del campamento:

  | Oficio | Toma | Hace |
  |---|---|---|
  | Cazador | 2 pieles (deja 2) | 1 cuero |
  | Cazador | 1 hueso (deja 1) | 4 puntas de hueso |
  | Pastor | 2 pieles de cabra | 1 fieltro |
  | Constructor | 2 troncos (deja 6) | 1 madera labrada |
  | Constructor | 2 de barro y 1 de forraje (deja 2) | 2 adobes |
  | Cocinero | 1 grasa | 3 velas de sebo |
  | Curandero | 2 hierbas y 1 miel (deja 2) | 1 ungüento |
  | Orfebre | 1 hueso (deja 1) | 1 peine |

- **Comida:** cada integrante come una ración por día. Con cocinero se ahorra una ración cada tres bocas. Si falta comida, la moral de todos cae.
- **Efectos diarios de lo construido** (dentro de 45 m del campamento):
  - Hoguera: +3 de moral.
  - Fogatas: +1 cada una, hasta +2.
  - Horno de cocina: +2 de moral.
  - Tótem de protección: el riesgo de rebelión baja a la mitad.
  - Torre de vigilancia: revela 80 m del mapa de memoria cada día.
  - Refugios, tiendas y yurtas: dan techo (refugio y tienda 1, choza de fieltro 2, casa de adobe y cabaña de troncos 3, yurta 4).
- **Obras duraderas** (v0.7.0): la casa de adobe (12 adobes y 2 de madera labrada), la cabaña de troncos (8 de madera labrada y 2 troncos) y la choza de fieltro (4 de fieltro, 2 troncos y cuerda). El adobe y la piedra se desgastan a 0,4 del ritmo normal y los troncos a 0,6 (`structure_wear_scale`).
- **Los NPCs actúan solos:** usan las mismas acciones que el jugador. Si no hay fogata, un integrante libre instala una; si faltan techos, otro instala una tienda.
- **Forja:**

  | Horno | Se forja |
  |---|---|
  | Bronce | sable, lanza y casco |
  | Acero | sable, coraza de escamas y escudo de láminas |

  - Hace falta el horno construido, un herrero y los materiales. Varios herreros forjan más rápido.
  - Lo forjado va a lo que llevas (lo que no cabe, al acopio). El jugador empuña el mejor sable que haya: wootz templado, wootz, acero templado, acero, bronce y común (v0.8.1; nunca uno frágil).

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
  - coraza laminar de cuero (pieles y cuerda) y botas y gorro de fieltro (fieltro);
  - talabartería (v0.7.0): cuero (2 pieles), silla de montar, brida, alforjas, aljaba y casco, brazales y grebas laminares de cuero;
  - hueso y asta: puntas de hueso (4), flechas con punta de hueso (5), peine y piezas de arco (hueso y tendón). Desde la v0.8.0 el arco compuesto reflejo se hace en tres etapas (abajo, «Las armas de la estepa»);
  - grasa y leche: velas de sebo (3), antorchas (leña y grasa) y queso seco (2 de leche);
  - para las obras: fieltro (2 pieles de cabra), madera labrada (2 troncos) y adobe (barro y forraje).
- **Forjado:** las armas y armaduras de metal piden su horno y un herrero, y las hace la tribu. Desde la v0.8.1, lo pequeño se hace a mano con la forja móvil, y el wootz y el temple, en el taller (abajo, «La forja»).
- **Ingredientes recolectables:**
  - matas de hierbas y pedernal en el campo (F; Mayús+F recoge todo lo cercano);
  - plumas de las aves, hueso y tendones del resto de animales al despiezarlos (F);
  - miel de las colmenas.
- **Reparar:** la pestaña lista las piezas gastadas que llevas puestas y las de tus contenedores cercanos, con su estado.
  - El fieltro y el cuero se remiendan con pieles (y cuerda), en cualquier sitio. Sin pieles, se untan con grasa (v0.7.0): devuelve el 30 %.
  - El bronce, el hierro y el acero piden herrero, su horno en el campamento (o la forja móvil a mano, v0.8.1) y metal con carbón.
  - Cada reparación devuelve parte del estado (60 % el cuero, 80 % el metal).

### Valor, comercio, caravanas y saqueo (implementado en v0.7.0 — `src/sim/trade.*`, en juego `src/game/trade_game.*`)
- **El valor:** cada bien vale una cantidad de plata (`item_value`), la moneda de la estepa (`utileria.material.plata`, 50 g la unidad). Lo trabajado vale más que su materia prima (el cuero, 10; las pieles, 4; una silla de montar, 40). Las joyas valen por su metal; el agua, las estructuras, los animales y los tatuajes no se comercian. El estado del guardián y el inventario dicen cuánto vale el acopio (`stock_value`) y la leyenda de cada objeto, cuánto vale uno.
- **El precio cambia con la región** (`price_factor`): lo que allí abunda vale ×0,7 y lo que escasea, ×1,5 (las especias y la seda abundan en el desierto; la madera, en el bosque; los metales, en el altiplano). Quien vende cobra el 80 % y quien compra paga el 125 %: lo que se queda el mercader. Comprar donde abunda y vender donde escasea deja ganancia.
- **Mercados** (`F` junto a ellos; abajo aparece «F: comerciar con...»): las tribus amigas y neutrales (a 18 m de su fuego), las aldeas (22 m), las capitales (30 m), los caravasares (14 m) y las caravanas (8 m). Cada uno vende lo propio (`market_goods`: la tribu, carne seca, queso, airag, cuero y fieltro; la aldea, pan, cerveza, cuerda, velas y flechas; la capital, sables de acero, escudos, cascos, bronce, vino y sillas; la caravana, seda, vino, especias, gemas y oro) y lo de su región.
  - **Vender:** lo que llevas encima, en las alforjas, la carreta o las mochilas de la escolta cercanas (no lo puesto ni el acopio); lo más caro primero. Enter o clic vende uno; con Mayús, todos.
  - **Comprar:** el precio en la casilla; Enter o clic compra uno, con Mayús cinco. La plata que no cabe, o lo comprado que no cabe, queda a tus pies.
- **Caravanas** (`caravan_routes`, `caravan_at`): una por caravasar, hasta cuatro, va y vuelve al paso de los camellos (1,1 m/s) hasta la capital más cercana, cada una a otra hora. Se ven tres camellos cargados, el mercader a pie y dos jinetes de escolta. Se detienen mientras se comercia con ellas.
  - **Asaltarla** (una opción del diálogo): su escolta (dos jinetes, un bandido y un arquero) desenvaina y la relación con el reino baja 10. Vencida (nadie en pie a 60 m del asalto ni a 40 m del jugador), la carga queda en el suelo: 3 de seda, 2 de especias, 2 de vino, 30-60 de plata, 1 de oro y una turquesa. La caravana descansa dos días y vuelve a salir de su caravasar. Si te alejas más de 200 m, sigue su camino.
- **Saquear un campamento rival:** cuando sus guerreros ya salieron al paso, `F` junto a su fuego (a 20 m) lo saquea si no queda nadie en pie a 40 m: 2 de cuero, 1 de fieltro, 4 de carne seca, 3 pieles y 15-35 de plata. Se puede volver a saquear tras alejarse 600 m (como la emboscada, se rearma).

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
  - pobladores: la ventana de la tribu (ver abajo);
  - capacitar: un oficio para alguien sin tarea, con su coste y tiempo (soldado, herrero, druida, orfebre, pastor, cazador, explorador);
  - estado;
  - migrar (v0.7.2, ver abajo);
  - disolver.
- **Recursos humanos:**
  - quien tiene una tarea queda ocupado y no trabaja en obras ni sale de escolta;
  - la gente ociosa del campamento acelera la capacitación (+25 % cada uno, hasta ×2);
  - cada maestro del oficio suma +60 %.
- **Pobladores** (v0.6.1): la gente del campamento (menos el guardián) con su oficio, salud, ánimo y tarea. Se elige a uno o dos y se les da una tarea; el número en cada casilla dice cuántos saben el oficio. Las tareas duran un tiempo fijo:
  - **salidas** (no se los ve: se alejan unos segundos y desaparecen; al volver aparecen en el borde del campamento):
    - buscar reclutas (240 s, 3 de carne seca por persona): vuelve con 0 a 2 personas (un explorador y un acompañante ayudan);
    - explorar recursos (300 s, 2 de carne seca por persona): marca en el mapa de memoria de 1 a 4 sitios nuevos, primero uno de cada clase y después los más cercanos: agua (lagos y pozos), pastos junto a los ríos, campamentos rivales y ciudadelas (estos dos, como peligro). Llegan a 0,9 km, +0,4 km por explorador y +0,3 km acompañados, y aprenden el camino (para despacharlos allí);
    - cazar (300 s, 1 de carne seca por persona): presas de la región (las del hábitat, según lo comunes que son), más con cazadores y acompañados; en invierno escasean (×0,55) y en otoño abundan (×1,25). Traen carne fresca, pieles (la piel con nombre de la presa, si tiene), huesos, tendones y grasa (en otoño, el doble; en primavera, nada).
    - el riesgo de cada salida (10 % reclutas, 14 % explorar, 20 % cazar) baja un 40 % por cada uno que sabe el oficio y un 45 % acompañados, y sube un 50 % de noche. Con un percance, cada uno vuelve herido o no vuelve (solos, el 35 % muere; acompañados, no siempre les toca a los dos y mueren menos).
  - **en el campamento** (se los ve):
    - pastorear (300 s): llevan el ganado de la tribu (herbívoros domados sueltos en el campamento) a un pasto a 28 m y lo rondan; el ganado come y bebe (con dos pastores, un 50 % más rápido; sin pasto, come la mitad) y al volver da leche (+1 por animal con dos pastores). Las fieras pueden llevarse un animal: 18 % solo, 7 % con dos pastores, menos con un pastor de oficio y más de noche;
    - guardia (600 s): hace la ronda de los soldados, sea cual sea su oficio.
  - **informes:** al volver cuentan qué pasó, en corto en el registro y completo en la ventana (último informe y la página «Informes» con los cuatro últimos).
- **Disolver:**
  - las estructuras arden y quedan cenizas;
  - el acopio se carga en la carreta y en las alforjas cercanas, y lo que no cabe se pierde;
  - la gente queda sin casa y sigue al jugador;
  - la escolta sin casa se queda en el próximo campamento que se funde (o en el campamento del guardián al dejar de escoltar).
- **Comida:** cada campamento alimenta a su gente con su acopio. La escolta come del campamento más cercano al jugador (en marcha, del que va cargado en la columna).
- **Migrar** (v0.7.2; reglas en `src/sim/camps.c`, en juego `src/game/camp_game.c`):
  - **levantar:** el guardián desarma las viviendas portátiles (yurta común, yurta del jefe, tienda ligera y choza de fieltro) y las cuenta en `CampSite.load`. El campamento queda en marcha (`migrating`) y no cuenta para `camp_at`, `camp_nearest` ni la distancia mínima. Las obras fijas quedan en el sitio viejo;
  - **la columna:** la gente que está en el campamento pasa a escolta, sin obra, trabajo ni guardia; las salidas siguen y, al volver, alcanzan la columna. El rebaño sigue al jugador desde más lejos (`CARAVAN_RANGE` 60 m, lo pierde a 90 m) aunque esté quieto;
  - **en marcha:** el acopio es el de la columna (el inventario lo muestra, de ahí comen la escolta y la columna, y el agua se busca donde va el jugador). En el sitio viejo ya no hay asaltos, descanso, cría ni faenas de los pastores. Si el jugador se desmaya, la columna lo atiende donde está. Si muere toda la columna, la caravana se pierde con su carga;
  - **plantar:** `F` junto al guardián, en seco, sin mucha pendiente y a más de 70 m de los demás. Las viviendas se arman en corros de 8 (a 12 m, la segunda vuelta a 18 m), con la puerta hacia el fogón que encienden si no hay uno. La gente vuelve a sus casas alrededor del fuego y el rebaño cercano pasta allí. Lo que no cabe sigue cargado. Si es el campamento inicial, su fuego pasa a ser el nuevo (calor, abrigo, adonde te llevan al desmayarte) y sus yurtas de origen ya no están.
- **Pastos de cada estación** (v0.7.2, `pasture_quality`): un valor entre 0,1 y 1,2 por región y estación.

  | Región | Primavera | Verano | Otoño | Invierno |
  |---|---|---|---|---|
  | Estepa | 1,0 | 0,7 | 0,8 | 0,45 |
  | Bosque | 0,8 | 0,9 | 0,7 | 0,3 |
  | Altiplano | 0,6 | 1,2 | 0,5 | 0,1 |
  | Fiordos | 0,7 | 0,9 | 0,6 | 0,2 |
  | Desierto | 0,6 | 0,2 | 0,5 | 0,6 |

  - **Forraje:** cada día, cada pastor del campamento junta forraje de más: 1 desde 0,8 y 2 desde 1,0.
  - **Hambre:** por debajo de 0,6, el ganado herbívoro del campamento suma (0,6 − pasto) × 0,8 de hambre por día.
  - **Aviso:** al cambiar la estación, el guardián dice en sus informes si el pasto alcanza, si es bueno o si conviene migrar.

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
- **A distancia montado** (v0.7.1, `src/sim/riding.*`): la dispersión crece con la velocidad (×2,5 a galope tendido sin práctica) y hacia atrás algo más (×1,4 a galope: el **disparo parto**, a más de 100° del rumbo del caballo); la monta (amuletos, tatuajes, joyas) y la puntería lo corrigen. Quieto y hacia delante, como a pie. El caballo mira hacia donde va y el jinete se gira en la montura.
- **Lanza en ristre** (v0.7.1): con una lanza, L la baja 3 s. Desde 5 m/s, el primero delante del caballo recibe el golpe con la carrera entera (×1 a 5 m/s, hasta ×2,5) y cae el 55-95 % de las veces (un 35 % de eso si lo para con el escudo). Un golpe por carga y 2 s de espera.
- **El cansancio de la montura** (v0.7.1): el galope gasta su aguante (unos 45 s); al paso se repone en un minuto y quieta en 25 s. Agotada no galopa hasta recobrar el 35 %. Con hambre se cansa un 50 % más rápido y con sed otro 50 % (y se repone más lento), y corre un 15 % menos por cada una. Lo heredado cambia la velocidad y el aguante (±25 %).
- **Arqueros a caballo** (v0.7.1): rodean al blanco a 24 m y tiran al galope; a menos de 11 m se alejan al galope (tirando hacia atrás) hasta 38 m y vuelven. Vienen en grupos de 2 o 3, entre los jinetes, con las tribus rivales y en la escolta de las caravanas.
- **Jinetes bandidos:** rápidos, con sable y cuero.
  - Aparecen en grupo o acompañando a los bandidos.
  - Derribados, caen del caballo, que queda suelto y se puede domar con el lazo.
- **Botín:** cada enemigo abatido deja una bolsa en el suelo. Contiene:
  - a veces su arma y su escudo (si aún los tenía);
  - flechas y su arco (arqueros), cuerda (captores), hierbas y ungüento (culto), carne seca;
  - rara vez un amuleto;
  - la mitad de sus piezas de armadura no rotas, con su desgaste.
- **Recoger el botín:** F junto a la bolsa guarda lo que quepa; el resto se queda en ella. Las bolsas desaparecen a los 15 minutos.

### Las armas de la estepa (v0.8.0 — `src/sim/combat.*`, `melee.*`, `riding.*`, `stealth.*`, `economy.*`)
Cada arma tiene sus rasgos en `WeaponStats` (y los arcos, `RangedDef.saddle`):

| Arma | Rasgos |
|---|---|
| **Arco simple** (`arco_simple`) | El de siempre (75 J). A caballo se abre el tiro como en la v0.7.1. Es el del equipo inicial y el de los arqueros bandidos. |
| **Arco compuesto reflejo** (`arco_compuesto`) | 95 J y menos dispersión. Corto: a caballo tira como a pie, también hacia atrás (`saddle` 0). Lo llevan los arqueros a caballo. El arco largo, en cambio, sufre la montura ×1,6. |
| **Sable de la estepa** (y los de bronce, acero y damasquinado) | Corte limpio: a caballo aprovecha la carrera entera y un 10 % más al galope, y nunca se traba. |
| **Akinakes** | La espada corta recta de los escitas: 15 de daño, combos rápidos. |
| **Lanza ligera** | La de siempre, a pie y a caballo. |
| **Lanza pesada** (4 m, a dos manos) | Solo a caballo (a pie no golpea). En ristre derriba el 70-98 % (un 60 % de eso con escudo) y rompe la formación: alcanza a 2 más en la línea de la carga, a menos de 4 m uno de otro, con la mitad de fuerza cada vez. |
| **Sagaris de pico** | El pico entra como una punta: la armadura lo para como a un proyectil (y la malla peor). Engancha escudos (golpe y parry). |
| **Sagaris de martillo** | Contundente: si entra, aturde. Engancha escudos. |
| **Maza** | Abate: +12 % de derribo con el golpe (30 % con el pesado) y, en la cabeza, el casco para la mitad. Los golpes contundentes no matan al abatido. |
| **Daga de anillo** (sármata) | En la mano torpe pega como en la buena. Con ella en cualquier mano: las llaves (agarre y parry) entran con ×1,35 de fuerza; un rehén solo se suelta si te tumban o con el doble de daño; la ejecución silenciosa se oye a 3 m (no a 6). La llevan los arqueros a caballo. |

- **A caballo, las demás armas:** la carrera les suma la mitad, y al galope (desde 3 m/s, del todo a 6) el hacha, la espada o la daga se traban un 25 % de las veces y la maza un 15 %: el próximo golpe tarda 0,9 s más.
- **El arco reflejo, en tres etapas** (a mano, Tab → Fabricar):
  1. el alma de madera (madera labrada);
  2. el arco encolado: el alma, dos piezas de arco (asta y tendón), dos tendones y cola de hueso (huesos hervidos). Se cuelga en el acopio del campamento y seca **2 días** (`stock_dry_day`, cada día nuevo); el guardián avisa cuando está seco;
  3. la cola de hueso y un tendón que lo terminan.
- **Forjadas por el herrero:** akinakes, sagaris (de pico y de martillo), maza y lanza pesada en el horno de bronce; la daga de anillo, en el de acero.
- **Dónde se consiguen:** las tribus venden el arco reflejo, el sagaris, la akinakes, la daga de anillo y la lanza pesada; las aldeas, el arco simple y la maza. Del botín: el arco simple de los arqueros y el arco reflejo y la daga de anillo de los arqueros a caballo.
- **La forja** (forja móvil, taller, hierro de las vetas y del pantano, wootz y temple): abajo, «La forja» (v0.8.1).

### La forja (v0.8.1 — `src/sim/forge.*`, en juego `src/game/forge_game.*`)
- **El hierro del mundo** (`world_iron_sites`: sale de la semilla y no se guarda):
  - hasta 12 vetas a cielo abierto en el altiplano, a más de 700 m del centro y a 350 m una de otra;
  - hierro de pantano: uno por lago del bosque o de los fiordos, en la orilla seca.
  - Se ven de cerca (rocas herrumbradas o barro rojizo, hasta 160 m). Al pasar a menos de 22 m, la veta queda marcada en el mapa de memoria. Los exploradores también las traen como hallazgo (`FIND_IRON`, que no es un peligro).
- **Picar** (`ACTION_MINE`, 8 s; `F` junto a la veta): 2 de mineral en la veta y 1 en el pantano. Va a lo que llevas; lo que no cabe, al acopio.
- **Fundir** (horno de acero y herrero): 3 de mineral y 2 de carbón dan 1 hierro.
- **La forja móvil** (`utileria.herramienta.forja_movil`): la hace el herrero en el horno de acero con 2 de cuero, 2 de hierro y 2 de piedra. Cuenta si está a mano: en los bolsillos, la mochila, las alforjas o la carreta cercanas, o en el acopio del campamento (o de la columna, en la migración). Con ella, a mano y en cualquier sitio:
  - puntas de flecha de hierro (hierro y carbón: 6). Con leña y plumas, 2 puntas dan 8 flechas;
  - cuchillo de hierro (hierro y hueso);
  - sable de acero con una hoja comprada (hoja, cuero y madera labrada).
  - Repara lo que pide el horno de bronce o el de acero (con herrero y metal, como siempre). Lo que se forja en un horno sigue pidiendo el horno.
- **El taller del herrero** (`BUILD_WORKSHOP`): 8 troncos, 8 piedras, 2 de hierro y 2 de cuero; lo levantan de 3 a 6 obreros, con un herrero.
  - Es una obra de campamento grande (`BuildDef.min_people`): pide 8 personas o más viviendo en él. El menú de Obras muestra «8+ personas» y cuántos viven allí.
  - Hace todo lo de la forja móvil y además el sable de acero wootz (torta de wootz, 2 de carbón y cuero), la lanza de punta de acero (hierro, 2 de carbón y troncos) y el temple.
- **El temple** (`CraftDef.fails`): del sable de acero, el sable wootz o la daga de anillo, con carbón, en el taller y con herrero. Sale templada (`_templado`/`_templada`) o frágil (`_fragil`):
  - la probabilidad (`temper_chance`) es 0,5, más 0,15 por herrero (hasta 2), más 0,25 × la destreza del clan (`STAT_CRAFT`), entre 0,3 y 0,95;
  - templada: ×1,15 de daño;
  - frágil: ×1,1, pero se quiebra en un golpe que entra el 5 % de las veces (el 12 % si se lo paran) y se pierde (`weapon_break_chance`).
- **El acero wootz** pega ×1,4 (el acero, ×1,3). La empuñadura del sable toma el mejor que tengas: wootz templado, wootz, acero templado, acero, bronce y el de la estepa. Nunca uno frágil: ese se empuña a mano.
- **Comercio:** las caravanas venden acero wootz (40 de plata) y las capitales, hojas de sable de acero (30). La forja móvil vale 70, el mineral 0,5 y las puntas de hierro 1,5. El sable wootz templado vale 160; uno frágil, menos que uno sin templar.
- **El taller de una ciudad tomada** (v0.9.1): dentro de la muralla de una capital que la tribu tomó, su fundición hace de taller y de los dos hornos (`ct_workshop_at`).
- **Lo que no entró:** untar las armas con grasa (las armas todavía no se gastan con el uso).

### Ciudades y reinos (v0.9.0 — `src/sim/city.*`, en juego `src/game/city_game.*` y `src/game/trade_game.*`)
Cada asentamiento del mundo (las capitales y las aldeas de `world.h`) tiene su economía, un día a la vez (`city_day`), en `GameActions.cities`.

- **La gente y el granero:**
  - Los labradores (el 55 % de la gente, hasta lo que da su tierra: 4000 almas en la capital, 600 en la aldea) cosechan según la estación (primavera 0,5; verano 0,9; otoño 2; invierno nada), la región (el bosque rinde más; el desierto, menos) y la suerte del año (0,8 a 1,2, la misma toda la estación).
  - Cada alma come una ración por día, y el granero guarda hasta 150 días.
  - Con un mes de reserva y poco descontento, la gente crece (más despacio pasada su tierra). Con hambre, se va un 1,5 % por día y sube el descontento.
- **El tesoro:**
  - Los escribas cobran 0,05 de plata por alma y día, por su eficiencia (la capital 0,8, la aldea 0,45); cobran menos con descontento o con la cosecha arrasada.
  - Los talleres hacen armas y armaduras para la armería (2 de plata el juego) sin bajar de una parte de la reserva.
  - El tesoro paga a los soldados (0,5 por día); se reclutan con armas de la armería si hay paga para veinte días. Si no alcanza, los mercenarios se van y los soldados desertan.
  - Con plata de sobra, por encima de la reserva (1,5 por alma), llegan mercenarios (1,2 por día). Bajo la reserva, no se les renueva el contrato.
- **La ciudad crece desde el centro** (`city_layout`, de la semilla y de su gente):
  - en la capital, el palacio mirando a una de sus cuatro puertas y, alrededor, la fundición, el cuartel y la carpintería, al estilo de su región; después, dos anillos de casas (las grandes adentro), con graneros, el templo del fuego (desierto y altiplano) y carretas-casa (estepa). Todo sale del kit de edificios de la edad del hierro (`src/sim/buildkit.c`, v0.10.0; ver [ARQUITECTURA_EDAD_DEL_HIERRO.md](ARQUITECTURA_EDAD_DEL_HIERRO.md));
  - afuera, entre las puertas, los campos y dos corrales, en huecos fijos que no se mueven cuando la ciudad crece, en seco y sin cuesta;
  - las tiendas de los mercenarios, junto a la puerta principal;
  - la aldea, un anillo de casas chicas (la del jefe, un poco más grande), sus campos y un corral.
  - Se dibujan los campos según la estación (los arrasados, negros, desde el que se saqueó), los labradores de día, la guardia del palacio y de las puertas (más soldados, más guardias) y el estandarte del reino.
- **El mercado** (`F` en la aldea o la capital):
  - el granero mueve el precio de la comida (×1,6 con hambre, ×0,8 con el granero lleno) y la armería el de las armas;
  - el tesoro paga lo que le vendes (y cobra lo que le compras), y el grano vendido va a su granero;
  - «La ciudad» cuenta cómo está.
- **El saqueo de los campos** (`F` en un campo, `city_raid_fields`):
  - se lleva el grano de las eras: 120 raciones en otoño, 80 en verano, 50 en primavera y 30 en invierno, en sacos de 10;
  - los campos pierden un 35 % (se resiembran en paz: rápido en primavera, nada en invierno), sube el descontento y el rencor (+30);
  - no se puede saquear de nuevo hasta la otra estación;
  - sale su guardia (`ENEMY_SOLDIER`: lanza, escudo de láminas, casco de bronce y escamas): de 2 a 6 (en la aldea, 3), según su guarnición. Los que caen se descuentan de ella (`Enemy.city`) y dejan más rencor;
  - en el kanato, la relación con el kan baja 10.
  - Con 60 de rencor, la ciudad no comercia contigo; se olvida a 0,6 por día.
- **La tribu come grano:** en el acopio, después de la carne y la leche, cada saco da para diez.
- **El tributo al Kanato de Hierro:**
  - en su capital (la de la estepa), «Pagar el tributo» cuesta 4 de plata más 4 por integrante, una vez por estación; sube la relación (+10), y la plata va a su tesoro;
  - al empezar una estación, si la anterior pasó sin pagarlo, la relación baja (−6) y lo cuenta el guardián del campamento inicial (la primera revisión solo se anota);
  - con la relación en −30 o menos, el kanato no comercia contigo.

### Ejército y asedio (v0.9.1 — `src/sim/army.*`, en juego `src/game/city_game.*`, `src/game/combat_game.c` y `src/game/trade_game.c`)
- **El ejército que sale** (`army_compose`):
  - de la guarnición (soldados y mercenarios); un tercio se queda de guardia;
  - en campaña lo paga el tesoro, con la mitad de lo que tiene: 6 de plata por hombre, 9 por infante pesado y 30 por carro. Sin plata no sale nadie;
  - en este orden: el frente de paveses, soldados, ballesteros, arqueros y carros. Las aldeas no tienen paveses ni carros. Una salida en defensa de la ciudad no lleva carros ni la paga el tesoro.
- **Las tropas** (al final de `EnemyKind`):
  - el infante pesado (`ENEMY_HEAVY_INFANTRY`): pavés, lanza y escamas de hierro enteras (casco, coraza, faldar y grebas); 120 de vida, lento (3,4 m/s);
  - el arquero del reino (arco reflejo) y el ballestero (ballesta: tira lento y fuerte);
  - el carro de guerra (`ENEMY_CHARIOT`): dos caballos, conductor y arquero; 130 de vida, la de los dos. Va a 10 m/s en el llano (`chariot_speed`): pasada una cuesta de 0,06, pierde 3,5 veces lo que sube (le queda al menos el 20 %), y vadeando, más de la mitad.
    - Hace pasadas contra su blanco: sigue 22 m de largo y da la vuelta, girando a 1,3 rad/s.
    - Su arquero tira al acercarse. Arrolla a quien se le cruza delante: lo derriba y lo golpea según la velocidad.
    - Derribado, vuelca: sus caballos quedan sueltos y queda un soldado a pie.
- **La formación cerrada** (`Combat.formations`, `formation_slot`):
  - filas de hasta 5, a 1,4 m; los paveses delante y los tiradores detrás;
  - marcha hacia su meta (o hacia el enemigo que ven sus hombres) al paso del más lento y gira despacio; frente al enemigo se planta a dos pasos;
  - cada hombre se aparta de su lugar 3 m como mucho para pelear;
  - los paveses van en alto: paran el 90 % de las flechas que llegan de frente (el 60 % si no están en alto);
  - con la mitad de sus filas caídas, se rompe y cada uno pelea por su cuenta;
  - los carros van sueltos, por los flancos.
- **Las expediciones de castigo** (`punitive_due`):
  - salen al empezar el día con la relación con el kanato en −40 o menos (de su capital) o con 80 de rencor (de cualquier ciudad); no más de una por estación de cada ciudad, y una a la vez;
  - van contra el campamento que el jugador tiene cerca (a 150 m). Salen a la vista a 80 m, del lado de su ciudad: hasta 12 figuras;
  - si llegan al campamento, lo saquean (`stock_plunder`): se llevan comida, grano y plata, del 10 al 60 % (`punitive_share`: más con más hombres, menos con defensores). Después se vuelven, y el enojo se calma: la relación sube 25 y el rencor baja 40;
  - si cae la mitad, la expedición cae. Si el jugador se aleja, llega igual;
  - lejos del jugador, el guardián cuenta el saqueo.
- **La guerra** (`city_at_war`): con 60 de rencor o, en la estepa, con la relación en −30 o menos (mientras el kanato no caiga), la ciudad cierra el mercado y las puertas. En paz, rehace las puertas quemadas y quita las escalas.
- **La muralla** (`city_wall_blocks`):
  - doce tramos rectos entre las torres; cada tercer tramo es una puerta: 6 m de paso entre dos lienzos de muro;
  - **la puerta acodada** (v0.10.2): delante de cada puerta, un antemuro a 5,5 m, de 14 m de largo y cerrado de un costado. Se entra por su boca, a lo largo de la muralla, y se dobla hacia la puerta, bajo el adarve;
  - la muralla y los antemuros cortan el paso del jugador (que desliza a lo largo), de las flechas (se clavan) y de los enemigos. Los enemigos y la guarnición entran y salen por las puertas, rodeando el antemuro, por el camino más corto (`city_route`);
  - en guerra, las puertas están cerradas.
- **Cómo entrar:**
  - trepando: la trepa (`Tab`), a 3,5 m de la muralla y mirándola;
  - por una escala de asedio (a mano: 2 troncos y 2 cuerdas; pesa 12 kg). `F` junto a un tramo de muro, desde afuera, la apoya. `F` al pie, de cualquier lado, sube y pasa;
  - quemando una puerta, con una flecha encendida que se clave en ella o con la antorcha (`F`). Arde 20 s y queda abierta; la lluvia la apaga.
- **La guarnición sale** (`ct_update`): a 45 m de la muralla (o del centro de la aldea), si no hay nadie suyo afuera. Sale de la puerta más cercana (del cuartel, si ya estás adentro): hasta 6 (8 adentro), una vez por minuto.
- **El cerco y el hambre:**
  - con un campamento de la tribu a 220 m, la ciudad en guerra queda cercada (`city_siege_day`): no cosecha, la gente del campo se refugia adentro y come (media ración más por alma), entra la mitad de los impuestos y crece el descontento;
  - cada flecha encendida que cae adentro quema un techo (`city_burn_grain`): el 3 % del granero (al menos 40 raciones), y asusta.
- **La rendición** («Exigir la rendición», en el palacio o hablando con la guardia de una puerta): se rinde con la guarnición en la quinta parte de la que quiere tener (al menos 2), con grano para menos de 5 días o en revuelta (descontento del 90 %). Si no, dice cuánto le falta.
- **La paz** («Pagar la paz»): cuesta 3 tributos. El rencor baja a 30 y la relación con el kan sube a −20.
- **La ciudad tomada** (`City.taken`):
  - sin ejército, sin rencor y abierta: comercia contigo;
  - cada estación paga de tributo la mitad de lo que cobran sus escribas en la estación, al acopio del campamento inicial;
  - su fundición es tu taller, con los hornos de bronce y acero: dentro de su muralla se forja y se templa;
  - su estandarte, rojo y oro;
  - tomar la capital del kanato acaba con el tributo y con la relación.

### Cría (implementado en v0.7.1 — `src/sim/animals.*`, en juego `src/game/fauna_game.c`)
- **Quién se cría:** las monturas (caballo, burro, buey, camello) y el ganado (cabra, becerro). La mula y el elefante, no.
- **Sexo y lo heredado:** cada animal nace con sexo y con dos rasgos: velocidad y aguante, que solo cuentan montado. Los salvajes, ±10 % (salen de dónde nacieron); los criados, el promedio de sus padres ±5 %, sin pasar de ±25 %.
- **Preñez y parto:** cada día, en cada campamento, una hembra adulta de la tribu, bien comida y sin sed, con un macho adulto de su especie, queda preñada el 35 % de las veces (del mejor macho del rebaño). A los 3 días pare junto a ella.
- **Las crías:** tardan 4 días en ser adultas; mientras, son más chicas, no se ensillan ni se montan, ni dan leche. Las que andan fuera del campamento también crecen.
- **Tope:** como mucho 24 animales de la tribu entre todos y 14 por campamento; sin lugar, la madre espera.
- **Informes:** la preñez, el nacimiento (con lo heredado) y la cría que ya es adulta salen en los informes del guardián, y los nacimientos en el registro del día.

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
