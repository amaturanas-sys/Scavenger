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

**Pendiente de definir:** consecuencias jugables de una rebelión (duelo por el liderazgo, escisión de la tropa), cómo se ganan nuevos reclutas en el mundo, economía del tributo.

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
Historia principal con protagonista de **nombre elegido por el jugador**: hijo de un jefe nómade exiliado, traicionado por su medio-hermano, que sobrevive en cinco climas bajo la filosofía *Scavengers Thrive* y regresa para liberar tanto a su pueblo como a la ciudad. Argumento completo en [NARRATIVA.md](NARRATIVA.md). Los diálogos y las escenas los define el autor.

## Dirección de arte
- 3D low-poly con sombreado plano, renderizado a 640×360 y escalado sin suavizado.
- Paleta terrosa de estepa; acentos saturados en objetos importantes (puertas de las yurtas, fuego, estandartes).
- Los modelos se generan por código con **Kiln** (`tools/assets/*.kiln.js` → GLB). Ver [ROADMAP.md](ROADMAP.md).

## Preguntas abiertas
- Título definitivo.
- Preguntas de la narrativa: ver [NARRATIVA.md](NARRATIVA.md#preguntas-abiertas-para-el-autor).
- ¿Muerte permanente del líder o escenarios de derrota a lo Outward?
- Escala del mapa y número de reinos.
