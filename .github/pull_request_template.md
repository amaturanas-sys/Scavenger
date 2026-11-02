## Qué cambia
<!-- En pocas líneas y con los números de verdad: lo que hace el diff, no lo que se quería. -->

## Cómo se probó
- [ ] Compilado de cero después de traer `main` (`git merge origin/main`), sin advertencias nuevas
- [ ] `./build/sim_tests` (comprobaciones: …, fallos: 0)
- [ ] `python3 tools/ci/guardas.py` sin errores
- [ ] i18n (`extraer` y `check`), inventario, animaciones, íconos, esquema y modelos (`.clinerules`, «Pruebas antes de subir»)
- [ ] Las partidas de referencia (`--probar-partidas ../tests/partidas`)
- [ ] Una captura mirada: <!-- el comando y lo que se ve -->

## Para Claude (lo llena quien implementa en el Codespace)
<!-- Claude Code verifica este PR antes de fusionarlo (docs/GUIA_IA.md, sección 2). -->
- **Hecho:**
- **Falta o no estoy seguro:**
- **Dónde mirar** (archivos, funciones, lugares del juego con su comando `--ir` o `--ver`):
- **Pruebas nuevas:** <!-- qué función de tests/test_main.c prueba cada regla nueva -->

## Documentos y versión
- [ ] El documento del sistema (GDD, MUNDO, ARQUITECTURA…)
- [ ] Si cambia lo que se ve: las capturas (`bash tools/capturas.sh build`) y `docs/CAPTURAS.md`
- [ ] Lo completa Claude al verificar: `VERSION`, la sección de la versión en el `README.md`, su línea en `docs/ROADMAP.md`, `docs/GUIA_IA.md` (sección 9) y la release

## Pendiente o para decidir
