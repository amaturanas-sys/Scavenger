# Roadmap

Próxima gran actualización (combate de cuatro botones, tribu, economía, caballos, armas y asentamientos agrarios): [PLAN_GRAN_ACTUALIZACION.md](PLAN_GRAN_ACTUALIZACION.md).

Herramientas de generación (edificios con un kit de piezas, animaciones desde videos con GRAIL o alternativas, modelos desde imágenes con TRELLIS.2): [PLAN_HERRAMIENTAS_IA.md](PLAN_HERRAMIENTAS_IA.md).

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
- [x] Minimapa con la geografía (regiones, relieve, ríos, lagos, mar, nieve) y los lugares vistos.
- [x] HUD de rol: placa de constantes (vida, calor, sed, aguante), barra rápida 1-9 y columna de acciones táctiles.
- [x] Muerte y regreso al último lugar de descanso, con calavera y huesos donde se cae.
- [x] Aves: no atacan a las personas; rapaces que cazan en picado y cuervos carroñeros.
- [x] Memoria espacial: empieza negro, se ilumina al recorrer, lo frecuente brilla más y todo se olvida con el tiempo (lo familiar, más despacio). Con tests.
- [x] Marcas de interés y de peligro.
- [x] Cobertura de todo el mundo (memoria dispersa por páginas) y olvido medido en días de juego.
- [ ] Marcas con nombre y guardado.

## Acciones (ver la sección «Acciones» de [GDD.md](GDD.md))
- [x] Manos: empuñaduras (una mano, una en cada mano, dos manos, arma y escudo), enfundar y desenfundar, tomar y lanzar. Con tests.
- [x] Acciones individuales para el jugador y los NPCs: fogata, tienda, trinchera, trepa, lazo, antorcha, montura.
- [x] Construcciones en grupo con cuadrilla mínima, oficios y habilidad (función, dones, moral). Con tests.
- [x] En juego: menú de acciones (Tab), atajos (1 a 9, F, T), barra de progreso, obras que avanzan con la tribu.
- [x] NPCs que caminan a las obras e instalan fogatas y tiendas por su cuenta.
- [x] Acopio de materiales y comida, recolección diaria por función, efectos de cada construcción y forja de bronce y acero.
- [x] Animales: deambular y huir, doma con el lazo, ensillar y montar.
- [x] Fauna: 24 especies en 5 clases (monturas, depredadores domables, hostiles, presas, ganado); manadas, acecho, rivalidad y huida; caza, despiece, ordeño y pastoreo.
- [x] Inventario por contenedores (bolsillos, mochila, alforjas, carreta, acopio y armería) con peso y estado; menú de equipo con armadura por piezas y amuletos.
- [x] Menú de entrada (nueva partida, cargar con minifoto y fecha, instructivo), pausa con guardar y cargar, HUD limpio (F1 controles).
- [x] Partidas por bloques con el esquema de sus campos (v0.4.4): las versiones nuevas cargan las partidas de las anteriores aunque cambien los structs; partidas de referencia en CI ([PARTIDAS.md](PARTIDAS.md)).
- [x] Los edificios de la edad del hierro (v0.10.0, la parte B del plan de herramientas): la investigación por región y un kit de piezas con reglas y semilla (`src/sim/buildkit.c`, con tests): yurtas y carretas-casa en la estepa, casas de postes y paja en el bosque, de piedra y adobe en el altiplano, largas de turba en los fiordos y de adobe con patio en el desierto; el palacio de cada región, la fundición con su horno de cuba, el cuartel, la carpintería, los graneros y el templo del fuego, con cuatro variantes; las ciudades, las aldeas y las tribus se arman con él; lo de adentro se rellena y de lejos van con la mitad de detalle; `--edificios` los muestra.
- [x] La tubería de modelos (v0.9.3, con la herramienta de Cline): de una imagen (TRELLIS.2 en Hugging Face) o un GLB al juego en un paso: low-poly bajo el `tris_max` del inventario, colores planos con la luz horneada, a escala, en la ruta de su id y con su procedencia anotada; las personas y los animales esperan su esqueleto (RigNet) y sus clips; `--ver <id>` muestra un objeto de cerca; chequeo y pruebas en el CI.
- [x] La vegetación (v0.9.2, desde el PR #115 de Cline): pasto, stipa con plumas plateadas, flores de primavera, helechos y juncos alrededor del jugador, del color del suelo de su región y su estación; crecen, se secan, se aplastan en invierno y la nieve los tapa (asoman la stipa y los juncos secos); el viento los mece en olas y se apartan al pasar; nada en el agua honda, la roca, los ríos, los pueblos ni los campamentos, y lo quemado reverdece. Una ventana que sigue al jugador, con menos radio en Android.
- [x] Ejército y asedio (v0.9.1): el ejército de los reinos sale de su guarnición y de su tesoro: infantes pesados con pavés en formación cerrada (el pavés para las flechas de frente), soldados, arqueros, ballesteros y carros de guerra que vuelan en el llano, se arrastran en la cuesta y en el agua, hacen pasadas y arrollan; con la relación por el suelo (o mucho rencor) salen expediciones de castigo contra el campamento, que lo saquean si llegan; en guerra, la ciudad cierra sus puertas, la muralla corta el paso y la guarnición sale a pelear; se entra trepando, con escalas o quemando una puerta (flechas encendidas o la antorcha), el cerco la deja sin grano y el fuego quema su granero; rendida, paga tributo y su fundición es tu taller, y tomar la capital del kanato acaba con el tributo.
- [x] Ciudades y reinos (v0.9.0): las capitales crecen desde el centro (el palacio con su corte, la fundición, el cuartel y la carpintería al estilo de su región, los anillos de casas, los campos con surcos que siguen la estación y los corrales, con labradores de día) y las aldeas también; cada asentamiento tiene su economía día a día (gente, cosechas por estación y región, granero, impuestos según los escribas, talleres de armas, tesoro, soldados y mercenarios que crecen con la riqueza); el mercado cambia con su granero y su armería, y paga con su tesoro; los campos se saquean (grano que la tribu come) y sale su guardia de soldados del reino, cuyas bajas pesan en su guarnición; el tributo al kanato se paga en su capital una vez por estación.
- [x] La forja (v0.8.1): vetas de hierro a cielo abierto en el altiplano y hierro de pantano en las orillas, que se ven, quedan en el mapa al pasar (y las marcan los exploradores) y se pican con F; el mineral se funde en el horno de acero; la forja móvil (a mano, también en la migración) hace puntas de hierro, cuchillos y sables con una hoja comprada, y repara el metal; el taller del herrero, solo en un campamento de 8 personas o más, forja el sable wootz y la lanza de punta de acero, y templa: filo duro y lomo flexible, o un arma frágil que se quiebra en el golpe; el wootz llega con las caravanas y las hojas, de la capital.
- [x] Las armas de la estepa (v0.8.0): el arco compuesto reflejo (en tres etapas, con dos días de secado; a caballo, sin castigo) y el arco simple; el sable que a caballo corta limpio y la akinakes; la lanza pesada que en ristre rompe la formación; el sagaris de pico (perfora) y de martillo (aturde); la maza que abate y vence al casco; la daga de anillo para la mano torpe, las llaves, el rehén y la ejecución.
- [x] Migración en caravana (v0.7.2): el guardián levanta el campamento (yurtas, tiendas y chozas cargadas con el acopio) y la tribu te sigue en columna con el rebaño; donde quieras, lo planta en corro alrededor de un fogón nuevo; el pasto de cada región cambia con la estación (forraje de más con buen pasto, hambre del ganado con pasto pobre) y el guardián avisa si conviene migrar.
- [x] Caballos (v0.7.1): el caballo sigue su rumbo y el jinete se gira para tirar (el disparo parto, al galope y hacia atrás); la lanza en ristre (L) a galope tendido; la montura se cansa (su barra en el HUD) y corre según lo heredado, el hambre y la sed; cría en el campamento con herencia de velocidad y aguante; arqueros a caballo que rodean y se alejan.
- [x] Oficios, comercio y saqueo (v0.7.0): cada oficio trabaja lo que sobra del acopio (cuero, fieltro, madera labrada, adobe, puntas de hueso, velas, ungüento, peines); talabartería, hueso y asta (el arco compuesto), velas, queso y obras duraderas (adobe, troncos, fieltro); todo vale plata y el precio cambia con la región; se comercia con tribus, aldeas, capitales, caravasares y caravanas; las caravanas van y vuelven con escolta y se pueden asaltar; los campamentos rivales se saquean.
- [x] Obras ubicadas desde arriba (v0.6.2): la cámara sube sobre el campamento y la obra (un fantasma verde o rojo, con el motivo) sigue al puntero, al dedo o a WASD; Q/E la giran, Mayús la alinea a una rejilla; las cercas y los muros se trazan de un extremo al otro por tramos; la obra terminada conserva su giro.
- [x] La tribu desde el guardián (v0.6.1): la ventana de pobladores (oficio, salud, ánimo y tarea); uno o dos salen a buscar reclutas, explorar (marcan agua, pastos, rivales y ciudadelas en el mapa) o cazar (según la región y la estación), o se quedan a pastorear (el ganado come, bebe y da leche) o hacer guardia; acompañados y con el oficio corren menos riesgo y rinden más; informes al volver.
- [x] Escolta con órdenes (atacar, defender, seguir: Y+1..3 o casillas) que se retira herida; soldados que hacen la ronda del campamento y salen al encuentro; asaltos al campamento y pobladores que se esconden en las yurtas (v0.6.0).
- [x] La espalda, rehenes y abatidos (v0.5.1): por detrás los enemigos solo oyen; con la espalda ganada, K ejecuta en silencio (solo lo oyen a menos de 6 m) y G toma rehén (los suyos no se acercan ni disparan); los golpes no letales tumban y el abatido despierta y huye a avisar; F lo toma prisionero, H o K lo rematan (con su costo moral).
- [x] Combate de cuatro botones (v0.5.0): H ataque, J bloqueo (escudo o arma), K parry en la ventana justa (enganche, llave o desvío), L carga o patada; los rivales anuncian el golpe; selector de objetivo (Tab o un toque) con anillo y puntería sola a distancia; agacharse (X) y esconderse tras rocas, matas y hierba alta; mapa de teclas nuevo (F interactúa con todo) y casillas táctiles de combate.
- [x] Cuerpo a cuerpo para todos: combos con armas cortas y largas, golpe pesado, patada y patada con inercia, escudo (bloqueo, golpe, carga), agarre y llave, gancho al escudo; enfundar, empuñar y cambiar de mano.
- [x] Clima con azar: incendios forestales en verano, rayos que queman árboles y estructuras, derrumbes por lluvia torrencial sin mantenimiento; la lluvia apaga todo fuego. Flechas encendidas.
- [x] Animales acuáticos y anfibios (peces, cocodrilos, tortugas), venenosos (víbora, escorpión, araña) con veneno, e insectos en enjambre (abejas con miel, avispas, mosquitos, moscas).
- [x] Menú de fabricar (a mano y forja) con ingredientes recolectables, y reparación de armaduras por material.
- [x] Interfaz en español e inglés (botón de idioma; `assets/i18n/en.tsv`).
- [x] Nivel y experiencia; árbol de tatuajes permanentes (5 motivos × 3 grados con ramas excluyentes, 8 zonas del cuerpo) hechos por el druida; joyas (anillos, brazaletes, collar, aretes, hebilla) del orfebre con metal y piedra, encantadas por el druida (pasivo o activo); piedras de rocas, corales, excavaciones y ríos.
- [x] Campamentos: fundar con una estructura básica y un guardián; acopio y gente por campamento; diálogo del guardián (obras, escolta, reclutar, capacitar en 7 oficios, estado, disolver con fuego y carga en carretas).
- [x] Mochilas de tres tamaños (frenan las grandes), dejarlas y recogerlas (Mayús+G), cargarlas en carretas y animales; mochilas de la escolta. Los animales de la tribu comen: pastan, cazan o se les da forraje o carne; los pastores los alimentan; con hambre se van.
- [x] Órdenes a la escolta (Mayús+Y): despachar seguidores a campamentos o sitios marcados que alguno conozca, con riesgo según conocimiento, distancia, grupo y noche; mensajeros que vuelven con refuerzos.
- [x] Ropa y clima: prendas por capas (abrigo, sombra, lluvia, peso), calor y golpe de calor, desierto que quema de día y hiela de noche, pieles de depredador que escarmientan, 20 recetas a mano, la tribu se cambia según el tiempo.
- [x] El agua: sed (más con calor), espíritus malditos en el agua cruda (incubación y fiebre), hervir y el alcohol purifican, borrachera (torpe y fatigado), la tribu bebe cada día, los animales tienen sed.
- [x] El mundo: gran círculo con cinco regiones fijas de altitud estandarizada; borde de mar con fiordos, muro con cañón y gran canal con tributarios; cada partida cambia ríos, lagos, montañas, pueblos y guaridas; reinos, aldeas, capitales y guaridas por región; vista orbital (F5) y mapa general.
- [x] El mundo v2: radio doble y borde fractal; estepa central rodeada de meandros; bosque tupido al noroeste con canal sigmoideo y tributarios; desierto al noreste con gran cañón y muro de estratos; altiplano al sureste con muro de hielo y ríos trenzados; fiordos al suroeste. Texturas del suelo, horizonte lejano, cielo con sol, luna con fases y estrellas, y nubes con viento que envuelven las cumbres.
- [x] Tras la prueba en tablet (v0.1.1–v0.2.0): assets dentro del APK; controles con alternativa Ctrl, botón Atrás y botones táctiles; diagnóstico; animales fuera de los lagos (beben desde la orilla); arroyos, rocas, matas y flores; más fauna; tribus rivales, neutrales y amigables; ruinas y estructuras por región; nubes pixeladas sacadas de fotos.
- [x] Voxeles (v0.3.0): nubes al modo de Nubis (perfil dimensional, erosión por ruido, Beer-Lambert y ambiente horneados) con carteles a lo lejos; estructuras y casas de voxeles con caras expuestas y oclusión ambiental; ruinas erosionadas.
- [x] Paisajes de las fotos (v0.3.1): playas volcánicas, basalto, farallones y espuma (fiordos); lechos de cantos y rápidos (ríos); farallones estratificados, torres de arenisca y creta (desierto); seracs y témpanos (glaciar).
- [x] Tras la prueba en Lenovo P11 (v0.3.2): imágenes y modelos en Android (ruta sin prefijo `assets/`), minifotos y ajustes; agua continua con pendiente y textura; fogatas con halo chico y punto brillante; voxeles al doble de resolución; pozos con agua limpia.
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
