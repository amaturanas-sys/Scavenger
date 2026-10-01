# ESTEPA (título provisional)

RPG de mundo abierto en 3D low-res, inspirado en las tribus nómadas de la estepa (hunos, mongoles), con elementos de *Outward* y *Freedom Fighters*. Lideras una tropa guerrillera: acampas, reclutas, delegas, juzgas, y cada decisión pesa en la moral de los tuyos y en tu relación con el reino al que rindes tributo.

**Plataformas:** Windows y Android · **Tecnología:** C11 + [raylib](https://www.raylib.com) 5.5

- Diseño: [docs/GDD.md](docs/GDD.md)
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

Teclas de prueba de la política del campamento: `1` reclutar · `2` tomar prisionero · `3` ejecutar prisionero · `4` ejecutar integrante · `5` desterrar · `6` repartir botín · `7` liberar prisionero · `Enter` avanzar un día.

## Captura automática

```bash
./build/estepa --screenshot captura.png --frames 60
```
