# SCAVENGERS THRIVE — instrucciones para GitHub Copilot

Valen para Copilot Chat, el modo agente y el agente de Copilot que abre PRs. Cline tiene las mismas en
`.clinerules`: el bloque común es idéntico. El detalle (cómo se reparte el trabajo con Claude Code, el
flujo con sus comandos, cómo mirar el juego, lo que ya salió mal y el estado del proyecto) está en
`docs/GUIA_IA.md`: léelo al empezar cada sesión.

<!-- reglas-comunes:inicio -->
<!-- Este bloque es igual en .clinerules y en .github/copilot-instructions.md: se cambia en los dos
     a la vez (tools/ci/guardas.py lo comprueba). El detalle de todo está en docs/GUIA_IA.md. -->

## Cómo se reparte el trabajo
- **Tú (Cline o Copilot, en el Codespace) haces la primera versión** de cada tarea: en tu rama, con sus
  pruebas y su PR.
- **Claude Code la verifica y la termina:** lee el diff, corrige, completa pruebas y documentos, sube la
  versión, fusiona y publica la release.
- Por eso **no** tocas `VERSION`, ni las novedades del `README.md`, ni `docs/ROADMAP.md`; **no** fusionas
  PRs y **no** publicas releases. Al terminar, el PR queda listo con la sección «Para Claude» llena.
- Lee `docs/GUIA_IA.md` al empezar cada sesión. Ante una duda, o si la tarea choca con estas reglas,
  pregunta al usuario antes de seguir. Si la tarea es grande o ambigua, primero un plan corto (archivos,
  pruebas, documentos) y esperar su OK.

## Respetar lo que ya existe
- **Antes de escribir un sistema, búscalo** con `git grep` y extiéndelo. Nunca armes uno paralelo ni uno
  que «reemplace más adelante» al que hay: ya pasó con el campamento desde JSON, el `anim_system`, el
  campamento nodal y `src/entities/`, y todo eso rompió el CI o quedó sin usar.
- **Cambia solo lo que la tarea pide.** No reescribas archivos ni funciones que andan: el diff tiene que
  mostrar tus líneas, no un archivo rehecho. No borres código de otros sin decirlo en el PR.
- **Las pruebas solo crecen:** en `tests/test_main.c` agregas funciones nuevas con su `RUN`. Nunca
  borras, recortas, mueves ni comentas una prueba que existe. Si una falla, el error está en tu cambio.
- **Las herramientas compartidas no se cambian para que algo pase:** `tools/assets/`, `tools/save/`,
  `tools/modelos/`, `tools/ci/`, `.github/workflows/`, `.devcontainer/` y `CMakeLists.txt` (ahí solo
  agregas tu `.c`). Si una te frena, detente y pregunta.
- **Nada de marcadores:** ni archivos vacíos, ni modelos o texturas de mentira, ni carpetas nuevas en la
  raíz o en `src/` sin acordarlo.
- **Lo generado no se edita a mano:** `docs/INVENTARIO.md`, `docs/ANIMACIONES.md`,
  `src/game/save_schema.inc` y el atlas de íconos salen de sus scripts.
- **Lo que escribes dice la verdad:** el PR, los commits y los documentos cuentan solo lo que el diff hace
  y está probado. Nada de carpetas, teclas o sistemas que no existen. Un commit que no subiste no es un
  punto de partida para nadie.

## Git
1. **Una tarea por rama, desde `main` al día:** `git fetch origin` y
   `git switch -c cline/<tarea> origin/main` (Copilot: `copilot/<tarea>`).
2. **Nunca trabajes en `main`, ni en la copia local:** todo commit va en tu rama.
   - Nunca subas a `main`, a `claude/*` ni a ramas ajenas.
   - Nunca `--force`, ni `rebase` de algo ya subido.
   - Nunca `gh pr merge`, ni con `--admin` ni con `--auto`: fusiona Claude, con el CI verde.
3. **Al terminar cada sesión, sube tu rama aunque falte algo:** `git push -u origin cline/<tarea>`. Lo que
   vive solo en el Codespace se pierde. A medias, el PR va como borrador (`gh pr create --draft`).
4. **Antes de cada push:**
   - `git fetch origin && git merge origin/main` (con conflictos, lo de `main` manda);
   - compilar de cero y correr todas las pruebas de abajo;
   - `git diff origin/main...HEAD --stat` lista solo archivos de tu tarea.
5. **El PR** sigue `.github/pull_request_template.md`. Si el CI queda rojo, lo arreglas en la misma rama;
   si no ves la causa, lo dices en el PR y paras.

## Pruebas antes de subir (lo mismo que el CI)
```bash
rm -rf build && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/sim_tests
python3 tools/ci/guardas.py
python3 tools/assets/i18n.py extraer && python3 tools/assets/i18n.py check
python3 tools/assets/inventario.py check && python3 tools/assets/inventario.py doc --verificar
python3 tools/assets/animaciones.py check && python3 tools/assets/animaciones.py doc --verificar
python3 tools/assets/iconos.py check
python3 tools/save/esquema.py check
python3 tools/modelos/modelos.py check && .venv/bin/python tools/modelos/test_modelos.py
cmake -E copy_directory assets build/assets
(cd build && xvfb-run -a ./estepa --probar-partidas ../tests/partidas)
```
- **`tools/ci/guardas.py`** frena lo que ya salió mal: pruebas borradas o recortadas, archivos vacíos,
  texto roto, tipos repetidos, enums que crecen por el medio, archivos sueltos. Sus errores se arreglan,
  no se esquivan.
- **Que compile no alcanza:** saca una captura del lugar que cambió (`docs/GUIA_IA.md`, sección 7) y
  mírala.
- **Cada salida se lee entera:** nunca `| head` ni `| tail` sobre la compilación o las pruebas. Solo la
  carpeta `build/`, de cero: una vieja no prueba nada.
- **Si algo falla, lo dices con el error exacto.** Nunca digas que algo anda sin la salida que lo muestra.

## El código (gcc, MSVC y el NDK de Android)
- **Las carpetas:** `src/sim/` es C puro, sin raylib, con las reglas y sus pruebas. `src/game/`,
  `src/world/` y `src/ui/` usan raylib. Ver `docs/ARQUITECTURA.md`.
- **C11 llano:** sin arreglos de largo variable, sin extensiones de GNU, sin `unistd.h` ni `dirent.h` en
  `src/` y sin `M_PI`.
- **Cero advertencias nuevas.** Los shaders, también en GLSL ES 100, con `precision mediump float;` en el de
  vértices (con `highp`, Mesa no dibuja nada en OpenGL ES) y un respaldo si no compila (ver `src/world/terrain.c`).
- **La memoria:** arreglos fijos (`*_MAX`) y estructuras planas. Nada se reserva ni se carga dentro del
  bucle de cuadros. Sin bibliotecas nuevas.
- **Un `.c` nuevo** va en `CMakeLists.txt`. Un tipo vive en un solo header.
- **Renombrar o quitar algo:** `git grep` todos sus usos y arreglarlos en el mismo commit.
- **raylib:** nunca un `Material` en `{ 0 }` en `DrawMesh` (`LoadMaterialDefault()` una vez), y
  `LoadShaderFromMemory(vs, fs)` con el de vértices primero.

## Los datos
- **Los textos:** todo lo visible pasa por `T("...")` o `N_("...")` y se traduce en `assets/i18n/en.tsv`
  (`i18n.py faltan` lista lo que falta). Documentos, comentarios y mensajes, en español y en UTF-8.
- **Los enums crecen al final,** justo antes de su `_COUNT`: las partidas guardan números.
- **Las partidas:** un campo nuevo de un struct que se guarda empieza en cero. Después se corre
  `python3 tools/save/esquema.py` (ver `docs/PARTIDAS.md`).
- **Inventario e íconos:** los objetos nuevos van en `assets/inventario.tsv` y después `inventario.py doc`.
  Los íconos, con `iconos.py generar`.

## Modelos 3D: TRELLIS.2 + RigNet + GRAIL → el juego (`tools/modelos`)
Cuando el usuario pase la URL de un `.glb`:
1. **Nunca** le pidas que baje o vuelva a subir archivos desde la tablet.
2. **El id:** `python3 tools/modelos/modelos.py estado` (lo pendiente) o `inventario.py faltan <categoría>`.
3. **Traerlo:** `python3 tools/modelos/modelos.py traer <ID> --url "<URL>" --fuente trellis2 --girar 90`.
   Deja el modelo al estilo del juego (low-poly, colores planos, la medida del inventario, el frente a +X
   y el origen en la base). Lo marca `importado` y anota su procedencia en
   `arte/modelos/models_manifest.json`.
4. **Mirarlo:** desde `build/`, `xvfb-run -a ./estepa --ver <ID> --frames 40 --screenshot ver.png`. Si el
   frente no mira a la cámara, `traer` otra vez con otro `--girar` y `--reemplazar`.
5. **Subirlo** como cualquier tarea: el GLB, `assets/inventario.tsv`, `docs/INVENTARIO.md` (generado) y el
   manifiesto, en su rama y con su PR.

Además:
- **La ruta sale del id:** `assets/models/<categoría>/<sub>/<nombre>.glb`. Las categorías son las de
  `tools/assets/inventario.py`: no se inventan otras (como `personajes`, en plural).
- **Personas, animales y mecanismos** llevan esqueleto y los clips obligatorios de `docs/ANIMACIONES.md`.
  El riggeado entra con `traer <ID> --archivo <archivo> --riggeado`.
- **Sin código C** para registrar un modelo o un clip: el motor los carga por nombre (`src/world/props.c`).
- **`python3 tools/modelos/modelos.py check`** tiene que pasar (triángulos, medida, base y texturas).

## Al terminar
- **En el PR,** llena la sección «Para Claude»: qué hiciste, qué falta, cómo lo probaste (con números),
  dónde mirar y tus dudas.
- **Al usuario,** en español y en pocas líneas: lo que cambió, el enlace del PR y lo pendiente. Lo que no
  se pudo, con el error exacto.
<!-- reglas-comunes:fin -->

## Solo para Copilot
- **Tu rama es `copilot/<tarea>`.** El agente de Copilot que abre PRs solo sigue estas mismas reglas: sus PRs
  también los verifica Claude Code antes de fusionar.
- **Si Cline también trabaja en el Codespace,** cada herramienta en su propia rama y su propia tarea: nunca
  los dos sobre la misma.
- **Antes de cerrar la sesión,** commit y push de tu rama: nada queda solo en el Codespace.
- **La línea `Co-authored-by: Copilot`** que agregas a los commits se deja: dice quién hizo qué.

## Las skills (sobre todo, para lo que se ve)
- **Están en `.claude/skills/`,** y Copilot las carga desde ahí: el modo agente de VS Code (con el harness
  Local o el de Copilot) y el agente de Copilot que abre PRs. También lee `.github/skills/` y
  `.agents/skills/`, pero no se copian. Cada una entra sola por su descripción, o con `/` y su nombre
  (`/estetica`), o nombrándola en el pedido («usa la skill estetica»).
- **Para pulir lo que se ve, la skill `estetica`:** el ciclo mirar → cambiar → volver a mirar, y qué
  herramienta usar en cada caso. Con ella van `cad` (text-to-CAD), `modelos` (TRELLIS.2) y `animaciones`.
- **El agente `pulir`** (`.github/agents/pulir.agent.md`, en el selector de agentes del chat) trae
  cargadas `estepa` y `estetica` y sigue ese ciclo hasta el PR. Cuando tocas modelos, texturas, íconos o
  cómo se dibuja el mundo, entra solo `.github/instructions/arte.instructions.md`.
- **Los agentes `gamedev-*` y `/gamedev` son de Claude Code:** VS Code puede mostrarlos en el selector,
  porque lee `.claude/agents/`, pero no los uses.
- **El entorno lo deja `.devcontainer/setup.sh`:** text-to-CAD en `.venv-cad` con Chromium, ImageMagick, y
  Node para Kiln. Si falta text-to-CAD: `python3 tools/assets/cad.py instalar`.
