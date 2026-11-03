Versión @VERSION@ de **SCAVENGERS THRIVE — THEY COME FROM THE STEPPES**: la tribu acampa en la estepa central de un mundo de cinco regiones que cambia en cada partida nueva.

## Novedades: las bocas de las cuevas

- **La roca, de voxeles:** cada boca es un risco de la piedra de su región, con su arco natural, el túnel detrás y dos rocas bajas en el umbral. En el desierto es arenisca en estratos; en el altiplano, granito; en el resto, piedra con liquen y pasto en lo alto. En una ladera no queda nada en el aire.
- **De afuera se ve de quién es:** las habitadas tienen una escalera que sube a un saliente (el del imperio, de piedra y con baranda); la tribu, un bastidor con un cuero; los forajidos, su estacada. Las abandonadas tienen una escalera rota y la fachada de una civilización desaparecida, con su reja caída a un costado.
- Todo está fuera del paso: se sigue entrando caminando por la boca.
- Las partidas de antes cargan igual.

Lo de la versión 0.13.0 sigue igual: las manos (`H`, `I`, `J`, `K`), la barra rápida de 10 casillas, el objetivo (`O`), la cobertura (`C`) y el interior de las cuevas. El detalle, en el [README](https://github.com/amaturanas-sys/Scavenger/blob/v@VERSION@/README.md#controles) (en «Controles», los apartados «Las bocas de las cuevas» y «El sistema táctico y las cuevas») y en [docs/PLAN_TACTICO_Y_CUEVAS.md](https://github.com/amaturanas-sys/Scavenger/blob/v@VERSION@/docs/PLAN_TACTICO_Y_CUEVAS.md).

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
