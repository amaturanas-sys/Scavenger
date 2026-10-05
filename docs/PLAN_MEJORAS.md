# Plan de mejoras tras la prueba en tablet (v0.1.0)

> **Estado:** hecho en v0.1.1 (fases 1 y 2) y v0.2.0 (fases 3 a 6).
>
> **Quedó para después:**
> - pasar a OpenGL ES 3 (por ahora se sigue en ES 2, con texturas seguras);
> - la reasignación de teclas;
> - la rejilla A\* (los bigotes y la orilla resuelven los lagos);
> - el comercio con las tribus amigables;
> - el botín y los relatos de las ruinas.

Lo que reveló la primera prueba en Android, con sus causas y el trabajo para resolverlo. Cada fase va en su propio PR, con CI en verde. Las fases 1 y 2 salen enseguida como **v0.1.1**, para volver a probar en la tablet; el resto sale como **v0.2.0**.

## Fase 1 — Texturas, menú e iconos en Android (bloqueante)

**Causas encontradas en el código:**

1. **`FileExists()` antes de cargar.** En Android los assets viven dentro del APK, y `FileExists()` mira el sistema de archivos, así que no los ve. Por eso no se cargan:
   - los iconos (`src/ui/icons.c:22`);
   - el fondo y las láminas del menú (`src/game/title_menu.c:137`);
   - la galería (`src/world/gallery.c:65`);
   - parte de los modelos (`src/world/props.c:92`).
2. **Texturas que no son potencia de 2 en OpenGL ES 2.** Por ejemplo `iconos.png` (512×288), `fondo_titulo.png` (640×360), el emblema y las láminas. Con repetición o mipmaps, ES 2 las trata como *incompletas* y salen negras o invisibles. Además, `GenTextureMipmaps` falla sobre ellas.

**Trabajo:**
- `platform_asset_exists()`: en Android consulta el `AAssetManager`; en PC, `FileExists`. Reemplaza todos los `FileExists` de assets.
- Compilar raylib para Android con **OpenGL ES 3** (manifest `glEsVersion 0x30000`), que acepta texturas que no son potencia de 2. Además, por seguridad:
  - rellenar los atlas a potencia de 2 (iconos a 512×512) en sus scripts de `tools/assets`;
  - fijar *clamp* sin mipmaps en las texturas que no lo sean.
- `tools/assets/texturas_check.py`: el CI falla si una textura del juego no es potencia de 2 o pasa de 2048.
- Un registro de los assets que no cargan, visible con `adb logcat -s raylib`, y una pantalla de **diagnóstico** en el menú: GPU, versión de GL, texturas y modelos cargados o fallidos.

## Fase 2 — Controles con teclados de tablet

**Problema:** muchos teclados nativos de tablet no envían `Esc` (lo convierten en «Atrás» o el sistema se lo queda) y no tienen F1 ni F5.

**Trabajo:**
- **Entrada centralizada** (`src/game/input.*`): acciones (pausa, controles, vista orbital, menú…) con varias teclas cada una, en lugar de `IsKeyPressed(KEY_…)` repartidos.
- **Pausa:** `Esc`, **`KEY_BACK`** (botón Atrás o el Esc de esos teclados) y `P`.
- **Teclas sin F:** controles con `F1` o `Ctrl+H`; vista orbital con `F5` o `Ctrl+M`. Se revisan antes las teclas libres.
- **Botones táctiles discretos:** pausa y menú en una esquina, siempre visibles en Android. Los menús ya aceptan toques como clics: se verificará.
- **«Probar teclado»** en Opciones: muestra el código de cada tecla pulsada, para detectar teclas que no llegan.
- **Reasignación de teclas,** guardada en los ajustes.
- **Aviso de teclado:** un botón «Jugar igual» por si la tablet no informa su teclado como QWERTY.

## Fase 3 — Movimiento de animales y NPCs (pathing)

**Problema:** los animales terrestres entran en los lagos.

**Trabajo:**
- **Transitable por especie:** profundidad del agua, pendiente, muros, borde del mundo y obstáculos (rocas, árboles, edificios).
  - Los terrestres nunca pasan de 0,3 m de agua.
  - Los vados de los ríos son puntos de cruce.
- **Dirección con «bigotes»:** sondeos por delante que esquivan el agua y lo empinado. Si el destino cae en el agua, se lleva a la orilla más cercana.
- **Beber:** con sed, buscan un punto de orilla (agua de 0 a 0,15 m junto a tierra seca), van hasta él y beben mirando al agua, sin entrar.
- **Rejilla de navegación gruesa (4 m) con A\*** en la zona cargada, para quien se atasca. Si alguien no avanza en N segundos, vuelve a planificar.
- **NPCs humanos** (jinetes, tribus, caravanas) con el mismo sistema y caminos preferidos entre asentamientos.
- **Tests en `sim_tests`:**
  - ningún animal terrestre termina en agua honda tras horas simuladas;
  - un animal con sed llega a la orilla y bebe;
  - nadie queda atascado.

## Fase 4 — Densidad del mundo

**Vegetación:**
- Matas y arbustos, manchas de pasto alto y flores de estepa.
- Sauces y álamos junto a los ríos; abedules y alerces en las faldas.
- Bosque más tupido.
- Todo por chunk, en mallas baratas, con menos detalle a lo lejos.

**Rocas:**
- Cantos rodados, afloramientos, pedreros del altiplano y rocas de la costa.
- Con colisión simple.

**Ríos:**
- Arroyos tributarios de 2 a 4 m que bajan de cumbres y lagos a los ríos grandes, también en la estepa hacia los meandros.
- `RIVERS_MAX` de 24 a 64, con niveles siempre bajando.

**Animales:**
- Más manadas y más grandes según el bioma: caballos salvajes, saigas, ciervos, marmotas, aves.
- Un presupuesto de aparición alrededor del jugador, para cuidar el rendimiento en la tablet.

**Rendimiento:** medir los FPS en el build de Android con un contador en el diagnóstico y ajustar las densidades.

## Fase 5 — Tribus y estructuras

**Tribus con actitud: rivales, neutrales y amigables.**
- Campamentos de yurtas por región, con relación dinámica (reputación).
- Las amigables comercian y dan noticias; las neutrales reaccionan según la reputación; las rivales emboscan y asaltan.
- Patrullas y caravanas entre asentamientos.

**Estructuras:**

| Tipo | Ejemplos |
|---|---|
| **Ruinas** | Kurganes, balbales (estelas), piedras de ciervo, fortalezas derruidas, ciudades enterradas en la arena, petroglifos. |
| **En uso** | Caravasares, torres de vigía, ovoos (altares de piedra), pozos, puentes, puertos en los fiordos. |

**Estilos por región:**

| Región | Estilo |
|---|---|
| Estepa | Fieltro y yurtas |
| Bosque | Madera |
| Altiplano | Piedra |
| Desierto | Adobe |
| Fiordos | Casas largas |

**Colocación y contenido:**
- Las coloca `world.c` con reglas de distancia, terreno llano y seco.
- Aparecen en el mapa y en la vista orbital.
- Tienen botín y relatos.

## Fase 6 — Nubes

**Problema:** las nubes actuales son bollos lisos.

**Trabajo:**
- Un **atlas de nubes** generado a partir de fotografías de nubes reales, pixelado low-res con paleta limitada, como las texturas del suelo: cúmulos, estratos, cirros y nubarrones de tormenta.
- **Impostores:** planos con la textura orientados a la cámara, con varios tamaños por nube y profundidad para que las cumbres sigan entrando en ellas.
- **Sombreado según el sol:** lado iluminado y lado en sombra, y color cálido al ocaso.
- **Tipo según el clima:** despejado, nublado o tormenta.

## Orden

1. Fase 1 + Fase 2 → **v0.1.1**: se prueba de nuevo en la tablet.
2. Fase 3: pathing.
3. Fase 4: densidad.
4. Fase 5: tribus y estructuras.
5. Fase 6: nubes.
6. → **v0.2.0**, con capturas y documentación al día.
