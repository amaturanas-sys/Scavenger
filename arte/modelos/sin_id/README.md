# Low-poly con textura, sin id

Personas y animales al estilo de PS2 (`tools/modelos/piezas.py`: pocas caras, la textura de las tres vistas, sin
luz horneada), desde las hojas de tres vistas que generó el usuario con Canva, para los que todavía no hay un id
en `assets/inventario.tsv` que les corresponda. Los que lo tienen están en `assets/models/` (los quietos) o en
`arte/modelos/para_riggear/` (los que se animan).

Cada uno se hizo con el presupuesto de un id parecido (`piezas.py <id parecido> --salida ... --sin-inventario`);
para pasarlo a un id propio, se rehace con ese id y sin `--salida`.

| Archivo | Id parecido (presupuesto) | Forma | Medida |
|---|---|---|---|
| `hombre_1`, `hombre_3`, `hombre_5` | `personaje.base.cuerpo_comun` | volumen `--miembros` | 1.70 m de alto |
| `mujer_1` a `mujer_5` | `personaje.base.cuerpo_comun` | volumen `--miembros` | 1.60 m de alto (la mujer estándar) |
| `caballo_2`, `caballo_4` | `animal.montura.caballo_estepario` | volumen | 1.58 m a la cruz |
| `oso_polar.glb` | `animal.salvaje.oso` | volumen | 1.60 m a la cruz |
| `cierva.glb` | `animal.salvaje.ciervo` | volumen | 1.20 m a la cruz |
| `vaca.glb` | `animal.ganado.buey` | volumen, `--voltear-lado` | 1.62 m a la cruz (la hoja la dibuja mirando a la derecha) |

Los animales, a la cruz de los ejemplares más grandes de su especie: docs/TAMANOS.md.
