# Capturas de modelos OpenGameArt importados

Este directorio contiene capturas renderizadas en el juego (`xvfb-run -a ./build/estepa --ver <id>`)
de los modelos importados del OpenGameArt FREE Remastered Meadows (`assets/referencia/gltf.zip`).

Se seleccionaron **1-2 modelos representativos por categoría** (31 de 268 modelos totales) para
servir como punto de partida visual. Tras modificar los assets con las skills de voxeado del
repositorio, se pueden generar nuevas capturas con los mismos IDs y comparar lado a lado.

## Estructura

```
docs/referencia/oga_capturas/
├── <categoria>/<id>.png          # Captura individual por modelo
├── <categoria>_contacto.png      # Hoja de contacto por categoría
└── README.md                     # Este archivo
```

## Categorías y modelos capturados

| Categoría | Modelos capturados | Total en inventario |
|-----------|-------------------|---------------------|
| `estructura.cerca` | `fence_pin_01a`, `fence_pin_01b` | 34 |
| `estructura.portal` | `frostp_portal` | 1 |
| `estructura.puente` | `bridge_01`, `bridge_02` | 18 |
| `mapa.camino` | `downroad_2x2_seam_01a`, `downroad_2x2_seam_01b` | 19 |
| `mapa.roca` | `rock_01a`, `rock_01b` | 20 |
| `mapa.roca_acantilado` | `clif_01a`, `clif_01b` | 5 |
| `mapa.roca_piedra` | `stone_cobble_06`, `stone_cobble_06b` | 3 |
| `mapa.terreno_colina` | `hill_01a`, `hill_01b` | 17 |
| `mapa.vegetacion_arbol` | `tree_01c`, `tree_01c_1` | 37 |
| `mapa.vegetacion_arbusto` | `bush_02b`, `bush_02c` | 9 |
| `mapa.vegetacion_hierba` | `grass_01`, `grass_06` | 3 |
| `mapa.vegetacion_planta` | `plant_01`, `plant_02` | 4 |
| `mapa.vegetacion_seta` | `mushroom_01`, `mushroom_03` | 5 |
| `mapa.vegetacion_tronco` | `trunk_01d`, `trunk_01e` | 5 |
| `utileria.generico` | `prop_forester_watchtower_01`, `prop_pin_01a` | 20 |
| `utileria.objeto_generico` | `ea01_env_branch_01`, `ea01_env_branch_02` | 68 |

**Total:** 31 modelos capturados de 268 importados (12 categorías con 2 modelos, 4 con 1).

## Cómo generar más capturas

Para capturar un modelo específico:

```bash
cd build && xvfb-run -a ./estepa --ver <id> --frames 40 --screenshot salida.png
```

Para regenerar todas las capturas de este documento:

```bash
python3 tools/generar_capturas_oga.py --modelos-por-cat 2
```

## Notas

- Las capturas muestran el modelo a escala real del inventario, con la cámara en tres cuartos desde el frente (+X).
- Algunos modelos grandes (puentes, acantilados, troncos altos) pueden aparecer recortados en la vista de tres cuartos.
- Las medidas del inventario se actualizaron a las dimensiones reales de los `.glb` convertidos (commit `d219648`).
- Los 2 modelos con avisos de escala restantes (`arbusto_sorrel.glb`) son preexistentes y no forman parte de esta importación.