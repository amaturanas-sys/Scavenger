## Qué cambia
<!-- En pocas líneas y con los números de verdad: lo que hace el diff, no lo que se quería. -->

## Cómo se probó
- [ ] `cmake --build build && ./build/sim_tests` (comprobaciones: …)
- [ ] i18n (`extraer` y `check`), inventario, animaciones, íconos, esquema y modelos (`.clinerules`, «Pruebas antes de subir»)
- [ ] Las partidas de referencia (`--probar-partidas ../tests/partidas`)
- [ ] Compilado de cero después de traer `main` (`git merge origin/main`)
- [ ] Una captura mirada: <!-- el comando y lo que se ve -->

## Documentos y versión
- [ ] El documento del sistema (GDD, MUNDO, ARQUITECTURA…)
- [ ] Si cambia el juego: `VERSION`, la sección de la versión en el `README.md` y su línea en `docs/ROADMAP.md`
- [ ] Si cambia lo que se ve: las capturas (`bash tools/capturas.sh build`) y `docs/CAPTURAS.md`
- [ ] `docs/GUIA_IA.md`, sección 6 (el estado)

## Pendiente o para decidir
