---
name: crear-modelo
description: "Create and integrate a new low-poly SCAVENGERS THRIVE model from CAD, GLB, or a provided reference."
---

Crea o integra el modelo indicado por el usuario.

1. Lee `docs/GUIA_IA.md`, activa `estepa`, `cad`, `modelos` y
   `gamedev-asset-pipeline` según el origen.
2. Busca el ID existente en `assets/inventario.tsv`; si no existe, pide la
   categoría y las medidas antes de crear datos.
3. Usa la tubería existente y respeta +Y arriba, frente +X, origen en la base,
   medidas del inventario, `tris_max` y estilo low-poly.
4. No edites manualmente `docs/INVENTARIO.md`; regénéralo con la herramienta.
5. Ejecuta los validadores y compila si corresponde.
6. Revisa visualmente:

   ```sh
   cmake -E copy_directory assets build/assets
   (cd build && xvfb-run -a ./estepa --ver <id> --frames 40 --screenshot /tmp/modelo.png)
   ```

7. Abre la captura antes de afirmar que el modelo es correcto.
8. Crea una rama `copilot/<tarea>`, un PR con la plantilla y la sección «Para
   Claude» completa. No fusiones ni publiques release.
