# Capturas: las pantallas del juego desde su menú

Todas a 640x360, la resolución real del juego (en pantalla se escala con píxeles nítidos). Se regeneran con:

```bash
tools/capturas.sh          # usa build/; necesita xvfb-run en Linux
```

Los menús usan **iconos** (siluetas del atlas [`assets/ui/iconos.png`](../assets/ui/iconos.png), editable: ver [ESTILO_VISUAL.md](ESTILO_VISUAL.md#iconos)). El nombre de cada botón aparece, discreto, en la **leyenda de la base de la pantalla** al pasar el ratón (o el dedo) por encima, o al elegirlo con el teclado.

## Entrada

| | |
|---|---|
| ![Título](capturas/01_titulo.png) | **Título** (`--menu`): nueva partida, cargar, instructivo, salir. |
| ![Cargar](capturas/02_cargar.png) | **Cargar partida** (`--huecos`): tres huecos con minifoto, fecha, día y tribu. |
| ![Instructivo](capturas/03_instructivo.png) | **Instructivo** (`--instructivo`): láminas con las reglas; botones de página anterior, volver y siguiente. |

## En la partida

| | |
|---|---|
| ![Juego](capturas/04_juego.png) | **El juego**: HUD compacto, brújula, clima y salud. |
| ![Controles](capturas/05_controles.png) | **Controles** (F1, `--controles`). |
| ![Pausa](capturas/06_pausa.png) | **Pausa** (Esc, `--pausa`): continuar, guardar, cargar, instructivo, al título, salir. |
| ![Guardar](capturas/07_guardar.png) | **Guardar** (`--guardar`): la minifoto es la partida al pausar. |

## Menú de acciones (L): acciones, obras, fabricar, reparar

| | |
|---|---|
| ![Acciones](capturas/08_acciones.png) | **Acciones** (`--pestana 0`). |
| ![Obras](capturas/09_obras.png) | **Obras en grupo** (`--pestana 1`): las grises no se pueden aún (falta oficio o cuadrilla). |
| ![Fabricar](capturas/10_fabricar.png) | **Fabricar** (`--pestana 2`): punto turquesa = a mano; los ingredientes, como iconos con lo que tienes / lo que hace falta. |
| ![Reparar](capturas/11_reparar.png) | **Reparar** (`--pestana 3`): la barra al pie es el estado de la pieza. |

## Inventario y equipo

| | |
|---|---|
| ![Inventario](capturas/12_inventario.png) | **Inventario** (Tab, `--inventario`): dos columnas; arriba, los contenedores a mano (bolsillos, mochila, alforjas, carreta, campamento). |
| ![Equipo](capturas/13_equipo.png) | **Equipo** (P, `--equipo`): los nueve huecos de armadura alrededor de la figura y los tres amuletos; a la derecha, sus cifras y la salud. |

## Combate y mundo

| | |
|---|---|
| ![Jinetes](capturas/14_jinetes.png) | **Jinetes bandidos** (`--enemigos jinetes`). |
| ![Botín](capturas/15_botin.png) | **Bolsas de botín** (`--botin`). |
| ![Galería](capturas/16_galeria.png) | **Galería de modelos** (`--galeria`): todos los objetos del inventario a escala. |

### Las manos

Los cuatro botones (`H I J K`, abajo a la derecha) según lo que está fuera; la espada y el escudo se guardan por separado (`1` y `2`, [PLAN_TACTICO_Y_CUEVAS.md](PLAN_TACTICO_Y_CUEVAS.md), A1). El montaje: [`capturas/las_manos.png`](capturas/las_manos.png).

| | |
|---|---|
| ![Escudo solo](capturas/manos_escudo.png) | **El escudo solo** (`--enemigos bandidos --manos escudo`): la espada al cinto; puñetazo, parry con el escudo, cubrirse y carga. |
| ![Espada sola](capturas/manos_espada.png) | **La espada sola** (`--manos espada`): el escudo a la espalda; golpe, parry, parar con el arma y patada. |
| ![Nada](capturas/manos_nada.png) | **Todo guardado** (`--manos nada`): puñetazo, la llave y patada; sin bloqueo. |
| ![Rehén](capturas/manos_rehen.png) | **Con un rehén** (`--enemigos bandidos --rehen`): `H` lo ejecuta, `I` lo empuja, `G` lo suelta. |

### La barra, el objetivo y la cobertura

El resto del sistema táctico ([PLAN_TACTICO_Y_CUEVAS.md](PLAN_TACTICO_Y_CUEVAS.md), A2 a A4):

| | |
|---|---|
| ![Barra](capturas/tactico_barra.png) | **La barra rápida desde el inventario** (`--inventario`): abajo, la fila de las diez casillas (`1` a `0`); un número, o un toque en la fila, pone ahí lo elegido. |
| ![Objetivo](capturas/tactico_objetivo.png) | **El objetivo** (`--enemigos bandidos --objetivo`, como pulsar `O`): el elegido lleva la barra de vida con marco dorado. |
| ![Cobertura](capturas/tactico_cobertura.png) | **A cubierto** (`--cubierto --enemigos bandidos`): `C` junto a una roca del mundo; «OCULTO» arriba y lo que se puede hacer al pie. |

## Las cuevas

Una por guarida ([PLAN_TACTICO_Y_CUEVAS.md](PLAN_TACTICO_Y_CUEVAS.md), B1 a B3). Con `--ir boca-<tipo>` (afuera) y
`--cueva tipo[:lugar]` (adentro; `--antorcha`, encendida; `--cueva-f`, como pulsar F). El montaje:
[`capturas/las_cuevas.png`](capturas/las_cuevas.png).

| | |
|---|---|
| ![Boca de forajidos](capturas/cueva_boca_forajidos.png) | **La boca de un refugio de forajidos** (`--ir boca-forajidos`): el peñasco, el túnel, las antorchas y la estacada; sus guardias salen a pelear. |
| ![Boca de fieras](capturas/cueva_boca_fieras.png) | **La boca de una guarida de fieras** (`--ir boca-animal`). |
| ![Boca del imperio](capturas/cueva_boca_imperio.png) | **La boca de un almacén del imperio** (`--ir boca-imperio`): piedra labrada, estandartes y la guardia imperial. |
| ![Guarida](capturas/cueva_fieras.png) | **Adentro de una guarida de fieras** (`--cueva animal:nido --antorcha`): el nido con sus huevos, a la luz de la antorcha. |
| ![Cisterna](capturas/cueva_habitada.png) | **La Cisterna de los forajidos** (`--cueva forajidos:0 --antorcha`): la fogata, la columna de ladrillo, las pinturas y su antorcha, una telaraña y los charcos. |
| ![Prisionero](capturas/cueva_prisionero.png) | **Un prisionero** (`--cueva forajidos:dialogo`): lo que dice, si se lo ve sincero, y romper las cadenas o dejarlo. |
| ![Rescate](capturas/cueva_rescate.png) | **Se une a la tribu** (`--cueva imperio:dialogo --cueva-f`): la tropa pasa de 7 a 8 y la celda queda abierta. |
| ![Traición](capturas/cueva_traicion.png) | **Te traiciona** (`--cueva forajidos:dialogo --cueva-f`): te golpea, te roba tres virotes y huye. |
| ![A oscuras](capturas/cueva_oscura.png) | **Una recámara a oscuras** (`--cueva abandonada:1`): sin antorcha se ve apenas alrededor. |
| ![Con la antorcha](capturas/cueva_antorcha.png) | **La misma, con la antorcha** (`--antorcha`). |

## El mundo

Ver [MUNDO.md](MUNDO.md). Con `--ir lugar` y `--orbital`:

| | |
|---|---|
| ![Orbital](capturas/orbital_1206_30.png) | **Vista orbital** (F5, `--orbital 30 0.5`): el gran círculo con sus cinco regiones, el canal, el mar y el muro; torres doradas: capitales; blancas: aldeas; rojas: guaridas; turquesa: el jugador. |
| ![Bosque](capturas/mundo_bosque.png) | **Bosque de coníferas** (`--ir bosque`). |
| ![Altiplano](capturas/mundo_altiplano.png) | **Altiplano glaciar** (`--ir altiplano`). |
| ![Mar](capturas/mundo_mar.png) | **Costa de fiordos y el mar** (`--ir mar`). |
| ![Muro](capturas/mundo_muro.png) | **El cañón y el muro del desierto** (`--ir muro`). |
| ![Canal](capturas/mundo_canal.png) | **El gran canal** (`--ir canal`). |
| ![Capital](capturas/mundo_capital.png) | **Una capital** (`--ir capital`): la del Principado de los Pinos Negros. |
| ![Aldea](capturas/mundo_aldea.png) | **Una aldea** del desierto (`--ir aldea`). |
| ![Guarida](capturas/mundo_guarida.png) | **Una guarida** (`--ir guarida`). |
| ![Lago](capturas/mundo_lago.png) | **Un lago** (`--ir lago`): el más grande fuera del bosque, del otro lado de su desborde ([AGUA.md](AGUA.md)). |
| ![Desborde](capturas/mundo_desborde.png) | **El desborde de un lago** (`--ir desborde`): por ahí sale su río. |
| ![Cascada](capturas/mundo_cascada.png) | **Una cascada** (`--ir cascada`): la más grande tierra adentro, desde abajo. |
| ![Lengua](capturas/mundo_lengua.png) | **El pie del glaciar** (`--ir lengua`): la morrena, lo más bajo junto al muro de hielo. |

### Las estaciones del agua

El mismo lugar en dos momentos del año ([AGUA.md](AGUA.md), fase 2), con `--dia` (la primavera empieza el día 1;
el verano, el 8; el otoño, el 15; el invierno, el 22):

| | |
|---|---|
| ![Meandro en otoño](capturas/estacion_meandro_otono.png) | **Un río del glaciar** (`--dia 17 --ir meandro`): todavía con agua. |
| ![Meandro seco](capturas/estacion_meandro_invierno.png) | **El mismo, seco** (`--dia 20`): con la helada, el glaciar no suelta agua y queda el cauce de cantos. |
| ![Lago en otoño](capturas/estacion_lago_otono.png) | **Un lago glaciar** (`--dia 21 --ir lago --dron 30`): los grandes se hielan después que los chicos. |
| ![Lago helado](capturas/estacion_lago_invierno.png) | **El mismo, helado** (`--dia 25`). |
| ![Mar en deshielo](capturas/estacion_mar_deshielo.png) | **El mar al romperse su hielo** (`--dia 5 --ir mar`): el hielo de la costa y los témpanos a la deriva. |
| ![Mar helado](capturas/estacion_mar_invierno.png) | **El mar helado en la costa** (`--dia 26`). |
| ![Lengua en primavera](capturas/estacion_lengua_primavera.png) | **La lengua del glaciar** (`--dia 6 --ir lengua`): cubre la morrena. |
| ![Lengua en verano](capturas/estacion_lengua_verano.png) | **La morrena en el verano** (`--dia 9`): el glaciar se retiró. |

### Hielo, nado y pesca

La tribu en el agua y la pesca en el hielo ([AGUA.md](AGUA.md), fase 3), con `--trampa`:

| | |
|---|---|
| ![Nado](capturas/agua_nado.png) | **La escolta cruza un río a nado** (`--dia 11 --ir meandro --trampa nado`): con la cabeza afuera, hacia el jugador en la orilla. |
| ![Orilla](capturas/agua_orilla.png) | **Y sale a la orilla** (el mismo, unos segundos después). |
| ![Hundido](capturas/agua_hundido.png) | **Un compañero rompió el hielo** (`--dia 25 --ir lago --trampa hundido`): espera, con la cabeza afuera, a que lo saquen (F). |
| ![Pesca](capturas/agua_pesca.png) | **El agujero para pescar** (`--dia 25 --ir lago --trampa agujero`): abierto con el picahielo sobre el lago helado. |

### Cómo se ve el agua

El shader del agua ([AGUA.md](AGUA.md), fase 4), en verano (`--dia 11`):

| | |
|---|---|
| ![Corriente](capturas/agua_corriente.png) | **La corriente de un río** (`--ir meandro --trampa nado`): la superficie avanza aguas abajo, y quien nada lleva su anillo de espuma. |
| ![Olas](capturas/agua_olas.png) | **Las olas del mar en las rocas** (`--ir mar --dron 6 --camara 0.45`): el oleaje, y la espuma que va y viene en la costa y el basalto. |
| ![Lago](capturas/agua_lago.png) | **Un lago glaciar** (`--ir desborde`): el vaivén, las ondas que se cruzan y la espuma fina de la orilla. |
| ![Rápidos](capturas/agua_rapidos.png) | **Los rápidos de un arroyo** (`--ir rapidos`): la corriente y la espuma en lo bajo, entre los cantos. |
| ![Cascada](capturas/agua_cascada.png) | **Una cascada de cerca** (`--ir cascada-pie`): la cortina que baja, la espuma y la bruma al pie. |
| ![Respaldo](capturas/agua_respaldo.png) | **El agua sin su shader** (`--sin-shader`): lo que queda si el teléfono no lo compila (las ondas y la malla de espuma de antes). |

### Los bordes

El muro tras el canal del bosque y el mar abierto ([AGUA.md](AGUA.md), fase 5):

| | |
|---|---|
| ![Muro desde la orilla](capturas/borde_muro_canal.png) | **El muro de roca del otro lado del canal** (`--ir muro-canal`): estratos levantados, desde la orilla despejada. |
| ![Muro desde arriba](capturas/borde_muro_lejos.png) | **El mismo, desde arriba del bosque** (`--ir canal --dron 45`). |
| ![Tiburón](capturas/borde_tiburon.png) | **La aleta del tiburón** (`--ir mar --trampa tiburon`): da vueltas alrededor de quien nada mar adentro de su línea. |
| ![Playa](capturas/borde_playa.png) | **Despertar en la playa** (`--ir mar --trampa mordida`): herido y con secuelas («herido · secuelas»). |

## Los edificios de la edad del hierro

Ver [ARQUITECTURA_EDAD_DEL_HIERRO.md](ARQUITECTURA_EDAD_DEL_HIERRO.md). Con `--edificios tipo[:región]`
(una fila por región, las cuatro variantes de cada una) y `--ir plaza-<región> --dron 40 --camara 0.85`:

| | |
|---|---|
| ![Casas](capturas/kit_casa.png) | **Las casas** (`--edificios casa`): la yurta, la casa de postes y paja, la de piedra y adobe, la casa larga de turba y la de adobe con patio. |
| ![Palacios](capturas/kit_palacio.png) | **Los palacios** (`--edificios palacio`): la orda del kan, las salas largas y las salas de columnas. |
| ![Fundiciones](capturas/kit_fundicion.png) | **Las fundiciones** (`--edificios fundicion`): el horno de cuba bajo el cobertizo de cada región. |
| ![Cuarteles](capturas/kit_cuartel.png) | **Los cuarteles** (`--edificios cuartel`), con su armero de lanzas. |
| ![Carpinterías](capturas/kit_carpinteria.png) | **Las carpinterías** (`--edificios carpinteria`). |
| ![Graneros](capturas/kit_granero.png) | **Los graneros** (`--edificios granero`): sobre postes, o de adobe con silos. |
| ![Templos](capturas/kit_templo.png) | **Los templos del fuego** (`--edificios templo`), del desierto y el altiplano. |
| ![Carretas](capturas/kit_carreta.png) | **Las carretas-casa** de la estepa (`--edificios carreta`). |
| ![Murallas](capturas/kit_muralla.png) | **Los tramos de la muralla** (`--edificios muralla`): el terraplén con la empalizada, el *murus gallicus*, la piedra con contrafuertes, la piedra seca y la turba, el adobe sobre zócalo. |
| ![Torres](capturas/kit_torre.png) | **Las torres de la muralla** (`--edificios torre`). |
| ![Puertas](capturas/kit_puerta.png) | **Las puertas acodadas** (`--edificios puerta`): el antemuro, cerrado de un costado. |
| ![Asedio](capturas/asedio_puerta.png) | **La puerta acodada en el asedio** (`--asedio puerta`): desde el patio del antemuro, mirando la puerta. |
| ![Salida](capturas/asedio_salida.png) | **La guarnición sale** por la puerta acodada (`--asedio salida`). |
| ![Fortalezas](capturas/kit_fortaleza.png) | **Las fortalezas derruidas** (`--edificios fortaleza`): la muralla de cada región, comida desde arriba. |
| ![Enterrada](capturas/kit_enterrada.png) | **La ciudad enterrada** del desierto (`--edificios enterrada`). |
| ![Refugios](capturas/kit_refugio.png) | **El refugio de ramas y pieles** (`--edificios refugio`), una obra de la tribu. |
| ![Empalizadas](capturas/kit_empalizada.png) | **La empalizada** (`--edificios empalizada`). |
| ![Muros](capturas/kit_muro.png) | **El muro de piedra seca** (`--edificios muro`). |
| ![Hornos](capturas/kit_horno.png) | **El horno de cocina** (`--edificios horno`). |
| ![Fundición](capturas/kit_horno_fundicion.png) | **Los hornos de fundición** del bronce y del acero (`--edificios horno_fundicion`). |
| ![Atalayas](capturas/kit_atalaya.png) | **La atalaya** (`--edificios atalaya`). |
| ![Corrales](capturas/kit_corral.png) | **El corral** de cuerdas y estacas (`--edificios corral`). |
| ![Talleres](capturas/kit_taller.png) | **El taller del herrero** (`--edificios taller`). |
| ![Tiendas](capturas/kit_tienda.png) | **La tienda ligera y la choza de fieltro** (`--edificios tienda`). |
| ![Escombros](capturas/kit_escombros.png) | **Los escombros y el muro en ruinas** (`--edificios escombros`). |
| ![Cenizas](capturas/kit_cenizas.png) | **Las cenizas y la yurta quemada** (`--edificios cenizas`). |
| ![Estepa](capturas/kit_plaza_estepa.png) | **La capital del kanato** (`--ir plaza-estepa`). |
| ![Bosque](capturas/kit_plaza_bosque.png) | **La capital del bosque** (`--ir plaza-bosque`). |
| ![Altiplano](capturas/kit_plaza_altiplano.png) | **La capital del altiplano** (`--ir plaza-altiplano`). |
| ![Fiordos](capturas/kit_plaza_fiordos.png) | **La capital de los fiordos** (`--ir plaza-fiordos`). |
| ![Desierto](capturas/kit_plaza_desierto.png) | **La capital del desierto** (`--ir plaza-desierto`). |
