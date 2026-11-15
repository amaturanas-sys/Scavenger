#!/usr/bin/env python3
"""Guarda en el repo una imagen de referencia hecha con Perchance: el paso 0 de la tubería de modelos.

La imagen la genera el usuario, o Cline en el Codespace, en el navegador (gratis y sin cuenta):
  https://perchance.org/ai-text-to-image-generator   objetos, animales, estructuras
  https://perchance.org/ai-character-generator       personas
Este script no la genera, porque Perchance no tiene una API oficial: la baja de su enlace, o copia el archivo que
se subió al Codespace, a arte/referencias/<id>/<vista>.<ext>. Anota de dónde salió (el prompt, el enlace, el
sha256 y la fecha) en arte/referencias/referencias.json. Usa solo la biblioteca estándar.

Las vistas:
  hoja      la hoja de tres vistas (frente | lado | espalda) en una imagen: para piezas.py y los voxeles
  unica     una sola vista del sujeto, de tres cuartos: para TRELLIS.2. Nunca la hoja: TRELLIS hace un modelo
            por cada vista
  frente, lado, espalda   una vista suelta

Después:
  hoja:   python3 tools/modelos/piezas.py <id> --hoja arte/referencias/<id>/hoja.<ext> --forma <la que toque>
  unica:  subirla a TRELLIS.2 y traer el GLB con python3 tools/modelos/modelos.py traer <id> --url <GLB>
Las pautas y los prompts, en la skill modelos (.claude/skills/modelos/SKILL.md, paso 2).
"""
import argparse
import hashlib
import json
import os
import re
import sys
import urllib.error
import urllib.request
from datetime import datetime, timezone

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))
CARPETA = os.path.join(ROOT, "arte", "referencias")
REGISTRO = os.path.join(CARPETA, "referencias.json")
INVENTARIO = os.path.join(ROOT, "assets", "inventario.tsv")
VISTAS = ("hoja", "unica", "frente", "lado", "espalda")
GENERADORES = {
    "texto": "https://perchance.org/ai-text-to-image-generator",
    "personaje": "https://perchance.org/ai-character-generator",
}
MAX_BYTES = 15 * 1024 * 1024    # más que esto no es una imagen de referencia
AVISO_BYTES = 2 * 1024 * 1024   # va en git: mejor un JPEG más chico


def tipo_de_imagen(data):
    """La extensión según los primeros bytes (PNG, JPEG o WebP), o None si no es una imagen."""
    if data.startswith(b"\x89PNG\r\n\x1a\n"):
        return "png"
    if data.startswith(b"\xff\xd8\xff"):
        return "jpg"
    if data[:4] == b"RIFF" and data[8:12] == b"WEBP":
        return "webp"
    return None


def bajar(url):
    if not url.startswith("https://"):
        raise SystemExit("--url: solo enlaces https://")
    pedido = urllib.request.Request(url, headers={"User-Agent": "estepa-referencias/1.0"})
    try:
        with urllib.request.urlopen(pedido, timeout=60) as resp:
            data = resp.read(MAX_BYTES + 1)
    except urllib.error.HTTPError as e:
        raise SystemExit(f"No se pudo bajar la imagen (HTTP {e.code}). Si el enlace es temporal o necesita la "
                         "página abierta, guarda la imagen, súbela al Codespace y usa --archivo.")
    except urllib.error.URLError as e:
        raise SystemExit(f"No se pudo bajar la imagen: {e.reason}")
    if len(data) > MAX_BYTES:
        raise SystemExit("La imagen pasa de 15 MB.")
    return data


def leer_archivo(ruta):
    if not os.path.isfile(ruta):
        raise SystemExit(f"--archivo: no existe {ruta}")
    if os.path.getsize(ruta) > MAX_BYTES:
        raise SystemExit("La imagen pasa de 15 MB.")
    with open(ruta, "rb") as fh:
        return fh.read()


def ids_del_inventario():
    with open(INVENTARIO, encoding="utf-8") as fh:
        return {linea.split("\t", 1)[0] for linea in fh if linea.strip()}


def cargar_registro():
    if not os.path.exists(REGISTRO):
        return {}
    with open(REGISTRO, encoding="utf-8") as fh:
        return json.load(fh)


def main(argv=None):
    ap = argparse.ArgumentParser(
        prog="fetch_perchance_ref.py",
        description="Guarda una imagen de referencia de Perchance en arte/referencias/<id>/ y anota de dónde salió.",
        epilog="Ejemplo: python3 tools/fetch_perchance_ref.py animal.salvaje.lobo --url https://... --vista hoja "
               "--prompt \"turnaround reference sheet of a single grey wolf, ...\"")
    ap.add_argument("id", help="el id del inventario (assets/inventario.tsv), p. ej. animal.salvaje.lobo")
    fuente = ap.add_mutually_exclusive_group(required=True)
    fuente.add_argument("--url", help="el enlace https de la imagen generada")
    fuente.add_argument("--archivo", help="la imagen ya subida al Codespace")
    ap.add_argument("--vista", choices=VISTAS, default="hoja",
                    help="hoja: tres vistas para piezas.py y los voxeles; unica: una vista para TRELLIS.2 "
                         "(por omisión, hoja)")
    ap.add_argument("--prompt", default="", help="el prompt con el que se generó (queda en el registro)")
    ap.add_argument("--generador", choices=sorted(GENERADORES), default="texto",
                    help="texto: ai-text-to-image-generator; personaje: ai-character-generator")
    ap.add_argument("--reemplazar", action="store_true", help="pisa la referencia de esa vista si ya estaba")
    a = ap.parse_args(argv)

    if not re.fullmatch(r"[a-z0-9_]+(\.[a-z0-9_]+){2}", a.id):
        raise SystemExit(f"{a.id}: un id es categoria.sub.nombre, en minúsculas (ver assets/inventario.tsv)")
    if a.id not in ids_del_inventario():
        print(f"Aviso: {a.id} no está en assets/inventario.tsv; agrega su fila antes de modelarlo.", file=sys.stderr)

    data = bajar(a.url) if a.url else leer_archivo(a.archivo)
    ext = tipo_de_imagen(data)
    if ext is None:
        raise SystemExit("No es una imagen PNG, JPEG ni WebP (¿bajó una página en lugar de la imagen?).")

    carpeta = os.path.join(CARPETA, a.id)
    previas = [n for n in os.listdir(carpeta) if os.path.splitext(n)[0] == a.vista] if os.path.isdir(carpeta) else []
    if previas and not a.reemplazar:
        raise SystemExit(f"Ya hay una referencia {a.vista} de {a.id} ({previas[0]}): --reemplazar para pisarla.")
    os.makedirs(carpeta, exist_ok=True)
    for nombre in previas:
        os.remove(os.path.join(carpeta, nombre))
    destino = os.path.join(carpeta, f"{a.vista}.{ext}")
    with open(destino, "wb") as fh:
        fh.write(data)

    rel = os.path.relpath(destino, ROOT).replace(os.sep, "/")
    registro = cargar_registro()
    registro.setdefault(a.id, {})[a.vista] = {
        "archivo": rel,
        "fuente": "perchance",
        "generador": GENERADORES[a.generador],
        "url": a.url or "",
        "prompt": a.prompt,
        "sha256": hashlib.sha256(data).hexdigest(),
        "bytes": len(data),
        "fecha": datetime.now(timezone.utc).isoformat(timespec="seconds"),
    }
    with open(REGISTRO, "w", encoding="utf-8") as fh:
        json.dump(registro, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")

    print(f"Guardada {rel} ({len(data) // 1024} KB) y anotada en arte/referencias/referencias.json.")
    if len(data) > AVISO_BYTES:
        print("Aviso: pasa de 2 MB y va en git; mejor un JPEG más chico.", file=sys.stderr)
    if a.vista == "hoja":
        print(f"Sigue: python3 tools/modelos/piezas.py {a.id} --hoja {rel} --forma <plana|revolucion|disco|"
              "secciones|volumen> (skill modelos).")
    elif a.vista == "unica":
        print(f"Sigue: subirla a TRELLIS.2 y traer el GLB con python3 tools/modelos/modelos.py traer {a.id} "
              "--url <GLB> --fuente trellis2 (skill modelos).")
    return 0


if __name__ == "__main__":
    sys.exit(main())
