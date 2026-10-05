# SCAVENGERS THRIVE - THEY COME FROM THE STEPPES

*(nombre interno del proyecto: `estepa`)*

RPG de mundo abierto en 3D low-res, inspirado en las tribus nómadas de la estepa (hunos, mongoles), con elementos de *Outward* y *Freedom Fighters*. Lideras una tropa guerrillera: acampas, reclutas, delegas, juzgas, y cada decisión pesa en la moral de los tuyos y en tu relación con el reino al que rindes tributo.

**Plataformas:** Windows y Android · **Tecnología:** C11 + [raylib](https://www.raylib.com) 5.5

- Diseño: [docs/GDD.md](docs/GDD.md)
- Narrativa: [docs/NARRATIVA.md](docs/NARRATIVA.md)
- Roadmap: [docs/ROADMAP.md](docs/ROADMAP.md)
- Arquitectura: [docs/ARQUITECTURA.md](docs/ARQUITECTURA.md)
- Capturas de todas las pantallas: [docs/CAPTURAS.md](docs/CAPTURAS.md)
- Modelos editables en Nomad Sculpt (un GLB por objeto) y atlas de materiales para pintar: [arte/nomad/](arte/nomad/README.md) · iconos de los menús: [assets/ui/iconos.png](assets/ui/iconos.png) (ver [ESTILO_VISUAL.md](docs/ESTILO_VISUAL.md#iconos))
- Inventario de assets: [docs/INVENTARIO.md](docs/INVENTARIO.md) · animaciones: [docs/ANIMACIONES.md](docs/ANIMACIONES.md) · cómo importar modelos: [assets/models/README.md](assets/models/README.md)

## Compilar y jugar

Requisitos: CMake ≥ 3.20 y un compilador C (GCC, Clang o MSVC). raylib se descarga solo.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build      # tests del nucleo
./build/estepa              # Windows: build\Release\estepa.exe
```

En Linux hacen falta las cabeceras de X11/OpenGL:
`sudo apt install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`

## Idioma / Language

Toda la interfaz está en **español o inglés**. Se cambia con el botón del globo (Idioma / Language) en el menú de entrada o en la pausa, y se recuerda entre partidas (`saves/ajustes.txt`).

*The whole interface is available in **Spanish or English**: use the globe button (Idioma / Language) on the title or pause menu. The choice is remembered.*

- En el código, cada texto visible se escribe en español dentro de `T("...")` (o `N_("...")` en tablas que se traducen al mostrarse): [src/sim/lang.h](src/sim/lang.h).
- Las traducciones están en [assets/i18n/en.tsv](assets/i18n/en.tsv): español, tabulador, inglés. Los nombres de los objetos salen del inventario.
- Flujo de trabajo:
  ```bash
  python3 tools/assets/i18n.py extraer   # añade los textos nuevos (sin traducir) y quita los que ya no se usan
  python3 tools/assets/i18n.py faltan    # lista lo que falta traducir
  python3 tools/assets/i18n.py check     # el CI falla si falta una traduccion o si un %d/%s no coincide
  ```

## Menú y partidas

Al abrir el juego aparece el **menú de entrada**: nueva partida, cargar partida, instructivo y salir. `Esc` en la partida la pausa: continuar, guardar, cargar, instructivo, volver al título. Hay tres huecos de guardado, cada uno con su minifoto, la fecha en que se guardó, el día de juego y la tribu. `F1` muestra los controles sin salir de la partida.

Las partidas se guardan en `saves/` junto al ejecutable (en Android, en el almacenamiento interno de la app). Pruebas: `--menu`, `--instructivo`, `--huecos` abren el menú; `--autoguardar N` guarda en el hueco N al terminar una captura; `--cargar N` carga el hueco N al empezar; `--inventario` y `--equipo` abren esos menús.

## Controles (Fase 0)

| Acción | Teclado | Mando |
|---|---|---|
| Moverse | WASD / flechas | Stick izquierdo |
| Correr | Shift | LB |
| Acechar (sigilo) | C / Ctrl | Click stick derecho |
| Saltar | Espacio | A |
| Cámara | Q / E, clic derecho + arrastrar, rueda | — |
| Marcar sitio de interés / de peligro | M / Shift+M | — |
| Menú de acciones, obras, fabricar y reparar | Tab (flechas + Enter; Q/E cambia de pestaña) | — |
| Cambiar empuñadura / enfundar | X / H | — |
| Tomar / lanzar objetos, recoger botín | F (Mayús+F: todo lo cercano) / T | — |
| Montar / desmontar | R | — |
| Inventario (bolsillos, mochila, alforjas, carreta, campamento) | I | — |
| Esconderse | acechar (C) dentro de la hierba alta | — |
| Beber / tomar hierbas contra los espíritus del agua | N / Mayús+N | — |
| Vista orbital del mundo | F5 (flechas: girar e inclinar) | — |

Teclas de prueba de la política del campamento: `1` reclutar · `2` tomar prisionero · `3` ejecutar prisionero · `4` ejecutar integrante · `5` desterrar · `6` repartir botín · `7` liberar prisionero · `8` encontrar un gran guerrero · `G` ficha del último gran guerrero · `Enter` saltar al día siguiente · `Ctrl+N` adelantar dos minutos.

Peligros: `Y` escolta (dos integrantes te acompañan) · `F` junto a un compañero atrapado para rescatarlo · `J K L U I O` en el minijuego de rescate.

Menús: son **iconos**. Pasa el ratón por encima (o elige con las flechas) y su nombre aparece en la leyenda de la base de la pantalla; un clic elige y otro clic (o Enter) lo usa. En el inventario, `Q`/`E` cambia de contenedor y por el borde de la cuadrícula se pasa a la otra columna. En el equipo, el clic derecho (o `Supr`) quita la pieza.

Combate y salud: `V` o clic izquierdo golpear (tres seguidos: combo; mantener: golpe pesado; con arco u honda: mantener para tensar y soltar para disparar; la cámara alza la mira) · `J` o botón central del ratón patada (corriendo: con inercia) · `Z` cubrirse (con escudo: `Z`+`V` golpe de escudo, corriendo + `Z` carga) · `U` agarre y llave · `O` enganchar el escudo enemigo (hacha, guja, alabarda) · `Mayús+X` cambiar el arma de mano · `B` vendar (a ti o a un compañero cercano) · `L` encender la flecha junto a un fuego · `P` equipo (armadura, amuletos y heridas) · `I` inventario · `9` enemigos de prueba (`Shift+9` lobos, `Ctrl+9` culto, `Alt+9` arqueros, `Ctrl+Shift+9` jinetes).

Combate montado: a caballo solo se golpea con el arma (`V`; mantener: pesado), con +0.9 m de alcance y la inercia del galope (hasta ×1.8); con lanza por encima de 6 m/s el golpe suele derribar; al galope arrollas a quien tengas delante. Los golpes enemigos a veces dan a tu montura y un derribo te tira del caballo; a distancia, la carrera dispersa el tiro (el amuleto del caballo lo corrige). Los jinetes bandidos derribados pierden el caballo, que queda suelto para echarle el lazo.

Fabricar y reparar (`Tab`, pestañas Fabricar y Reparar): a mano, flechas (leña + plumas + pedernal → 5), virotes, piedras de honda, cuerda de tendones, ungüento (hierbas + miel), coraza de cuero y botas de fieltro; lo forjado pide herrero y horno. Los ingredientes se recolectan: matas de hierbas y pedernal en el campo (`F`), plumas de las aves y hueso y tendones de los demás animales al despiezarlos (`K`). Reparar: fieltro y cuero con pieles; bronce, hierro y acero con herrero, horno y metal.

Botín: los enemigos abatidos dejan una bolsa (su arma, su escudo, flechas, comida, a veces un amuleto y piezas de su armadura con el desgaste que tengan); `F` junto a ella guarda lo que quepa.

Mochilas:
- **Tamaños:** pequeña (15 kg, 12 huecos), mediana (25 kg, 18) o grande (40 kg, 28). La grande frena aun vacía.
- **`G`:** deja la mochila (en la carreta o en el lomo de una mula, burro, camello, caballo o buey ensillado si están al lado; si no, en el suelo) y la recoge. Sin ella vas más ligero para pelear.
- **`U` sobre una mochila en el inventario:** cambias de mochila (lo que llevas tiene que caber).
- **La escolta lleva mochila:** las de quien esté a tu lado aparecen como contenedores en el inventario. Al disolver un campamento, la gente carga lo que quepa.

Animales de la tribu:
- **Comen.** Los herbívoros pastan solos si hay pasto. Con nieve hay que darles forraje: `F` corta hierba alta y los pastores lo juntan cada día.
- **Los carnívoros** comen carne o cazan presas cercanas cuando tienen hambre.
- **`K`** da de comer al animal hambriento que tengas al lado. Los pastores alimentan, con el acopio, a los que están en su campamento.
- **El hambre:** sobre la cabeza aparece un cuenco. Con mucha hambre adelgazan y al final se van y vuelven a ser salvajes.

Campamentos:
- **Fundar:** al levantar una tienda (`Tab`, «Instalar tienda») o un refugio lejos (más de 70 m) de los demás campamentos, nace uno nuevo. Hay que nombrar guardián a alguien de la escolta, que se queda a administrarlo.
- **Lo propio de cada campamento:** su acopio y su gente. Las obras las levantan los del lugar; fuera de todo campamento, la escolta.
- **El guardián (`F` junto a él):**
  - ordenar obras con el acopio del campamento;
  - elegir quién sale de escolta;
  - reclutar gente nueva: alguien sale a buscarla con comida;
  - capacitar en un oficio: soldado, herrero, druida, orfebre, pastor, cazador o explorador;
  - ver el estado (gente, tareas, acopio);
  - disolver el campamento.
- **Tareas:** gastan recursos y ocupan gente. Avanzan más rápido con más gente sin tarea (hasta ×2) y mucho más con un maestro del oficio (+60 % cada uno).
- **Disolver:** se queman las estructuras (y las yurtas del campamento inicial). Lo del acopio se carga en la carreta y en las alforjas que estén cerca; lo que no cabe se pierde. La gente sigue al jugador.
- **El campamento inicial:** su guardián es el lugarteniente.

Pruebas: `--hablar guardian`, `--fundar`.

Órdenes a la escolta (`Mayús+Y`):
- **Despachar:** marcas quiénes salen y eliges el destino: un campamento o un sitio marcado en el mapa (`M`). Al menos uno tiene que conocerlo (haber estado allí). Cuantos más lo conozcan, menos riesgo; también influyen la distancia, el tamaño del grupo y la noche.
- **El viaje:** parten a pie o a caballo, como iban contigo, y se pierden en el horizonte. Aparecen en el destino al llegar, pero solo sabes cómo les fue cuando vas allí: si hubo un percance, algunos llegan heridos y otros no llegan.
- **Mensajero:** lleva el pedido a un campamento que conozca y vuelve con refuerzos (la gente libre de allí, no el guardián). Tarda la ida y la vuelta hasta donde estés.
- **La gente aprende los sitios** por donde pasa y conoce su campamento.

Pruebas: `--ordenes`, `--despachar`.

Ropa y clima:
- **`P`, pestaña Ropa:** cinco capas (cabeza, cuello y cara, cuerpo, capa o abrigo, pies). Cada prenda abriga, da sombra contra el sol, frena la lluvia y algunas pesan.
- **Frío:** pieles y abrigos de oso, reno, lobo, tigre, puma, hiena o lana de cabra, gorros de piel y botas de piel de reno.
- **Sol y calor:** sombrero de ala ancha, pañuelo del desierto, túnica de seda blanca y manto blanco. El desierto quema de día (hasta +11°) y hiela de noche. Con calor te sofocas (más lento); con golpe de calor te desmayas y la tribu te lleva a la sombra. La sombra de las yurtas y el agua refrescan.
- **Escarmiento:** las pieles de depredador espantan. Al verte de cerca, el enemigo puede echarse atrás, y herido huye antes (los fanáticos casi no se inmutan).
- **Fabricar (`Tab`, a mano):** con las pieles del despiece (los depredadores, el reno y la cabra dan su piel), la lana de cabra (los pastores esquilan cada día) y la seda del botín de bandidos y jinetes.
- **La tribu se cambia sola** según el tiempo, con la ropa de su mochila y, en su campamento, la del acopio. Sin ropa adecuada pasa frío o calor (un copo o un sol sobre la cabeza) y baja la moral.

Prueba: `--ropa`.

El mundo ([MUNDO.md](docs/MUNDO.md)):
- **Una gran tierra casi redonda (4,8 km de radio, de borde fractal) con cinco regiones fijas:** la estepa en el centro (allí acampa la tribu), rodeada de ríos de meandros; el bosque de coníferas, muy tupido, al noroeste; el desierto al noreste; el altiplano glaciar al sureste; la costa de fiordos al suroeste. Cada una tiene su altitud media: la costa, la más baja; el altiplano, el más alto.
- **El borde:** el mar tras los fiordos, un gran cañón y un muro de estratos en el desierto, un muro de hielo en el altiplano y un canal sinuoso en el bosque, del que salen ríos tributarios. Del altiplano bajan ríos trenzados por lechos de grava. No se puede salir.
- **Texturas del suelo** low-res inspiradas en fotos de la estepa y del Altai (matas, pasto alto, tundra, grava, nieve, estratos, hielo): `tools/assets/texturas_terreno.py`.
- **El cielo:** el sol sale por el este y se pone por el oeste; la luna cambia de fase (14 días) y las estrellas giran alrededor del polo. Las nubes van a una altura fija y el viento las lleva; las cumbres altas las atraviesan y quedan envueltas (dentro de una nube, niebla). A lo lejos se ve el horizonte: montañas, muros y mar a ~2,4 km.
- **Cada partida nueva es otro mundo:** cambian los ríos, los lagos, las montañas, las fronteras entre regiones, los pueblos y las guaridas, siempre con las mismas reglas.
- **Reinos vecinos:** uno por región, con su capital amurallada y sus aldeas al estilo de la región; al llegar quedan en el mapa.
- **Guaridas:** de las fieras de cada región; cerca aparecen sus dueños.
- **`F5`:** vista orbital del mundo entero (flechas: girar e inclinar).

Pruebas: `--semilla N` fija el mundo; `--ir lugar` lleva a `estepa`, `meandro`, `bosque`, `canal`, `altiplano`, `trenzado`, `hielo`, `cumbre`, `fiordos`, `mar`, `desierto`, `muro`, `capital`, `aldea` o `guarida`; `--camara inclinación` y `--rumbo grados` orientan la cámara; `--orbital [grados] [inclinación]`; `--mapa-mundo archivo.png` (con `--mapa-tam N`).

El agua:
- **La sed** baja con el tiempo, más con calor y corriendo. Con sed vas más lento y te cansas; con la sed a cero te desmayas y la tribu te lleva al campamento a beber.
- **`N` bebe** lo más seguro que tengas a mano: agua hervida, agua con vino, airag, cerveza, leche, vino y, al final, agua cruda. En la orilla, sin nada, bebes del río.
- **Espíritus malditos:** el agua que corre por los suelos (ríos, lagos) puede traerlos, más con calor. Se incuban uno o dos minutos y luego dan fiebre: más lento, más sed, te cansas. Se van solos en unos minutos (antes descansando); las hierbas (`Mayús+N`) los cortan.
- **Hervir el agua los elimina** (`Tab`, «Hervir agua», junto a un fuego, una leña). El alcohol también: la cerveza, el vino, el airag y el agua con vino son seguros. «Llenar el odre» en la orilla da agua cruda (3 tragos por odre).
- **El exceso de alcohol** emborracha: torpe (menos daño y peor puntería), fatigado y te tambaleas al andar. Se pasa con el tiempo.
- **Fabricar:** agua con vino (2 de agua y 1 de vino), airag (2 de leche) y cerveza de pan. El vino y la cerveza también salen del botín de jinetes y bandidos.
- **La tribu bebe cada día** del acopio. Si falta, del río cercano: hervida si hay leña (una cada cuatro); si no, cruda, y a algunos les caen los espíritus (fiebre: baja la vida y el ánimo). Sin agua cerca, pasan sed.
- **Los animales de la tribu beben** solos junto al agua. Con sed, una gota sobre la cabeza: llévalos al agua o dales de beber (`K` con agua); los pastores les dan del acopio. Con mucha sed se van.

Tatuajes, joyas y nivel:
- **Nivel:** subes peleando, cazando, fabricando, construyendo y domando.
- **Tatuajes:** `F` junto al druida (Ulagan, en el campamento).
  - Son permanentes: eliges motivo (lobo, ciervo, grifo, tamga, olas), grado (I, II, III; el III se bifurca en dos ramas que se excluyen) y zona del cuerpo.
  - La zona cambia a qué atributo va el bonus y cuánto pesa.
  - Tinta: carbón y hierbas (y miel en el tercer grado).
- **Joyas:** el orfebre (Altani, `F`) hace anillos (dos por mano), brazaletes, collar, aretes y hebilla.
  - Se hacen con metal (bronce, plata, oro: la potencia) y una piedra (el efecto).
  - Sin encantar no hacen nada: el druida las encanta con un efecto pasivo (un atributo) o activo (una habilidad), por hierbas y miel.
- **Piedras y metales:**
  - rompe rocas a golpes (la maza y el golpe pesado parten mejor);
  - `F` en los arrecifes de coral de los lagos;
  - cava trincheras;
  - criba en la orilla (`Tab`, «Cribar en el agua»);
  - el lingote de bronce sale del horno de bronce.
- **Habilidades activas:** `F2`, `F3`, `F4`.
- **Menú de equipo (`P`):** pestañas Armadura, Joyas y Tatuajes (`Q`/`E`).

Pruebas: `--hablar druida|orfebre`, `--joyas`, `--tatuajes`.

Fauna: lazo (menú) para domar monturas o atar un depredador debilitado · `K` dar de comer al atado, despiezar un cadáver u ordeñar · `Mayús+K` sacrificar ganado · caminar cerca del ganado lo pastorea · golpear hacia el agua (o una flecha) pesca · `K` en una colmena con la antorcha encendida: miel. Prueba: `--lago` arranca en la orilla de un lago.

Al reclutar o tomar prisioneros, a veces aparece un gran guerrero por azar.

## Android

El port de Android ofrece la misma experiencia que en PC y **requiere teclado físico** (tablets o Chromebooks con teclado; ratón o mando opcionales). Si no hay teclado conectado, el juego se pausa con un aviso.

```bash
# Requiere Android SDK + NDK (ANDROID_SDK_ROOT, ANDROID_NDK_ROOT), CMake, Ninja y Java.
tools/android/build_apk.sh build-android/estepa.apk
adb install -r build-android/estepa.apk
```

El CI publica el APK como artefacto `estepa-android`.

### Actualizaciones
Cada versión nueva **se instala encima de la anterior**, sin desinstalar y conservando los datos:
- Todos los builds se firman con la misma clave de desarrollo (`android/estepa-dev.keystore`); el script verifica la huella del certificado (`android/dev-cert.sha256`) en cada build.
- El `versionCode` es automático (minutos desde 1970), así que cada build supera a la anterior.
- La versión aparece en el HUD: `ESTEPA v0.1.0 (build N)`. Para cambiar la versión legible, edita el archivo `VERSION`.

La clave de desarrollo es solo para builds de prueba. Para publicar en Google Play se usará una clave de release guardada en los *secrets* de GitHub (`ESTEPA_KEYSTORE`, `ESTEPA_KEYSTORE_PASS`, `ESTEPA_KEY_ALIAS`).

## Captura automática

```bash
./build/estepa --screenshot captura.png --frames 60
./build/estepa --screenshot aviso.png --frames 60 --sin-teclado   # simula Android sin teclado
./build/estepa --galeria                                           # galería del inventario de assets
./build/estepa --dia 25 --minuto 14                                # noche de pleno invierno
./build/estepa --dia 23 --ir altiplano                             # glaciares en invierno
./build/estepa --dia 25 --lago --trampa rescate                    # minijuego: sacar a un compañero del hielo
./build/estepa --semilla 42 --mapa-mundo mapa.png                  # mapa general del mundo 42
./build/estepa --orbital 30 0.5                                    # vista orbital (grados, inclinación)
./build/estepa --pos 60 40 --enemigos bandidos --heridas            # combate y heridas
```
