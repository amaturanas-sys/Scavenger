# Fuentes de assets con licencia abierta

Estas fuentes las encontró Cline en noviembre de 2026 (PR #165). Son pistas, no assets aprobados: antes de traer uno,
hay que confirmar la licencia en su página y revisar que encaje en el juego.

## Antes de traer un asset

- **El estilo:** low-poly mate, estilo PS2, o voxeles ([ESTILO_VISUAL.md](ESTILO_VISUAL.md)). Lo fotorrealista
  (escaneos, texturas PBR de 8k, HDRI) no encaja.
- **El presupuesto:** los triángulos y las medidas de su fila en `assets/inventario.tsv`. Las medidas las decide el
  usuario.
- **El camino:** `python3 tools/modelos/modelos.py traer <id> --url <URL> --fuente otro`. Así queda anotada la
  procedencia en `arte/modelos/models_manifest.json` y el modelo se convierte al estilo del juego. Nada se copia a mano
  en `assets/`.
- **La licencia:**
  - **Sirven** CC0 y CC-BY. Con CC-BY, el crédito va en el README.
  - **No sirven** los assets sin licencia, los de «solo uso personal» ni los de uso no comercial.
- **El motor:** raylib, sin iluminación por imagen. Los HDRI no le sirven.

## Fuentes

| Fuente | Licencia (según su página) | Encaje |
|---|---|---|
| [Low Poly Vegetation Pack](https://opengameart.org/content/low-poly-vegetation-pack) (Proxy Games, OpenGameArt) | CC0 | El mejor candidato: árboles, arbustos, hierba y piedras de 20 a 264 triángulos, con color por vértice. |
| [FREE Remastered: Free Mid Poly Meadows](https://opengameart.org/content/free-remastered-free-mid-poly-meadows) (EmacEArt) | CC0 | 30 modelos en glTF: árboles, cercas, rocas, ramas y raíces. Hay que revisar sus triángulos. |
| [Grassland LowPoly](https://opengameart.org/content/grassland-lowpoly) (jjmoser) | CC-BY 3.0 | Pastizal low-poly. Exige crédito. |
| [Grass Pack #01](https://opengameart.org/content/grass-pack-01) (Yughues) | CC0 | Hierba y vegetación. |
| [Modular grassland 3D tileset](https://opengameart.org/content/modular-grassland-3d-tileset) (rubberduck) | CC0 | Pensado para Godot. Sirve como referencia. |
| [Poly Haven](https://polyhaven.com) | CC0 | Fotorrealista: no encaja. A lo sumo, como referencia de colores. |
| [Polyfork](https://polyfork.dev/assets) | Gratis y con uso comercial, según su sitio (sin verificar) | Low-poly para three.js en GLB, de 160 a 6300 triángulos. Revisar sus términos antes de usarlo. |
| [Sketchfab: Game Ready](https://sketchfab.com/tags/game-ready) | Distinta en cada modelo | Se revisa modelo por modelo, y pide cuenta. |
| [Pixabay](https://pixabay.com/es/) | Pixabay License | Imágenes y sonido de referencia, no modelos. |

## Lo que no entró del PR #165, y por qué

- **El arbusto y la fogata de Poly Haven:**
  - el arbusto medía 7 cm de alto;
  - la fogata pasaba de los 3800 triángulos y repetía `estructura.campamento.fogata`;
  - ninguno tenía textura;
  - el juego no usaba ninguno.
- **Tres HDRI:** sumaban 7,6 MB al APK, y el motor no los usa.
- **Un cuerpo base de primitivas:** reemplazaba a `personaje.base.cuerpo_comun` sin sus clips obligatorios, y el CI
  lo rechazaba. Además, el juego lo habría usado para dibujar a toda la tropa (`src/game/actions_game.c`).
- **Filas de inventario de texturas y de un paquete de 30 modelos:** tenían medidas inventadas (`4x0x4`) y no tenían
  archivo.

Agent-Reach, la herramienta con la que se hizo la búsqueda, no forma parte del flujo:
- busca con servicios de terceros (Exa y Jina), que ven cada consulta;
- para las redes sociales, copia las cookies del navegador.

Para buscar en la web están WebSearch y WebFetch.
