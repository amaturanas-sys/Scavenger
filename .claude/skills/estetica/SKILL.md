---
name: estetica
description: Pulir y revisar cómo se ve SCAVENGERS THRIVE con las herramientas del repo (text-to-CAD con cadgen, la tubería de modelos con TRELLIS.2, Kiln, Nomad, íconos, texturas, nubes, logo y capturas del juego), sin perder el estilo ni las medidas. Usar cuando se pida mejorar, refinar, corregir o comparar el aspecto de un modelo, una textura, un ícono, la interfaz o un lugar del mundo.
---

# Pulir lo que se ve

Esta skill sirve igual para Copilot (modo agente de VS Code, en el Codespace), Claude Code y Cline: todo
son comandos de terminal y archivos del repo. Las reglas de siempre siguen valiendo
(`.github/copilot-instructions.md` o `.clinerules`; el detalle, en `docs/GUIA_IA.md`).

## El ciclo: mirar, cambiar, volver a mirar

1. **Mirar antes de tocar,** y guardar esa imagen como «antes»:
   - **En el juego** (desde `build/`, con los assets al día: `cmake -E copy_directory ../assets assets`):
     `xvfb-run -a ./estepa --ver <id> --frames 40 --screenshot antes.png`. Para un lugar, `--ir <lugar>`; la
     tabla completa está en `docs/GUIA_IA.md`, sección 7.
   - **El modelo solo,** desde varias cámaras: `python3 tools/assets/cad.py foto <id> [iso right front top]`
     escribe `build/cad/<nombre>_<cámara>.png`. El frente de los modelos es +X: lo muestra la cámara `right`.
   - **Medirlo** contra el inventario: `python3 tools/assets/cad.py visor` abre el CAD Viewer. En el
     Codespace se abre la URL que imprime: el puerto se reenvía solo.
2. **Elegir la herramienta:**

   | Qué se pule | Con qué | El detalle |
   |---|---|---|
   | Un modelo nuevo o rehecho desde una imagen | `python3 tools/modelos/modelos.py traer <ID> --url … --fuente trellis2` | skill `modelos` |
   | Piezas de superficie dura (ballestas, cañones, maquinaria de asedio, placas) | cadgen (build123d), y `cadgen glb build` para el GLB | skill `cad` |
   | Un modelo low-poly escrito como programa | Kiln (`tools/assets/*.kiln.js`) | `tools/assets/README.md` |
   | Esculpir o pintar a mano | Nomad Sculpt y su atlas (`arte/nomad/`) | `arte/nomad/README.md` |
   | Animaciones | GRAIL → SMPL → clips en el GLB | skill `animaciones` |
   | Íconos de los menús | pintar su celda en `assets/ui/iconos.png`; después `iconos.py guia` y `iconos.py check` | `tools/assets/iconos.py` |
   | Texturas del suelo | `tools/assets/texturas_terreno.py` | su uso, al principio del script |
   | Nubes | `tools/assets/nubes.py` | ídem |
   | El logo y los íconos de la app | `tools/assets/logo.py` | ídem |
   | Paneles, colores y HUD | `src/ui/theme.h` y `src/ui/` | `docs/ESTILO_VISUAL.md` |

3. **Cuidar el estilo y la medida:**
   - **La interfaz es orfebrería nómade:** oro, plata, turquesa y cornalina, pixel a pixel a 640×360, sin
     degradados (`docs/ESTILO_VISUAL.md`; la paleta, en `src/ui/theme.h`).
   - **Los modelos:**
     - low-poly y mates;
     - en metros, +Y arriba, +X adelante y el origen en la base;
     - con la medida y el `tris_max` de `assets/inventario.tsv` (`assets/models/README.md`).

     El modelo se ajusta al inventario, no al revés.
4. **Volver a mirar** con el mismo comando y la misma cámara, y poner las dos imágenes lado a lado:
   `montage antes.png despues.png -tile 2x1 -geometry +0+0 comparar.png`.
   - **Si puedes ver imágenes,** ábrelas y di qué cambió.
   - **Si no,** abre el PNG en el editor y pide al usuario que lo mire.
   - **Nunca digas que algo se ve bien** sin que alguien haya mirado la imagen.
5. **Comprobar:** `python3 tools/modelos/modelos.py check`, `python3 tools/assets/inventario.py check`,
   `python3 tools/assets/iconos.py check` y, antes de subir, la lista completa de pruebas.
6. **Subir** como cualquier tarea: tu rama, el PR con su plantilla y, en él, el antes y el después. Claude
   Code lo verifica y lo fusiona.

## Lo que no se hace
- **Editar a mano lo generado:** `docs/INVENTARIO.md`, `docs/ANIMACIONES.md`, las guías `*_guia.png` y
  `src/game/save_schema.inc` salen de sus scripts.
- **Usar `--forzar` sobre lo pintado o esculpido a mano** (atlas, íconos, texturas, los modelos de
  `arte/nomad/`), salvo que el usuario lo pida: lo pisa.
- **Cambiar una medida del inventario** para que un modelo «entre».
- **Subir imágenes de prueba:** `build/` no se sube, y las capturas de `docs/` se rehacen con
  `bash tools/capturas.sh build`.

## El entorno
El Codespace lo prepara `.devcontainer/setup.sh`:
- el juego compilado;
- `.venv`, para la tubería de modelos;
- `.venv-cad`, para text-to-CAD, con Chromium para las fotos;
- ImageMagick (`montage`);
- Node, para Kiln.

Si algo falta:
- **text-to-CAD:** `python3 tools/assets/cad.py instalar`; `python3 tools/assets/cad.py doctor` lo comprueba.
  Si las fotos fallan por bibliotecas del sistema: `sudo .venv-cad/bin/python -m playwright install-deps chromium`.
- **Kiln:** los pasos de `tools/assets/README.md`.
