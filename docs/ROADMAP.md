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
- [ ] Esconderse y trepar.
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
- [ ] Ciclo día/noche y clima.

## Narrativa (ver [NARRATIVA.md](NARRATIVA.md))
- [ ] Prólogo jugable con el padre: combate a caballo en el asedio, tutorial de batalla y derrota por guion (requiere caballo y combate base).
- [ ] Estado del mundo persistente (decisiones y final elegido), integrado con el guardado.
- [ ] Años de exilio visibles en el mundo; eventos en los que el culto toma prisioneros y rescates con relatos.
- [ ] Afinidad u hostilidad por pueblo según la conducta del jugador.
- [ ] Grandes guerreros: integrantes únicos con estados de integrante, enemigo, desertor o prisionero.
- [ ] Tres finales (asedio, asesinato, exilio) que modifican facciones, PNJ, asentamientos, misiones y tesoros; juego libre después del final.

## Interfaz (ver [ESTILO_VISUAL.md](ESTILO_VISUAL.md))
- [x] Tema de orfebrería: paneles, barras, separadores y texto (`src/ui/theme.*`), aplicado al HUD y al aviso de teclado.
- [ ] Fuente de mapa de bits propia, emblemas pixel art de bestias en combate y marcos calados para menús.

## Herramientas
- **Assets 3D:** Kiln (`tools/assets/*.kiln.js`). Ver [tools/assets/README.md](../tools/assets/README.md).
- **Optimización:** Ghidra sobre nuestros propios builds. Ver [GHIDRA.md](GHIDRA.md).
