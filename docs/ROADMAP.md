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

## Fase 0b — Android
- [ ] Build del APK en CI (NDK + empaquetado con `NativeActivity`).
- [ ] Controles táctiles: stick virtual, botones de acción, gesto para la cámara.
- [ ] Prueba de rendimiento en un dispositivo de gama baja (meta: 60 fps).

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

## Herramientas
- **Assets 3D:** Kiln (`tools/assets/*.kiln.js`). Ver [tools/assets/README.md](../tools/assets/README.md).
- **Optimización:** Ghidra sobre nuestros propios builds. Ver [GHIDRA.md](GHIDRA.md).
