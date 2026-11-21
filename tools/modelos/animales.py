#!/usr/bin/env python3
"""Los animales de IMPORTED/Animals/Models: mallas sin esqueleto (de Hunyuan3D, blancas salvo la del leopardo, que trae
textura), con su esqueleto de cuadrupedo y sus clips armados aqui.

  python3 tools/modelos/animales.py <carpeta de los GLB> <carpeta de las imagenes> [<n> ...]

(IMPORTED/Animals/Models e IMPORTED/Animals/Imagenes/Clean, en la rama cline/procesar-imagenes-animales; n, los numeros
de la FICHA que se arman: todos si no se dice.)

FICHA: el numero de cada modelo en la ficha numerada que se reviso con el usuario, su archivo, los ids del inventario
que lleva y de donde sale su color (la imagen de referencia de la que salio la malla, su textura o uno liso).

Lo que hace con cada uno:
1. La malla: sin las piezas sueltas (las que flotan lejos del cuerpo, restos de la reconstruccion), el frente a +X,
   +Y arriba y la base en el suelo.
2. El color: el de su imagen de referencia (de lado, mirando a la izquierda) proyectada de costado: cada vertice toma
   el pixel de su largo y su alto, dentro de la silueta (el fondo, del color de las esquinas, queda fuera). El leopardo,
   el de su textura; el oso y el perro, que no tienen imagen, uno liso.
3. La medida del inventario (una escala para los tres ejes: la media del largo y el alto) y su tris_max, con la luz
   del sol horneada cara por cara (lowres.SUN), como el cuerpo humano (humanoide.py).
4. El esqueleto, donde esta cada parte en la malla:
   - el vientre: el corte horizontal mas bajo que abarca casi todo el largo y el ancho del cuerpo;
   - las patas: lo que queda debajo, en cuatro (delantera y trasera, izquierda y derecha), con su eje de abajo
     arriba; cada una, de tres huesos (el brazo o el muslo, la cana y el casco o la mano);
   - la columna (cadera, lomo, pecho) a media altura del cuerpo, sobre las patas;
   - el cuello y la cabeza, hacia lo de mas adelante (el hocico; sin las astas ni los cuernos, que van mas arriba);
   - la cola, hacia lo de mas atras, en dos huesos.
5. Pesos automaticos: cada vertice de una pata, a los dos huesos mas cercanos de esa pata (y la cadera o el pecho
   arriba); los demas, a los dos mas cercanos de la columna, el cuello, la cabeza y la cola.
6. Los clips del rig cuadrupedo (assets/animaciones.tsv), armados con poses:
   - las marchas con el orden de las patas de cada una (caminar, al paso lateral; trotar, en diagonal; galopar, de a
     dos), con el ritmo del largo de las patas, y acechar (agachado y lento);
   - pastar y comer_presa, con el hocico en el suelo;
   - atacar (la embestida o el mordisco), encabritarse, saltar, aullar, recibir_golpe, morir (cae de costado y queda),
     forcejear_lazo, idle y montado_idle (sin echarse ni tirar_carga, opcionales: el juego no los pide).
"""
import json
import os
import re
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "assets"))
import animaciones as indice  # noqa: E402
import inventario  # noqa: E402
import lowres  # noqa: E402

# n: (archivo, ids del inventario, color). El color: "textura", (clave, sufijo) de la imagen de referencia (su nombre
# contiene la clave y termina en sufijo + ".png"; sufijo "" es la que no lleva -1, -2...) o un RGB liso.
FICHA = {
    1: ("01-glb.glb", ["animal.salvaje.leopardo_nieves"], "textura"),
    2: ("hy3d-model.glb", ["animal.salvaje.ibice_2"], ("Siberian mountain ibex", "")),
    3: ("hy3d-model(12).glb", ["animal.salvaje.hiena"], ("striped hyena", "")),
    4: ("hy3d-model(13).glb", ["animal.salvaje.puma"], ("mountain cougar", "")),
    5: ("white_mesh.glb", ["animal.salvaje.ciervo"], ("red deer stag", "-1")),
    6: ("white_mesh(1).glb", ["animal.salvaje.cierva"], ("red deer hind", "")),
    7: ("white_mesh(2).glb", ["animal.montura.camello_bactriano_2"], ("Bactrian camel", "")),
    8: ("white_mesh(3).glb", ["animal.montura.dromedario"], ("dromedary camel", "")),
    9: ("white_mesh(4).glb", ["animal.ganado.yak"], ("highland yak", "-1")),
    10: ("white_mesh(5).glb", ["animal.ganado.cabra"], ("cashmere goat", "-1")),
    11: ("white_mesh(6).glb", ["animal.ganado.buey"], ("draft ox", "-2")),
    12: ("white_mesh(7).glb", ["animal.salvaje.gacela"], ("goitered gazelle", "")),
    13: ("white_mesh(8).glb", ["animal.salvaje.ibice"], ("Siberian mountain ibex", "-1")),
    14: ("white_mesh(9).glb", ["animal.montura.mula"], ("pack mule", "")),
    15: ("white_mesh(10).glb", ["animal.salvaje.zorro"], ("red fox", "-1")),
    16: ("white_mesh(11).glb", ["animal.salvaje.reno", "animal.montura.reno"], ("steppe reindeer", "")),
    17: ("white_mesh(12).glb", ["animal.montura.camello_bactriano"], ("Bactrian camel", "-1")),
    18: ("white_mesh(13).glb", ["animal.montura.elefante_guerra"], ("war elephant", "")),
    19: ("white_mesh(14).glb", ["animal.salvaje.oso"], (96, 68, 44)),
    20: ("white_mesh(15).glb", ["animal.ganado.perro_pastor"], (178, 142, 98)),
}
# Las variantes (id_2) que no estaban en el inventario: una fila como la de su especie.
VARIANTES = {"animal.salvaje.ibice_2": "animal.salvaje.ibice",
             "animal.montura.camello_bactriano_2": "animal.montura.camello_bactriano"}
PATAS = ("di", "dd", "ti", "td")  # delantera / trasera, izquierda (-Z) / derecha (+Z)
NOTA = ("Malla de IMPORTED/Animals/Models (n.º {n} de la ficha), con su esqueleto de cuadrúpedo y sus clips "
        "(tools/modelos/animales.py).")
VIEJA = re.compile(r"Malla low-poly con textura \(estilo PS2\) en arte/modelos/para_riggear/[^;]*; falta el esqueleto y los clips\. ?")


# ------------------------------------------------------------------ cuaterniones (x, y, z, w, como glTF)
def q_eje(eje, grados):
    a = np.radians(grados) / 2.0
    e = np.asarray(eje, float)
    return np.array([*(e / np.linalg.norm(e) * np.sin(a)), np.cos(a)])


def qmul(a, b):
    x1, y1, z1, w1 = a
    x2, y2, z2, w2 = b
    return np.array([w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2, w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
                     w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2, w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2])


def qrot(q, v):
    x, y, z, w = q
    u = np.array([x, y, z])
    return v + 2.0 * np.cross(u, np.cross(u, v) + w * v)


# ------------------------------------------------------------------ la malla
def cargar(ruta):
    """Vertices, caras y (si trae textura) el color de cada vertice, con los vertices repetidos soldados."""
    import trimesh
    sc = trimesh.load(ruta, force="scene")
    g = sc.to_geometry() if hasattr(sc, "to_geometry") else sc.dump(concatenate=True)
    v, f, col = np.asarray(g.vertices, float), np.asarray(g.faces, np.int64), None
    vis = getattr(g, "visual", None)
    mat = getattr(vis, "material", None)
    # El leopardo trae su textura como emisiva (sin luz), con el color base en negro.
    tex = getattr(mat, "baseColorTexture", None) or getattr(mat, "emissiveTexture", None)
    if tex is not None and getattr(vis, "uv", None) is not None:
        img = np.asarray(tex.convert("RGB"), float)
        h, w = img.shape[:2]
        uv = np.asarray(vis.uv, float)  # trimesh da la v hacia arriba (la de OpenGL)
        px = np.clip(np.round((uv[:, 0] % 1.0) * (w - 1)), 0, w - 1).astype(int)
        py = np.clip(np.round((1.0 - uv[:, 1] % 1.0) * (h - 1)), 0, h - 1).astype(int)
        col = img[py, px]
    u, primero, inv = np.unique(np.round(v, 6), axis=0, return_index=True, return_inverse=True)
    f = inv.reshape(-1)[f]
    f = f[(f[:, 0] != f[:, 1]) & (f[:, 1] != f[:, 2]) & (f[:, 0] != f[:, 2])]
    return u, f, (col[primero] if col is not None else None)


def limpiar(v, f, col):
    """Sin las piezas sueltas: las que no llegan a tocar el cuerpo (la pieza mayor). Devuelve tambien cuantas quito."""
    from scipy.sparse import coo_matrix
    from scipy.sparse.csgraph import connected_components
    from scipy.spatial import cKDTree
    n = len(v)
    a, b = np.concatenate([f[:, 0], f[:, 1], f[:, 2]]), np.concatenate([f[:, 1], f[:, 2], f[:, 0]])
    k, lab = connected_components(coo_matrix((np.ones(len(a)), (a, b)), shape=(n, n)), directed=False)
    cara = lab[f[:, 0]]
    cuantas = np.bincount(cara, minlength=k)
    cuerpo = int(np.argmax(cuantas))
    kd = cKDTree(v[lab == cuerpo])
    tope = 0.01 * (v.max(0) - v.min(0)).max()
    fuera = [c for c in range(k) if c != cuerpo and cuantas[c] and kd.query(v[lab == c])[0].min() > tope]
    f = f[~np.isin(cara, fuera)]
    usados = np.unique(f)
    nuevo = np.full(n, -1)
    nuevo[usados] = np.arange(len(usados))
    return v[usados], nuevo[f], (col[usados] if col is not None else None), len(fuera)


def imagen_de(carpeta, clave, sufijo):
    for nombre in sorted(os.listdir(carpeta)):
        if clave in nombre and nombre.endswith(".png"):
            fin = re.search(r"-(\d)\.png$", nombre)
            if (fin and sufijo == "-" + fin.group(1)) or (not fin and not sufijo):
                return os.path.join(carpeta, nombre)
    raise SystemExit(f"no esta la imagen de referencia «{clave}{sufijo}» en {carpeta}")


def color_de_imagen(v, ruta):
    """El color de cada vertice desde la imagen de referencia, vista de lado (mirando a la izquierda: la cabeza a -X)."""
    from PIL import Image
    from scipy import ndimage
    img = np.asarray(Image.open(ruta).convert("RGB"), float)
    borde = np.concatenate([img[:4].reshape(-1, 3), img[-4:].reshape(-1, 3), img[:, :4].reshape(-1, 3), img[:, -4:].reshape(-1, 3)])
    fondo = np.median(borde, axis=0)  # blanco, o el verde de la hiena
    sil = ndimage.binary_opening(np.linalg.norm(img - fondo, axis=2) > 40, iterations=2)
    lab, _ = ndimage.label(sil)
    sil = ndimage.binary_fill_holes(lab == (np.argmax(np.bincount(lab.ravel())[1:]) + 1))
    ys, xs = np.nonzero(sil)
    x0, x1, y0, y1 = xs.min(), xs.max(), ys.min(), ys.max()
    # Sin el halo del fondo en el borde (ni los pixeles que se le parecen: el verde de la hiena en la punta de la cola).
    adentro = ndimage.binary_erosion(sil, iterations=3) & (np.linalg.norm(img - fondo, axis=2) > 90)
    _, (iy, ix) = ndimage.distance_transform_edt(~adentro, return_indices=True)
    lo, hi = v.min(0), v.max(0)
    px = np.clip(np.round(x0 + (v[:, 0] - lo[0]) / (hi[0] - lo[0]) * (x1 - x0)), 0, img.shape[1] - 1).astype(int)
    py = np.clip(np.round(y1 - (v[:, 1] - lo[1]) / (hi[1] - lo[1]) * (y1 - y0)), 0, img.shape[0] - 1).astype(int)
    suave = ndimage.uniform_filter(img, size=(5, 5, 1))
    if fondo[1] > fondo[0] + 40 and fondo[1] > fondo[2] + 40:  # fondo verde: sin el verde que derrama en el animal
        suave[..., 1] = np.minimum(suave[..., 1], np.maximum(suave[..., 0], suave[..., 2]))
    return suave[iy[py, px], ix[py, px]]


def color_liso(v, rgb):
    """Un color, mas oscuro hacia las patas; el hocico (lo de mas adelante, la cabeza a -X), oscuro."""
    lo, hi = v.min(0), v.max(0)
    alto = (v[:, 1] - lo[1]) / (hi[1] - lo[1])
    c = np.asarray(rgb, float)[None, :] * (0.72 + 0.28 * alto[:, None])
    hocico = (v[:, 0] < lo[0] + 0.05 * (hi[0] - lo[0])) & (alto > 0.4)
    c[hocico] *= 0.45
    return c


def orientar(v, textura):
    """El frente a +X: las de Hunyuan3D miran a -X (salen de una imagen de lado, mirando a la izquierda); la del
    leopardo, a +Z."""
    if textura:
        return np.column_stack([v[:, 2], v[:, 1], -v[:, 0]])
    return np.column_stack([-v[:, 0], v[:, 1], -v[:, 2]])


def suavizar(v, f, veces=6, lam=0.5, mu=-0.53):
    """Taubin: alisa el pelo en mechones (la cabra, el yak) sin encoger el cuerpo, para que la reduccion guarde la
    forma y no los mechones."""
    from scipy.sparse import coo_matrix, diags
    n = len(v)
    a = np.concatenate([f[:, 0], f[:, 1], f[:, 2], f[:, 1], f[:, 2], f[:, 0]])
    b = np.concatenate([f[:, 1], f[:, 2], f[:, 0], f[:, 0], f[:, 1], f[:, 2]])
    A = coo_matrix((np.ones(len(a)), (a, b)), shape=(n, n)).tocsr()
    A.data[:] = 1.0
    M = diags(1.0 / np.maximum(np.asarray(A.sum(axis=1)).ravel(), 1.0)) @ A
    for _ in range(veces):
        v = v + lam * (M @ v - v)
        v = v + mu * (M @ v - v)
    return v


def muestras(v, f, n=40000, semilla=7):
    """Puntos repartidos por la superficie (por area): para medir el cuerpo sin depender de la densidad de la malla."""
    rng = np.random.default_rng(semilla)
    t = v[f]
    area = 0.5 * np.linalg.norm(np.cross(t[:, 1] - t[:, 0], t[:, 2] - t[:, 0]), axis=1)
    k = rng.choice(len(f), n, p=area / area.sum())
    r1, r2 = rng.random(n), rng.random(n)
    s = np.sqrt(r1)
    return t[k, 0] * (1 - s)[:, None] + t[k, 1] * (s * (1 - r2))[:, None] + t[k, 2] * (s * r2)[:, None]


def reducir(pos, idx, color, tris):
    import fast_simplification
    from scipy.spatial import cKDTree
    p2, f2 = pos.astype(np.float32), idx.astype(np.int32)
    for _ in range(12):  # de a poco hasta el tope (de una vez, se queda corto)
        if len(f2) <= tris:
            break
        p2, f2 = fast_simplification.simplify(p2, f2, min(0.9, 1.0 - tris / len(f2)))
    _, cerca = cKDTree(pos).query(p2)
    return p2.astype(float), f2.astype(np.int64), color[cerca]


# ------------------------------------------------------------------ el esqueleto
class Esqueleto:
    """Los huesos (nombre, padre, donde empieza) y lo medido del cuerpo: el vientre, las patas, el lomo, la cabeza."""

    def __init__(self):
        self.nombres, self.padres, self.pos = [], [], []

    def hueso(self, nombre, padre, pos):
        self.nombres.append(nombre)
        self.padres.append(self.nombres.index(padre) if padre else -1)
        self.pos.append(np.asarray(pos, float))

    def i(self, nombre):
        return self.nombres.index(nombre)


def _componentes(sl, lo, celda):
    from scipy import ndimage
    gx = ((sl[:, 0] - lo[0]) / celda).astype(int)
    gz = ((sl[:, 2] - lo[2]) / celda).astype(int)
    grid = np.zeros((gx.max() + 2, gz.max() + 2), bool)
    grid[gx, gz] = True
    lab, k = ndimage.label(grid, structure=np.ones((3, 3)))
    return [np.nonzero(lab == c) for c in range(1, k + 1)]


def medir(P):
    """Mide un cuadrupedo en sus puntos P (el frente a +X, el suelo en y = 0, el medio en z = 0)."""
    lo, hi = P.min(0), P.max(0)
    L, A, W = hi - lo
    celda = 0.02 * L
    vientre = None
    for y in np.arange(0.04, 0.8, 0.01) * A:
        sl = P[np.abs(P[:, 1] - y) < 0.01 * A]
        if len(sl) < 30:
            continue
        for gx, gz in _componentes(sl, lo, celda):
            if np.ptp(gx) * celda > 0.4 * L and np.ptp(gz) * celda > 0.3 * W:
                vientre = y
                break
        if vientre is not None:
            break
    if vientre is None:
        raise ValueError("no se encuentra el vientre: ¿es un cuadrupedo de pie?")
    # Las cuatro patas: los puntos de abajo, en cuatro grupos (k-medias desde los cuadrantes).
    bajo = P[P[:, 1] < 0.25 * vientre]
    xm = np.median(bajo[:, 0])
    centros = np.array([[np.percentile(bajo[:, 0], 80), -W / 4], [np.percentile(bajo[:, 0], 80), W / 4],
                        [np.percentile(bajo[:, 0], 20), -W / 4], [np.percentile(bajo[:, 0], 20), W / 4]])
    for _ in range(20):
        d = np.linalg.norm(bajo[:, None, [0, 2]] - centros[None], axis=2)
        grupo = np.argmin(d, axis=1)
        for k in range(4):
            if (grupo == k).any():
                centros[k] = bajo[grupo == k][:, [0, 2]].mean(0)
    orden = [int(np.argmin([np.hypot(c[0] - x, c[1] - z) for c in centros]))
             for x, z in ((xm + L, -W), (xm + L, W), (xm - L, -W), (xm - L, W))]
    if len(set(orden)) < 4:  # dos cuadrantes al mismo grupo: por los signos
        orden = [int(np.argmax(centros[:, 0] * sx + centros[:, 1] * sz))
                 for sx, sz in ((1, -0.5), (1, 0.5), (-1, -0.5), (-1, 0.5))]
    separacion = min(np.ptp(centros[:, 0]), np.ptp(centros[:, 1]))
    radio = 0.45 * max(separacion, 0.05 * L)
    ejes = {}
    for nombre, k in zip(PATAS, orden):
        c = centros[k].copy()
        eje = []
        for y in np.linspace(0.0, 1.05 * vientre, 22):
            sl = P[(np.abs(P[:, 1] - y) < 0.05 * vientre) & (np.hypot(P[:, 0] - c[0], P[:, 2] - c[1]) < radio)]
            if len(sl) > 5:
                c = sl[:, [0, 2]].mean(0)
            eje.append((c[0], y, c[1]))
        ejes[nombre] = np.array(eje)
    xd = np.mean([ejes[n][-1, 0] for n in ("di", "dd")])
    xt = np.mean([ejes[n][-1, 0] for n in ("ti", "td")])
    medio = P[(P[:, 0] > xt + 0.25 * (xd - xt)) & (P[:, 0] < xd - 0.25 * (xd - xt))]
    lomo = np.percentile(medio[:, 1], 98) if len(medio) > 20 else vientre * 2
    return dict(lo=lo, hi=hi, L=L, A=A, W=W, vientre=vientre, ejes=ejes, xd=xd, xt=xt, lomo=lomo, radio=radio)


def armar_esqueleto(P, m):
    """Los huesos, desde las medidas (medir) y los puntos P."""
    e = Esqueleto()
    vb, lomo, L = m["vientre"], m["lomo"], m["L"]
    yc = vb + 0.55 * (lomo - vb)  # la columna, a media altura del cuerpo
    e.hueso("raiz", None, (0.0, 0.0, 0.0))
    e.hueso("cadera", "raiz", (m["xt"], yc, 0.0))
    e.hueso("lomo", "cadera", (0.5 * (m["xt"] + m["xd"]), yc, 0.0))
    e.hueso("pecho", "lomo", (m["xd"], yc, 0.0))
    # La cabeza: el hocico (lo de mas adelante, de los puntos de abajo de esa punta: no las astas) y lo de cerca.
    frente = P[P[:, 0] > m["xd"]]
    punta = frente[frente[:, 0] > np.percentile(frente[:, 0], 99.5) - 0.04 * L]
    hocico = punta[punta[:, 1] <= np.median(punta[:, 1])].mean(0)
    cerca = frente[(np.linalg.norm(frente - hocico, axis=1) < 0.12 * L) & (frente[:, 1] < hocico[1] + 0.1 * L)]
    centro = cerca.mean(0) if len(cerca) > 10 else hocico - np.array([0.05 * L, 0, 0])
    atras = centro - hocico  # la nuca: del hocico hacia el centro de la cabeza, un largo de cabeza (17 % del cuerpo)
    atras = atras / np.linalg.norm(atras) if np.linalg.norm(atras) > 0.03 * L else np.array([-0.96, 0.28, 0.0])
    nuca = hocico + 0.17 * L * atras
    nuca[2] = hocico[2] = 0.0
    cuello = np.array([m["xd"] + 0.25 * (nuca[0] - m["xd"]), yc + 0.4 * (lomo - yc), 0.0])
    if nuca[0] < cuello[0] + 0.02 * L:  # sin cuello (el oso): la nuca, delante del pecho
        nuca[0] = cuello[0] + 0.05 * L
    # La piel alrededor de la columna (lo primero que se toca hacia arriba, hacia abajo y a los costados de cada hueso;
    # hacia arriba, el lomo y no un cuerno que pase por encima): lo que no es de la cabeza (cerca_de_la_cabeza).
    piel = []
    for p in e.pos[1:4]:
        franja = np.abs(P[:, 0] - p[0]) < 0.03 * L
        col = franja & (np.abs(P[:, 2]) < 0.06 * m["W"])
        arriba, abajo = P[col & (P[:, 1] > p[1])], P[col & (P[:, 1] < p[1])]
        lado = P[franja & (np.abs(P[:, 1] - p[1]) < 0.05 * m["A"])]
        piel += [arriba[np.argmin(arriba[:, 1])]] if len(arriba) else []
        piel += [abajo[np.argmax(abajo[:, 1])]] if len(abajo) else []
        piel += [lado[np.argmin(lado[:, 2])], lado[np.argmax(lado[:, 2])]] if len(lado) else []
    e.piel = np.array(piel)
    e.hueso("cuello", "pecho", cuello)
    e.hueso("cabeza", "cuello", nuca)
    e.hueso("hocico", "cabeza", hocico)  # la punta (sin peso: marca el largo de la cabeza)
    # La cola: lo de mas atras de las patas traseras, por encima de los corvejones.
    xcorte = min(m["ejes"][n][:, 0].min() for n in ("ti", "td")) - 0.02 * L
    cola = P[(P[:, 0] < xcorte) & (P[:, 1] > 0.3 * vb)]
    cadera = e.pos[e.i("cadera")]
    if len(cola) > 30:
        punta_cola = cola[np.argmax(np.linalg.norm(cola - cadera, axis=1))]
    else:
        punta_cola = cadera + np.array([-0.2 * L, -0.05 * L, 0.0])
    base = cadera + 0.35 * (punta_cola - cadera)
    base[2] = punta_cola[2] = 0.0
    e.hueso("cola", "cadera", base)
    e.hueso("cola_2", "cola", base + 0.5 * (punta_cola - base))
    e.hueso("cola_punta", "cola_2", punta_cola)
    # Las patas: el brazo o el muslo (dentro del cuerpo), la cana (desde la rodilla), el casco y su punta.
    for n in PATAS:
        eje = m["ejes"][n]
        def en(y):
            return np.array([np.interp(y, eje[:, 1], eje[:, 0]), y, np.interp(y, eje[:, 1], eje[:, 2])])
        arriba = en(vb)
        arriba[1] = vb + 0.4 * (yc - vb)
        arriba[2] *= 0.6
        padre = "pecho" if n[0] == "d" else "cadera"
        e.hueso(n + "_1", padre, arriba)
        e.hueso(n + "_2", n + "_1", en(0.5 * vb))
        e.hueso(n + "_3", n + "_2", en(0.12 * vb))
        e.hueso(n + "_punta", n + "_3", en(0.0) + np.array([0.04 * vb, 0.0, 0.0]))
    return e


# ------------------------------------------------------------------ los pesos
def _dist_segmento(p, a, b):
    ab = b - a
    t = np.clip(((p - a) @ ab) / max(ab @ ab, 1e-12), 0.0, 1.0)
    return np.linalg.norm(p - (a + t[:, None] * ab), axis=1)


def cerca_de_la_cabeza(v, f, e, m):
    """Que vertices estan mas cerca (por la superficie) de la cabeza que del cuerpo: la cabeza con sus cuernos y sus
    astas, que se curvan sobre el lomo pero salen de la cabeza. El cuerpo: la piel sobre, bajo y a los lados de la
    cadera, el lomo y el pecho (armar_esqueleto; la cruz, para que no se la lleve la cabeza)."""
    from scipy.sparse import coo_matrix
    from scipy.sparse.csgraph import connected_components, dijkstra
    from scipy.spatial import cKDTree
    a = np.concatenate([f[:, 0], f[:, 1], f[:, 2]])
    b = np.concatenate([f[:, 1], f[:, 2], f[:, 0]])
    G = coo_matrix((np.linalg.norm(v[a] - v[b], axis=1), (a, b)), shape=(len(v), len(v))).tocsr()
    k, lab = connected_components(G, directed=False)
    if k > 1:  # las piezas sueltas tras la reduccion (la punta de un cuerno): un puente a lo mas cercano del cuerpo
        mayor = np.nonzero(lab == np.argmax(np.bincount(lab)))[0]
        kd = cKDTree(v[mayor])
        pa, pb, pw = [], [], []
        for c in set(lab[f[:, 0]]) - {lab[mayor[0]]}:
            dentro = np.nonzero(lab == c)[0]
            dist, j = kd.query(v[dentro])
            q = int(np.argmin(dist))
            pa.append(dentro[q]), pb.append(mayor[j[q]]), pw.append(max(dist[q], 1e-6))
        G = G + coo_matrix((pw, (pa, pb)), shape=G.shape).tocsr()
    nuca, hocico = e.pos[e.i("cabeza")], e.pos[e.i("hocico")]
    cabeza = [nuca, 0.5 * (nuca + hocico), nuca + [0.0, 0.05 * m["L"], 0.0]]  # de donde salen los cuernos y las astas
    semillas = [int(np.argmin(np.linalg.norm(v - q, axis=1))) for q in [*cabeza, *e.piel]]
    d = dijkstra(G, directed=False, indices=semillas)
    return (d[:3].min(axis=0) < d[3:].min(axis=0)) & (v[:, 0] > e.pos[e.i("cadera")][0])


def pesos(v, f, e, m):
    """Dos huesos por vertice: en una pata, los de esa pata (y la cadera o el pecho, arriba); si no, los del cuerpo."""
    segs = {}  # cada hueso, de donde empieza a donde empieza su primer hijo (sin la raiz ni las puntas)
    for k, n in enumerate(e.nombres):
        hijos = [c for c in range(len(e.nombres)) if e.padres[c] == k]
        if k and hijos:
            segs[k] = (e.pos[k], e.pos[hijos[0]])
    d = np.stack([_dist_segmento(v, a, b) for a, b in segs.values()], axis=1)
    claves = list(segs)
    vb = m["vientre"]
    # A que pata pertenece cada vertice bajo el vientre: la de eje mas cerca en el plano, a su altura.
    pata = np.full(len(v), -1)
    bajo = v[:, 1] < 1.1 * vb
    dmin = np.full(len(v), np.inf)
    for k, n in enumerate(PATAS):
        eje = m["ejes"][n]
        x = np.interp(v[:, 1], eje[:, 1], eje[:, 0])
        z = np.interp(v[:, 1], eje[:, 1], eje[:, 2])
        dd = np.hypot(v[:, 0] - x, v[:, 2] - z)
        mejor = bajo & (dd < dmin) & (dd < 1.2 * m["radio"])
        pata[mejor], dmin[mejor] = k, dd[mejor]
    cuerpo = [k for k, c in enumerate(claves) if not e.nombres[c][:2] in PATAS]
    cabeza = [k for k, c in enumerate(claves) if e.nombres[c] in ("cuello", "cabeza")]
    de_cabeza = cerca_de_la_cabeza(v, f, e, m)
    joints = np.zeros((len(v), 4), int)
    weights = np.zeros((len(v), 4))
    for i in range(len(v)):
        if pata[i] >= 0:
            n = PATAS[pata[i]]
            permitidos = [k for k, c in enumerate(claves) if e.nombres[c].startswith(n + "_")]
            if v[i, 1] > 0.85 * vb:
                permitidos.append(claves.index(e.i("pecho" if n[0] == "d" else "cadera")))
        elif de_cabeza[i]:
            permitidos = cabeza  # la cabeza con sus cuernos, astas y orejas (aunque pasen sobre el lomo)
        else:
            permitidos = cuerpo
        dd = d[i, permitidos]
        dos = np.argsort(dd)[:2]
        w = 1.0 / (dd[dos] + 0.01 * m["L"]) ** 2
        joints[i, :2] = [claves[permitidos[k]] for k in dos]
        weights[i, :2] = w / w.sum()
    return joints, weights


# ------------------------------------------------------------------ los clips
def marcha(fase, apoyo, A, K):
    """El angulo del brazo (o el muslo) y el de la cana de una pata en su fase: apoyada, de adelante (+A) a atras (-A);
    en el aire, de vuelta adelante con la rodilla doblada (K)."""
    if fase < apoyo:
        return A * (1.0 - 2.0 * fase / apoyo), 0.0
    s = (fase - apoyo) / (1.0 - apoyo)
    return -A + 2.0 * A * (0.5 - 0.5 * np.cos(np.pi * s)), K * np.sin(np.pi * s)


GIRO_Z, GIRO_Y, GIRO_X = (0, 0, 1), (0, 1, 0), (1, 0, 0)


def clips(e, m):
    """Los clips del rig cuadrupedo: {nombre: (tiempos, pose(t))}; pose(t) da {hueso: cuaternion} y la traslacion de
    la raiz."""
    L, W = m["L"], m["W"]
    pata = e.pos[e.i("ti_1")][1]  # el largo de las patas: el ritmo de los pasos
    ritmo = float(np.clip(np.sqrt(pata / 0.9), 0.6, 1.8))
    # pastar: cuanto baja el cuello para que el hocico llegue al suelo (con la cabeza inclinada 25 grados)
    p0, c0, nuca, hoc = (e.pos[e.i(h)] for h in ("pecho", "cuello", "cabeza", "hocico"))
    def altura_hocico(gr):  # el pecho baja una quinta parte; el cuello, el resto; la cabeza, 25 grados mas
        q1 = q_eje(GIRO_Z, 0.2 * gr)
        q2 = qmul(q1, q_eje(GIRO_Z, 0.8 * gr))
        c1 = p0 + qrot(q1, c0 - p0)
        n1 = c1 + qrot(q2, nuca - c0)
        return (n1 + qrot(qmul(q2, q_eje(GIRO_Z, -25)), hoc - nuca))[1]
    bajar = next((g for g in range(0, -86, -2) if altura_hocico(g) < 0.06 * m["A"]), -85)  # los de cuello corto, hasta ahi

    def pasos(t, T, orden, apoyo, A, K, extra=None, agachado=0.0):
        g = {}
        for n, desfase in orden.items():
            a, k = marcha((t / T + desfase) % 1.0, apoyo, A, K)
            g[n + "_1"] = [(GIRO_Z, a)]
            g[n + "_2"] = [(GIRO_Z, -k - agachado)]
            g[n + "_3"] = [(GIRO_Z, 0.5 * k + 0.5 * agachado)]
        for h, giro in (extra or {}).items():
            g.setdefault(h, []).extend(giro)
        return g

    lateral = {"ti": 0.0, "di": 0.25, "td": 0.5, "dd": 0.75}
    diagonal = {"di": 0.0, "td": 0.0, "dd": 0.5, "ti": 0.5}
    galope = {"ti": 0.0, "td": 0.1, "di": 0.5, "dd": 0.6}
    out = {}

    def ciclo(nombre, T, n, fn):
        ts = np.linspace(0.0, T, n + 1)
        out[nombre] = (ts, [fn(t) for t in ts])

    Tc, Tt, Tg = 1.05 * ritmo, 0.62 * ritmo, 0.46 * ritmo
    ciclo("caminar", Tc, 16, lambda t: (pasos(t, Tc, lateral, 0.65, 16, 35, {"cuello": [(GIRO_Z, 3 * np.sin(4 * np.pi * t / Tc))]}),
                                         (0, -0.012 * pata * np.cos(4 * np.pi * t / Tc), 0)))
    ciclo("trotar", Tt, 12, lambda t: (pasos(t, Tt, diagonal, 0.5, 24, 55, {"cuello": [(GIRO_Z, 4 * np.sin(4 * np.pi * t / Tt))]}),
                                        (0, 0.03 * pata * np.abs(np.sin(2 * np.pi * t / Tt)), 0)))
    ciclo("galopar", Tg, 12, lambda t: (pasos(t, Tg, galope, 0.4, 34, 75, {
        "lomo": [(GIRO_Z, 6 * np.sin(2 * np.pi * t / Tg))], "cuello": [(GIRO_Z, -8 * np.sin(2 * np.pi * t / Tg))]}),
        (0, 0.05 * pata * np.sin(2 * np.pi * t / Tg), 0)))
    Ta = 1.6 * Tc
    ciclo("acechar", Ta, 16, lambda t: (pasos(t, Ta, lateral, 0.7, 11, 30, {"cuello": [(GIRO_Z, -12)], "cabeza": [(GIRO_Z, -5)]},
                                              agachado=22), (0, -0.14 * pata, 0)))
    ciclo("idle", 3.2, 16, lambda t: ({"cuello": [(GIRO_Y, 5 * np.sin(2 * np.pi * t / 3.2))],
                                        "cabeza": [(GIRO_Z, 3 * np.sin(4 * np.pi * t / 3.2))],
                                        "cola": [(GIRO_Y, 12 * np.sin(4 * np.pi * t / 3.2))],
                                        "lomo": [(GIRO_Z, 0.8 * np.sin(4 * np.pi * t / 3.2))]}, (0, 0, 0)))
    ciclo("montado_idle", 3.2, 8, lambda t: ({"cabeza": [(GIRO_Z, 2 * np.sin(2 * np.pi * t / 3.2))],
                                               "cola": [(GIRO_Y, 10 * np.sin(4 * np.pi * t / 3.2))]}, (0, 0, 0)))
    ciclo("pastar", 2.4, 12, lambda t: ({"pecho": [(GIRO_Z, 0.2 * bajar)], "cuello": [(GIRO_Z, 0.8 * bajar)],
                                          "cabeza": [(GIRO_Z, -25 + 4 * np.sin(8 * np.pi * t / 2.4))],
                                          "cola": [(GIRO_Y, 12 * np.sin(2 * np.pi * t / 2.4))]}, (0, 0, 0)))
    ciclo("comer_presa", 1.2, 12, lambda t: ({"pecho": [(GIRO_Z, 0.18 * bajar)],
                                               "cuello": [(GIRO_Z, 0.72 * bajar + 10 * max(0.0, np.sin(2 * np.pi * t / 1.2)))],
                                               "cabeza": [(GIRO_Z, -25), (GIRO_Y, 15 * np.sin(4 * np.pi * t / 1.2))],
                                               "di_1": [(GIRO_Z, 8)], "dd_1": [(GIRO_Z, 8)]}, (0, -0.04 * pata, 0)))
    ciclo("forcejear_lazo", 1.0, 12, lambda t: ({"cadera": [(GIRO_Z, -6)], "ti_1": [(GIRO_Z, -6)], "td_1": [(GIRO_Z, -6)],
                                                  "di_1": [(GIRO_Z, 26)], "dd_1": [(GIRO_Z, 26)],
                                                  "cuello": [(GIRO_Z, 6 * np.sin(4 * np.pi * t))],
                                                  "cabeza": [(GIRO_Y, 25 * np.sin(2 * np.pi * t))]},
                                                 (-0.04 * L * (0.6 + 0.4 * np.sin(2 * np.pi * t)), 0, 0)))

    def claves(nombre, marcas):
        """Un clip de una vez: poses en ciertos tiempos (las que faltan, la de reposo)."""
        ts = np.array([t for t, _, _ in marcas], float)
        out[nombre] = (ts, [(g, tr) for _, g, tr in marcas])

    quieto = ({}, (0, 0, 0))
    claves("atacar", [(0.0, *quieto),
                      (0.2, {"cuello": [(GIRO_Z, -10)], "di_1": [(GIRO_Z, -10)], "dd_1": [(GIRO_Z, -10)]}, (-0.06 * L, -0.06 * pata, 0)),
                      (0.4, {"cuello": [(GIRO_Z, 5)], "cabeza": [(GIRO_Z, -25)], "di_1": [(GIRO_Z, 30)], "dd_1": [(GIRO_Z, 30)],
                             "di_2": [(GIRO_Z, -30)], "dd_2": [(GIRO_Z, -30)], "ti_1": [(GIRO_Z, -20)], "td_1": [(GIRO_Z, -20)]},
                       (0.12 * L, 0, 0)),
                      (0.8, *quieto)])
    claves("encabritarse", [(0.0, *quieto),
                            (0.4, {"cadera": [(GIRO_Z, 32)], "ti_1": [(GIRO_Z, -32)], "td_1": [(GIRO_Z, -32)],
                                   "di_1": [(GIRO_Z, 55)], "dd_1": [(GIRO_Z, 45)], "di_2": [(GIRO_Z, -100)], "dd_2": [(GIRO_Z, -90)],
                                   "cuello": [(GIRO_Z, 10)]}, (0, 0, 0)),
                            (0.8, {"cadera": [(GIRO_Z, 34)], "ti_1": [(GIRO_Z, -34)], "td_1": [(GIRO_Z, -34)],
                                   "di_1": [(GIRO_Z, 40)], "dd_1": [(GIRO_Z, 55)], "di_2": [(GIRO_Z, -60)], "dd_2": [(GIRO_Z, -100)],
                                   "cuello": [(GIRO_Z, 14)]}, (0, 0, 0)),
                            (1.4, *quieto)])
    claves("saltar", [(0.0, *quieto),
                      (0.2, {n + "_2": [(GIRO_Z, -25)] for n in PATAS}, (0, -0.12 * pata, 0)),
                      (0.45, {"di_1": [(GIRO_Z, 45)], "dd_1": [(GIRO_Z, 45)], "di_2": [(GIRO_Z, -80)], "dd_2": [(GIRO_Z, -80)],
                              "ti_1": [(GIRO_Z, -35)], "td_1": [(GIRO_Z, -35)], "ti_2": [(GIRO_Z, -40)], "td_2": [(GIRO_Z, -40)]},
                       (0, 0.3 * pata, 0)),
                      (0.7, {n + "_2": [(GIRO_Z, -15)] for n in PATAS}, (0, -0.06 * pata, 0)),
                      (0.9, *quieto)])
    claves("aullar", [(0.0, *quieto), (0.4, {"cuello": [(GIRO_Z, 40)], "cabeza": [(GIRO_Z, 20)]}, (0, 0, 0)),
                      (1.6, {"cuello": [(GIRO_Z, 42)], "cabeza": [(GIRO_Z, 22)]}, (0, 0, 0)), (2.0, *quieto)])
    claves("recibir_golpe", [(0.0, *quieto),
                             (0.12, {"cadera": [(GIRO_X, 6)], "lomo": [(GIRO_Z, 4)], "cuello": [(GIRO_Z, 12)]}, (-0.04 * L, 0, 0)),
                             (0.45, *quieto)])
    caido = {"raiz": [(GIRO_X, 90)], **{n + "_1": [(GIRO_Z, 10 if n[0] == "d" else -10)] for n in PATAS},
             **{n + "_2": [(GIRO_Z, -20)] for n in PATAS}, "cuello": [(GIRO_Z, -15)]}
    claves("morir", [(0.0, *quieto),
                     (0.3, {n + "_2": [(GIRO_Z, -40)] for n in PATAS}, (0, -0.15 * pata, 0)),
                     (0.9, caido, (0, 0.5 * W, 0)), (1.2, caido, (0, 0.5 * W, 0))])
    return out


def rotaciones(e, giros):
    """El cuaternion local de cada hueso para una pose ({hueso: [(eje, grados), ...]})."""
    R = np.tile([0.0, 0.0, 0.0, 1.0], (len(e.nombres), 1))
    for h, lista in giros.items():
        q = np.array([0.0, 0.0, 0.0, 1.0])
        for eje, gr in lista:
            q = qmul(q, q_eje(eje, gr))
        R[e.i(h)] = q
    return R


# ------------------------------------------------------------------ el GLB
def escribir(e, pos, nor, col, joints, weights, animaciones, nombre):
    buf = bytearray()
    views, accs = [], []

    def vista(b, target=None):
        while len(buf) % 4:
            buf.append(0)
        views.append({"buffer": 0, "byteOffset": len(buf), "byteLength": len(b), **({"target": target} if target else {})})
        buf.extend(b)
        return len(views) - 1

    def accesor(a, ctype, tipo, target=None, normalized=False, minmax=False):
        a = np.ascontiguousarray(a)
        d = {"bufferView": vista(a.tobytes(), target), "componentType": ctype, "count": len(a), "type": tipo}
        if normalized:
            d["normalized"] = True
        if minmax:
            d["min"] = np.atleast_1d(a.min(axis=0)).tolist()
            d["max"] = np.atleast_1d(a.max(axis=0)).tolist()
        accs.append(d)
        return len(accs) - 1

    attrs = {"POSITION": accesor(pos.astype(np.float32), 5126, "VEC3", 34962, minmax=True),
             "NORMAL": accesor(nor.astype(np.float32), 5126, "VEC3", 34962),
             "COLOR_0": accesor(np.column_stack([col, np.full(len(col), 255)]).astype(np.uint8), 5121, "VEC4", 34962, normalized=True),
             "JOINTS_0": accesor(joints.astype(np.uint8), 5121, "VEC4", 34962),
             "WEIGHTS_0": accesor(weights.astype(np.float32), 5126, "VEC4", 34962)}
    nodes = []
    for k, n in enumerate(e.nombres):
        p = e.padres[k]
        local = e.pos[k] - (e.pos[p] if p >= 0 else 0.0)
        nd = {"name": n, "translation": [float(x) for x in local]}
        hijos = [c for c in range(len(e.nombres)) if e.padres[c] == k]
        if hijos:
            nd["children"] = hijos
        nodes.append(nd)
    nodes.append({"name": "cuerpo", "mesh": 0, "skin": 0})
    ibm = np.stack([np.eye(4) for _ in e.nombres])
    for k in range(len(e.nombres)):
        ibm[k, :3, 3] = -e.pos[k]
    ibm_acc = accesor(np.stack([m.T.reshape(-1) for m in ibm]).astype(np.float32), 5126, "MAT4")
    anims = []
    for clip, (ts, poses) in animaciones.items():
        entrada = accesor(ts.astype(np.float32).reshape(-1, 1), 5126, "SCALAR", minmax=True)
        R = np.stack([rotaciones(e, g) for g, _ in poses])  # cuadros x huesos x 4
        for c in range(1, len(R)):  # el camino corto de un cuadro al siguiente
            flip = np.sum(R[c] * R[c - 1], axis=1) < 0
            R[c][flip] *= -1
        samplers, channels = [], []
        for k in range(len(e.nombres)):
            samplers.append({"input": entrada, "output": accesor(R[:, k].astype(np.float32), 5126, "VEC4"), "interpolation": "LINEAR"})
            channels.append({"sampler": len(samplers) - 1, "target": {"node": k, "path": "rotation"}})
        T = np.array([tr for _, tr in poses], float)
        samplers.append({"input": entrada, "output": accesor(T.astype(np.float32), 5126, "VEC3"), "interpolation": "LINEAR"})
        channels.append({"sampler": len(samplers) - 1, "target": {"node": 0, "path": "translation"}})
        anims.append({"name": clip, "samplers": samplers, "channels": channels})
    doc = {"asset": {"version": "2.0", "generator": "tools/modelos/animales.py"},
           "scene": 0, "scenes": [{"nodes": [0, len(nodes) - 1]}], "nodes": nodes,
           "meshes": [{"name": nombre, "primitives": [{"attributes": attrs, "mode": 4}]}],
           "skins": [{"joints": list(range(len(e.nombres))), "skeleton": 0, "inverseBindMatrices": ibm_acc}],
           "animations": anims, "accessors": accs, "bufferViews": views, "buffers": [{"byteLength": len(buf)}]}
    js = json.dumps(doc, separators=(",", ":")).encode()
    js += b" " * (-len(js) % 4)
    while len(buf) % 4:
        buf.append(0)
    total = 12 + 8 + len(js) + 8 + len(buf)
    return struct.pack("<III", 0x46546C67, 2, total) + struct.pack("<II", len(js), 0x4E4F534A) + js + \
        struct.pack("<II", len(buf), 0x004E4942) + bytes(buf)


# ------------------------------------------------------------------ armar un id
def preparar(ruta, color, imagenes, log=print):
    """La malla limpia, con su color, de frente a +X y sin escala (la del inventario va en armar)."""
    v, f, col = cargar(ruta)
    v, f, col, sueltas = limpiar(v, f, col)
    if color == "textura":
        if col is None:
            raise SystemExit(f"{ruta}: no trae textura")
    elif isinstance(color, tuple) and isinstance(color[0], str):
        col = color_de_imagen(v, imagen_de(imagenes, *color))
    else:
        col = color_liso(v, color)
    v = orientar(suavizar(v, f), color == "textura")
    log(f"  {os.path.basename(ruta)}: {len(f)} triangulos, {sueltas} piezas sueltas fuera")
    return v, f, col


def armar(malla_t, it, log=print):
    _, alto, largo = (float(x) for x in it["medidas_m"].split("x"))  # el ancho sale de la escala
    tmax = int(it["tris_max"]) or 800
    v, f, col = malla_t
    caja = v.max(0) - v.min(0)
    s = float(np.sqrt(largo / caja[0] * alto / caja[1]))
    v = v * s
    v = v - [0.0, v[:, 1].min(), 0.5 * (v[:, 2].min() + v[:, 2].max())]
    P = muestras(v, f)
    m = medir(P)
    # El medio, entre las patas delanteras y las traseras (no el de la caja: la cola larga del leopardo lo corre).
    dx = 0.5 * (m["xd"] + m["xt"])
    v, P = v - [dx, 0, 0], P - [dx, 0, 0]
    m = medir(P)
    e = armar_esqueleto(P, m)
    vr, fr, cr = reducir(v, f, col, tmax)
    joints, weights = pesos(vr, fr, e, m)
    # Solo los clips que le tocan en assets/animaciones.tsv (acechar y comer_presa, los cazadores; pastar, los que
    # pastan; montado_idle, las monturas...).
    filas, _ = indice.load_clips()
    suyos = {c["clip"] for c in filas if c["rig"] == "cuadrupedo" and any(indice.matches(it, x) for x in c["filtros"])}
    animaciones = {k: c for k, c in clips(e, m).items() if k in suyos}
    # Cara por cara: el color medio y la luz del sol horneada (como humanoide.py).
    tri = vr[fr]
    n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    sol = np.array(lowres.SUN, float)
    sol /= np.linalg.norm(sol)
    luz = 0.72 + 0.38 * np.clip(n @ sol, 0.0, 1.0)
    caras = np.clip(cr[fr].mean(axis=1) * luz[:, None], 0, 255)
    datos = escribir(e, tri.reshape(-1, 3), np.repeat(n, 3, axis=0), np.repeat(caras, 3, axis=0),
                     joints[fr].reshape(-1, 4), weights[fr].reshape(-1, 4), animaciones, it["id"])
    armar.ultimo = dict(e=e, m=m, v=vr, f=fr, joints=joints, weights=weights)
    c = vr.max(0) - vr.min(0)
    log(f"{it['id']}: {len(fr)} triangulos, {c[0]:.2f} x {c[1]:.2f} x {c[2]:.2f} m (largo x alto x ancho), "
        f"vientre a {m['vientre']:.2f} m, {len(animaciones)} clips")
    return datos, len(fr)


def main():
    if len(sys.argv) < 3:
        raise SystemExit(__doc__)
    carpeta, imagenes = sys.argv[1], sys.argv[2]
    numeros = [int(x) for x in sys.argv[3:]] or sorted(FICHA)
    items, errores = inventario.load()
    if errores:
        raise SystemExit("\n".join(errores))
    filas = open(inventario.TSV, encoding="utf-8").read().split("\n")
    ids = {f.split("\t")[0]: k for k, f in enumerate(filas) if f and not f.startswith("#")}
    for nuevo, base in VARIANTES.items():  # la fila de la variante, como la de su especie
        if nuevo not in ids:
            c = filas[ids[base]].split("\t")
            c[0], c[1] = nuevo, c[1] + " (variante 2)"
            filas.insert(ids[base] + 1, "\t".join(c))
            ids = {f.split("\t")[0]: k for k, f in enumerate(filas) if f and not f.startswith("#")}
    open(inventario.TSV, "w", encoding="utf-8").write("\n".join(filas))
    items, _ = inventario.load()
    por_id = {it["id"]: it for it in items}
    for n in numeros:
        archivo, mids, color = FICHA[n]
        malla_t = preparar(os.path.join(carpeta, archivo), color, imagenes)
        for mid in mids:
            datos, _ = armar(malla_t, por_id[mid])
            ruta = os.path.join(ROOT, inventario.path_of(por_id[mid]))
            os.makedirs(os.path.dirname(ruta), exist_ok=True)
            open(ruta, "wb").write(datos)
            c = filas[ids[mid]].split("\t")
            c[5] = "importado"
            nota = NOTA.format(n=n)
            c[6] = (VIEJA.sub("", c[6]).strip() + " " + nota).strip() if nota not in c[6] else c[6]
            filas[ids[mid]] = "\t".join(c)
    open(inventario.TSV, "w", encoding="utf-8").write("\n".join(filas))
    print("Sigue: python3 tools/assets/inventario.py doc && python3 tools/assets/animaciones.py doc")


if __name__ == "__main__":
    main()
