"""De un modelo de alta resolucion (TRELLIS.2, un escaneo, un GLB cualquiera) al estilo del juego.

convertir(fuente, medidas, tris_max, ...) devuelve el GLB low-poly (bytes) y un informe:
  1. junta todas las mallas de la escena (con sus transformaciones) y quita los pedacitos sueltos;
  2. gira el frente hacia +X (girar: grados alrededor de +Y; TRELLIS suele dejarlo hacia +Z);
  3. escala a las medidas del inventario por su lado mas largo (ancho x alto x largo: Z, Y, X) y
     deja el origen en el centro de la base (o en `origen`, el punto de agarre de un arma);
  4. diezma hasta tris_max (fast-simplification) sin mirar las costuras de la textura;
  5. pinta cada cara con el color de la superficie original debajo (la textura o los colores de
     vertice), lo reduce a una paleta corta y le hornea la luz del sol del juego (la misma del
     terreno, src/world/terrain.c): un facetado de colores planos, sin texturas, que raylib dibuja
     de una vez.
     suave=True (personas, animales y armas, al estilo de PS2): los vertices se comparten, la normal de
     cada uno es el promedio de sus caras y el color (y la luz) va por vertice, asi que se interpola
     entre las caras en lugar de facetarlas.
Dependencias (no las necesita el juego ni el chequeo del CI): tools/modelos/requirements.txt.
"""
import io
import json
import struct

import numpy as np
import trimesh
from PIL import Image
from scipy.spatial import cKDTree

# La luz horneada del terreno (src/world/terrain.c: push_tri).
SUN = np.array([-0.45, 1.0, -0.3]) / np.linalg.norm([-0.45, 1.0, -0.3])
AMBIENT, DIFFUSE = 0.55, 0.45


class ConversionError(Exception):
    pass


def _colors_at(mesh, face, bary):
    """El color (RGB 0..255) de la superficie en las caras face, en las coordenadas baricentricas bary
    (la textura de su material, sus colores de vertice o de cara, o el color del material)."""
    vis, n = mesh.visual, len(face)
    mat = getattr(vis, "material", None)
    uv = getattr(vis, "uv", None)
    if uv is not None and len(uv) == len(mesh.vertices):
        uvs = np.einsum("nj,njk->nk", bary, np.asarray(uv)[mesh.faces[face]])
        try:
            cols = mat.to_color(uvs) if mat is not None else None
        except Exception:
            cols = None
        if cols is not None:
            cols = np.asarray(cols, dtype=np.float64)
            if cols.ndim == 1:  # sin imagen: el color del material
                cols = np.tile(cols, (n, 1))
            factor = getattr(mat, "baseColorFactor", None)
            if getattr(mat, "baseColorTexture", None) is not None and factor is not None:
                cols[:, :3] *= np.asarray(factor, dtype=np.float64)[:3] / 255.0  # la textura por el factor
            return cols[:, :3]
    if vis.kind == "vertex":
        vc = np.asarray(vis.vertex_colors, dtype=np.float64)[:, :3]
        return np.einsum("nj,njk->nk", bary, vc[mesh.faces[face]])
    if vis.kind == "face":
        return np.asarray(vis.face_colors, dtype=np.float64)[face, :3]
    main = getattr(mat, "main_color", None)
    return np.tile(np.asarray(main[:3] if main is not None else (160, 160, 160), dtype=np.float64), (n, 1))


def _surface_colors(mesh):
    """Muestras de la superficie (puntos y colores): al azar segun el area (con semilla) y el centro
    de cada cara, asi en las mallas gruesas cada cara tiene la suya. Los colores quedan como los da
    la textura (sRGB): raylib no convierte, y asi se ven igual que con la textura."""
    count = int(min(400000, max(20000, len(mesh.faces) * 2)))
    pts, face = trimesh.sample.sample_surface(mesh, count, seed=1)
    tri = mesh.triangles[face]
    bary = trimesh.triangles.points_to_barycentric(tri, pts)
    cols = _colors_at(mesh, face, bary)
    if len(mesh.faces) <= 400000:
        every = np.arange(len(mesh.faces))
        third = np.full((len(every), 3), 1.0 / 3.0)
        pts = np.vstack([pts, mesh.triangles_center])
        cols = np.vstack([cols, _colors_at(mesh, every, third)])
    return np.asarray(pts, dtype=np.float64), np.asarray(cols, dtype=np.float64)


def _load(path):
    """Todas las mallas de la escena en una (en metros del archivo), con su nube de colores."""
    scene = trimesh.load(path, force="scene", process=False)
    verts, faces, cloud_p, cloud_c, offset = [], [], [], [], 0
    for node in scene.graph.nodes_geometry:
        transform, name = scene.graph[node]
        geom = scene.geometry.get(name)
        if not isinstance(geom, trimesh.Trimesh) or len(geom.faces) == 0:
            continue
        m = geom.copy()
        m.apply_transform(transform)
        p, c = _surface_colors(m)
        cloud_p.append(p)
        cloud_c.append(c)
        verts.append(np.asarray(m.vertices, dtype=np.float64))
        faces.append(np.asarray(m.faces, dtype=np.int64) + offset)
        offset += len(m.vertices)
    if not verts:
        raise ConversionError(f"{path}: no tiene mallas de triangulos")
    mesh = trimesh.Trimesh(np.vstack(verts), np.vstack(faces), process=True)  # suelda las costuras
    return mesh, np.vstack(cloud_p), np.vstack(cloud_c)


def _drop_crumbs(mesh, keep=0.02):
    """Quita los pedacitos sueltos (menos de keep del area): el ruido que deja la generacion."""
    parts = mesh.split(only_watertight=False, repair=False)  # medir el area no necesita reparar (ni networkx)
    if len(parts) <= 1:
        return mesh
    total = sum(p.area for p in parts)
    kept = [p for p in parts if p.area >= keep * total]
    return trimesh.util.concatenate(kept) if kept else mesh


def _rotation(axis, degrees):
    """Giro alrededor de un eje (x, y o z), regla de la mano derecha: +90 en y lleva +Z a +X."""
    a = np.radians(degrees)
    c, s = np.cos(a), np.sin(a)
    if axis == "x":
        return np.array([[1.0, 0.0, 0.0], [0.0, c, -s], [0.0, s, c]])
    if axis == "y":
        return np.array([[c, 0.0, s], [0.0, 1.0, 0.0], [-s, 0.0, c]])
    if axis == "z":
        return np.array([[c, -s, 0.0], [s, c, 0.0], [0.0, 0.0, 1.0]])
    raise ConversionError(f"eje '{axis}' (x, y o z)")


def _decimate(mesh, tris_max):
    """Diezma hasta tris_max. El algoritmo (cuadricas rapidas) se frena en las reducciones muy
    grandes (de un millon de caras a unos cientos) y segun el tamaño: se normaliza la malla y se
    repite hasta llegar."""
    import fast_simplification

    v, f = np.asarray(mesh.vertices, dtype=np.float64), np.asarray(mesh.faces, dtype=np.int64)
    for _ in range(16):
        if len(f) <= tris_max:
            return v, f
        center, size = v.mean(axis=0), float(np.linalg.norm(v.max(axis=0) - v.min(axis=0))) or 1.0
        v2, f2 = fast_simplification.simplify((v - center) / size, f, target_reduction=1.0 - tris_max / len(f))
        if len(f2) >= len(f):
            break
        v, f = np.asarray(v2, dtype=np.float64) * size + center, np.asarray(f2, dtype=np.int64)
    if len(f) > tris_max:
        raise ConversionError(f"no baja de {len(f)} triangulos (limite {tris_max})")
    return v, f


# Los puntos de cada cara donde se mira el color de la fuente: el centro y los de una rejilla de
# cuartos (sin las esquinas, que comparte con las vecinas).
BARY = np.array([[1 / 3, 1 / 3, 1 / 3]] + [[i / 4, j / 4, (4 - i - j) / 4] for i in range(4) for j in range(5 - i)
                                            if max(i, j, 4 - i - j) < 4])


def _face_colors(samples, n):
    """El color de cada cara (samples: caras x puntos x RGB): el que mas aparece de una paleta de n
    colores (corte por la mediana de PIL), no el promedio, que embarra los bordes entre colores.
    n = 0: el promedio, sin paleta."""
    faces, k = samples.shape[0], samples.shape[1]
    if n <= 0:
        return samples.mean(axis=1)
    flat = np.clip(np.rint(samples.reshape(-1, 3)), 0, 255).astype(np.uint8)
    q = Image.fromarray(flat.reshape(1, -1, 3), "RGB").quantize(colors=min(n, 256), method=Image.Quantize.MEDIANCUT,
                                                                 dither=Image.Dither.NONE)
    labels = np.asarray(q, dtype=np.int64).reshape(faces, k)
    palette = np.asarray(q.getpalette()[: 3 * 256], dtype=np.float64).reshape(-1, 3)
    votes = np.zeros((faces, len(palette)), dtype=np.int64)
    np.add.at(votes, (np.repeat(np.arange(faces), k), labels.ravel()), 1)
    return palette[votes.argmax(axis=1)]


def _glb_bytes(positions, normals, colors, name):
    """Un GLB de una malla sin indices: tres vertices por cara (facetado), con su color."""
    n = len(positions)
    pos = positions.astype("<f4").tobytes()
    nor = normals.astype("<f4").tobytes()
    col = np.concatenate([colors.astype(np.uint8), np.full((n, 1), 255, np.uint8)], axis=1).tobytes()
    blob = pos + nor + col
    doc = {
        "asset": {"version": "2.0", "generator": "estepa tools/modelos/lowres.py"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": name, "mesh": 0}],
        "meshes": [{"name": name, "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "COLOR_0": 2}, "material": 0}]}],
        "materials": [{"name": "facetado", "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1], "metallicFactor": 0, "roughnessFactor": 1}}],
        "buffers": [{"byteLength": len(blob)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(pos), "target": 34962},
            {"buffer": 0, "byteOffset": len(pos), "byteLength": len(nor), "target": 34962},
            {"buffer": 0, "byteOffset": len(pos) + len(nor), "byteLength": len(col), "target": 34962},
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": n, "type": "VEC3",
             "min": positions.min(axis=0).tolist(), "max": positions.max(axis=0).tolist()},
            {"bufferView": 1, "componentType": 5126, "count": n, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5121, "normalized": True, "count": n, "type": "VEC4"},
        ],
    }
    js = json.dumps(doc, separators=(",", ":")).encode("utf-8")
    js += b" " * (-len(js) % 4)
    blob += b"\x00" * (-len(blob) % 4)
    out = io.BytesIO()
    out.write(struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(js) + 8 + len(blob)))
    out.write(struct.pack("<I4s", len(js), b"JSON") + js)
    out.write(struct.pack("<I4s", len(blob), b"BIN\x00") + blob)
    return out.getvalue()


def _glb_bytes_suave(positions, normals, colors, faces, name):
    """Un GLB con indices: los vertices compartidos, su normal suave y su color (sombreado de Gouraud)."""
    n = len(positions)
    pos = positions.astype("<f4").tobytes()
    nor = normals.astype("<f4").tobytes()
    col = np.concatenate([colors.astype(np.uint8), np.full((n, 1), 255, np.uint8)], axis=1).tobytes()
    big = n > 65535
    idx = faces.astype("<u4" if big else "<u2").ravel().tobytes()
    idx += b"\x00" * (-len(idx) % 4)
    blob = pos + nor + col + idx
    o_col, o_idx = len(pos) + len(nor), len(pos) + len(nor) + len(col)
    doc = {
        "asset": {"version": "2.0", "generator": "estepa tools/modelos/lowres.py (suave)"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"name": name, "mesh": 0}],
        "meshes": [{"name": name, "primitives": [{"attributes": {"POSITION": 0, "NORMAL": 1, "COLOR_0": 2}, "indices": 3,
                                                  "material": 0}]}],
        "materials": [{"name": "suave", "pbrMetallicRoughness": {"baseColorFactor": [1, 1, 1, 1], "metallicFactor": 0, "roughnessFactor": 1}}],
        "buffers": [{"byteLength": len(blob)}],
        "bufferViews": [
            {"buffer": 0, "byteOffset": 0, "byteLength": len(pos), "target": 34962},
            {"buffer": 0, "byteOffset": len(pos), "byteLength": len(nor), "target": 34962},
            {"buffer": 0, "byteOffset": o_col, "byteLength": len(col), "target": 34962},
            {"buffer": 0, "byteOffset": o_idx, "byteLength": faces.size * (4 if big else 2), "target": 34963},
        ],
        "accessors": [
            {"bufferView": 0, "componentType": 5126, "count": n, "type": "VEC3",
             "min": positions.min(axis=0).tolist(), "max": positions.max(axis=0).tolist()},
            {"bufferView": 1, "componentType": 5126, "count": n, "type": "VEC3"},
            {"bufferView": 2, "componentType": 5121, "normalized": True, "count": n, "type": "VEC4"},
            {"bufferView": 3, "componentType": 5125 if big else 5123, "count": int(faces.size), "type": "SCALAR"},
        ],
    }
    js = json.dumps(doc, separators=(",", ":")).encode("utf-8")
    js += b" " * (-len(js) % 4)
    out = io.BytesIO()
    out.write(struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(js) + 8 + len(blob)))
    out.write(struct.pack("<I4s", len(js), b"JSON") + js)
    out.write(struct.pack("<I4s", len(blob), b"BIN\x00") + blob)
    return out.getvalue()


def normales_aristas(v, f, angulo=40.0):
    """La normal de cada esquina (caras x 3 x 3) de una malla de vertices compartidos (v, f): el promedio (por area) de
    las caras de su vertice que no doblan mas de angulo grados respecto de la suya. Suave en lo curvo y cortada en las
    aristas duras: el criterio del patron (tools/modelos/patron.py) para personas, animales y equipo. Para una malla
    cara por cara (sin vertices compartidos), soldarla antes por posicion (normales_soldando)."""
    tri = v[f]
    fn = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    area = np.linalg.norm(fn, axis=1)
    fn = fn / np.maximum(area[:, None], 1e-12)
    lim = np.cos(np.radians(angulo))
    orden = np.argsort(f.ravel(), kind="stable")  # las esquinas de cada vertice, juntas
    vert = f.ravel()[orden]
    cara = orden // 3
    inicio = np.searchsorted(vert, np.arange(len(v)))
    fin = np.searchsorted(vert, np.arange(len(v)), side="right")
    out = np.zeros((len(f), 3, 3))
    for i in range(len(f)):
        for k in range(3):
            vec = cara[inicio[f[i, k]]:fin[f[i, k]]]
            nn = fn[vec]
            ok = nn @ fn[i] >= lim
            s = (nn[ok] * area[vec][ok][:, None]).sum(0)
            out[i, k] = s / max(np.linalg.norm(s), 1e-12)
    return out


def normales_soldando(pos, angulo=40.0):
    """normales_aristas de una malla cara por cara (pos: tres vertices por cara, sin indices), soldada por posicion para
    saber que caras comparten cada vertice. Devuelve una normal por vertice de pos (en su orden)."""
    escala = max(float(np.ptp(pos, axis=0).max()), 1e-9) * 1e-6
    _, inv = np.unique(np.round(pos / escala).astype(np.int64), axis=0, return_inverse=True)
    inv = inv.reshape(-1)
    v = np.zeros((inv.max() + 1, 3))
    v[inv] = pos
    return normales_aristas(v, inv.reshape(-1, 3), angulo).reshape(-1, 3)


def orientar(v, f):
    """Las caras de una malla de vertices compartidos con el sentido de sus vecinas. La reduccion por cuadricas da
    vuelta algunos triangulos (pliega uno sobre otro), y el juego no dibuja las caras de espaldas: por ahi se ve lo
    de atras. Por cada pieza (caras unidas por aristas de dos caras), un recorrido a lo ancho le da a cada cara el
    sentido de la que la alcanzo; despues, toda la pieza toma el sentido que tenia la mayoria de su area."""
    from collections import deque
    f = np.array(f, dtype=np.int64, copy=True)
    tri = v[f]
    area = np.linalg.norm(np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]), axis=1)
    aristas = {}
    for i, (a, b, c) in enumerate(f.tolist()):
        for x, y in ((a, b), (b, c), (c, a)):
            aristas.setdefault((min(x, y), max(x, y)), []).append(i)
    vuelta = np.zeros(len(f), bool)  # dada vuelta respecto de como vino
    hecho = np.zeros(len(f), bool)
    for s in range(len(f)):
        if hecho[s]:
            continue
        pieza, cola = [s], deque([s])
        hecho[s] = True
        while cola:
            i = cola.popleft()
            a, b, c = f[i].tolist()
            for x, y in ((a, b), (b, c), (c, a)):
                vec = aristas[(min(x, y), max(x, y))]
                if len(vec) != 2:  # borde, o arista de mas de dos caras: por ahi no se sigue
                    continue
                j = vec[0] if vec[1] == i else vec[1]
                if hecho[j]:
                    continue
                p, q, r = f[j].tolist()
                if (x, y) in ((p, q), (q, r), (r, p)):  # la recorre en el mismo sentido: al reves de su vecina
                    f[j] = f[j][[0, 2, 1]]
                    vuelta[j] = True
                hecho[j] = True
                cola.append(j)
                pieza.append(j)
        pieza = np.array(pieza)
        if area[pieza][vuelta[pieza]].sum() > area[pieza].sum() / 2:  # la mayoria estaba al reves de como quedo
            f[pieza] = f[pieza][:, [0, 2, 1]]
    return f


def capa_de_afuera(v, f, celdas=280, lejos=(1.0, 1.5, 2.0, 3.0), crecer=6, suelta=0.02):
    """Las caras (mascara booleana) que dan al aire de afuera. Una malla con grosor (rigged-model.glb: el cuerpo es una
    cascara de unos 4 mm, con la capa de adentro mirando al hueco) gasta la mitad de sus triangulos en una capa que no
    se ve, y la de adentro confunde la distancia con signo a la piel.
    1. Las caras, en voxeles (celdas a lo largo del lado mas largo), con una celda mas de pared (asi se cierran las
       grietas de la malla); el aire de afuera es lo que se llena desde una esquina.
    2. Ese aire vuelve a entrar, hasta crecer celdas, por lo libre sin la celda de mas: la entrepierna y entre los
       dedos, que la pared gruesa cerraba.
    3. Una cara es de afuera si, delante de ella (lejos: en celdas, por su normal), algun punto cae en ese aire; las de
       adentro miran al hueco.
    4. Lo que quedo adentro en manchas sueltas (menos de suelta de las caras) es de afuera: la capa de adentro es una
       sola pieza grande. Una malla de una sola capa sale entera."""
    from scipy import ndimage
    from scipy.sparse import coo_matrix
    from scipy.sparse.csgraph import connected_components
    f = np.asarray(f, dtype=np.int64)
    lo = v.min(0)
    h = float(np.ptp(v, axis=0).max()) / celdas
    pad = 4
    dims = np.ceil(np.ptp(v, axis=0) / h).astype(int) + 2 * pad + 1
    tri = v[f]
    largo = np.linalg.norm(tri - np.roll(tri, 1, axis=1), axis=2).max(1)
    n = np.maximum(np.ceil(largo / (h * 0.5)).astype(int), 1)
    puntos = []
    for k in np.unique(n):  # cada triangulo, con muestras a menos de media celda
        sel = tri[n == k]
        g = np.array([(i, j, k - i - j) for i in range(k + 1) for j in range(k + 1 - i)], float) / k
        puntos.append(np.einsum("bk,fkd->fbd", g, sel).reshape(-1, 3))
    q = np.floor((np.concatenate(puntos) - lo) / h).astype(int) + pad
    pared0 = np.zeros(dims, bool)
    pared0[q[:, 0], q[:, 1], q[:, 2]] = True
    lab, _ = ndimage.label(~ndimage.binary_dilation(pared0))
    aire = ndimage.binary_dilation(lab == lab[0, 0, 0], iterations=crecer, mask=~pared0)
    fn = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    fn /= np.maximum(np.linalg.norm(fn, axis=1, keepdims=True), 1e-12)
    c = tri.mean(1)
    afuera = np.zeros(len(f), bool)
    for d in lejos:
        p = np.floor((c + fn * d * h - lo) / h).astype(int) + pad
        p = np.clip(p, 0, np.array(dims) - 1)
        afuera |= aire[p[:, 0], p[:, 1], p[:, 2]]
    resto = np.flatnonzero(~afuera)
    if len(resto):  # las piezas de lo de adentro, por aristas (soldada por posicion: sin las costuras de la textura)
        escala = max(float(np.ptp(v, axis=0).max()), 1e-9) * 1e-6
        _, w = np.unique(np.round(v / escala).astype(np.int64), axis=0, return_inverse=True)
        fw = w.reshape(-1)[f[resto]]
        e = np.sort(np.concatenate([fw[:, [0, 1]], fw[:, [1, 2]], fw[:, [2, 0]]]), axis=1)
        clave = e[:, 0] * (int(w.max()) + 1) + e[:, 1]
        cara = np.tile(np.arange(len(resto)), 3)
        orden = np.argsort(clave, kind="stable")
        igual = clave[orden][1:] == clave[orden][:-1]
        a, b = cara[orden][:-1][igual], cara[orden][1:][igual]
        _, pieza = connected_components(coo_matrix((np.ones(len(a)), (a, b)), shape=(len(resto), len(resto))), directed=False)
        afuera[resto[np.bincount(pieza)[pieza] < suelta * len(f)]] = True
    return afuera


def _suave(v, f, cloud_p, cloud_c, colores, luz):
    """Vertices compartidos con normal suave (el promedio de sus caras, por area) y color por vertice: el
    promedio de la superficie original alrededor de cada uno (sin paleta, salvo colores > 0)."""
    tri = v[f]
    fn = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])  # largo: el doble del area
    keep = np.linalg.norm(fn, axis=1) > 1e-12 * max(1.0, float(np.linalg.norm(fn, axis=1).max(initial=0.0)))
    f, fn = f[keep], fn[keep]
    if len(f) == 0:
        raise ConversionError("no quedo ninguna cara")
    used = np.unique(f)
    remap = np.full(len(v), -1, dtype=np.int64)
    remap[used] = np.arange(len(used))
    v, f = v[used], remap[f]
    vn = np.zeros_like(v)
    for k in range(3):
        np.add.at(vn, f[:, k], fn)
    vn /= np.maximum(np.linalg.norm(vn, axis=1, keepdims=True), 1e-12)
    # El color: los puntos de la fuente mas cercanos a cada vertice (y a medio camino de sus vecinos).
    tree = cKDTree(cloud_p)
    edge = float(np.median(np.linalg.norm(v[f[:, 0]] - v[f[:, 1]], axis=1))) or 1e-3
    near = tree.query_ball_point(v, r=edge * 0.5)
    cols = np.empty((len(v), 3))
    _, nearest = tree.query(v, k=1)
    for i, ids in enumerate(near):
        cols[i] = cloud_c[ids].mean(axis=0) if len(ids) else cloud_c[nearest[i]]
    if colores > 0:  # una paleta corta, como el facetado
        q = Image.fromarray(np.clip(np.rint(cols), 0, 255).astype(np.uint8).reshape(1, -1, 3), "RGB").quantize(
            colors=min(colores, 256), method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
        pal = np.asarray(q.getpalette()[: 3 * 256], dtype=np.float64).reshape(-1, 3)
        cols = pal[np.asarray(q, dtype=np.int64).reshape(-1)]
    palette = len(np.unique(np.rint(cols).astype(np.int64), axis=0))
    if luz:
        cols = cols * (AMBIENT + DIFFUSE * np.clip(vn @ SUN, 0.0, 1.0))[:, None]
    return v, f, vn, cols, palette


def convertir(fuente, medidas, tris_max, girar=0.0, rotar=None, escala=None, origen=None, colores=24, luz=True, limpiar=0.01,
              nombre="modelo", suave=False):
    """fuente: ruta a un GLB (o glTF/OBJ). medidas: (ancho, alto, largo) en m, las del inventario.
    rotar: giros previos [(eje, grados), ...] (una espada acostada: [('z', 90)]); girar: el giro
    final alrededor de +Y, para el frente. escala: el factor a mano (si no, el lado mas largo del
    inventario manda); origen: (x, y, z) en m del modelo ya escalado que queda en el origen (el
    agarre de un arma; si no, el centro de la base). Devuelve (bytes del GLB, informe)."""
    mesh, cloud_p, cloud_c = _load(fuente)
    tris_in = len(mesh.faces)
    if limpiar > 0:
        mesh = _drop_crumbs(mesh, limpiar)
    rot = np.eye(3)
    for axis, degrees in rotar or []:
        rot = _rotation(axis, degrees) @ rot
    rot = _rotation("y", girar) @ rot
    mesh = trimesh.Trimesh(np.asarray(mesh.vertices) @ rot.T, np.asarray(mesh.faces), process=False)
    cloud_p = cloud_p @ rot.T
    v, f = _decimate(mesh, tris_max)
    if suave:
        return _convertir_suave(v, f, cloud_p, cloud_c, medidas, tris_in, girar, rotar, escala, origen, colores, luz, nombre)
    tri = v[f]  # (caras, 3, 3), todavia en las unidades de la fuente
    normals = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    lens = np.linalg.norm(normals, axis=1)
    ok = lens > 1e-12 * max(1.0, float(lens.max(initial=0.0)))
    tri, normals = tri[ok], normals[ok] / lens[ok][:, None]
    if len(tri) == 0:
        raise ConversionError("no quedo ninguna cara")
    # El color de cada cara: el de la superficie original debajo de sus puntos (el que mas aparece).
    samples = np.einsum("kj,fjd->fkd", BARY, tri).reshape(-1, 3)
    _, idx = cKDTree(cloud_p).query(samples, k=1)
    face_cols = _face_colors(cloud_c[idx].reshape(len(tri), len(BARY), 3), colores)
    palette = len(np.unique(np.rint(face_cols).astype(np.int64), axis=0))
    if luz:
        face_cols = face_cols * (AMBIENT + DIFFUSE * np.clip(normals @ SUN, 0.0, 1.0))[:, None]
    # A las medidas del inventario (largo en X, alto en Y, ancho en Z): el lado mas largo manda.
    pts = tri.reshape(-1, 3)
    lo, hi = pts.min(axis=0), pts.max(axis=0)
    ancho, alto, largo = medidas
    target = np.array([largo, alto, ancho], dtype=np.float64)
    if escala is None:
        axis = int(np.argmax(target))
        if hi[axis] - lo[axis] <= 1e-12:
            raise ConversionError("el modelo es plano en el eje de su medida mas larga: revisa --girar")
        escala = float(target[axis] / (hi[axis] - lo[axis]))
    base = np.array([(lo[0] + hi[0]) * 0.5, lo[1], (lo[2] + hi[2]) * 0.5])
    positions = (pts - base) * escala
    if origen is not None:
        positions = positions - np.asarray(origen, dtype=np.float64)
    vnormals = np.repeat(normals, 3, axis=0)
    vcolors = np.repeat(np.clip(np.rint(face_cols), 0, 255), 3, axis=0)
    data = _glb_bytes(positions, vnormals, vcolors, nombre)
    lo, hi = positions.min(axis=0), positions.max(axis=0)
    informe = {
        "tris_fuente": int(tris_in),
        "tris": int(len(tri)),
        "escala": float(escala),
        "girar": float(girar),
        "rotar": [[a, float(d)] for a, d in (rotar or [])],
        "origen": [float(x) for x in (origen if origen is not None else (0.0, 0.0, 0.0))],
        "caja_m": [round(float(x), 4) for x in (hi - lo)],  # X (largo), Y (alto), Z (ancho)
        "paleta": int(palette),
        "bytes": len(data),
    }
    return data, informe


def _a_medidas(pts, medidas, escala, origen):
    """A las medidas del inventario (largo en X, alto en Y, ancho en Z: el lado mas largo manda), con el
    origen en el centro de la base (o en origen). Devuelve (puntos, escala)."""
    lo, hi = pts.min(axis=0), pts.max(axis=0)
    ancho, alto, largo = medidas
    target = np.array([largo, alto, ancho], dtype=np.float64)
    if escala is None:
        axis = int(np.argmax(target))
        if hi[axis] - lo[axis] <= 1e-12:
            raise ConversionError("el modelo es plano en el eje de su medida mas larga: revisa --girar")
        escala = float(target[axis] / (hi[axis] - lo[axis]))
    base = np.array([(lo[0] + hi[0]) * 0.5, lo[1], (lo[2] + hi[2]) * 0.5])
    out = (pts - base) * escala
    if origen is not None:
        out = out - np.asarray(origen, dtype=np.float64)
    return out, float(escala)


def _convertir_suave(v, f, cloud_p, cloud_c, medidas, tris_in, girar, rotar, escala, origen, colores, luz, nombre):
    v, f, vn, cols, palette = _suave(v, f, cloud_p, cloud_c, colores, luz)
    positions, escala = _a_medidas(v, medidas, escala, origen)
    data = _glb_bytes_suave(positions, vn, np.clip(np.rint(cols), 0, 255), f, nombre)
    lo, hi = positions.min(axis=0), positions.max(axis=0)
    informe = {
        "tris_fuente": int(tris_in),
        "tris": int(len(f)),
        "escala": float(escala),
        "girar": float(girar),
        "rotar": [[a, float(d)] for a, d in (rotar or [])],
        "origen": [float(x) for x in (origen if origen is not None else (0.0, 0.0, 0.0))],
        "caja_m": [round(float(x), 4) for x in (hi - lo)],
        "paleta": int(palette),
        "suave": True,
        "bytes": len(data),
    }
    return data, informe
