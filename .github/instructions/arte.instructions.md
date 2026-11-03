---
description: "Al cambiar lo que se ve: modelos, texturas, íconos, animaciones, el inventario, o cómo se dibujan el mundo y la interfaz."
applyTo: "assets/**,arte/**,src/world/**,src/ui/**,src/game/world_game.c,docs/INVENTARIO.md,docs/ANIMACIONES.md"
---

# Lo que se ve

- **Primero, la skill `estetica`** (`.claude/skills/estetica/SKILL.md`): el ciclo mirar → cambiar →
  volver a mirar, y qué herramienta va en cada caso. Para text-to-CAD, `cad`; para los GLB, `modelos`; para
  los clips, `animaciones`. Las convenciones del juego están en `estepa`.
- **Lo que ya existe se reutiliza:** los ids, las categorías y las rutas de `assets/inventario.tsv`, y las
  herramientas de `tools/assets/` y `tools/modelos/`. No se inventan otras.
- **Las medidas:**
  - la del inventario;
  - el origen en la base y el frente a +X (`assets/models/README.md`);
  - el `tris_max`, la paleta y lo que aguanta Android.
- **Lo generado no se edita a mano:** `docs/INVENTARIO.md`, `docs/ANIMACIONES.md` y el atlas de íconos
  salen de sus scripts.
- **Nada de marcadores:** ni modelos, texturas ni carpetas de mentira para tapar lo que falta.
- **Se mira antes y después:** con una captura del juego (`docs/GUIA_IA.md`, sección 7), abierta. En el
  PR van las capturas y los números exactos de los validadores.
