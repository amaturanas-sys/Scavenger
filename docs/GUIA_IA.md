# Guía para trabajar con IA en SCAVENGERS THRIVE

Para Cline en el Codespace y para cualquier otra IA que continúe el proyecto. Recoge cómo se trabajó
con Claude Code hasta la v0.10.3: el flujo de cada cambio con sus comandos, las reglas que no se
rompen, lo que ya salió mal y el estado del proyecto.

- **`.clinerules`** tiene las reglas en corto (Cline las carga en cada mensaje).
- **Acá está el detalle.**
- **Al final**, el prompt maestro para empezar una sesión y la plantilla para pedir cada tarea.

## 1. El proyecto en un minuto

- **El juego:** *SCAVENGERS THRIVE — THEY COME FROM THE STEPPES* (nombre interno `estepa`), en C11 con
  raylib 5.5, a 640 × 360. Corre en Windows y en Android (APK); se desarrolla en Linux.
- **Dónde va:** la versión está en `VERSION`. Lo hecho y lo pendiente, en [ROADMAP.md](ROADMAP.md); el
  plan vigente, en [PLAN_HERRAMIENTAS_IA.md](PLAN_HERRAMIENTAS_IA.md). Las novedades de cada versión,
  en el `README.md`.
- **Las carpetas:**
  - `src/sim/`: C puro, sin raylib. Todo lo que tiene reglas va acá y se prueba en `tests/test_main.c`.
  - `src/game/`: el juego (une la simulación con raylib: entrada, dibujo, diálogos).
  - `src/world/`: el dibujo del mundo (terreno, voxeles, vegetación, objetos sueltos).
  - `src/ui/`: la interfaz.
  - `assets/`: el inventario (`inventario.tsv`), las traducciones (`i18n/en.tsv`), los modelos, los íconos.
  - `tools/`: los scripts de assets, partidas, modelos, Android, Windows y capturas.
  - `tests/`: las pruebas del núcleo y las partidas de referencia (`tests/partidas/`).
  - `docs/`: la documentación, siempre en español.
- **Los documentos:**

| Documento | Qué tiene |
|---|---|
| `README.md` | Compilar, controles, el Codespace y las novedades de cada versión. |
| [GDD.md](GDD.md) | El diseño y las reglas de cada sistema, con los nombres de sus funciones. |
| [ARQUITECTURA.md](ARQUITECTURA.md) | Los módulos de cada carpeta y qué hace cada uno. |
| [MUNDO.md](MUNDO.md) | La generación del mundo, el terreno, la vegetación, el cielo. |
| [PARTIDAS.md](PARTIDAS.md) | El formato de las partidas y cómo cambiar un struct que se guarda. |
| [ARQUITECTURA_EDAD_DEL_HIERRO.md](ARQUITECTURA_EDAD_DEL_HIERRO.md) | El kit de edificios: casas, palacios, murallas, ruinas, obras. |
| [INVENTARIO.md](INVENTARIO.md), [ANIMACIONES.md](ANIMACIONES.md) | Generados por sus scripts: no se editan a mano. |
| [PLAN_HERRAMIENTAS_IA.md](PLAN_HERRAMIENTAS_IA.md), [ROADMAP.md](ROADMAP.md) | El plan, su estado y lo que falta. |
| [CAPTURAS.md](CAPTURAS.md) | Las capturas de cada pantalla y cómo se sacan. |
| [NARRATIVA.md](NARRATIVA.md), [ESTILO_VISUAL.md](ESTILO_VISUAL.md) | La historia y el estilo. |
| `.claude/skills/estepa`, `modelos`, `animaciones` | Las convenciones y las tuberías de modelos y animaciones (sirven de guía también para Cline). |

## 2. El flujo de cada cambio

Así se hizo cada versión. Cada paso evita un error que ya pasó (ver la sección 5).

1. **Entender antes de tocar:**
   - Leer el documento del sistema y su código. Buscar con `git grep` si ya existe algo que lo haga:
     se extiende lo que hay; no se arma un sistema paralelo.
   - Si la tarea es grande o ambigua, proponer al usuario un plan corto y esperar su OK: qué archivos,
     qué pruebas, qué documentos, qué versión.
2. **Rama nueva desde el `main` al día:**
   ```bash
   git fetch origin
   git switch -c cline/<tarea> origin/main
   ```
3. **Implementar:**
   - Las reglas, en `src/sim/` con sus pruebas en `tests/test_main.c`.
   - El dibujo y la entrada, en `src/game/` o `src/world/`.
   - Los textos visibles, con `T()` o `N_()`.
   - Un `.c` nuevo, en `CMakeLists.txt`.
4. **Probar todo** (lo mismo que corre el CI):
   ```bash
   cmake --build build && ./build/sim_tests
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
5. **Mirar el juego** (sección 4): una captura del lugar que cambió, y abrir la imagen. Que compile no
   alcanza: un juego que compila igual puede caerse en el primer cuadro.
6. **Documentar:**
   - El documento del sistema (GDD, MUNDO, ARQUITECTURA...).
   - Si cambia el juego: el `README.md` (una sección «… (vX.Y.Z)» con las novedades), el
     `docs/ROADMAP.md` (una línea arriba de la anterior) y `VERSION`.
   - Si cambia lo que se ve: las capturas (`bash tools/capturas.sh build`) y `docs/CAPTURAS.md`.
   - La sección 6 de esta guía (el estado actual).
7. **Antes de subir, traer `main` y compilar de cero** (una carpeta `build/` vieja esconde errores):
   ```bash
   git fetch origin && git merge origin/main
   rm -rf build && cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build
   ```
   Después, otra vez el paso 4. Revisar `git diff origin/main...HEAD --stat`: tiene que listar solo los
   archivos de la tarea.
8. **Commit y PR:**
   ```bash
   git add -A && git commit -m "Lo que cambia (vX.Y.Z): en una línea, en español"
   git push -u origin cline/<tarea>
   gh pr create --base main --title "..." --body "..."   # o desde GitHub
   gh pr checks --watch                                   # Linux, Windows y Android
   ```
   - El mensaje y el PR dicen lo que hace el diff de verdad, con los números.
   - El PR sigue la plantilla (`.github/pull_request_template.md`).
9. **Merge con el CI verde** en Linux, Windows y Android:
   ```bash
   gh pr merge --squash --delete-branch
   git switch main && git pull
   ```
   Si el CI queda rojo, se arregla en la misma rama; nunca se fusiona en rojo.
10. **Publicar**, solo si cambió `VERSION`:
    ```bash
    gh workflow run release.yml --ref main
    ```
    - También se puede desde GitHub: Actions → Release → Run workflow.
    - Si el token no alcanza, se sube la etiqueta: `git tag vX.Y.Z && git push origin vX.Y.Z`.
    - Hay que comprobar que la release vX.Y.Z tenga el zip de Windows y el APK.
11. **Informar al usuario** (sección 7).

**Las versiones:** `X.Y.Z`.
- Sube la Z: cada PR que cambia el juego (código, assets, reglas).
- Sube la Y: una fase o un sistema grande.
- Lo que solo toca documentos o configuración no sube la versión ni publica.

## 3. Reglas que no se rompen

**El código** (CI compila con gcc en Linux, MSVC en Windows y el NDK en Android):
- **C11 llano:**
  - sin arreglos de largo variable (MSVC no los tiene);
  - sin extensiones de GNU (`typeof`, `__attribute__`, sentencias como expresión);
  - sin `unistd.h` ni `dirent.h` en `src/`;
  - sin `M_PI` (usar las constantes `PI` del código).
- **Cero advertencias nuevas** (`-Wall -Wextra -Wpedantic`, `/W4`).
- **Los shaders** también en GLSL ES 100.
- **La memoria:** arreglos fijos (`*_MAX`) y estructuras planas; nada se reserva por cuadro y no hay ECS genérico.
- **Sin bibliotecas externas nuevas** (cJSON ni ninguna): la compilación solo baja raylib. Si hace falta
  una, se pregunta.
- **raylib:**
  - nunca un `Material` hecho con `{ 0 }` en `DrawMesh` (se cae en el primer cuadro);
  - `LoadShaderFromMemory(vs, fs)` lleva el de vértices primero;
  - nada se carga dentro del bucle de cuadros.
- **Antes de renombrar o quitar algo** (una función, un campo, un archivo): `git grep` todos sus usos
  y arreglarlos en el mismo commit.

**Los datos:**
- **Los textos:** todo lo visible pasa por `T("...")` o `N_("...")`, y se traduce en `assets/i18n/en.tsv`.
- **Los enums crecen al final:** se guardan como números.
- **Las partidas:**
  - Al cambiar un struct que se guarda: `python3 tools/save/esquema.py`.
  - Un campo nuevo empieza en cero en las partidas viejas. Si no puede, se sube la versión del bloque y
    se migra en `migrate()`.
  - Un arreglo puede crecer; para achicarlo hace falta migrar. Ver PARTIDAS.md.
- **El inventario:**
  - Los objetos nuevos van en `assets/inventario.tsv` (después, `inventario.py doc`).
  - Los íconos se generan con `iconos.py generar`.
- **Los modelos 3D** entran solo con `tools/modelos/modelos.py traer`, nunca a mano: cada id tiene una
  ruta fija.
- **Los generados no se editan a mano:** `src/game/save_schema.inc`, `docs/INVENTARIO.md`,
  `docs/ANIMACIONES.md`, el atlas de íconos.

**Git:**
- Solo ramas `cline/<tarea>` desde `origin/main`, una tarea por rama.
- Nunca se sube a `main`, a `claude/*` ni a la rama de otro.
- Nunca `git push --force`, ni `rebase` de algo ya subido.
- Si el push es rechazado: `git fetch` y `git merge origin/main`.
- Con la rama fusionada, se borra (`--delete-branch`).

**Las pruebas:**
- Nunca se apaga, se salta ni se afloja una prueba para tener verde.
- Si una regla cambia a propósito, la prueba se actualiza con los valores nuevos y el PR lo dice.

## 4. Cómo mirar el juego sin pantalla

Desde `build/` (antes, `cmake -E copy_directory ../assets assets`), con `xvfb-run -a` delante:

| Para ver | Comando |
|---|---|
| El arranque | `./estepa --frames 60 --screenshot shot.png` |
| Un lugar del mundo | `--ir estepa`, `bosque`, `altiplano`, `desierto`, `mar`, `capital`, `aldea`, `fortaleza`, `ciudad`, `plaza-<región>`… (con `--dia 11 --minuto 7` es verano al mediodía; `--camara`, `--dron 40` desde arriba) |
| El kit de edificios | `--edificios casa` (o `palacio`, `muralla`, `puerta`, `fortaleza`, `refugio`… ; `:región` para una sola fila) |
| Un objeto de cerca | `--ver <id del inventario>` |
| El asedio | `--asedio puerta`, `fuego`, `salida`, `quemada`, `tomada`… |
| Combate | `--enemigos bandidos`, `culto`, `lobos`, `arqueros` |
| La galería de assets | `--galeria` |
| El mapa del mundo | `--semilla 42 --mapa-mundo mapa.png` |
| Las partidas de referencia | `--probar-partidas ../tests/partidas` |

- **Más opciones:** la lista completa está arriba de `src/main.c` y en el README («Captura automática»).
- **Las capturas de la documentación:** `bash tools/capturas.sh build` las rehace todas (unos 4
  minutos); se suben las que cambiaron.

## 5. Lo que ya salió mal (y no tiene que repetirse)

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
  - Reglas que salieron de ahí: nunca `--force`, nunca a ramas ajenas, y el Codespace se rehace
    desde `main` en vez de remendarlo.
- **Un commit que decía otra cosa que su diff.** «Increase population and vegetation density»
  anunciaba «más casas y campos por persona», pero sus números hacían lo contrario: cada casa
  alojaba más gente. Antes de escribir el mensaje se verifican los números.
- **Números cambiados sin sus pruebas ni sus documentos.** Cambiar una regla obliga a actualizar su
  prueba y el GDD en el mismo PR.

## 6. Estado actual y qué sigue

*(Se actualiza al terminar cada tarea: la versión, lo último y lo que sigue.)*

- **Versión:** 0.10.3. Las tres ramas quedaron unidas en `main`, ahora la única. Las ciudades, los
  árboles y la vegetación son más densos, y el Codespace se prepara solo (`.devcontainer/`).
- **Hecho:**
  - el plan grande (fases 0 a 8b);
  - las tuberías de modelos y animaciones (v0.9.3);
  - la parte B, los edificios: el kit, las murallas con la puerta acodada, las ruinas y las obras (v0.10.0 a v0.10.2).
- **Lo que sigue** espera material del usuario (PLAN_HERRAMIENTAS_IA.md):
  - **las imágenes de la D.2:** primero las piezas de estilo animal, los amuletos y el akinakes; esas entran con
    `tools/modelos/modelos.py traer` (ver `.clinerules`, «Modelos 3D»);
  - **los cuerpos riggeados y los videos de la C.1:** para las animaciones.
- **Para preguntar al usuario:** las casas y los campos por persona (sección 5). Hoy cada casa aloja
  150 almas en la capital y 70 en la aldea. Si quería más casas visibles, hay que bajar esos números.

## 7. Cómo informar al usuario

- Siempre en español y en pocas líneas.
- Qué cambió, cómo se probó (con números: comprobaciones, caminatas, capturas), el enlace del PR y el de
  la release, y lo pendiente o lo que hay que decidir.
- Si algo no se pudo hacer, decirlo claro, con el error y lo que hace falta.
- **Antes de algo que no se deshace** (borrar ramas o archivos del usuario, reescribir historia,
  publicar), preguntar.

## 8. El prompt maestro

Para la primera sesión de Cline en un Codespace nuevo (o cuando haya que retomar todo). Se copia
entero:

```text
Vas a continuar el desarrollo de SCAVENGERS THRIVE (C11 + raylib, Windows y Android) en este
Codespace, donde lo dejó Claude Code en la v0.10.3. Trabajamos en español.

Antes de tocar nada:
1. Lee completos, en este orden: .clinerules, docs/GUIA_IA.md, README.md (sobre todo las novedades de
   cada versión), docs/ROADMAP.md y docs/PLAN_HERRAMIENTAS_IA.md. Para cada tarea, lee además el
   documento de su sistema (docs/GDD.md, docs/ARQUITECTURA.md, docs/MUNDO.md, docs/PARTIDAS.md...)
   y .claude/skills/estepa/SKILL.md.
2. Revisa el entorno y dime qué encontraste:
   - git status: en main, limpio; git fetch origin y que main esté igual a origin/main;
   - que gh esté instalado y autenticado (gh auth status); si no, corre bash .devcontainer/setup.sh;
   - cmake --build build && ./build/sim_tests (si no hay build/, el setup lo arma);
   - una captura: cmake -E copy_directory assets build/assets y, desde build/,
     xvfb-run -a ./estepa --frames 60 --screenshot shot.png; mírala.
3. Resúmeme en pocas líneas la versión, el estado del plan y lo que propones hacer después. No cambies
   nada hasta que te diga qué tarea hacer.

En cada tarea, sin excepciones:
- Sigue el flujo de la sección 2 de docs/GUIA_IA.md: rama cline/<tarea> desde origin/main; las
  reglas de la sección 3; todas las pruebas y una captura que mires; los documentos; PR; CI verde en
  Linux, Windows y Android; squash merge borrando la rama; la release si subió VERSION.
- Nunca subas a main ni uses --force. Nunca apagues ni aflojes una prueba para tener verde. Nunca
  agregues bibliotecas externas ni armes un sistema paralelo a uno que ya existe (búscalo con git grep).
- Si la tarea es grande o ambigua, primero proponme un plan corto (archivos, pruebas, documentos,
  versión) y espera mi OK. Si algo falla y no ves la causa, para y pregúntame.
- Al terminar, actualiza la sección 6 de docs/GUIA_IA.md e infórmame según la sección 7: qué
  cambió, cómo lo probaste (con números), el enlace del PR y de la release, y lo pendiente.
```

## 9. Plantilla para pedir cada tarea

```text
Tarea: <lo que quieres, con el detalle que tengas: qué, dónde y por qué>.

Sigue .clinerules y docs/GUIA_IA.md. Antes de escribir código, dime en pocas líneas qué vas a
cambiar (archivos, pruebas, documentos y versión) y espera mi OK.
```

Para los modelos 3D alcanza con pasar la URL del `.glb` (de TRELLIS.2 o RigNet en Hugging Face):
`.clinerules` dice qué hacer con ella.

## 10. Para el dueño del repositorio

Dos ajustes de GitHub que hacen que ni una IA ni un descuido puedan romper `main`:
- **Settings → Branches → Add branch protection rule** (o *Rules → Rulesets*) para `main`:
  - *Require a pull request before merging*, sin pedir aprobaciones: hay un solo dueño.
  - *Require status checks to pass*, con «Linux (tests + build + captura)», «Windows (tests + build)»
    y «Android (APK arm64)».
  - *Block force pushes*.
- **Settings → General → Pull Requests → Automatically delete head branches:** cada rama se borra
  sola al fusionar su PR, y queda solo `main`.
