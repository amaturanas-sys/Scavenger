Versión @VERSION@ de **SCAVENGERS THRIVE — THEY COME FROM THE STEPPES**: la tribu acampa en la estepa central de un mundo de cinco regiones que cambia en cada partida nueva.

## Novedades: el sistema táctico y las cuevas

- **Las manos:** `H` ataque, `I` parry, `J` bloqueo, `K` carga o patada, según lo que tengas fuera; la espada (`1`) y el escudo (`2`) se guardan por separado.
- **La barra rápida, de 10 casillas** (`1` a `9` y `0`): con el inventario abierto (`Tab`), un número, o un toque en la fila de abajo, pone ahí lo elegido. El odre bebe o, junto al agua, se llena; el lazo va al animal elegido.
- **El objetivo:** `O`, o un toque sobre la silueta, entre enemigos y animales salvajes; con arco, los clics cambian y mantener 1 s lo suelta. Un enemigo tarda 5 s en reconocerte de día, 7 de noche y 3 con la antorcha.
- **A cubierto:** `C` junto a un tronco, una mata alta, una roca, una peña o una ruina; con el arco te asomas, disparas y vuelves.
- **Las cuevas:** cada guarida tiene la suya. Se entra caminando por la boca. Adentro está oscuro: la antorcha, los cofres y las armas, los prisioneros que se unen a la tribu o te traicionan, y los guardias de las habitadas que salen a la boca.
- Las partidas de antes cargan igual.

El detalle, en el README («Novedades») y en `docs/PLAN_TACTICO_Y_CUEVAS.md`.

## Android

1. Descarga **`scavengers-thrive-@VERSION@.apk`** en el dispositivo.
2. Ábrelo. Si Android lo pide, permite «Instalar apps desconocidas» para el navegador o el gestor de archivos.
3. **Conecta un teclado físico** (USB o Bluetooth). Sin teclado, el juego se pausa con un aviso. El ratón y el mando son opcionales.
4. Abre **Scavengers Thrive**. En el menú de entrada, elige **Nueva partida**.

Requisitos y notas:
- Android 7.0 o superior, de 64 bits (arm64-v8a).
- Las partidas se guardan en el almacenamiento interno de la app.
- Las siguientes versiones se instalan encima, sin perder las partidas, porque van firmadas con la misma clave.

Desde un PC: `adb install -r scavengers-thrive-@VERSION@.apk`.

## Windows

Descomprime **`scavengers-thrive-@VERSION@-windows.zip`** y ejecuta `ScavengersThrive/estepa.exe`. Deja la carpeta `assets` junto al ejecutable.

## Controles básicos

| Acción | Tecla |
|---|---|
| Moverse / correr / saltar | WASD · Shift · Espacio |
| Sigilo; junto a un obstáculo, cubrirse | C |
| Cámara | Q / E, clic derecho + arrastrar, rueda |
| Combate | H ataque · I parry · J bloqueo · K carga o patada |
| Objetivo | O |
| Acciones, obras, fabricar | L |
| Inventario | Tab |
| Tomar, hablar, recoger botín | F |
| Montar | R |
| Beber | N |
| Vista orbital del mundo | F5 |
| Controles en pantalla | F1 |
| Pausa, guardar y cargar | Esc |

El **instructivo** del menú explica el resto.
