#!/usr/bin/env python3
"""Piezas low-poly con textura, desde una hoja de tres vistas (frente | lado | espalda), al estilo de PS2:
pocas caras, una textura chica (<= 256 px, filtro de punto) y sin luz horneada. Para lo duro: armas, escudos,
cascos; y para el volumen de personas y animales.

  python3 tools/modelos/piezas.py <id> --hoja HOJA.png --forma plana|revolucion|disco|secciones|volumen [opciones]

Formas:
  plana       la silueta del frente, extruida al grosor del lado (espadas, hachas, dagas, escudos no redondos).
  revolucion  el perfil del frente girado alrededor del eje vertical (mazas, astas).
  disco       el perfil del lado girado alrededor del eje X (escudos redondos u ovalados: la tabla, el borde y el
              umbo; el ancho, el del frente).
  secciones   una elipse por altura: el ancho del frente por el largo del lado (cascos: una sola pieza a cada
              altura, en anillos limpios).
  volumen     lo que cae dentro del frente y del lado (con secciones elipticas), alisado y reducido a
              tris_max (personas, animales: varias piezas a la misma altura, como las patas). Lo de arriba toma
              la textura de la espalda y lo de abajo la del frente (no hay vista de arriba). Con pelaje blanco
              (patas, panza), --umbral 20: con 40 el blanco se va con el fondo y las patas salen partidas.
La textura: las tres vistas de la hoja en un atlas; cada cara toma la vista que la mira (el frente, la espalda
o el lado).

Ejes y origen (assets/models/README.md): +Y arriba, +X el frente (la vista de frente de la hoja), metros (ver
Medidas). Origen:
  --agarre auto|F   armas: el centro del agarre (auto: el tramo mas angosto de abajo; F: fraccion del alto)
  --agarre escudo   el centro de atras (la manija detras del umbo)
  --agarre base     el centro de la base (cascos: el centro de la abertura; personas y animales)
  --invertir        la hoja lo dibuja al reves (la hoja de una espada hacia abajo): se gira 180 grados
                    alrededor de X, y queda hacia +Y
  --girar 90|-90    la hoja lo dibuja acostado (un sable en horizontal): el frente y la espalda se giran en su
                    plano hasta que quede parado

Medidas: manda el alto del inventario y lo demas sale de la imagen; --ancla largo, el largo; --cruz M (animales),
la altura a la cruz: la de los ejemplares mas grandes de la especie (docs/TAMANOS.md).

Otras opciones: --grosor M (plana: el grosor en metros; si no, el de la vista de lado), --cresta (secciones: la
cresta o el penacho de arriba, de otro color, va aparte como una aleta plana), --miembros (volumen: los brazos
redondos, no tan hondos como el torso), --res N (volumen: celdas del lado mas largo), --filas N --fila K
(varios objetos en la hoja), --vertical (las vistas una debajo de la otra),
--umbral U (el fondo), --voltear-lado, --textura 256, --salida RUTA (si no, la del id; personas y animales van
a arte/modelos/para_riggear/), --sin-inventario (no toca assets/inventario.tsv ni el manifiesto).
Dependencias: las de tools/modelos/requirements.txt.
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

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "assets"))
sys.path.insert(0, os.path.join(HERE, "..", "voxeles"))
import inventario  # noqa: E402
import modelos  # noqa: E402
import voxeles  # noqa: E402

TEX_MAX = modelos.TEXTURE_MAX


def log(msg):
    print(msg, flush=True)


# ---------------------------------------------------------------- contornos y triangulos
def extremos(mask):
    """Por fila (de arriba abajo): la primera y la ultima columna llenas (bordes de pixel), solo las filas con algo."""
    rows = np.flatnonzero(mask.any(axis=1))
    left = np.array([np.flatnonzero(mask[r])[0] for r in rows], dtype=np.float64)
    right = np.array([np.flatnonzero(mask[r])[-1] + 1 for r in rows], dtype=np.float64)
    return rows, left, right


def _dp(pts, tol):
    """Douglas-Peucker de una cadena abierta, en 2 o mas dimensiones (conserva los extremos)."""
    if len(pts) < 3:
        return pts
    a, b = pts[0], pts[-1]
    ab = b - a
    n = np.linalg.norm(ab)
    rel = pts - a
    if n < 1e-12:
        d = np.linalg.norm(rel, axis=1)
    else:  # la distancia a la recta de a a b
        u = ab / n
        d = np.linalg.norm(rel - np.outer(rel @ u, u), axis=1)
    i = int(np.argmax(d))
    if d[i] <= tol:
        return np.array([a, b])
    return np.concatenate([_dp(pts[: i + 1], tol)[:-1], _dp(pts[i:], tol)])


def simplificar(chain, n):
    """La cadena abierta con, como mucho, n puntos (busca la tolerancia)."""
    if len(chain) <= n:
        return chain
    lo, hi = 0.0, float(np.ptp(chain, axis=0).max())
    best = _dp(chain, hi)
    for _ in range(40):
        mid = (lo + hi) / 2
        s = _dp(chain, mid)
        if len(s) > n:
            lo = mid
        else:
            best, hi = s, mid
    return best


def contorno(mask, n):
    """El borde de la silueta (filas de una sola tirada: espadas, hachas, escudos), en pixeles (columna, fila),
    simplificado a n puntos, en sentido antihorario con y hacia arriba."""
    rows, left, right = extremos(mask)
    top, bot = rows[0], rows[-1] + 1
    lchain = np.concatenate([[[left[0], top]], np.stack([left, rows + 0.5], axis=1), [[left[-1], bot]]])
    rchain = np.concatenate([[[right[-1], bot]], np.stack([right[::-1], rows[::-1] + 0.5], axis=1), [[right[0], top]]])
    k = max(3, n // 2)
    poly = np.concatenate([simplificar(lchain, k)[:-1], simplificar(rchain, n - k + 1)[:-1]])
    # sin repetidos
    keep = np.ones(len(poly), bool)
    keep[1:] = np.hypot(*(np.diff(poly, axis=0)).T) > 1e-9
    return poly[keep]


def _area(poly):
    x, y = poly[:, 0], poly[:, 1]
    return 0.5 * float(np.dot(x, np.roll(y, -1)) - np.dot(y, np.roll(x, -1)))


def triangular(poly):
    """Ear clipping de un poligono simple (en el plano, cualquier sentido): indices de los triangulos, en el
    sentido del poligono."""
    idx = list(range(len(poly)))
    ccw = _area(poly) > 0
    tris = []

    def convexo(a, b, c):
        cross = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])
        return cross > 1e-12 if ccw else cross < -1e-12

    def dentro(p, a, b, c):
        d1 = (p[0] - b[0]) * (a[1] - b[1]) - (a[0] - b[0]) * (p[1] - b[1])
        d2 = (p[0] - c[0]) * (b[1] - c[1]) - (b[0] - c[0]) * (p[1] - c[1])
        d3 = (p[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (p[1] - a[1])
        neg = (d1 < 0) or (d2 < 0) or (d3 < 0)
        pos = (d1 > 0) or (d2 > 0) or (d3 > 0)
        return not (neg and pos)

    guard = 0
    while len(idx) > 3 and guard < 10000:
        guard += 1
        for k in range(len(idx)):
            i0, i1, i2 = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]
            a, b, c = poly[i0], poly[i1], poly[i2]
            if not convexo(a, b, c):
                continue
            if any(dentro(poly[j], a, b, c) for j in idx if j not in (i0, i1, i2)):
                continue
            tris.append((i0, i1, i2))
            idx.pop(k)
            break
        else:  # sin oreja (poligono degenerado): se corta igual
            tris.append((idx[-1], idx[0], idx[1]))
            idx.pop(0)
    if len(idx) == 3:
        tris.append(tuple(idx))
    return np.array(tris, dtype=np.int64)


# ---------------------------------------------------------------- formas (en metros: Y arriba desde la base, Z a
# la izquierda de la vista de frente, X hacia el frente)
def plana(front, side, alto, tris_max, grosor=None):
    fm, _ = front
    sm, _ = side
    H, W = fm.shape
    s = alto / H
    t = grosor or max(float(np.median(sm.sum(axis=1))), 2.0) * alto / sm.shape[0]  # el grosor: lo tipico del lado
    n = max(4, (tris_max + 4) // 4)
    poly = contorno(fm, n)
    z = (W / 2.0 - poly[:, 0]) * s
    y = (H - poly[:, 1]) * s
    tri = triangular(np.stack([z, y], axis=1))
    m = len(poly)
    v = np.concatenate([np.stack([np.full(m, t / 2), y, z], axis=1), np.stack([np.full(m, -t / 2), y, z], axis=1)])
    faces = []
    # El frente (+X) y la espalda (-X): el sentido de cada tapa, hacia afuera.
    for a, b, c in tri:
        faces.append((a, b, c))
        faces.append((m + a, m + c, m + b))
    for i in range(m):  # las paredes
        j = (i + 1) % m
        faces.append((i, m + i, m + j))
        faces.append((i, m + j, j))
    return _hacia_afuera(v, np.array(faces, dtype=np.int64))


def _girar(prof, segmentos, punto):
    """Los vertices y caras de un perfil (r, h) girado: punto(r, h, angulo) -> (x, y, z). Donde r es 0, el anillo
    se cierra en el eje (sin caras degeneradas)."""
    ang = np.linspace(0, 2 * np.pi, segmentos, endpoint=False)
    v = [punto(ri, hi, a) for ri, hi in prof for a in ang]
    S, faces = segmentos, []
    for i in range(len(prof) - 1):
        for j in range(S):
            a, b = i * S + j, i * S + (j + 1) % S
            c, d = (i + 1) * S + j, (i + 1) * S + (j + 1) % S
            if prof[i][0] > 1e-9:
                faces.append((a, b, d))
            if prof[i + 1][0] > 1e-9:
                faces.append((a, d, c))
    return _hacia_afuera(np.array(v, dtype=np.float64), np.array(faces, dtype=np.int64))


def disco(side, alto, tris_max, segmentos=12, front=None):
    """Revolucion alrededor del eje X (escudos redondos): a cada distancia del centro, lo que el lado sobresale
    adelante (el umbo) y atras, girado. El alto es el diametro; con front, el ancho (Z) se estira al del frente
    (escudos ovalados)."""
    sm, _ = side
    H, W = sm.shape
    s = alto / H
    rows, left, right = extremos(sm)
    y = (H - (rows + 0.5)) * s
    yc = ((H - rows[-1] - 1) + (H - rows[0])) * s / 2.0
    R = (rows[-1] + 1 - rows[0]) * s / 2.0
    xf, xb = (W / 2.0 - left) * s, (W / 2.0 - right) * s  # adelante (+X) y atras, a cada altura
    k = np.minimum(np.round(np.abs(y - yc) / s).astype(int), int(round(R / s)))
    radios = np.unique(k)
    frente = np.array([xf[k == i].max() for i in radios])  # las dos mitades juntas: lo de mas afuera
    atras = np.array([xb[k == i].min() for i in radios])
    r = np.minimum(radios * s, R)
    chain = np.concatenate([[[0.0, frente[0]]], np.stack([r, frente], axis=1), [[R, frente[-1]], [R, atras[-1]]],
                            np.stack([r, atras], axis=1)[::-1], [[0.0, atras[0]]]])
    prof = simplificar(chain, max(4, tris_max // (2 * segmentos) + 2))
    kz = 1.0
    if front is not None:
        fm = front[0]
        cols = np.flatnonzero(fm.any(axis=0))
        kz = (cols[-1] + 1 - cols[0]) * alto / fm.shape[0] / max(2 * R, 1e-9)
    return _girar(prof, segmentos, lambda ri, hi, a: (hi, yc + ri * np.cos(a), kz * ri * np.sin(a)))


def revolucion(front, alto, tris_max, segmentos=8):
    fm, _ = front
    H, W = fm.shape
    s = alto / H
    rows, left, right = extremos(fm)
    r = (right - left) / 2.0 * s
    y = (H - (rows + 0.5)) * s
    prof = np.stack([r, y], axis=1)[::-1]  # de abajo arriba
    prof = np.concatenate([[[0.0, (H - rows[-1] - 1) * s]], prof, [[0.0, (H - rows[0]) * s]]])
    k = max(3, tris_max // (2 * segmentos) + 1)
    prof = simplificar(prof, k)
    return _girar(prof, segmentos, lambda ri, yi, a: (ri * np.cos(a), yi, ri * np.sin(a)))


def _tramos(mask, s, ys):
    """Por cada altura ys (metros desde abajo, con s metros por pixel): la primera y la ultima columna de la fila de
    la silueta a esa altura (la fila con algo mas cercana)."""
    rows, left, right = extremos(mask)
    r = np.clip(mask.shape[0] - ys / s, rows[0], rows[-1] + 0.999)
    i = np.clip(np.searchsorted(rows, np.floor(r)), 0, len(rows) - 1)
    j = np.clip(i - 1, 0, len(rows) - 1)  # la fila con algo mas cercana (por arriba o por abajo)
    i = np.where(np.abs(rows[j] - np.floor(r)) < np.abs(rows[i] - np.floor(r)), j, i)
    return left[i], right[i]


def secciones(front, side, alto, tris_max, segmentos=8):
    """Una elipse por altura: de lado a lado (Z), lo que ocupa el frente; de adelante atras (X), lo que ocupa el
    lado. Las alturas, las que mejor siguen el perfil (Douglas-Peucker en cinco dimensiones), tantas como entren en
    tris_max (2 x segmentos x alturas, con las dos tapas), entre la fila mas baja y la mas alta que tienen algo en
    las dos vistas. Para lo que es una sola pieza a cada altura: cascos."""
    fm, sm = front[0], side[0]
    sf, ss = alto / fm.shape[0], alto / sm.shape[0]
    rf, rs = np.flatnonzero(fm.any(axis=1)), np.flatnonzero(sm.any(axis=1))
    y0 = max((fm.shape[0] - rf[-1] - 1) * sf, (sm.shape[0] - rs[-1] - 1) * ss)
    y1 = min((fm.shape[0] - rf[0]) * sf, (sm.shape[0] - rs[0]) * ss)
    n = max(len(rf), len(rs))
    ys = np.concatenate([[y0], y0 + (np.arange(n) + 0.5) * (y1 - y0) / n, [y1]])
    lf, rf = _tramos(fm, sf, ys)
    ls, rs = _tramos(sm, ss, ys)
    # Las columnas a metros, como en uvs(): z = (W/2 - columna) s en el frente; x = (W/2 - columna) s en el lado.
    z0, z1 = (fm.shape[1] / 2.0 - rf) * sf, (fm.shape[1] / 2.0 - lf) * sf
    x0, x1 = (sm.shape[1] / 2.0 - rs) * ss, (sm.shape[1] / 2.0 - ls) * ss
    k = max(2, tris_max // (2 * segmentos))
    perfil = simplificar(np.stack([ys, x0, x1, z0, z1], axis=1), k)
    ang = (np.arange(segmentos) + 0.5) * 2 * np.pi / segmentos  # medio paso: una cara mira al frente y otra al lado
    v = []
    for y, a0, a1, b0, b1 in perfil:
        cx, cz, rx, rz = (a0 + a1) / 2, (b0 + b1) / 2, (a1 - a0) / 2, (b1 - b0) / 2
        v += [(cx + rx * np.cos(t), y, cz + rz * np.sin(t)) for t in ang]
    S, K = segmentos, len(perfil)
    abajo, arriba = len(v), len(v) + 1
    v.append((np.mean([p[0] for p in v[:S]]), perfil[0][0], np.mean([p[2] for p in v[:S]])))
    v.append((np.mean([p[0] for p in v[-S - 1:-1]]), perfil[-1][0], np.mean([p[2] for p in v[-S - 1:-1]])))
    faces = []
    for i in range(K - 1):
        for j in range(S):
            a, b = i * S + j, i * S + (j + 1) % S
            faces += [(a, b, b + S), (a, b + S, a + S)]
    for j in range(S):
        faces.append((abajo, (j + 1) % S, j))
        faces.append((arriba, (K - 1) * S + j, (K - 1) * S + (j + 1) % S))
    return _hacia_afuera(np.array(v, dtype=np.float64), np.array(faces, dtype=np.int64))


CRESTA_TRIS = 36  # la aleta de la cresta: diez puntos de contorno


def _mayor(mask):
    """La mayor pieza conexa de la mascara."""
    from scipy import ndimage

    lab, n = ndimage.label(mask)
    if n <= 1:
        return mask
    sizes = ndimage.sum(mask, lab, range(1, n + 1))
    return lab == 1 + int(np.argmax(sizes))


def _tono(rgb):
    """El tono (0..1, el rojo en 0) y la saturacion de cada pixel."""
    c = rgb.astype(np.float64) / 255.0
    mx, mn = c.max(axis=-1), c.min(axis=-1)
    d = np.maximum(mx - mn, 1e-9)
    r, g, b = c[..., 0], c[..., 1], c[..., 2]
    h = np.where(mx == r, ((g - b) / d) % 6, np.where(mx == g, (b - r) / d + 2, (r - g) / d + 4)) / 6.0
    return h, np.where(mx > 0, (mx - mn) / np.maximum(mx, 1e-9), 0.0)


def _arriba(mask, frac=0.08):
    """Lo de la mascara en el frac de arriba de su alto."""
    rows = np.flatnonzero(mask.any(axis=1))
    out = np.zeros_like(mask)
    out[rows[0]:rows[0] + max(2, int(frac * mask.shape[0]))] = True
    return out & mask


def cresta(vistas, tono=0.04, saturacion=0.35):
    """La cresta (o el penacho) de un casco: lo del tono de lo mas alto del frente (el 8 % de arriba, que es solo
    cresta), saturado y pegado a lo de arriba de cada vista (el contorno oscuro del casco tiene un tono parecido,
    pero sin saturacion). Devuelve las vistas sin la cresta (para secciones), la mascara de la cresta en el lado y
    su ancho de lado a lado en el frente y la columna de su centro (en pixeles), o None si no hay una."""
    from scipy import ndimage

    (fm, fc), (sm, sc), (bm, bc) = vistas
    h, sat = _tono(fc)
    top = _arriba(fm)
    if np.median(sat[top]) < saturacion:
        return None
    h0 = float(np.median(h[top]))

    def del_tono(m, c):
        h, sat = _tono(c)
        match = m & (np.abs((h - h0 + 0.5) % 1.0 - 0.5) < tono) & (sat > saturacion)
        match = ndimage.binary_closing(match, iterations=2) & m
        lab, _ = ndimage.label(match)
        keep = np.unique(lab[_arriba(m) & match])
        return ndimage.binary_fill_holes(np.isin(lab, keep[keep > 0])) & m

    cf, cs, cb = del_tono(fm, fc), del_tono(sm, sc), del_tono(bm, bc)
    if cs.sum() < 0.02 * sm.sum() or not cf.any():
        return None
    sin = [(_mayor(m & ~ndimage.binary_dilation(cm, iterations=1)), c) for (m, c), cm in zip(vistas[:2], (cf, cs))]
    sin.append((bm & ~cb, bc))
    ancho = float(np.median(cf.sum(axis=1)[cf.any(axis=1)]))
    centro = float(np.median(np.nonzero(cf)[1])) + 0.5
    return sin, cs, ancho, centro


def aleta(side_mask, alto, tris_max, ancho_m):
    """Una aleta plana: la silueta de la mascara del lado, extruida de lado a lado (Z) con ancho_m de grosor."""
    v, f = plana((side_mask, None), (side_mask, None), alto, tris_max, grosor=ancho_m)
    return _hacia_afuera(v[:, [2, 1, 0]], f)  # el ancho de plana (X) pasa a Z; el largo del lado, a X


def volumen(hoja, kw, alto, tris_max, vistas, res=96, miembros=False):
    """El volumen de voxeles.desde_hoja (secciones elipticas; con miembros, los brazos redondos), su superficie suave
    y reducida a tris_max, en metros y en el marco de las vistas (el de uvs): la escala de cada eje sale de las celdas
    del volumen y de los pixeles de las vistas, no de la malla, que pierde las puntas finas (cuernos, pezuñas) al
    alisar y al reducir; estirarla a su alto corria la textura (el lomo tomaba lo de arriba del lomo)."""
    import lowres

    filled, rgb = voxeles.desde_hoja(hoja, res=res, seccion="miembros" if miembros else "elipse", **kw)
    v, f, _ = voxeles.superficie(filled, rgb)
    import trimesh

    v2, f2 = lowres._decimate(trimesh.Trimesh(v, f, process=False), tris_max)
    v2 = np.asarray(v2, dtype=np.float64)
    nx, ny, nz = filled.shape  # de lado a lado, de adelante atras, arriba
    (hf, wf), (hs, ds) = vistas[0][0].shape, vistas[1][0].shape
    largo, ancho = ds * alto / hs, wf * alto / hf  # el lado y el frente, en metros
    v2 = np.stack([(v2[:, 0] + 0.5 - ny / 2.0) * largo / ny, (v2[:, 1] + 0.5) * alto / nz,
                   (v2[:, 2] + 0.5 - nx / 2.0) * ancho / nx], axis=1)
    return _hacia_afuera(v2, np.asarray(f2, dtype=np.int64))


def _hacia_afuera(v, f):
    """Que las caras miren hacia afuera (el volumen con signo, positivo)."""
    tri = v[f]
    vol = np.einsum("ij,ij->i", tri[:, 0], np.cross(tri[:, 1], tri[:, 2])).sum()
    if vol < 0:
        f = f[:, [0, 2, 1]]
    return v, f


# ---------------------------------------------------------------- textura proyectada
HUECO = np.array([34.0, 28.0, 22.0])  # lo que se ve por un hueco (la abertura de un casco): sombra


def _rellenar(rgb, mask, umbral=40, borde=2, huecos=False):
    """Para la textura: dentro de la silueta, lo que tiene el color del fondo (se ve el fondo por un hueco: la
    abertura de un casco, entre los pinchos de una maza) pasa a sombra; el borde de la silueta (borde pixeles,
    mezclados con el fondo por el antialias) y lo de afuera toman el color del pixel de adentro mas cercano: sin
    rayas claras donde una cara cae un poco fuera de la silueta. Las partes mas finas que el doble del borde quedan
    como estan. Con huecos (secciones, donde un anillo cubre toda la fila), lo de afuera entre dos partes de una
    misma fila (entre las carrilleras de un casco) tambien pasa a sombra: es una abertura. En volumen no: la malla
    no cubre esos huecos (las patas van separadas), y las caras de cerca, entre las puntas de una cornamenta o los
    cuernos, saldrian negras."""
    from scipy import ndimage

    out = rgb.astype(np.float64).copy()
    bg = np.median(out[~mask], axis=0) if (~mask).any() else np.array([255.0, 255.0, 255.0])
    hole = mask & (np.abs(out - bg).sum(axis=2) <= umbral)
    out[hole] = HUECO
    core = ndimage.binary_erosion(mask, iterations=borde, border_value=0)
    core |= mask & ~ndimage.binary_dilation(core, iterations=borde + 1)  # lo fino, entero
    if not core.any():
        core = mask
    _, (iy, ix) = ndimage.distance_transform_edt(~core, return_indices=True)
    out = out[iy, ix]
    if huecos:
        for r in np.flatnonzero(mask.any(axis=1)):
            c = np.flatnonzero(mask[r])
            gap = ~mask[r, c[0]:c[-1] + 1]
            out[r, c[0]:c[-1] + 1][gap] = HUECO
    return out


def atlas(vistas, size=TEX_MAX, huecos=False, umbral=40):
    """Las vistas (frente, lado, espalda), una al lado de la otra, en una textura cuadrada potencia de dos.
    Devuelve (imagen, [(x0, y0, k) por vista]): el pixel (c, r) de la vista va a (x0 + c k, y0 + r k)."""
    crops = [_rellenar(rgb, m, umbral=umbral, huecos=huecos) for m, rgb in vistas]  # el umbral de la hoja: con
    # uno mayor, el pelaje casi blanco (que la mascara si toma) pasaba a sombra
    gap = 2
    wsum = sum(c.shape[1] for c in crops) + gap * (len(crops) + 1)
    hmax = max(c.shape[0] for c in crops) + 2 * gap
    k = min(size / wsum, size / hmax)
    side = 1 << int(np.ceil(np.log2(max(wsum * k, hmax * k, 8))))
    side = min(side, size)
    img = Image.new("RGB", (side, side), (0, 0, 0))
    places, x = [], gap * k
    for c in crops:
        w, h = max(1, int(round(c.shape[1] * k))), max(1, int(round(c.shape[0] * k)))
        im = Image.fromarray(np.clip(c, 0, 255).astype(np.uint8)).resize((w, h), Image.LANCZOS)
        img.paste(im, (int(round(x)), int(round(gap * k))))
        places.append((int(round(x)), int(round(gap * k)), w / c.shape[1], h / c.shape[0]))
        x += w + gap * k
    return img, places


def uvs(v, f, vistas, places, alto, side, encima=None):
    """Por cara, la vista que la mira: +X el frente, -X la espalda, lo demas el lado (los dos lados, del mismo).
    encima (volumen: animales y personas, que se ven desde arriba): que toma lo de arriba, que ninguna vista mira.
      "espalda": la espalda, donde cae dentro de su silueta (si no, como "lado").
      "lado": el lado, un poco por debajo del borde (el color del lomo, sin el borde).
    Lo de abajo, el frente (el pecho y la panza). Devuelve las coordenadas de textura de cada vertice de cada cara
    (caras x 3 x 2)."""
    from scipy import ndimage

    tri = v[f]
    nrm = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-12)
    out = np.zeros((len(f), 3, 2))
    (fm, _), (sm, _), (bm, _) = vistas
    sf, ss, sb = alto / fm.shape[0], alto / sm.shape[0], alto / bm.shape[0]
    nucleo = ndimage.binary_erosion(bm, iterations=3) if encima == "espalda" else None
    bajar = 0.03 * sm.shape[0]  # "lado": tres centesimos del alto por debajo del borde de arriba
    for i in range(len(f)):
        nx, ny = nrm[i, 0], nrm[i, 1]
        p = tri[i]
        arriba = encima and ny > 0.5 and abs(nx) <= 0.5
        view = None
        if nx > 0.5 or (encima and ny < -0.5):
            view, col, row = 0, fm.shape[1] / 2.0 - p[:, 2] / sf, fm.shape[0] - p[:, 1] / sf
        elif nx < -0.5 or (arriba and encima == "espalda"):
            view, col, row = 2, bm.shape[1] / 2.0 + p[:, 2] / sb, bm.shape[0] - p[:, 1] / sb
            if arriba:
                c = np.clip(col.astype(int), 0, bm.shape[1] - 1)
                r = np.clip(row.astype(int), 0, bm.shape[0] - 1)
                if not nucleo[r, c].all():
                    view = None
        if view is None:  # la hoja muestra el lado desde -Z (el frente a la izquierda); el de +Z es el mismo punto
            view, row = 1, sm.shape[0] - p[:, 1] / ss + (bajar if arriba else 0.0)
            col = sm.shape[1] / 2.0 - p[:, 0] / ss
        h, w = vistas[view][0].shape
        col, row = np.clip(col, 0.5, w - 0.5), np.clip(row, 0.5, h - 0.5)
        x0, y0, kx, ky = places[view]
        out[i, :, 0] = (x0 + col * kx) / side
        out[i, :, 1] = (y0 + row * ky) / side
    return out


# ---------------------------------------------------------------- origen y GLB
def cruz_px(sm, columna=None):
    """La cruz de un cuadrupedo en la vista de lado (el frente a la izquierda): (fila, columna). Las patas, en la
    franja de abajo (del 75 al 92 % del alto, sobre las pezuñas); las de adelante, las tiras de antes del hueco mas
    grande; la cruz, el borde de arriba del cuerpo sobre la parte de atras de las patas de adelante (el cuello sale
    hacia adelante de ahi): el primer tramo largo de esa columna (un decimo del alto), no una punta de cornamenta
    que pasa por encima.
    columna (fraccion del ancho, desde el frente) la fija a mano (la joroba de un dromedario empieza justo ahi)."""
    rows = np.flatnonzero(sm.any(axis=1))
    top, h = rows[0], rows[-1] + 1 - rows[0]
    banda = top + int(0.75 * h)
    if columna is not None:
        c = int(np.clip(columna * sm.shape[1], 0, sm.shape[1] - 1))
    else:
        franja = sm[banda: top + int(0.92 * h)].any(axis=0)
        tiras = [(a, b) for a, b in voxeles.corridas(franja) if b - a >= 2]
        if not tiras:
            raise SystemExit("--cruz: no encuentro las patas en la vista de lado")
        huecos = [tiras[i + 1][0] - tiras[i][1] for i in range(len(tiras) - 1)]
        corte = int(np.argmax(huecos)) + 1 if huecos else len(tiras)
        c = tiras[corte - 1][1] - 1
    tramos = voxeles.corridas(sm[:, c])
    cuerpo = [t for t in tramos if t[1] - t[0] >= 0.1 * h] or tramos  # sin las puntas finas que pasan por encima
    return int(cuerpo[0][0]), c


def agarre_auto(fm, invertido):
    """La altura del agarre (desde abajo, en fraccion del alto). Con guarda (espadas, dagas), el tramo angosto entre
    el pomo y la guarda; sin guarda (hachas, mazas), el tramo angosto mas largo entre el 6 y el 45 % de abajo (sin la
    punta del pomo, que tambien es angosta). La guarda: la primera fila, de abajo arriba y hasta el 60 %, mucho mas
    ancha que lo mas angosto que tiene debajo (el agarre; con al menos un 5 % del alto debajo, para que el pomo no
    cuente). No se compara con la hoja: la de una daga es casi tan ancha como su guarda."""
    rows, left, right = extremos(fm)
    width = right - left
    H = fm.shape[0]
    y = (H - (rows + 0.5)) / H  # desde abajo
    if invertido:
        y = 1.0 - y
    o = np.argsort(y)
    y, width = y[o], width[o]
    if not ((y > 0.06) & (y < 0.6)).any():
        return 0.15
    top = 0.45
    for i in np.flatnonzero((y > 0.06) & (y < 0.6)):
        below = (y > 0.06) & (y < y[i])
        if below.sum() >= 0.05 * H and width[i] > 1.8 * width[below].min():
            top = y[i] - 0.5 / H
            break
    low = (y > 0.06) & (y < top)
    if not low.any():
        return 0.15
    narrow = low & (width <= width[low].min() * 1.35 + 1)
    ys = np.sort(y[narrow])
    runs = np.split(ys, np.flatnonzero(np.diff(ys) > 2.5 / H) + 1)
    best = max(runs, key=lambda r: r[-1] - r[0])
    return float((best[0] + best[-1]) / 2)


def glb_textura(v, f, uv, tex, nombre):
    """GLB sin indices: tres vertices por cara con su normal (plana), su UV y la textura PNG (filtro de punto)."""
    tri = v[f].reshape(-1, 3).astype(np.float64)
    n = np.cross(v[f][:, 1] - v[f][:, 0], v[f][:, 2] - v[f][:, 0])
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    nor = np.repeat(n, 3, axis=0)
    st = uv.reshape(-1, 2)
    png = io.BytesIO()
    tex.save(png, "PNG", optimize=True)
    png = png.getvalue()
    bp, bn, bt = tri.astype("<f4").tobytes(), nor.astype("<f4").tobytes(), st.astype("<f4").tobytes()
    blob = bp + bn + bt
    o_img = len(blob)
    blob += png + b"\x00" * (-len(png) % 4)
    count = len(tri)
    doc = {
        "asset": {"version": "2.0", "generator": "estepa tools/modelos/piezas.py"},
        "scene": 0, "scenes": [{"nodes": [0]}], "nodes": [{"name": nombre, "mesh": 0}],
        "meshes": [{"name": nombre, "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "TEXCOORD_0": 2}, "material": 0}]}],
        "materials": [{"name": "pieza", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 0,
                                                                  "roughnessFactor": 1}}],
        "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 33071}],
        "textures": [{"sampler": 0, "source": 0}],
        "images": [{"bufferView": 3, "mimeType": "image/png"}],
        "buffers": [{"byteLength": len(blob)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(bp), "target": 34962},
            {"buffer": 0, "byteOffset": len(bp), "byteLength": len(bn), "target": 34962},
            {"buffer": 0, "byteOffset": len(bp) + len(bn), "byteLength": len(bt), "target": 34962},
            {"buffer": 0, "byteOffset": o_img, "byteLength": len(png)},
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": count, "type": "VEC3",
             "min": tri.min(axis=0).tolist(), "max": tri.max(axis=0).tolist()},
            {"bufferView": 1, "componentType": 5126, "count": count, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5126, "count": count, "type": "VEC2"},
        ],
    }
    js = json.dumps(doc, separators=(",", ":")).encode("utf-8")
    js += b" " * (-len(js) % 4)
    out = io.BytesIO()
    out.write(struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(js) + 8 + len(blob)))
    out.write(struct.pack("<I4s", len(js), b"JSON") + js)
    out.write(struct.pack("<I4s", len(blob), b"BIN\x00") + blob)
    return out.getvalue()


def construir(mid, hoja, forma, filas=1, fila=1, vertical=False, umbral=40, voltear_lado=False, invertir=False,
              agarre="base", segmentos=8, textura=TEX_MAX, res=96, alto=None, tris_max=None, grosor=None,
              con_cresta=False, miembros=False, ancla=None, girar=0, cruz=None, cruz_columna=None):
    """Devuelve (bytes del GLB, informe)."""
    items, _ = inventario.load()
    it = modelos.find_item(items, mid)
    if it is None:
        raise SystemExit(f"{mid}: no esta en assets/inventario.tsv")
    ancho, alto_inv, largo = modelos.dims(it)
    alto = alto or alto_inv
    # La medida que manda (lo demas sale de la imagen): el largo en los animales (es su tamaño en la simulacion,
    # src/sim/animals.c), el alto en lo demas.
    ancla = ancla or ("largo" if it["categoria"] == "animal" else "alto")
    tris_max = tris_max or int(it["tris_max"]) or 300
    vistas = voxeles.hoja(hoja, umbral, filas, fila, vertical)
    if girar:  # la hoja lo dibuja acostado: el frente y la espalda (espejada) se giran en su plano
        k = {90: 1, -90: -1, 180: 2}[girar]
        vistas[0] = tuple(np.rot90(a, k) for a in vistas[0])
        vistas[2] = tuple(np.rot90(a, -k) for a in vistas[2])
    nb = None  # los vertices del cuerpo (el origen de la base): todos, salvo la cresta
    if voltear_lado:
        sm, sc = vistas[1]
        vistas[1] = (sm[:, ::-1], sc[:, ::-1])
    if forma == "plana":
        v, f = plana(vistas[0], vistas[1], alto, tris_max, grosor)
    elif forma == "revolucion":
        v, f = revolucion(vistas[0], alto, tris_max, segmentos)
    elif forma == "disco":
        v, f = disco(vistas[1], alto, tris_max, segmentos, front=vistas[0])
    elif forma == "secciones":
        partes = cresta(vistas) if con_cresta else None
        if con_cresta and partes is None:
            raise SystemExit(f"{mid}: --cresta, pero no hay una cresta de otro color arriba del frente")
        if partes:
            sin, cs, ancho_px, centro = partes
            sf = alto / vistas[0][0].shape[0]
            vc, fc = aleta(cs, alto, CRESTA_TRIS, ancho_px * sf)
            vc[:, 2] += (vistas[0][0].shape[1] / 2.0 - centro) * sf
            vb, fb = secciones(sin[0], sin[1], alto, tris_max - len(fc), segmentos)
            v, f, nb = np.concatenate([vb, vc]), np.concatenate([fb, fc + len(vb)]), len(vb)
        else:
            v, f = secciones(vistas[0], vistas[1], alto, tris_max, segmentos)
    elif forma == "volumen":
        v, f = volumen(hoja, dict(umbral=umbral, filas=filas, fila=fila, voltear_lado=voltear_lado, vertical=vertical),
                       alto, tris_max, vistas, res, miembros)
    else:
        raise SystemExit("--forma: plana, revolucion, disco, secciones o volumen")
    tex, places = atlas(vistas, textura, huecos=forma == "secciones", umbral=umbral)
    uv = uvs(v, f, vistas, places, alto, tex.size[0], encima="lado" if forma == "volumen" else None)
    # El origen (y, si hace falta, el giro de 180 grados alrededor de X: la hoja hacia arriba).
    lo, hi = v.min(axis=0), v.max(axis=0)
    techo = hi[1]  # el y de antes de girar es techo - y
    if invertir:
        v = np.stack([v[:, 0], hi[1] - v[:, 1], -v[:, 2]], axis=1)  # una rotacion: las caras siguen hacia afuera
        lo, hi = v.min(axis=0), v.max(axis=0)
    if agarre == "base":  # el centro de la base del cuerpo (sin la cresta, que se va hacia atras)
        bl, bh = v[:nb].min(axis=0), v[:nb].max(axis=0)
        o = np.array([(bl[0] + bh[0]) / 2, lo[1], (bl[2] + bh[2]) / 2])
    elif agarre == "escudo":
        o = np.array([lo[0], (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2])
    else:
        frac = agarre_auto(vistas[0][0], invertir) if agarre == "auto" else float(agarre)
        yg = lo[1] + frac * (hi[1] - lo[1])
        zc = 0.0  # revolucion: el eje
        if forma == "plana":  # el centro de lado a lado: el de la silueta del frente a la altura del agarre
            fm = vistas[0][0]
            sf = alto / fm.shape[0]
            left, right = _tramos(fm, sf, np.array([techo - yg if invertir else yg]))
            zc = (fm.shape[1] / 2.0 - (left[0] + right[0]) / 2.0) * sf * (-1.0 if invertir else 1.0)
        o = np.array([(lo[0] + hi[0]) / 2, yg, zc])
    v = v - o
    cruz_m = None
    if cruz:  # la altura a la cruz manda (animales: la de los mas grandes de su especie)
        r, _ = cruz_px(vistas[1][0], cruz_columna)
        cruz_m = (vistas[1][0].shape[0] - r) * alto / vistas[1][0].shape[0]
        v = v * (cruz / cruz_m)
    elif ancla == "largo":
        v = v * (largo / max(float(np.ptp(v[:, 0])), 1e-9))
    data = glb_textura(v, f, uv, tex, mid)
    lo, hi = v.min(axis=0), v.max(axis=0)
    informe = {"tris": int(len(f)), "tris_max": tris_max, "caja_m": [round(float(x), 4) for x in (hi - lo)],
               "cruz_m": cruz,
               "inventario_m": [largo, alto_inv, ancho], "textura": list(tex.size), "origen": o.round(4).tolist(),
               "bytes": len(data)}
    return data, informe


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("id")
    ap.add_argument("--hoja", required=True)
    ap.add_argument("--forma", required=True, choices=("plana", "revolucion", "disco", "secciones", "volumen"))
    ap.add_argument("--filas", type=int, default=1)
    ap.add_argument("--fila", type=int, default=1)
    ap.add_argument("--vertical", action="store_true")
    ap.add_argument("--umbral", type=int, default=40)
    ap.add_argument("--voltear-lado", action="store_true")
    ap.add_argument("--invertir", action="store_true")
    ap.add_argument("--agarre", default="base")
    ap.add_argument("--segmentos", type=int, default=8)
    ap.add_argument("--textura", type=int, default=TEX_MAX)
    ap.add_argument("--res", type=int, default=96)
    ap.add_argument("--grosor", type=float, default=None, help="plana: el grosor en m (si no, el del lado)")
    ap.add_argument("--cresta", action="store_true", help="secciones: la cresta de arriba, de otro color, como aleta")
    ap.add_argument("--miembros", action="store_true", help="volumen: los brazos redondos, no tan hondos como el torso")
    ap.add_argument("--alto", type=float, help="el alto en metros, en vez del del inventario (personas sin id propio)")
    ap.add_argument("--cruz", type=float, help="animales: la altura a la cruz en metros (la escala sale de ahi, y no del "
                                               "inventario)")
    ap.add_argument("--cruz-columna", type=float, help="donde medir la cruz, en fraccion del ancho de la vista de lado "
                                                       "desde el frente (si no, sobre las patas de adelante)")
    ap.add_argument("--girar", type=int, default=0, choices=(0, 90, -90, 180),
                    help="la hoja lo dibuja acostado: gira el frente (y la espalda, al reves) en su plano, en grados "
                         "antihorarios, hasta que quede parado")
    ap.add_argument("--ancla", choices=("alto", "largo"), help="la medida del inventario que manda (si no: el largo en "
                                                                "los animales, el alto en lo demas)")
    ap.add_argument("--salida")
    ap.add_argument("--sin-inventario", action="store_true")
    a = ap.parse_args(argv)
    data, informe = construir(a.id, a.hoja, a.forma, a.filas, a.fila, a.vertical, a.umbral, a.voltear_lado, a.invertir,
                              a.agarre, a.segmentos, a.textura, a.res, alto=a.alto, grosor=a.grosor, con_cresta=a.cresta,
                              miembros=a.miembros, ancla=a.ancla, girar=a.girar, cruz=a.cruz,
                              cruz_columna=a.cruz_columna)
    items, _ = inventario.load()
    it = modelos.find_item(items, a.id)
    rig = bool(modelos.needs_clips(items, it))
    dest = a.salida or (os.path.join(modelos.PARA_RIGGEAR, a.id + ".glb") if rig else os.path.join(inventario.ROOT, inventario.path_of(it)))
    os.makedirs(os.path.dirname(os.path.abspath(dest)), exist_ok=True)
    with open(dest, "wb") as fh:
        fh.write(data)
    log(json.dumps(informe, ensure_ascii=False))
    log(f"[piezas] {os.path.relpath(dest, inventario.ROOT)}")
    if not a.sin_inventario and not a.salida:
        if not rig:
            modelos.set_state(a.id, "importado" if it["estado"] == "pendiente" else it["estado"])
        man = modelos.load_manifest()
        with open(a.hoja, "rb") as fh:
            sha = hashlib.sha256(fh.read()).hexdigest()
        man["models"][a.id] = {
            "file": os.path.relpath(dest, inventario.ROOT).replace(os.sep, "/"), "category": it["categoria"], "has_rig": False,
            "source": {"tool": "otro", "url": "", "file": os.path.basename(a.hoja), "sha256": sha},
            "params": {"piezas": a.forma, "filas": a.filas, "fila": a.fila, "invertir": a.invertir, "agarre": a.agarre,
                       "segmentos": a.segmentos, "grosor": a.grosor, "cresta": a.cresta, "miembros": a.miembros,
                       "res": a.res, "umbral": a.umbral, "ancla": a.ancla, "girar": a.girar, "cruz": a.cruz,
                       "cruz_columna": a.cruz_columna,
                       "textura": informe["textura"]},
            "tris": informe["tris"], "updated_at": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        }
        modelos.save_manifest(man)
        if not rig:
            import subprocess
            subprocess.call([sys.executable, os.path.join(HERE, "..", "assets", "inventario.py"), "doc"], stdout=subprocess.DEVNULL)
    return 0


if __name__ == "__main__":
    sys.exit(main())
