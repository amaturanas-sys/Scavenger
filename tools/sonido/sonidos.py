#!/usr/bin/env python3
"""Los sonidos del juego: los que sirven de los paquetes subidos a IMPORTED/Sound/SFX, convertidos para el juego.

  python3 tools/sonido/sonidos.py <carpeta IMPORTED/Sound/SFX>

(La carpeta, en la rama cline/procesar-imagenes-animales. Hace falta ffmpeg con libvorbis.)

El juego no lleva musica: el ambiente natural (el viento, el bosque de dia y de noche, la lluvia, la tormenta, el rio,
la cascada, el mar, la cueva y el fuego, en bucle) y los sonidos de lo que pasa (los pasos segun el suelo, las aves, el
agua, los golpes, el arco, los quejidos, beber y comer). SONIDOS dice de donde sale cada uno.

Cada uno, a assets/sonidos/<id>.ogg: mono, 22 050 Hz, Vorbis, y nivelado (los bucles, a un volumen medio parecido; los
golpes, con el pico a -3 dB), para que el juego los mezcle con sus propios volumenes (src/sim/sonido.c). Los bucles,
ademas, sin costura: el final se funde con el principio (un segundo), asi no chasquean al dar la vuelta. Los creditos,
en assets/sonidos/CREDITOS.md.
"""
import os
import re
import subprocess
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
SALIDA = os.path.join(ROOT, "assets", "sonidos")

TOM = "Free Fantasy SFX Pack By TomMusic/Free Fantasy SFX Pack By TomMusic/OGG Files"
CI = "400 Sounds Pack"
EXTRA = "96 Extra SFX"
PAQUETES = {
    TOM: ("Free Fantasy 200 SFX Pack, de TomMusic", "https://tommusic.itch.io/free-fantasy-200-sfx-pack",
          "libre de regalías, también en proyectos comerciales; el crédito se agradece"),
    CI: ("400 Sounds Pack, de Chequered Ink", "https://ci.itch.io/400-sounds-pack", "libre para uso comercial"),
    EXTRA: ("96 Extra SFX", "", "subido por el usuario a IMPORTED/Sound/SFX"),
}

# id: (paquete, archivo dentro del paquete, bucle). Los ids son los de src/game/audio_game.c (SND_*).
SONIDOS = {
    # El ambiente, en bucle.
    "viento": (CI, "Environment/ambient_wind.wav", True),
    "bosque_dia": (TOM, "BGS Loops/Forest Day/Forest Day.ogg", True),
    "bosque_noche": (TOM, "BGS Loops/Forest Night/Forest Night.ogg", True),
    "lluvia": (TOM, "BGS Loops/Forest Day/Forest Day Rain.ogg", True),
    "tormenta": (TOM, "BGS Loops/Forest Day/Forest Day Storm.ogg", True),
    "rio": (TOM, "SFX/Waterfalls Rivers and Streams/River Stream Loop.ogg", True),
    "cascada": (TOM, "SFX/Waterfalls Rivers and Streams/Waterfall Loop.ogg", True),
    "mar": (TOM, "BGS Loops/Sea/Sea.ogg", True),
    "cueva": (TOM, "BGS Loops/Cave/Cave.ogg", True),
    "fuego": (TOM, "SFX/Torch/Torch Loop.ogg", True),
    # Los pasos, segun el suelo (cuatro de cada uno, para no repetir).
    **{f"paso_{suelo}_{k}": (TOM, f"SFX/Footsteps/{carpeta}/{carpeta} Walk {k}.ogg", False)
       for suelo, carpeta in (("tierra", "Dirt"), ("piedra", "Stone"), ("agua", "Water"), ("madera", "Wood"))
       for k in range(1, 5)},
    # Las aves, de vez en cuando (en el bosque de dia y en la estepa).
    "ave": (EXTRA, "bird_tweet.wav", False),
    "cuervo_1": (EXTRA, "crow_1.wav", False),
    "cuervo_2": (EXTRA, "crow_2.wav", False),
    "cuervos": (EXTRA, "crows_multiple.wav", False),
    "grajilla": (EXTRA, "jackdaw.wav", False),
    # El agua y el cuerpo.
    "chapoteo": (EXTRA, "splash_quick.wav", False),
    "beber": (EXTRA, "human_gulp.wav", False),
    "comer": (CI, "Other/munching_food.wav", False),
    **{f"quejido_{k}": (CI, f"Human/man_{k}.wav", False) for k in range(1, 4)},
    # El combate.
    **{f"espada_tajo_{k}": (TOM, f"SFX/Attacks/Sword Attacks Hits and Blocks/Sword Attack {k}.ogg", False) for k in range(1, 4)},
    **{f"espada_impacto_{k}": (TOM, f"SFX/Attacks/Sword Attacks Hits and Blocks/Sword Impact Hit {k}.ogg", False) for k in range(1, 4)},
    **{f"espada_bloqueo_{k}": (TOM, f"SFX/Attacks/Sword Attacks Hits and Blocks/Sword Blocked {k}.ogg", False) for k in range(1, 4)},
    "espada_parry": (TOM, "SFX/Attacks/Sword Attacks Hits and Blocks/Sword Parry 1.ogg", False),
    **{f"arco_disparo_{k}": (TOM, f"SFX/Attacks/Bow Attacks Hits and Blocks/Bow Attack {k}.ogg", False) for k in range(1, 3)},
    **{f"flecha_impacto_{k}": (TOM, f"SFX/Attacks/Bow Attacks Hits and Blocks/Bow Impact Hit {k}.ogg", False) for k in range(1, 3)},
    "golpe_puno": (CI, "Combat and Gore/punch.wav", False),
    "patada": (CI, "Combat and Gore/kick.wav", False),
    # El trabajo: martillar o forjar.
    **{f"martillo_{k}": (TOM, f"SFX/Chopping and Mining/mine {k}.ogg", False) for k in range(1, 3)},
}


def volumen_medio(ruta):
    """El volumen medio y el pico (dB), de ffmpeg volumedetect."""
    r = subprocess.run(["ffmpeg", "-hide_banner", "-nostats", "-i", ruta, "-af", "volumedetect", "-f", "null", "-"],
                       capture_output=True, text=True)
    media = float(re.search(r"mean_volume: (-?[\d.]+) dB", r.stderr).group(1))
    pico = float(re.search(r"max_volume: (-?[\d.]+) dB", r.stderr).group(1))
    return media, pico


HZ = 22050
FUNDIDO = 1.0  # los segundos en que el final de un bucle se funde con su principio


def sin_costura(x):
    """El bucle con su final fundido en el principio (a potencia constante): el ultimo trozo de x pasa a sonar debajo
    del primero, asi el sonido sigue sin salto al volver a empezar. Queda mas corto por lo que dura el fundido."""
    c = min(int(FUNDIDO * HZ), len(x) // 4)
    t = np.linspace(0.0, 1.0, c, endpoint=False, dtype=np.float32)
    y = x[:len(x) - c].copy()
    y[:c] = x[:c] * np.sqrt(t) + x[len(x) - c:] * np.sqrt(1.0 - t)
    return y


def convertir(origen, destino, bucle):
    media, pico = volumen_medio(origen)
    # Los bucles, a -24 dB de media (sin pasar de -1 de pico); los golpes, con el pico a -3 dB.
    gan = min(-24.0 - media, -1.0 - pico) if bucle else -3.0 - pico
    cod = ["-c:a", "libvorbis", "-q:a", "0" if bucle else "2", destino]
    if not bucle:
        subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-i", origen, "-ac", "1", "-ar", str(HZ),
                        "-af", f"volume={gan:.2f}dB"] + cod, check=True)
        return
    crudo = subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-i", origen, "-ac", "1", "-ar", str(HZ),
                            "-af", f"volume={gan:.2f}dB", "-f", "f32le", "-"], capture_output=True, check=True).stdout
    y = sin_costura(np.frombuffer(crudo, np.float32))
    subprocess.run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-y", "-f", "f32le", "-ar", str(HZ), "-ac", "1",
                    "-i", "-"] + cod, input=np.clip(y, -1.0, 1.0).astype(np.float32).tobytes(), check=True)


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    base = sys.argv[1]
    os.makedirs(SALIDA, exist_ok=True)
    usados = {}
    for sid, (paquete, archivo, bucle) in SONIDOS.items():
        origen = os.path.join(base, paquete, archivo)
        if not os.path.isfile(origen):
            raise SystemExit(f"{sid}: no esta {origen}")
        destino = os.path.join(SALIDA, sid + ".ogg")
        convertir(origen, destino, bucle)
        usados.setdefault(paquete, []).append((sid, archivo))
        print(f"{sid}: {archivo} ({os.path.getsize(destino) // 1024} KB)")
    for viejo in os.listdir(SALIDA):  # sin los que ya no estan en SONIDOS
        if viejo.endswith(".ogg") and viejo[:-4] not in SONIDOS:
            os.remove(os.path.join(SALIDA, viejo))
    lineas = ["# Créditos de los sonidos", "",
              "Generado por `tools/sonido/sonidos.py` (no editar a mano). Cada sonido del juego, de dónde sale.", ""]
    for paquete, lista in usados.items():
        nombre, url, licencia = PAQUETES[paquete]
        lineas += [f"## {nombre}", "", f"{url + ' · ' if url else ''}{licencia}.", ""]
        lineas += [f"- `{sid}.ogg`: {archivo}" for sid, archivo in lista] + [""]
    open(os.path.join(SALIDA, "CREDITOS.md"), "w", encoding="utf-8").write("\n".join(lineas))
    total = sum(os.path.getsize(os.path.join(SALIDA, f)) for f in os.listdir(SALIDA))
    print(f"{len(SONIDOS)} sonidos, {total / 1048576:.1f} MB en assets/sonidos")


if __name__ == "__main__":
    main()
