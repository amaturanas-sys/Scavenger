#!/usr/bin/env python3
"""Modelos de voxeles para el juego (.vox de MagicaVoxel), desde tres vistas, un splat de Idol o un GLB.

  python3 tools/voxeles/voxeles.py vistas FRENTE.png LADO.png ARRIBA.png -o salida.vox [--res 40] [--colores 24]
  python3 tools/voxeles/voxeles.py hoja HOJA.png -o salida.vox [--filas 4 --fila 2] [--res 48]
  python3 tools/voxeles/voxeles.py splat objeto.ply -o salida.vox [--res 48] [--arriba y]
  python3 tools/voxeles/voxeles.py malla objeto.glb -o salida.vox [--res 48] [--frente +x]
  python3 tools/voxeles/voxeles.py ver modelo.vox -o vista.png

Las tres vistas son ortograficas y del mismo objeto, con fondo transparente o liso (se toma el color de los
bordes). Frente: el objeto de frente. Lado: su lado derecho, con el frente a la izquierda de la imagen
(--voltear-lado si esta a la derecha). Arriba: visto desde arriba, con el frente abajo (--voltear-arriba si
esta arriba). El volumen es lo que cae dentro de las tres siluetas; el color de cada cara sale de la vista
que la mira. Siempre con --vista-previa (o el comando ver) para mirarlo antes de traerlo al juego.

Una hoja: una imagen con las tres vistas en fila, frente | lado | espalda (sin la de arriba), como las que
genera Canva; con --filas, varios objetos uno debajo del otro (--fila elige cual). La forma sale del frente y
del lado; el color de atras, de la espalda.

Con -o algo.glb, en lugar de voxeles sale la superficie suave del mismo volumen (marching cubes sobre el
volumen alisado, con el color de cada vertice): la fuente densa para tools/modelos/modelos.py traer --suave,
que la lleva al estilo de PS2 (personas, animales y armas, que en voxeles no quedan bien). Para eso conviene
--res 96 o 128.

La salida .vox va a assets/models/<categoria>/.../<nombre>.vox, con la etiqueta formato:voxel en
assets/inventario.tsv: el juego la carga como los GLB, a la altura de sus medidas (src/world/voxfile.c).
Dependencias: las de tools/modelos/requirements.txt (numpy, scipy, Pillow; trimesh para malla).
"""
import argparse
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import vox  # noqa: E402


def log(msg):
    print(msg, flush=True)


# ---------------------------------------------------------------------------------------- paleta
def cuantizar(colors, n):
    """colors: (N, 3) uint8 -> (indices 1..k por color, paleta (k, 3)). Corte por la mediana: estable."""
    colors = np.asarray(colors, dtype=np.uint8).reshape(-1, 3)
    if len(colors) == 0:
        return np.zeros(0, np.uint8), np.zeros((1, 3), np.uint8)
    n = max(1, min(255, int(n)))
    img = Image.fromarray(colors.reshape(1, -1, 3), "RGB")
    q = img.quantize(colors=n, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
    idx = np.asarray(q, dtype=np.int32).reshape(-1)
    used = int(idx.max()) + 1
    pal = np.asarray(q.getpalette()[: used * 3], dtype=np.uint8).reshape(-1, 3)
    return (idx + 1).astype(np.uint8), pal


def expuestas(filled):
    """Celdas llenas con alguna cara al aire, y la mascara de cada direccion (-x, +x, -y, +y, -z, +z)."""
    p = np.pad(filled, 1)
    dirs = []
    for axis in range(3):
        for step in (-1, 1):
            nb = np.roll(p, -step, axis=axis)[1:-1, 1:-1, 1:-1]
            dirs.append(filled & ~nb)
    return np.any(dirs, axis=0), dirs


def guardar(path, filled, rgb, ncolors):
    """filled (sx, sy, sz) bool; rgb (sx, sy, sz, 3) float 0..255. Cuantiza solo lo que se ve."""
    surf, _ = expuestas(filled)
    grid = np.zeros(filled.shape, np.uint8)
    cols = np.clip(rgb[surf], 0, 255).astype(np.uint8)
    idx, pal = cuantizar(cols, ncolors)
    grid[surf] = idx
    inner = filled & ~surf
    grid[inner] = 1  # lo de adentro no se ve
    n = vox.escribir(path, grid, pal)
    log(f"[voxeles] {path}: {filled.shape[0]}x{filled.shape[1]}x{filled.shape[2]}, {n} celdas, {len(pal)} colores")
    return grid, pal


# ---------------------------------------------------------------------------------------- vistas
def mascara(a, umbral):
    """Lo que no es fondo en una imagen RGBA (int16): la transparencia o lo que se aparta del color de los bordes."""
    if a[..., 3].min() < 250:
        return a[..., 3] > 127
    # El fondo: un anillo un poco hacia adentro del borde (algunas hojas traen un marco de otro color).
    d = max(1, int(round(0.03 * min(a.shape[:2]))))
    border = np.concatenate([a[d, :, :3], a[-1 - d, :, :3], a[:, d, :3], a[:, -1 - d, :3]])
    bg = np.median(border, axis=0)
    return np.abs(a[..., :3] - bg).sum(axis=2) > umbral


def objeto(m, rgb, donde="la imagen"):
    """(mascara, rgb) del objeto: la mancha mas grande, sin agujeros y recortada a su caja."""
    from scipy import ndimage

    m = ndimage.binary_opening(m, iterations=1) if m.sum() > 400 else m
    lab, n = ndimage.label(m)
    if n == 0:
        raise ValueError(f"{donde}: no se encontro el objeto (prueba otro --umbral)")
    sizes = ndimage.sum(m, lab, range(1, n + 1))
    m = lab == (int(np.argmax(sizes)) + 1)
    # Los agujeros chicos (ruido) se tapan; los grandes (entre las ruedas, un arco, el armazon) quedan.
    holes, nh = ndimage.label(ndimage.binary_fill_holes(m) & ~m)
    if nh:
        area = ndimage.sum(np.ones_like(m), holes, range(1, nh + 1))
        small = np.flatnonzero(area < max(4.0, 0.002 * m.sum())) + 1
        m = m | np.isin(holes, small)
    ys, xs = np.nonzero(m)
    y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
    return m[y0:y1, x0:x1], rgb[y0:y1, x0:x1, :3].astype(np.float32)


def silueta(path, umbral):
    """(mascara, rgb) recortadas al objeto. Fondo: la transparencia o el color de los bordes."""
    a = np.asarray(Image.open(path).convert("RGBA")).astype(np.int16)
    return objeto(mascara(a, umbral), a, path)


def tramos(perfil, n, donde):
    """Los n tramos de un perfil (filas o columnas con algo): si hay mas, se juntan los separados por menos."""
    idx = np.flatnonzero(perfil)
    if len(idx) == 0:
        raise ValueError(f"{donde}: vacia")
    cuts = np.flatnonzero(np.diff(idx) > 1)
    runs = [[idx[0], idx[-1] + 1]] if len(cuts) == 0 else []
    if len(cuts):
        starts = np.concatenate([[idx[0]], idx[cuts + 1]])
        ends = np.concatenate([idx[cuts] + 1, [idx[-1] + 1]])
        runs = [[int(a), int(b)] for a, b in zip(starts, ends)]
    while len(runs) > n:  # el hueco mas angosto no separa dos vistas: se juntan
        gaps = [runs[i + 1][0] - runs[i][1] for i in range(len(runs) - 1)]
        i = int(np.argmin(gaps))
        runs[i:i + 2] = [[runs[i][0], runs[i + 1][1]]]
    if len(runs) < n:
        raise ValueError(f"{donde}: se esperaban {n} partes separadas por fondo y hay {len(runs)}")
    return runs


def hoja(path, umbral=40, filas=1, fila=1, vertical=False):
    """Las tres vistas (frente, lado, espalda) de la fila elegida de una hoja: [(mascara, rgb)] * 3. vertical:
    las tres, una debajo de la otra (las aves con las alas abiertas)."""
    a = np.asarray(Image.open(path).convert("RGBA")).astype(np.int16)
    m = mascara(a, umbral)
    # Lo que va de punta a punta (las rayas que separan las vistas en algunas hojas) no es objeto.
    m[:, m.mean(axis=0) > 0.97] = False
    m[m.mean(axis=1) > 0.97, :] = False
    if vertical:
        return [objeto(m[r0:r1], a[r0:r1], path) for r0, r1 in tramos(m.any(axis=1), 3, path)]
    r0, r1 = tramos(m.any(axis=1), filas, path)[fila - 1]
    m, a = m[r0:r1], a[r0:r1]
    views = []
    for c0, c1 in tramos(m.any(axis=0), 3, path):
        views.append(objeto(m[:, c0:c1], a[:, c0:c1], path))
    return views


def remuestrear(mask, rgb, w, h):
    """La mascara y el color (premultiplicado: el fondo no tiñe los bordes) a w x h celdas."""
    mf = mask.astype(np.float32)
    m = np.asarray(Image.fromarray(mf, "F").resize((w, h), Image.BOX))
    ch = []
    for i in range(3):
        c = np.asarray(Image.fromarray(rgb[..., i] * mf, "F").resize((w, h), Image.BOX))
        ch.append(c / np.maximum(m, 1e-4))
    return m >= 0.5, np.stack(ch, axis=-1)


def desde_vistas(frente, lado, arriba, res=40, umbral=40, voltear_lado=False, voltear_arriba=False):
    """(filled, rgb) en ejes del .vox: x a la derecha (vista de frente), y hacia atras (0: el frente), z arriba."""
    fm, fc = silueta(frente, umbral)
    sm, sc = silueta(lado, umbral)
    tm, tc = silueta(arriba, umbral)
    if voltear_lado:
        sm, sc = sm[:, ::-1], sc[:, ::-1]
    if voltear_arriba:
        tm, tc = tm[::-1], tc[::-1]
    hf, wf = fm.shape
    hs, ds = sm.shape
    X, Z, Y = float(wf), float(hf), ds * hf / float(hs)  # en pixeles de la vista de frente
    k = res / max(X, Y, Z)
    nx, ny, nz = (max(1, int(round(v * k))) for v in (X, Y, Z))
    F, Fc = remuestrear(fm, fc, nx, nz)  # filas: z de arriba abajo; columnas: x
    S, Sc = remuestrear(sm, sc, ny, nz)  # columnas: y desde el frente
    T, Tc = remuestrear(tm, tc, nx, ny)  # filas: y desde atras; columnas: x
    F, Fc = F[::-1].T, Fc[::-1].transpose(1, 0, 2)  # [x, z]
    S, Sc = S[::-1].T, Sc[::-1].transpose(1, 0, 2)  # [y, z]
    T, Tc = T[::-1].T, Tc[::-1].transpose(1, 0, 2)  # [x, y]
    filled = F[:, None, :] & S[None, :, :] & T[:, :, None]
    rgb = np.broadcast_to(Fc[:, None, :, :], filled.shape + (3,)).copy()  # de base, el color del frente
    _, d = expuestas(filled)
    side = np.broadcast_to(Sc[None, :, :, :], rgb.shape)
    top = np.broadcast_to(Tc[:, :, None, :], rgb.shape)
    for mask, src in ((d[4] | d[5], top), (d[0] | d[1], side), (d[2], np.broadcast_to(Fc[:, None, :, :], rgb.shape))):
        rgb[mask] = src[mask]  # en orden: arriba y abajo, los lados y, sobre todo, el frente
    log(f"[voxeles] vistas: {nx}x{ny}x{nz} celdas (frente {wf}x{hf} px, lado {ds}x{hs}, arriba {tm.shape[1]}x{tm.shape[0]})")
    return filled, rgb


def corridas(v):
    """Los tramos [a, b) de True de un vector."""
    d = np.diff(np.concatenate([[0], v.astype(np.int8), [0]]))
    return list(zip(np.flatnonzero(d == 1), np.flatnonzero(d == -1)))


def secciones_elipse(F, S):
    """Sin vista de arriba: en cada altura, una elipse en cada rectangulo que dejan el frente y el lado (lo
    redondo: animales, cascos, piedras). Toca los cuatro lados del rectangulo: las siluetas no cambian."""
    nx, nz = F.shape
    ny = S.shape[0]
    filled = np.zeros((nx, ny, nz), bool)
    for z in range(nz):
        for x0, x1 in corridas(F[:, z]):
            for y0, y1 in corridas(S[:, z]):
                xs = (np.arange(x0, x1) + 0.5 - (x0 + x1) / 2.0) / ((x1 - x0) / 2.0)
                ys = (np.arange(y0, y1) + 0.5 - (y0 + y1) / 2.0) / ((y1 - y0) / 2.0)
                e = xs[:, None] ** 2 + ys[None, :] ** 2 <= 1.0
                e[(x1 - x0) // 2, :] = True  # la cruz del centro, entera: las siluetas quedan iguales
                e[:, (y1 - y0) // 2] = True
                filled[x0:x1, y0:y1, z] |= e
    return filled


def desde_hoja(path, res=48, umbral=40, filas=1, fila=1, voltear_lado=False, seccion="caja", vertical=False):
    """(filled, rgb) desde una hoja: la forma, del frente y del lado; el color de atras, de la espalda."""
    (fm, fc), (sm, sc), (bm, bc) = hoja(path, umbral, filas, fila, vertical)
    if voltear_lado:
        sm, sc = sm[:, ::-1], sc[:, ::-1]
    hf, wf = fm.shape
    hs, ds = sm.shape
    X, Z, Y = float(wf), float(hf), ds * hf / float(hs)
    k = res / max(X, Y, Z)
    nx, ny, nz = (max(1, int(round(v * k))) for v in (X, Y, Z))
    F, Fc = remuestrear(fm, fc, nx, nz)
    S, Sc = remuestrear(sm, sc, ny, nz)
    _, Bc = remuestrear(bm[:, ::-1], bc[:, ::-1], nx, nz)  # la espalda, espejada: su izquierda es nuestra derecha
    F, Fc = F[::-1].T, Fc[::-1].transpose(1, 0, 2)  # [x, z]
    S, Sc = S[::-1].T, Sc[::-1].transpose(1, 0, 2)  # [y, z]
    Bc = Bc[::-1].transpose(1, 0, 2)
    filled = secciones_elipse(F, S) if seccion == "elipse" else F[:, None, :] & S[None, :, :]
    side = np.broadcast_to(Sc[None, :, :, :], filled.shape + (3,))
    rgb = side.copy()  # de base (arriba, abajo y los lados), el color del lado
    _, d = expuestas(filled)
    rgb[d[3]] = np.broadcast_to(Bc[:, None, :, :], rgb.shape)[d[3]]  # atras, la espalda
    rgb[d[2]] = np.broadcast_to(Fc[:, None, :, :], rgb.shape)[d[2]]  # y sobre todo, el frente
    log(f"[voxeles] hoja (fila {fila} de {filas}): {nx}x{ny}x{nz} celdas (frente {wf}x{hf} px, lado {ds}x{hs})")
    return filled, rgb


# ---------------------------------------------------------------------------------------- splat
PLY_TYPES = {"char": "i1", "uchar": "u1", "short": "i2", "ushort": "u2", "int": "i4", "uint": "u4",
             "float": "f4", "double": "f8", "int8": "i1", "uint8": "u1", "int16": "i2", "uint16": "u2",
             "int32": "i4", "uint32": "u4", "float32": "f4", "float64": "f8"}


def leer_ply(path):
    """Los vertices de un .ply (binario o ascii) como arreglo estructurado de numpy."""
    with open(path, "rb") as f:
        fmt, count, props, in_vertex = None, 0, [], False
        while True:
            line = f.readline()
            if not line:
                raise ValueError(f"{path}: cabecera PLY sin end_header")
            w = line.decode("ascii", "replace").split()
            if not w:
                continue
            if w[0] == "format":
                fmt = w[1]
            elif w[0] == "element":
                in_vertex = w[1] == "vertex"
                if in_vertex:
                    count = int(w[2])
            elif w[0] == "property" and in_vertex:
                if w[1] == "list":
                    raise ValueError(f"{path}: los vertices con listas no se leen")
                props.append((w[2], PLY_TYPES[w[1]]))
            elif w[0] == "end_header":
                break
        if fmt == "ascii":
            rows = np.loadtxt(f, max_rows=count, ndmin=2)
            out = np.zeros(count, dtype=[(n, t) for n, t in props])
            for i, (n, _) in enumerate(props):
                out[n] = rows[:, i]
            return out
        order = "<" if fmt == "binary_little_endian" else ">"
        dt = np.dtype([(n, order + t) for n, t in props])
        return np.frombuffer(f.read(dt.itemsize * count), dtype=dt, count=count)


def ejes(points, arriba, girar):
    """Los puntos (N, 3) del archivo a ejes del .vox (z arriba), girados en torno a la vertical."""
    p = np.asarray(points, np.float64)
    a = arriba.lstrip("+-")
    sign = -1.0 if arriba.startswith("-") else 1.0
    if a == "y":  # glTF y la mayoria de los splats: y arriba, el frente hacia +z -> -y del .vox
        p = np.stack([p[:, 0], -p[:, 2], p[:, 1] * sign], axis=1)
        if sign < 0:
            p[:, 1] = -p[:, 1]
    elif a == "z":
        p = np.stack([p[:, 0], p[:, 1], p[:, 2] * sign], axis=1)
    else:
        raise ValueError("--arriba: y, -y, z o -z")
    if girar:
        c, s = math.cos(math.radians(girar)), math.sin(math.radians(girar))
        p = np.stack([p[:, 0] * c - p[:, 1] * s, p[:, 0] * s + p[:, 1] * c, p[:, 2]], axis=1)
    return p


def rejilla(points, rgb, weight, res, densidad):
    """Nube de puntos -> (filled, rgb): celdas con peso >= densidad, cerradas y rellenas por dentro."""
    from scipy import ndimage

    # Lo que flota lejos (las gaussianas sueltas) queda afuera; la caja, justa a lo que queda.
    lo = np.percentile(points, 0.5, axis=0)
    hi = np.percentile(points, 99.5, axis=0)
    span = np.maximum(hi - lo, 1e-6)
    keep = np.all((points >= lo - span * 0.1) & (points <= hi + span * 0.1), axis=1)
    points, rgb, weight = points[keep], rgb[keep], weight[keep]
    lo, hi = points.min(axis=0), points.max(axis=0)
    hi = hi + np.maximum(hi - lo, 1e-6) * 1e-4
    cell = float((hi - lo).max()) / res
    dims = np.maximum(1, np.ceil((hi - lo) / cell).astype(int))
    inside = np.all((points >= lo) & (points < hi), axis=1)
    idx = ((points[inside] - lo) / cell).astype(int)
    idx = np.minimum(idx, dims - 1)
    w, c = weight[inside], rgb[inside]
    flat = np.ravel_multi_index(idx.T, dims)
    acc = np.bincount(flat, weights=w, minlength=int(np.prod(dims)))
    col = np.stack([np.bincount(flat, weights=w * c[:, i], minlength=acc.size) for i in range(3)], axis=1)
    acc, col = acc.reshape(dims), col.reshape(tuple(dims) + (3,))
    colored = acc >= densidad
    # Cerrar las rendijas de la cascara y rellenar lo de adentro, con un margen: la erosion del cierre se
    # come lo que toca el borde de la rejilla (y por ahi se escaparia el relleno).
    pad = np.pad(colored, 2)
    filled = ndimage.binary_fill_holes(ndimage.binary_closing(pad, iterations=1))[2:-2, 2:-2, 2:-2] | colored
    out = np.zeros(tuple(dims) + (3,), np.float32)
    out[colored] = col[colored] / acc[colored][:, None]
    if colored.any():  # lo que lleno el cierre: el color de la celda con color mas cercana
        _, near = ndimage.distance_transform_edt(~colored, return_indices=True)
        out = out[near[0], near[1], near[2]]
    return filled, out


def desde_splat(path, res=48, arriba="y", girar=0.0, opacidad=0.3, densidad=0.5):
    v = leer_ply(path)
    names = v.dtype.names
    pts = np.stack([v["x"], v["y"], v["z"]], axis=1).astype(np.float64)
    if "f_dc_0" in names:  # 3DGS: el color es el termino de grado 0 de los armonicos esfericos
        c0 = 0.28209479177387814
        rgb = np.stack([0.5 + c0 * v[f"f_dc_{i}"] for i in range(3)], axis=1) * 255.0
    elif "red" in names:
        rgb = np.stack([v["red"], v["green"], v["blue"]], axis=1).astype(np.float64)
    else:
        rgb = np.full((len(v), 3), 160.0)
    alpha = 1.0 / (1.0 + np.exp(-v["opacity"].astype(np.float64))) if "opacity" in names else np.ones(len(v))
    keep = alpha >= opacidad
    log(f"[voxeles] splat: {len(v)} gaussianas, {int(keep.sum())} con opacidad >= {opacidad}")
    pts = ejes(pts[keep], arriba, girar)
    return rejilla(pts, np.clip(rgb[keep], 0, 255), alpha[keep], res, densidad)


# ---------------------------------------------------------------------------------------- malla
def frente_a_menos_y(a, frente):
    """Gira la rejilla (x, y, z, ...) en torno a la vertical para que el frente quede hacia -y (y = 0)."""
    if frente == "+y":  # (x, y) -> (-x, -y)
        return a[::-1, ::-1]
    if frente == "+x":  # (x, y) -> (y, -x): lo de x alto queda en y = 0
        return np.swapaxes(a, 0, 1)[:, ::-1]
    if frente == "-x":  # (x, y) -> (-y, x)
        return np.swapaxes(a[:, ::-1], 0, 1)
    return a  # ya mira a -y


def desde_malla(path, res=48, frente="+x"):
    """GLB (y arriba, como glTF) -> (filled, rgb) del .vox. frente: hacia donde mira el objeto en el GLB."""
    import trimesh
    from scipy.spatial import cKDTree

    if frente not in ("+x", "-x", "+z", "-z"):
        raise ValueError("--frente: +x, -x, +z o -z")
    mesh = trimesh.load(path, force="mesh")
    pitch = float(mesh.extents.max()) / res
    vg = mesh.voxelized(pitch).fill()
    mat = np.asarray(vg.matrix, bool)  # ejes del GLB: x, y (arriba), z
    ijk = np.argwhere(mat)
    centers = vg.indices_to_points(ijk)
    col = np.zeros(mat.shape + (3,), np.float32)
    col[tuple(ijk.T)] = colores_de_malla(mesh, centers, cKDTree)
    log(f"[voxeles] malla: {len(mesh.faces)} triangulos, {len(ijk)} celdas")
    # Al .vox: (x, y, z) = (x, -z, y) del GLB, y el frente hacia -y.
    filled = np.transpose(mat, (0, 2, 1))[:, ::-1, :]
    rgb = np.transpose(col, (0, 2, 1, 3))[:, ::-1, :, :]
    f = {"+x": "+x", "-x": "-x", "+z": "-y", "-z": "+y"}[frente]
    return np.ascontiguousarray(frente_a_menos_y(filled, f)), np.ascontiguousarray(frente_a_menos_y(rgb, f))


def colores_de_malla(mesh, points, kdtree):
    """El color de la malla en cada punto: con textura, el texel del punto mas cercano de la superficie;
    si no, el color del vertice mas cercano (o gris, sin color)."""
    import trimesh

    vis = mesh.visual
    if getattr(vis, "kind", None) == "texture" and getattr(vis, "uv", None) is not None:
        mat = vis.material
        img = getattr(mat, "baseColorTexture", None) or getattr(mat, "image", None)
        if img is not None:
            tex = np.asarray(img.convert("RGB"), np.float32)
            closest, _, tri = trimesh.proximity.closest_point(mesh, points)
            faces = mesh.faces[tri]
            bary = trimesh.triangles.points_to_barycentric(mesh.vertices[faces], closest)
            uv = np.einsum("ij,ijk->ik", bary, np.asarray(vis.uv)[faces])
            h, w = tex.shape[:2]
            u = np.clip((uv[:, 0] % 1.0) * (w - 1), 0, w - 1).astype(int)
            v = np.clip((1.0 - uv[:, 1] % 1.0) * (h - 1), 0, h - 1).astype(int)
            col = tex[v, u]
            factor = getattr(mat, "baseColorFactor", None)
            if factor is not None:
                col = col * (np.asarray(factor, np.float32)[:3] / (255.0 if np.max(factor) > 1.0 else 1.0))
            return col
        vis = vis.to_color()
    try:
        vcol = np.asarray(vis.vertex_colors)[:, :3].astype(np.float32)
    except Exception:  # sin color ni textura
        vcol = np.full((len(mesh.vertices), 3), 160.0, np.float32)
    _, near = kdtree(mesh.vertices).query(points)
    return vcol[near]


# ---------------------------------------------------------------------------------------- superficie suave
def superficie(filled, rgb):
    """La superficie suave del volumen: (vertices, caras, colores RGB), en los ejes de un GLB del juego (x
    hacia el frente, y arriba, z a la izquierda) y en celdas. mcubes.smooth alisa sin perder lo fino (las
    patas, las hojas de las armas): la superficie sigue pasando entre las celdas llenas y las vacias."""
    import mcubes
    import trimesh
    from scipy.spatial import cKDTree

    pad = np.pad(filled, 2)
    field = mcubes.smooth(pad)
    v, f = mcubes.marching_cubes(field, 0.0)
    v = v - 2.0
    surf, _ = expuestas(filled)
    cells = np.argwhere(surf)
    _, near = cKDTree(cells).query(v)
    col = rgb[tuple(cells[near].T)]
    # Del .vox (x a la derecha, y hacia atras, z arriba) al GLB: el frente (y = 0) hacia +X.
    sx, sy, _ = filled.shape
    out = np.stack([sy - 1 - v[:, 1], v[:, 2], sx - 1 - v[:, 0]], axis=1)
    mesh = trimesh.Trimesh(out, f, process=True)
    mesh.update_faces(mesh.nondegenerate_faces())  # marching cubes deja triangulos sin area: rompen la malla
    mesh.remove_unreferenced_vertices()
    mesh.merge_vertices()
    mesh.fix_normals()
    keep = mesh.vertices  # process=True puede juntar vertices: el color se vuelve a buscar
    _, near = cKDTree(out).query(keep)
    return np.asarray(mesh.vertices), np.asarray(mesh.faces), col[near]


def guardar_glb(path, filled, rgb):
    import trimesh

    v, f, c = superficie(filled, rgb)
    rgba = np.concatenate([np.clip(c, 0, 255).astype(np.uint8), np.full((len(c), 1), 255, np.uint8)], axis=1)
    trimesh.Trimesh(v, f, vertex_colors=rgba, process=False).export(path)
    log(f"[voxeles] {path}: superficie suave de {len(f)} triangulos (para modelos.py traer --suave)")


# ---------------------------------------------------------------------------------------- ver
def primera(filled, axis, desde_el_final):
    """Para cada rayo a lo largo de axis: si toca algo y el indice de la primera celda llena."""
    f = np.flip(filled, axis) if desde_el_final else filled
    hit = f.any(axis=axis)
    i = f.argmax(axis=axis)
    return hit, (filled.shape[axis] - 1 - i) if desde_el_final else i


def vista_previa(grid, pal, path, escala=6):
    """El frente, el lado derecho, arriba y una isometrica (frente, derecha y arriba), con luz por cara."""
    filled = grid > 0
    sx, sy, sz = grid.shape
    rgb = pal[np.maximum(grid.astype(int) - 1, 0), :3].astype(np.uint8)
    bg = np.array([236, 236, 236], np.uint8)

    def imagen(hit, col):  # (a, b) -> filas b de arriba abajo, columnas a
        img = np.where(hit[..., None], col, bg)
        img = np.transpose(img, (1, 0, 2))[::-1]
        return Image.fromarray(np.ascontiguousarray(img)).resize((img.shape[1] * escala, img.shape[0] * escala), Image.NEAREST)

    X, Z = np.meshgrid(np.arange(sx), np.arange(sz), indexing="ij")
    h, iy = primera(filled, 1, False)
    front = imagen(h, rgb[X, iy, Z])
    Y, Z2 = np.meshgrid(np.arange(sy), np.arange(sz), indexing="ij")
    h, ix = primera(filled, 0, True)
    side = imagen(h, rgb[ix, Y, Z2])  # el frente a la izquierda
    X2, Y2 = np.meshgrid(np.arange(sx), np.arange(sy), indexing="ij")
    h, iz = primera(filled, 2, True)
    top = imagen(h, rgb[X2, Y2, iz])  # el frente abajo
    # Isometrica desde adelante, a la derecha y arriba: se ven las caras -y, +x y +z.
    s = escala
    W = int((sx + sy) * s * 0.9) + 2 * s
    H = int((sx + sy) * s * 0.5 + sz * s) + 2 * s
    iso = Image.new("RGB", (W, H), tuple(int(v) for v in bg))
    dr = ImageDraw.Draw(iso)
    surf, d = expuestas(filled)
    ox, oy = s, sy * s * 0.5 + sz * s + s

    def P(x, y, z):
        return (ox + (x + y) * s * 0.9, oy + (x - y) * s * 0.5 - z * s)

    cells = np.argwhere(surf)
    if len(cells):
        cells = cells[np.argsort(cells[:, 0] - cells[:, 1] + cells[:, 2], kind="stable")]  # de lejos a cerca
    for x, y, z in cells:
        c = rgb[x, y, z].astype(np.float32)
        if d[5][x, y, z]:
            dr.polygon([P(x, y, z + 1), P(x + 1, y, z + 1), P(x + 1, y + 1, z + 1), P(x, y + 1, z + 1)], fill=tuple(int(v) for v in c))
        if d[2][x, y, z]:
            dr.polygon([P(x, y, z), P(x + 1, y, z), P(x + 1, y, z + 1), P(x, y, z + 1)], fill=tuple(int(v * 0.84) for v in c))
        if d[1][x, y, z]:
            dr.polygon([P(x + 1, y, z), P(x + 1, y + 1, z), P(x + 1, y + 1, z + 1), P(x + 1, y, z + 1)], fill=tuple(int(v * 0.66) for v in c))
    pad = 8
    tiles = [front, side, top, iso]
    out = Image.new("RGB", (sum(t.width for t in tiles) + pad * 5, max(t.height for t in tiles) + 2 * pad), (210, 210, 210))
    x = pad
    for t in tiles:
        out.paste(t, (x, pad))
        x += t.width + pad
    out.save(path)
    log(f"[voxeles] vista previa: {path}")


# ---------------------------------------------------------------------------------------- main
def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    common = argparse.ArgumentParser(add_help=False)
    common.add_argument("-o", "--salida", required=True)
    common.add_argument("--colores", type=int, default=24, help="colores de la paleta (1 a 255)")
    common.add_argument("--vista-previa", help="PNG con el frente, el lado, arriba y la isometrica")
    v = sub.add_parser("vistas", parents=[common])
    v.add_argument("frente")
    v.add_argument("lado")
    v.add_argument("arriba")
    v.add_argument("--res", type=int, default=40, help="celdas del lado mas largo")
    v.add_argument("--umbral", type=int, default=40, help="cuanto se aparta del fondo (suma RGB)")
    v.add_argument("--voltear-lado", action="store_true")
    v.add_argument("--voltear-arriba", action="store_true")
    h = sub.add_parser("hoja", parents=[common])
    h.add_argument("imagen")
    h.add_argument("--res", type=int, default=48, help="celdas del lado mas largo")
    h.add_argument("--umbral", type=int, default=40, help="cuanto se aparta del fondo (suma RGB)")
    h.add_argument("--filas", type=int, default=1, help="objetos en la hoja, uno debajo del otro")
    h.add_argument("--fila", type=int, default=1, help="cual (1 es el de arriba)")
    h.add_argument("--voltear-lado", action="store_true", help="en la vista de lado, el frente esta a la derecha")
    h.add_argument("--vertical", action="store_true", help="las tres vistas, una debajo de la otra")
    h.add_argument("--seccion", choices=("caja", "elipse"), default="caja",
                   help="cada altura: caja (carros, casas, muros) o elipse (animales, cascos, piedras)")
    s = sub.add_parser("splat", parents=[common])
    s.add_argument("ply")
    s.add_argument("--res", type=int, default=48)
    s.add_argument("--arriba", default="y", help="el eje de arriba en el archivo: y, -y, z o -z")
    s.add_argument("--girar", type=float, default=0.0, help="grados en torno a la vertical")
    s.add_argument("--opacidad", type=float, default=0.3)
    s.add_argument("--densidad", type=float, default=0.5, help="opacidad sumada para llenar una celda")
    m = sub.add_parser("malla", parents=[common])
    m.add_argument("glb")
    m.add_argument("--res", type=int, default=48)
    m.add_argument("--frente", default="+x", help="hacia donde mira el objeto en el GLB: +x, -x, +z o -z")
    w = sub.add_parser("ver")
    w.add_argument("vox")
    w.add_argument("-o", "--salida", required=True)
    a = ap.parse_args(argv)
    if a.cmd == "ver":
        grid, pal = vox.leer(a.vox)
        vista_previa(grid, pal, a.salida)
        return 0
    if a.cmd == "vistas":
        filled, rgb = desde_vistas(a.frente, a.lado, a.arriba, a.res, a.umbral, a.voltear_lado, a.voltear_arriba)
    elif a.cmd == "hoja":
        filled, rgb = desde_hoja(a.imagen, a.res, a.umbral, a.filas, a.fila, a.voltear_lado, a.seccion, a.vertical)
    elif a.cmd == "splat":
        filled, rgb = desde_splat(a.ply, a.res, a.arriba, a.girar, a.opacidad, a.densidad)
    else:
        filled, rgb = desde_malla(a.glb, a.res, a.frente)
    if not filled.any():
        log("[voxeles] no quedo ninguna celda: revisa las vistas o los parametros")
        return 1
    if max(filled.shape) > vox.MAX:
        log(f"[voxeles] {filled.shape}: mas de {vox.MAX} celdas por eje; baja --res")
        return 1
    if a.salida.lower().endswith(".glb"):
        guardar_glb(a.salida, filled, rgb)
        return 0
    grid, pal = guardar(a.salida, filled, rgb, a.colores)
    if a.vista_previa:
        vista_previa(grid, np.concatenate([pal, np.full((len(pal), 1), 255, np.uint8)], axis=1), a.vista_previa)
    return 0


if __name__ == "__main__":
    sys.exit(main())
