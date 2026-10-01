# Arquitectura

## Principios
1. **Liviano por defecto.** C11 + raylib, sin motor. Cada dependencia nueva debe justificar su peso.
2. **Simulación separada del render.** `src/sim/` es C puro sin raylib: se testea sin pantalla, es determinista y portable.
3. **Datos sobre código.** El balance (efectos de acciones, valores de los reinos) vive en tablas, no en lógica dispersa.
4. **Assets como código.** Los modelos se describen en programas Kiln versionados; el GLB es un derivado.

## Estructura

```
src/
  sim/      Nucleo determinista (sin raylib)
    rng.h        xorshift32 con semilla
    noise.*      ruido de valor + fBm (terreno)
    troop.*      tropa, moral, politica, reinos
    loadout.*    amuletos, tatuajes, arbol de habilidades
    memory_map.* mapa de memoria: exploracion, olvido, marcas
  world/    Mundo (raylib)
    terrain.*    chunks con streaming, altura consultable
    camp.*       campamento, props
  game/     Jugador e input
    player.*
  ui/       Interfaz (raylib)
    theme.*      tema de orfebreria: paneles, barras, texto
    minimap.*    minimapa circular tipo brujula
  main.c    Bucle principal, render low-res, HUD
tests/      Tests del nucleo (arnes propio, sin dependencias)
assets/     Assets de runtime (GLB, texturas, audio)
tools/      Herramientas de produccion (programas Kiln)
docs/       Diseño, roadmap, arquitectura
```

## Render
- Todo el mundo y el HUD se dibujan en un `RenderTexture` de **640×360** con filtro *point*, y se escala a la ventana conservando la proporción.
- Iluminación **horneada** en los colores de vértice del terreno (ambiente + difusa del sol): sin shaders de luz, costo cero en GPU.
- Objetivo de rendimiento: 60 fps en Android de gama baja. Referencia actual: 60 fps con render **por software** (Mesa llvmpipe) en CI.

## Mundo
- Chunks de 48 m con 24×24 celdas (2 m por celda, facetado low-poly); se mantienen 5×5 chunks cargados alrededor del jugador.
- `terrain_height(x, z)` es una función pura: física, IA y colocación de props la consultan sin tocar mallas.
- El terreno se aplana alrededor del campamento inicial.

## Plataformas
- **Windows / Linux:** CMake descarga raylib 5.5 (FetchContent) y lo enlaza estáticamente.
- **Android:** mismo código; con la toolchain del NDK el juego se compila como `libestepa.so` (raylib en modo Android, OpenGL ES 2) y `tools/android/build_apk.sh` lo empaqueta con `aapt2`, `zipalign` y `apksigner`, sin Gradle. Los assets van dentro del APK.
- `src/platform.*` concentra las diferencias: rutas de assets y detección de teclado físico (`AConfiguration_getKeyboard`). El port se juega con teclado; sin teclado, el juego se pausa con un aviso.

## Testing
- `ctest` corre `sim_tests` (13 tests, ~1000 comprobaciones).
- CI compila el juego, corre los tests y genera una captura del render bajo Xvfb como artefacto.
