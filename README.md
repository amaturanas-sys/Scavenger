# ESTEPA (título provisional)

RPG de mundo abierto en 3D low-res, inspirado en las tribus nómadas de la estepa (hunos, mongoles), con elementos de *Outward* y *Freedom Fighters*. Lideras una tropa guerrillera: acampas, reclutas, delegas, juzgas, y cada decisión pesa en la moral de los tuyos y en tu relación con el reino al que rindes tributo.

**Plataformas:** Windows y Android · **Tecnología:** C11 + [raylib](https://www.raylib.com) 5.5

- Diseño: [docs/GDD.md](docs/GDD.md)
- Narrativa: [docs/NARRATIVA.md](docs/NARRATIVA.md)
- Roadmap: [docs/ROADMAP.md](docs/ROADMAP.md)
- Arquitectura: [docs/ARQUITECTURA.md](docs/ARQUITECTURA.md)
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
| Menú de acciones, obras, fabricar y reparar | Tab (flechas + Enter; A/D o ←/→ cambia de pestaña) | — |
| Cambiar empuñadura / enfundar | X / H | — |
| Tomar / lanzar objetos, recoger botín | F (Mayús+F: todo lo cercano) / T | — |
| Montar / desmontar | R | — |
| Inventario (bolsillos, mochila, alforjas, carreta, campamento) | I | — |
| Esconderse | acechar (C) dentro de la hierba alta | — |

Teclas de prueba de la política del campamento: `1` reclutar · `2` tomar prisionero · `3` ejecutar prisionero · `4` ejecutar integrante · `5` desterrar · `6` repartir botín · `7` liberar prisionero · `8` encontrar un gran guerrero · `G` ficha del último gran guerrero · `Enter` saltar al día siguiente · `N` adelantar dos minutos.

Peligros: `Y` escolta (dos integrantes te acompañan) · `F` junto a un compañero atrapado para rescatarlo · `J K L U I O` en el minijuego de rescate.

Combate y salud: `V` o clic izquierdo golpear (tres seguidos: combo; mantener: golpe pesado; con arco u honda: mantener para tensar y soltar para disparar; la cámara alza la mira) · `J` o botón central del ratón patada (corriendo: con inercia) · `Z` cubrirse (con escudo: `Z`+`V` golpe de escudo, corriendo + `Z` carga) · `U` agarre y llave · `O` enganchar el escudo enemigo (hacha, guja, alabarda) · `Mayús+X` cambiar el arma de mano · `B` vendar (a ti o a un compañero cercano) · `L` encender la flecha junto a un fuego · `P` equipo (armadura, amuletos y heridas) · `I` inventario · `9` enemigos de prueba (`Shift+9` lobos, `Ctrl+9` culto, `Alt+9` arqueros, `Ctrl+Shift+9` jinetes).

Combate montado: a caballo solo se golpea con el arma (`V`; mantener: pesado), con +0.9 m de alcance y la inercia del galope (hasta ×1.8); con lanza por encima de 6 m/s el golpe suele derribar; al galope arrollas a quien tengas delante. Los golpes enemigos a veces dan a tu montura y un derribo te tira del caballo; a distancia, la carrera dispersa el tiro (el amuleto del caballo lo corrige). Los jinetes bandidos derribados pierden el caballo, que queda suelto para echarle el lazo.

Fabricar y reparar (`Tab`, pestañas Fabricar y Reparar): a mano, flechas (leña + plumas + pedernal → 5), virotes, piedras de honda, cuerda de tendones, ungüento (hierbas + miel), coraza de cuero y botas de fieltro; lo forjado pide herrero y horno. Los ingredientes se recolectan: matas de hierbas y pedernal en el campo (`F`), plumas de las aves y hueso y tendones de los demás animales al despiezarlos (`K`). Reparar: fieltro y cuero con pieles; bronce, hierro y acero con herrero, horno y metal.

Botín: los enemigos abatidos dejan una bolsa (su arma, su escudo, flechas, comida, a veces un amuleto y piezas de su armadura con el desgaste que tengan); `F` junto a ella guarda lo que quepa.

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
./build/estepa --dia 23 --pos -340 -130                            # glaciares en invierno
./build/estepa --dia 25 --pos -148 -12 --trampa rescate            # minijuego: sacar a un compañero del hielo
./build/estepa --pos 60 40 --enemigos bandidos --heridas            # combate y heridas
```
