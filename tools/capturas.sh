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
# El mundo (docs/MUNDO.md): a pie en cada sitio (verano, mediodia) y la vista orbital.
for p in estepa meandro arroyo bosque altiplano trenzado hielo cumbre mar desierto muro canal capital aldea guarida tribu rival kurgan caravasar ovoo ciudad; do
    shot "mundo_$p" 90 --dia 11 --minuto 7 --ir "$p" --camara 0.12
done
# El cielo: el ocaso al oeste, la luna al este y las estrellas.
shot cielo_ocaso 60 --dia 5 --minuto 15.5 --ir estepa --camara 0.06 --rumbo 270
shot cielo_luna 60 --dia 5 --minuto 18 --ir estepa --camara 0.06 --rumbo 90
shot orbital_1206_30 40 --dia 11 --minuto 7 --orbital 30 0.5
shot orbital_1206_150 40 --dia 11 --minuto 7 --orbital 150 0.3
shot orbital_1206_250 40 --dia 11 --minuto 7 --orbital 250 0.9
shot orbital_2024_300 40 --semilla 2024 --dia 11 --minuto 7 --orbital 300 0.5
shot orbital_42_90 40 --semilla 42 --dia 11 --minuto 7 --orbital 90 0.65
# Mapas generales de varias semillas.
mkdir -p "$ROOT/docs/mundo"
for s in 1206 7 42 1984 2024 31337; do
    xvfb-run -a -s "-screen 0 1280x720x24" ./estepa --semilla "$s" --mapa-mundo "$ROOT/docs/mundo/mapa_$s.png" >/dev/null 2>&1 || true
done
