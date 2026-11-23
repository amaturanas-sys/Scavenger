# INSTRUCCIONES DE INTEGRACIÓN 3D: REFERENCIAS CANVA AI -> HUNYUAN3D -> MOTOR C11 (`estepa`)

Este documento define las reglas obligatorias para que **Claude Code** interprete cada modelo 3D (`.glb`) generado mediante **Tencent Hunyuan3D**, lo vincule con su imagen de referencia homónima (`.png`) alojada en esta misma carpeta, lo optimice al presupuesto del motor y lo acople correctamente a los cuerpos y animaciones de **SCAVENGERS THRIVE** (`C11 + Raylib`).

---

## 1. Asociación Imagen de Referencia (`PNG`) <-> Modelo 3D (`GLB`)

1. **Emparejamiento estricto 1:1 por nombre de archivo:**
   - Cada imagen `<id>.png` en esta carpeta corresponde exactamente al modelo `<id>.glb` generado a partir de ella.
   - Antes de importar `<id>.glb` al proyecto, inspecciona visualmente `<id>.png` para verificar:
     - **Silueta y proporciones reales** del objeto.
     - **Huecos interiores:** En cascos, corazas, faldones, botas y calderos, verifica que Hunyuan3D no haya rellenado el interior como un bloque sólido. Si el interior quedó cerrado, ábrelo/recórtalo en Blender antes de exportar.
     - **Artefactos de fondo:** Elimina cualquier vértice flotante o rebaba residual heredada de los fondos de alto contraste (`#FFFFFF` o `#00FF00`).

2. **Normalización Geométrica y Estética (`low-poly` PS2 / Android):**
   - **Presupuesto de polígonos:** Reduce cada malla al límite `tris_max` establecido en `assets/inventario.tsv` mediante `python3 tools/modelos/modelos.py traer <id> ...` (o decimate limpio en Blender conservando bordes duros).
   - **Orientación canónica:** El frente funcional del objeto debe apuntar hacia **`+X`**, el eje vertical hacia **`+Y`** y el lateral hacia **`+Z`**.
   - **Ubicación del origen `(0, 0, 0)` (Pivote de acople):**
     - *Cascos (`helm_*`):* Base del cráneo / centro de la corona interior.
     - *Pecheras y corazas (`armor_*`):* Centro geométrico de la caja torácica (`Spine2` / `Chest`).
     - *Cinturones y faldones (`belt_*`, `skirt_*`):* Centro de la pelvis / hebilla frontal (`Hips`).
     - *Botas y grebas (`boots_*`, `greaves_*`):* Suela / base del tobillo (separar malla izquierda y derecha si vienen en par).
     - *Armas cuerpo a cuerpo y herramientas de mano (`wpn_*`, `item_torch`, `item_pickaxe`, etc.):* Centro de la empuñadura donde cierra la palma (`RightHand`).
     - *Escudos (`shield_*`):* Centro de la abrazadera interior del antebrazo (`LeftForeArm`).
     - *Arcos (`wpn_*_bow`):* Centro de la empuñadura del cuerpo del arco (`LeftHand`).
     - *Utilería de suelo (`item_cauldron_bronze`, `item_supply_chest`, etc.):* Base inferior apoyada en `Y = 0`.
   - **Materiales y Shaders:** Usar texturas difusas planas/nítidas de baja resolución sin mapas PBR pesados, garantizando compatibilidad simultánea con **Desktop (`glsl330`)** y **Android (`glsl100` / OpenGL ES 2.0)**.
   - **Reemplazo de *placeholders*:** Al mover/importar el `.glb` final a su ruta de destino en `assets/models/`, reemplaza los archivos *placeholder* de 0 bytes existentes sin inventar rutas paralelas ni alterar el esquema de `EQUIP_PATHS` en `src/entities/equipment.c`.

---

## 2. Tabla Maestra de Mapeo: Referencia -> Destino -> Hueso / Socket

| ID (`<id>.png` / `<id>.glb`) | Categoría | Ruta de Destino en `assets/models/` | Hueso / Socket Principal (`GetModelBoneTransform`) | Socket Alternativo (Enfundado / Guardado) |
| :--- | :--- | :--- | :--- | :--- |
| `helm_leather_cap` | Casco | `personajes/equipamento/cascos/` | `Head` (`cabeza`) | — |
| `helm_bronze_avian` | Casco | `personajes/equipamento/cascos/` | `Head` (`cabeza`) | — |
| `helm_scythian_scale` | Casco | `personajes/equipamento/cascos/` | `Head` (`cabeza`) | — |
| `helm_iron_spangenhelm` | Casco | `personajes/equipamento/cascos/` | `Head` (`cabeza`) | — |
| `armor_tunic` | Pechera (Base) | `personajes/equipamento/chalecos/` | `Spine2` (`pecho`) — *soporta Texture Swapping* | — |
| `armor_leather_cuirass` | Pechera | `personajes/equipamento/chalecos/` | `Spine2` (`pecho`) | — |
| `armor_scale` | Pechera | `personajes/equipamento/chalecos/` | `Spine2` (`pecho`) | — |
| `armor_lamellar_bronze` | Pechera | `personajes/equipamento/chalecos/` | `Spine2` (`pecho`) | — |
| `armor_saka_pectoral` | Pechera | `personajes/equipamento/chalecos/` | `Spine2` (`pecho`) | — |
| `armor_iron_jerkin` | Pechera | `personajes/equipamento/chalecos/` | `Spine2` (`pecho`) | — |
| `belt_plaque_wolf` | Cinturón | `personajes/equipamento/armadura_cinturon/` | `Hips` (`cintura`) | — |
| `skirt_lamellar_tassets` | Faldón | `personajes/equipamento/faldas/` | `Hips` (`cintura`) | — |
| `boots_felt_riding` | Botas | `personajes/equipamento/piernas/` | `LeftFoot` / `RightFoot` (`piernas`) | — |
| `boots_leather_buckles` | Botas | `personajes/equipamento/piernas/` | `LeftFoot` / `RightFoot` (`piernas`) | — |
| `greaves_bronze_anatomical` | Grebas | `personajes/equipamento/grebas/` | `LeftLeg` / `RightLeg` (`grebas`) | — |
| `wpn_akinakes` | Arma Corta | `equipment/` | `RightHand` (`mano_derecha`) | `Hips` (vaina cadera izq.) |
| `wpn_ceremonial_sword` | Arma Corta | `equipment/` | `RightHand` (`mano_derecha`) | `Hips` (vaina cadera izq.) |
| `wpn_steppe_axe` | Arma Corta | `equipment/` | `RightHand` (`mano_derecha`) | `Hips` (anilla cinturón) |
| `wpn_bronze_mace` | Arma Corta | `equipment/` | `RightHand` (`mano_derecha`) | `Hips` (anilla cinturón) |
| `wpn_cavalry_saber` | Arma Larga | `equipment/` | `RightHand` (`mano_derecha`) | `Hips` (vaina curva izq.) |
| `wpn_hallstatt_sword` | Arma Larga | `equipment/` | `RightHand` (`mano_derecha`) | `Spine2` (espalda diagonal) |
| `wpn_steppe_spear` | Asta | `equipment/` | `RightHand` (`mano_derecha`) | `Spine2` (espalda vertical) |
| `wpn_ordos_halberd` | Asta | `equipment/` | `RightHand` (`mano_derecha`) | `Spine2` (espalda vertical) |
| `wpn_self_bow` | Arco | `equipment/` | `LeftHand` (`mano_izquierda`) | `Spine2` (cruzado espalda) |
| `wpn_composite_bow` | Arco | `equipment/` | `LeftHand` (`mano_izquierda`) | `Hips` (dentro de *gorytos*) |
| `wpn_gorytos_quiver` | Carcaj | `equipment/` | `Hips` (cadera izquierda / espalda baja) | — |
| `wpn_javelin` | Arrojadiza | `equipment/` | `RightHand` (`mano_derecha`) | `Spine2` (carcaj espalda) |
| `shield_round_nomad` | Escudo | `equipment/` | `LeftForeArm` (`antebrazo_izq`) | `Spine2` (plano en espalda) |
| `shield_wicker_scythian` | Escudo | `equipment/` | `LeftForeArm` (`antebrazo_izq`) | `Spine2` (plano en espalda) |
| `shield_bronze_bossed` | Escudo | `equipment/` | `LeftForeArm` (`antebrazo_izq`) | `Spine2` (plano en espalda) |
| `shield_hide_pelt` | Escudo | `equipment/` | `LeftForeArm` (`antebrazo_izq`) | `Spine2` (plano en espalda) |
| `item_waterskin` | Quickbar | `utileria/` | `RightHand` (`mano_derecha`) | `Hips` (colgado en cinturón) |
| `item_lasso` | Quickbar | `utileria/` | `LeftHand` (rollo) + `RightHand` (lanzamiento) | `Hips` (enrollado en cintura) |
| `item_grappling_hook` | Quickbar | `utileria/` | `RightHand` (`mano_derecha`) | `Hips` (cinturón) |
| `item_bandage` | Quickbar | `utileria/` | `RightHand` (`mano_derecha`) | Oculto en inventario |
| `item_rations` | Quickbar | `utileria/` | `RightHand` (`mano_derecha`) | Oculto en inventario |
| `item_torch` | Quickbar | `utileria/` | `RightHand` o `LeftHand` | `Hips` (apagada en cintura) |
| `item_pickaxe` | Quickbar | `utileria/` | `RightHand` (`mano_derecha`) | `Spine2` (espalda) |
| `item_saddle_harness` | Montura | `utileria/` | `Spine` del rig de montura (`caballo` / `dromedario`) | Suelo / campamento |
| `accesorio.arreo.placa_arreo_padre` | Reliquia Arreo | `utileria/` | `Chest` / petral del rig de montura | — |
| `accesorio.amuleto.ciervo` | Amuleto | `personajes/equipamento/gorjales/` | `Neck` (`cuello` / clavícula) | — |
| `item_cauldron_bronze` | Campamento | `utileria/` | Estático en mundo (`camp.c`, origen base `Y=0`) | — |
| `item_iron_shackles` | Calabozo | `utileria/` | Estático en mundo o muñecas (`LeftHand`/`RightHand`) | — |
| `item_supply_chest` | Campamento | `utileria/` | Estático en mundo (`camp.c`, origen base `Y=0`) | — |

---

## 3. Integración en los Cuerpos del Juego (Arquitectura Híbrida C11)

1. **Regla estricta de rendimiento (`Zero Malloc`):**
   - Nunca reservar memoria dinámica (`malloc`, `calloc`, `realloc`) ni cargar modelos/texturas dentro del bucle de renderizado o simulación. Toda la gestión de equipamiento usa arreglos estáticos de capacidad fija en `src/entities/character.c`, `src/entities/equipment.c` y `src/entities/player_combat.c`.
2. **Personaje Principal (Kaelen — *Bone Rig* + *Modular Attachments*):**
   - Kaelen es la única entidad humanoide con esqueleto articulado activo en tiempo de ejecución (`assets/models/personajes/rigs/base_personaje.glb`).
   - Cada pieza equipada consulta la matriz del hueso destino en el fotograma actual mediante `GetModelBoneTransform(model, boneIndex)`, multiplica su *offset* local de calibración (traslación/rotación/escala específica del slot) y se dibuja con `DrawModelEx`.
   - **Intercambio de Texturas (*Texture Swapping*):** Sobre `armor_tunic.glb` (y metales compatibles), reutilizar la misma geometría intercambiando `model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture` con las texturas existentes en `assets/models/personajes/texturas/` (`felt_diffuse.png`, `leather_diffuse.png`, `bronze_diffuse.png`, `iron_diffuse.png`, `steel_diffuse.png`, `gold_diffuse.png`).
3. **NPCs y Fauna (*Vertex Animation Textures* — `vat_renderer.c`):**
   - Los NPCs de la horda, soldados imperiales y fauna se renderizan mediante VAT e instanciación (`DrawMeshInstanced`) sin esqueleto en CPU. No convertir los NPCs a *Bone Rig* para mostrar equipamiento; usar variantes estáticas pre-combinadas o *Texture Swapping* por arquetipo.

---

## 4. Movimiento y Transformación de los Modelos durante Acciones

Al implementar o ajustar las máquinas de estado en `src/entities/player_combat.c` y `assets/animaciones.tsv`, cada modelo acoplado debe comportarse de la siguiente manera:

### A. Estados de Enfundado y Desenfundado (`Tecla 1` Arma, `Tecla 2` Escudo)
- **`WEAPON_SHEATHED` -> `WEAPON_DRAWN` (clip `arma_desenfundar` / `arma_enfundar`):**
  - En estado enfundado, el modelo del arma permanece anclado al *offset* de cadera (`Hips`) o espalda (`Spine2`).
  - Al alcanzar el fotograma clave de agarre de la animación, el *parenting* cambia dinámicamente al hueso `RightHand` (`mano_derecha`) alineando el eje longitudinal del arma con la dirección de los nudillos.
- **`SHIELD_STOWED` -> `SHIELD_EQUIPPED` (clip `escudo_equipar` / `escudo_guardar`):**
  - Guardado: el escudo se orienta plano sobre `Spine2` (espalda).
  - Equipado: se acopla a `LeftForeArm` con la cara exterior apunt