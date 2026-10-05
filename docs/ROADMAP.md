# Roadmap

Enfoque: **rebanadas verticales**. Cada fase deja algo jugable y verificado, en vez de construir todos los sistemas a medias.

## Fase 0 — Fundaciones (hecha)
- [x] Proyecto C11 + raylib 5.5, CMake, ejecutable de ~1,7 MB.
- [x] Pipeline low-res: render a 640×360 escalado sin suavizado.
- [x] Estepa procedural por chunks con streaming e iluminación horneada.
- [x] Jugador: caminar, correr, acechar, saltar; cámara orbital en tercera persona.
- [x] Campamento con yurtas generadas por Kiln (GLB), fogata y árboles.
- [x] Núcleo de tropa, moral, deserción, rebelión y reinos tributarios, con tests.
- [x] Amuletos, tatuajes y desbloqueo del árbol de habilidades, con tests.
- [x] CI: tests + builds de Linux y Windows + captura automática del render.

## Fase 0b — Android (misma experiencia que en PC)
Decisión: el port de Android replica la experiencia de PC y se juega con **teclado físico** (tablets, Chromebooks); ratón o mando opcionales. **Sin controles táctiles.**
- [x] APK arm64 generado en CI con `NativeActivity` (`tools/android/build_apk.sh`, sin Gradle).
- [x] Detección de teclado en caliente: sin teclado, el juego se pausa con un aviso.
- [x] Sin filtro `reqHardKeyboard` en Google Play (excluiría tablets con teclado Bluetooth).
- [ ] Prueba en dispositivo real con teclado (meta: 60 fps).
- [x] Actualizaciones encima de la versión previa: clave de desarrollo fija (huella verificada en cada build) y `versionCode` creciente automático.
- [ ] Clave de release para Google Play en los secrets de GitHub.

## Fase 1 — Rebanada vertical: la tropa en el mundo
- [ ] Integrantes de la tropa como personajes en el campamento (deambulan según su función).
- [ ] Interacción en el mundo: hablar, asignar funciones, juzgar (reemplaza las teclas de prueba).
- [ ] Órdenes de escuadra en campo: seguir, mantener posición, atacar.
- [ ] Combate cuerpo a cuerpo base: agarre, derribo, combo de 3 golpes; arma corta y larga.
- [ ] IA enemiga con detección por vista y ruido (usa el "ruido" del jugador).
- [x] Esconderse (hierba alta) y trepar muros con la trepa.
- [ ] Consecuencias de la rebelión y de la deserción.
- [ ] Guardado y carga.

## Fase 2 — Movilidad
- [ ] Caballo, camello, elefante con habilidades propias.
- [ ] Carreta de bueyes (mover el campamento), botes, veleros, barcos, carruajes.
- [ ] Armas a distancia: arco, ballesta, mosquete, cañón.

## Fase 3 — Personaje
- [ ] Creador de personaje y vestuario.
- [ ] Contenido del árbol de habilidades, amuletos y tatuajes.

## Fase 4 — Mundo y reinos
- [ ] Biomas, asentamientos, tributo y diplomacia entre reinos.
- [x] Ciclo día/noche de 30 minutos con estaciones (noches de 10 a 20 minutos).
- [x] Clima: estaciones en el suelo, lagos que crecen y se hielan, glaciares, lluvia, nieve, ventisca y tormentas.
- [x] Efectos jugables del clima: frío y calor corporal, leña, barro, vadeo, lagos helados que se rompen, socavones (nieve y arena movediza), minijuego de rescate y escolta.
- [x] Salud y heridas para jugador, tribu y enemigos; combate cuerpo a cuerpo; enemigos (bandidos, culto, arqueros); vendajes y curandero.
- [x] Zonas del cuerpo con daño distinto; cuerpo articulado simple; armas a distancia con trayectoria balística; armadura por piezas con material y durabilidad.
- [ ] Ríos; abrigo como equipo; combate montado.

## Narrativa (ver [NARRATIVA.md](NARRATIVA.md))
- [ ] Prólogo jugable con el padre: combate a caballo en el asedio, tutorial de batalla y derrota por guion (requiere caballo y combate base).
- [ ] Estado del mundo persistente (decisiones y final elegido), integrado con el guardado.
- [ ] Años de exilio visibles en el mundo; eventos en los que el culto toma prisioneros y rescates con relatos.
- [ ] Afinidad u hostilidad por pueblo según la conducta del jugador.
- [x] Grandes guerreros: aparición por azar, historia combinada, dones y atributos de combate; integrantes o prisioneros sujetos a la moral de la tropa. Con tests.
- [ ] Grandes guerreros en el mundo: encuentros, como enemigos en otras facciones, y con su talla y su arma visibles en el modelo.
- [ ] Tres finales (asedio, asesinato, exilio) que modifican facciones, PNJ, asentamientos, misiones y tesoros; juego libre después del final.

## Interfaz (ver [ESTILO_VISUAL.md](ESTILO_VISUAL.md))
- [x] Tema de orfebrería: paneles, barras, separadores y texto (`src/ui/theme.*`), aplicado al HUD, al aviso de teclado y al aro del minimapa.
- [ ] Fuente de mapa de bits propia, emblemas pixel art de bestias en combate y marcos calados para menús.

## Mapa de memoria
- [x] Minimapa circular tipo brújula, que gira con la vista.
- [x] Memoria espacial: empieza negro, se ilumina al recorrer, lo frecuente brilla más y todo se olvida con el tiempo (lo familiar, más despacio). Con tests.
- [x] Marcas de interés y de peligro.
- [x] Cobertura de todo el mundo (memoria dispersa por páginas) y olvido medido en días de juego.
- [ ] Marcas con nombre y guardado.

## Acciones (ver la sección «Acciones» de [GDD.md](GDD.md))
- [x] Manos: empuñaduras (una mano, una en cada mano, dos manos, arma y escudo), enfundar y desenfundar, tomar y lanzar. Con tests.
- [x] Acciones individuales para el jugador y los NPCs: fogata, tienda, trinchera, trepa, lazo, antorcha, montura.
- [x] Construcciones en grupo con cuadrilla mínima, oficios y habilidad (función, dones, moral). Con tests.
- [x] En juego: menú de acciones (Tab), atajos (X, H, F, T), barra de progreso, obras que avanzan con la tribu.
- [x] NPCs que caminan a las obras e instalan fogatas y tiendas por su cuenta.
- [x] Acopio de materiales y comida, recolección diaria por función, efectos de cada construcción y forja de bronce y acero.
- [x] Animales: deambular y huir, doma con el lazo, ensillar y montar.
- [x] Fauna: 24 especies en 5 clases (monturas, depredadores domables, hostiles, presas, ganado); manadas, acecho, rivalidad y huida; caza, despiece, ordeño y pastoreo.
- [x] Inventario por contenedores (bolsillos, mochila, alforjas, carreta, acopio y armería) con peso y estado; menú de equipo con armadura por piezas y amuletos.
- [x] Menú de entrada (nueva partida, cargar con minifoto y fecha, instructivo), pausa con guardar y cargar, HUD limpio (F1 controles).
- [x] Cuerpo a cuerpo para todos: combos con armas cortas y largas, golpe pesado, patada y patada con inercia, escudo (bloqueo, golpe, carga), agarre y llave, gancho al escudo; enfundar, empuñar y cambiar de mano.
- [x] Clima con azar: incendios forestales en verano, rayos que queman árboles y estructuras, derrumbes por lluvia torrencial sin mantenimiento; la lluvia apaga todo fuego. Flechas encendidas.
- [x] Animales acuáticos y anfibios (peces, cocodrilos, tortugas), venenosos (víbora, escorpión, araña) con veneno, e insectos en enjambre (abejas con miel, avispas, mosquitos, moscas).
- [x] Menú de fabricar (a mano y forja) con ingredientes recolectables, y reparación de armaduras por material.
- [x] Interfaz en español e inglés (botón de idioma; `assets/i18n/en.tsv`).
- [x] Nivel y experiencia; árbol de tatuajes permanentes (5 motivos × 3 grados con ramas excluyentes, 8 zonas del cuerpo) hechos por el druida; joyas (anillos, brazaletes, collar, aretes, hebilla) del orfebre con metal y piedra, encantadas por el druida (pasivo o activo); piedras de rocas, corales, excavaciones y ríos.
- [x] Campamentos: fundar con una estructura básica y un guardián; acopio y gente por campamento; diálogo del guardián (obras, escolta, reclutar, capacitar en 7 oficios, estado, disolver con fuego y carga en carretas).
- [x] Mochilas de tres tamaños (frenan las grandes), dejarlas y recogerlas (G), cargarlas en carretas y animales; mochilas de la escolta. Los animales de la tribu comen: pastan, cazan o se les da forraje o carne; los pastores los alimentan; con hambre se van.
- [x] Órdenes a la escolta (Mayús+Y): despachar seguidores a campamentos o sitios marcados que alguno conozca, con riesgo según conocimiento, distancia, grupo y noche; mensajeros que vuelven con refuerzos.
- [x] Ropa y clima: prendas por capas (abrigo, sombra, lluvia, peso), calor y golpe de calor, desierto que quema de día y hiela de noche, pieles de depredador que escarmientan, 20 recetas a mano, la tribu se cambia según el tiempo.
- [x] El agua: sed (más con calor), espíritus malditos en el agua cruda (incubación y fiebre), hervir y el alcohol purifican, borrachera (torpe y fatigado), la tribu bebe cada día, los animales tienen sed.
- [x] El mundo: gran círculo con cinco regiones fijas de altitud estandarizada; borde de mar con fiordos, muro con cañón y gran canal con tributarios; cada partida cambia ríos, lagos, montañas, pueblos y guaridas; reinos, aldeas, capitales y guaridas por región; vista orbital (F5) y mapa general.
- [x] El mundo v2: radio doble y borde fractal; estepa central rodeada de meandros; bosque tupido al noroeste con canal sigmoideo y tributarios; desierto al noreste con gran cañón y muro de estratos; altiplano al sureste con muro de hielo y ríos trenzados; fiordos al suroeste. Texturas del suelo, horizonte lejano, cielo con sol, luna con fases y estrellas, y nubes con viento que envuelven las cumbres.
- [x] Combate montado (golpe con inercia, lanza que derriba, arrollar, la montura recibe golpes, derribo que desmonta), jinetes bandidos y botín de los enemigos.
- [ ] Trepar árboles y rocas sin trepa (Fase 2).

## Inventario de assets (ver [INVENTARIO.md](INVENTARIO.md))
- [x] Inventario de 304 objetos en 14 categorías con etiquetas, medidas, presupuesto de triángulos y estado (`assets/inventario.tsv`).
- [x] Ruta fija por id, marcadores provisionales y reemplazo automático al importar el modelo.
- [x] Validador y documento generado (`tools/assets/inventario.py`), comprobados en CI.
- [x] Galería en el juego (`--galeria`) para revisar los modelos a escala.
- [x] Activar text-to-cad (skill CAD en `.claude/skills/cad`, `tools/assets/cad.py`).
- [ ] Importar y refinar los modelos (text-to-cad + Kiln).
- [ ] Usar los modelos del inventario en el mundo (NPCs, animales, armas equipadas, etc.).

## Animaciones (ver [ANIMACIONES.md](ANIMACIONES.md))
- [x] Índice de clips por esqueleto (humanoide, cuadrúpedo, ave, mecanismo) y qué modelos los necesitan (`assets/animaciones.tsv`).
- [x] Validador y documento generado (`tools/assets/animaciones.py`); comprueba los nombres de los clips dentro de los GLB importados. En CI.
- [x] El juego elige el clip según el estado (`src/sim/anim_index.*`, con tests) y lo reproduce en cuanto el modelo exista (jugador, NPCs, animales).
- [ ] Importar los modelos animados (cuerpos base, protagonista, monturas, fauna).
- [ ] Mezcla entre clips y capas (parte superior del cuerpo para empuñar mientras se camina).

## Herramientas
- **Assets 3D:** inventario en `assets/inventario.tsv`; importación en [assets/models/README.md](../assets/models/README.md); Kiln (`tools/assets/*.kiln.js`) y text-to-cad para crear y refinar.
- **Optimización:** Ghidra sobre nuestros propios builds. Ver [GHIDRA.md](GHIDRA.md).
- **Skills y agentes de Claude Code:** `.claude/skills/` (programación de juegos, C/C++, pruebas, depuración, revisión, diseño de juego) y el equipo `gamedev-*` en `.claude/agents/` con `/gamedev <tarea>`. Origen y licencias en [.claude/skills/THIRD_PARTY.md](../.claude/skills/THIRD_PARTY.md); convenciones del proyecto en `.claude/skills/estepa/`.
