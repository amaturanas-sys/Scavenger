#!/usr/bin/env python3
"""Referencias de una sola vista para AniGen, desde las hojas de tres vistas.

AniGen (https://huggingface.co/spaces/VAST-AI/AniGen) arma el modelo con su esqueleto desde UNA imagen de un solo
sujeto: con la hoja de tres vistas hace tres animales en una malla. Este script toma cada hoja de
arte/referencias/_raw_multiview/ (la tabla HOJAS dice qué es cada una), separa sus tres vistas, elige las dos que
pide el esqueleto del inventario (rig), les quita el fondo y las sombras con rembg (el modelo isnet-general-use) y
las deja centradas sobre un color plano, en un cuadrado de 1024 px:

  arte/referencias/<id>/unica_01.png   la vista principal, la que pide el esqueleto
  arte/referencias/<id>/unica_02.png   la que la complementa

y las anota en arte/referencias/referencias.json, el mismo registro de tools/fetch_perchance_ref.py.

Las vistas, por esqueleto:
  humanoide   unica_01 de frente y unica_02 de espalda. AniGen quiere la pose en A (los brazos a 45°): si la hoja
              no la tiene, la referencia queda «con reparos».
  cuadrupedo  unica_01 de perfil, mirando a la izquierda (si la hoja mira a la derecha, se espeja), y unica_02 de
              frente
  ave         unica_01 de espaldas y unica_02 de frente, las dos con las alas extendidas

El fondo, según el color del sujeto:
  #FFFFFF  pelaje o plumaje pardo, gris, negro o rojizo; ropa oscura
  #000000  blanco o muy claro (casi la mitad del sujeto o más)
  #00FF00  de dos colores, manchado o rayado (con partes casi blancas que se perderían en el blanco), y lo que la
           tabla FONDO_FIJO dice

El borde: el color de cada pixel a medias se despeja del gris de la hoja antes de ponerlo sobre el fondo nuevo, así
no queda un halo gris.

Uso:
  pip install "rembg[cpu]" pillow numpy scipy   (una vez; rembg baja el modelo, 170 MB, la primera vez)
  python3 tools/modelos/preparar_refs_anigen.py                       todas las hojas
  python3 tools/modelos/preparar_refs_anigen.py --id animal.salvaje.tigre
  python3 tools/modelos/preparar_refs_anigen.py --check               revisa lo hecho, sin rembg

El chequeo (--check) mira cada referencia del registro: que el archivo exista y tenga su sha256, que el fondo sea el
color exacto en todo lo que no es el sujeto, que el sujeto no toque el margen (5 %) y que sea una sola pieza.
"""
import argparse
import hashlib
import json
import os
import sys
from datetime import datetime, timezone

import numpy as np
from PIL import Image
from scipy import ndimage

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
CARPETA = os.path.join(ROOT, "arte", "referencias")
HOJAS_DIR = os.path.join(CARPETA, "_raw_multiview")
REGISTRO = os.path.join(CARPETA, "referencias.json")
INVENTARIO = os.path.join(ROOT, "assets", "inventario.tsv")
ANIGEN = "https://huggingface.co/spaces/VAST-AI/AniGen"
MODELO = "isnet-general-use"  # rembg: el que mejor deja afuera la sombra del suelo
TAM = 1024                    # el lado del cuadrado
RELLENO = 0.80                # lo que ocupa el lado mayor del sujeto
ESCALA_MAX = 1.6              # cuánto se puede agrandar una vista chica
MARGEN_MIN = 0.05             # el margen mínimo que pide el chequeo, por lado

BLANCO, NEGRO, VERDE = (255, 255, 255), (0, 0, 0), (0, 255, 0)

# Las hojas (de la carpeta «imágenes generadas en 3 planos» del Drive del usuario, hechas con Canva IA): su
# disposición, el orden de sus vistas, los ids que salen de ella, si su perfil mira a la derecha (se espeja) y lo
# que se vio al revisarla. «columnas»: las tres vistas una al lado de otra; «filas»: una debajo de otra; «paneles»:
# columnas separadas por líneas blancas. Los nombres c01..c32 son los de arte/modelos/models_manifest.json.
H, C, A = "humanoide", "cuadrupedo", "ave"
POSE_A = "brazos caídos junto al cuerpo, no en A: AniGen puede pegarlos al torso"
HOJAS = {
    "c01.jpg": ("paneles", ("frente", "lado", "espalda"), ["personaje.base.cuerpo_comun_2"], False, POSE_A,
                "Humanoids/Imagen generada con Canva IA 18 oct 2026, 22_27_01.jpg"),
    "c02.jpg": ("paneles", ("frente", "lado", "espalda"), ["personaje.base.cuerpo_mujer"], False, POSE_A,
                "Humanoids/Imagen generada con Canva IA 18 oct 2026, 22_30_31.jpg"),
    "c03.jpg": ("paneles", ("frente", "lado", "espalda"), ["personaje.base.cuerpo_comun"], False, POSE_A,
                "Humanoids/Imagen generada con Canva IA 18 oct 2026, 22_35_33.jpg"),
    "c04.jpg": ("paneles", ("frente", "lado", "espalda"), ["personaje.base.cuerpo_mujer_2"], False, POSE_A,
                "Humanoids/Imagen generada con Canva IA 18 oct 2026, 22_36_38.jpg"),
    "c05.jpg": ("paneles", ("frente", "lado", "espalda"), ["personaje.base.cuerpo_comun_3"], False, POSE_A,
                "Humanoids/Imagen generada con Canva IA 18 oct 2026, 22_38_14.jpg"),
    "c06.jpg": ("paneles", ("frente", "lado", "espalda"), ["personaje.base.cuerpo_mujer_3"], False, POSE_A,
                "Humanoids/Imagen generada con Canva IA 18 oct 2026, 22_39_09.jpg"),
    "c07.jpg": ("paneles", ("frente", "lado", "espalda"), ["personaje.base.cuerpo_robusto"], False, POSE_A,
                "Humanoids/Imagen generada con Canva IA 18 oct 2026, 22_40_20.jpg"),
    "c08.jpg": ("paneles", ("frente", "lado", "espalda"), ["personaje.base.cuerpo_mujer_4"], False, POSE_A,
                "Humanoids/Imagen generada con Canva IA 18 oct 2026, 22_41_28.jpg"),
    "c09.jpg": ("paneles", ("frente", "lado", "espalda"), ["personaje.base.cuerpo_comun_4"], False, POSE_A,
                "Humanoids/Imagen generada con Canva IA 18 oct 2026, 22_43_43.jpg"),
    "c10.jpg": ("paneles", ("frente", "lado", "espalda"), ["personaje.base.cuerpo_mujer_5"], False, POSE_A,
                "Humanoids/Imagen generada con Canva IA 18 oct 2026, 22_44_36.jpg"),
    "c11.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.montura.caballo_estepario"], False, "",
                "Animals/Imagen generada con Canva IA 18 oct 2026, 23_44_01.jpg"),
    "c12.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.montura.caballo_estepario_2"], False, "",
                "Animals/Imagen generada con Canva IA 18 oct 2026, 23_46_36.jpg"),
    "c13.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.montura.caballo_guerra"], False, "",
                "Animals/Imagen generada con Canva IA 18 oct 2026, 23_47_33.jpg"),
    "c14.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.montura.caballo_estepario_3"], False, "",
                "Animals/Imagen generada con Canva IA 18 oct 2026, 23_48_25.jpg"),
    "c15.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.salvaje.lobo"], False, "",
                "Animals/Imagen generada con Canva IA 18 oct 2026, 23_49_41.jpg"),
    "c16.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.salvaje.lobo_2", "animal.salvaje.coyote"], False,
                "", "Animals/Imagen generada con Canva IA 18 oct 2026, 23_50_57.jpg"),
    "c17.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.ganado.perro_pastor"], False, "",
                "Animals/Imagen generada con Canva IA 18 oct 2026, 23_52_13.jpg"),
    "c18.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.salvaje.perro_salvaje"], False, "",
                "Animals/Imagen generada con Canva IA 18 oct 2026, 23_53_09.jpg"),
    "c19.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.salvaje.oso"], False, "",
                "Animals/Imagen generada con Canva IA 18 oct 2026, 23_54_17.jpg"),
    "c20.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.salvaje.oso_polar"], False, "",
                "Animals/Imagen generada con Canva IA 18 oct 2026, 23_55_45.jpg"),
    "c21.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.montura.dromedario"], False, "",
                "Animals/Imagen generada con Canva IA 18 oct 2026, 23_56_54.jpg"),
    "c22.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.salvaje.jabali"], False, "",
                "Animals/Imagen generada con Canva IA 19 oct 2026, 10_21_55.jpg"),
    "c23.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.salvaje.ciervo"], False, "",
                "Animals/Imagen generada con Canva IA 19 oct 2026, 10_23_11.jpg"),
    "c24.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.salvaje.cierva"], False, "",
                "Animals/Imagen generada con Canva IA 19 oct 2026, 10_24_04.jpg"),
    "c25.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.salvaje.puma"], False, "",
                "Animals/Imagen generada con Canva IA 19 oct 2026, 10_25_29.jpg"),
    "c26.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.salvaje.tigre"], False, "",
                "Animals/Imagen generada con Canva IA 19 oct 2026, 10_26_31.jpg"),
    "c27.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.ganado.buey"], False, "",
                "Animals/Imagen generada con Canva IA 19 oct 2026, 10_29_00.jpg"),
    "c28.jpg": ("columnas", ("frente", "lado", "espalda"), ["animal.ganado.vaca"], False, "",
                "Animals/Imagen generada con Canva IA 19 oct 2026, 10_30_21.jpg"),
    "c29.jpg": ("filas", ("frente", "lado", "espalda"), ["animal.ave.cuervo"], False, "",
                "Animals/Imagen generada con Canva IA 19 oct 2026, 10_31_48.jpg"),
    "c30.jpg": ("filas", ("frente", "lado", "espalda"), ["animal.ave.aguila"], False, "",
                "Animals/Imagen generada con Canva IA 19 oct 2026, 10_32_53.jpg"),
    "c31.jpg": ("filas", ("frente", "lado", "espalda"), ["animal.ave.buitre"], False, "",
                "Animals/Imagen generada con Canva IA 19 oct 2026, 10_34_14.jpg"),
}
# Las que no sirven para AniGen, y por qué (quedan en _raw_multiview/ pero no salen referencias).
SIN_REFERENCIA = {
    "c32.jpg": ("animal.acuatico.peces", "es un banco de cientos de peces, no un pez solo: hace falta una hoja de un pez"),
}
# Las vistas que se toman, por esqueleto: (unica_01, unica_02).
VISTAS_POR_RIG = {H: ("frente", "espalda"), C: ("lado", "frente"), A: ("espalda", "frente")}
# El fondo de los que el pedido nombra (rayados, manchados, el cuervo brillante, el oso polar).
FONDO_FIJO = {
    "animal.salvaje.tigre": (VERDE, "rayado"),
    "animal.ganado.vaca": (VERDE, "manchada"),
    "animal.salvaje.leopardo_nieves": (VERDE, "manchado"),
    "animal.ave.cuervo": (VERDE, "negro brillante (iridiscente)"),
    "animal.ave.aguila": (VERDE, "de dos colores: la cabeza y la cola claras se perderían en el blanco"),
    "animal.salvaje.oso_polar": (NEGRO, "blanco"),
    "personaje.base.cuerpo_mujer_3": (NEGRO, "ropa beige, tela clara"),
}


def hexa(c):
    return "#%02X%02X%02X" % tuple(c)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def rigs_del_inventario():
    rigs = {}
    with open(INVENTARIO, encoding="utf-8") as fh:
        for linea in fh:
            if linea.startswith("#") or not linea.strip():
                continue
            c = linea.rstrip("\n").split("\t")
            etiquetas = dict(t.split(":", 1) for t in c[2].split(",") if ":" in t) if len(c) > 2 else {}
            rigs[c[0]] = etiquetas.get("rig", "")
    return rigs


# ---------------------------------------------------------------- separar las vistas
def fondo_local(rgb):
    """El color del fondo en cada pixel: la hoja tiene viñeta y manchas suaves, no un gris parejo."""
    h, w, _ = rgb.shape
    b = max(4, int(0.02 * min(h, w)))
    borde = np.concatenate([rgb[:b].reshape(-1, 3), rgb[-b:].reshape(-1, 3), rgb[:, :b].reshape(-1, 3),
                            rgb[:, -b:].reshape(-1, 3)])
    gris = np.median(borde, axis=0)
    claro = np.abs(rgb - gris).max(axis=2) < 30
    m = claro.astype(np.float32)
    sig = (h / 16.0, w / 16.0)
    num = np.stack([ndimage.gaussian_filter(rgb[..., k] * m, sig) for k in range(3)], axis=2)
    den = ndimage.gaussian_filter(m, sig)[..., None]
    return np.where(den > 1e-3, num / np.maximum(den, 1e-3), gris)


def separadores(rgb):
    """Las columnas de las líneas blancas que separan los paneles (las hojas de personas)."""
    blanco = (rgb.min(axis=2) > 232).mean(axis=0)
    cols = np.where(blanco > 0.85)[0]
    return cols


def tramos(perfil, n, minimo):
    """Los n tramos con sujeto de un perfil (pixeles de sujeto por columna o por fila): se descarta el ruido (menos
    del 1 % de la masa) y se juntan los más cercanos (la punta de una cola, un cuerno suelto) hasta que quedan n."""
    act = np.concatenate([[False], perfil > minimo, [False]])
    bordes = np.flatnonzero(np.diff(act.astype(np.int8)))
    t = [[int(bordes[i]), int(bordes[i + 1])] for i in range(0, len(bordes), 2)]
    total = float(perfil.sum())
    t = [x for x in t if perfil[x[0]:x[1]].sum() >= 0.01 * total]
    while len(t) > n:
        k = min(range(len(t) - 1), key=lambda i: t[i + 1][0] - t[i][1])
        t[k] = [t[k][0], t[k + 1][1]]
        t.pop(k + 1)
    if len(t) != n:
        raise ValueError(f"se esperaban {n} vistas y se encontraron {len(t)}")
    return t


def recortes(rgb, disposicion):
    """Las tres vistas de la hoja: (x0, y0, x1, y1) de cada una, en orden (izquierda a derecha o arriba abajo)."""
    h, w, _ = rgb.shape
    fondo = fondo_local(rgb)
    sujeto = np.abs(rgb - fondo).max(axis=2) > 24
    if disposicion == "paneles":
        sep = separadores(rgb)
        cortes = [0] + [int(c) for c in sep] + [w]
        paneles = [(a + 3, b - 3) for a, b in zip(cortes[:-1], cortes[1:]) if b - a > w * 0.12]
        if len(paneles) == 3:
            return [(a, 0, b, h) for a, b in paneles]
        sujeto[:, sep] = False  # sin las líneas (o sin paneles: como columnas)
        disposicion = "columnas"
    sujeto = ndimage.binary_opening(sujeto, iterations=2)
    eje = 0 if disposicion == "columnas" else 1
    perfil = sujeto.sum(axis=eje)
    largo = w if disposicion == "columnas" else h
    t = tramos(perfil, 3, max(2, int(0.004 * (h if eje == 0 else w))))
    pad = int(0.03 * largo)
    cajas = []
    for i, (a, b) in enumerate(t):
        lo = (t[i - 1][1] + a) // 2 if i > 0 else 0
        hi = (b + t[i + 1][0]) // 2 if i < len(t) - 1 else largo
        a2, b2 = max(lo, a - pad), min(hi, b + pad)
        cajas.append((a2, 0, b2, h) if disposicion == "columnas" else (0, a2, w, b2))
    return cajas


# ---------------------------------------------------------------- cortar el sujeto
_SESION = None


def mate(crop):
    """El alfa del sujeto (0..1) con rembg: deja afuera el fondo y la sombra del suelo."""
    global _SESION
    from rembg import new_session, remove  # solo al procesar: --check no lo necesita

    if _SESION is None:
        _SESION = new_session(MODELO)
    m = remove(Image.fromarray(crop.astype(np.uint8)), session=_SESION, only_mask=True)
    return np.asarray(m, dtype=np.float32) / 255.0


def limpiar(alfa):
    """Una sola pieza: se quedan las partes grandes (el sujeto) y se van los textos, las motas y lo de las otras vistas.
    Devuelve el alfa limpio y si el sujeto toca el borde del recorte (puede haber quedado cortado)."""
    alfa = np.where(alfa < 0.03, 0.0, alfa)
    lab, n = ndimage.label(alfa > 0.5)
    if n == 0:
        raise ValueError("rembg no encontró el sujeto")
    areas = ndimage.sum(np.ones_like(alfa), lab, index=range(1, n + 1))
    grande = float(areas.max())
    quedan = [i + 1 for i, a in enumerate(areas) if a >= 0.03 * grande]
    solido = np.isin(lab, quedan)
    zona = ndimage.binary_dilation(solido, iterations=6)
    alfa = np.where(zona, alfa, 0.0)
    # Los huecos chicos encerrados en el sujeto (una mancha blanca del pelaje que rembg tomó por fondo) se rellenan;
    # los grandes (entre las patas y la cola) quedan abiertos.
    huecos = ndimage.binary_fill_holes(solido) & ~solido
    lh, nh = ndimage.label(huecos)
    if nh:
        ah = ndimage.sum(np.ones_like(alfa), lh, index=range(1, nh + 1))
        chicos = [i + 1 for i, a in enumerate(ah) if a < 0.002 * solido.sum()]
        alfa = np.where(np.isin(lh, chicos), 1.0, alfa)
    b = 2
    toca = bool(solido[:b].any() or solido[-b:].any() or solido[:, :b].any() or solido[:, -b:].any())
    return alfa, toca, len(quedan)


def despejar(crop, alfa):
    """El color del sujeto sin el gris de la hoja en los pixeles a medias (I = a·F + (1-a)·B), y extendido afuera
    (el del pixel sólido más cercano) para que al achicar o agrandar no se meta el fondo en el borde."""
    fuera = alfa < 0.02
    m = fuera.astype(np.float32)
    num = np.stack([ndimage.gaussian_filter(crop[..., k] * m, 12) for k in range(3)], axis=2)
    den = ndimage.gaussian_filter(m, 12)[..., None]
    fondo = np.where(den > 1e-3, num / np.maximum(den, 1e-3), np.median(crop[fuera], axis=0) if fuera.any() else 200.0)
    a = alfa[..., None]
    f = np.clip((crop - (1.0 - a) * fondo) / np.maximum(a, 1e-3), 0, 255)
    solido = alfa >= 0.6
    _, (iy, ix) = ndimage.distance_transform_edt(~solido, return_indices=True)
    cercano = crop[iy, ix]
    return np.where(a >= 0.3, f, cercano)


def elegir_fondo(color, alfa, mid):
    """El fondo de más contraste con el sujeto: blanco, negro o verde (las reglas de arriba)."""
    if mid in FONDO_FIJO:
        c, motivo = FONDO_FIJO[mid]
        return c, motivo
    px = color[alfa >= 0.9]
    if len(px) == 0:
        return BLANCO, "sin pixeles sólidos"
    y = px @ np.array([0.299, 0.587, 0.114])
    claro, oscuro = float((y > 200).mean()), float((y < 70).mean())
    if claro > 0.45:
        return NEGRO, f"claro ({claro:.0%} del sujeto)"
    if claro > 0.06:  # las patas o el vientre blancos se perderían en el blanco
        return VERDE, f"de dos tonos ({claro:.0%} claro, {oscuro:.0%} oscuro)"
    return BLANCO, f"pardo, gris u oscuro ({claro:.0%} claro)"


def componer(color, alfa, fondo, tam=TAM, relleno=RELLENO):
    """El sujeto centrado en un cuadrado de tam px sobre el color exacto del fondo."""
    ys, xs = np.nonzero(alfa > 0.02)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    f = color[y0:y1, x0:x1]
    a = alfa[y0:y1, x0:x1]
    alto, ancho = a.shape
    k = min(relleno * tam / max(alto, ancho), ESCALA_MAX)
    nw, nh = max(1, round(ancho * k)), max(1, round(alto * k))
    fi = np.asarray(Image.fromarray(np.clip(f, 0, 255).astype(np.uint8)).resize((nw, nh), Image.LANCZOS), np.float32)
    ai = np.asarray(Image.fromarray((a * 255).astype(np.uint8)).resize((nw, nh), Image.LANCZOS), np.float32)
    ai = np.where(ai <= 2, 0.0, ai) / 255.0
    lienzo = np.empty((tam, tam, 3), np.float32)
    lienzo[:] = fondo
    oy, ox = (tam - nh) // 2, (tam - nw) // 2
    zona = lienzo[oy:oy + nh, ox:ox + nw]
    aa = ai[..., None]
    lienzo[oy:oy + nh, ox:ox + nw] = aa * fi + (1.0 - aa) * zona
    out = np.clip(np.rint(lienzo), 0, 255).astype(np.uint8)
    out[oy:oy + nh, ox:ox + nw][ai == 0] = fondo  # el fondo, exacto
    return out


# ---------------------------------------------------------------- revisar
def revisar(path, fondo):
    """Lo que pide AniGen de la imagen: fondo del color exacto, márgenes, una sola pieza. Devuelve los problemas."""
    img = np.asarray(Image.open(path).convert("RGB"))
    tam = img.shape[0]
    problemas = []
    if img.shape[0] != img.shape[1]:
        problemas.append("no es cuadrada")
    distinto = np.any(img != np.array(fondo, np.uint8), axis=2)
    if not distinto.any():
        return ["está vacía"]
    ys, xs = np.nonzero(distinto)
    margen = min(ys.min(), xs.min(), tam - 1 - ys.max(), tam - 1 - xs.max())
    if margen < MARGEN_MIN * tam:
        problemas.append(f"margen de {margen} px (menos del {MARGEN_MIN:.0%})")
    lab, n = ndimage.label(ndimage.binary_closing(distinto, iterations=2))
    if n > 1:
        areas = sorted(ndimage.sum(np.ones(distinto.shape), lab, index=range(1, n + 1)), reverse=True)
        sueltas = [a for a in areas[1:] if a > 0.002 * areas[0]]
        if sueltas:
            problemas.append(f"{len(sueltas)} piezas sueltas además del sujeto")
    return problemas


# ---------------------------------------------------------------- registro
def cargar_registro():
    if not os.path.isfile(REGISTRO):
        return {}
    with open(REGISTRO, encoding="utf-8") as fh:
        return json.load(fh)


def guardar_registro(reg):
    with open(REGISTRO, "w", encoding="utf-8") as fh:
        json.dump(reg, fh, ensure_ascii=False, indent=2, sort_keys=True)
        fh.write("\n")


def procesar(hoja, ids_pedidos, rigs, reg, informe):
    disp, orden, ids, espejar, reparo, origen = HOJAS[hoja]
    ids = [i for i in ids if not ids_pedidos or i in ids_pedidos]
    if not ids:
        return
    ruta = os.path.join(HOJAS_DIR, hoja)
    with open(ruta, "rb") as fh:
        crudo = fh.read()
    rgb = np.asarray(Image.open(ruta).convert("RGB"), np.float32)
    cajas = recortes(rgb, disp)
    rig = rigs.get(ids[0], "")
    if rig not in VISTAS_POR_RIG:
        rig = H if ids[0].startswith("personaje.") else rig
    vistas = {}
    for nombre in VISTAS_POR_RIG[rig]:
        x0, y0, x1, y1 = cajas[orden.index(nombre)]
        crop = rgb[y0:y1, x0:x1]
        alfa, toca, piezas = limpiar(mate(crop))
        color = despejar(crop, alfa)
        espejada = bool(espejar and nombre == "lado")
        if espejada:
            color, alfa = color[:, ::-1], alfa[:, ::-1]
        vistas[nombre] = (color, alfa, toca, piezas, espejada)
    principal = VISTAS_POR_RIG[rig][0]
    fecha = datetime.now(timezone.utc).isoformat(timespec="seconds")
    for mid in ids:
        fondo, motivo = elegir_fondo(vistas[principal][0], vistas[principal][1], mid)
        carpeta = os.path.join(CARPETA, mid)
        os.makedirs(carpeta, exist_ok=True)
        for n, nombre in enumerate(VISTAS_POR_RIG[rig], 1):
            color, alfa, toca, piezas, espejada = vistas[nombre]
            img = componer(color, alfa, fondo)
            clave = f"unica_{n:02d}"
            destino = os.path.join(carpeta, clave + ".png")
            Image.fromarray(img).save(destino, optimize=True)
            with open(destino, "rb") as fh:
                data = fh.read()
            problemas = revisar(destino, fondo)
            if toca:
                problemas.append("el sujeto toca el borde de su vista en la hoja: revisar que no falte nada")
            reparos = [reparo] if reparo else []
            if mid == "animal.salvaje.coyote":
                reparos.append("es la hoja del lobo (c16), como el modelo actual: no hay hoja de un coyote")
            estado = "no lista" if problemas else ("con reparos" if reparos else "lista")
            previo = reg.get(mid, {}).get(clave, {})
            rel = os.path.relpath(destino, ROOT).replace(os.sep, "/")
            reg.setdefault(mid, {})[clave] = {
                "archivo": rel,
                "fuente": "hoja de tres vistas (Canva IA)",
                "generador": "Canva IA",
                "url": "",
                "prompt": "",
                "sha256": sha256(data),
                "bytes": len(data),
                "fecha": previo.get("fecha", fecha) if previo.get("sha256") == sha256(data) else fecha,
                "hoja": os.path.relpath(ruta, ROOT).replace(os.sep, "/"),
                "hoja_sha256": sha256(crudo),
                "hoja_origen": "Drive, carpeta «imágenes generadas en 3 planos»: " + origen,
                "vista": nombre,
                "espejada": espejada,
                "fondo": hexa(fondo),
                "fondo_motivo": motivo,
                "rig": rig,
                "destino": ANIGEN,
                "herramienta": f"tools/modelos/preparar_refs_anigen.py (rembg {MODELO})",
                "estado": estado,
                "notas": "; ".join(problemas + reparos),
            }
            informe.append((mid, clave, nombre, hexa(fondo), estado, "; ".join(problemas + reparos)))
            print(f"  {rel}: {nombre}, fondo {hexa(fondo)} ({motivo}), {estado}" +
                  (f" — {'; '.join(problemas + reparos)}" if problemas or reparos else ""))


def chequear(reg):
    malos = 0
    for mid in sorted(reg):
        for clave, e in sorted(reg[mid].items()):
            if not clave.startswith("unica_") or "fondo" not in e:
                continue
            ruta = os.path.join(ROOT, e["archivo"])
            if not os.path.isfile(ruta):
                print(f"ERROR {mid} {clave}: falta {e['archivo']}")
                malos += 1
                continue
            with open(ruta, "rb") as fh:
                data = fh.read()
            if sha256(data) != e["sha256"]:
                print(f"ERROR {mid} {clave}: el sha256 no coincide con el registro")
                malos += 1
            fondo = tuple(int(e["fondo"][i:i + 2], 16) for i in (1, 3, 5))
            problemas = revisar(ruta, fondo)
            if problemas:
                print(f"ERROR {mid} {clave}: {'; '.join(problemas)}")
                malos += 1
    print(f"{malos} errores." if malos else "Todas las referencias de AniGen están bien (fondo exacto, márgenes, una pieza).")
    return 1 if malos else 0


def main(argv=None):
    ap = argparse.ArgumentParser(description="Referencias de una sola vista para AniGen, desde las hojas de tres vistas.")
    ap.add_argument("--id", action="append", default=[], help="solo este id (se puede repetir)")
    ap.add_argument("--check", action="store_true", help="revisa las referencias ya hechas (sin rembg)")
    a = ap.parse_args(argv)
    reg = cargar_registro()
    if a.check:
        return chequear(reg)
    rigs = rigs_del_inventario()
    informe = []
    for hoja in sorted(HOJAS):
        if not os.path.isfile(os.path.join(HOJAS_DIR, hoja)):
            print(f"Falta {hoja} en {os.path.relpath(HOJAS_DIR, ROOT)}: se salta.", file=sys.stderr)
            continue
        if a.id and not set(a.id) & set(HOJAS[hoja][2]):
            continue
        print(hoja)
        procesar(hoja, set(a.id), rigs, reg, informe)
        guardar_registro(reg)
    for hoja, (mid, motivo) in sorted(SIN_REFERENCIA.items()):
        if not a.id or mid in a.id:
            print(f"{hoja}: {mid} sin referencia: {motivo}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
