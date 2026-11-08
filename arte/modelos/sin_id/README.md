# Low-poly con textura, sin id

Personas, animales y equipo al estilo de PS2 (`tools/modelos/piezas.py`: pocas caras, la textura de las tres
vistas, sin luz horneada), desde las hojas de tres vistas que generó el usuario con Canva, que no tienen todavía un
id propio en `assets/inventario.tsv`. Los que lo tienen están en `assets/models/` (los quietos) o en
`arte/modelos/para_riggear/` (los que se animan).

Cada uno se hizo con el presupuesto y las medidas de un id parecido (`piezas.py <id parecido> --salida ...
--sin-inventario`); para pasarlo a un id propio, se rehace con ese id y sin `--salida`.

| Archivo | Id parecido (presupuesto, medidas) | Forma | Nota |
|---|---|---|---|
| `sable.glb` | `arma.corta.sable` | plana, `--girar 90` | sable curvo; la hoja lo dibuja acostado |
| `escudo_oval.glb` | `escudo.mano.lamina` | disco (ovalado) | de tablas con borde y umbo de hierro: no es de láminas |
| `casco_anteojos.glb` | `armadura.casco.malla` | secciones | casco de anteojos (nasal y antiparras), sin malla |
| `casco_conico.glb` | `armadura.casco.escamas_hierro` | secciones | cónico con nasal y carrilleras, sin escamas |
| `hombre_1`, `hombre_3`, `hombre_5`, `mujer_1` a `mujer_5` | `personaje.base.cuerpo_comun` | volumen `--miembros` | 1.70 m, el alto del cuerpo común |
| `caballo_2`, `caballo_4` | `animal.montura.caballo_estepario` | volumen | a la cruz de los más grandes (docs/TAMANOS.md) |
| `lobo_2`, `lobo_3`, `lobo_4` | `animal.salvaje.lobo` | volumen | ídem |
| `oso_polar.glb` | `animal.salvaje.oso` | volumen | ídem, el del oso polar |
| `cierva.glb` | `animal.salvaje.ciervo` | volumen | ídem, el de la cierva |
| `vaca.glb` | `animal.ganado.buey` | volumen, `--voltear-lado` | ídem, el de la vaca; la hoja la dibuja mirando a la derecha |
