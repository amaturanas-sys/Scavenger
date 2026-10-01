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

- **Grandes guerreros:** personajes únicos que pueden ser integrantes, enemigos, desertores o prisioneros según la moral de la tribu (ver [NARRATIVA.md](NARRATIVA.md#reputación-y-grandes-guerreros)).
- **Afinidad con los pueblos del mundo:** las decisiones secundarias y la conducta del jugador mueven la hostilidad o afinidad de cada pueblo hacia la tribu.

**Pendiente de definir:** consecuencias jugables de una rebelión (duelo por el liderazgo, escisión de la tropa), cómo se ganan nuevos reclutas en el mundo, economía del tributo.

### Mapa de memoria y brújula (implementado — `src/sim/memory_map.*`, `src/ui/minimap.*`)
- **Minimapa circular** en la esquina superior derecha que gira con la vista, como una brújula: arriba es hacia donde mira el jugador, y una "N" en el aro marca el norte.
- **Empieza negro:** el personaje no conoce el mundo. Las zonas se **iluminan poco a poco** a medida que el jugador las recorre.
- **Frecuencia = nitidez:** cada zona acumula familiaridad. Lo recorrido a menudo brilla más, como la memoria espacial real.
- **Olvido:** el brillo se apaga con el tiempo. Lo familiar se olvida más despacio (repetición espaciada).
- **Marcas:** el jugador marca sitios de interés (`M`) o de peligro (`Shift+M`). Pulsar de nuevo cerca de una marca la quita. Las marcas fuera de alcance quedan en el borde, señalando su dirección.
- Los parámetros (radio de vista, velocidad de aprendizaje y de olvido, brillo de una sola pasada) están en `memmap_default_params` y son de balance.

**Pendiente:** mapa grande a pantalla completa, guardar el mapa con la partida, marcas con nombre, y un mapa que cubra todo el mundo (hoy cubre 1,5 km × 1,5 km alrededor del origen).

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
- Pendiente: biomas (desierto, montaña, ríos), asentamientos de los reinos, ciclo día/noche, clima.

## Narrativa
Historia principal con protagonista de **nombre elegido por el jugador**: hijo de un jefe nómade exiliado, traicionado por su medio-hermano, que sobrevive en cinco climas bajo la filosofía *Scavengers Thrive* y regresa a la ciudad de su infancia. El prólogo se juega con el padre, a caballo, durante la masacre. Hay **tres finales** (asedio, asesinato o exilio del medio-hermano) que cambian el mundo, y la partida continúa después. Argumento completo en [NARRATIVA.md](NARRATIVA.md). Los diálogos y las escenas los define el autor.

## Dirección de arte
- 3D low-poly con sombreado plano, renderizado a 640×360 y escalado sin suavizado.
- Paleta terrosa de estepa; acentos saturados en objetos importantes (puertas de las yurtas, fuego, estandartes).
- **Interfaz:** orfebrería de estilo animal (placas de oro y plata, granulado, turquesa, cuero). Ver [ESTILO_VISUAL.md](ESTILO_VISUAL.md).
- Los modelos se generan por código con **Kiln** (`tools/assets/*.kiln.js` → GLB). Ver [ROADMAP.md](ROADMAP.md).

## Preguntas abiertas
- Título definitivo.
- Preguntas de la narrativa: ver [NARRATIVA.md](NARRATIVA.md#preguntas-abiertas-para-el-autor).
- ¿Muerte permanente del líder o escenarios de derrota a lo Outward?
- Escala del mapa y número de reinos.
