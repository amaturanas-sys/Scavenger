#!/usr/bin/env python3
"""El equipo de los humanoides desde el patron armado a mano: arte/fuentes/patron/model_equipped_01.glb.

  python3 tools/modelos/patron.py medir              lo que se lee del patron: cada pieza, su malla de Props, la
                                                     transformacion exacta, la holgura a la piel y su hueso
  python3 tools/modelos/patron.py hacer [id ...]     el equipo (todo o los ids dados), src/world/anclajes.inc y el
                                                     inventario
  python3 tools/modelos/patron.py ver [id ...]       vistas de control: el cuerpo en reposo con lo hecho puesto
                                                     (build/patron/*.png)
  python3 tools/modelos/patron.py cascos             cada casco en cada cuerpo, en reposo: la holgura a la piel, los
                                                     vertices del craneo a menos de 2 mm (tienen que ser 0), el techo
                                                     sobre la coronilla y el borde sobre las cejas

El patron es el cuerpo de arte/fuentes/humanoide/rigged-model.glb sin tocar, en su pose de piel (la T), con el casco,
el chaleco, las botas, el escudo, la aljaba, el arco, el sable y la daga puestos, y la antorcha, la lanza y la
alabarda de pie al lado. Cada pieza es una malla de arte/fuentes/props/modelos con una transformacion afin exacta
(error 0 mm): solo se escalo (a veces distinto en cada eje), se giro y se coloco. De ahi sale todo:

- Las prendas con piel (corazas y botas): la malla de Props puesta donde la puso el patron (las que no estan en el,
  con el mismo criterio: la holgura del chaleco), sin las caras de adentro (las que miran al cuerpo: no se ven),
  reducida a su tris_max, con los pesos de la piel de abajo y llevada de la T al reposo con los mismos huesos que el
  cuerpo (humanoide.armar). Un GLB por cuerpo (nombre.glb, el comun; nombre@mujer, @gigante, @protagonista), con el
  esqueleto del cuerpo y sin clips: el juego la mueve con la animacion del cuerpo.
- Lo rigido (cascos, armas, escudos, la aljaba, la antorcha): un GLB en su marco (las armas, la empunadura en el
  origen, la hoja hacia +Y y el filo hacia +Z; los escudos mirando a +X, el origen en el centro de atras; lo demas,
  en el centro de la base) y, en src/world/anclajes.inc, donde va en cada cuerpo: el hueso y la matriz del patron
  (la espalda, la cadera) o de la mano (empunado).
- Los cascos, por su cupula sobre el craneo (calzar_casco): de frente a +X por su simetria, derechos, el borde de
  adelante sobre las cejas, el centro de la cupula sobre el del craneo y la escala menor con 2 mm de la piel en todo
  el casco; el del patron, donde lo puso el usuario, agrandado lo justo. Su marco: el origen en el centro del craneo
  del cuerpo comun (a la altura de las cejas), +X adelante; GEAR_HEAD lleva ese punto al de cada cuerpo, con la
  escala con la que ningun casco toca su cabeza.
- La reduccion: cuadricas (fast_simplification) hasta el tris_max, sin centrar ni escalar a ciegas: la escala y el
  lugar son los del patron. Las normales, suaves entre caras de menos de 40 grados y cortadas en las aristas mas
  duras. La textura: el render de Props proyectado de frente (<= 256 px, filtro de punto), con su UV.
"""
import io
import itertools
import json
import os
import struct
import sys

import numpy as np
from PIL import Image
from scipy import ndimage
from scipy.spatial import cKDTree

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "assets"))
import inventario  # noqa: E402

ROOT = inventario.ROOT
PATRON = os.path.join(ROOT, "arte", "fuentes", "patron", "model_equipped_01.glb")
PROPS = os.path.join(ROOT, "arte", "fuentes", "props")
ESQUELETO = os.path.join(ROOT, "arte", "fuentes", "humanoide", "exported-model.glb")
RIGGED = os.path.join(ROOT, "arte", "fuentes", "humanoide", "rigged-model.glb")
MANIQUI_F = os.path.join(ROOT, "arte", "fuentes", "animaciones", "UAL2", "Mannequin_F.glb")
ANCLAJES = os.path.join(ROOT, "src", "world", "anclajes.inc")
VISTAS = os.path.join(ROOT, "build", "patron")
# Los cuerpos: el sufijo de su variante y su id (el orden, el de GEAR_BODIES en src/world/props.c).
CUERPOS = (("", "personaje.base.cuerpo_comun"), ("mujer", "personaje.base.cuerpo_mujer"),
           ("gigante", "personaje.base.cuerpo_gigante"), ("protagonista", "personaje.narrativo.protagonista"))

# Las mallas del patron: de que malla de Props sale cada una (las botas, una por pieza suelta de botas_montar).
PIEZAS = {"model": "casco_aguila", "white_mesh(3)": "chaleco_largo", "white_mesh(18) 1": "botas_montar",
          "white_mesh(18) 2": "botas_montar", "white_mesh(28)": "aljaba", "white_mesh(27)": "arco",
          "hy3d-model(3)": "escudo_redondo_3", "Curved sword": "sable", "white_mesh(25)": "espada",
          "white_mesh(20)": "antorcha", "white_mesh(24)": "lanza", "white_mesh(26)": "alabarda"}
CUERPO_PATRON = "rigged-model"


def log(msg):
    print(msg, flush=True)


# ---------------------------------------------------------------- GLB
def leer(path):
    d = open(path, "rb").read()
    n = struct.unpack("<I", d[12:16])[0]
    doc = json.loads(d[20:20 + n])
    off = 20 + n
    bl = struct.unpack("<I", d[off:off + 4])[0]
    return doc, d[off + 8:off + 8 + bl]


def acc(doc, b, i):
    a = doc["accessors"][i]
    bv = doc["bufferViews"][a["bufferView"]]
    dt = {5126: "<f4", 5125: "<u4", 5123: "<u2", 5121: "u1"}[a["componentType"]]
    nc = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[a["type"]]
    o = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
    sz = np.dtype(dt).itemsize * nc
    st = bv.get("byteStride", sz)
    raw = np.frombuffer(b, np.uint8, count=st * (a["count"] - 1) + sz, offset=o)
    out = np.lib.stride_tricks.as_strided(raw, (a["count"], sz), (st, 1)).copy().view(dt).reshape(a["count"], nc)
    if a.get("normalized") and dt != "<f4":
        out = out.astype(float) / float(np.iinfo(np.dtype(dt)).max)
    return out


def _local(nd):
    from scipy.spatial.transform import Rotation
    if "matrix" in nd:
        return np.array(nd["matrix"], float).reshape(4, 4).T
    M = np.eye(4)
    M[:3, :3] = Rotation.from_quat(nd.get("rotation", [0, 0, 0, 1])).as_matrix() * np.array(nd.get("scale", [1, 1, 1]))
    M[:3, 3] = nd.get("translation", [0, 0, 0])
    return M


def mundo_nodos(doc):
    """La matriz de mundo (4x4) de cada nodo."""
    nodes = doc["nodes"]
    par = {c: i for i, nd in enumerate(nodes) for c in nd.get("children", [])}
    W = [None] * len(nodes)
    def w(i):
        if W[i] is None:
            W[i] = (w(par[i]) if i in par else np.eye(4)) @ _local(nodes[i])
        return W[i]
    return [w(i) for i in range(len(nodes))]


def mallas(path):
    """Las primitivas de un GLB, con las transformaciones de sus nodos: [(nombre, vertices, caras)], los vertices en
    el orden del archivo (las piezas del patron se emparejan con su malla de Props por ese orden)."""
    doc, b = leer(path)
    W = mundo_nodos(doc)
    out = []
    for i, nd in enumerate(doc["nodes"]):
        if "mesh" not in nd:
            continue
        m = doc["meshes"][nd["mesh"]]
        for pr in m["primitives"]:
            v = acc(doc, b, pr["attributes"]["POSITION"]).astype(float)
            if "skin" not in nd:  # una malla con piel no lleva la matriz de su nodo (glTF)
                v = v @ W[i][:3, :3].T + W[i][:3, 3]
            f = acc(doc, b, pr["indices"]).astype(np.int64).reshape(-1, 3) if "indices" in pr else \
                np.arange(len(v)).reshape(-1, 3)
            out.append((m.get("name"), v, f))
    return out


def malla_props(nombre):
    """Una malla de arte/fuentes/props/modelos: todas sus primitivas juntas, en el orden del archivo."""
    partes = mallas(os.path.join(PROPS, "modelos", nombre + ".glb"))
    v, f, base = [], [], 0
    for _, pv, pf in partes:
        v.append(pv)
        f.append(pf + base)
        base += len(pv)
    return np.concatenate(v), np.concatenate(f)


def soldar(v, f):
    """Los vertices repetidos (costuras de la UV o de las normales), uno solo; sin las caras que quedan sin area."""
    escala = max(float(np.ptp(v, axis=0).max()), 1e-9) * 1e-6
    _, idx, inv = np.unique(np.round(v / escala).astype(np.int64), axis=0, return_index=True, return_inverse=True)
    g = inv.reshape(-1)[f]
    g = g[(g[:, 0] != g[:, 1]) & (g[:, 1] != g[:, 2]) & (g[:, 0] != g[:, 2])]
    return v[idx], g


def piezas_conexas(v, f):
    """La pieza conexa de cada vertice (soldando por posicion)."""
    from scipy.sparse import coo_matrix
    from scipy.sparse.csgraph import connected_components
    escala = max(float(np.ptp(v, axis=0).max()), 1e-9) * 1e-6
    _, inv = np.unique(np.round(v / escala).astype(np.int64), axis=0, return_inverse=True)
    inv = inv.reshape(-1)
    e = inv[np.concatenate([f[:, [0, 1]], f[:, [1, 2]], f[:, [2, 0]]])]
    n = inv.max() + 1
    _, lab = connected_components(coo_matrix((np.ones(len(e)), (e[:, 0], e[:, 1])), shape=(n, n)), directed=False)
    return lab[inv]


# ---------------------------------------------------------------- transformaciones
def afin(a, b):
    """La afin (4x4) que lleva los puntos a a los b (emparejados), por cuadrados minimos."""
    A = np.column_stack([a, np.ones(len(a))])
    X, *_ = np.linalg.lstsq(A, b, rcond=None)
    M = np.eye(4)
    M[:3, :3] = X[:3].T
    M[:3, 3] = X[3]
    return M


def aplicar(M, p):
    return p @ M[:3, :3].T + M[:3, 3]


def icp_afin(a, b, vueltas=50):
    """La afin de a a b sin saber que punto va con cual (las piezas que no traen los vertices en el mismo orden):
    arranca de los ejes principales de las dos (con los ocho juegos de signos) y empareja por cercania."""
    tb = cKDTree(b)
    ma, mb = a.mean(0), b.mean(0)
    ea, va = np.linalg.eigh(np.cov((a - ma).T))
    eb, vb = np.linalg.eigh(np.cov((b - mb).T))
    mejor = None
    for sg in itertools.product((1, -1), repeat=3):
        L = (vb * np.array(sg)) @ np.diag(np.sqrt(np.maximum(eb, 1e-12) / np.maximum(ea, 1e-12))) @ va.T
        M = np.eye(4)
        M[:3, :3], M[:3, 3] = L, mb - L @ ma
        for _ in range(vueltas):
            M = afin(a, b[tb.query(aplicar(M, a))[1]])
        d = tb.query(aplicar(M, a))[0]
        if mejor is None or np.median(d) < mejor[0]:
            mejor = (float(np.median(d)), M)
    return mejor[1]


def umeyama(a, b):
    """La semejanza (escala, giro y traslacion, 4x4) de a a b, emparejados."""
    ma, mb = a.mean(0), b.mean(0)
    A, B = a - ma, b - mb
    U, S, Vt = np.linalg.svd(B.T @ A / len(a))
    D = np.eye(3)
    if np.linalg.det(U @ Vt) < 0:
        D[2, 2] = -1
    R = U @ D @ Vt
    s = np.trace(np.diag(S) @ D) / (A ** 2).sum(1).mean()
    M = np.eye(4)
    M[:3, :3], M[:3, 3] = s * R, mb - s * R @ ma
    return M


def icp_semejanza(a, b, muestras=20000, vueltas=40, semilla=1):
    """La semejanza de a a b, dos nubes del mismo cuerpo sin el mismo orden: prueba los 24 giros de ejes desde los
    centros, a la escala del radio, y se queda con el que mejor calza."""
    rng = np.random.default_rng(semilla)
    sa = a[rng.choice(len(a), min(muestras, len(a)), replace=False)]
    tb = cKDTree(b)
    ma, mb = a.mean(0), b.mean(0)
    k = np.sqrt(((b - mb) ** 2).sum(1).mean() / ((a - ma) ** 2).sum(1).mean())
    giros = []
    for perm in itertools.permutations(range(3)):
        for sg in itertools.product((1, -1), repeat=3):
            R = np.zeros((3, 3))
            R[range(3), perm] = sg
            if np.linalg.det(R) > 0:
                giros.append(R)
    giros.sort(key=lambda R: -np.trace(R))  # sin giro primero: el patron y el esqueleto miran al mismo lado
    mejor = None
    radio = np.sqrt(((b - mb) ** 2).sum(1).mean())
    for R in giros:
        M = np.eye(4)
        M[:3, :3], M[:3, 3] = k * R, mb - k * R @ ma
        for _ in range(8):
            M = umeyama(sa, b[tb.query(aplicar(M, sa))[1]])
        e = float(np.median(tb.query(aplicar(M, sa))[0]))
        if mejor is None or e < mejor[0]:
            mejor = (e, M)
        if e < 5e-3 * radio:  # ya calza (lo afina el final): los demas giros sobran
            break
    M = mejor[1]
    for _ in range(vueltas):
        M = umeyama(sa, b[tb.query(aplicar(M, sa))[1]])
    return M


def rigida(M):
    """La parte rigida de una afin (polar): el giro sin la escala, y la traslacion."""
    U, _, Vt = np.linalg.svd(M[:3, :3])
    R = U @ Vt
    if np.linalg.det(R) < 0:
        U[:, -1] *= -1
        R = U @ Vt
    out = np.eye(4)
    out[:3, :3], out[:3, 3] = R, M[:3, 3]
    return out


# ---------------------------------------------------------------- el patron
class Patron:
    """Las piezas del patron (en sus unidades: 10 de alto, el frente a +Z) y la transformacion exacta de cada malla de
    Props a su lugar en el."""

    def __init__(self):
        self.mallas = {nombre: (v, f) for nombre, v, f in mallas(PATRON)}
        self.M = {}
        self.error = {}
        for nombre, fuente in PIEZAS.items():
            g = self.mallas[nombre][0]
            o, _ = malla_props(fuente)
            if fuente == "botas_montar":  # dos piezas sueltas: la de este pie
                lab = piezas_conexas(*malla_props(fuente))
                lados = [o[lab == k] for k in np.unique(lab)]
                o = next(x for x in lados if len(x) == len(g))
            M = afin(o, g) if len(o) == len(g) else None
            if M is None or np.abs(aplicar(M, o) - g).max() > 1e-4 * np.ptp(g, axis=0).max():
                M = icp_afin(o, g)
            self.M[nombre] = M
            self.error[nombre] = cKDTree(g).query(aplicar(M, o))[0]

    def cuerpo(self):
        """Los vertices de la piel del cuerpo del patron: la capa de afuera de rigged-model.glb (una cascara de unos
        4 mm; humanoide.malla se queda con la misma)."""
        if not hasattr(self, "_cuerpo"):
            import lowres
            v, f = self.mallas[CUERPO_PATRON]
            self._cuerpo = v[np.unique(f[lowres.capa_de_afuera(v, f)])]
        return self._cuerpo

    def al_esqueleto(self, comun):
        """La semejanza (4x4) del patron a las medidas del esqueleto del cuerpo comun: el cuerpo del patron es la malla
        de rigged-model.glb, la misma que arma humanoide.py (sin el mismo orden ni los vertices repetidos)."""
        if not hasattr(self, "_S"):
            a, b = self.cuerpo(), comun.u["m"]
            S = icp_semejanza(a, b)
            # La capa de afuera de cada una sale de sus voxeles: unos pocos vertices de la de adentro (a los 4 mm de la
            # cascara) quedan de un lado y no del otro. Sin ellos (a mas de 1 mm), la semejanza con los emparejados.
            arbol = cKDTree(b)
            d, j = arbol.query(aplicar(S, a))
            par = d < 1e-3 / comun.u["s"]
            for _ in range(3):
                S = umeyama(a[par], b[j[par]])
                d, j = arbol.query(aplicar(S, a))
            self._S, self.error_cuerpo = S, d[par]
            self.sueltos = int((~par).sum())
        return self._S

    def en_t(self, nombre, comun):
        """La transformacion (4x4) de la malla de Props de la pieza a su lugar sobre el cuerpo comun en la pose T (en
        las medidas del esqueleto: humanoide.armar)."""
        return self.al_esqueleto(comun) @ self.M[nombre]


# ---------------------------------------------------------------- los cuerpos
class Cuerpo:
    """Un cuerpo base armado como lo arma humanoide.py (sin escribirlo): su malla entera en la pose T (en las medidas
    del esqueleto) con sus pesos, el paso de la T al reposo de su GLB y su esqueleto."""

    def __init__(self, sufijo, mid, u, juego):
        self.sufijo, self.id, self.u = sufijo, mid, u
        self.juego = juego  # (vertices, caras) del GLB del juego, en reposo: lo que se dibuja

    def a_reposo(self, p, joints, weights):
        """Puntos de la T al reposo (el GLB del juego): con sus huesos y pesos, como el cuerpo."""
        vh = np.column_stack([p, np.ones(len(p))])
        out = np.zeros_like(p)
        for k in range(joints.shape[1]):
            mats = np.stack([self.u["skin"][j] for j in joints[:, k]])
            out += weights[:, k:k + 1] * np.einsum("nij,nj->ni", mats, vh)[:, :3]
        return out * self.u["s"]

    def hueso_reposo(self, nombre):
        """La matriz del hueso de la T al reposo (escalada como el cuerpo): lo rigido que va pegado a el."""
        i = self.u["names"].index(nombre)
        M = self.u["skin"][i].copy()
        M[:3, :] *= self.u["s"]
        return M


def armar_cuerpos(log=log):
    import humanoide
    esq = humanoide.Esqueleto(ESQUELETO)
    humanoide.sintetizar(esq)
    items, errores = inventario.load()
    if errores:
        raise SystemExit("\n".join(errores))
    por_id = {it["id"]: it for it in items}
    fuentes = {RIGGED: humanoide.malla(RIGGED), MANIQUI_F: humanoide.malla(MANIQUI_F)}
    out = {}
    for sufijo, mid in CUERPOS:
        src = MANIQUI_F if sufijo == "mujer" else RIGGED
        log(f"[patron] {mid}: armando el cuerpo como humanoide.py")
        humanoide.armar(esq, fuentes[src], por_id[mid], log=lambda *_: None)
        u = dict(humanoide.armar.ultimo)
        u["names"] = list(esq.names)
        u["nodos"] = esq.nodes
        doc, b = leer(os.path.join(ROOT, inventario.path_of(por_id[mid])))
        v = acc(doc, b, doc["meshes"][0]["primitives"][0]["attributes"]["POSITION"]).astype(float)
        out[sufijo] = Cuerpo(sufijo, mid, u, (v, np.arange(len(v)).reshape(-1, 3)))
        out[sufijo].fuente = fuentes[src]
        # El esqueleto armado es el del GLB del juego: si no, la prenda no seguiria al cuerpo.
        nodos = [n for n in doc["nodes"] if "mesh" not in n]
        lejos = max(np.abs(np.array(n["translation"]) - u["TI"][i]).max() for i, n in enumerate(nodos))
        if lejos > 1e-4:
            raise SystemExit(f"{mid}: el esqueleto armado no es el del GLB del juego ({lejos * 1000:.2f} mm): "
                             "vuelve a armar los cuerpos con humanoide.py")
    return out


# ---------------------------------------------------------------- geometria
def normales_caras(v, f):
    n = np.cross(v[f[:, 1]] - v[f[:, 0]], v[f[:, 2]] - v[f[:, 0]])
    a = np.linalg.norm(n, axis=1, keepdims=True)
    return n / np.maximum(a, 1e-12), a[:, 0] / 2


class Superficie:
    """Puntos de una malla (repartidos por area) con su normal: la distancia con signo de cualquier punto a ella
    (positiva afuera), por el punto mas cercano."""

    def __init__(self, v, f, n=80000, semilla=7):
        v, f = compactar(v, f)  # solo los vertices de estas caras
        fn, area = normales_caras(v, f)
        rng = np.random.default_rng(semilla)
        cara = rng.choice(len(f), n, p=area / area.sum())
        r1, r2 = np.sqrt(rng.random(n)), rng.random(n)
        b = np.column_stack([1 - r1, r1 * (1 - r2), r1 * r2])
        self.p = np.concatenate([(v[f[cara]] * b[:, :, None]).sum(1), v])
        # la normal de cada vertice: la de sus caras (para que los vertices tambien sirvan de muestra)
        vn = np.zeros_like(v)
        for k in range(3):
            np.add.at(vn, f[:, k], fn * area[:, None])
        vn /= np.maximum(np.linalg.norm(vn, axis=1, keepdims=True), 1e-12)
        self.n = np.concatenate([fn[cara], vn])
        self.arbol = cKDTree(self.p)

    def distancia(self, q):
        d, j = self.arbol.query(q)
        s = np.sign(((q - self.p[j]) * self.n[j]).sum(1))
        return d * np.where(s == 0, 1, s), j


def sin_caras_de_adentro(v, f, piel, cerca, umbral=-0.25):
    """Sin las caras de adentro de una prenda (las que miran al cuerpo, a menos de cerca m de el): no se ven y el
    tris_max rinde el doble. Las del borde (el grosor de la abertura) y las de afuera quedan."""
    fn, _ = normales_caras(v, f)
    c = v[f].mean(1)
    d, j = piel.distancia(c)
    adentro = (d < cerca) & ((fn * piel.n[j]).sum(1) < umbral)
    return f[~adentro]


def compactar(v, f):
    usados = np.unique(f)
    m = np.full(len(v), -1, np.int64)
    m[usados] = np.arange(len(usados))
    return v[usados], m[f]


def reducir(v, f, tris):
    """Hasta tris triangulos con cuadricas (fast_simplification), de a poco y con la malla normalizada (como
    lowres._decimate): la forma, la escala y el lugar quedan; sin los pedacitos sueltos que deja."""
    import fast_simplification
    v, f = soldar(v, f)
    v, f = compactar(v, f)
    for _ in range(24):
        if len(f) <= tris:
            break
        c, t = v.mean(0), float(np.linalg.norm(np.ptp(v, axis=0))) or 1.0
        v2, f2 = fast_simplification.simplify((v - c) / t, f, target_reduction=min(0.9, 1.0 - tris / len(f)))
        if len(f2) >= len(f):
            break
        v, f = np.asarray(v2, float) * t + c, np.asarray(f2, np.int64)
    _, area = normales_caras(v, f)
    f = f[area > 1e-10]
    lab = piezas_conexas(v, f)
    grande = np.bincount(lab[f[:, 0]], minlength=lab.max() + 1)
    f = f[grande[lab[f[:, 0]]] >= max(4, 0.01 * len(f))]
    return compactar(v, f)


def normales_40(v, f, angulo=40.0):
    """La normal de cada esquina (caras x 3): suave entre caras de menos de 40 grados, cortada en las aristas mas duras
    (lowres.normales_aristas, la misma de los cuerpos y los animales)."""
    import lowres
    return lowres.normales_aristas(v, f, angulo)


def reescribir_normales(path):
    """Las normales de un GLB ya hecho (cara por cara: los cuerpos de humanoide.py, los animales de animales.py), con
    el criterio de 40 grados, en su lugar: las posiciones, el color, la piel y los clips quedan como estan. Devuelve
    cuantos vertices cambio (0 si la malla tiene indices: ya comparte sus vertices)."""
    import lowres
    data = bytearray(open(path, "rb").read())
    n = struct.unpack("<I", data[12:16])[0]
    doc = json.loads(bytes(data[20:20 + n]))
    b0 = 20 + n + 8
    binario = bytes(data[b0:])
    total = 0
    for m in doc["meshes"]:
        for pr in m["primitives"]:
            at = pr["attributes"]
            if "indices" in pr or "NORMAL" not in at:
                continue
            pos = acc(doc, binario, at["POSITION"]).astype(float)
            nor = lowres.normales_soldando(pos).astype("<f4")
            a = doc["accessors"][at["NORMAL"]]
            bv = doc["bufferViews"][a["bufferView"]]
            o = b0 + bv.get("byteOffset", 0) + a.get("byteOffset", 0)
            paso = bv.get("byteStride", 12)
            for i in range(len(nor)):
                data[o + i * paso:o + i * paso + 12] = nor[i].tobytes()
            total += len(nor)
    open(path, "wb").write(data)
    return total


def pesos_de(q, piel_p, piel_j, piel_w, f=None, k=8, alisar=4):
    """Los pesos de cada punto: los de los k vertices de la piel mas cercanos (por la inversa de la distancia),
    alisados entre los vertices vecinos de la prenda (f) para que no salten de una pierna a la otra; los 4 mayores."""
    d, j = cKDTree(piel_p).query(q, k=k)
    w = 1.0 / np.maximum(d, 1e-4) ** 2
    huesos = int(max(piel_j.max(), 0)) + 1
    W = np.zeros((len(q), huesos))
    for c in range(k):
        for h in range(piel_j.shape[1]):
            np.add.at(W, (np.arange(len(q)), piel_j[j[:, c], h]), w[:, c] * piel_w[j[:, c], h])
    W /= np.maximum(W.sum(1, keepdims=True), 1e-12)
    if f is not None and alisar:
        e = np.concatenate([f[:, [0, 1]], f[:, [1, 2]], f[:, [2, 0]]])
        e = np.concatenate([e, e[:, ::-1]])
        for _ in range(alisar):
            S = np.zeros_like(W)
            np.add.at(S, e[:, 0], W[e[:, 1]])
            cnt = np.bincount(e[:, 0], minlength=len(q))[:, None]
            W = 0.5 * W + 0.5 * np.where(cnt > 0, S / np.maximum(cnt, 1), W)
    orden = np.argsort(-W, axis=1)[:, :4]
    ww = np.take_along_axis(W, orden, axis=1)
    ww /= np.maximum(ww.sum(1, keepdims=True), 1e-12)
    return orden.astype(np.uint8), ww.astype(np.float32)


def empujar(v, f, piel, holgura, veces=4):
    """Lo que de una prenda queda a menos de holgura m de la piel (o adentro), hacia afuera por la normal de la piel:
    los vertices y, por las caras grandes de una malla reducida, lo de adentro de cada cara (lo mueven sus tres
    vertices). El desplazamiento se alisa entre vecinos: sin picos."""
    v = v.copy()
    bary = np.array([[1 / 3, 1 / 3, 1 / 3], [0.6, 0.2, 0.2], [0.2, 0.6, 0.2], [0.2, 0.2, 0.6]])
    e = np.concatenate([f[:, [0, 1]], f[:, [1, 2]], f[:, [2, 0]]])
    e = np.concatenate([e, e[:, ::-1]])
    for _ in range(veces):
        mover = np.zeros_like(v)
        cuenta = np.zeros(len(v))
        d, j = piel.distancia(v)
        mal = d < holgura
        mover[mal] += piel.n[j[mal]] * (holgura - d[mal])[:, None]
        cuenta[mal] += 1
        for b in bary:
            q = (v[f] * b[None, :, None]).sum(1)
            d, j = piel.distancia(q)
            mal = d < holgura
            for k in range(3):
                np.add.at(mover, f[mal, k], piel.n[j[mal]] * ((holgura - d[mal]) * b[k] * 2.0)[:, None])
                np.add.at(cuenta, f[mal, k], 1)
        if not cuenta.any():
            break
        paso = mover / np.maximum(cuenta, 1)[:, None]
        S = np.zeros_like(paso)
        np.add.at(S, e[:, 0], paso[e[:, 1]])
        n = np.bincount(e[:, 0], minlength=len(v))[:, None]
        v += 0.6 * paso + 0.4 * S / np.maximum(n, 1)
    return v


def empujar_desde_piel(v, f, piel, holgura, veces=6, lejos=0.015):
    """Al reves que empujar: la piel que pasa una cara grande de la prenda reducida, o se le acerca a menos de holgura
    (la punta del pie por la puntera de la bota cae entre las muestras de empujar), la empuja por su normal lo que
    falta. Solo la piel debajo de la cara (su proyeccion cae dentro de ella y las dos miran al mismo lado) y que la
    pasa no mas de lejos: lo que asoma; la rodilla, por encima del borde de la cana, no la abre en embudo."""
    v = v.copy()
    lo, hi = v.min(0) - lejos, v.max(0) + lejos
    sel = np.all((piel.p > lo) & (piel.p < hi), axis=1)
    pp, pn = piel.p[sel], piel.n[sel]
    if not len(pp):
        return v
    for _ in range(veces):
        fn, _ = normales_caras(v, f)
        a, b, c = v[f[:, 0]], v[f[:, 1]], v[f[:, 2]]
        _, cand = cKDTree((a + b + c) / 3).query(pp, k=min(8, len(f)))
        cand = cand.reshape(len(pp), -1)
        falta = np.zeros(len(f))
        for k in range(cand.shape[1]):
            F = cand[:, k]
            n = fn[F]
            h = ((pp - a[F]) * n).sum(1)  # la altura de la piel sobre el plano de la cara (+, afuera)
            q = pp - h[:, None] * n
            e0, e1, e2 = b[F] - a[F], c[F] - a[F], q - a[F]
            d00, d01, d11 = (e0 * e0).sum(1), (e0 * e1).sum(1), (e1 * e1).sum(1)
            d20, d21 = (e2 * e0).sum(1), (e2 * e1).sum(1)
            den = np.where(np.abs(d00 * d11 - d01 * d01) < 1e-18, 1e-18, d00 * d11 - d01 * d01)
            bb, bc = (d11 * d20 - d01 * d21) / den, (d00 * d21 - d01 * d20) / den
            ok = (bb >= 0) & (bc >= 0) & (bb + bc <= 1) & ((n * pn).sum(1) > 0.5) & (h > -holgura) & (h < lejos)
            np.maximum.at(falta, F[ok], h[ok] + holgura)
        if not (falta > 1e-5).any():
            break
        mover = np.zeros_like(v)
        cuenta = np.zeros(len(v))
        mal = falta > 1e-5
        for k in range(3):
            np.add.at(mover, f[mal, k], fn[mal] * falta[mal, None])
            np.add.at(cuenta, f[mal, k], 1)
        v += mover / np.maximum(cuenta, 1)[:, None]
    return v


# ---------------------------------------------------------------- vistas (sin dependencias: un z-buffer en numpy)
VISTA = {"frente": np.array([-1.0, 0, 0]), "lado": np.array([0, 0, 1.0]), "atras": np.array([1.0, 0, 0]),
         "tres_cuartos": np.array([-0.75, -0.25, 0.6]), "izquierda": np.array([0, 0, -1.0])}
LUZ = np.array([-0.45, 1.0, -0.3]) / np.linalg.norm([-0.45, 1.0, -0.3])


def render(objetos, mira, alto_px=640, ancho_px=360, margen=0.06, caja=None):
    """objetos: [(vertices, caras, color de cada cara RGB)]; mira: hacia donde mira la camara (ortografica). La luz,
    la del sol del juego, mas una de relleno."""
    mira = mira / np.linalg.norm(mira)
    arriba = np.array([0, 1.0, 0]) - mira * mira[1]
    arriba /= np.linalg.norm(arriba)
    der = np.cross(mira, arriba)
    todos = np.concatenate([o[0] for o in objetos])
    pu, pv = todos @ der, todos @ arriba
    if caja is None:
        caja = (pu.min(), pu.max(), pv.min(), pv.max())
    u0, u1, v0, v1 = caja
    esc = min(ancho_px / (u1 - u0), alto_px / (v1 - v0)) * (1 - 2 * margen)
    cu, cv = (u0 + u1) / 2, (v0 + v1) / 2
    img = np.full((alto_px, ancho_px, 3), 236.0)
    z = np.full((alto_px, ancho_px), np.inf)
    for obj in objetos:
        v, f, col = obj[:3]
        uvs, tex = (obj[3], obj[4]) if len(obj) > 4 and obj[4] is not None else (None, None)
        fn, _ = normales_caras(v, f)
        luz = 0.42 + 0.48 * np.clip(fn @ LUZ, 0, 1) + 0.18 * np.clip(-(fn @ mira), 0, 1)
        X = (v @ der - cu) * esc + ancho_px / 2
        Y = alto_px / 2 - (v @ arriba - cv) * esc
        Z = v @ mira
        for i, t in enumerate(f):
            if fn[i] @ mira > 0.05:  # de espaldas a la camara
                continue
            xs, ys, zs = X[t], Y[t], Z[t]
            x0, x1 = max(int(np.floor(xs.min())), 0), min(int(np.ceil(xs.max())), ancho_px - 1)
            y0, y1 = max(int(np.floor(ys.min())), 0), min(int(np.ceil(ys.max())), alto_px - 1)
            if x1 < x0 or y1 < y0:
                continue
            gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
            d = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
            if abs(d) < 1e-12:
                continue
            a = ((ys[1] - ys[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gy - ys[2])) / d
            b = ((ys[2] - ys[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gy - ys[2])) / d
            c = 1 - a - b
            dentro = (a >= -1e-6) & (b >= -1e-6) & (c >= -1e-6)
            if not dentro.any():
                continue
            zz = a * zs[0] + b * zs[1] + c * zs[2]
            sub = z[y0:y1 + 1, x0:x1 + 1]
            gana = dentro & (zz < sub)
            sub[gana] = zz[gana]
            if tex is None:
                img[y0:y1 + 1, x0:x1 + 1][gana] = np.clip(col[i] * luz[i], 0, 255)
            else:  # la textura en cada pixel (la UV de sus esquinas, interpolada), con filtro de punto como el juego
                q = uvs[i]
                u = a[gana] * q[0, 0] + b[gana] * q[1, 0] + c[gana] * q[2, 0]
                w = a[gana] * q[0, 1] + b[gana] * q[1, 1] + c[gana] * q[2, 1]
                th, tw = tex.shape[:2]
                t = tex[np.clip((w * th).astype(int), 0, th - 1), np.clip((u * tw).astype(int), 0, tw - 1)]
                img[y0:y1 + 1, x0:x1 + 1][gana] = np.clip(t * luz[i], 0, 255)
    return Image.fromarray(img.astype(np.uint8)), caja


def rot_y(grados):
    a = np.radians(grados)
    c, s = np.cos(a), np.sin(a)
    return np.array([[c, 0, s], [0, 1, 0], [-s, 0, c]])


def rot_z(grados):
    """El giro alrededor de +Z (de oreja a oreja): con grados positivos, +X (el frente) sube."""
    a = np.radians(grados)
    c, s = np.cos(a), np.sin(a)
    return np.array([[c, -s, 0], [s, c, 0], [0, 0, 1.0]])


def m4(R=None, t=None, s=1.0):
    M = np.eye(4)
    if R is not None:
        M[:3, :3] = R * s
    if t is not None:
        M[:3, 3] = t
    return M


# ---------------------------------------------------------------- la textura: el render de Props, de frente
TEX = 256
TONOS = {  # rampa de oscuro a claro (RGB) por luminancia: el material del id cuando el render es de otro
    "hierro": ((28, 30, 34), (98, 102, 110), (190, 194, 200)),
    "cuero": ((38, 22, 12), (118, 72, 38), (196, 146, 96)),
    "fieltro": ((58, 52, 44), (142, 130, 110), (206, 196, 174)),
    "oro": ((62, 40, 10), (186, 136, 40), (250, 222, 130)),
}


def imagen(nombre):
    """El render y la silueta del objeto (lo que no es el color del fondo, el de los bordes; sin motas ni huecos). Con
    fondo verde, sin el verde que se pega al borde: el verde no pasa del mayor de rojo y azul."""
    rgb = np.asarray(Image.open(os.path.join(PROPS, "imagenes", nombre + ".jpg")).convert("RGB"))
    a = rgb.astype(np.int16)
    bg = np.median(np.concatenate([a[0], a[-1], a[:, 0], a[:, -1]]), axis=0)
    m = ndimage.binary_opening(np.abs(a - bg).sum(axis=2) > 40, iterations=1)
    lab, n = ndimage.label(m)
    if n:
        tam = ndimage.sum(m, lab, range(1, n + 1))
        m = np.isin(lab, 1 + np.flatnonzero(tam >= tam.max() * 0.05))
    m = ndimage.binary_fill_holes(m)
    if bg[1] > bg[0] + 30 and bg[1] > bg[2] + 30:
        a = a.copy()
        a[..., 1] = np.minimum(a[..., 1], np.maximum(a[..., 0], a[..., 2]) + 6)
    return a.astype(np.uint8), m


def silueta(p2, f, cols, rows):
    from PIL import ImageDraw
    img = Image.new("L", (cols, rows), 0)
    d = ImageDraw.Draw(img)
    for t in f:
        d.polygon([(float(p2[i, 0]), float(p2[i, 1])) for i in t], fill=255, outline=255)
    return np.asarray(img) > 0


def coincide(uv, f, mask, n=64):
    """Interseccion sobre union de la silueta de la malla proyectada (uv: u a la derecha, v arriba) con la del render,
    las dos en su caja."""
    def norm(m):
        ys, xs = np.nonzero(m)
        c = m[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
        return np.asarray(Image.fromarray(c.astype(np.uint8) * 255).resize((n, n), Image.BILINEAR)) > 127
    u0, u1, v0, v1 = uv[:, 0].min(), uv[:, 0].max(), uv[:, 1].min(), uv[:, 1].max()
    s = max(u1 - u0, v1 - v0) / 300
    sil = silueta(np.column_stack([(uv[:, 0] - u0) / s, (v1 - uv[:, 1]) / s]), f, int((u1 - u0) / s) + 2, int((v1 - v0) / s) + 2)
    a, b = norm(sil), norm(mask)
    return (a & b).sum() / max((a | b).sum(), 1)


def tonar(rgb, tono):
    lum = (rgb @ np.array([0.299, 0.587, 0.114])) / 255.0
    lum = (lum - lum.min()) / max(np.ptp(lum), 1e-6)
    a, b, c = (np.array(x, float) for x in TONOS[tono])
    t = lum[..., None]
    return np.where(t < 0.5, a + (b - a) * (t / 0.5), b + (c - b) * ((t - 0.5) / 0.5))


def _extender(img, nucleo):
    """Lo que no es nucleo, con el color de lo que si (cerca, el de al lado; lejos, el medio de una zona mas grande):
    una media gaussiana normalizada a escalas crecientes. Sin las rayas que deja copiar el pixel mas cercano, que las
    caras de costado (las que la proyeccion de frente ve de canto) estiran en barridos."""
    out = img.astype(float).copy()
    peso = nucleo.astype(float)
    falta = ~nucleo
    for sigma in (2.0, 6.0, 16.0, 40.0, 100.0):
        if not falta.any():
            break
        den = ndimage.gaussian_filter(peso, sigma)
        ok = falta & (den > 0.05)
        for c in range(3):
            num = ndimage.gaussian_filter(img[..., c].astype(float) * peso, sigma)
            out[..., c][ok] = (num / np.maximum(den, 1e-6))[ok]
        falta &= ~ok
    if falta.any():
        out[falta] = img[nucleo].mean(0)
    return out


class Textura:
    """El render recortado a su silueta en dos mitades de una textura de TEX px: el frente y, para las caras que
    miran atras (el render no muestra la espalda), lo mismo sin los huecos. Las mitades van una al lado de la otra si
    el objeto es alto, una sobre otra si es ancho. Sin la luz grande del render (se divide por la luminancia muy
    borrosa): la luz la pone el juego. Con paleta de 128 colores, como las de PS2."""

    def __init__(self, nombre, tono=None, mezcla=1.0):
        from piezas import _rellenar
        self.rgb, self.mask = imagen(nombre)
        ys, xs = np.nonzero(self.mask)
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        crop, cm = self.rgb[y0:y1, x0:x1], self.mask[y0:y1, x0:x1]
        frente = _rellenar(crop, cm, umbral=40, borde=2)
        hueco = np.all(np.abs(frente - np.array([34.0, 28.0, 22.0])) < 0.5, axis=2)
        # Lo claro como el fondo en la franja del borde (el pelo de la piel sobre el fondo blanco del render, el brillo
        # del canto) no es color de la pieza: con lo de afuera, toma el de lo de adentro (las manchas claras de los
        # costados de los cascos).
        a = self.rgb.astype(float)
        fondo = np.median(np.concatenate([a[0], a[-1], a[:, 0], a[:, -1]]), axis=0)
        franja = cm & ~ndimage.binary_erosion(cm, iterations=max(2, int(0.02 * max(cm.shape))))
        claro = franja & (np.abs(crop.astype(float) - fondo).sum(axis=2) < 150)
        color = cm & ~claro
        frente = _extender(frente, color if color.sum() > 50 else cm)
        lleno = color & ~hueco
        self.lleno = lleno  # el color de la pieza en el recorte de la silueta (sin huecos ni lo claro del borde)
        # Para el color de las caras de canto: cada pixel, apenas alisado (sin el grano del cuero o del pelo).
        self.suave = np.stack([ndimage.gaussian_filter(frente[..., k], 1.5) for k in range(3)], axis=-1)
        nucleo = ndimage.binary_erosion(lleno, iterations=2) if lleno.sum() > 50 else lleno
        espalda = _extender(frente, nucleo if nucleo.any() else cm)
        self.alto_img = (y1 - y0) >= (x1 - x0)
        tam = (TEX // 2, TEX) if self.alto_img else (TEX, TEX // 2)
        mitades = []
        for out in (frente, espalda):
            lum = out @ np.array([0.299, 0.587, 0.114])
            blur = ndimage.gaussian_filter(lum, sigma=max(out.shape[:2]) / 5.0)
            out = out * np.clip((blur.mean() / np.maximum(blur, 1.0)) ** 0.45, 0.7, 1.5)[..., None]
            if tono:
                out = tonar(np.clip(out, 0, 255), tono) * mezcla + np.clip(out, 0, 255) * (1.0 - mezcla)
            mitades.append(np.asarray(Image.fromarray(np.clip(out, 0, 255).astype(np.uint8)).resize(tam, Image.LANCZOS)))
        img = np.concatenate(mitades, axis=1 if self.alto_img else 0)
        self.png = Image.fromarray(img).quantize(128, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)

    def uv(self, p, f, frente, arriba):
        """La UV de cada esquina (caras x 3 x 2): los vertices p proyectados de frente (frente: hacia la camara;
        arriba: el arriba del render), su caja sobre la de la silueta. Si la malla calza mejor en espejo (el render la
        muestra del otro lado), en espejo. Las caras que miran atras, en la otra mitad."""
        der = np.cross(arriba, frente)
        q = np.column_stack([p @ der, p @ arriba])
        espejo = coincide(q * [-1, 1], f, self.mask) > coincide(q, f, self.mask) + 0.02
        if espejo:
            q[:, 0] *= -1
        u0, u1, v0, v1 = q[:, 0].min(), q[:, 0].max(), q[:, 1].min(), q[:, 1].max()
        uv = np.column_stack([(q[:, 0] - u0) / max(u1 - u0, 1e-9), (v1 - q[:, 1]) / max(v1 - v0, 1e-9)])
        uv = np.clip(uv, 0.002, 0.998)
        fn, area = normales_caras(p, f)
        atras = fn @ frente < 0
        out = uv[f].copy()
        # Las caras que la proyeccion ve de canto (los costados) estiraban el borde del render (el brillo del canto de
        # un casco, el pelo de la oreja del gorro) en un barrido claro: un solo color, el de sus vecinas en la malla.
        # De las caras que si se ven de frente y tocan un vertice suyo (si no hay, la mas cercana en la malla), el
        # color que cubre mas superficie entre ellas (las que se le parecen, sumadas por area), tomado del centro de
        # la que lo tiene. Lo fino no gana nunca: ni el brillo del canto ni las puntas claras del pelo, ni el borde
        # oscuro de la silueta (que dejaba triangulos negros), ni la correa del tobillo junto a la cana de fieltro. Y
        # no depende de la imagen alrededor de la cara (el medio del render puede ser el hueco entre las dos botas).
        canto = np.abs(fn @ frente) < 0.3
        if canto.any() and (~canto).any():
            from collections import deque
            caras_de = [[] for _ in range(len(p))]
            for i, t in enumerate(f.tolist()):
                for x in t:
                    caras_de[x].append(i)
            de = np.full(len(f), -1)
            de[~canto] = np.flatnonzero(~canto)
            cola = deque(np.flatnonzero(~canto).tolist())
            while cola:
                i = cola.popleft()
                for x in f[i]:
                    for j in caras_de[x]:
                        if de[j] < 0:
                            de[j] = de[i]
                            cola.append(j)
            centro = out.mean(1)
            h, w = self.lleno.shape
            py = np.clip((centro[:, 1] * h).astype(int), 0, h - 1)
            px = np.clip((centro[:, 0] * w).astype(int), 0, w - 1)
            color, sobre = self.suave[py, px], self.lleno[py, px]  # el color del centro de cada cara, si cae en la pieza
            for i in np.flatnonzero(canto & (de >= 0)):
                vecinas = sorted({j for x in f[i] for j in caras_de[x] if not canto[j] and sobre[j]})
                vs = np.array(vecinas if vecinas else [de[i]])
                parecidas = np.abs(color[vs][:, None] - color[vs][None]).sum(axis=2) < 75
                out[i] = centro[vs[np.argmax((parecidas * area[vs][None]).sum(axis=1))]]
        k = 0 if self.alto_img else 1
        out[..., k] = out[..., k] * 0.5 + np.where(atras, 0.5, 0.0)[:, None]
        return out, espejo


# ---------------------------------------------------------------- GLB del juego
def _glb(doc, blob):
    js = json.dumps(doc, separators=(",", ":")).encode("utf-8")
    js += b" " * (-len(js) % 4)
    blob += b"\x00" * (-len(blob) % 4)
    out = io.BytesIO()
    out.write(struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(js) + 8 + len(blob)))
    out.write(struct.pack("<I4s", len(js), b"JSON") + js)
    out.write(struct.pack("<I4s", len(blob), b"BIN\x00") + blob)
    return out.getvalue()


def glb_pieza(v, f, nor, uv, tex, nombre, generador, piel=None):
    """Tres vertices por cara con su normal (la de normales_40), su UV y la textura (PNG, filtro de punto). Con piel
    (joints, weights por esquina y el Cuerpo): el esqueleto del cuerpo (sus nodos, en su reposo) y su piel, sin clips."""
    pos = v[f].reshape(-1, 3).astype("<f4")
    datos = [pos.tobytes(), nor.reshape(-1, 3).astype("<f4").tobytes(), uv.reshape(-1, 2).astype("<f4").tobytes()]
    tipos = [("VEC3", 5126, True), ("VEC3", 5126, False), ("VEC2", 5126, False)]
    nombres = ["POSITION", "NORMAL", "TEXCOORD_0"]
    if piel is not None:
        joints, weights, cuerpo = piel
        datos += [joints.reshape(-1, 4).astype(np.uint8).tobytes(), weights.reshape(-1, 4).astype("<f4").tobytes()]
        tipos += [("VEC4", 5121, False), ("VEC4", 5126, False)]
        nombres += ["JOINTS_0", "WEIGHTS_0"]
    png = io.BytesIO()
    tex.save(png, "PNG", optimize=True)
    blob, views, accs = b"", [], []
    for (tipo, comp, minmax), d in zip(tipos, datos):
        blob += b"\x00" * (-len(blob) % 4)
        views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(d), "target": 34962})
        a = {"bufferView": len(views) - 1, "componentType": comp, "count": len(pos), "type": tipo}
        if minmax:
            a["min"], a["max"] = pos.min(0).tolist(), pos.max(0).tolist()
        accs.append(a)
        blob += d
    blob += b"\x00" * (-len(blob) % 4)
    views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(png.getvalue())})
    img_view = len(views) - 1
    blob += png.getvalue()
    prim = {"attributes": {n: i for i, n in enumerate(nombres)}, "material": 0}
    doc = {"asset": {"version": "2.0", "generator": generador}, "scene": 0,
           "meshes": [{"name": nombre, "primitives": [prim]}],
           "materials": [{"name": "pieza", "pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 0,
                                                                     "roughnessFactor": 1}}],
           "samplers": [{"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 33071}],
           "textures": [{"sampler": 0, "source": 0}], "images": [{"bufferView": img_view, "mimeType": "image/png"}],
           "accessors": accs, "bufferViews": views}
    if piel is None:
        doc["nodes"] = [{"name": nombre, "mesh": 0}]
        doc["scenes"] = [{"nodes": [0]}]
    else:
        u = cuerpo.u
        nodos = []
        for i, nd in enumerate(u["nodos"]):
            n = {"name": u["names"][i], "translation": [float(x) for x in u["TI"][i]], "rotation": [float(x) for x in u["RI"][i]]}
            if nd.get("children"):
                n["children"] = nd["children"]
            nodos.append(n)
        nodos.append({"name": nombre, "mesh": 0, "skin": 0})
        ibm = np.stack([m.T.reshape(-1) for m in u["ibm"]]).astype("<f4").tobytes()
        blob += b"\x00" * (-len(blob) % 4)
        views.append({"buffer": 0, "byteOffset": len(blob), "byteLength": len(ibm)})
        accs.append({"bufferView": len(views) - 1, "componentType": 5126, "count": len(u["ibm"]), "type": "MAT4"})
        blob += ibm
        doc["nodes"] = nodos
        doc["scenes"] = [{"nodes": [u["root"], len(nodos) - 1]}]
        doc["skins"] = [{"joints": list(range(len(u["nodos"]))), "skeleton": u["root"], "inverseBindMatrices": len(accs) - 1}]
    doc["buffers"] = [{"byteLength": len(blob) + (-len(blob) % 4)}]
    return _glb(doc, blob)


# ---------------------------------------------------------------- las recetas
# id, malla de Props, forma, imagen (el render de Props que da la textura) y lo propio de cada una:
#   patron: la malla del patron de la que sale (su lugar exacto); sin ella, con el criterio de las de su forma
#   az: el giro (grados alrededor de +Y) que pone la malla como la muestra el render (de frente, a +Z)
#   punta: "arriba" o "abajo", donde va el extremo ancho (la cabeza de la lanza; la cruz de la espada, abajo: la
#          hoja arriba); agarre: la empunadura, en fraccion del largo desde abajo; filo: "convexo" (el sable: el lado
#          convexo de la hoja, que corta, hacia +Z)
#   tono, mezcla: el render recoloreado con el material del id
RECETAS = [
    # Corazas y botas: con piel (una por cuerpo)
    dict(id="armadura.torso.laminar_cuero", malla="chaleco_largo", forma="torso", img="chaleco_cuero-1", patron="white_mesh(3)"),
    dict(id="armadura.torso.fieltro", malla="coraza_faldar", forma="torso", img="coraza_cuero-4", tono="fieltro", mezcla=0.85),
    dict(id="armadura.torso.escamas_hierro", malla="laminar_hombreras", forma="torso", img="laminar_hombreras-1", tono="hierro",
         mezcla=0.8),
    dict(id="armadura.torso.culto", malla="coraza_lisa", forma="torso", img="pectoral_oro-1"),
    dict(id="armadura.botas.laminar_cuero", malla="botas_montar", forma="botas", img="botas_montar-4",
         patron=("white_mesh(18) 1", "white_mesh(18) 2")),
    dict(id="armadura.botas.fieltro", malla="botas_montar", forma="botas", img="botas_fieltro-2",
         patron=("white_mesh(18) 1", "white_mesh(18) 2")),
    # Cascos: rigidos, en la cabeza
    dict(id="armadura.casco.oro_padre", malla="casco_aguila", forma="casco", img="casco_aguila-1", az=270, tono="oro", mezcla=0.6,
         patron="model"),
    dict(id="armadura.casco.fieltro", malla="gorro_piel", forma="casco", img="gorro_piel-1", az=0),
    dict(id="armadura.casco.culto", malla="casco_calado", forma="casco", img="casco_bronce-1", az=30),
    dict(id="armadura.casco.mascara_culto", malla="casco_visera", forma="casco", img="casco_escamas-3", az=330),
    dict(id="armadura.casco.laminar_cuero", malla="casco_costillas", forma="casco", img="casco_hierro-3", az=0, tono="cuero",
         mezcla=0.85),
    dict(id="armadura.casco.bronce", malla="casco_carrilleras", forma="casco", img="casco_bronce-2", az=30),
    dict(id="armadura.casco.escamas_hierro", malla="casco_nasal", forma="casco", img="casco_hierro-4", az=0),
    dict(id="armadura.casco.malla", malla="casco_ala", forma="casco", img="casco_hierro-1", az=0),
    # Armas, escudos y objetos: rigidos
    dict(id="arma.corta.sable", malla="sable", forma="arma", img="sable-3", punta="abajo", agarre=0.07, filo="convexo",
         patron="Curved sword"),
    dict(id="arma.corta.daga", malla="daga", forma="arma", img="daga-1", punta="abajo", agarre=0.12),
    dict(id="arma.larga.espada_larga", malla="espada", forma="arma", img="espada-3", punta="abajo", agarre=0.08,
         patron="white_mesh(25)"),
    dict(id="arma.corta.maza", malla="maza_pinchos", forma="arma", img="maza_pinchos-1", punta="arriba", agarre=0.12),
    dict(id="arma.larga.lanza", malla="lanza", forma="arma", img="lanza-1", punta="arriba", agarre=0.38, patron="white_mesh(24)",
         plana=True),
    dict(id="arma.larga.alabarda", malla="alabarda", forma="arma", img="hacha_asta-2", punta="arriba", agarre=0.35,
         patron="white_mesh(26)", plana=True),
    dict(id="arma.distancia.arco_simple", malla="arco", forma="arco", img="arco_simple-2", agarre=0.5, patron="white_mesh(27)",
         plana=True),
    dict(id="utileria.objeto.antorcha", malla="antorcha", forma="arma", img="antorcha-2", punta="arriba", agarre=0.25,
         patron="white_mesh(20)"),
    dict(id="utileria.objeto.aljaba", malla="aljaba", forma="objeto", img="aljaba-2", patron="white_mesh(28)"),
    dict(id="escudo.mano.cuero", malla="escudo_redondo_3", forma="escudo", img="escudo_madera-1", patron="hy3d-model(3)"),
    dict(id="escudo.mano.umbo", malla="escudo_redondo_1", forma="escudo", img="escudo_bronce-4"),
    dict(id="escudo.mano.culto", malla="escudo_redondo_2", forma="escudo", img="escudo_bronce-3"),
]
PRENDAS = ("torso", "botas")
# Los anclajes (el orden, el de GearSocket en src/world/props.h): el hueso y de que pieza del patron sale su lugar.
# Los de la mano y el antebrazo no estan en el patron: salen de la mano del cuerpo en reposo (anclaje_mano).
ANCLAJES_DEF = (("GEAR_HEAD", "head", "model"), ("GEAR_BACK_SHIELD", "spine_03", "hy3d-model(3)"),
                ("GEAR_BACK_QUIVER", "spine_03", "white_mesh(28)"), ("GEAR_BACK_LONG", "spine_03", "white_mesh(27)"),
                ("GEAR_HIP_LEFT", "pelvis", "Curved sword"), ("GEAR_HIP_RIGHT", "pelvis", "white_mesh(25)"),
                ("GEAR_HAND_RIGHT", "hand_r", None), ("GEAR_HAND_LEFT", "hand_l", None), ("GEAR_FOREARM_LEFT", "lowerarm_l", None))
HOLGURA = 0.005  # lo minimo entre la prenda y la piel (el patron: p05 de 4 a 7 mm)


def receta(mid):
    return next((r for r in RECETAS if r["id"] == mid), None)


def pieza_suelta(o, f, n):
    """De una malla con piezas sueltas (las dos botas), la de n vertices: sus vertices y sus caras."""
    lab = piezas_conexas(o, f)
    k = next(k for k in np.unique(lab) if (lab == k).sum() == n)
    keep = lab == k
    idx = np.full(len(o), -1)
    idx[keep] = np.arange(keep.sum())
    return o[keep], idx[f[keep[f[:, 0]]]]


# ---------------------------------------------------------------- como la muestra el render
def ancho_extremos(v, frac=0.15):
    lo, hi = v[:, 1].min(), v[:, 1].max()
    h = hi - lo
    abajo, arriba = v[v[:, 1] <= lo + frac * h], v[v[:, 1] >= hi - frac * h]
    return np.ptp(abajo[:, [0, 2]], axis=0).max(), np.ptp(arriba[:, [0, 2]], axis=0).max()


def ancho_extremos_img(mask, frac=0.15):
    rows = np.flatnonzero(mask.any(axis=1))
    r0, r1 = rows[0], rows[-1]
    h = r1 - r0 + 1
    def ancho(sl):
        cols = np.flatnonzero(mask[sl].any(axis=0))
        return cols[-1] - cols[0] + 1 if len(cols) else 0
    return ancho(slice(r1 - int(frac * h), r1 + 1)), ancho(slice(r0, r0 + int(frac * h) + 1))


def a_render(o, f, rec, tex):
    """La rotacion (3x3) que pone la malla de Props como la muestra su render: +Y arriba, el ancho en X y la vista
    desde +Z. Las armas vienen acostadas: el eje mas largo pasa a Y, el mediano a X y el corto (el grosor de la hoja)
    a Z, con la punta ancha del lado que la muestra el render; lo demas viene de pie, girado az grados."""
    if rec["forma"] in ("arma", "arco"):
        c = o - (o.min(0) + o.max(0)) / 2
        orden = np.argsort(-np.ptp(c, axis=0))
        R = np.zeros((3, 3))
        R[1, orden[0]] = R[0, orden[1]] = R[2, orden[2]] = 1.0
        if np.linalg.det(R) < 0:
            R[2] *= -1
        v = c @ R.T
        ma, mb = ancho_extremos(v)
        ia, ib = ancho_extremos_img(tex.mask)
        if (mb > ma) != (ib > ia):  # la punta ancha del mismo lado que en el render
            R = np.diag([-1.0, -1.0, 1.0]) @ R
        return R
    return rot_y(rec.get("az", 0))


def marco(o, f, rec, it, R):
    """Del render al marco del objeto en el juego (4x4, con la escala del inventario):
    - armas: la empunadura en el origen, la hoja (o el asta) hacia +Y, el filo hacia +Z (el grosor en X);
    - el arco: el agarre en el origen, las palas en Y, el lomo (lo que mira al blanco) hacia +Z;
    - escudos: el frente hacia +X, el origen en el centro de atras;
    - lo demas (la aljaba): el frente hacia +X, el origen en el centro de la base."""
    v = o @ R.T
    alto = float(it["medidas_m"].split("x")[1])
    forma = rec["forma"]
    if forma in ("arma", "arco"):
        # render: la hoja en el plano XY (se ve de plano), el grosor en Z. Marco: X = Z del render, Y = Y (con la
        # punta ancha arriba o abajo, segun la receta), Z = X x Y.
        ma, mb = ancho_extremos(v)
        arriba_ancho = mb > ma
        sy = 1.0
        if rec.get("punta") == "arriba" and not arriba_ancho or rec.get("punta") == "abajo" and arriba_ancho:
            sy = -1.0
        B = np.array([[0, 0, 1.0], [0, sy, 0], [0, 0, 0]])
        B[2] = np.cross(B[0], B[1])
        w = v @ B.T
        if rec.get("filo") == "convexo" or forma == "arco":
            # el lado convexo hacia +Z: el medio de la hoja (o del arco) sale hacia ese lado respecto de las puntas
            y0, y1 = w[:, 1].min(), w[:, 1].max()
            medio = w[np.abs(w[:, 1] - (y0 + y1) / 2) < 0.1 * (y1 - y0), 2].mean()
            puntas = w[(w[:, 1] < y0 + 0.08 * (y1 - y0)) | (w[:, 1] > y1 - 0.08 * (y1 - y0)), 2].mean()
            if medio < puntas:
                B[0] *= -1
                B[2] *= -1
                w = v @ B.T
        y0, y1 = w[:, 1].min(), w[:, 1].max()
        ya = y0 + rec.get("agarre", 0.0) * (y1 - y0)
        cerca = w[np.abs(w[:, 1] - ya) < 0.04 * (y1 - y0)]
        origen = np.array([cerca[:, 0].mean(), ya, cerca[:, 2].mean()])
        k = alto / (y1 - y0)
    else:
        B = np.array([[0, 0, 1.0], [0, 1.0, 0], [-1.0, 0, 0]])  # el frente del render (+Z) a +X
        w = v @ B.T
        k = alto / np.ptp(w[:, 1])
        if forma == "escudo":
            origen = np.array([w[:, 0].min(), (w[:, 1].min() + w[:, 1].max()) / 2, (w[:, 2].min() + w[:, 2].max()) / 2])
        else:
            origen = np.array([(w[:, 0].min() + w[:, 0].max()) / 2, w[:, 1].min(), (w[:, 2].min() + w[:, 2].max()) / 2])
    M = np.eye(4)
    M[:3, :3] = k * (B @ R)
    M[:3, 3] = -k * origen
    return M


# ---------------------------------------------------------------- los cascos: la cupula sobre el craneo
HOLGURA_CASCO = 0.002  # m: lo minimo entre el casco y la piel de la cabeza, en todo el casco y en cada cuerpo
CEJAS = 0.6            # las cejas, de la barbilla (0) a la coronilla (1): la altura del centro del craneo
BORDE_CASCO = 0.678    # el borde de adelante del casco (frente a los ojos), en la misma medida: el del aguila del patron
OJOS = 0.032           # m (en el comun): de la linea media a cada ojo, donde se mide el borde de adelante


BARI = 1e-9  # la tolerancia de las coordenadas baricentricas: el rayo que pasa justo por una arista o un vertice golpea


def rayos(o, d, v, f, ultimo=False):
    """La distancia del primer golpe de cada rayo (origen o, direccion d unitaria) contra los triangulos v[f] (inf si
    no golpea; con ultimo, la del ultimo golpe, 0 si no golpea): Moller-Trumbore, de a tandas. Con la tolerancia
    BARI, un rayo justo por una arista o un vertice no se escapa entre las caras."""
    a = v[f[:, 0]]
    e1, e2 = v[f[:, 1]] - a, v[f[:, 2]] - a
    out = np.full(len(o), 0.0 if ultimo else np.inf)
    tanda = max(8, int(2e6 // max(len(f), 1)))
    for i in range(0, len(o), tanda):
        oo, dd = o[i:i + tanda, None], d[i:i + tanda, None]
        p = np.cross(dd, e2[None])
        det = (e1[None] * p).sum(-1)
        inv = 1.0 / np.where(np.abs(det) > 1e-12, det, np.inf)
        s = oo - a[None]
        u = (s * p).sum(-1) * inv
        q = np.cross(s, e1[None])
        w = (dd * q).sum(-1) * inv
        t = (e2[None] * q).sum(-1) * inv
        golpe = (u >= -BARI) & (w >= -BARI) & (u + w <= 1 + BARI) & (t > 1e-9)
        if ultimo:
            out[i:i + tanda] = np.where(golpe, t, 0.0).max(1)
        else:
            out[i:i + tanda] = np.where(golpe, t, np.inf).min(1)
    return out


class Cabeza:
    """La piel de la cabeza y el cuello, para calzar cascos: la distancia con signo de un punto (negativa adentro). El
    modulo, al punto mas cercano de la piel; el signo, desde el centro del craneo: adentro si esta mas cerca del centro
    que lo mas lejano de la piel en esa direccion (la cabeza vista desde su centro: las normales no sirven, que los
    pliegues de las orejas tienen caras que miran adentro)."""

    def __init__(self, v, f, centro, paso=2.0):
        self.sup = Superficie(v, f, n=80000)
        self.c = np.asarray(centro, float)
        self.el = np.radians(np.arange(-90.0, 90.0 + paso / 2, paso))
        self.az = np.radians(np.arange(0.0, 360.0 + paso / 2, paso))
        E, A = np.meshgrid(self.el, self.az, indexing="ij")
        d = np.stack([np.cos(E) * np.cos(A), np.sin(E), np.cos(E) * np.sin(A)], -1).reshape(-1, 3)
        self.r = rayos(np.tile(self.c, (len(d), 1)), d, v, f, ultimo=True).reshape(E.shape)

    def radio(self, d):
        """Lo mas lejano de la piel desde el centro en las direcciones d (unitarias), interpolado."""
        el = np.arcsin(np.clip(d[:, 1], -1, 1))
        az = np.arctan2(d[:, 2], d[:, 0]) % (2 * np.pi)
        i = np.clip((el - self.el[0]) / (self.el[1] - self.el[0]), 0, len(self.el) - 1.001)
        j = np.clip(az / (self.az[1] - self.az[0]), 0, len(self.az) - 1.001)
        i0, j0 = i.astype(int), j.astype(int)
        fi, fj = i - i0, j - j0
        r = self.r
        return ((1 - fi) * (1 - fj) * r[i0, j0] + fi * (1 - fj) * r[i0 + 1, j0] + (1 - fi) * fj * r[i0, j0 + 1] +
                fi * fj * r[i0 + 1, j0 + 1])

    def distancia(self, q):
        d, j = self.sup.arbol.query(q)
        rel = q - self.c
        n = np.linalg.norm(rel, axis=1)
        adentro = n < self.radio(rel / np.maximum(n, 1e-12)[:, None])
        return np.where(adentro, -d, d), j


def muestras(v, f, n, semilla=5):
    """n puntos repartidos por area sobre la malla."""
    _, area = normales_caras(v, f)
    rng = np.random.default_rng(semilla)
    cara = rng.choice(len(f), n, p=area / area.sum())
    r1, r2 = np.sqrt(rng.random(n)), rng.random(n)
    b = np.column_stack([1 - r1, r1 * (1 - r2), r1 * r2])
    return (v[f[cara]] * b[:, :, None]).sum(1)


def craneo(cuerpo):
    """La cabeza del cuerpo en reposo (la malla de su GLB, en metros), para calzar los cascos: su piel con signo (la
    cabeza, el cuello y los hombros), la coronilla, la barbilla (donde el perfil de la cara se mete en el cuello), las
    cejas (CEJAS) y el centro del craneo: a la altura de las cejas, en medio de la frente y la nuca y de oreja a oreja.
    Las medidas, con rayos sobre la malla."""
    v, f = cuerpo.juego
    u = cuerpo.u
    cab = u["PI"][u["names"].index("head")]
    k = u["s"] / 1.85  # el tamano del cuerpo (el comun, 1)
    tri = v[f]
    cerca = (tri[:, :, 1] > cab[1] - 0.25 * k).all(1) & (np.abs(tri[:, :, [0, 2]] - cab[[0, 2]]) < 0.3 * k).all((1, 2))
    fc = f[cerca]
    alto = v[fc][:, :, 1].max()
    # el perfil de la cara en el plano medio, de a 2 mm: lo de mas adelante a cada altura
    ys = np.arange(cab[1] - 0.06 * k, alto, 0.002 * k)
    o = np.column_stack([np.full(len(ys), cab[0] + 1.0), ys, np.full(len(ys), cab[2])])
    xf = o[:, 0] - rayos(o, np.tile([-1.0, 0, 0], (len(ys), 1)), v, fc)
    nariz = int(np.argmax(np.where(np.isfinite(xf), xf, -np.inf)))
    corte = (xf[nariz] + xf[0]) / 2  # entre lo mas adelante de la cara y el frente del cuello
    i = nariz
    while i > 0 and xf[i - 1] > corte:
        i -= 1
    barbilla = ys[i]
    cejas = barbilla + CEJAS * (alto - barbilla)
    o = np.array([[cab[0] + 1, cejas, cab[2]], [cab[0] - 1, cejas, cab[2]], [cab[0], cejas, cab[2] + 1], [cab[0], cejas, cab[2] - 1]])
    t = rayos(o, np.array([[-1.0, 0, 0], [1, 0, 0], [0, 0, -1], [0, 0, 1]]), v, fc)
    frente, nuca, izq, der = o[0, 0] - t[0], o[1, 0] + t[1], o[2, 2] - t[2], o[3, 2] + t[3]
    centro = np.array([(frente + nuca) / 2, cejas, (izq + der) / 2])
    cuello = fc[(v[fc][:, :, 1] > barbilla - 0.03 * k).all(1)]  # la cabeza y el cuello, sin los hombros
    return dict(id=cuerpo.id, piel=Cabeza(v, cuello, centro), v=v, f=cuello, alto=alto, barbilla=barbilla, cejas=cejas, k=k,
                centro=centro, fondo=frente - nuca, ancho=izq - der, borde=barbilla + BORDE_CASCO * (alto - barbilla),
                cabeza=cab)


def girar_casco(o, f, az, rango=60.0):
    """El giro alrededor de +Y que pone el casco (malla de Props) de frente a +X: el de su render (az: de la malla a su
    render, una vista de tres cuartos de frente a +Z, y de ahi a +X) corregido por la simetria del casco de izquierda
    a derecha, dentro de rango grados: el giro que hace que el casco espejado de oreja a oreja caiga sobre si mismo.
    Devuelve el giro (3x3) y la correccion en grados."""
    q = muestras(o @ rot_y(90 + az).T, f, 4000)
    q -= q.mean(0)

    def asimetria(g):
        w = q @ rot_y(g).T
        m = w * [1.0, 1.0, -1.0]
        m[:, 2] += 2 * w[:, 2].mean()
        return cKDTree(w).query(m)[0].mean()
    gruesa = np.arange(-rango, rango + 0.5, 2.0)
    g0 = gruesa[np.argmin([asimetria(g) for g in gruesa])]
    fina = np.arange(g0 - 2.0, g0 + 2.01, 0.25)
    g = float(fina[np.argmin([asimetria(x) for x in fina])])
    return rot_y(90 + az + g), g


def golpes(o, d, v, f):
    """Todos los golpes de un rayo (origen o, direccion d) contra v[f]: las caras y sus distancias, de cerca a lejos.
    Un golpe justo en una arista o un vertice cuenta una vez (no una por cada cara que lo comparte)."""
    a = v[f[:, 0]]
    e1, e2 = v[f[:, 1]] - a, v[f[:, 2]] - a
    p = np.cross(d, e2)
    det = (e1 * p).sum(1)
    inv = 1.0 / np.where(np.abs(det) > 1e-12, det, np.inf)
    s = o - a
    u = (s * p).sum(1) * inv
    q = np.cross(s, e1)
    w = (d * q).sum(1) * inv
    t = (e2 * q).sum(1) * inv
    i = np.nonzero((u >= -BARI) & (w >= -BARI) & (u + w <= 1 + BARI) & (t > 1e-9))[0]
    i = i[np.argsort(t[i])]
    una = np.concatenate([[True], np.diff(t[i]) > 1e-7])[:len(i)]  # la misma distancia: el mismo golpe
    return i[una], t[i][una]


def abertura(v, f, cx, cz, zcol):
    """El tope de la abertura de la cara del casco (de frente a +X): con rayos hacia adelante desde el eje (cx, cz) a
    zcol de cada lado (los ojos), bajando desde arriba, la primera altura en que ya no golpea (aunque abajo haya
    nasal o carrilleras). La media de los dos lados; None si no hay abertura."""
    y1, y0 = v[:, 1].max(), v[:, 1].min()
    ys = np.arange(y1, y0, -(y1 - y0) / 300)
    hs = []
    for z in (cz + zcol, cz - zcol):
        o = np.column_stack([np.full(len(ys), cx), ys, np.full(len(ys), z)])
        golpe = np.isfinite(rayos(o, np.tile([1.0, 0, 0], (len(ys), 1)), v, f))
        if not golpe.any():
            return None
        i = int(np.argmax(golpe))
        if golpe[i:].all():
            return None
        j = i + int(np.argmax(~golpe[i:]))
        hs.append((ys[j - 1] + ys[j]) / 2)
    return float(np.mean(hs))


def centro_cupula(v, f, y, cx, cz):
    """El centro (x, z) del corte de la cara de adentro de la cupula a la altura y: rayos horizontales desde el eje,
    corrigiendolo hasta que quede en el medio de adelante-atras y de izquierda-derecha."""
    d = np.array([[1.0, 0, 0], [-1.0, 0, 0], [0, 0, 1.0], [0, 0, -1.0]])
    for _ in range(3):
        t = rayos(np.tile([cx, y, cz], (4, 1)), d, v, f)
        if not np.isfinite(t).all():
            break
        cx, cz = cx + (t[0] - t[1]) / 2, cz + (t[2] - t[3]) / 2
    return cx, cz


def capas_forro(v, f):
    """Los rayos verticales (de abajo hacia arriba) del medio de la cavidad de un casco derecho: en cada uno que pasa
    por una capa entera (una cara que mira abajo y otra arriba) debajo del techo de adentro, el punto mas alto de esa
    capa y el techo. Devuelve el eje de la copa (cx, cz), cuantos rayos golpean el casco y, por cada rayo con capa,
    (x, z, lo mas alto de la capa, el techo)."""
    fn, _ = normales_caras(v, f)
    lo, hi = v.min(0), v.max(0)
    arriba = v[v[:, 1] > lo[1] + 0.5 * (hi[1] - lo[1])]
    cx, cz = (arriba[:, 0].min() + arriba[:, 0].max()) / 2, (arriba[:, 2].min() + arriba[:, 2].max()) / 2
    r = 0.25 * min(np.ptp(arriba[:, 0]), np.ptp(arriba[:, 2]))
    capas, n = [], 0
    for dx in np.linspace(-r, r, 5):
        for dz in np.linspace(-r, r, 5):
            o = np.array([cx + dx, lo[1] - 1.0, cz + dz])
            i, t = golpes(o, np.array([0, 1.0, 0]), v, f)
            abajo = np.nonzero(fn[i, 1] < 0)[0]
            if len(i) < 2 or not len(abajo):
                continue
            n += 1
            if abajo.max() >= 2:  # una capa entera debajo del techo de adentro
                capas.append((o[0], o[2], o[1] + t[:abajo.max()].max(), o[1] + t[abajo.max()]))
    return cx, cz, n, np.array(capas, float).reshape(-1, 4)


def pendiente_forro(v, f):
    """Si la reconstruccion cerro la boca del casco (de frente a +X, derecho) con un forro (como en forro_de): cuanto
    sube la boca de atras hacia adelante, en grados (el plano por lo mas alto del forro). None si no tiene forro."""
    _, _, n, capas = capas_forro(v, f)
    if len(capas) <= n / 2 or len(capas) < 6:
        return None
    a = np.linalg.lstsq(np.column_stack([capas[:, 0], capas[:, 1], np.ones(len(capas))]), capas[:, 2], rcond=None)[0][0]
    return float(np.degrees(np.arctan(a)))


def forro_de(v, f):
    """Si la reconstruccion cerro la boca del casco (de frente a +X, derecho) con un forro: una capa (una cara que
    mira abajo y otra arriba) debajo del techo de adentro en mas de la mitad de los rayos verticales del medio de la
    cavidad (capas_forro). Devuelve la funcion que dice que puntos caen en el (adentro del aro de la boca, medido justo
    arriba del forro, y debajo de lo mas alto del forro), o None. El forro queda adentro de la cabeza: no cuenta para
    la holgura y lo que de el queda adentro de la cabeza se quita (afinar_casco)."""
    lo, hi = v.min(0), v.max(0)
    cx, cz, n, capas = capas_forro(v, f)
    if len(capas) <= n / 2:
        return None
    tope = float(np.percentile(capas[:, 2], 90))
    th = np.radians(np.arange(0.0, 360.0, 5.0))
    y = tope + 0.4 * (float(np.median(capas[:, 3])) - tope)  # el radio de la copa, entre el forro y el techo
    radio = rayos(np.tile([cx, y, cz], (len(th), 1)), np.column_stack([np.cos(th), np.zeros(len(th)), np.sin(th)]), v, f)
    radio = np.where(np.isfinite(radio), radio, np.median(radio[np.isfinite(radio)]))
    th, radio = np.append(th, 2 * np.pi), np.append(radio, radio[0])

    def en_forro(p):
        ang = np.arctan2(p[:, 2] - cz, p[:, 0] - cx) % (2 * np.pi)
        return (np.hypot(p[:, 0] - cx, p[:, 2] - cz) < 0.97 * np.interp(ang, th, radio)) & (p[:, 1] < tope + 0.02 * (hi[1] - lo[1]))
    return en_forro


def escala_minima(q, piel, lo, hi, holgura=HOLGURA_CASCO):
    """La escala menor s en [lo, hi] con la que los puntos q(s) (una funcion) quedan a holgura de la piel o mas:
    subiendo de a 3 % desde lo (mas grande no siempre entra: las carrilleras llegan a los hombros) y despues fina
    entre la ultima que no entra y la primera que si. None si ninguna entra."""
    def entra(s):
        return piel.distancia(q(s))[0].min() >= holgura
    s = lo
    while s <= hi and not entra(s):
        s *= 1.03
    if s > hi:
        return None
    a, b = s / 1.03, s
    for _ in range(12):
        m = (a + b) / 2
        a, b = (a, m) if entra(m) else (m, b)
    return b


def calzar_casco(o, f, rec, cr, M_patron=None):
    """El casco (malla de Props) sobre la cabeza del comun en reposo (cr: su craneo). Devuelve la semejanza (4x4) de la
    malla al reposo y lo medido. El del patron (M_patron: de su malla al reposo, donde lo puso el usuario): ahi mismo,
    agrandado lo justo alrededor del centro del craneo para quedar a HOLGURA_CASCO. Los demas, con los pasos de la
    cupula:
    1. de frente a +X por su simetria (girar_casco) y derecho: el pitch de la reconstruccion, como el aguila del
       patron, que el usuario puso sin girar; si la reconstruccion cerro la boca con un forro, la boca nivelada (el
       plano del forro, pendiente_forro: el gorro de piel salio echado atras, con el frente alto y la nuca colgando);
    2. su borde de adelante (el tope de la abertura frente a los ojos) en el de la cabeza (cr["borde"]: sobre las
       cejas, donde quedo el del aguila);
    3. el centro de su cupula (el corte de su cara de adentro un cuarto del alto de la cupula arriba del borde) sobre
       el centro del craneo en X y Z;
    4. la escala menor, alrededor de ese punto, con HOLGURA_CASCO de la piel de la cabeza y el cuello en todo el
       casco (sin su forro, si la reconstruccion cerro la boca: queda adentro de la cabeza).
    Al final, si todos los rayos desde el centro del craneo hacia arriba de las cejas golpean el casco (que lo cubre)."""
    c = cr["centro"]
    piel = cr["piel"]
    if M_patron is not None:
        q = aplicar(M_patron, muestras(o, f, 6000))
        k = escala_minima(lambda s: (q - c) * s + c, piel, 1.0, 1.5)
        M = m4(t=c) @ m4(np.eye(3), s=k) @ m4(t=-c) @ M_patron
        info = dict(giro=0.0, pitch=0.0, forro=False, escala=float(np.cbrt(np.linalg.det(M[:3, :3]))), agrandado=k)
    else:
        R, giro = girar_casco(o, f, rec.get("az", 0))
        boca = pendiente_forro(o @ R.T, f)
        pitch = 0.0 if boca is None else -boca  # el pitch 0 es el de la boca, no el de la malla
        R = rot_z(pitch) @ R
        base = o @ R.T
        vr, fr = reducir(base, f, 3000)
        lo, hi = vr.min(0), vr.max(0)
        arriba = vr[vr[:, 1] > lo[1] + 0.5 * (hi[1] - lo[1])]
        cx, cz = (arriba[:, 0].min() + arriba[:, 0].max()) / 2, (arriba[:, 2].min() + arriba[:, 2].max()) / 2
        s0 = 0.17 / np.ptp(arriba[:, 2])  # la cupula, de oreja a oreja, a unos 17 cm: el tamano para empezar
        b = abertura(vr, fr, cx, cz, OJOS / s0)
        if b is None:
            raise SystemExit(f"{rec['id']}: no encuentro la abertura de la cara del casco")
        cx, cz = centro_cupula(vr, fr, b + 0.25 * (hi[1] - b), cx, cz)
        b = abertura(vr, fr, cx, cz, OJOS / s0)
        piv = np.array([cx, b, cz])
        en_forro = forro_de(base, f)
        q = muestras(base, f, 6000)
        if en_forro is not None:
            q = q[~en_forro(q)]
        destino = np.array([c[0], cr["borde"], c[2]])
        s = escala_minima(lambda s: (q - piv) * s + destino, piel, 0.6 * s0, 2.5 * s0)
        if s is None:
            raise SystemExit(f"{rec['id']}: ninguna escala lo deja a {HOLGURA_CASCO * 1000:.0f} mm de la cabeza")
        M = m4(t=destino) @ m4(np.eye(3), s=s) @ m4(t=-piv) @ m4(R)
        info = dict(giro=giro, pitch=pitch, forro=en_forro is not None, escala=s, en_forro=en_forro, R=R)
    w = aplicar(M, o)
    d = np.array([[0, 1.0, 0]] + [[np.cos(e) * np.cos(a), np.sin(e), np.cos(e) * np.sin(a)]
                                  for e in np.radians((25, 45, 65)) for a in np.radians(np.arange(0, 360, 20))])
    info["cubre"] = bool(np.isfinite(rayos(np.tile(c, (len(d), 1)), d, w, f)).all())
    return M, info


def afinar_casco(v, f, cr, holgura=HOLGURA_CASCO, forro=False):
    """El casco ya reducido (v: en su marco, el origen en el centro del craneo del comun) contra la cabeza del comun:
    sin las caras que quedan enteras adentro de la cabeza (no se ven: el forro que cerraba la boca) y agrandado lo
    justo alrededor del origen hasta que ningun vertice del craneo (la cabeza arriba de la barbilla) quede a menos de
    holgura del casco, mirando desde el centro, y ningun punto del casco a menos de holgura de la piel. Devuelve
    cuales caras quedan y el factor."""
    c = cr["centro"]
    piel = cr["piel"]
    p = v + c
    rel = v[f].mean(1)  # el centro de cada cara, desde el centro del craneo
    n = np.linalg.norm(rel, axis=1)
    quedan = n >= piel.radio(rel / np.maximum(n, 1e-12)[:, None]) - 0.002
    if forro:  # lo que quedo del forro cruza la cabeza: fuera toda cara con un vertice adentro
        nv = np.linalg.norm(v, axis=1)
        hondo = nv < piel.radio(v / np.maximum(nv, 1e-12)[:, None]) - 0.002
        quedan &= ~hondo[f].any(1)
    fq = f[quedan]
    q = puntos_casco(v, fq)
    k = escala_minima(lambda s: q * s + c, piel, 1.0, 1.3, holgura)
    if k is None:
        d, _ = piel.distancia(q + c)
        i = int(np.argmin(d))
        raise SystemExit(f"el casco reducido no queda a la holgura de la cabeza ni agrandado un 30 %: {d[i] * 1000:.1f} mm "
                         f"en {np.round(q[i] + c, 3).tolist()} (del centro del craneo {np.round(q[i], 3).tolist()})")
    k = max(1.0, k)
    for _ in range(40):
        if not penetran(v * k, fq, cr, holgura):
            return quedan, k
        k *= 1.005
    raise SystemExit("el casco reducido sigue tocando el craneo agrandado un 20 %")


def puntos_casco(v, f):
    """Los puntos con los que se mide la holgura de un casco: 8000 repartidos por area y sus vertices (los mismos en
    el calce, en el anclaje de cada cuerpo y en la verificacion: escalar no cambia cuales son)."""
    return np.concatenate([muestras(v, f, 8000), v[np.unique(f)]])


def penetran(v, f, cr, holgura=HOLGURA_CASCO):
    """Cuantos vertices del craneo (la piel arriba de la barbilla) quedan a menos de holgura del casco (v: en su marco,
    el origen en el centro del craneo), mirando desde el centro: el primer golpe del rayo hacia el vertice antes que el
    vertice mas la holgura."""
    c = cr["centro"]
    pv = cr["v"][np.unique(cr["f"])]
    pv = pv[pv[:, 1] > cr["barbilla"]]
    d = pv - c
    n = np.linalg.norm(d, axis=1)
    t = rayos(np.tile(c, (len(pv), 1)), d / n[:, None], v + c, f)
    return int((t < n + holgura).sum())


# ---------------------------------------------------------------- ajustes por analogia (lo que no esta en el patron)
def referencias(P, comun):
    """Lo que el patron dice de como se lleva cada forma, para las piezas que no estan en el (ajustar_torso): medido
    sobre el cuerpo comun en la T. (Los cascos, con la cupula sobre el craneo: calzar_casco.)"""
    out = {}
    # El chaleco: su borde de arriba, su ancho, su hondura, la proporcion de su alto y cuanto se mete en el torso (sobre
    # la cadera y sin los brazos: el chaleco del patron mismo tiene un 2 % de sus vertices adentro, en las sisas).
    o, _ = malla_props("chaleco_largo")
    p = aplicar(P.en_t("white_mesh(3)", comun), o)
    piel = Superficie(comun.u["m"], comun.u["idx"], n=120000)
    torso = piel_del_torso(comun)
    cadera = comun.u["PT"][comun.u["names"].index("thigh_l")][1]
    d, _ = torso.distancia(p[p[:, 1] > cadera])
    Mv = P.en_t("white_mesh(3)", comun)
    out["torso"] = dict(piel=piel, torso=torso, cadera=cadera, p05=np.percentile(d, 5), adentro=float((d < 0).mean()),
                        arriba=np.array([(p[:, 0].min() + p[:, 0].max()) / 2, p[:, 1].max(), (p[:, 2].min() + p[:, 2].max()) / 2]),
                        ancho=np.ptp(p[:, 0]), hondo=np.ptp(p[:, 2]), alto_ancho=Mv[1, 1] / Mv[0, 0])
    # ajustar_torso aplicado al chaleco no da justo el del patron: la diferencia, como holgura.
    Mt = ajustar_torso(o, comun, out["torso"])
    out["torso"]["holgura"] = float(np.mean(np.diag(Mv[:3, :3]) / np.diag(Mt[:3, :3])))
    return out


TORSO = ("pelvis", "spine_01", "spine_02", "spine_03", "neck_01", "clavicle_l", "clavicle_r")


def piel_del_torso(cuerpo):
    """La piel del torso del cuerpo en la T (las caras de la malla entera cuyos vertices mueven sobre todo la pelvis,
    la columna, el cuello o las claviculas): sin los brazos, que en la T cruzan las hombreras."""
    u = cuerpo.u
    dom = u["jf"][np.arange(len(u["jf"])), np.argmax(u["wf"], axis=1)]
    ok = np.isin(dom, [u["names"].index(n) for n in TORSO])
    return Superficie(u["m"], u["idx"][ok[u["idx"]].all(1)], n=120000)


def ajustar_torso(vr, cuerpo, ref):
    """La coraza (vr: como en su render, de frente a +Z) sobre el torso del cuerpo en la T, con el criterio del chaleco
    del patron (ref): el alto y el ancho en la misma proporcion que su render, el borde de arriba donde el del chaleco
    y el ancho y la hondura lo mas justos que no se metan en el torso mas que el chaleco (sobre la cadera, sin los
    brazos), con su holgura. Lo poco que se mete lo saca empujar, ya reducida. Devuelve la afin (4x4) de vr a la T."""
    piel = ref["torso"]
    c = vr - np.array([(vr[:, 0].min() + vr[:, 0].max()) / 2, vr[:, 1].max(), (vr[:, 2].min() + vr[:, 2].max()) / 2])
    k0 = ref["ancho"] / np.ptp(c[:, 0])
    kd0 = ref["hondo"] / np.ptp(c[:, 2])
    cs = c[np.random.default_rng(5).choice(len(c), min(3000, len(c)), replace=False)]
    def costo(kw, kd):
        S = np.array([k0 * kw, k0 * kw * ref["alto_ancho"], kd0 * kd])
        p = cs * S + ref["arriba"]
        # Solo el torso: lo de abajo de la cadera (el faldar) va suelto sobre los muslos, que en la T estan abiertos;
        # con su piel, los sigue.
        d, _ = piel.distancia(p[p[:, 1] > ref["cadera"]])
        adentro = (d < 0).mean()
        ok = adentro <= max(ref["adentro"] * 1.25, 0.01) and np.percentile(d, 5) >= ref["p05"] * 0.8
        return kw * kd + (0 if ok else 100 + adentro * 100), S
    mejor = min((costo(kw, kd) for kw in np.linspace(0.85, 1.6, 13) for kd in np.linspace(0.85, 1.9, 13)), key=lambda x: x[0])
    for paso in (0.03, 0.01):  # mas fino alrededor del mejor
        kw0, kd0_ = mejor[1][0] / k0, mejor[1][2] / kd0
        mejor = min([mejor] + [costo(kw0 + i * paso, kd0_ + j * paso) for i in range(-3, 4) for j in range(-3, 4)],
                    key=lambda x: x[0])
    S = mejor[1] * ref.get("holgura", 1.0)
    M = np.eye(4)
    M[:3, :3] = np.diag(S)
    M[:3, 3] = ref["arriba"] - S * np.array([(vr[:, 0].min() + vr[:, 0].max()) / 2, vr[:, 1].max(), (vr[:, 2].min() + vr[:, 2].max()) / 2])
    return M


# ---------------------------------------------------------------- hacer cada pieza
GENERADOR = "estepa tools/modelos/patron.py"


def de_esqueleto(cuerpo, comun, p, joints, weights):
    """Puntos de la T del comun a la T de otro cuerpo (la mujer): con sus huesos, cada uno donde esta el mismo hueso
    en el otro (sus medidas son las del mismo esqueleto: humanoide.armar)."""
    if cuerpo is comun:
        return p
    vh = np.column_stack([p, np.ones(len(p))])
    out = np.zeros_like(p)
    for k in range(joints.shape[1]):
        mats = np.stack([cuerpo.u["MT"][j] @ np.linalg.inv(comun.u["MT"][j]) for j in joints[:, k]])
        out += weights[:, k:k + 1] * np.einsum("nij,nj->ni", mats, vh)[:, :3]
    return out


def piel_juego(cuerpo):
    """La superficie del cuerpo del juego (su GLB, en reposo): lo que la prenda no debe atravesar."""
    if not hasattr(cuerpo, "_piel_juego"):
        cuerpo._piel_juego = Superficie(*cuerpo.juego, n=60000)
    return cuerpo._piel_juego


def sanear(v, f):
    """Una prenda reducida sin aletas ni caras dadas vuelta. Las cuadricas funden las dos paredes del cuero: quedan
    aristas de tres o cuatro caras (aletas) y caras al reves de sus vecinas, y el juego no dibuja las de espaldas (por
    ahi se ve el suelo). En cada arista de mas de dos caras quedan las dos mas grandes; despues, todas con el sentido
    de sus vecinas (lowres.orientar)."""
    import lowres
    f = np.asarray(f, np.int64)
    _, unica = np.unique(np.sort(f, axis=1), axis=0, return_index=True)
    f = f[np.sort(unica)]
    _, area = normales_caras(v, f)
    for _ in range(6):
        e = np.sort(np.concatenate([f[:, [0, 1]], f[:, [1, 2]], f[:, [2, 0]]]), axis=1)
        cara = np.tile(np.arange(len(f)), 3)
        _, inv, cuantas = np.unique(e, axis=0, return_inverse=True, return_counts=True)
        inv = inv.reshape(-1)
        malas = np.unique(inv[cuantas[inv] > 2])
        if not len(malas):
            break
        fuera = set()
        for k in malas:
            caras = cara[inv == k]
            fuera.update(caras[np.argsort(-area[caras])][2:].tolist())
        quedan = np.setdiff1d(np.arange(len(f)), np.fromiter(fuera, np.int64))
        f, area = f[quedan], area[quedan]
    return v, lowres.orientar(v, f)


def reducir_prenda(vT, fT, piel, cerca, tris, rendija, raja, raja_pocos):
    """Una prenda a tris triangulos, sin lo que mira al cuerpo (sin_caras_de_adentro), de las dos maneras:
    - abierta: sin lo de adentro primero y reducida despues (todo el presupuesto, para lo que se ve);
    - cerrada: reducida entera (con su cara de adentro) y sin lo de adentro despues, con el objetivo que haga entrar lo
      que queda en tris. La superficie abierta, sin nada que la sostenga de fondo, a veces se pliega al reducirla (el
      frente del chaleco quedaba con rendijas por donde se veia el cuerpo); la cerrada, a veces pierde un lado entero
      (la cana de una bota) o se raja (la costura de atras de la otra bota, con 360 triangulos).
    Cada una, saneada (sanear) y con sus agujeros (de contorno menor que rendija, o rajas: de ancho medio menor que
    raja, o que raja_pocos si tienen hasta POCOS vertices) tapados (tapar_rendijas). Queda la que menos se aparta de la
    prenda de Props sumando, cada uno sobre el menor de los dos: la distancia entre sus superficies (p95, en los dos
    sentidos: tambien ve lo que le falta) y lo mas que se aparta (desvio: las puas de la abierta, o una boca que la
    reduccion dejo en un triangulo y quedo tapada)."""
    fA = sin_caras_de_adentro(vT, fT, piel, cerca)
    candidatas = [reducir(vT, fA, tris)]
    objetivo = 2 * tris
    for _ in range(5):
        vR, fR = reducir(vT, fT, objetivo)
        fR = sin_caras_de_adentro(vR, fR, piel, cerca)
        if len(fR) <= tris:
            break
        objetivo = max(int(objetivo * tris / len(fR) * 0.97), 8)
    candidatas.append(compactar(vR, fR) if len(fR) <= tris else reducir(vR, fR, tris))
    candidatas = [tapar_rendijas(*sanear(*c), rendija, raja, POCOS, raja_pocos) for c in candidatas]
    fuente = Superficie(vT, fT, n=200000)
    ref = Superficie(vT, fA, n=20000).p
    arbol_ref = cKDTree(ref)

    def distancia(c):
        p = Superficie(c[0], c[1], n=20000).p
        return max(np.percentile(cKDTree(p).query(ref)[0], 95), np.percentile(arbol_ref.query(p)[0], 95))
    d = np.array([distancia(c) for c in candidatas])
    m = np.array([desvio(*c, fuente) for c in candidatas])
    nota = d / d.min() + m / m.min()
    k = int(np.argmin(nota))
    log(f"[patron]   reducida {('abierta', 'cerrada')[k]} (sobre la menor: p95 a la de Props {d[0] / d.min():.2f} y "
        f"{d[1] / d.min():.2f}; desvio {m[0] / m.min():.2f} y {m[1] / m.min():.2f})")
    return candidatas[k]


def tapar_rendijas(v, f, maximo, angosto=0.0, pocos=0, angosto_pocos=0.0):
    """Los agujeros que deja la reduccion de una prenda al sacarle lo de adentro (las dos paredes del cuero, fundidas en
    la reducida, se van con lo de adentro), tapados en abanico desde su centro y con el sentido de sus vecinas: sin
    ellos, en el juego se veia el suelo por la rendija (el talon y la costura de atras de una bota). Los lazos de borde
    de contorno menor que maximo y las rajas, aunque sean largas: de ancho medio menor que angosto, o menor que
    angosto_pocos si tienen pocos vertices o menos (la grieta de la costura de una bota, en dos triangulos largos de
    2 y 3 cm de ancho). La boca de la bota (un pentagono de 6 cm de ancho medio, como poco), el cuello, las sisas y la
    cintura quedan abiertos."""
    e = np.concatenate([f[:, [0, 1]], f[:, [1, 2]], f[:, [2, 0]]])
    dirigidas = set(map(tuple, e.tolist()))
    sig = {}
    for a, b in e.tolist():
        if (b, a) not in dirigidas:  # arista de borde: el agujero la recorre al reves
            sig.setdefault(b, []).append(a)
    nuevas, centros = [], []
    for s in list(sig):
        while sig[s]:  # cada lazo que sale de s (dos, si el borde es un ocho con un vertice comun)
            lazo, x = [s], sig[s].pop()
            while x != s and len(lazo) <= 64 and sig.get(x):
                lazo.append(x)
                x = s if s in sig[x] else sig[x][0]  # en un vertice con dos salidas, la que cierra el lazo
                sig[lazo[-1]].remove(x)
            if x != s or len(lazo) < 3:
                continue
            p = v[lazo]
            contorno = np.linalg.norm(p - np.roll(p, -1, axis=0), axis=1).sum()
            area = 0.5 * np.linalg.norm(np.cross(p - p.mean(0), np.roll(p, -1, axis=0) - p.mean(0)), axis=1).sum()
            ancho = 2 * area / contorno
            if contorno >= maximo and ancho >= angosto and (len(lazo) > pocos or ancho >= angosto_pocos):
                continue
            if len(lazo) == 3:
                nuevas.append(lazo)
            else:
                c = len(v) + len(centros)
                centros.append(p.mean(0))
                nuevas += [[lazo[i], lazo[(i + 1) % len(lazo)], c] for i in range(len(lazo))]
    if not nuevas:
        return v, f
    return np.concatenate([v, np.array(centros).reshape(-1, 3)]), np.concatenate([f, np.array(nuevas, dtype=f.dtype)])


RENDIJA = {"torso": 0.45, "botas": 0.35}  # m: el contorno de agujero mas largo que se tapa (tapar_rendijas)
RAJA = 0.015  # m: el ancho medio de un lazo de borde por debajo del cual es una raja y se tapa, aunque sea largo
POCOS, RAJA_POCOS = 5, 0.04  # m: un lazo de hasta 5 vertices es una raja si su ancho medio no llega a 4 cm


def desvio(v, f, fuente):
    """Lo mas que se aparta la reducida de la prenda de Props (fuente: su Superficie): en cada cara, sus esquinas, los
    medios de sus lados y su centro. Una pua (la grieta de la costura de una bota, estirada hacia afuera) pasa de los
    3 cm; lo demas, de 1 cm."""
    t = v[f]
    p = np.concatenate([t, (t + np.roll(t, 1, axis=1)) / 2, t.mean(1, keepdims=True)], axis=1)
    return float(fuente.arbol.query(p.reshape(-1, 3))[0].max())


def prenda(rec, it, P, cuerpos, refs):
    """Una coraza o unas botas: {sufijo: bytes del GLB} (una por cuerpo) y la caja del comun en reposo."""
    o, f = malla_props(rec["malla"])
    tex = Textura(rec["img"], rec.get("tono"), rec.get("mezcla", 1.0))
    com = cuerpos[""]
    R = rot_y(rec.get("az", 0))
    if rec["forma"] == "botas":  # cada bota, donde la puso el patron
        partes = []
        for nombre in rec["patron"]:
            ob, fb = pieza_suelta(o, f, len(P.mallas[nombre][0]))
            partes.append((ob, fb, P.en_t(nombre, com)))
    else:
        M = P.en_t(rec["patron"], com) if rec.get("patron") else \
            ajustar_torso(o @ R.T, com, refs["torso"]) @ m4(R)
        partes = [(o, f, M)]
    vT, fT, lado, base = [], [], [], 0
    for k, (ob, fb, M) in enumerate(partes):
        vT.append(aplicar(M, ob))
        fT.append(fb + base)
        lado.append(np.full(len(ob), k))
        base += len(ob)
    vT, fT, lado = np.concatenate(vT), np.concatenate(fT), np.concatenate(lado)
    s_com = com.u["s"]
    piel = refs["torso"]["piel"]
    cerca = (0.07 if rec["forma"] == "torso" else 0.05) / s_com
    tris = int(it["tris_max"]) - 2
    vR, fR, n = [], [], 0
    for ob, fb, M in partes:  # cada pieza (cada bota) con su parte del presupuesto: juntas, una se quedaba sin cana
        parte = meta = tris // len(partes)
        for _ in range(3):  # con los agujeros tapados, dentro del presupuesto
            a, b = reducir_prenda(aplicar(M, ob), fb, piel, cerca, meta, RENDIJA[rec["forma"]] / s_com, RAJA / s_com,
                                  RAJA_POCOS / s_com)
            if len(b) <= parte:
                break
            meta -= len(b) - parte
        vR.append(a)
        fR.append(b + n)
        n += len(a)
    vR, fR = np.concatenate(vR), np.concatenate(fR)
    # La UV: cada vertice reducido, de vuelta a su malla de Props (por la pieza de la que sale) y de frente.
    k = lado[cKDTree(vT).query(vR)[1]]
    fuente = np.zeros_like(vR)
    for i, (_, _, M) in enumerate(partes):
        fuente[k == i] = aplicar(np.linalg.inv(M), vR[k == i])
    uv, _ = tex.uv(fuente @ R.T, fR, np.array([0, 0, 1.0]), np.array([0, 1.0, 0]))
    jc, wc = pesos_de(vR, com.u["m"], com.u["jf"], com.u["wf"], fR)
    out, caja = {}, None
    for sufijo, cuerpo in cuerpos.items():
        vS = de_esqueleto(cuerpo, com, vR, jc.astype(int), wc)
        j, w = (jc, wc) if cuerpo is com or cuerpo.u["m"] is com.u["m"] else \
            pesos_de(vS, cuerpo.u["m"], cuerpo.u["jf"], cuerpo.u["wf"], fR)
        v = cuerpo.a_reposo(vS, j.astype(int), w)
        v = empujar(v, fR, piel_juego(cuerpo), HOLGURA)
        if rec["forma"] == "botas":  # el pie asomaba por una cara grande; en la coraza, el brazo en la sisa le sacaba aletas
            v = empujar_desde_piel(v, fR, piel_juego(cuerpo), HOLGURA)
        nor = normales_40(v, fR)
        out[sufijo] = glb_pieza(v, fR, nor, uv, tex.png, it["id"], GENERADOR, piel=(j[fR], w[fR], cuerpo))
        if sufijo == "":
            caja = (v.min(0), v.max(0))
        out.setdefault("_vistas", {})[sufijo] = (v, fR, uv)
    return out, caja, tex


def varilla(a, b, x, z, g):
    """Una varilla de seccion cuadrada (g de lado) de y = a a y = b, en (x, z): sus 4 lados (los extremos quedan
    dentro de lo que une), hacia afuera."""
    sx, sz = np.array([-1, 1, 1, -1]) * g / 2, np.array([-1, -1, 1, 1]) * g / 2
    v = np.array([[x + sx[i], y, z + sz[i]] for y in (a, b) for i in range(4)])
    f = []
    for i in range(4):
        j = (i + 1) % 4
        for t in ((i, j, 4 + j), (i, 4 + j, 4 + i)):
            n = np.cross(v[t[1]] - v[t[0]], v[t[2]] - v[t[0]])
            afuera = v[list(t)].mean(0) - np.array([x, (a + b) / 2, z])
            f.append(t if n @ afuera > 0 else (t[0], t[2], t[1]))
    return v, np.array(f, dtype=np.int64)


def extruida(v, f, tris, minimo=0.012, cuerda=False):
    """Lo plano (las astas, el arco), con pocos triangulos: las cuadricas aplastaban lo fino (la cabeza de la
    alabarda, de 34 a 7 cm; la curva del arco; el asta de la lanza, a una cinta). La silueta de la propia malla en su
    plano (YZ de su marco: la hoja; el grosor en X), su borde simplificado a lo que cabe en tris y extruida con el
    grosor tipico de la malla (no menos que minimo), como piezas.plana. Con cuerda (el arco), la pala es el primer
    tramo de cada fila y la cuerda, lo de mas atras: una varilla aparte, de punta a punta. En el mismo marco que v."""
    from piezas import plana
    y0, y1 = v[:, 1].min(), v[:, 1].max()
    z0, z1 = v[:, 2].min(), v[:, 2].max()
    filas = 720
    s = (y1 - y0) / filas
    cols = int(np.ceil((z1 - z0) / s)) + 4
    # la silueta: columna c = (z1 - z) / s + 2 (crece hacia -Z, como piezas.plana), fila r = (y1 - y) / s
    p2 = np.column_stack([(z1 - v[:, 2]) / s + 2, (y1 - v[:, 1]) / s])
    mask = silueta(p2, f, cols, filas + 1)
    cz = None
    if cuerda:
        pala, centros = np.zeros_like(mask), []
        for r in np.flatnonzero(mask.any(axis=1)):
            d = np.diff(np.concatenate([[0], mask[r].astype(np.int8), [0]]))
            ini, fin = np.flatnonzero(d == 1), np.flatnonzero(d == -1)
            pala[r, ini[0]:fin[0]] = True
            if len(ini) > 1:
                centros.append((ini[-1] + fin[-1]) / 2)
        mask = pala
        cz = z1 - (float(np.median(centros)) - 2) * s
        tris -= 8
    # el grosor: en cada fila, lo que ocupa en X; el tipico (la mediana), no menos que minimo
    banda = np.clip(((y1 - v[:, 1]) / s).astype(int), 0, filas)
    ancho = np.zeros(filas + 1)
    for r in np.unique(banda):
        x = v[banda == r, 0]
        ancho[r] = np.ptp(x)
    t = max(float(np.median(ancho[ancho > 0])), minimo)
    pv, pf = plana((mask, None), (mask, None), (y1 - y0) * (mask.shape[0]) / filas, tris, grosor=t)
    # de piezas.plana (y desde el borde de abajo de la imagen, z = (W/2 - c) s, x en +-t/2) al marco
    H = mask.shape[0]
    xc = (v[:, 0].min() + v[:, 0].max()) / 2
    out = np.column_stack([pv[:, 0] + xc, pv[:, 1] + (y1 - H * s), pv[:, 2] - cols * s / 2 + z1 + 2 * s])
    if cz is not None:
        cv, cf = varilla(y0, y1, xc, cz, max(minimo / 2, 0.006))
        out, pf = np.concatenate([out, cv]), np.concatenate([pf, cf + len(out)])
    return out, pf


def rigido(rec, it, P, cuerpos, craneos):
    """Un casco, un arma, un escudo o un objeto: los bytes de su GLB (en su marco) y su caja. Los cascos, en el marco
    de la cabeza: el origen en el centro del craneo del comun (a la altura de las cejas), de frente a +X y +Y arriba,
    como su reposo (calzar_casco, afinar_casco); lo que dice el calce, en el registro."""
    o, f = malla_props(rec["malla"])
    tex = Textura(rec["img"], rec.get("tono"), rec.get("mezcla", 1.0))
    com, cr = cuerpos[""], craneos[""]
    if rec["forma"] == "casco":
        R = rot_y(rec.get("az", 0))  # al render (la textura sale de el)
        M_patron = com.hueso_reposo("head") @ P.en_t(rec["patron"], com) if rec.get("patron") else None
        M, info = calzar_casco(o, f, rec, cr, M_patron)
        C = m4(t=-cr["centro"]) @ M
    else:
        R = a_render(o, f, rec, tex)
        C = marco(o, f, rec, it, R)
    v = aplicar(C, o)
    if rec.get("plana"):
        vR, fR = extruida(v, f, int(it["tris_max"]) - 2, rec.get("minimo", 0.012), cuerda=rec["forma"] == "arco")
    else:
        vR, fR = reducir(v, f, int(it["tris_max"]) - 2)
        # Las cuadricas ponen algun vertice de una punta (la presilla de la aljaba) un poco afuera de la pieza: dentro
        # de su caja, que es la medida del inventario.
        vR = np.clip(vR, v.min(0), v.max(0))
    render = aplicar(np.linalg.inv(C), vR) @ R.T
    uv, _ = tex.uv(render, fR, np.array([0, 0, 1.0]), np.array([0, 1.0, 0]))
    nor = normales_40(vR, fR)
    if rec["forma"] == "casco":  # sin el forro, sin lo que queda adentro de la cabeza y agrandado lo justo
        if info.get("en_forro") is not None:  # el forro que cerraba la boca, fuera: el casco queda hueco
            cen = aplicar(np.linalg.inv(C), vR[fR].mean(1)) @ info["R"].T
            hueco = ~info["en_forro"](cen)
            fR, uv, nor = fR[hueco], uv[hueco], nor[hueco]
        quedan, k = afinar_casco(vR, fR, cr, forro=info.get("forro", False))
        fR, uv, nor, vR = fR[quedan], uv[quedan], nor[quedan], vR * k
        info.update(afinado=k, quitadas=int((~quedan).sum()))
        log(f"[patron] {rec['id']}: giro {info['giro']:+.2f} grados, "
            f"{'pitch ' + format(info['pitch'], '+.1f') + ' (la boca del forro, nivelada), ' if info['pitch'] else ''}"
            f"escala {info['escala'] * k:.4f}, "
            f"{'con forro (quitadas ' + str(info['quitadas']) + ' caras), ' if info['forro'] else ''}"
            f"afinado x{k:.3f}{', cubre el craneo' if info['cubre'] else ', NO CUBRE EL CRANEO'}")
    return glb_pieza(vR, fR, nor, uv, tex.png, it["id"], GENERADOR), (vR.min(0), vR.max(0)), (vR, fR, uv), tex


def marco_de(rec, it, tex=None):
    """El marco (4x4: de su malla de Props a el) de un objeto rigido que no es casco."""
    o, f = malla_props(rec["malla"])
    tex = tex or Textura(rec["img"])
    return marco(o, f, rec, it, a_render(o, f, rec, tex))


# ---------------------------------------------------------------- los anclajes
def anclaje_mano(cuerpo, lado):
    """Donde se empuna (el centro de la mano, un poco hacia la palma) y como: la hoja (+Y del objeto) hacia adelante
    (+X: el pulgar, con el brazo caido) y el filo (+Z) hacia abajo. En reposo, en metros."""
    u = cuerpo.u
    m = cuerpo.a_reposo(u["m"], u["jf"], u["wf"])
    huesos = [i for i, n in enumerate(u["names"]) if n == f"hand_{lado}" or (n.endswith(f"_{lado}") and any(
        n.startswith(d) for d in ("thumb", "index", "middle", "ring", "pinky")))]
    peso = np.isin(u["jf"], huesos) * u["wf"]
    mano = m[peso.sum(1) > 0.6]
    c = mano.mean(0)
    c[2] -= np.sign(c[2]) * 0.012  # hacia la palma (que mira al cuerpo)
    Rm = np.array([[0, 1.0, 0], [0, 0, -1.0], [-1.0, 0, 0]])
    return m4(Rm, c)


def anclaje_antebrazo(cuerpo):
    """El escudo embrazado: su centro a mitad del antebrazo izquierdo, del lado de afuera, mirando afuera (-Z)."""
    u = cuerpo.u
    i, j = u["names"].index("lowerarm_l"), u["names"].index("hand_l")
    a, b = u["PI"][i], u["PI"][j]
    m = cuerpo.a_reposo(u["m"], u["jf"], u["wf"])
    brazo = m[((u["jf"] == i) * u["wf"]).sum(1) > 0.6]
    ab = b - a
    t = np.clip(((brazo - a) @ ab) / (ab @ ab), 0, 1)
    radio = np.median(np.linalg.norm(brazo - (a + t[:, None] * ab), axis=1))
    c = (a + b) / 2 + np.array([0, 0, -(radio + 0.012)])
    Rm = np.array([[0, 0, 1.0], [0, 1.0, 0], [-1.0, 0, 0]])
    return m4(Rm, c)


def anclajes(P, cuerpos, refs):
    """Para cada cuerpo y cada anclaje (ANCLAJES_DEF): el hueso y la matriz del marco del objeto al reposo del cuerpo.
    Los del patron: el objeto de esa pieza en su marco, donde lo puso el patron (sin su escala: lo que se lleva tiene
    la medida del inventario), llevado al reposo con su hueso. El de la cabeza queda en None: sale de los cascos ya
    hechos (anclaje_cabeza)."""
    items, _ = inventario.load()
    por_id = {it["id"]: it for it in items}
    com = cuerpos[""]
    tabla = {}
    for sufijo, cuerpo in cuerpos.items():
        fila = []
        for nombre, hueso, pieza in ANCLAJES_DEF:
            if nombre == "GEAR_HEAD":
                fila.append((hueso, None))
            elif pieza is None:
                fila.append((hueso, anclaje_mano(cuerpo, hueso[-1]) if hueso.startswith("hand") else anclaje_antebrazo(cuerpo)))
            else:
                rec = next(r for r in RECETAS if r.get("patron") == pieza)
                G = rigida(P.en_t(pieza, com) @ np.linalg.inv(marco_de(rec, por_id[rec["id"]])))
                if not (cuerpo is com or cuerpo.u["m"] is com.u["m"]):
                    i = cuerpo.u["names"].index(hueso)
                    G = cuerpo.u["MT"][i] @ np.linalg.inv(com.u["MT"][i]) @ G
                fila.append((hueso, rigida(cuerpo.hueso_reposo(hueso) @ G)))
        tabla[sufijo] = fila
    return tabla


def anclaje_cabeza(cr_com, cr, cascos):
    """GEAR_HEAD de un cuerpo: la matriz (4x4) del marco de los cascos (el origen en el centro del craneo del comun, a
    la altura de las cejas) al reposo del cuerpo: al centro de su craneo (tambien a la altura de sus cejas, asi el
    borde de adelante queda sobre ellas) y con la escala de cada casco (cascos: [(id, vertices, caras)] en su marco):
    la menor con la que queda a HOLGURA_CASCO de su piel, sin vertices de su craneo a menos de eso. El anclaje lleva
    la mayor (la del casco que mas pide: ninguno se mete en esa cabeza) y cada casco, la suya (GEAR_HELMET_FIT): con una
    sola, los demas le quedaban holgados. Las cabezas tienen el mismo giro en el reposo de todos los cuerpos: sin giro.
    El comun: su centro, escala 1. Devuelve la matriz, la escala del anclaje y {id: la escala de ese casco}."""
    if cr is cr_com:
        return m4(t=cr["centro"]), 1.0, {mid: 1.0 for mid, _, _ in cascos}
    c = cr["centro"]
    s0 = np.mean([(cr["alto"] - cr["cejas"]) / (cr_com["alto"] - cr_com["cejas"]), cr["ancho"] / cr_com["ancho"],
                  cr["fondo"] / cr_com["fondo"]])
    por_casco = {}
    for mid, v, f in cascos:
        q = puntos_casco(v, f)
        sc = escala_minima(lambda k: q * k + c, cr["piel"], 0.85 * s0, 1.5 * s0)
        if sc is None:
            raise SystemExit(f"{mid}: no queda a {HOLGURA_CASCO * 1000:.0f} mm de la cabeza de {cr['id']}")
        for _ in range(40):
            if not penetran(v * sc, f, cr):
                break
            sc *= 1.005
        por_casco[mid] = float(sc)
    s = max(por_casco.values())
    return m4(np.eye(3), c, s), float(s), por_casco


def escribir_anclajes(tabla, ajustes):
    """src/world/anclajes.inc: los anclajes de cada cuerpo (tabla) y la escala de cada casco en GEAR_HEAD de cada
    cuerpo, relativa a la del anclaje (ajustes: {sufijo del cuerpo: {id del casco: factor}})."""
    lineas = ["// Generado por tools/modelos/patron.py desde arte/fuentes/patron/model_equipped_01.glb: no editar a mano.",
              "// Para cada cuerpo (GEAR_BODIES) y cada anclaje (GearSocket): el hueso y la matriz que lleva el objeto, en su",
              "// marco (las armas: la empunadura en el origen, la hoja hacia +Y y el filo hacia +Z; los escudos: mirando a +X;",
              "// los cascos: el origen en el centro del craneo del cuerpo comun, a la altura de las cejas), a la pose de reposo",
              "// del cuerpo (la de su GLB), en metros. GEAR_HEAD: el centro del craneo de cada cuerpo y su escala (calzar_casco).",
              "static const GearSocketDef GEAR_SOCKETS[GEAR_BODY_COUNT][GEAR_SOCKET_COUNT] = {"]
    for sufijo, mid in CUERPOS:
        lineas.append(f"    {{ // {mid}")
        for (nombre, _, _), (hueso, M) in zip(ANCLAJES_DEF, tabla[sufijo]):
            n = ", ".join(f"{x:.5f}f" for x in M[:4].reshape(-1))
            lineas.append(f'        [{nombre}] = {{ "{hueso}", {{ {n} }} }},')
        lineas.append("    },")
    lineas.append("};")
    lineas += ["// GEAR_HEAD, casco por casco: su escala en cada cuerpo (GEAR_BODIES), relativa a la del anclaje (la del casco que",
               "// mas pide): la menor con la que queda a 2 mm de esa cabeza (anclaje_cabeza). Un casco que no esta, la del anclaje.",
               "static const GearHelmetFit GEAR_HELMET_FIT[] = {"]
    for mid in sorted(ajustes[""]):
        k = ", ".join(f"{ajustes[sufijo][mid]:.5f}f" for sufijo, _ in CUERPOS)
        lineas.append(f'    {{ "{mid}", {{ {k} }} }},')
    lineas.append("};")
    open(ANCLAJES, "w", encoding="utf-8").write("\n".join(lineas) + "\n")


# ---------------------------------------------------------------- el inventario
def marcar(it, caja, rec, nota_patron):
    """El id, importado y con su nota; lo que se viste, con la medida de su caja en el comun (ancho x alto x largo)."""
    filas = open(inventario.TSV, encoding="utf-8").read().split("\n")
    for i, fila in enumerate(filas):
        c = fila.split("\t")
        if c[0] != it["id"]:
            continue
        if rec["forma"] in ("torso", "botas", "casco"):
            lo, hi = caja
            c[3] = "x".join(f"{x:.2f}".rstrip("0").rstrip(".") for x in (hi[2] - lo[2], hi[1] - lo[1], hi[0] - lo[0]))
        c[5] = "importado"
        viejo = c[6].split(" Del patron (tools/modelos/patron.py)")[0].split("Del patron (tools/modelos/patron.py)")[0].rstrip()
        c[6] = (viejo + " " if viejo else "") + nota_patron
        filas[i] = "\t".join(c)
    open(inventario.TSV, "w", encoding="utf-8").write("\n".join(filas))


def nota(rec):
    de = f"la malla {rec['malla']} de arte/fuentes/props/modelos"
    if rec["forma"] in PRENDAS:
        lugar = "donde la puso el patron" if rec.get("patron") else "ajustada al torso con el criterio del chaleco del patron"
        return (f"Del patron (tools/modelos/patron.py): {de}, {lugar}, con la piel del cuerpo (una variante por cuerpo) "
                f"y la textura de su render ({rec['img']}).")
    if rec["forma"] == "casco":
        lugar = "donde la puso el patron" if rec.get("patron") else "apoyada en la cabeza con el criterio del casco del patron"
        return f"Del patron (tools/modelos/patron.py): {de}, {lugar}; en la cabeza (GEAR_HEAD) y con la textura de su render ({rec['img']})."
    return (f"Del patron (tools/modelos/patron.py): {de}, a la medida del inventario, con la textura de su render "
            f"({rec['img']}); se lleva en los anclajes de src/world/anclajes.inc.")


def manifiesto(rec):
    import hashlib
    from datetime import datetime, timezone
    ruta = os.path.join(ROOT, "arte", "modelos", "models_manifest.json")
    data = json.load(open(ruta, encoding="utf-8"))
    src = os.path.join(PROPS, "modelos", rec["malla"] + ".glb")
    data["models"][rec["id"]] = {
        "file": os.path.relpath(src, ROOT), "category": rec["id"].split(".")[0], "has_rig": rec["forma"] in PRENDAS,
        "source": {"tool": "patron", "url": "", "file": os.path.relpath(PATRON, ROOT),
                   "sha256": hashlib.sha256(open(src, "rb").read()).hexdigest()},
        "params": {k: v for k, v in rec.items() if k not in ("id",)},
        "updated_at": datetime.now(timezone.utc).isoformat(timespec="seconds")}
    json.dump(data, open(ruta, "w", encoding="utf-8"), ensure_ascii=False, indent=2)


def ruta_variante(path, sufijo):
    return path if not sufijo else path[:-4] + "@" + sufijo + ".glb"


def hacer(ids=None):
    items, errores = inventario.load()
    if errores:
        raise SystemExit("\n".join(errores))
    por_id = {it["id"]: it for it in items}
    P = Patron()
    cuerpos = armar_cuerpos()
    com = cuerpos[""]
    P.al_esqueleto(com)
    log(f"[patron] cuerpo del patron sobre el esqueleto: {P.error_cuerpo.max() * com.u['s'] * 1000:.2f} mm como mucho "
        f"({P.sueltos} vertices de la capa de adentro, sin par)")
    refs = referencias(P, com)
    craneos = {sufijo: craneo(c) for sufijo, c in cuerpos.items()}
    for sufijo, cr in craneos.items():
        cab = cr["cabeza"]
        log(f"[patron] craneo de {cr['id']} (desde el pivote head, en mm): coronilla +{(cr['alto'] - cab[1]) * 1000:.0f}, "
            f"cejas +{(cr['cejas'] - cab[1]) * 1000:.0f}, ancho {cr['ancho'] * 1000:.0f}, fondo {cr['fondo'] * 1000:.0f}; centro "
            f"{np.round(cr['centro'], 4).tolist()}")
    tabla = anclajes(P, cuerpos, refs)
    hechos = {}
    for rec in RECETAS:
        if ids and rec["id"] not in ids:
            continue
        it = por_id[rec["id"]]
        ruta = os.path.join(ROOT, inventario.path_of(it))
        os.makedirs(os.path.dirname(ruta), exist_ok=True)
        if rec["forma"] in PRENDAS:
            out, caja, _ = prenda(rec, it, P, cuerpos, refs)
            for sufijo, _ in CUERPOS:
                open(ruta_variante(ruta, sufijo), "wb").write(out[sufijo])
            hechos[rec["id"]] = out["_vistas"]
        else:
            datos, caja, malla, _ = rigido(rec, it, P, cuerpos, craneos)
            open(ruta, "wb").write(datos)
            hechos[rec["id"]] = {"": malla}
        marcar(it, caja, rec, nota(rec))
        manifiesto(rec)
        lo, hi = caja
        log(f"[patron] {rec['id']}: {hi[2] - lo[2]:.2f} x {hi[1] - lo[1]:.2f} x {hi[0] - lo[0]:.2f} m (ancho x alto x largo)")
    # La cabeza de cada cuerpo, con todos los cascos (los recien hechos y los que ya estaban)
    cascos = []
    for rec in RECETAS:
        if rec["forma"] == "casco":
            v, f, _, _, _ = leer_pieza(os.path.join(ROOT, inventario.path_of(por_id[rec["id"]])))
            cascos.append((rec["id"], v, f))
    ajustes = {}
    for sufijo, _ in CUERPOS:
        M, s, por_casco = anclaje_cabeza(craneos[""], craneos[sufijo], cascos)
        tabla[sufijo][0] = ("head", M)
        ajustes[sufijo] = {mid: k / s for mid, k in por_casco.items()}
        log(f"[patron] GEAR_HEAD de {craneos[sufijo]['id']}: el centro del craneo, escala {s:.4f} (cada casco, de "
            f"{min(por_casco.values()):.4f} a {s:.4f})")
    escribir_anclajes(tabla, ajustes)
    log(f"[patron] {os.path.relpath(ANCLAJES, ROOT)}: {len(ANCLAJES_DEF)} anclajes x {len(cuerpos)} cuerpos")
    return hechos, cuerpos, tabla


def lamina(imagenes, ruta):
    w = sum(i.width for i in imagenes)
    h = max(i.height for i in imagenes)
    out = Image.new("RGB", (w, h), (236, 236, 236))
    x = 0
    for i in imagenes:
        out.paste(i, (x, 0))
        x += i.width
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    out.save(ruta)
    return ruta


# ---------------------------------------------------------------- vistas de control
def leer_pieza(path):
    """Un GLB del juego, para mirarlo: vertices, caras y el color de cada cara (su textura en el centro de su UV, o el
    color de sus vertices)."""
    doc, b = leer(path)
    pr = doc["meshes"][0]["primitives"][0]
    a = pr["attributes"]
    v = acc(doc, b, a["POSITION"]).astype(float)
    f = acc(doc, b, pr["indices"]).astype(np.int64).reshape(-1, 3) if "indices" in pr else np.arange(len(v)).reshape(-1, 3)
    if doc.get("images") and "TEXCOORD_0" in a:
        bv = doc["bufferViews"][doc["images"][0]["bufferView"]]
        o = bv.get("byteOffset", 0)
        img = np.asarray(Image.open(io.BytesIO(b[o:o + bv["byteLength"]])).convert("RGB"), float)
        uv = acc(doc, b, a["TEXCOORD_0"]).astype(float)[f]
        return v, f, np.tile([180.0, 180.0, 180.0], (len(f), 1)), uv, img
    if "COLOR_0" in a:
        c = acc(doc, b, a["COLOR_0"]).astype(float)
        col = (c[:, :3] * (255.0 if c.max() <= 1.0 else 1.0))[f].mean(1)
    else:
        col = np.tile([180.0, 180.0, 180.0], (len(f), 1))
    return v, f, col, None, None


def leer_anclajes():
    """La tabla de src/world/anclajes.inc: {sufijo del cuerpo: {nombre del anclaje: matriz 4x4}}."""
    import re
    txt = open(ANCLAJES, encoding="utf-8").read()
    out, cuerpo = {}, None
    for linea in txt.split("\n"):
        m = re.match(r"\s*\{ // (\S+)", linea)
        if m:
            cuerpo = next(s for s, mid in CUERPOS if mid == m.group(1))
            out[cuerpo] = {}
        m = re.match(r'\s*\[(\w+)\] = \{ "(\w+)", \{ ([^}]*) \} \},', linea)
        if m and cuerpo is not None:
            M = np.array([float(x.rstrip("f")) for x in m.group(3).split(",")]).reshape(4, 4)
            out[cuerpo][m.group(1)] = M
    return out


def leer_ajustes_casco():
    """GEAR_HELMET_FIT de src/world/anclajes.inc: {id del casco: {sufijo del cuerpo: su escala relativa al anclaje}}."""
    import re
    partes = open(ANCLAJES, encoding="utf-8").read().split("GEAR_HELMET_FIT[] = {")
    out = {}
    for m in re.finditer(r'\{ "([\w.]+)", \{ ([^}]*) \} \},', partes[1] if len(partes) > 1 else ""):
        out[m.group(1)] = {sufijo: float(x.rstrip("f")) for (sufijo, _), x in zip(CUERPOS, m.group(2).split(","))}
    return out


# Lo que lleva el patron, guardado y empunado (con la mano: el sable en la derecha, el escudo en el antebrazo).
GUARDADO = (("armadura.torso.laminar_cuero", None), ("armadura.botas.laminar_cuero", None), ("armadura.casco.oro_padre", "GEAR_HEAD"),
            ("escudo.mano.cuero", "GEAR_BACK_SHIELD"), ("utileria.objeto.aljaba", "GEAR_BACK_QUIVER"),
            ("arma.distancia.arco_simple", "GEAR_BACK_LONG"), ("arma.corta.sable", "GEAR_HIP_LEFT"), ("arma.corta.daga", "GEAR_HIP_RIGHT"))
EMPUNADO = (("armadura.torso.laminar_cuero", None), ("armadura.botas.laminar_cuero", None), ("armadura.casco.oro_padre", "GEAR_HEAD"),
            ("escudo.mano.cuero", "GEAR_FOREARM_LEFT"), ("utileria.objeto.aljaba", "GEAR_BACK_QUIVER"),
            ("arma.distancia.arco_simple", "GEAR_BACK_LONG"), ("arma.corta.sable", "GEAR_HAND_RIGHT"), ("arma.corta.daga", "GEAR_HIP_RIGHT"))


def ver(kits=None, cuerpos_ver=("", "mujer", "gigante", "protagonista"), vistas=("frente", "lado", "atras", "tres_cuartos")):
    """Cada cuerpo del juego en reposo con lo puesto (kits: [(nombre, [(id, anclaje o None si es prenda)])]), desde
    varias camaras: build/patron/<cuerpo>_<kit>.png."""
    items, _ = inventario.load()
    por_id = {it["id"]: it for it in items}
    tabla = leer_anclajes()
    kits = kits or (("guardado", GUARDADO), ("empunado", EMPUNADO))
    rutas = []
    for sufijo in cuerpos_ver:
        mid = next(m for s, m in CUERPOS if s == sufijo)
        cuerpo = leer_pieza(os.path.join(ROOT, inventario.path_of(por_id[mid])))
        for nombre, kit in kits:
            objetos = [cuerpo]
            for iid, anclaje in kit:
                path = os.path.join(ROOT, inventario.path_of(por_id[iid]))
                if anclaje is None:
                    path = ruta_variante(path, sufijo)
                if not os.path.isfile(path):
                    continue
                v, f, col, uv, img = leer_pieza(path)
                if anclaje is not None:
                    v = aplicar(tabla[sufijo][anclaje], v)
                objetos.append((v, f, col, uv, img))
            ims = [render(objetos, VISTA[vista], 640, 360)[0] for vista in vistas]
            rutas.append(lamina(ims, os.path.join(VISTAS, f"{sufijo or 'comun'}_{nombre}.png")))
    return rutas


def medir():
    P = Patron()
    cuerpos = armar_cuerpos()
    com = cuerpos[""]
    P.al_esqueleto(com)
    s = com.u["s"]
    log(f"el cuerpo del patron es rigged-model.glb en su pose de piel (la T): a {P.error_cuerpo.max() * s * 1000:.2f} mm como mucho")
    piel = Superficie(com.u["m"], com.u["idx"], n=120000)
    for nombre, fuente in PIEZAS.items():
        M = P.en_t(nombre, com)
        L = M[:3, :3] * s
        o, f = malla_props(fuente)
        if fuente == "botas_montar":
            o, f = pieza_suelta(o, f, len(P.mallas[nombre][0]))
        p = aplicar(M, o)
        d, _ = piel.distancia(p)
        log(f"{nombre:18s} <- {fuente:17s} error {P.error[nombre].max() * 1000:.3f} (patron x1000); escalas por eje "
            f"{np.round(np.linalg.svd(L)[1], 4).tolist()}; caja {np.round(np.ptp(p, 0) * s, 3).tolist()} m; a la piel "
            f"p05 {np.percentile(d, 5) * s * 1000:.0f} mm, mediana {np.median(d) * s * 1000:.0f} mm, adentro {(d < 0).mean() * 100:.1f} %")


def verificar_cascos():
    """Cada casco (su GLB) puesto en cada cuerpo en reposo como lo pone el juego (src/world/anclajes.inc: GEAR_HEAD y
    la escala de ese casco, GEAR_HELMET_FIT): la holgura menor a la piel de la cabeza y el cuello, los vertices del
    craneo a menos de HOLGURA_CASCO del casco (mirando desde el centro del craneo), el techo de adentro sobre la
    coronilla y el borde de adelante sobre las cejas. Devuelve cuantos fallan (holgura menor que HOLGURA_CASCO o algun
    vertice del craneo adentro)."""
    items, _ = inventario.load()
    por_id = {it["id"]: it for it in items}
    cuerpos = armar_cuerpos(log=lambda *_: None)
    tabla = leer_anclajes()
    ajustes = leer_ajustes_casco()
    fallan = 0
    log(f"{'casco':32s} {'cuerpo':32s} holgura  craneo  techo  borde   (mm; craneo: vertices a menos de "
        f"{HOLGURA_CASCO * 1000:.0f} mm; techo: sobre la coronilla; borde: sobre las cejas)")
    for sufijo, mid in CUERPOS:
        cr = craneo(cuerpos[sufijo])
        c = cr["centro"]
        for rec in [r for r in RECETAS if r["forma"] == "casco"]:
            k = ajustes.get(rec["id"], {}).get(sufijo, 1.0)
            M = tabla[sufijo]["GEAR_HEAD"] @ m4(np.eye(3), s=k)
            s = float(np.cbrt(np.linalg.det(M[:3, :3])))
            v, f, _, _, _ = leer_pieza(os.path.join(ROOT, inventario.path_of(por_id[rec["id"]])))
            w = aplicar(M, v)
            d = cr["piel"].distancia(puntos_casco(w, f))[0].min()
            n = penetran(w - c, f, cr)
            techo = c[1] + rayos(c[None], np.array([[0, 1.0, 0]]), w, f)[0]
            borde = abertura(w, f, c[0], c[2], OJOS * s)
            malo = d < HOLGURA_CASCO - 1e-4 or n > 0
            fallan += malo
            log(f"{rec['id']:32s} {mid:32s} {d * 1000:7.1f} {n:7d} {(techo - cr['alto']) * 1000:6.0f} "
                f"{(borde - cr['cejas']) * 1000 if borde is not None else float('nan'):6.0f}{'  FALLA' if malo else ''}")
    return fallan


def main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("medir")
    sub.add_parser("cascos", help="cada casco en cada cuerpo: holgura, vertices del craneo adentro, techo y borde")
    h = sub.add_parser("hacer")
    h.add_argument("ids", nargs="*")
    v = sub.add_parser("ver")
    v.add_argument("ids", nargs="*")
    n = sub.add_parser("normales", help="las normales a 40 grados de los cuerpos y los animales ya hechos (o de los GLB dados)")
    n.add_argument("rutas", nargs="*")
    a = ap.parse_args(argv)
    if a.cmd == "normales":
        import glob
        rutas = a.rutas or sorted(glob.glob(os.path.join(ROOT, "assets", "models", "personaje", "**", "*.glb"), recursive=True) +
                                  glob.glob(os.path.join(ROOT, "assets", "models", "animal", "**", "*.glb"), recursive=True))
        for r in rutas:
            log(f"[patron] {os.path.relpath(r, ROOT)}: {reescribir_normales(r)} normales")
        return
    if a.cmd == "cascos":
        if verificar_cascos():
            raise SystemExit(1)
    elif a.cmd == "medir":
        medir()
    elif a.cmd == "hacer":
        hacer(set(a.ids) or None)
        print("Sigue: python3 tools/assets/inventario.py doc && python3 tools/modelos/patron.py ver")
    else:
        for r in ver():
            print(r)


if __name__ == "__main__":
    main()
