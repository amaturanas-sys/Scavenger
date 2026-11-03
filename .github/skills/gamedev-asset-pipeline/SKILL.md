---
name: gamedev-asset-pipeline
description: "Use when importing, refining, validating, previewing, or packaging game assets for SCAVENGERS THRIVE, including GLB models, low-poly props, inventory entries, platform budgets, and CI checks."
---

# Tubería de assets de SCAVENGERS THRIVE

Usa esta skill junto con `estepa`, `cad` y `modelos`. Lee
`docs/GUIA_IA.md`, `assets/models/README.md` y el inventario antes de tocar un asset.

## Reglas

- Busca primero el ID en `assets/inventario.tsv`; no inventes categorías ni rutas.
- Los modelos van en `assets/models/<categoria>/<subcategoria>/<nombre>.glb`.
- Respeta las medidas, `tris_max`, +Y arriba, frente +X y origen en el centro de la base.
- Usa colores planos y texturas pequeñas; conserva el presupuesto de Android.
- No edites a mano `docs/INVENTARIO.md`, `docs/ANIMACIONES.md` ni el atlas.
- No registres modelos nuevos en C: el motor los carga por ID.
- Modelos con rig o clips deben pasar `animaciones.py check`.
- No añadas memoria dinámica ni carga de assets dentro del bucle de cuadros.

## Flujo

1. Revisa modelos similares y el estado del inventario.
2. Para CAD, usa `cad`; para GLB desde imagen, usa `modelos`.
3. Importa o convierte con `tools/modelos/modelos.py`.
4. Ejecuta los validadores de inventario, modelos, animaciones e iconos.
5. Compila y revisa una captura con `--ver <id>` o `--galeria`.
6. Comprueba el impacto en Linux, Windows y Android mediante CI.
7. Abre un PR con la procedencia, comandos y captura revisada.

## Validación mínima

```sh
python3 tools/assets/inventario.py check
python3 tools/assets/inventario.py doc --verificar
python3 tools/assets/animaciones.py check
python3 tools/assets/iconos.py check
python3 tools/modelos/modelos.py check
python3 tools/modelos/test_modelos.py
python3 tools/ci/check_copilot_skills.py
```

## Procedencia

Esta skill adapta el agente `gamedev-asset-pipeline` de Claude del proyecto para
Copilot. La política del proyecto y las herramientas locales son la autoridad.
