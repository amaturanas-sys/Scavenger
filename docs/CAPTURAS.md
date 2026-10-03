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

## Menú Tab: acciones, obras, fabricar, reparar

| | |
|---|---|
| ![Acciones](capturas/08_acciones.png) | **Acciones** (`--pestana 0`). |
| ![Obras](capturas/09_obras.png) | **Obras en grupo** (`--pestana 1`): las grises no se pueden aún (falta oficio o cuadrilla). |
| ![Fabricar](capturas/10_fabricar.png) | **Fabricar** (`--pestana 2`): punto turquesa = a mano; los ingredientes, como iconos con lo que tienes / lo que hace falta. |
| ![Reparar](capturas/11_reparar.png) | **Reparar** (`--pestana 3`): la barra al pie es el estado de la pieza. |

## Inventario y equipo

| | |
|---|---|
| ![Inventario](capturas/12_inventario.png) | **Inventario** (I, `--inventario`): dos columnas; arriba, los contenedores a mano (bolsillos, mochila, alforjas, carreta, campamento). |
| ![Equipo](capturas/13_equipo.png) | **Equipo** (P, `--equipo`): los nueve huecos de armadura alrededor de la figura y los tres amuletos; a la derecha, sus cifras y la salud. |

## Combate y mundo

| | |
|---|---|
| ![Jinetes](capturas/14_jinetes.png) | **Jinetes bandidos** (`--enemigos jinetes`). |
| ![Botín](capturas/15_botin.png) | **Bolsas de botín** (`--botin`). |
| ![Galería](capturas/16_galeria.png) | **Galería de modelos** (`--galeria`): todos los objetos del inventario a escala. |
