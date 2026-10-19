#!/usr/bin/env bash
# Prepara el Codespace (o cualquier devcontainer de Ubuntu 24.04) como el CI de Linux: las
# bibliotecas de X11 y OpenGL que pide raylib, Xvfb para correr el juego sin pantalla, cmake y
# ninja, y Python con lo de la tuberia de modelos en su propio entorno (.venv). Despues compila de
# cero y corre las pruebas del nucleo. Se puede volver a correr: bash .devcontainer/setup.sh
set -euo pipefail
cd "$(dirname "$0")/.."
SUDO=""
if [ "$(id -u)" -ne 0 ]; then SUDO="sudo"; fi
$SUDO apt-get update
$SUDO env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git ca-certificates \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libgl1-mesa-dri \
    xvfb xauth python3 python3-venv python3-pip
python3 -m venv .venv
.venv/bin/pip install --quiet --upgrade pip
.venv/bin/pip install --quiet -r tools/modelos/requirements.txt
rm -rf build # de cero, como el CI: una carpeta vieja esconde errores
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake -E copy_directory assets build/assets
./build/sim_tests | tail -1
echo "Listo. El juego sin pantalla: cd build && xvfb-run -a ./estepa --frames 60 --screenshot shot.png"
