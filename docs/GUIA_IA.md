# Guía para trabajar con IA en SCAVENGERS THRIVE

Para Cline y Copilot en el Codespace, y para Claude Code. Dice cómo se reparte el trabajo entre ellos, el
flujo de cada cambio con sus comandos, las reglas que no se rompen, las guardas que las hacen cumplir, lo
que ya salió mal y el estado del proyecto.

- **`.clinerules`** (Cline) y **`.github/copilot-instructions.md`** (Copilot) tienen las reglas en corto.
  Cada herramienta carga las suyas en cada mensaje, y las dos comparten un bloque idéntico.
- **Acá está el detalle.**
- **Al final**, el prompt maestro para empezar una sesión y las plantillas para pedir cada tarea y su
  verificación.

## 1. El proyecto en un minuto

- **El juego:** *SCAVENGERS THRIVE — THEY COME FROM THE STEPPES* (nombre interno `estepa`), en C11 con
  raylib 5.5, a 640 × 360. Corre en Windows y en Android (APK); se desarrolla en Linux.
- **Dónde va:**
  - la versión está en `VERSION`;
  - lo hecho y lo pendiente, en [ROADMAP.md](ROADMAP.md);
  - el plan vigente, en [PLAN_HERRAMIENTAS_IA.md](PLAN_HERRAMIENTAS_IA.md);
  - las novedades de cada versión, en el `README.md`.
- **Las carpetas:**
  - `src/sim/`: C puro, sin raylib. Todo lo que tiene reglas va acá y se prueba en `tests/test_main.c`.
  - `src/game/`: el juego (une la simulación con raylib: entrada, dibujo, diálogos).
  - `src/world/`: el dibujo del mundo (terreno, voxeles, vegetación, cuerpos, objetos sueltos).
  - `src/ui/`: la interfaz.
  - `src/entities/`: un andamiaje sin conectar (ver [ARQUITECTURA.md](ARQUITECTURA.md)).
  - `assets/`: el inventario (`inventario.tsv`), las traducciones (`i18n/en.tsv`), los modelos, los íconos.
  - `tools/`: los scripts de assets, partidas, modelos, Android, Windows, capturas y las guardas del CI
    (`tools/ci/guardas.py`).
  - `tests/`: las pruebas del núcleo y las partidas de referencia (`tests/partidas/`).
  - `docs/`: la documentación, siempre en español.
- **Los documentos:**

| Documento | Qué tiene |
|---|---|
| `README.md` | Compilar, controles, el Codespace y las novedades de cada versión. |
| [GDD.md](GDD.md) | El diseño y las reglas de cada sistema, con los nombres de sus funciones. |
| [ARQUITECTURA.md](ARQUITECTURA.md) | Los módulos de cada carpeta y qué hace cada uno. |
| [MUNDO.md](MUNDO.md) | La generación del mundo, el terreno, la vegetación, el cielo. |
| [AGUA.md](AGUA.md) | El diseño del agua del usuario, página por página, y sus cinco fases: lo hecho y lo que sigue. |
| [PARTIDAS.md](PARTIDAS.md) | El formato de las partidas y cómo cambiar un struct que se guarda. |
| [ARQUITECTURA_EDAD_DEL_HIERRO.md](ARQUITECTURA_EDAD_DEL_HIERRO.md) | El kit de edificios: casas, palacios, murallas, ruinas, obras. |
| [INVENTARIO.md](INVENTARIO.md), [ANIMACIONES.md](ANIMACIONES.md) | Generados por sus scripts: no se editan a mano. |
| [PLAN_HERRAMIENTAS_IA.md](PLAN_HERRAMIENTAS_IA.md), [ROADMAP.md](ROADMAP.md) | El plan, su estado y lo que falta. |
| [CAPTURAS.md](CAPTURAS.md) | Las capturas de cada pantalla y cómo se sacan. |
| [NARRATIVA.md](NARRATIVA.md), [ESTILO_VISUAL.md](ESTILO_VISUAL.md) | La historia y el estilo. |
| `.claude/skills/estepa`, `modelos`, `animaciones` | Las convenciones y las tuberías de modelos y animaciones (sirven de guía para todas las IA). |

## 2. Cómo se reparte el trabajo

El Codespace hace la primera versión de cada cosa; Claude Code la verifica, la corrige y la publica. Así
cada release sale revisada por quien conoce todo el proyecto, y nada se fusiona sin esa revisión.

| Paso | Quién | Qué hace |
|---|---|---|
| 1. Pedir | El usuario | Le pasa la tarea a Cline o a Copilot con la plantilla de la sección 12. |
| 2. Implementar | Cline o Copilot, en el Codespace | Rama `cline/<tarea>` o `copilot/<tarea>`: el código, sus pruebas nuevas y el documento del sistema. Abre el PR con la sección «Para Claude» llena. No toca `VERSION`, ni las novedades del `README.md`, ni el `ROADMAP`. No fusiona. |
| 3. Verificar | Claude Code | Lee el diff entero, corre todo, mira capturas, corrige lo que esté mal y completa pruebas y documentos (sección 4). |
| 4. Publicar | Claude Code | Sube la versión, abre su PR con todo, lo fusiona con el CI verde, cierra el PR original y publica la release con el ZIP y el APK. |
| 5. Informar | Claude Code | Le cuenta al usuario qué encontró, qué corrigió y los enlaces. |

- **Por qué Claude abre su propio PR:** solo puede subir a su rama (`claude/…`). Toma la rama del
  Codespace tal cual (con su historia y los `Co-authored-by`), le suma sus correcciones y abre el PR
  desde ahí. El PR original se cierra con el enlace al nuevo.
- **Cada error nuevo que encuentre la verificación** se vuelve una regla (en el bloque común) o una
  guarda (en `tools/ci/guardas.py`), para que no se repita.
- **Lo que Claude haga solo** (una fase grande, un arreglo) sigue el mismo flujo, con su PR.

## 3. El flujo en el Codespace

Cada paso evita un error que ya pasó (ver la sección 8).

1. **Entender antes de tocar:**
   - Leer el documento del sistema y su código. Buscar con `git grep` si ya existe algo que lo haga: se
     extiende lo que hay; no se arma un sistema paralelo.
   - Si la tarea es grande o ambigua, proponer al usuario un plan corto (archivos, pruebas, documentos) y
     esperar su OK.
2. **Rama nueva desde el `main` al día** (nunca en `main`, tampoco en la copia local):
   ```bash
   git fetch origin
   git switch -c cline/<tarea> origin/main    # Copilot: copilot/<tarea>
   ```
3. **Implementar:**
   - Las reglas, en `src/sim/`, con sus pruebas nuevas al final de `tests/test_main.c` (una función
     `test_…` y su `RUN`).
   - El dibujo y la entrada, en `src/game/` o `src/world/`.
   - Los textos visibles, con `T()` o `N_()`.
   - Un `.c` nuevo, en `CMakeLists.txt`.
   - Solo lo que la tarea pide: el diff muestra tus líneas, no archivos rehechos.
4. **Probar todo** (lo mismo que corre el CI):
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
   - Si `i18n.py check` falla, falta traducir: `python3 tools/assets/i18n.py faltan` los lista y se
     traducen en `assets/i18n/en.tsv`.
   - Si cambió un struct que se guarda: `python3 tools/save/esquema.py` (ver PARTIDAS.md).
   - Sin `.venv` (un contenedor nuevo), la prueba de los modelos la pide una vez:
     `python3 -m venv .venv && .venv/bin/pip install -r tools/modelos/requirements.txt`.
   - Cada salida se lee entera, sin `| head` ni `| tail`.
5. **Mirar el juego** (sección 7): una captura del lugar que cambió, y abrir la imagen. Un juego que
   compila igual puede caerse en el primer cuadro.
6. **Documentar** el sistema que cambió (GDD, MUNDO, ARQUITECTURA…) y, si cambia lo que se ve, las
   capturas (`bash tools/capturas.sh build`). La versión y las novedades las escribe Claude.
7. **Antes de subir, traer `main` y repetir el paso 4:**
   ```bash
   git fetch origin && git merge origin/main   # con conflictos, lo de main manda
   ```
   `git diff origin/main...HEAD --stat` tiene que listar solo los archivos de la tarea.
8. **Commit, push y PR:**
   ```bash
   git add <los archivos de la tarea> && git commit -m "Lo que cambia: en una línea, en español"
   git push -u origin cline/<tarea>
   gh pr create --base main --title "..." --body-file <el cuerpo con la plantilla>   # --draft si falta algo
   ```
   - El mensaje y el PR dicen lo que hace el diff de verdad, con los números.
   - La sección «Para Claude» de la plantilla, llena: qué está hecho, qué falta, dónde mirar.
   - Al terminar la sesión, la rama siempre subida, aunque falte (como borrador).
9. **El CI:** se mira una vez con `gh pr checks <número>`; si queda rojo, se arregla en la misma rama.
   Nunca `gh pr merge`: fusiona Claude.
10. **Informar al usuario** (sección 10), con el enlace del PR.

### Las skills en el Codespace (para pulir lo que se ve)

Las skills del proyecto están en `.claude/skills/`. Se usan en el Codespace para refinar lo estético sin
gastar a Claude, y el flujo de arriba no cambia: rama, PR, y Claude verifica y fusiona.

| Quién | Cómo las usa |
|---|---|
| Copilot (modo agente de VS Code, con el harness Local o el de Copilot, y el agente que abre PRs) | Las carga por su cuenta desde `.claude/skills/` (también lee `.github/skills/` y `.agents/skills/`). Entra la que corresponde por su descripción, la que se pide con `/` (`/estetica`) o la que se nombra en el pedido. |
| Claude Code | Las carga por su cuenta. |
| Cline | Se le pide que lea el `SKILL.md` de la que haga falta. |

- **Para lo que se ve, la skill `estetica`:**
  - el ciclo mirar → cambiar → volver a mirar, con el antes y el después lado a lado;
  - qué herramienta va para cada cosa: text-to-CAD (skill `cad`), la tubería de modelos (`modelos`), Kiln,
    Nomad, las animaciones (`animaciones`), los íconos, las texturas, las nubes y el logo.
- **El entorno lo deja `.devcontainer/setup.sh`:**
  - text-to-CAD en `.venv-cad`, con Chromium y sus bibliotecas para las fotos;
  - ImageMagick (`montage`);
  - Node 22 para Kiln, que viene de `devcontainer.json`.

  `devcontainer.json` también instala la extensión de Copilot Chat y activa `chat.useAgentSkills`, que hace
  falta en las versiones de VS Code donde las skills todavía son experimentales. Un Codespace creado antes de
  este cambio lo toma con *Codespaces: Rebuild Container*. Si solo falta text-to-CAD, alcanza con
  `python3 tools/assets/cad.py instalar`.
- **El agente `pulir`** (`.github/agents/pulir.agent.md`): se elige en el selector de agentes del chat de
  Copilot. Trae cargadas `estepa` y `estetica` y sigue el ciclo hasta el PR, sin tocar `VERSION` ni fusionar.
  Cuando Copilot toca modelos, texturas, íconos o cómo se dibuja el mundo, entra solo
  `.github/instructions/arte.instructions.md`, con las medidas y lo que no se hace.
- **Un pedido de ejemplo:** «Usa la skill estetica: saca fotos de `estructura.vivienda.yurta_comun` con
  `cad.py foto` y en el juego con `--ver`, compárala con su medida del inventario y con el estilo de
  `assets/models/README.md`, y propón qué pulir antes de tocar nada».
- **Lo que solo existe en Claude Code** (los agentes `gamedev-*`, `/gamedev`) no está para Copilot ni para
  Cline. VS Code puede mostrar esos agentes en su selector, porque lee `.claude/agents/`: no se usan.

## 4. La verificación de Claude Code

Lo que hace Claude con cada PR del Codespace, antes de publicar:

1. **Traer la rama** y crear la suya encima: `git fetch origin <rama>` y
   `git switch -c claude/<…> FETCH_HEAD`.
2. **Leer el diff entero** contra `main`, no solo el resumen del PR. Mirar sobre todo:
   - que haga lo que el PR dice, y nada que no diga (archivos rehechos, código ajeno borrado);
   - que no arme un sistema paralelo, ni deje andamiajes sin conectar ni marcadores;
   - las reglas de la sección 5: la arquitectura, los enums, las partidas, `T()`, la memoria por cuadro;
   - que cada regla nueva tenga su prueba y que ninguna prueba se haya aflojado;
   - que los documentos digan la verdad (teclas, carpetas, números).
3. **Correr todo de cero** (sección 3, paso 4), las guardas y las partidas de referencia, y **mirar** las
   capturas del lugar que cambió (también de noche, en invierno o donde la regla lo pida).
4. **Corregir** en su rama lo que esté mal y completar lo que falte: pruebas, documentos, traducciones.
5. **Versión y novedades:** `VERSION`, la sección «… (vX.Y.Z)» del `README.md`, la línea del
   `docs/ROADMAP.md` y la sección 9 de esta guía.
6. **PR propio, CI verde, squash merge** y cerrar el PR original con el enlace al nuevo.
7. **La release:** `gh workflow run release.yml --ref main` (o Actions → Release → Run workflow), y
   comprobar que tenga el ZIP de Windows y el APK.
8. **Informar** al usuario: qué encontró, qué corrigió, los enlaces y lo pendiente.

## 5. Reglas que no se rompen

**Respetar lo que ya existe:**
- Antes de escribir un sistema se busca con `git grep` y se extiende. Nunca uno paralelo ni uno que
  «reemplace más adelante» al que hay.
- Se cambia solo lo que la tarea pide; no se reescriben archivos ni funciones que andan.
- Las pruebas solo crecen: se agregan, nunca se borran, recortan, mueven ni comentan. Si un sistema entero
  se quita, sus pruebas van a `tests/retiradas.txt` con el motivo, y lo aprueba Claude.
- Las herramientas compartidas (`tools/assets/`, `tools/save/`, `tools/modelos/`, `tools/ci/`, el CI,
  `.devcontainer/`, `CMakeLists.txt` salvo para agregar un `.c`) no se cambian para que algo pase.
- Nada de marcadores: ni archivos vacíos, ni modelos de mentira, ni carpetas nuevas sin acordarlas.
- Los documentos, el PR y los commits dicen solo lo que el diff hace y está probado.

**El código** (CI compila con gcc en Linux, MSVC en Windows y el NDK en Android):
- **C11 llano:**
  - sin arreglos de largo variable (MSVC no los tiene);
  - sin extensiones de GNU (`typeof`, `__attribute__`, sentencias como expresión);
  - sin `unistd.h` ni `dirent.h` en `src/`;
  - sin `M_PI` (usar las constantes `PI` del código).
- **Cero advertencias nuevas** (`-Wall -Wextra -Wpedantic`, `/W4`).
- **Los shaders** también en GLSL ES 100, con `precision mediump float;` en el de vértices (como el de raylib:
  con `highp`, llvmpipe de Mesa no dibuja nada en OpenGL ES) y un respaldo si no compila. El CI los compila y
  dibuja también con OpenGL ES 2.0 (`-DOPENGL_VERSION="ES 2.0"`; ver el del agua, `src/world/terrain.c`).
- **La memoria:** arreglos fijos (`*_MAX`) y estructuras planas; nada se reserva por cuadro y no hay ECS
  genérico.
- **Sin bibliotecas externas nuevas** (cJSON ni ninguna): la compilación solo baja raylib. Si hace falta
  una, se pregunta.
- **raylib:**
  - nunca un `Material` hecho con `{ 0 }` en `DrawMesh` (se cae en el primer cuadro);
  - `LoadShaderFromMemory(vs, fs)` lleva el de vértices primero;
  - nada se carga dentro del bucle de cuadros.
- **Un tipo vive en un solo header.** Antes de renombrar o quitar algo (una función, un campo, un
  archivo): `git grep` todos sus usos y arreglarlos en el mismo commit.

**Los datos:**
- **Los textos:** todo lo visible pasa por `T("...")` o `N_("...")`, y se traduce en `assets/i18n/en.tsv`.
  Todo lo escrito, en español y en UTF-8.
- **Los enums crecen al final** (justo antes de su `_COUNT`): se guardan como números.
- **Las partidas:**
  - al cambiar un struct que se guarda: `python3 tools/save/esquema.py`;
  - un campo nuevo empieza en cero en las partidas viejas; si no puede, se sube la versión del bloque y
    se migra en `migrate()`;
  - un arreglo puede crecer; para achicarlo hace falta migrar. Ver PARTIDAS.md.
- **El inventario:** los objetos nuevos van en `assets/inventario.tsv` (después, `inventario.py doc`); los
  íconos se generan con `iconos.py generar`. Las categorías son las que ya existen.
- **Los modelos 3D** entran solo con `tools/modelos/modelos.py traer`, nunca a mano: cada id tiene una
  ruta fija.
- **Los generados no se editan a mano:** `src/game/save_schema.inc`, `docs/INVENTARIO.md`,
  `docs/ANIMACIONES.md`, el atlas de íconos.

**Git:**
- Ramas `cline/<tarea>` o `copilot/<tarea>` desde `origin/main`, una tarea por rama, nunca en `main`.
- Nunca se sube a `main`, a `claude/*` ni a la rama de otro. Nunca `--force`, ni `rebase` de algo ya
  subido. Si el push es rechazado: `git fetch` y `git merge origin/main`.
- Al terminar la sesión, la rama se sube aunque falte algo.
- Solo Claude fusiona (squash, borrando la rama) y publica. Nunca `--admin` ni `--auto`.

**Las versiones** (las sube Claude al verificar): `X.Y.Z`.
- Sube la Z en cada PR que cambia el juego (código, assets, reglas); la Y, en una fase o un sistema grande.
- Lo que solo toca documentos o configuración no sube la versión ni publica.

## 6. Las guardas automáticas

`tools/ci/guardas.py` corre en el CI de cada PR (en el paso de Linux, que es obligatorio para fusionar) y
antes de cada push en el Codespace (`.githooks/pre-push`). Cada guarda nació de un error de la sección 8:

| Guarda | Frena | El error que la originó |
|---|---|---|
| `lugar`, `basura` | Archivos sueltos en la raíz, carpetas nuevas en la raíz o en `src/`, registros, caches de Python. | `ASSETS_MANIFEST.md`, `build_log.txt`, `campamento.json`. |
| `vacio`, `formato` | Archivos vacíos en `assets/`, `arte/` o `android/`; un `.glb` o `.png` que no lo es. | 82 GLB y PNG de 0 bytes presentados como modelos. |
| `texto` | Texto roto por la codificación. | El «×» de «9 slots × 6» escrito como los bytes `\xc2\x97`. |
| `cmake` | Un `.c` de `src/` que no se compila. | `vat_example.c`. |
| `tipos` | El mismo tipo o enumerador en dos headers. | `ChannelState`; `BoneIndex` en `src/entities/`. |
| `reglas` | Que el bloque común de Cline y de Copilot no sea idéntico. | — |
| `pruebas` | Pruebas borradas o recortadas, un `RUN` repetido, quitado o sin su función. | Mil líneas menos en `tests/test_main.c`. |
| `enums` | Un enum que crece por el medio o pierde valores. | `KA_TARGET_LEFT` antes de `KA_MARK`. |
| `version` | `VERSION` sin su sección en el `README.md` o el `ROADMAP`, o sin cambios del juego. | La v0.10.6, solo de documentos; la v0.10.8, sin novedades. |

- **A mano:** `python3 tools/ci/guardas.py` compara con el punto donde la rama salió de `origin/main` y mira
  también lo que aún no se commiteó.
- **Los errores** se arreglan; nunca se esquiva una guarda. Los avisos (herramientas tocadas, reglas sin
  prueba, una prueba sin `RUN`) no frenan, pero Claude los revisa.
- **El pre-push** también impide subir a `main` o a `claude/*`. En el Codespace lo activa
  `.devcontainer/setup.sh`; en uno anterior, una vez: `git config core.hooksPath .githooks`.

## 7. Cómo mirar el juego sin pantalla

Desde `build/` (antes, `cmake -E copy_directory ../assets assets`), con `xvfb-run -a` delante:

| Para ver | Comando |
|---|---|
| El arranque | `./estepa --frames 60 --screenshot shot.png` |
| Un lugar del mundo | `--ir estepa`, `bosque`, `altiplano`, `desierto`, `mar`, `capital`, `aldea`, `fortaleza`, `ciudad`, `plaza-<región>`… (con `--dia 11 --minuto 7` es verano al mediodía; `--camara`, `--dron 40` desde arriba; `--vista cerca\|media\|lejos`) |
| El agua | `--ir lago`, `desborde`, `cascada`, `meandro`, `trenzado`, `arroyo`, `rapidos` |
| El kit de edificios | `--edificios casa` (o `palacio`, `muralla`, `puerta`, `fortaleza`, `refugio`… ; `:región` para una sola fila) |
| Un objeto de cerca | `--ver <id del inventario>` |
| El asedio | `--asedio puerta`, `fuego`, `salida`, `quemada`, `tomada`… |
| Combate | `--enemigos bandidos`, `culto`, `lobos`, `arqueros` |
| El sistema táctico | `--manos escudo` (`espada`, `nada`), `--rehen`, `--objetivo`, `--cubierto`, `--inventario` (la fila de la barra) |
| Las cuevas | `--ir boca-forajidos` (`animal`, `abandonada`, `tribu`, `imperio`): la boca, afuera (`boca-<tipo>-ladera`: la de ese tipo con más desnivel; con `-lado` al final, desde un costado); `--cueva forajidos:0 --antorcha`: adentro (lugar: una sala o `boca`, `pasilloN`, `celda`, `dialogo`, `nido`, `cofre`, `fogata`, `estalagmita`, `huesos`, `roca`…); `--cueva-f` pulsa F; `--explorar`: F en la boca más cercana (carga la cueva); `--caminar adelante` (o `atras`). La línea `CUEVA` de la consola dice lo que pesa cada cueva cargada |
| La galería de assets | `--galeria` |
| El mapa del mundo | `--semilla 42 --mapa-mundo mapa.png` |
| Las partidas de referencia | `--probar-partidas ../tests/partidas` |

- **Más opciones:** la lista completa está arriba de `src/main.c` y en el README («Captura automática»).
- **Las capturas de la documentación:** `bash tools/capturas.sh build` las rehace todas (unos 4
  minutos); se suben las que cambiaron.

### Si Cline no puede correr comandos

Si los comandos fallan con «Command execution aborted», no muestran salida o aparece «Shell Integration
Unavailable», la falla está en cómo Cline usa la terminal de VS Code, no en el proyecto. Repetir el
comando no sirve: Cline corta la tarea a las 6 llamadas iguales. Se arregla así, en este orden:
1. **En Cline:** el engranaje (Settings) → *Terminal Settings* → *Terminal Execution Mode* en
   **Background Exec**. Los comandos corren en un proceso aparte, sin la integración de la terminal. Es el
   arreglo que la documentación de Cline propone primero.
2. **Si sigue,** en las mismas opciones:
   - *Default Terminal Profile* en **bash** (el Codespace ya la trae por defecto desde
     `.devcontainer/devcontainer.json`);
   - *Shell integration timeout* en **10** segundos;
   - *Enable aggressive terminal reuse* apagado.
3. **Recargar la ventana:** `Ctrl+Shift+P` → *Developer: Reload Window*.
4. **Si tampoco anda a mano en una terminal nueva** (`` Ctrl+` ``): el Codespace está mal. Primero
   *Codespaces: Rebuild Container*; si no alcanza, crear uno nuevo desde `main`.

Mientras tanto, Cline puede leer archivos y ver si existen con sus herramientas de leer y listar
archivos, sin la terminal. Los comandos, de a uno: cadenas largas con `&&` son lo primero que se
traba.

## 8. Lo que ya salió mal (y no tiene que repetirse)

- **PR #114 y #115: el CI quedó rojo y el juego se caía.**
  - Qué pasó:
    - las ramas estaban atrasadas respecto de `main`;
    - se agregaron cJSON y un `anim_system` (una biblioteca y un sistema paralelos a los que había);
    - la vegetación se caía en el primer cuadro (`Material` en cero);
    - había modelos fuera de su ruta.
  - Se rescató lo bueno y se reescribió (v0.9.1, v0.9.2).
  - Reglas que salieron de ahí: ramas desde el `main` al día, compilar de cero, sin bibliotecas
    nuevas, extender lo que existe.
- **v0.10.3: el repositorio quedó dividido en tres ramas.**
  - Un push desde un Codespace viejo reemplazó la rama de Claude con el código de la v0.9.0.
  - Se unió sin perder historia.
  - Reglas que salieron de ahí: nunca `--force`, nunca a ramas ajenas, el Codespace se rehace desde
    `main` en vez de remendarlo, y el pre-push que impide subir a `main` o a `claude/*`.
- **Un commit que decía otra cosa que su diff.** «Increase population and vegetation density»
  anunciaba «más casas y campos por persona», pero sus números hacían lo contrario. Antes de escribir el
  mensaje se verifican los números.
- **Números cambiados sin sus pruebas ni sus documentos.** Cambiar una regla obliga a actualizar su
  prueba y el GDD en el mismo PR.
- **v0.10.5 a v0.10.8: cinco versiones sin verificar** (lo encontró la revisión de la v0.10.9):
  - el cambio de objetivo quedó en Q/E solas, que también giran la cámara: en combate, cada Q o E hacía
    las dos cosas (el README decía Ctrl+Q y Ctrl+E);
  - unas líneas pisaban la pose suave del parry, rompían las del desvío y el gancho, y hacían que los
    guerreros de la tribu levantaran un parry en medio de su golpe;
  - campos que nadie leía, `KA_TARGET_*` metidos en medio de su enum y declaraciones repetidas en
    `city.h`;
  - la v0.10.6 subió la versión solo con documentos, y la v0.10.7 anunció carpetas
    (`assets/referencia/`, `assets/videos/`) que no existían;
  - `src/entities/` (v0.10.8) quedó sin conectar, con dos advertencias y sus tipos repetidos en dos
    headers, y sus novedades llegaron en otro PR que editó a mano `docs/INVENTARIO.md` (CI rojo);
  - ninguna de esas versiones tuvo pruebas nuevas ni release.
- **v0.10.9: el Codespace se rompió con tres tareas mezcladas en `main`.**
  - Qué pasó:
    - el VAT (de Copilot), un campamento nodal y el comienzo del agua quedaron en el `main` local, sin
      rama y sin subir;
    - `tests/test_main.c` perdió unas mil líneas (siete pruebas borradas, otra recortada, `RUN`
      repetidos) y no enlazaba;
    - `ChannelState` estaba en dos headers;
    - 82 GLB y PNG vacíos en `assets/models/personajes/` (una categoría inventada, en plural);
    - `inventario.py` cambiado para aceptar esas rutas (rompía la de cada objeto con `_` en el nombre);
    - un `.vscode/settings.json` cortado a la mitad, `build/` y `build-vscode/` mezclados, y salidas de la
      compilación pasadas por `head`;
    - un reporte que llamó «punto estable» a un commit que nunca se había subido ni pasado el CI.
  - Se rescató todo en la rama `cline/rescate-codespace` (queda de archivo, sin fusionar) y `main` no se
    tocó.
  - Lo que salió de ahí: el reparto de la sección 2 (el Codespace implementa y Claude verifica), las
    guardas de la sección 6, subir la rama al terminar cada sesión, las pruebas que solo crecen y las
    herramientas compartidas que no se cambian para que algo pase.
- **v0.13.0: seis PR apilados (#139 a #144) que decían «Hecho» de lo que eran números.**
  - Qué pasó:
    - las cuevas (B1 a B3) declaraban restos, prisioneros, cofres y riesgo de traición que el jugador nunca
      veía ni usaba; el interior se dibujaba afuera, sobre el terreno, y no se recorría; el anillo de
      recámaras se armaba alrededor de la boca y no había salida (la única unión era la bajada sin vuelta);
    - la cobertura (A4) volvía a cubrir en el sitio viejo después de disparar lejos de él, y solo contaba
      objetos puestos, no los árboles y las rocas del mundo;
    - el objetivo (A3) elegía también a los animales de la tribu y se perdía al instante sin línea de
      visión; los 5 / 7 / 3 s eran iguales a 1 m que a 20 m;
    - la barra (A2) llenaba el odre al instante, saltándose la acción, y la columna «Beber» llenaba en vez
      de beber;
    - el plan los daba por hechos («el gancho sube como una tirolesa»: no existía).
  - Se revisaron y corrigieron en la rama del #138, con pruebas y capturas, y salieron juntos.
  - Lo que salió de ahí: «Hecho» quiere decir que se ve o se usa en el juego, con su captura y su prueba;
    un dato que nadie lee no se declara.

## 9. Estado actual y qué sigue

*(Lo actualiza Claude al publicar cada versión: la versión, lo último y lo que sigue.)*

- **Versión:** 0.14.0. Las cuevas grandes, la base ([PLAN_CUEVAS_GRANDES.md](PLAN_CUEVAS_GRANDES.md)): salas
  y Cisterna más grandes y altas (el techo a más de tres personas), pasillos largos y con vueltas; junto a la boca,
  F explora y la cueva se carga aparte; la roca, en mallas con la luz por vértice solo en lo cercano; las
  estalactitas, las estalagmitas y las rocas, de voxeles. Además: los controles de combate nuevos (#154 y #157);
  los voxeles desde las hojas del usuario (`tools/voxeles`) y las estructuras armadas a mano con su hoja; las
  personas, los animales y las armas al estilo PS2 (#161); los asentamientos antiguos (`src/sim/ancient.c`); y la
  cámara cerca, detrás y a la altura de la cabeza (`N` cambia de vista, `src/sim/camview.c`), con `F` para las cosas,
  `Enter` para hablar y comerciar, y sin la columna de acciones del HUD. Las partidas viejas siguen valiendo.
- **Hecho:**
  - el plan grande (fases 0 a 8b);
  - las tuberías de modelos y animaciones (v0.9.3);
  - la parte B, los edificios: el kit, las murallas con la puerta acodada, las ruinas y las obras (v0.10.0 a
    v0.10.2);
  - las casas y los campos por persona (v0.10.4 y v0.10.7: 110 y 45 almas por casa, 180 y 40 por campo);
  - la verificación de la v0.10.9: el reparto Codespace → Claude, las reglas comunes y las guardas;
  - el agua, las cinco fases (v0.11.0 a v0.11.4);
  - el sistema táctico y las cuevas: de A1 a B3 (v0.13.0).
  - las bocas de las cuevas, de voxeles, con lo de cada tipo (v0.13.1);
  - las cuevas grandes: la base; los voxeles desde las hojas; los modelos al estilo PS2; los asentamientos
    antiguos; la cámara cerca con `F` y `Enter` (v0.14.0).
- **En archivo, sin fusionar** (rama `cline/rescate-codespace`):
  - el VAT de Copilot: un renderer y sus shaders sin conectar, sin herramienta que hornee las texturas y
    con modelos vacíos;
  - el campamento nodal: descartado por el usuario (duplicaba el campamento);
  - los cauces por estación: los rehizo la fase 2 del agua (v0.11.1).
- **Lo que sigue:**
  - **Las tareas de las cuevas grandes para Copilot** ([PLAN_CUEVAS_GRANDES.md](PLAN_CUEVAS_GRANDES.md), de C1
    a C16, en orden): los voxeles de lo de adentro (racimos, columnas, cofres, rocas, fuentes, el lago) y lo
    demás según las referencias (huesos, antorchas, pinturas, celdas, túneles labrados, la boca vista desde
    adentro), el rendimiento y las capturas. Hitos: v0.14.1 (C4), v0.14.2 (C10) y v0.14.3 (C16).
  - **Lo que queda de las hojas** ([PLAN_TACTICO_Y_CUEVAS.md](PLAN_TACTICO_Y_CUEVAS.md)): las bestias
    terribles (el tipo B de cueva), la comida desde la barra (cuando el jugador tenga hambre) y el gancho en
    las torres con la cuerda como tirolesa.
  - **Lo que espera material del usuario** (PLAN_HERRAMIENTAS_IA.md): las imágenes de la D.2 y los
    cuerpos riggeados y los videos de la C.1.

## 10. Cómo informar al usuario

- Siempre en español y en pocas líneas.
- Qué cambió, cómo se probó (con números: comprobaciones, caminatas, capturas), el enlace del PR y, si hubo
  versión, el de la release. Lo pendiente o lo que hay que decidir.
- Si algo no se pudo hacer, decirlo claro, con el error y lo que hace falta.
- **Antes de algo que no se deshace** (borrar ramas o archivos del usuario, reescribir historia,
  publicar), preguntar.
- **Los resúmenes de cada versión** van en el `README.md` y en el `ROADMAP`, no en esta guía.

## 11. El prompt maestro

Para la primera sesión de Cline o de Copilot en un Codespace nuevo (o cuando haya que retomar todo). Se
copia entero:

```text
Vas a continuar el desarrollo de SCAVENGERS THRIVE (C11 + raylib, Windows y Android) en este
Codespace. Trabajamos en español. Tú haces la primera versión de cada tarea, en tu rama y con su PR;
Claude Code la verifica, la corrige, la fusiona y publica la release (docs/GUIA_IA.md, sección 2).

Antes de tocar nada:
1. Lee completos, en este orden: tus reglas (.clinerules o .github/copilot-instructions.md),
   docs/GUIA_IA.md, README.md (sobre todo las novedades de cada versión), docs/ROADMAP.md y
   docs/PLAN_HERRAMIENTAS_IA.md. Para cada tarea, lee además el documento de su sistema
   (docs/GDD.md, docs/ARQUITECTURA.md, docs/MUNDO.md, docs/PARTIDAS.md...) y
   .claude/skills/estepa/SKILL.md.
2. Revisa el entorno y dime qué encontraste (los comandos de a uno, sin encadenarlos con &&; si uno
   se aborta o no muestra salida, no lo repitas: lee la sección 7 de docs/GUIA_IA.md y avísame):
   - git status: limpio; git fetch origin; en main y al día con origin/main (sin commits propios);
   - git config core.hooksPath (tiene que decir .githooks; si no, córrelo: git config core.hooksPath .githooks);
   - que gh esté instalado y autenticado (gh auth status); si no, corre bash .devcontainer/setup.sh;
   - rm -rf build, cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release, cmake --build build y
     ./build/sim_tests (lee la salida entera);
   - python3 tools/ci/guardas.py (0 errores);
   - una captura: cmake -E copy_directory assets build/assets y, desde build/,
     xvfb-run -a ./estepa --frames 60 --screenshot shot.png; mírala y dime qué se ve.
3. Resúmeme en pocas líneas la versión y lo que propones hacer después. No cambies nada hasta que te
   diga qué tarea hacer.

En cada tarea, sin excepciones:
- Sigue la sección 3 de docs/GUIA_IA.md: rama cline/<tarea> (o copilot/<tarea>) desde origin/main;
  solo lo que la tarea pide; pruebas nuevas al final de tests/test_main.c; todas las pruebas, las
  guardas y una captura que mires; el documento del sistema; PR con la sección «Para Claude» llena.
- No toques VERSION, ni las novedades del README, ni el ROADMAP. No fusiones ni publiques: eso lo hace
  Claude al verificar.
- Nunca trabajes en main. Nunca --force, --admin ni --auto. Nunca borres, recortes ni aflojes una
  prueba. Nunca cambies una herramienta compartida para que algo pase. Nunca armes un sistema paralelo
  a uno que ya existe (búscalo con git grep). Nunca subas archivos vacíos ni marcadores.
- Si la tarea es grande o ambigua, primero proponme un plan corto (archivos, pruebas, documentos) y
  espera mi OK. Si algo falla y no ves la causa, para y pregúntame.
- Al terminar la sesión, tu rama siempre subida (aunque falte: el PR como borrador). Infórmame en
  pocas líneas: qué cambió, cómo lo probaste (con números), el enlace del PR y lo pendiente.
```

## 12. Plantillas

**Para pedir una tarea al Codespace** (Cline o Copilot):

```text
Tarea: <lo que quieres, con el detalle que tengas: qué, dónde y por qué>.

Sigue tus reglas y docs/GUIA_IA.md. Antes de escribir código, dime en pocas líneas qué vas a
cambiar (archivos, pruebas y documentos) y espera mi OK. Al final, la rama subida y el PR con la
sección «Para Claude» llena.
```

Para los modelos 3D alcanza con pasar la URL del `.glb` (de TRELLIS.2 o RigNet en Hugging Face): las
reglas dicen qué hacer con ella.

**Para pedir la verificación a Claude Code:**

```text
Verifica el PR #<número> (<lo que debía hacer, en una línea>). Si está bien o lo corriges, publícalo
con su versión y su release.
```

## 13. Para el dueño del repositorio

Los ajustes de GitHub que hacen que ni una IA ni un descuido puedan romper `main`:
- **Settings → Rules → Rulesets** (o *Branches → Add branch protection rule*) para `main`:
  - *Require a pull request before merging*, sin pedir aprobaciones: hay un solo dueño.
  - *Require status checks to pass*, con «Linux (tests + build + captura)», «Windows (tests + build)»
    y «Android (APK arm64)». Las guardas corren dentro del de Linux.
  - *Block force pushes*.
  - *Bypass list* vacía: así nadie, ni con permisos de administrador (`gh pr merge --admin`), fusiona con
    el CI en rojo.
- **Settings → General → Pull Requests → Automatically delete head branches:** cada rama se borra sola al
  fusionar su PR, y queda solo `main` (más la de archivo, `cline/rescate-codespace`).
