#!/usr/bin/env bash
# Capturas de todas las pantallas del juego (para docs/CAPTURAS.md).
# Uso: tools/capturas.sh [carpeta_build]   (por defecto build/; necesita xvfb-run en Linux)
# Escribe docs/capturas/*.png a 640x360, la resolucion real del juego.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/build}"
OUT="$ROOT/docs/capturas"
mkdir -p "$OUT"
cd "$BUILD"
export LIBGL_ALWAYS_SOFTWARE=1
shot() { # nombre cuadros [opciones...]
  local name="$1" frames="$2"
  shift 2
  xvfb-run -a -s "-screen 0 1280x720x24" ./estepa --screenshot "$OUT/$name.png" --frames "$frames" "$@" >/dev/null 2>&1
  test -s "$OUT/$name.png" && echo "  $name.png"
}
echo "Capturas en $OUT:"
# Una partida guardada en el hueco 1 (con su minifoto) para la pantalla de cargar.
xvfb-run -a -s "-screen 0 1280x720x24" ./estepa --screenshot /tmp/estepa_guardado.png --frames 40 --autoguardar 1 >/dev/null 2>&1 || true
shot 01_titulo 20 --menu
shot 02_cargar 20 --huecos
shot 03_instructivo 20 --instructivo
shot 04_juego 60
shot 05_controles 30 --controles
shot 06_pausa 30 --pausa
shot 07_guardar 30 --guardar
shot 08_acciones 30 --pestana 0
shot 09_obras 30 --pestana 1
shot 10_fabricar 30 --pestana 2
shot 11_reparar 30 --pestana 3
shot 12_inventario 30 --inventario
shot 13_equipo 30 --equipo --heridas
shot 14_jinetes 40 --enemigos jinetes
shot 15_botin 30 --botin
shot 16_galeria 40 --galeria
