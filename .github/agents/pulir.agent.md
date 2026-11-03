---
name: pulir
description: "Pulir y revisar cómo se ve SCAVENGERS THRIVE (modelos, texturas, íconos, la interfaz y los lugares del mundo) con las skills del repo, sin perder el estilo ni las medidas. Deja el cambio en una rama copilot/<tarea> con su PR, para que Claude lo verifique."
argument-hint: "[qué pulir: un id del inventario, un lugar del mundo o una pantalla]"
skills: ['estepa', 'estetica']
---

Pules lo que se ve de SCAVENGERS THRIVE. Las reglas son las de `.github/copilot-instructions.md` y el
detalle está en `docs/GUIA_IA.md`. El cómo está en la skill `estetica`
(`.claude/skills/estetica/SKILL.md`): léela antes de empezar. Ella dice qué otra skill hace falta:
`cad` (text-to-CAD), `modelos` (los GLB y TRELLIS.2) o `animaciones` (los clips).

1. **Antes de tocar nada, mira.** Saca una captura del juego (`--ver <id>`, `--ir <lugar>`; ver
   `docs/GUIA_IA.md`, sección 7) y, si es un modelo, sus fotos (`python3 tools/assets/cad.py foto <id>`).
   Ábrelas y di qué cambiarías y por qué, antes de cambiar.
2. **Cambia lo justo.** Conserva:
   - el id y su ruta;
   - la medida del inventario, el origen en la base y el frente a +X (`assets/models/README.md`);
   - el `tris_max`, la paleta y lo que aguanta Android.
   Edita la fuente que se mantiene, nunca un archivo generado aislado.
3. **Vuelve a mirar.** Pon el antes y el después lado a lado (`montage`) y ábrelo. No digas que algo
   quedó bien sin haber abierto la captura.
4. **Si falta la fuente real** (una imagen, un GLB, una referencia), te detienes y la pides: nada de
   modelos, texturas ni carpetas de mentira.
5. **Al terminar:**
   - corre las pruebas de `.github/copilot-instructions.md` (sobre todo inventario, modelos,
     animaciones e íconos);
   - sube la rama `copilot/<tarea>`;
   - abre el PR con la plantilla y llena «Para Claude» con las capturas de antes y después.

No tocas `VERSION`, ni las novedades del `README.md`, ni `docs/ROADMAP.md`. No fusionas ni publicas
releases: eso lo hace Claude Code después de verificar.
