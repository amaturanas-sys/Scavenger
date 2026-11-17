---
name: animaciones
description: Las animaciones de personas y animales de SCAVENGERS THRIVE desde videos de referencia o bancos de movimiento (video → SMPL → esqueleto del juego → clips en el GLB), con la licencia de cada etapa. Usar cuando el usuario quiera animar un cuerpo, pase videos o clips de movimiento, o pregunte qué grabar.
---

# Animaciones desde videos

El juego elige el clip por su nombre (`src/sim/anim_index.c`) dentro del GLB del modelo; la lista de
clips, obligatorios y opcionales, por esqueleto, está en `docs/ANIMACIONES.md` (sale de
`assets/animaciones.tsv`). No hace falta tocar C para un clip nuevo del índice.

## Qué pedirle al usuario (docs/PLAN_HERRAMIENTAS_IA.md, C.1)

- **Antes, los cuerpos:** los clips se pegan a un cuerpo riggeado (`personaje.base.*`, los animales
  con `rig:`). Sin él no hay donde verlos: primero el modelo (skill `modelos`, con RigNet).
- **Videos:** una persona, de cuerpo entero, cámara fija de lado o en tres cuartos a la altura de la
  cadera, fondo liso, buena luz, ropa ajustada, 30 fps o más, de 3 a 10 s, empezando y terminando
  en posición neutra; los ciclos (caminar, correr) con 3 o 4 pasos. En `arte/videos/<clip>.mp4`.
- **Primero lo que más se ve:** `idle`, `caminar`, `correr`, los golpes y `recibir_golpe`; después,
  montar y la vida diaria. Antes de grabar, mirar si el clip ya existe en CMU o Mixamo (ver «Las
  licencias»): menos trabajo.

## Las licencias

Si el juego puede venderse, cada etapa tiene que permitir el uso comercial. La decisión es del
usuario; antes de publicar, leer la licencia vigente de cada pieza.

- **No comerciales:**
  - GRAIL (`recon_4dhoi`, GEM-SMPL): NVIDIA License, uso no comercial.
  - GVHMR: su `LICENSE` lo limita a «educational, research and non-profit purposes».
  - El modelo SMPL (y SMPL-X): excluye usarlo en un producto comercial y la «production of other
    artifacts for commercial purposes». Toda la etapa 1 produce movimiento SMPL, así que lo
    necesita: WHAM, por ejemplo, tiene código MIT, pero pide SMPL.
  - AMASS: aunque incluye los movimientos de CMU, su versión en SMPL es de uso no comercial.
- **Con uso comercial:**
  - La licencia comercial de SMPL, que vende Meshcapade (más la del método, a sus autores).
  - CMU: «free for all uses»; se puede incluir en un juego que se vende, pero no revender los
    datos, ni convertidos. Ojo con las conversiones de terceros: el FBX de 4TU, por ejemplo, es
    CC BY-NC.
  - Mixamo: sin regalías en juegos, sin ofrecer los archivos de animación sueltos.
- **Sin SMPL:** MediaPipe Pose (código Apache-2.0; 33 puntos en 3D, más tosco, y cambia el retarget
  de la etapa 2). Revisar la ficha del modelo que se use.

## La tubería (en una máquina con GPU o un espacio de Hugging Face)

1. **Video → movimiento SMPL:** GRAIL u otro método de la misma etapa, con la licencia que
   corresponda (ver «Las licencias»).
2. **SMPL → el esqueleto del juego** (Blender; los clips de CMU o Mixamo entran acá, desde su propio
   esqueleto): retarget al esqueleto compartido del cuerpo, corregir el pie que patina, recortar y
   cerrar los ciclos, 30 fps, la raíz en el sitio (el juego mueve al personaje). Para revisarlo con
   datos (ejes, pies que patinan o atraviesan el piso), la skill `blender-motion-state-inspection`.
3. **Exportar** un GLB del cuerpo con todos sus clips, cada uno con el nombre exacto de
   `docs/ANIMACIONES.md`.
4. **Traerlo:** `python3 tools/modelos/modelos.py traer <id> --archivo cuerpo.glb --fuente grail --riggeado`
   (o `--url`; `--fuente otro` si viene de CMU o Mixamo). Avisa los clips obligatorios que falten; el
   CI (`animaciones.py check`) falla si un modelo importado no los trae.
5. **Mirarlo:** `./estepa --ver <id> --screenshot ver.png` (desde `build/`), y en la partida.

## Lo que no se hace acá

Esta sesión no tiene GPU ni llega a huggingface.co: la generación (TRELLIS.2, RigNet, GRAIL) corre
del lado del usuario (la tablet con los espacios de Hugging Face, o Codespaces con Cline); acá se
integra, se valida y se revisa.
