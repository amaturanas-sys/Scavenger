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
    champion.*   grandes guerreros: azar, historia, dones
    clock.*      reloj de juego: dias de 30 min, estaciones, luz y noche
    climate.*    clima: temperatura, tiempo, nieve, lagos, glaciares
    hazards.*    frio, barro, hielo, socavones, desierto, minijuego (QTE)
    health.*     vida, sangre y heridas (jugador, tribu, enemigos)
    combat.*     armas, enemigos (y jinetes), reglas de cada golpe, botin, inercia montada
    body.*       cuerpo humano articulado por zonas (dibujo e impactos)
    armor.*      armadura por piezas: materiales, cobertura, durabilidad
    ballistics.* armas a distancia y trayectoria de proyectiles
    inventory.*  inventario de assets (lee assets/inventario.tsv)
    actions.*    manos y empunadura, acciones individuales, construcciones en grupo
    economy.*    acopio, comida y recoleccion diaria, efectos del campamento, forja, fabricar a mano, reparar
    animals.*    fauna: clases, relaciones (manada, acecho, rivalidad, huida), doma, montura, ganado, anfibios
    swarms.*     enjambres y bancos: peces, abejas, avispas, mosquitos, moscas
    fire.*       fuego que se propaga y se apaga, incendios, rayos, mantenimiento de estructuras
    melee.*      cuerpo a cuerpo: combos, pesado, patadas, escudo, agarre, gancho, parry, cambiar de mano
    keymap.*     el mapa de teclas: cada accion con su tecla y sus modificadores (H J K L, F, X...)
    targeting.*  el selector de objetivo: el siguiente alrededor del jugador, se pierde a 25 m
    stealth.*    la espalda (cono trasero, lo que ve un enemigo segun de donde vienes), el rehen que tapa el tiro y los abatidos
    storage.*    contenedores: bolsillos, mochila, alforjas, carreta, acopio; peso y estado de las piezas
    loadout.*    amuletos (y tatuajes) con sus efectos
    anim_index.* que clip de animacion corresponde a cada estado
    save_format.* partida por bloques con esquema: escribir, leer y convertir campo por campo
  world/    Mundo (raylib)
    terrain.*    chunks con streaming, altura consultable
    camp.*       campamento, props
    gallery.*    galeria del inventario (--galeria)
    sky.*        cielo, tinte de noche, estrellas y brillo de los fuegos
    weather.*    lluvia, nieve, ventisca, relampagos y bruma
    body_draw.*  dibuja el cuerpo articulado con su armadura
    props.*      objetos sueltos en el mundo (modelo o marcador)
  game/     Jugador e input
    player.*
    actions_game.*  menu de acciones, atajos, obras en curso
    hazards_game.*  peligros en el juego: calor corporal, hielo, socavones, escolta, rescates
    combat_game.*   combate (a pie y montado), enemigos, botin, vendajes, salud de la tribu y del jugador
    fauna_game.*    fauna en el mundo: aparicion por bioma, ataques, K (comer, despiezar, ordeñar)
    disasters_game.* fuego, rayos, lluvia torrencial, fogatas apagadas por la lluvia
    save_game.*     partidas guardadas: tres huecos con minifoto y fecha, por bloques (docs/PARTIDAS.md)
    save_schema.inc esquema de lo que se guarda (generado por tools/save/esquema.py)
    save_v1.inc     esquema congelado de las partidas planas (hasta la v0.4.3)
    save_check.c    resumen del estado y partidas de referencia (--probar-partidas)
    title_menu.*    menu de entrada y de pausa, huecos, instructivo (arte en assets/ui/)
    inventory_game.* inventario (I) y equipo (P): contenedores a mano, armadura, amuletos, reparar, bolsas de botin
  ui/       Interfaz (raylib)
    theme.*      tema de orfebreria: paneles, barras, texto
    minimap.*    minimapa circular tipo brujula
  main.c    Bucle principal, render low-res, HUD
tests/      Tests del nucleo (arnes propio, sin dependencias)
assets/     Assets de runtime: inventario.tsv, animaciones.tsv, models/<categoria>/<sub>/<nombre>.glb, textures/
tools/      Herramientas de produccion (programas Kiln, validadores, text-to-cad)
.claude/skills/cad/  Skill CAD de text-to-cad (MIT, earthtojake/text-to-cad)
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
