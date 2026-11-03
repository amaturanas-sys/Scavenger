#!/usr/bin/env bash
# Prepara el Codespace (o cualquier devcontainer de Ubuntu 24.04) como el CI de Linux: las
# bibliotecas de X11 y OpenGL que pide raylib, Xvfb para correr el juego sin pantalla, cmake y
# ninja, gh (los PR, el CI y las releases desde la terminal; ver docs/GUIA_IA.md) y Python con lo de
# la tuberia de modelos en su propio entorno (.venv). Despues compila de cero y corre las pruebas
# del nucleo. Al final, lo de las skills de diseno (.claude/skills/estetica): text-to-cad en
# .venv-cad, con Chromium para sus fotos, e ImageMagick para poner capturas lado a lado; Node, para
# Kiln, lo trae devcontainer.json. Se puede volver a correr: bash .devcontainer/setup.sh
set -euo pipefail
cd "$(dirname "$0")/.."
# El pre-push del repo: nada a main ni a claude/*, y las guardas a la vista (docs/GUIA_IA.md).
git config core.hooksPath .githooks
SUDO=""
if [ "$(id -u)" -ne 0 ]; then SUDO="sudo"; fi
$SUDO apt-get update
$SUDO env DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git ca-certificates \
    libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev libgl1-mesa-dri \
    xvfb xauth python3 python3-venv python3-pip gh imagemagick
python3 -m venv .venv
.venv/bin/pip install --quiet --upgrade pip
.venv/bin/pip install --quiet -r tools/modelos/requirements.txt
rm -rf build # de cero, como el CI: una carpeta vieja esconde errores
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake -E copy_directory assets build/assets
./build/sim_tests | tail -1
python3 tools/ci/guardas.py --base HEAD || true
# text-to-cad para las skills de diseno (la usan Copilot, Claude y Cline). Si falla, el Codespace
# sigue sirviendo para el juego: se reintenta con python3 tools/assets/cad.py instalar.
if python3 tools/assets/cad.py instalar; then
    # Las bibliotecas del sistema que pide Chromium para las fotos (cad.py foto).
    $SUDO .venv-cad/bin/python -m playwright install-deps chromium ||
        echo "AVISO: faltan bibliotecas de Chromium; las fotos de text-to-cad pueden fallar."
else
    echo "AVISO: text-to-cad no se instalo; se reintenta con: python3 tools/assets/cad.py instalar"
fi
echo "Listo. El juego sin pantalla: cd build && xvfb-run -a ./estepa --frames 60 --screenshot shot.png"
