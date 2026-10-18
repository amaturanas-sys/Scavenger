---
name: animaciones
description: Las animaciones de personas y animales de SCAVENGERS THRIVE desde videos de referencia (GRAIL o alternativas abiertas → SMPL → esqueleto del juego → clips en el GLB). Usar cuando el usuario quiera animar un cuerpo, pase videos o clips de movimiento, o pregunte qué grabar.
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
  montar y la vida diaria. Antes de grabar, mirar si el clip ya existe en bancos abiertos (CMU,
  AMASS, Mixamo): menos trabajo y licencias más claras.

## La tubería (en una máquina con GPU o un espacio de Hugging Face)

1. **Video → movimiento SMPL:** GRAIL (`recon_4dhoi`, GEM-SMPL) es **solo para uso no comercial**;
   si el juego puede venderse, usar una alternativa abierta de la misma etapa (GVHMR, WHAM o
   similar, con su licencia revisada). La decisión es del usuario.
2. **SMPL → el esqueleto del juego** (Blender): retarget al esqueleto compartido del cuerpo, corregir
   el pie que patina, recortar y cerrar los ciclos, 30 fps, la raíz en el sitio (el juego mueve al
   personaje).
3. **Exportar** un GLB del cuerpo con todos sus clips, cada uno con el nombre exacto de
   `docs/ANIMACIONES.md`.
4. **Traerlo:** `python3 tools/modelos/modelos.py traer <id> --archivo cuerpo.glb --fuente grail --riggeado`
   (o `--url`). Avisa los clips obligatorios que falten; el CI (`animaciones.py check`) falla si un
   modelo importado no los trae.
5. **Mirarlo:** `./estepa --ver <id> --screenshot ver.png` (desde `build/`), y en la partida.

## Lo que no se hace acá

Esta sesión no tiene GPU ni llega a huggingface.co: la generación (TRELLIS.2, RigNet, GRAIL) corre
del lado del usuario (la tablet con los espacios de Hugging Face, o Codespaces con Cline); acá se
integra, se valida y se revisa.
