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
- **Todo el mundo:** el minimapa siempre se centra en el jugador y muestra el mundo a su alrededor; lo no explorado aparece oscuro. La memoria se guarda por páginas que solo se crean al explorar, así que crece con lo recorrido, no con el tamaño del mundo.
- **Días automáticos:** el reloj de juego avanza los días solo; `Enter` salta al amanecer siguiente y `N` adelanta dos minutos (prueba).
- **Marcas:** el jugador marca sitios de interés (`M`) o de peligro (`Shift+M`). Pulsar de nuevo cerca de una marca la quita. Las marcas fuera de alcance quedan en el borde, señalando su dirección.
- Los parámetros (radio de vista, velocidad de aprendizaje y de olvido, brillo de una sola pasada) están en `memmap_default_params` y son de balance.

**Pendiente:** guardar el mapa con la partida y marcas con nombre.

### Día, noche y estaciones (implementado — `src/sim/clock.*`, `src/world/sky.*`)
- **Ciclo de 30 minutos reales** (día + noche). Cada día de juego empieza al amanecer.
- **Estaciones:** primavera, verano, otoño e invierno, de 7 días cada una (año de 28 días). La partida empieza el primer día de la primavera.
- **Duración de la luz según la estación:** en pleno invierno la noche alcanza **20 minutos** y el día 10; en pleno verano, al revés. En los equinoccios (mitad de la primavera y del otoño) son 15 y 15. El cambio es gradual día a día.
- **Alba y ocaso:** penumbra de 90 segundos de juego con luz cálida; el sol bajo también entibia los colores.
- **La noche se ve:** la escena se oscurece y azulea, aparecen las estrellas y los fuegos alumbran a su alrededor (fogata del campamento, fogatas, hogueras, hornos y la antorcha encendida en la mano).
- **HUD:** el panel dice el día, la estación, la fase y los minutos que faltan para el próximo cambio; bajo el minimapa, una barra muestra el reparto de luz (oro) y noche (lapislázuli) con la hora actual.
- **Prueba:** `./build/estepa --dia 25 --minuto 14` arranca en una noche de pleno invierno.

**Pendiente:** efectos jugables de la noche (visión y sigilo, fieras, frío).

### Clima y estaciones (implementado — `src/sim/climate.*`, `src/world/terrain.*`, `src/world/weather.*`)
Todo sale de una función pura del instante y la semilla (`climate_at`): igual en cada máquina, sin estado que guardar.
- **Temperatura continental:** de unos −18 °C a mitad del invierno a +24 °C a mitad del verano; sube de día y baja de noche. La partida empieza al final del invierno: el deshielo llega hacia el día 5.
- **Tiempo atmosférico** en bloques de 7,5 minutos con transición suave: despejado, nublado, lluvia, tormenta eléctrica, nevada y ventisca. Cada estación tiene sus probabilidades (verano seco con tormentas, otoño lluvioso, nevadas en invierno) y con helada la precipitación es nieve. El tiempo tiende a durar más de un bloque.
- **Suelo según la estación:** pasto verde al final de la primavera, seco y amarillo al final del verano, ocre en otoño y dormido con helada. La lluvia lo oscurece y embarra. La nieve lo cubre en manchas que crecen hasta tapar todo en invierno; cuesta más en las pendientes. Las yurtas y las copas de los árboles se nevan.
- **Lagos:** ocupan las hondonadas más bajas (7 % del terreno bajo su nivel base, siempre debajo del campamento). Crecen hasta 3 m con el deshielo de primavera y las lluvias y bajan al final del verano, dejando a la vista el lecho seco y una orilla de barro. En invierno se congelan.
- **Cordilleras y glaciares:** lejos del campamento se levantan crestas de hasta ~60 m. La nieve permanente baja en invierno (glaciares extensos) y sube en verano (solo las cumbres).
- **Atmósfera:** lluvia y nieve como partículas ancladas al mundo, ventisca casi horizontal que blanquea la vista, relámpagos en las tormentas, cielo gris y luz apagada con nubes; las nubes tapan las estrellas.
- **HUD:** bajo la barra del ciclo, el tiempo y la temperatura (p. ej. «nevada · −12 °C»).
- **Modelos por estación:** un modelo puede traer variantes junto al base (`yurta_comun@invierno.glb`, `@primavera`, `@verano`, `@otono`); el juego usa la de la estación si existe.
- **Prueba:** `--dia N --minuto M` elige el momento y `--pos X Z` el lugar (p. ej. `--dia 23 --pos -340 -130`: glaciares en invierno; `--dia 7 --pos -112 -2`: un lago crecido).

**Pendiente:** cosechas y pasto para el ganado según la estación, ríos.

### Peligros del clima y del terreno (implementado — `src/sim/hazards.*`, `src/game/hazards_game.*`)
- **Frío:** el jugador tiene **calor corporal** (barra bajo el minimapa: abrigado, fresco, frío, helado, hipotermia). Lo baja la sensación térmica: temperatura + abrigo de pieles − viento − ropa mojada. Lo sube estar junto a un fuego (fogata del campamento, fogatas, hogueras, hornos); las yurtas cortan el viento y la antorcha da algo de calor. La lluvia empapa y el fuego seca. Con frío el cuerpo se entumece (más lento) y con **hipotermia** el jugador se desmaya: la tribu lo lleva junto al fuego (moral −3). Una noche de pleno invierno a la intemperie congela en unos minutos.
- **Leña:** en las noches heladas cada fuego del campamento gasta leña del acopio (el doble con frío extremo). Si falta, la tribu pasa frío (moral −4).
- **Barro:** la lluvia y el deshielo frenan la marcha en suelo blando (hasta −40 %); no en la arena ni bajo la nieve.
- **Agua:** sin hielo, los lagos se vadean (más lento, la ropa se moja) o se nadan.
- **Lagos helados:** con el lago congelado se camina sobre el hielo, pero la capa puede romperse: más riesgo corriendo, a caballo y con hielo recién formado. El agujero queda abierto hasta el día siguiente.
- **Desierto:** regiones de dunas lejos del campamento (arena dorada, sin barro, casi sin nieve).
- **Socavones ocultos:** cada día aparecen en sitios al azar distintos: **socavones de nieve** en los glaciares y **arena movediza** en el desierto. No se ven; al acercarse solo una pista sutil (un cerco de grietas o de arena húmeda). Pisarlo atrapa.
- **Minijuego de rescate (coordinación mano-ojo):** al caer, el juego pide una serie de teclas (J K L U I O) que hay que pulsar en orden y a tiempo; cada acierto acorta el tiempo de la siguiente y los errores (o las demoras) se cuentan. Ganar: sales (empapado si fue el hielo). Perder: sales a duras penas, helado, y se hunde lo que cargabas o tu arma.
- **Escolta (Y):** dos integrantes libres te acompañan. También pueden romper el hielo o caer en un socavón: entonces corre el tiempo (unos 40–60 s) y hay que acercarse y pulsar **F** para sacarlos con el minijuego (si falla, se puede reintentar con menos tiempo). Salvarlo sube la moral (+3); si no llegas, muere (moral −6). Si caes tú con la escolta cerca, te tienden la lanza: la serie es más corta y con más margen.
- **Prueba:** `--trampa hielo|nieve|arena|rescate` arranca en cada caso (p. ej. `--dia 25 --pos -148 -12 --trampa rescate`; arena: `--dia 12 --pos 352 20 --trampa arena`).

**Pendiente:** heridas y salud, ropa y abrigo como equipo, que los NPCs del campamento también sufran el frío individualmente, animales que caen al hielo.

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

### Animales, trepar y esconderse (implementado — `src/sim/animals.*`)
- **Animales del mundo:** caballos, camellos, ciervos, lobos e íbices deambulan por su territorio y huyen del jugador si se acerca demasiado.
- **Lazo:** intento de doma con una probabilidad propia de cada especie. Si lo logra, el animal sigue a la tribu; si falla, se escapa.
- **Silla y montura:** la silla instala la montura en un animal domado y montable (caballo o camello). `R` monta y desmonta. Montado, el jugador va más rápido: caballo ×2,4, camello ×1,7.
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

### 4. Movimiento (caminar, correr, acechar y saltar implementados)
- **Implementado:** caminar, correr, acechar (más lento, emite menos ruido) y saltar. El "ruido" de cada postura queda expuesto para la IA de detección.
- **Pendiente:** esconderse (hierba alta, interiores), trepar árboles y muros.
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
- **Interfaz:** orfebrería de estilo animal (placas de oro y plata, granulado, turquesa, cuero). Ver [ESTILO_VISUAL.md](ESTILO_VISUAL.md).
- **Animaciones:** cada modelo animado tiene su lista de clips en [ANIMACIONES.md](ANIMACIONES.md); el juego elige el clip según lo que hace el personaje o el animal.
- **Inventario de assets:** todos los modelos del juego están listados en [INVENTARIO.md](INVENTARIO.md), con id, medidas, presupuesto y estado.
- Los modelos se generan por código con **Kiln** (`tools/assets/*.kiln.js` → GLB). Ver [ROADMAP.md](ROADMAP.md).

## Preguntas abiertas
- Título definitivo.
- Preguntas de la narrativa: ver [NARRATIVA.md](NARRATIVA.md#preguntas-abiertas-para-el-autor).
- ¿Muerte permanente del líder o escenarios de derrota a lo Outward?
- Escala del mapa y número de reinos.
