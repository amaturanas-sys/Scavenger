# Estilo visual de la interfaz

**Referencia:** orfebrería nómade de **estilo animal** (escita, xiongnu, Altái). El autor aportó cuatro imágenes de referencia:
1. **Placa de cinturón de oro** con un dragón y un tigre en combate, incrustaciones de **turquesa** y un marco con **granulado** y gotas de turquesa, sobre cuero envejecido.
2. **Placa de plata de un ciervo** echado, con las astas terminadas en cabezas de ave (estilo animal clásico).
3. **Placa calada de oro**: un águila ataca a un íbice sobre fondo de terciopelo negro, con roleos y ojos de turquesa.
4. **Hebilla de plata** de un tigre enroscado sobre una correa de cuero agrietado, con cuentas de colores (cornalina, lapislázuli).

Las imágenes no se incluyen en el repositorio: son solo de consulta. Este documento recoge lo que se toma de ellas.

## Principios

- **La interfaz es un objeto de orfebrería.** Paneles, menús y marcos se ven como placas de cinturón: marco de metal, banda de granulado, incrustaciones y fondo de cuero.
- **Oro para lo principal, plata para lo secundario.** El oro marca lo activo o importante (HUD, menú principal, objetos únicos); la plata, lo secundario, lo inactivo o lo deshabilitado.
- **La turquesa es el acento.** Es el color de lo "vivo": barras, selección, foco. La cornalina (rojo) se reserva para peligro y alertas; el lapislázuli, para un segundo valor.
- **Pixel a pixel.** Todo se dibuja a 640×360 sin suavizado. Un gránulo de oro mide 2×2 px y una turquesa engastada, 3×3 px. Nada de degradados suaves.
- **Bestias en combate** como motivo de los emblemas: cabeceras de menú, iconos de facción y pantallas de carga. Se componen como en las placas: animales entrelazados, roleos y siluetas caladas.

## Paleta

Definida en `src/ui/theme.h`.

| Nombre | Uso | RGB |
|---|---|---|
| Oro | Marcos, títulos, valores importantes | 212, 166, 72 |
| Oro claro | Brillos, títulos destacados | 248, 218, 132 |
| Oro oscuro | Contornos, sombras, tabiques | 120, 82, 28 |
| Plata / clara / oscura | Elementos secundarios o inactivos | 192, 192, 184 · 236, 236, 228 · 92, 92, 90 |
| Turquesa / clara / oscura | Incrustaciones, barras, selección | 68, 198, 196 · 150, 232, 224 · 26, 110, 116 |
| Cornalina | Peligro, rebelión, daño | 184, 62, 40 |
| Lapislázuli | Segundo valor (p. ej., lealtad) | 44, 66, 136 |
| Cuero / grieta | Fondo de los paneles | 54, 37, 26 · 33, 22, 15 |
| Terciopelo | Fondo de las barras y huecos calados | 16, 15, 18 |
| Hueso / hueso tenue | Texto principal y secundario | 242, 228, 192 · 200, 186, 152 |

## Componentes (`src/ui/theme.c`)

| Componente | Aspecto | Referencia |
|---|---|---|
| `ui_panel` | Cuero agrietado, marco biselado de 3 px, banda de gránulos de oro alternados con turquesas y filete interior. | Imagen 1 |
| `ui_strip` | Franja de cuero con un filete de metal y gránulos arriba (barra de ayuda inferior). | Imágenes 1 y 4 |
| `ui_bar` | Barra con marco de metal e incrustación **tabicada** (cloisonné): el relleno se ve como piezas de turquesa engastadas. | Imagen 1 |
| `ui_divider` | Línea de granulado con turquesas al centro. | Imagen 1 |
| `ui_text` | Texto en color hueso con sombra de grabado. | — |

Todo se dibuja con primitivas, sin texturas que cargar: es barato en Android y queda nítido al escalar.

## Pendiente

- **Fuente propia** de mapa de bits, con trazo de grabado, que sustituya a la fuente por defecto de raylib.
- **Emblemas pixel art** de bestias en combate para cabeceras y facciones (tigre y dragón, águila e íbice, ciervo). Son assets dibujados a mano; no se generan con código.
- **Marcos calados** para menús grandes: siluetas de animales recortadas en el marco, con el fondo visible a través.
- **Iconos de objetos**: amuletos y arreos con el mismo lenguaje (los arreos del padre del acto III son literalmente estas piezas).
- Texturas del mundo (cuero de yurtas, arreos de caballo) coherentes con esta paleta.
