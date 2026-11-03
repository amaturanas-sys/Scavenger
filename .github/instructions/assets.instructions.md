---
description: "Apply to asset, model, CAD, inventory, animation, and world-rendering changes."
applyTo: "assets/**,tools/assets/**,tools/modelos/**,src/world/**,docs/INVENTARIO.md,docs/ANIMACIONES.md"
---

# Assets y modelos

- Lee `docs/GUIA_IA.md`, `.github/skills/estepa/SKILL.md` y la skill específica antes de cambiar assets.
- Reutiliza IDs, categorías, rutas y herramientas existentes.
- Para text-to-CAD activa `cad`; para GLB activa `modelos`; para clips activa `animaciones`.
- Respeta escala, origen, orientación, `tris_max`, paleta y restricciones Android.
- No edites manualmente `docs/INVENTARIO.md`, `docs/ANIMACIONES.md` ni el atlas de iconos.
- Regenera documentos con sus scripts y abre/revisa una captura del resultado.
- No crees modelos, texturas o carpetas de marcador para ocultar un trabajo incompleto.
- Ejecuta los validadores de assets y deja sus resultados exactos en el PR.
