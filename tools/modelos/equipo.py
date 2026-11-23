#!/usr/bin/env python3
"""El equipo de Props: armas, escudos, herramientas, la silla, cascos, corazas y botas, desde las mallas de
Hunyuan3D que subio el usuario (arte/fuentes/props/modelos) y el color del render de referencia del que salio cada
una (arte/fuentes/props/imagenes).

  python3 tools/modelos/equipo.py lista              las recetas: id, malla, imagen y forma
  python3 tools/modelos/equipo.py hacer [id ...]     convierte todas (o las dadas) y las marca en el inventario
  python3 tools/modelos/equipo.py emparejar MALLA GRUPO [GRUPO ...]
                                                     las imagenes de esos grupos (casco_bronce, lanza...) que mejor
                                                     calzan con la silueta de la malla, y con que giro

Cada receta (RECETAS) une una malla con su imagen y su id. `az` es el giro (grados, alrededor de +Y) que pone la
malla como la muestra el render: se eligio por la silueta (emparejar) y se reviso a ojo.

Lo rigido (armas, escudos, herramientas, la silla) sale como en piezas.py: low-poly hasta el tris_max del
inventario, caras planas, la textura de la imagen proyectada de frente (<= 256 px, filtro de punto), +X el frente.
El origen: las armas, en la empunadura (la hoja hacia +Y y el filo hacia +Z, como las toma el juego); los escudos,
en el centro de atras, mirando a +X; lo demas, en el centro de la base.

Las prendas (cascos, corazas, botas) se ajustan a cada cuerpo base en su pose de piel: el casco baja hasta apoyarse
en la cabeza, la coraza envuelve el torso y cada bota calza su pie; lo que queda adentro del cuerpo se empuja hacia
afuera. Las caras de adentro (las que miran al cuerpo) se quitan: no se ven y asi el tris_max rinde el doble. Llevan
el esqueleto del cuerpo (sus 56 huesos, en su pose) y los pesos de la piel mas cercana: el juego las mueve con la
animacion del cuerpo y no traen clips (assets/models/README.md). Un archivo por cuerpo: el del comun, sin sufijo, y
nombre@mujer, nombre@gigante y nombre@protagonista junto a el.

Los tonos (hierro, cuero, fieltro, oro) recolorean el render cuando la forma calza con un id de otro material.
"""
import argparse
import hashlib
import io
import json
import os
import struct
import sys
from datetime import datetime, timezone

import numpy as np
from PIL import Image
from scipy import ndimage
from scipy.spatial import cKDTree

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "assets"))
import inventario  # noqa: E402
import modelos  # noqa: E402
from piezas import _rellenar, glb_textura, plana  # noqa: E402

ROOT = inventario.ROOT
FUENTES = os.path.join(ROOT, "arte", "fuentes", "props")
CUERPOS = (("", "personaje.base.cuerpo_comun"), ("mujer", "personaje.base.cuerpo_mujer"),
           ("gigante", "personaje.base.cuerpo_gigante"), ("protagonista", "personaje.narrativo.protagonista"))

# id, malla, imagen, forma y lo propio de cada una:
#   az, inv: el giro del render (grados alrededor de +Y; inv, la malla cabeza abajo)
#   derecha: la malla viene acostada; se para por su eje largo (armas y herramientas)
#   plana: hojas y astas: la silueta de frente extruida (piezas.plana), no la malla reducida; minimo: su ancho y
#          su grosor mas finos (m)
#   punta: "arriba", el extremo ancho (la cabeza de la lanza, de la maza) va hacia +Y; "abajo", hacia -Y (la cruz de
#          la espada: la hoja arriba)
#   agarre: armas, la altura de la empunadura (fraccion del largo, desde abajo)
#   filo: "convexo" (sable: el lomo curvo hacia +Z) o "cabeza" (hacha, pico: la cabeza hacia +Z)
#   giro: lo rigido, grados alrededor de +Y despues de poner el frente en +X; escala: la medida del inventario que
#         manda (alto, por defecto; largo o ancho)
#   tono: recolorear el render; tex: lado de la textura; k: holgura de la prenda (1 = justa)
RECETAS = [
    # Armas
    dict(id="arma.corta.sable", malla="sable", img="sable-3", forma="arma", derecha=True, plana=True, punta="abajo", agarre=0.07,
         filo="convexo"),
    dict(id="arma.corta.maza", malla="maza_pinchos", img="maza_pinchos-1", forma="arma", derecha=True, punta="arriba",
         agarre=0.12),
    dict(id="arma.corta.daga", malla="daga", img="daga-1", forma="arma", derecha=True, plana=True, punta="abajo", agarre=0.12),
    dict(id="arma.larga.espada_larga", malla="espada", img="espada-3", forma="arma", derecha=True, plana=True, punta="abajo",
         agarre=0.08),
    dict(id="arma.larga.lanza", malla="lanza", img="lanza-1", forma="arma", derecha=True, plana=True, minimo=0.03, punta="arriba", agarre=0.38),
    dict(id="arma.larga.alabarda", malla="alabarda", img="hacha_asta-2", forma="arma", derecha=True, plana=True, minimo=0.03, punta="arriba",
         agarre=0.35, filo="cabeza"),
    dict(id="arma.distancia.arco_simple", malla="arco", img="arco_simple-2", forma="arma", derecha=True, plana=True, minimo=0.025, arco=True, agarre=0.5),
    dict(id="utileria.objeto.antorcha", malla="antorcha", img="antorcha-2", forma="arma", derecha=True, plana=True, minimo=0.035,
         punta="arriba", agarre=0.25),
    dict(id="utileria.herramienta.pico", malla="pico", img="pico-1", forma="arma", derecha=True, plana=True, minimo=0.03, punta="arriba",
         agarre=0.3, filo="cabeza"),
    # Escudos (de frente en la malla y en el render)
    dict(id="escudo.mano.umbo", malla="escudo_redondo_1", img="escudo_bronce-4", forma="escudo", tex=256),
    dict(id="escudo.mano.culto", malla="escudo_redondo_2", img="escudo_bronce-3", forma="escudo", tex=256),
    dict(id="escudo.mano.cuero", malla="escudo_redondo_3", img="escudo_madera-1", forma="escudo", tex=256),
    # Objetos
    dict(id="utileria.objeto.aljaba", malla="aljaba", img="aljaba-2", forma="objeto", az=330),
    dict(id="utileria.herramienta.gancho_trepa", malla="gancho_trepa", img="gancho_trepa-1", forma="objeto", az=330),
    dict(id="accesorio.arreo.silla_montar", malla="silla_montar", img="silla_montar-4", forma="objeto", az=330, tex=256,
         giro=90, escala="largo", origen="asiento"),
    # Cascos
    dict(id="armadura.casco.fieltro", malla="gorro_piel", img="gorro_piel-1", forma="casco", tex=256, az=0),
    dict(id="armadura.casco.culto", malla="casco_calado", img="casco_bronce-1", forma="casco", tex=256, az=30),
    dict(id="armadura.casco.mascara_culto", malla="casco_visera", img="casco_escamas-3", forma="casco", tex=256, az=330),
    dict(id="armadura.casco.laminar_cuero", malla="casco_costillas", img="casco_hierro-3", forma="casco", tex=256, az=0,
         tono="cuero"),
    dict(id="armadura.casco.bronce", malla="casco_carrilleras", img="casco_bronce-2", forma="casco", tex=256, az=30),
    dict(id="armadura.casco.escamas_hierro", malla="casco_nasal", img="casco_hierro-4", forma="casco", tex=256, az=0),
    dict(id="armadura.casco.malla", malla="casco_ala", img="casco_hierro-1", forma="casco", tex=256, az=0),
    dict(id="armadura.casco.oro_padre", malla="casco_aguila", img="casco_aguila-1", forma="casco", tex=256, az=270, tono="oro"),
    # Corazas
    dict(id="armadura.torso.fieltro", malla="coraza_faldar", img="coraza_cuero-4", forma="torso", tono="fieltro",
         tex=256),
    dict(id="armadura.torso.laminar_cuero", malla="chaleco_largo", img="chaleco_cuero-1", forma="torso", tex=256),
    dict(id="armadura.torso.escamas_hierro", malla="laminar_hombreras", img="laminar_hombreras-1", forma="torso",
         tono="hierro", tex=256, hombreras=True),
    dict(id="armadura.torso.culto", malla="coraza_lisa", img="pectoral_oro-1", forma="torso", tex=256),
    # Botas (el par, una por pie)
    dict(id="armadura.botas.laminar_cuero", malla="botas_montar", img="botas_montar-4", forma="botas", tex=256),
    dict(id="armadura.botas.fieltro", malla="botas_montar", img="botas_fieltro-2", forma="botas", tex=256),
]
PRENDAS = ("casco", "torso", "botas")


def log(msg):
    print(msg, flush=True)


# ---------------------------------------------------------------- lectura
def leer_glb(path):
    with open(path, "rb") as fh:
        data = fh.read()
    n = struct.unpack("<I", data[12:16])[0]
    doc = json.loads(data[20:20 + n])
    off = 20 + n
    bl = struct.unpack("<I", data[off:off + 4])[0]
    return doc, data[off + 8:off + 8 + bl]


def accesor(doc, binario, i):
    a = doc["accessors"][i]
    bv = doc["bufferViews"][a["bufferView"]]
    dt = {5126: "<f4", 5125: "<u4", 5123: "<u2", 5121: "u1"}[a["componentType"]]
    nc = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[a["type"]]
    o = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
    size = np.dtype(dt).itemsize * nc
    st = bv.get("byteStride", size)
    raw = np.frombuffer(binario, np.uint8, count=st * (a["count"] - 1) + size, offset=o)
    out = np.lib.stride_tricks.as_strided(raw, (a["count"], size), (st, 1)).copy().view(dt).reshape(a["count"], nc)
    if a.get("normalized") and dt == "u1":
        out = out.astype(np.float32) / 255.0
    return out


def malla(nombre):
    """Vertices y caras de una malla de arte/fuentes/props/modelos (todas sus partes, soldadas)."""
    import trimesh
    m = trimesh.load(os.path.join(FUENTES, "modelos", nombre + ".glb"), force="mesh", process=False)
    # Soldada solo por la posicion: con textura (el casco del aguila), las costuras de la UV la partian en pedazos
    # sueltos, y al reducirla quedaban astillas.
    v = np.asarray(m.vertices, float)
    _, idx, inv = np.unique(np.round(v / (np.ptp(v, axis=0).max() * 1e-6)).astype(np.int64), axis=0, return_index=True,
                            return_inverse=True)
    f = inv.reshape(-1)[np.asarray(m.faces, np.int64)]
    f = f[(f[:, 0] != f[:, 1]) & (f[:, 1] != f[:, 2]) & (f[:, 0] != f[:, 2])]
    return v[idx], f


def mascara(rgb, umbral=40):
    """La silueta del objeto en el render: lo que no es el color del fondo (el de los bordes), sin las motas."""
    a = rgb.astype(np.int16)
    bg = np.median(np.concatenate([a[0], a[-1], a[:, 0], a[:, -1]]), axis=0)
    m = ndimage.binary_opening(np.abs(a - bg).sum(axis=2) > umbral, iterations=1)
    lab, n = ndimage.label(m)
    if n:
        sizes = ndimage.sum(m, lab, range(1, n + 1))
        m = np.isin(lab, 1 + np.flatnonzero(sizes >= sizes.max() * 0.05))
    return ndimage.binary_fill_holes(m)


def imagen(nombre):
    """El render y su silueta. Con fondo verde, sin el verde que se le pega al borde (lo fino: un asta, una cuerda):
    el verde no pasa del mayor de rojo y azul."""
    rgb = np.asarray(Image.open(os.path.join(FUENTES, "imagenes", nombre + ".jpg")).convert("RGB"))
    mask = mascara(rgb)
    a = rgb.astype(np.int16)
    bg = np.median(np.concatenate([a[0], a[-1], a[:, 0], a[:, -1]]), axis=0)
    if bg[1] > bg[0] + 30 and bg[1] > bg[2] + 30:
        a = a.copy()
        a[..., 1] = np.minimum(a[..., 1], np.maximum(a[..., 0], a[..., 2]) + 6)
        rgb = a.astype(np.uint8)
    return rgb, mask


# ---------------------------------------------------------------- giros
def rot_y(grados):
    a = np.radians(grados)
    c, s = np.cos(a), np.sin(a)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])


ROT_Z180 = np.diag([-1.0, -1.0, 1.0])
ROT_X180 = np.diag([1.0, -1.0, -1.0])
A_JUEGO = rot_y(90)  # el frente del render (+Z, hacia quien mira) pasa a +X


def parar(v):
    """Acostada -> parada: el eje mas largo a Y, el mediano a X (el ancho en el render), el corto a Z."""
    c = v - (v.min(0) + v.max(0)) / 2
    o = np.argsort(-np.ptp(c, axis=0))
    out = np.column_stack([c[:, o[1]], c[:, o[0]], c[:, o[2]]])
    if np.linalg.det(np.eye(3)[[o[1], o[0], o[2]]]) < 0:
        out[:, 2] *= -1  # que siga siendo una rotacion (las caras hacia afuera)
    return out


def ancho_extremos(v, frac=0.15):
    """(ancho abajo, ancho arriba): lo que ocupa en X y Z cerca de cada punta del eje Y."""
    lo, hi = v[:, 1].min(), v[:, 1].max()
    h = hi - lo
    abajo, arriba = v[v[:, 1] <= lo + frac * h], v[v[:, 1] >= hi - frac * h]
    return (np.ptp(abajo[:, [0, 2]], axis=0).max(), np.ptp(arriba[:, [0, 2]], axis=0).max())


def ancho_extremos_img(mask, frac=0.15):
    rows = np.flatnonzero(mask.any(axis=1))
    r0, r1 = rows[0], rows[-1]
    h = r1 - r0 + 1
    def ancho(sl):
        cols = np.flatnonzero(mask[sl].any(axis=0))
        return cols[-1] - cols[0] + 1 if len(cols) else 0
    return ancho(slice(r1 - int(frac * h), r1 + 1)), ancho(slice(r0, r0 + int(frac * h) + 1))  # (abajo, arriba)


def silueta(p2, f, cols, rows):
    """La silueta de las caras proyectadas (p2: columna, fila en pixeles) en una mascara rows x cols."""
    from PIL import ImageDraw
    img = Image.new("L", (cols, rows), 0)
    d = ImageDraw.Draw(img)
    for t in f:
        d.polygon([(p2[i, 0], p2[i, 1]) for i in t], fill=255, outline=255)
    return np.asarray(img) > 0


def coincide(v, f, mask, n=64):
    """Interseccion sobre union de la silueta de frente de la malla con la del render (las dos en su caja)."""
    def normal(m):
        ys, xs = np.nonzero(m)
        c = m[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
        return np.asarray(Image.fromarray(c.astype(np.uint8) * 255).resize((n, n * 4), Image.BILINEAR)) > 127
    x0, x1, y0, y1 = caja_xy(v)
    s = max(x1 - x0, y1 - y0) / 400
    sil = silueta(np.column_stack([(v[:, 0] - x0) / s, (y1 - v[:, 1]) / s]), f, int((x1 - x0) / s) + 2,
                  int((y1 - y0) / s) + 2)
    a, b = normal(sil), normal(mask)
    return (a & b).sum() / max((a | b).sum(), 1)


def a_imagen(v, rec, mask, f=None):
    """La malla como la muestra el render: +Y arriba, el ancho en X y la vista desde +Z."""
    if rec.get("derecha"):
        v = parar(v)
        # La punta ancha del mismo lado que en el render (una lanza con la cabeza arriba, una espada con la cruz
        # arriba): las siluetas finas no lo deciden.
        ma, mb = ancho_extremos(v)
        ia, ib = ancho_extremos_img(mask)
        if (mb > ma) != (ib > ia):
            v = v @ ROT_Z180.T
        # Y no en espejo (el arco: la madera del lado de la madera): de los dos lados, el que mejor calza.
        if f is not None:
            g = v @ rot_y(180).T
            if coincide(g, f, mask) > coincide(v, f, mask):
                v = g
    v = v @ rot_y(rec.get("az", 0)).T
    if rec.get("inv"):
        v = v @ ROT_Z180.T
    return v


# ---------------------------------------------------------------- textura
TONOS = {  # rampa de oscuro a claro (RGB 0-255) por luminancia
    "hierro": ((28, 30, 34), (98, 102, 110), (190, 194, 200)),
    "cuero": ((38, 22, 12), (118, 72, 38), (196, 146, 96)),
    "fieltro": ((58, 52, 44), (142, 130, 110), (206, 196, 174)),
    "oro": ((62, 40, 10), (186, 136, 40), (250, 222, 130)),
}


def tonar(rgb, tono):
    lum = (rgb @ np.array([0.299, 0.587, 0.114])) / 255.0
    lum = (lum - lum.min()) / max(np.ptp(lum), 1e-6)
    a, b, c = (np.array(x, float) for x in TONOS[tono])
    t = lum[..., None]
    return np.where(t < 0.5, a + (b - a) * (t / 0.5), b + (c - b) * ((t - 0.5) / 0.5))


def textura(rec, rgb, mask):
    """El recorte del render (su silueta), con lo de afuera del color de adentro mas cercano y sin la luz grande del
    render (se divide por la luminancia muy borrosa), en dos mitades de un cuadrado potencia de dos: arriba, el
    frente; abajo, para las caras de atras (el render no muestra la espalda), lo mismo sin los huecos (la abertura de
    la cara de un casco, el cuello de una coraza), rellenos con el color de alrededor."""
    ys, xs = np.nonzero(mask)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    crop, cm = rgb[y0:y1, x0:x1], mask[y0:y1, x0:x1]
    frente = _rellenar(crop, cm, umbral=40, borde=2)
    hueco = np.all(np.abs(frente - np.array([34.0, 28.0, 22.0])) < 0.5, axis=2)
    lleno = cm & ~hueco
    _, (iy, ix) = ndimage.distance_transform_edt(~ndimage.binary_erosion(lleno, iterations=2) if lleno.sum() > 50 else ~lleno,
                                                 return_indices=True)
    espalda = frente[iy, ix]
    size = rec.get("tex", 128)
    mitades = []
    for out in (frente, espalda):
        lum = out @ np.array([0.299, 0.587, 0.114])
        blur = ndimage.gaussian_filter(lum, sigma=max(out.shape[:2]) / 5.0)
        out = out * np.clip((blur.mean() / np.maximum(blur, 1.0)) ** 0.45, 0.7, 1.5)[..., None]
        if rec.get("tono"):
            out = tonar(np.clip(out, 0, 255), rec["tono"])
        mitades.append(np.asarray(Image.fromarray(np.clip(out, 0, 255).astype(np.uint8)).resize((size, size // 2), Image.LANCZOS)))
    # Con paleta (128 colores, sin tramado), como las texturas de PS2: pesa un tercio (cada prenda va 4 veces, una por
    # cuerpo) y no se nota a 640x360.
    return Image.fromarray(np.concatenate(mitades, axis=0)).quantize(128, method=Image.Quantize.MEDIANCUT,
                                                                       dither=Image.Dither.NONE)


def uv_caras(uv, f, atras):
    """La UV de cada esquina de cada cara (F x 3 x 2), en la mitad de arriba de la textura (el frente) o, con
    atras (la normal de la cara mira hacia atras del render), en la de abajo."""
    out = uv[f].copy()
    out[..., 1] = out[..., 1] * 0.5 + np.where(atras, 0.5, 0.0)[:, None]
    return out


def uvs(v_img, caja):
    """De frente: la caja (x0, x1, y0, y1) de la malla cae sobre el recorte de la silueta (0..1, v hacia abajo)."""
    x0, x1, y0, y1 = caja
    u = (v_img[:, 0] - x0) / max(x1 - x0, 1e-9)
    w = (y1 - v_img[:, 1]) / max(y1 - y0, 1e-9)
    return np.clip(np.column_stack([u, w]), 0.0, 1.0)


def caja_xy(v):
    return (v[:, 0].min(), v[:, 0].max(), v[:, 1].min(), v[:, 1].max())


# ---------------------------------------------------------------- malla
def compactar(v, f):
    usados, inv = np.unique(f, return_inverse=True)
    return v[usados], inv.reshape(f.shape), usados


def reducir(v, f, tris):
    import fast_simplification
    if len(f) <= tris:
        return v, f
    v2, f2 = fast_simplification.simplify(v.astype(np.float32), f.astype(np.int32), target_count=int(tris))
    v2, f2 = np.asarray(v2, float), np.asarray(f2, np.int64)
    for _ in range(6):  # a veces se pasa por unas pocas caras
        if len(f2) <= tris:
            break
        v2, f2 = fast_simplification.simplify(v2.astype(np.float32), f2.astype(np.int32), target_count=int(tris) - 2)
        v2, f2 = np.asarray(v2, float), np.asarray(f2, np.int64)
    if len(f2) > tris:  # muchas piezas sueltas (no bajan mas): se van las caras mas chicas
        area = np.linalg.norm(np.cross(v2[f2[:, 1]] - v2[f2[:, 0]], v2[f2[:, 2]] - v2[f2[:, 0]]), axis=1)
        f2 = f2[np.sort(np.argsort(-area)[:tris])]
    keep = np.linalg.norm(np.cross(v2[f2[:, 1]] - v2[f2[:, 0]], v2[f2[:, 2]] - v2[f2[:, 0]]), axis=1) > 1e-12
    return compactar(v2, f2[keep])[:2]


def normales_caras(v, f):
    n = np.cross(v[f[:, 1]] - v[f[:, 0]], v[f[:, 2]] - v[f[:, 0]])
    return n / np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)


def hacia_afuera(v, f):
    """Todas las caras hacia afuera: las vecinas (por una arista) giran en el mismo sentido, y cada pieza, con su
    volumen con signo positivo. Algunas mallas (el casco del aguila) traen caras en los dos sentidos: sin esto, al
    quitar las caras de adentro se iba media cascara de afuera."""
    from collections import deque
    from scipy.sparse import coo_matrix
    from scipy.sparse.csgraph import connected_components
    n = len(f)
    dirigidas = np.concatenate([f[:, [0, 1]], f[:, [1, 2]], f[:, [2, 0]]])
    cara = np.tile(np.arange(n), 3)
    clave = np.sort(dirigidas, axis=1)
    orden = np.lexsort((clave[:, 1], clave[:, 0]))
    clave, dirigidas, cara = clave[orden], dirigidas[orden], cara[orden]
    igual = np.all(clave[1:] == clave[:-1], axis=1)
    i = np.flatnonzero(igual)  # pares de caras que comparten la arista (las aristas de mas de dos, de a pares)
    a_, b_ = cara[i], cara[i + 1]
    mismo = dirigidas[i, 0] == dirigidas[i + 1, 0]  # la recorren en el mismo sentido: una de las dos esta al reves
    vecinos = [[] for _ in range(n)]
    for x, y, m in zip(a_.tolist(), b_.tolist(), mismo.tolist()):
        vecinos[x].append((y, m))
        vecinos[y].append((x, m))
    vuelta = np.zeros(n, bool)
    visto = np.zeros(n, bool)
    for semilla in range(n):
        if visto[semilla]:
            continue
        visto[semilla] = True
        cola = deque([semilla])
        while cola:
            x = cola.popleft()
            for y, m in vecinos[x]:
                if not visto[y]:
                    visto[y] = True
                    vuelta[y] = vuelta[x] ^ m
                    cola.append(y)
    f = f.copy()
    f[vuelta] = f[vuelta][:, [0, 2, 1]]
    _, comp = connected_components(coo_matrix((np.ones(len(a_)), (a_, b_)), shape=(n, n)), directed=False)
    # Cada pieza, con su volumen con signo positivo (un solido, aunque este abierto en un borde: el casco del aguila,
    # de doble pared, abierto abajo); si casi no encierra volumen (una cascara de una sola capa), afuera es lejos de
    # su centro.
    tri = v[f]
    cen = tri.mean(axis=1)
    area_n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    k = comp.max() + 1
    centro = np.stack([np.bincount(comp, weights=cen[:, i], minlength=k) for i in range(3)], axis=1) / \
        np.maximum(np.bincount(comp, minlength=k), 1)[:, None]
    rel = tri - centro[comp][:, None, :]
    vol = np.bincount(comp, weights=np.einsum("ij,ij->i", rel[:, 0], np.cross(rel[:, 1], rel[:, 2])) / 6.0, minlength=k)
    area = np.bincount(comp, weights=np.linalg.norm(area_n, axis=1) / 2.0, minlength=k)
    lejos = np.bincount(comp, weights=np.einsum("ij,ij->i", area_n, cen - centro[comp]), minlength=k)
    solido = np.abs(vol) > 0.002 * area ** 1.5  # encierra algo: el volumen manda
    signo = np.where(solido, vol, lejos)
    al_reves = signo[comp] < 0
    f[al_reves] = f[al_reves][:, [0, 2, 1]]
    return f


# ---------------------------------------------------------------- lo rigido
def de_silueta(vi, f, caja, alto, tris, filas=480, minimo=0.0, solo_madera=False):
    """Lo plano y largo (hojas, astas, arcos): la silueta de frente de la malla, extruida al grosor de su silueta de
    lado (piezas.plana). Lo fino no se pierde, como al reducir la malla. minimo (m): el ancho y el grosor mas finos
    (un asta de 1 cm no se ve a 640x360). Devuelve (v, f, uv), ya con +X el frente."""
    x0, x1, y0, y1 = caja
    s = (y1 - y0) / filas
    cols = max(int(np.ceil((x1 - x0) / s)) + 1, 2)
    fm = silueta(np.column_stack([(vi[:, 0] - x0) / s, (y1 - vi[:, 1]) / s]), f, cols, filas)
    z0 = vi[:, 2].min()
    sm = silueta(np.column_stack([(vi[:, 2] - z0) / s, (y1 - vi[:, 1]) / s]), f,
                 max(int(np.ceil(np.ptp(vi[:, 2]) / s)) + 1, 2), filas)
    if solo_madera:  # el arco: de cada fila, solo la madera (la tirada que se curva), sin la cuerda (la recta)
        filas_ok = np.flatnonzero(fm.any(axis=1))
        izq = np.array([np.flatnonzero(fm[r])[0] for r in filas_ok])
        der = np.array([np.flatnonzero(fm[r])[-1] for r in filas_ok])
        madera_izq = np.ptp(izq) > np.ptp(der)
        for r in filas_ok:
            lab, n = ndimage.label(fm[r])
            if n > 1:
                fm[r] = lab == (1 if madera_izq else n)
    grosor = None
    if minimo > 0:
        px = minimo / (alto / filas)  # pixeles de la mascara (ya en la escala del inventario)
        for r in np.flatnonzero(fm.any(axis=1)):
            c = np.flatnonzero(fm[r])
            if c[-1] - c[0] + 1 < px:
                m = (c[0] + c[-1]) / 2.0
                fm[r, max(int(np.floor(m - px / 2)), 0):min(int(np.ceil(m + px / 2)) + 1, cols)] = True
        grosor = max(float(np.median(sm.sum(axis=1)[sm.any(axis=1)])) * alto / filas, minimo)
    v, fp = plana((fm, None), (sm, None), alto, tris, grosor=grosor)
    sp = alto / filas
    uv = np.clip(np.column_stack([(cols / 2.0 - v[:, 2] / sp) / cols, (filas - v[:, 1] / sp) / filas]), 0, 1)
    return v, fp, uv


def rigida(rec, it):
    v, f = malla(rec["malla"])
    rgb, mask = imagen(rec["img"])
    vi = a_imagen(v, rec, mask, f)
    caja = caja_xy(vi)
    tris = int(it["tris_max"]) or 300
    ancho, alto, largo = modelos.dims(it)
    forma = rec["forma"]
    if rec.get("plana"):
        v, f, uv = de_silueta(vi, f, caja, alto, tris, minimo=rec.get("minimo", 0.0), solo_madera=rec.get("arco", False))
    else:
        vi, f = reducir(vi, hacia_afuera(vi, f), tris)
        uv = uvs(vi, caja)
        v = vi @ A_JUEGO.T
    if forma == "arma" and rec.get("punta"):
        # Con la vista de frente, la punta ancha quedo del lado del render; ahora, del lado que pide el juego.
        ma, mb = ancho_extremos(v)
        if (mb > ma) != (rec["punta"] == "arriba"):
            v = v @ ROT_X180.T
    if rec.get("giro"):
        v = v @ rot_y(rec["giro"]).T
    eje, medida = {"alto": (1, alto), "largo": (0, largo), "ancho": (2, ancho)}[rec.get("escala", "alto")]
    v = v * (medida / np.ptp(v[:, eje]))
    lo, hi = v.min(0), v.max(0)
    if forma == "arma":
        yg = lo[1] + rec.get("agarre", 0.1) * (hi[1] - lo[1])
        cerca = v[np.abs(v[:, 1] - yg) < 0.03 * (hi[1] - lo[1])]
        cerca = cerca if len(cerca) else v
        o = np.array([cerca[:, 0].mean(), yg, cerca[:, 2].mean()])
        v = v - o
        filo = rec.get("filo")
        if filo:  # que el filo (o la cabeza) mire a +Z
            arriba = v[v[:, 1] > 0.25 * v[:, 1].max()]
            if filo == "convexo":  # el sable: el centro de la hoja, del lado de afuera de la cuerda punta-agarre
                tip = v[np.argmax(v[:, 1])]
                medio = arriba[np.abs(arriba[:, 1] - tip[1] / 2) < 0.1 * tip[1]]
                lado = medio[:, 2].mean() - tip[2] / 2
            else:  # la cabeza del hacha o del pico: hacia donde sale, arriba
                lado = v[v[:, 1] > 0.8 * v[:, 1].max()][:, 2].mean()
            if lado < 0:
                v = v @ rot_y(180).T
    elif forma == "escudo":  # el centro de atras: la manija detras del umbo
        v = v - np.array([lo[0], (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2])
    elif rec.get("origen") == "asiento":  # la silla: el centro de abajo del asiento (los estribos cuelgan mas abajo)
        c = (lo + hi) / 2
        centro = v[(np.abs(v[:, 0] - c[0]) < 0.15 * (hi[0] - lo[0])) & (np.abs(v[:, 2] - c[2]) < 0.12 * (hi[2] - lo[2]))]
        v = v - np.array([c[0], centro[:, 1].min() if len(centro) else lo[1], c[2]])
    else:  # el centro de la base
        v = v - np.array([(lo[0] + hi[0]) / 2, lo[1], (lo[2] + hi[2]) / 2])
    tex = textura(rec, rgb, mask)
    data = glb_textura(v, f, uv_caras(uv, f, normales_caras(v, f)[:, 0] < 0), tex, rec["id"])
    return {"": data}, {"tris": int(len(f)), "caja_m": np.round(np.ptp(v, axis=0), 3).tolist()}


# ---------------------------------------------------------------- los cuerpos
CABEZA = ("head", "head_leaf")
TORSO = ("pelvis", "spine_01", "spine_02", "spine_03", "clavicle_l", "clavicle_r")


class Cuerpo:
    def __init__(self, mid):
        items, _ = inventario.load()
        it = modelos.find_item(items, mid)
        self.path = os.path.join(ROOT, inventario.path_of(it))
        self.doc, self.bin = leer_glb(self.path)
        prim = self.doc["meshes"][0]["primitives"][0]
        a = prim["attributes"]
        self.v = accesor(self.doc, self.bin, a["POSITION"]).astype(float)
        if "indices" in prim:
            self.f = accesor(self.doc, self.bin, prim["indices"]).reshape(-1, 3).astype(np.int64)
        else:  # sin indices: tres vertices por cara (como los escribe humanoide.py)
            self.f = np.arange(len(self.v), dtype=np.int64).reshape(-1, 3)
        self.j = accesor(self.doc, self.bin, a["JOINTS_0"]).astype(np.int64)
        self.w = accesor(self.doc, self.bin, a["WEIGHTS_0"]).astype(float)
        skin = self.doc["skins"][0]
        self.joints = skin["joints"]
        self.ibm = accesor(self.doc, self.bin, skin["inverseBindMatrices"])
        self.nombres = [self.doc["nodes"][n].get("name", "") for n in self.joints]
        self.hueso = {n: i for i, n in enumerate(self.nombres)}
        self.dom = self.j[np.arange(len(self.v)), np.argmax(self.w, axis=1)]
        if np.einsum("ij,ij->i", self.v[self.f[:, 0]], np.cross(self.v[self.f[:, 1]], self.v[self.f[:, 2]])).sum() < 0:
            self.f = self.f[:, [0, 2, 1]]

    def region(self, nombres):
        ids = [self.hueso[n] for n in nombres if n in self.hueso]
        return np.isin(self.dom, ids)

    def pos_hueso(self, nombre):
        m = self.ibm[self.hueso[nombre]].reshape(4, 4).T  # por columnas
        return np.linalg.inv(m)[:3, 3]

    def normales_afuera(self):
        """La normal de cada cara, hacia afuera: las mallas de los cuerpos traen caras en los dos sentidos (el comun,
        casi la mitad al reves). Afuera es lejos del hueso de la cara (del segmento del hueso a su hijo)."""
        tri = self.v[self.f]
        cen = tri.mean(axis=1)
        n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
        n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
        pos = np.array([self.pos_hueso(x) for x in self.nombres])
        hijo = np.arange(len(pos))
        for i, j in enumerate(self.joints):
            hijos = [self.joints.index(c) for c in self.doc["nodes"][j].get("children", []) if c in self.joints]
            if hijos:  # el hijo mas alejado (la columna sigue a la columna, no a la clavicula)
                hijo[i] = max(hijos, key=lambda c: np.linalg.norm(pos[c] - pos[i]))
        b = self.dom[self.f[:, 0]]
        a, d = pos[b], pos[hijo[b]] - pos[b]
        t = np.clip(np.einsum("ij,ij->i", cen - a, d) / np.maximum(np.einsum("ij,ij->i", d, d), 1e-12), 0, 1)
        afuera = cen - (a + d * t[:, None])
        n[np.einsum("ij,ij->i", n, afuera) < 0] *= -1
        return n

    def superficie(self, vmask):
        """Las caras de esos vertices, todas hacia afuera (para saber que queda adentro: el numero de giro)."""
        sel = vmask[self.f].all(axis=1)
        n = self.normales_afuera()[sel]
        f = self.f[sel].copy()
        tri = self.v[f]
        al_reves = np.einsum("ij,ij->i", np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]), n) < 0
        f[al_reves] = f[al_reves][:, [0, 2, 1]]
        return self.v, f

    def piel(self, vmask, n=24000, semilla=3):
        """Puntos de la piel (en las caras de esos vertices) con su normal hacia afuera: para empujar hacia afuera."""
        sel = vmask[self.f].all(axis=1)
        f = self.f[sel]
        nrm = self.normales_afuera()[sel]
        tri = self.v[f]
        area = np.linalg.norm(np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]), axis=1)
        rng = np.random.default_rng(semilla)
        k = rng.choice(len(f), size=n, p=area / area.sum())
        r1, r2 = rng.random(n), rng.random(n)
        s = np.sqrt(r1)
        p = tri[k, 0] * (1 - s)[:, None] + tri[k, 1] * (s * (1 - r2))[:, None] + tri[k, 2] * (s * r2)[:, None]
        return p, nrm[k]


def adentro(q, sup):
    """El numero de giro de la superficie (caras hacia afuera) alrededor de cada punto: ~1 adentro, ~0 afuera."""
    V, F = sup
    A, B, C = V[F[:, 0]], V[F[:, 1]], V[F[:, 2]]
    out = np.zeros(len(q))
    for i in range(0, len(q), 128):
        P = q[i:i + 128, None, :]
        a, b, c = A[None] - P, B[None] - P, C[None] - P
        la, lb, lc = (np.linalg.norm(x, axis=2) for x in (a, b, c))
        det = np.einsum("pij,pij->pi", a, np.cross(b, c))
        den = la * lb * lc + np.einsum("pij,pij->pi", a, b) * lc + np.einsum("pij,pij->pi", b, c) * la + \
            np.einsum("pij,pij->pi", c, a) * lb
        out[i:i + 128] = (2 * np.arctan2(det, den)).sum(axis=1) / (4 * np.pi)
    return out


def mover(q, arbol, piel, sup, holgura, hacia):
    """Cuanto y hacia donde mover cada punto: lo de adentro sale (por hacia, si va hacia afuera de la piel, o por
    la normal de la piel) lo que lo separa de la piel mas la holgura; lo de afuera a menos de la holgura, por la
    normal, hasta la holgura. Lo demas, nada: asi no se dispara en los bordes de la region."""
    p, n = piel
    dist, k = arbol.query(q)
    d = np.einsum("ij,ij->i", q - p[k], n[k])
    dentro = adentro(q, sup) > 0.5
    cerca = ~dentro & (d < holgura) & (dist < 0.05)
    mov = np.zeros_like(q)
    if dentro.any():
        dirs = n[k[dentro]]
        if hacia is not None:
            h = hacia(q[dentro])
            ok = np.einsum("ij,ij->i", h, dirs) > 0.25
            dirs[ok] = h[ok]
        mov[dentro] = dirs * np.minimum(dist[dentro] + holgura, 0.08)[:, None]
    mov[cerca] = n[k[cerca]] * (holgura - d[cerca])[:, None]
    return mov, dentro | cerca


def empujar(v, piel, sup, holgura, veces=3, hacia=None):
    """Lo que queda adentro del cuerpo (o a menos de holgura de su piel) sale hacia afuera: por la normal de la piel
    o, con hacia (una funcion de los puntos a direcciones), en esa direccion (el torso, en horizontal desde la
    columna: por la normal, en las axilas, la coraza subia y le salian alas)."""
    arbol = cKDTree(piel[0])
    for _ in range(veces):
        mov, hay = mover(v, arbol, piel, sup, holgura, hacia)
        if not hay.any():
            break
        v = v + mov
    return v


def empujar_caras(v, f, piel, sup, holgura, veces=4, hacia=None):
    """Como empujar, mirando tambien el centro y el medio de los lados de cada cara: una cara grande (la malla
    reducida) atraviesa la piel aunque sus vertices queden afuera. Lo que falta se reparte a sus tres vertices."""
    arbol = cKDTree(piel[0])
    pesos_m = np.array([[1, 1, 1], [1, 1, 0], [0, 1, 1], [1, 0, 1]], float)
    pesos_m /= pesos_m.sum(axis=1, keepdims=True)
    for _ in range(veces):
        mov = np.zeros_like(v)
        cuenta = np.zeros(len(v))
        for w in pesos_m:
            q = (v[f] * w[None, :, None]).sum(axis=1)
            m, hay = mover(q, arbol, piel, sup, holgura, hacia)
            for c in range(3):
                np.add.at(mov, f[hay, c], m[hay])
                np.add.at(cuenta, f[hay, c], 1.0)
        if not cuenta.any():
            break
        v = v + mov / np.maximum(cuenta, 1.0)[:, None]
    return v


def desde_columna(cuerpo):
    """Afuera del torso: en horizontal, desde la linea de la columna (pelvis, columna y cuello) a esa altura."""
    ejes = np.array([cuerpo.pos_hueso(x) for x in ("pelvis", "spine_01", "spine_02", "spine_03", "neck_01")])
    def hacia(q):
        x = np.interp(q[:, 1], ejes[:, 1], ejes[:, 0])
        z = np.interp(q[:, 1], ejes[:, 1], ejes[:, 2])
        d = np.column_stack([q[:, 0] - x, np.zeros(len(q)), q[:, 2] - z])
        return d / np.maximum(np.linalg.norm(d, axis=1, keepdims=True), 1e-9)
    return hacia


def desde_punto(c):
    def hacia(q):
        d = q - c
        return d / np.maximum(np.linalg.norm(d, axis=1, keepdims=True), 1e-9)
    return hacia


def sin_caras_de_adentro(v, f, piel):
    """Quita las caras que miran a la piel (la pared de adentro del casco o de la coraza)."""
    p, _ = piel
    c = v[f].mean(axis=1)
    _, k = cKDTree(p).query(c)
    afuera = c - p[k]
    afuera /= np.maximum(np.linalg.norm(afuera, axis=1, keepdims=True), 1e-12)
    keep = np.einsum("ij,ij->i", normales_caras(v, f), afuera) > -0.2
    return f[keep]


def pesos(v, cuerpo, permitidos, k=6):
    """Los pesos de la piel mas cercana (solo los huesos permitidos), por distancia inversa; hasta 4 por vertice."""
    ids = np.array([cuerpo.hueso[n] for n in permitidos if n in cuerpo.hueso])
    base = np.isin(cuerpo.dom, ids)
    idx = np.flatnonzero(base)
    d, kk = cKDTree(cuerpo.v[idx]).query(v, k=k)
    acum = np.zeros((len(v), len(cuerpo.nombres)))
    wd = 1.0 / np.maximum(d, 1e-4)
    for c in range(k):
        bv = idx[kk[:, c]]
        for s in range(4):
            jj, ww = cuerpo.j[bv, s], cuerpo.w[bv, s] * wd[:, c]
            np.add.at(acum, (np.arange(len(v)), jj), ww)
    mask = np.zeros(len(cuerpo.nombres), bool)
    mask[ids] = True
    acum[:, ~mask] = 0.0
    top = np.argsort(-acum, axis=1)[:, :4]
    w = np.take_along_axis(acum, top, axis=1)
    w /= np.maximum(w.sum(axis=1, keepdims=True), 1e-12)
    return top, w


# ---------------------------------------------------------------- ajustar a cada cuerpo
def ajustar_casco(v, f, cuerpo, rec):
    """Del ancho del craneo (k: cuanto mas ancho el casco), centrado en el y con la cupula 1,2 cm sobre la coronilla.
    Las mallas de Hunyuan son macizas (sin el hueco de la cabeza): la tapa de abajo, que queda dentro de la cabeza,
    se va con las caras de adentro."""
    cab = cuerpo.v[cuerpo.region(CABEZA)]
    y0, y1 = cab[:, 1].min(), cab[:, 1].max()
    craneo = cab[cab[:, 1] > y0 + 0.45 * (y1 - y0)]
    cx, cz = (craneo[:, 0].min() + craneo[:, 0].max()) / 2, (craneo[:, 2].min() + craneo[:, 2].max()) / 2
    s = np.ptp(craneo[:, 2]) * rec.get("k", 1.3) / np.ptp(v[:, 2])
    v = v * s
    # La cupula: lo mas alto donde el casco todavia tiene casi la mitad de su ancho (una cresta, un aguila o una punta
    # son finas: quedan arriba), o rec["cupula"] (fraccion del alto)
    c = (v.min(0) + v.max(0)) / 2
    ys = np.linspace(v[:, 1].min(), v[:, 1].max(), 41)
    anchos = np.array([np.ptp(v[(v[:, 1] >= a) & (v[:, 1] < b), 2]) if ((v[:, 1] >= a) & (v[:, 1] < b)).sum() > 2 else 0.0
                       for a, b in zip(ys[:-1], ys[1:])])
    ancho_ok = np.flatnonzero(anchos >= 0.45 * anchos.max())
    cupula = ys[ancho_ok[-1] + 1]
    if rec.get("cupula"):
        cupula = v[:, 1].min() + rec["cupula"] * np.ptp(v[:, 1])
    v = v + np.array([cx - c[0] + rec.get("atras", 0.0), y1 + 0.012 - cupula, cz - c[2]])
    return v, s


def ajustar_torso(v, f, cuerpo, rec):
    """Del ancho y el fondo del torso (sin los brazos) a lo largo de la coraza, con el alto en proporcion al ancho;
    arriba, los hombros."""
    t = cuerpo.v[cuerpo.region(TORSO)]
    hombro = cuerpo.v[cuerpo.region(("clavicle_l", "clavicle_r"))][:, 1].max()
    k = rec.get("k", 1.1)
    yp = cuerpo.pos_hueso("spine_03")[1]
    tramo = t[np.abs(t[:, 1] - yp) < 0.06]
    for _ in range(2):  # el pecho da el alto; el alto, el tramo del torso que cubre
        sz = np.ptp(tramo[:, 2]) * k * (1.25 if rec.get("hombreras") else 1.0) / np.ptp(v[:, 2])
        alto = np.ptp(v[:, 1]) * sz
        tramo = t[(t[:, 1] < hombro - 0.04) & (t[:, 1] > hombro - alto)]
    sx = np.ptp(tramo[:, 0]) * k / np.ptp(v[:, 0])
    v = v * np.array([sx, sz, sz])
    c = (v.min(0) + v.max(0)) / 2
    cp = (tramo.min(0) + tramo.max(0)) / 2
    v = v + np.array([cp[0] - c[0], hombro + 0.02 - v[:, 1].max(), cp[2] - c[2]])
    return v, np.array([sx, sz, sz])


def ajustar_bota(v, cuerpo, lado, rec):
    """Del largo del pie, con la suela en el suelo, la punta en la del pie y en su lado."""
    pie = cuerpo.v[cuerpo.region((f"foot_{lado}", f"ball_{lado}"))]
    pie = pie[pie[:, 1] < 0.14]
    suela = v[v[:, 1] < v[:, 1].min() + 0.2 * np.ptp(v[:, 1])]
    s = np.ptp(pie[:, 0]) * rec.get("k", 1.12) / np.ptp(suela[:, 0])
    v = v * s
    suela = suela * s
    v = v + np.array([(pie[:, 0].min() + pie[:, 0].max()) / 2 - (suela[:, 0].min() + suela[:, 0].max()) / 2,
                      -v[:, 1].min(), pie[:, 2].mean() - (suela[:, 2].min() + suela[:, 2].max()) / 2])
    return v, s


def prenda(rec, it):
    v0, f0 = malla(rec["malla"])
    rgb, mask = imagen(rec["img"])
    vi = a_imagen(v0, rec, mask)
    f0 = hacia_afuera(vi, f0)
    caja = caja_xy(vi)
    tex = textura(rec, rgb, mask)
    tris = int(it["tris_max"]) or 300
    forma = rec["forma"]
    salidas, informe = {}, {}
    for sufijo, cid in CUERPOS:
        cuerpo = Cuerpo(cid)
        vb = vi @ A_JUEGO.T
        if forma == "casco":
            region, holgura, permitidos = cuerpo.region(CABEZA + ("neck_01",)), 0.012, CABEZA
            vf, s = ajustar_casco(vb.copy(), f0, cuerpo, rec)

        elif forma == "torso":
            brazos = ("upperarm_l", "upperarm_r")
            region = cuerpo.region(TORSO + ("neck_01",) + (brazos if rec.get("hombreras") else ()))
            holgura = 0.015
            permitidos = TORSO + (brazos if rec.get("hombreras") else ())
            vf, s = ajustar_torso(vb.copy(), f0, cuerpo, rec)

        else:  # botas: cada una a su pie (en el render, la de la derecha es la izquierda de quien las calza)
            vf = vb.copy()
            medio = (vb[:, 2].min() + vb[:, 2].max()) / 2
            for lado, signo in (("l", -1.0), ("r", 1.0)):
                sel = (vb[:, 2] - medio) * signo > 0
                vf[sel], _ = ajustar_bota(vb[sel].copy(), cuerpo, lado, rec)
            region = cuerpo.region(("thigh_l", "calf_l", "foot_l", "ball_l", "thigh_r", "calf_r", "foot_r", "ball_r"))
            holgura, permitidos = 0.008, None
        piel = cuerpo.piel(region)
        sup = cuerpo.superficie(region)
        if forma == "torso":
            hacia = desde_columna(cuerpo)
        elif forma == "casco":
            cab = cuerpo.v[cuerpo.region(CABEZA)]
            hacia = desde_punto(np.array([(cab[:, 0].min() + cab[:, 0].max()) / 2, cab[:, 1].min() + 0.4 * np.ptp(cab[:, 1]),
                                          (cab[:, 2].min() + cab[:, 2].max()) / 2]))
        else:
            hacia = None
        vf = empujar(vf, piel, sup, holgura, hacia=hacia)
        # Se reduce la malla cerrada (cerca del doble del tope: la mitad se va con las caras de adentro); reducida
        # despues de quitarlas, abierta, la simplificacion no bajaba (se perdia la cupula del aguila) o dejaba espinas.
        uv_full = uvs(vi, caja)
        lado_full = np.sign(vb[:, 2] - (vb[:, 2].min() + vb[:, 2].max()) / 2)
        if forma == "botas":  # dos tubos: sin las caras de adentro primero, y la malla abierta se reduce bien
            vr2, fr2, _ = compactar(vf, sin_caras_de_adentro(vf, f0, piel))
            vr2, fr2 = reducir(vr2, fr2, tris)
        else:
            objetivo = 2 * tris
            for _ in range(6):  # la meta de la malla cerrada, hasta que sin las de adentro entre en el tope
                vr2, fr2 = reducir(vf, f0, objetivo)
                fq = sin_caras_de_adentro(vr2, fr2, piel)
                if len(fq) <= tris:
                    break
                objetivo = max(int(objetivo * tris / len(fq) * 0.97), 8)
            vr2, fr2, _ = compactar(vr2, fq)
        _, kk = cKDTree(vf).query(vr2)  # cada vertice reducido toma la UV y el lado del de antes mas cercano
        uv, lado = uv_full[kk], lado_full[kk]
        vr2 = empujar(vr2, piel, sup, holgura, hacia=hacia)
        vr2 = empujar_caras(vr2, fr2, piel, sup, holgura, hacia=hacia)
        if forma == "botas":
            J = np.zeros((len(vr2), 4), np.int64)
            W = np.zeros((len(vr2), 4))
            for l, signo in (("l", -1.0), ("r", 1.0)):
                sel = lado == signo
                if sel.any():
                    J[sel], W[sel] = pesos(vr2[sel], cuerpo, (f"calf_{l}", f"foot_{l}", f"ball_{l}"))
        elif forma == "casco":
            J = np.zeros((len(vr2), 4), np.int64)
            J[:, 0] = cuerpo.hueso["head"]
            W = np.zeros((len(vr2), 4))
            W[:, 0] = 1.0
        else:
            J, W = pesos(vr2, cuerpo, permitidos)
        salidas[sufijo] = glb_prenda(vr2, fr2, uv_caras(uv, fr2, normales_caras(vr2, fr2)[:, 0] < 0), tex, rec["id"],
                                     cuerpo, J, W)
        informe[sufijo or "comun"] = {"tris": int(len(fr2)), "caja_m": np.round(np.ptp(vr2, axis=0), 3).tolist()}
    return salidas, informe


def glb_prenda(v, f, uv, tex, nombre, cuerpo, J, W):
    """Como glb_textura, con la piel: los nodos del esqueleto del cuerpo (los mismos, en su pose), sus matrices
    inversas de la pose y, por vertice, sus 4 huesos y pesos."""
    tri = v[f].reshape(-1, 3)
    nrm = np.repeat(normales_caras(v, f), 3, axis=0)
    st = uv.reshape(-1, 2)
    jj = J[f].reshape(-1, 4).astype(np.uint8)
    ww = W[f].reshape(-1, 4).astype(np.float32)
    png = io.BytesIO()
    tex.save(png, "PNG", optimize=True)
    png = png.getvalue()
    partes = [tri.astype("<f4").tobytes(), nrm.astype("<f4").tobytes(), st.astype("<f4").tobytes(), jj.tobytes(),
              ww.astype("<f4").tobytes(), np.asarray(cuerpo.ibm, "<f4").tobytes()]
    blob, views = b"", []
    for i, p in enumerate(partes):
        views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(p), **({"target": 34962} if i < 5 else {})})
        blob += p + b"\x00" * (-len(p) % 4)
    views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(png)})
    blob += png + b"\x00" * (-len(png) % 4)
    count = len(tri)
    n = len(cuerpo.joints)
    nodes = [dict(cuerpo.doc["nodes"][j]) for j in cuerpo.joints]
    if cuerpo.joints != list(range(n)):
        raise SystemExit(f"{cuerpo.path}: los huesos no son los primeros nodos")
    for nd in nodes:
        nd.pop("mesh", None)
        nd.pop("skin", None)
    nodes.append({"name": nombre, "mesh": 0, "skin": 0})
    raices = [i for i in range(n) if not any(i in nd.get("children", []) for nd in nodes[:n])]
    doc = {
        "asset": {"version": "2.0", "generator": "estepa tools/modelos/equipo.py"},
        "scene": 0, "scenes": [{"nodes": raices + [n]}], "nodes": nodes,
        "meshes": [{"name": nombre, "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2,
                                                                   "JOINTS_0": 3, "WEIGHTS_0": 4}, "material": 0}]}],
        "skins": [{"joints": list(range(n)), "skeleton": raices[0], "inverseBindMatrices": 5}],
        "materials": [{"name": "prenda", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 0,
                                                                   "roughnessFactor": 1}}],
        "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 33071}],
        "textures": [{"sampler": 0, "source": 0}],
        "images": [{"bufferView": 6, "mimeType": "image/png"}],
        "buffers": [{"byteLength": len(blob)}],
        "bufferViews": views,
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": count, "type": "VEC3",
             "min": tri.min(axis=0).tolist(), "max": tri.max(axis=0).tolist()},
            {"bufferView": 1, "componentType": 5126, "count": count, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": count, "type": "VEC2"},
            {"bufferView": 3, "componentType": 5121, "count": count, "type": "VEC4"},
            {"bufferView": 4, "componentType": 5126, "count": count, "type": "VEC4"},
            {"bufferView": 5, "componentType": 5126, "count": n, "type": "MAT4"},
        ],
    }
    js = json.dumps(doc, separators=(",", ":")).encode("utf-8")
    js += b" " * (-len(js) % 4)
    out = io.BytesIO()
    out.write(struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(js) + 8 + len(blob)))
    out.write(struct.pack("<I4s", len(js), b"JSON") + js)
    out.write(struct.pack("<I4s", len(blob), b"BIN\x00") + blob)
    return out.getvalue()


# ---------------------------------------------------------------- comandos
def ruta_variante(path, sufijo):
    if not sufijo:
        return path
    base, ext = os.path.splitext(path)
    return f"{base}@{sufijo}{ext}"


def hacer(ids=None, inventariar=True):
    items, errors = inventario.load()
    if errors:
        raise SystemExit("\n".join(errors))
    man = modelos.load_manifest()
    hechos = 0
    for rec in RECETAS:
        if ids and rec["id"] not in ids:
            continue
        it = modelos.find_item(items, rec["id"])
        if it is None:
            raise SystemExit(f"{rec['id']}: no esta en assets/inventario.tsv")
        salidas, informe = (prenda if rec["forma"] in PRENDAS else rigida)(rec, it)
        dest = os.path.join(ROOT, inventario.path_of(it))
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        for sufijo, data in salidas.items():
            with open(ruta_variante(dest, sufijo), "wb") as fh:
                fh.write(data)
        log(f"[equipo] {rec['id']}: {json.dumps(informe, ensure_ascii=False)}")
        hechos += 1
        if not inventariar:
            continue
        modelos.set_state(rec["id"], "importado" if it["estado"] == "pendiente" else it["estado"])
        fuente = os.path.join(FUENTES, "modelos", rec["malla"] + ".glb")
        with open(fuente, "rb") as fh:
            sha = hashlib.sha256(fh.read()).hexdigest()
        man["models"][rec["id"]] = {
            "file": os.path.relpath(dest, ROOT).replace(os.sep, "/"), "category": it["categoria"],
            "has_rig": rec["forma"] in PRENDAS,
            "source": {"tool": "hunyuan3d", "url": "", "file": f"arte/fuentes/props/modelos/{rec['malla']}.glb",
                       "sha256": sha, "imagen": f"arte/fuentes/props/imagenes/{rec['img']}.jpg"},
            "params": {k: v for k, v in rec.items() if k not in ("id", "malla", "img")},
            "tris": max(x["tris"] for x in (informe.values() if rec["forma"] in PRENDAS else [informe])),
            "updated_at": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        }
    if inventariar and hechos:
        modelos.save_manifest(man)
        import subprocess
        subprocess.call([sys.executable, os.path.join(HERE, "..", "assets", "inventario.py"), "doc"], stdout=subprocess.DEVNULL)
    return 0


def emparejar(nombre, grupos):
    """La silueta de la malla (de frente, girada cada 30 grados, derecha o cabeza abajo) contra las de las imagenes
    de esos grupos, por interseccion sobre union."""
    from PIL import ImageDraw

    def normal(m, n=64):
        ys, xs = np.nonzero(m)
        c = m[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
        h, w = c.shape
        s = max(h, w)
        pad = np.zeros((s, s), bool)
        pad[(s - h) // 2:(s - h) // 2 + h, (s - w) // 2:(s - w) // 2 + w] = c
        return np.asarray(Image.fromarray(pad.astype(np.uint8) * 255).resize((n, n), Image.BILINEAR)) > 127

    def silueta(v, f, R=256):
        lo, s = v.min(0), np.ptp(v, axis=0).max() + 1e-9
        p = (v - lo) / s * (R - 1)
        img = Image.new("L", (R, R), 0)
        d = ImageDraw.Draw(img)
        for t in f:
            d.polygon([(p[i, 0], R - 1 - p[i, 1]) for i in t], fill=255)
        return np.asarray(img) > 0

    v, f = malla(nombre)
    v, f = reducir(v, f, 3000)
    masks = {}
    for fn in sorted(os.listdir(os.path.join(FUENTES, "imagenes"))):
        stem = os.path.splitext(fn)[0]
        if stem.rsplit("-", 1)[0] in grupos:
            masks[stem] = normal(imagen(stem)[1])
    best = []
    for az in range(0, 360, 30):
        for inv in (False, True):
            R = (ROT_Z180 if inv else np.eye(3)) @ rot_y(az)
            sil = normal(silueta(v @ R.T, f))
            for k, m in masks.items():
                best.append(((sil & m).sum() / max((sil | m).sum(), 1), k, az, inv))
    best.sort(reverse=True)
    vistos = set()
    for iou, k, az, inv in best:
        if k not in vistos:
            vistos.add(k)
            log(f"{k}: {iou:.2f} (az {az}{', inv' if inv else ''})")


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd")
    sub.add_parser("lista")
    h = sub.add_parser("hacer")
    h.add_argument("ids", nargs="*")
    h.add_argument("--sin-inventario", action="store_true", help="no toca assets/inventario.tsv ni el manifiesto")
    e = sub.add_parser("emparejar")
    e.add_argument("malla")
    e.add_argument("grupos", nargs="+")
    a = ap.parse_args(argv)
    if a.cmd == "lista":
        for r in RECETAS:
            log(f"{r['id']:36s} {r['forma']:7s} {r['malla']:20s} {r['img']}{'  (' + r['tono'] + ')' if r.get('tono') else ''}")
        return 0
    if a.cmd == "hacer":
        return hacer(set(a.ids) or None, not a.sin_inventario)
    if a.cmd == "emparejar":
        emparejar(a.malla, set(a.grupos))
        return 0
    ap.print_help()
    return 2


if __name__ == "__main__":
    sys.exit(main())
