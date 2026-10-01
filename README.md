# ESTEPA (título provisional)

RPG de mundo abierto en 3D low-res, inspirado en las tribus nómadas de la estepa (hunos, mongoles), con elementos de *Outward* y *Freedom Fighters*. Lideras una tropa guerrillera: acampas, reclutas, delegas, juzgas, y cada decisión pesa en la moral de los tuyos y en tu relación con el reino al que rindes tributo.

**Plataformas:** Windows y Android · **Tecnología:** C11 + [raylib](https://www.raylib.com) 5.5

- Diseño: [docs/GDD.md](docs/GDD.md)
- Narrativa: [docs/NARRATIVA.md](docs/NARRATIVA.md)
- Roadmap: [docs/ROADMAP.md](docs/ROADMAP.md)
- Arquitectura: [docs/ARQUITECTURA.md](docs/ARQUITECTURA.md)

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

## Controles (Fase 0)

| Acción | Teclado | Mando |
|---|---|---|
| Moverse | WASD / flechas | Stick izquierdo |
| Correr | Shift | LB |
| Acechar (sigilo) | C / Ctrl | Click stick derecho |
| Saltar | Espacio | A |
| Cámara | Q / E, clic derecho + arrastrar, rueda | — |
| Marcar sitio de interés / de peligro | M / Shift+M | — |

Teclas de prueba de la política del campamento: `1` reclutar · `2` tomar prisionero · `3` ejecutar prisionero · `4` ejecutar integrante · `5` desterrar · `6` repartir botín · `7` liberar prisionero · `Enter` avanzar un día.

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
```
