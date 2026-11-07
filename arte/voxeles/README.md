# Voxeles sin conectar

Modelos `.vox` (MagicaVoxel) hechos con `tools/voxeles/voxeles.py hoja` desde las hojas de tres vistas que
generó el usuario con Canva (las imágenes no se suben). El juego no los carga: hasta que se decida a qué id
del inventario va cada uno, se miran con `python3 tools/voxeles/voxeles.py ver <archivo.vox> -o vista.png` o
en MagicaVoxel.

- `personas/` y `animales/`: el juego los anima (piden clips), y un `.vox` es quieto.
- `estructuras/carreta.vox` y `carro_varas.vox`: los vehículos piden el clip `rodar`.
- `estructuras/atalaya.vox` y `muro_ruina.vox`: hoy los dibuja el kit de edificios; un `.vox` los reemplazaría.
- `estructuras/piedra_ciervo_*.vox`: otras dos piedras-ciervo (la primera ya es `mapa.hito.piedra_ciervo`).
- `estructuras/puerta_fortificada.vox`, `armas/` y `arboles/`: sin un id que les corresponda (o que pide clips).

Quedaron afuera las aves (su vista de lado tiene las alas abiertas en perspectiva: sale una placa) y el banco
de peces (no es un objeto).
