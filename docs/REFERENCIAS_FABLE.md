# Referencias de awesome-fable-5.2

Lista curada de juegos, mundos 3D y demos web creados con Claude Fable 5.2.
Fuente: https://github.com/fengyiqicoder/awesome-fable-5.2 (web: fablelist.000ooo.ooo)
Licencia de la lista: CC0-1.0. Cada obra pertenece a su creador.

Este documento es solo referencia. No modifica código del juego ni añade dependencias.

## Secciones del repositorio

- **Juegos** (all/games.md): 162 obras vivas con enlace y prompt de referencia.
- **Mundos 3D y escenas**: entornos tridimensionales explorables en el navegador.
- **Páginas web y herramientas**: demos interactivas y utilidades.
- **Animación y film (con fuente)**: cortos animados con código disponible.
- **Demos solo video**: 117 demos con GIF y enlace al post original.

## Mecánicas e ideas aplicables a Scavenger

| Obra | Mecánica / idea | Relevancia para estepa |
|---|---|---|
| Saltline — first-person shooter | FPS construido en 8 horas con prompts one-shot, modelos, sonidos y texturas generados en código (~$550). | Demuestra que un juego jugable puede nacer de prompts iterados; útil como referencia de alcance para prototipos rápidos. |
| Oil Paint — hand-painted game | Cada frame ~1.5 millones de pinceladas, **200 fps en una pestaña del navegador**. Código abierto. | Referencia de rendimiento extremo en render web; técnicas de optimización de dibujado a estudiar para el render 640×360 de estepa. |
| CROSSTOWN — open-world New York | Mundo urbano abierto en progreso; feedback rápido con tecla B. | Ejemplo de mundo abierto incremental y carga por zonas. |
| Mirror Earth | Planeta entero como mundo destruible con clima real en tiempo real. | Referencia de escala y clima dinámico (estepa tiene estaciones y clima). |
| What the Water Keeps | Paseo en barco por 8 mundos oníricos jugables en el navegador. | Transiciones entre biomas/ambientes; podría inspirar viajes por la estepa. |
| Reef Dive | Juego de buceo en three.js hecho en un día para sentirse como un juego más grande. | Referencia de cómo pocos elementos pueden sugerir un mundo amplio. |
| Cozy Bakery | Cocina cooperativa 1-4 jugadores, funciona en móvil. | Mecánicas de cooperativo local; posible referencia para multijugador en estepa (si se considera). |
| Roost | Carreras de moto de tierra 3D en un chat, 5 prompts, 15 minutos, 3 tracks, rivales IA. | Referencia de generación rápida de contenido de carreras; podría aplicarse a caravanas o persecuciones a caballo. |

## Optimizaciones de rendimiento documentadas

- **Oil Paint**: 200 fps en el navegador con millones de pinceladas por frame. Técnica de render optimizado a investigar.
- **prompt2rig / hq/worker.py** (referencia cruzada): speedups del worker de mocap (YOLO11m batched, ViTPose sin flip-test, resample 24→30 fps con ffmpeg ultrafast, long side 854px, GPU en un solo hilo calentado). Útil solo como referencia de optimización de pipelines de video/poses, no para integrar en el juego.

## Prompts de referencia

Cada entrada de la lista incluye un enlace "Get the prompt" o "Creator's prompt (free)" que apunta al prompt original del creador en la red social de origen. Pueden usarse como plantilla para generar prototipos rápidos de mecánicas o efectos visuales sin escribir código desde cero.

## Cómo usar estas referencias

1. Explorar fablelist.000ooo.ooo para buscar obras por categoría o likes.
2. Abrir el enlace vivo de una obra y su prompt de referencia.
3. Si una mecánica interesa para estepa, proponerla como tarea separada con plan de implementación en `src/sim/` o `src/game/`, respetando las reglas del repositorio (sin sistemas paralelos, sin bibliotecas nuevas en C).

## Limitaciones

- La lista indexa obras externas; no incluye el código completo de cada una.
- Algunas demos son solo video (no hay enlace vivo jugable).
- No afiliado con Anthropic; las obras pertenecen a sus creadores.