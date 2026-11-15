# Assets descubiertos con Agent-Reach

Descubrimiento de assets libres de copyright para texturas, imágenes, modelos 3D y referencia de movimiento, realizado con
**Agent-Reach 1.6.1** (CLI/MCP Python, MIT, instalación vía `uv tool install` desde GitHub, no desde PyPI).
Canales activos por defecto: github, youtube, bilibili, v2ex, web.

Este documento es solo un catálogo de referencias. No modifica código del juego, no añade dependencias ni crea sistemas
paralelos. Los assets que se quieran usar deben pasar por el pipeline oficial:
`tools/fetch_pipeline_asset.py` → `tools/modelos/modelos.py traer` → `assets/inventario.tsv` → `modelos.py check`.

## 1. OpenGameArt.org (búsqueda vía Agent-Reach `web search`)

| Asset | URL | Licencia | Tipo | Tris aprox. | Notas |
|---|---|---|---|---|---|
| Grassland LowPoly | https://opengameart.org/content/grassland-lowpoly | CC-BY 3.0 | 3D Art / vegetación | — | Pastizal low poly hecho en Blender para el juego Barbaros. Autor: Moser Juan José (jjmoser). |
| Low Poly Vegetation Pack | https://opengameart.org/content/low-poly-vegetation-pack | CC0 | 3D Art | Stone 68, Tree 94-218, Bush 264, Grass 20-100 | Pack de Proxy Games. Árboles, arbustos, hierba, piedras. Hecho en Blender. Paleta de colores por vértice. |
| FREE Remastered - Free Mid Poly Meadows | https://opengameart.org/content/free-remastered-free-mid-poly-meadows | CC0 | 3D Art | — | 30 modelos: 7 árboles, 3 cercas madera, 7 cercas pértiga, 11 rocas, 3 ramas, 6 raíces, 3 texturas de suelo (grass, sand, cliff), 1 material. Autor: EmacEArt. Formatos: fbxs.zip, gltf.zip, .blend. |
| Grass Pack #01 | https://opengameart.org/content/grass-pack-01 | CC0 | 3D Art | — | 28 packs de hierba/vegetación. Autor: Yughues. |
| modular grassland 3D tileset | https://opengameart.org/content/modular-grassland-3d-tileset | CC0 | 3D Art / tileset | — | Tileset de pastizal modular para Godot 3 con meshlibrary. Autor: rubberduck. Incluye XCF de GIMP y proyecto de ejemplo. |

**Observación:** OpenGameArt.org bloqueó el acceso directo con `fetch_web_content` (HTTP 403), pero Agent-Reach pudo leerlo mediante su backend de búsqueda. Los detalles arriba provienen del resultado de búsqueda de Agent-Reach.

## 2. Poly Haven (polyhaven.com)

- **Qué es:** Librería pública de assets 3D (antes HDRI Haven, Texture Haven, 3D Model Haven unificados).
- **Modelo de licencia:** 100% gratis, **CC0** (uso sin restricciones, sin paywalls, sin registro).
- **Producción:** Hecho por humanos, sin IA generativa.
- **Categorías:**
  - **HDRIs:** 16k+ resolución, sin recortar, para iluminación realista de exteriores.
  - **Texturas:** PBR sin costuras, 8k+ resolución.
  - **Modelos:** Modelos hiperrealistas para VFX y videojuegos next-gen.
- **Relevancia para estepa:** HDRIs de exterior para iluminación del mundo, texturas PBR de suelo (pasto, tierra, roca), modelos de vegetación.
- **Descarga:** Directa desde el sitio, sin cuenta.

## 3. Pixabay (pixabay.com/es)

- **Qué es:** Banco de medios libres de copyright.
- **Tipos de contenido:** Imágenes, vectores, música, videos.
- **Licencia:** Pixabay License (uso libre, incluso comercial, sin atribución requerida aunque se agradece).
- **Relevancia para estepa:** Imágenes de referencia para texturas pintadas a mano, concept art de estepa, música de ambiente, efectos de sonido.
- **Nota:** No se pudo leer directamente con `fetch_web_content` (HTTP 403); se recomienda explorar manualmente o mediante Agent-Reach cuando el rate limit de Exa lo permita.

## 4. Limitaciones del descubrimiento automatizado

- El backend de búsqueda web de Agent-Reach usa Exa con rate limit gratuito muy bajo. Tras la primera búsqueda exitosa, las siguientes búsquedas devolvieron `RATE_LIMITED` (espera de 3565 segundos).
- Para seguir descubriendo assets, se recomienda usar la búsqueda web integrada del agente o esperar a que se restablezca el rate limit, o bien agregar una clave API de Exa (`https://dashboard.exa.ai/api-keys`).
- Algunos sitios (OpenGameArt.org, Pixabay) bloquean el acceso directo desde herramientas de lectura; Agent-Reach los lee a través de su propio backend.

## 5. Pasos siguientes para usar un asset en el juego

1. Elegir un asset de la tabla anterior o buscar uno nuevo con Agent-Reach.
2. Descargar el archivo (GLB, FBX, PNG, etc.).
3. Si es un modelo 3D: asignar un id en `assets/inventario.tsv` (categoría válida: mapa, estructura, vehiculo, animal, arma, etc.).
4. Ejecutar `python3 tools/fetch_pipeline_asset.py --url <URL> --name <ID> --category <CAT>` o `tools/modelos/modelos.py traer <id> --url <URL>`.
5. Ejecutar `python3 tools/modelos/modelos.py check` y `python3 tools/assets/inventario.py check` para validar.
6. El juego carga el modelo por su id; no se necesita tocar código C.

## Capturas de pantalla

No se generaron capturas de pantalla de los sitios externos porque el entorno no dispone de navegador gráfico automatizado para sitios web (solo Xvfb para el cliente del juego). Los enlaces y detalles anteriores sirven como referencia.