# Voxeles sin conectar

Modelos `.vox` (MagicaVoxel) desde las hojas de tres vistas que generó el usuario con Canva (las imágenes no
se suben). El juego no los carga: hasta que se decida a qué id del inventario va cada uno, se miran con
`python3 tools/voxeles/voxeles.py ver <archivo.vox> -o vista.png` o en MagicaVoxel.

Los voxeles quedan para lo construido y lo de la tierra. Las personas, los animales y las armas van en
low-poly con textura, al estilo de PS2 (`tools/modelos/piezas.py`): los quietos en `assets/models/`, los que se
animan en `arte/modelos/para_riggear/`.

- `estructuras/piedra_ciervo_sol.vox` y `piedra_ciervo_felinos.vox`: otras dos piedras-ciervo, armadas a mano
  como la primera (`tools/voxeles/estructuras.py piedra_ciervo_sol` y `piedra_ciervo_felinos`), que ya es
  `mapa.hito.piedra_ciervo`. Sin id.

Ya conectados, armados a mano con `tools/voxeles/estructuras.py` desde las mismas hojas (en `assets/models/`):
la atalaya (`estructura.campamento.atalaya`), el muro en ruinas (`estructura.ruina.muro`), la carreta
(`vehiculo.tierra.carreta_mercancias`), el carro de varas (`vehiculo.tierra.carreta_bueyes`, la carreta del
campamento; sus ruedas pedirán el clip `rodar` cuando se mueva), la puerta fortificada
(`estructura.imperio.puerta`), el árbol frondoso (`mapa.vegetacion.roble`) y el sauce
(`mapa.vegetacion.sauce`). La hoja que el lote llamó abeto es la del pino (`mapa.vegetacion.pino`).
