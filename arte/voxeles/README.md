# Voxeles sin conectar

Modelos `.vox` (MagicaVoxel) hechos con `tools/voxeles/voxeles.py hoja` desde las hojas de tres vistas que
generó el usuario con Canva (las imágenes no se suben). El juego no los carga: hasta que se decida a qué id
del inventario va cada uno, se miran con `python3 tools/voxeles/voxeles.py ver <archivo.vox> -o vista.png` o
en MagicaVoxel.

Los voxeles quedan para lo construido y lo de la tierra. Las personas, los animales y las armas van en
low-poly con textura, al estilo de PS2 (`tools/modelos/piezas.py`): los quietos en `assets/models/`, los que se
animan en `arte/modelos/para_riggear/`.

- `estructuras/carreta.vox` y `carro_varas.vox`: los vehículos piden el clip `rodar`.
- `estructuras/atalaya.vox` y `muro_ruina.vox`: hoy los dibuja el kit de edificios; un `.vox` los reemplazaría.
- `estructuras/piedra_ciervo_*.vox`: otras dos piedras-ciervo (la primera ya es `mapa.hito.piedra_ciervo`).
- `estructuras/puerta_fortificada.vox` y `arboles/`: sin un id que les corresponda.
