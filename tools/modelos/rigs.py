#!/usr/bin/env python3
"""Los rigs de referencia de los animales, llevados a otras mallas.

El usuario subio a IMPORTED/Better animals/Modelos tres rigs hechos (en arte/fuentes/animales/rigs):
- caballo.glb: un burro con el esqueleto de caballo (56 huesos: la columna en seis, la cabeza con orejas, hocico y
  boca, cada pata en seis y la cola en cuatro) y sus clips (Idle, Walk, Trot, Run, Eating, Rear, Kick, Death...);
- hiena.glb: la hiena con su esqueleto (49 huesos) y sus clips (Idle, Sneak, Run, Bite, Howl, Fetch, Death...);
- aguila.glb: el esqueleto de un ave y sus clips (Flap, Glide, Idle, Walk), sin malla.

Llevar uno de ellos a otra malla (llevar):
1. Las marcas de las dos mallas (marcas): el pie, la rodilla y la cima de cada pata, la cadera, el lomo, el pecho, el
   cuello, la nuca, el hocico, la cola, el lomo por arriba (sobre la cadera y sobre los hombros), el vientre y los
   costados (animales.medir y animales.armar_esqueleto).
2. Un warp de placas delgadas (thin plate spline) de las marcas de la referencia a las de la malla nueva: cada hueso
   va a donde el warp lleva el suyo.
3. Los pesos: la malla de referencia, deformada con el mismo warp; cada vertice nuevo toma los del punto mas cercano de
   esa superficie.
4. Los clips: cada hueso guarda la orientacion del de referencia (solo cambian los largos), asi que las rotaciones
   locales de cada clip sirven tal cual. La traslacion de la cadera va a la escala de su altura, y las marchas, a su
   ritmo: el tiempo, por la raiz del cociente de las alturas de la cadera (el mismo numero de Froude).
Los clips del juego (assets/animaciones.tsv) salen de los de la referencia con MAPAS: uno tal cual, un trozo que
vuelve al principio ("trozo"), uno mas lento ("despacio"), en la hiena, el paso desde el acecho, erguido ("erguido":
sin lo agachado, el ciclo de las patas sobre la pose de reposo) y, en el caballo, pastar ("pastar"): el Eating de la
referencia abre las patas de adelante de costado, como una jirafa que bebe (el burro tiene el cuello corto), asi que
pastar es el cuerpo del Idle con la boca, las orejas y la cola del Eating, y el cuello bajado hasta que la boca toca el
suelo (cabeza_al_suelo).
"""
import json
import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import animales  # noqa: E402
import patron  # noqa: E402

ROOT = patron.ROOT
RIGS = os.path.join(ROOT, "arte", "fuentes", "animales", "rigs")
FPS = 30.0
# Del marco de los archivos (el frente a +Z, la izquierda a +X) al del juego (el frente a +X, la izquierda a -Z).
G = np.array([[0.0, 0.0, 1.0], [0.0, 1.0, 0.0], [-1.0, 0.0, 0.0]])
# Los clips del juego desde los de cada referencia: el nombre de uno, ("trozo", clip, desde, hasta): ese tramo y de
# vuelta a su primer cuadro en lo mismo; ("despacio", clip, factor): mas largo; ("erguido", clip, reposo, factor): el
# ciclo de las patas de clip sobre el primer cuadro de reposo, sin lo agachado; ("pastar", cuerpo, comer): el cuerpo de
# un clip con la boca, las orejas y la cola de otro, y la cabeza al suelo (cabeza_al_suelo).
MAPAS = {
    "caballo": {"idle": "Idle", "montado_idle": "Idle", "pastar": ("pastar", "Idle", "Eating"), "caminar": "Walk", "trotar": "Trot",
                "galopar": "Run", "encabritarse": "Rear", "forcejear_lazo": "Rear", "atacar": "Kick", "morir": "Death",
                "tirar_carga": "Walk", "recibir_golpe": ("trozo", "Rear", 0.0, 0.3)},
    "hiena": {"idle": "Idle", "caminar": ("erguido", "Sneak", "Idle", 0.7), "trotar": ("despacio", "Run", 1.45),
              "galopar": "Run", "acechar": "Sneak", "atacar": "Bite", "aullar": "Howl", "comer_presa": "Fetch",
              "pastar": "Fetch", "encabritarse": "Bite", "forcejear_lazo": "Idle_Alert", "morir": "Death",
              "recibir_golpe": ("trozo", "Death", 0.0, 0.25)},
}
MARCHAS = {"caminar", "trotar", "galopar", "acechar", "tirar_carga"}  # los que van al ritmo de las patas
# Los huesos de la cabeza que se pliegan en ella (sus pesos, a la cabeza) en los que tienen cuernos: las orejas de la
# referencia caen donde la malla nueva tiene los cuernos, que no se mueven.
OREJAS = ("ear", "Ear")
# Los que en el caballo de referencia cuelgan de la base del cuello (spine_4) y van al tronco (hombros_al_tronco): asi
# el cuello baja solo, sin llevarse las patas de adelante ni el pecho.
AL_TRONCO = ("front_scapula_l", "front_scapula_r", "ribcage_front")


# ------------------------------------------------------------------ cuaterniones (x, y, z, w), en arreglos
def qmul(a, b):
    x1, y1, z1, w1 = np.moveaxis(np.asarray(a, float), -1, 0)
    x2, y2, z2, w2 = np.moveaxis(np.asarray(b, float), -1, 0)
    return np.stack([w1 * x2 + x1 * w2 + y1 * z2 - z1 * y2, w1 * y2 - x1 * z2 + y1 * w2 + z1 * x2,
                     w1 * z2 + x1 * y2 - y1 * x2 + z1 * w2, w1 * w2 - x1 * x2 - y1 * y2 - z1 * z2], axis=-1)


def qconj(q):
    return np.asarray(q, float) * np.array([-1.0, -1.0, -1.0, 1.0])


def q_a_m(q):
    x, y, z, w = np.moveaxis(np.asarray(q, float), -1, 0)
    return np.stack([np.stack([1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)], -1),
                     np.stack([2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)], -1),
                     np.stack([2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)], -1)], -2)


def m_a_q(R):
    """El cuaternion de una rotacion 3x3."""
    R = np.asarray(R, float)
    t = np.trace(R)
    if t > 0:
        s = 2.0 * np.sqrt(t + 1.0)
        q = [(R[2, 1] - R[1, 2]) / s, (R[0, 2] - R[2, 0]) / s, (R[1, 0] - R[0, 1]) / s, 0.25 * s]
    elif R[0, 0] > R[1, 1] and R[0, 0] > R[2, 2]:
        s = 2.0 * np.sqrt(1.0 + R[0, 0] - R[1, 1] - R[2, 2])
        q = [0.25 * s, (R[0, 1] + R[1, 0]) / s, (R[0, 2] + R[2, 0]) / s, (R[2, 1] - R[1, 2]) / s]
    elif R[1, 1] > R[2, 2]:
        s = 2.0 * np.sqrt(1.0 + R[1, 1] - R[0, 0] - R[2, 2])
        q = [(R[0, 1] + R[1, 0]) / s, 0.25 * s, (R[1, 2] + R[2, 1]) / s, (R[0, 2] - R[2, 0]) / s]
    else:
        s = 2.0 * np.sqrt(1.0 + R[2, 2] - R[0, 0] - R[1, 1])
        q = [(R[0, 2] + R[2, 0]) / s, (R[1, 2] + R[2, 1]) / s, 0.25 * s, (R[1, 0] - R[0, 1]) / s]
    q = np.array(q)
    return q / np.linalg.norm(q)


def nlerp(a, b, u):
    """De a a b (por el camino corto) en u: los cuadros van a 30 por segundo, asi que basta con normalizar."""
    a, b = np.asarray(a, float), np.asarray(b, float)
    b = np.where((np.sum(a * b, -1) < 0)[..., None], -b, b)
    q = a * (1 - u) + b * u
    return q / np.linalg.norm(q, axis=-1, keepdims=True)


def q_media(Q):
    """El cuaternion medio de Q (cuadros x 4), alineados al primero."""
    Q = np.where((Q @ Q[0] < 0)[:, None], -Q, Q)
    m = Q.mean(0)
    return m / np.linalg.norm(m)


def trs(t, q):
    M = np.eye(4)
    M[:3, :3] = q_a_m(q)
    M[:3, 3] = t
    return M


# ------------------------------------------------------------------ leer un rig
class Rig:
    """Un rig: los huesos (nombres, padres, la matriz de bind de cada uno en el mundo), la malla en bind con sus pesos
    (cuatro huesos por vertice) y los clips: {nombre: {t, R (cuadros x huesos x 4: la rotacion local de cada hueso),
    T (cuadros x 3: la traslacion local de la cadera)}}."""


def _muestrear(doc, b, smp, t):
    ti = patron.acc(doc, b, smp["input"]).reshape(-1).astype(float)
    vo = patron.acc(doc, b, smp["output"]).astype(float).reshape(len(ti), -1)
    if smp.get("interpolation") == "CUBICSPLINE":  # tangente de entrada, valor, tangente de salida
        vo = vo.reshape(len(ti), 3, -1)[:, 1]
    if len(ti) == 1:
        return np.repeat(vo, len(t), axis=0), None
    i1 = np.clip(np.searchsorted(ti, t, side="right"), 1, len(ti) - 1)
    i0 = i1 - 1
    u = np.clip((t - ti[i0]) / np.maximum(ti[i1] - ti[i0], 1e-9), 0.0, 1.0)[:, None]
    if smp.get("interpolation") == "STEP":
        u = np.zeros_like(u)
    return vo[i0], (vo[i1], u)


def leer(ruta):
    """El rig de un GLB riggeado (como lo dejan sus matrices de bind: los TRS de sus nodos pueden estar en otra pose)."""
    doc, b = patron.leer(ruta)
    nodes = doc["nodes"]
    skin = doc["skins"][0]
    J = skin["joints"]
    k_de = {n: k for k, n in enumerate(J)}
    padre_nodo = {c: i for i, nd in enumerate(nodes) for c in nd.get("children", [])}
    r = Rig()
    r.nombres = [nodes[n].get("name", f"hueso_{k}") for k, n in enumerate(J)]
    r.padres = np.array([k_de.get(padre_nodo.get(n), -1) for n in J])
    ibm = patron.acc(doc, b, skin["inverseBindMatrices"]).reshape(-1, 4, 4).transpose(0, 2, 1)
    r.bind = np.linalg.inv(ibm)
    pr = doc["meshes"][0]["primitives"][0]
    r.v = patron.acc(doc, b, pr["attributes"]["POSITION"]).astype(float)
    r.f = patron.acc(doc, b, pr["indices"]).reshape(-1, 3).astype(np.int64)
    r.juntas = patron.acc(doc, b, pr["attributes"]["JOINTS_0"]).astype(np.int64)
    r.pesos = patron.acc(doc, b, pr["attributes"]["WEIGHTS_0"]).astype(float)
    r.pesos /= np.maximum(r.pesos.sum(1, keepdims=True), 1e-9)
    L = local_de(r.bind, r.padres)
    q0 = np.array([m_a_q(m[:3, :3]) for m in L])
    r.clips, r.cadera = {}, None
    for a in doc.get("animations", []):
        dur = max(float(patron.acc(doc, b, a["samplers"][c["sampler"]]["input"]).max()) for c in a["channels"])
        t = np.arange(int(round(dur * FPS)) + 1) / FPS
        R = np.tile(q0, (len(t), 1, 1))
        T = None
        for c in a["channels"]:
            n = c["target"]["node"]
            if n not in k_de:
                continue
            k = k_de[n]
            v0, sig = _muestrear(doc, b, a["samplers"][c["sampler"]], t)
            if c["target"]["path"] == "rotation":
                R[:, k] = v0 / np.linalg.norm(v0, axis=1, keepdims=True) if sig is None else nlerp(v0, sig[0], sig[1])
            elif c["target"]["path"] == "translation":
                r.cadera = k
                T = v0 if sig is None else v0 * (1 - sig[1]) + sig[0] * sig[1]
        if r.cadera is None:
            r.cadera = int(np.argmax(r.padres == 0)) if (r.padres == 0).any() else 0
        if T is None:
            T = np.tile(L[r.cadera][:3, 3], (len(t), 1))
        r.clips[a.get("name", f"clip_{len(r.clips)}")] = {"t": t, "R": R, "T": T}
    return r


def local_de(B, padres):
    """La matriz local de cada hueso desde las del mundo."""
    return np.stack([B[k] if p < 0 else np.linalg.inv(B[p]) @ B[k] for k, p in enumerate(padres)])


def al_juego(r):
    """El rig, de frente a +X, la izquierda a -Z, el suelo en y = 0 y el medio en z = 0: la vuelta (G) y el corrimiento,
    en el reposo y en los clips de la raiz."""
    v = r.v @ G.T
    off = np.array([0.0, -v[:, 1].min(), -0.5 * (v[:, 2].min() + v[:, 2].max())])
    M = np.eye(4)
    M[:3, :3], M[:3, 3] = G, off
    r.v = v + off
    r.bind = np.einsum("ij,njk->nik", M, r.bind)
    qG = m_a_q(G)
    for c in r.clips.values():
        for k in np.nonzero(r.padres < 0)[0]:
            c["R"][:, k] = qmul(qG, c["R"][:, k])
    return r


def hombros_al_tronco(r):
    """Los omoplatos y el pecho (AL_TRONCO), hijos del tronco y no de la base del cuello: en cada clip, el giro local de
    cada uno pasa a ser el de la base del cuello por el suyo (la misma orientacion en el mundo; solo el punto donde se
    articula deja de girar con el cuello)."""
    k_de = {n: k for k, n in enumerate(r.nombres)}
    for n in AL_TRONCO:
        k = k_de.get(n)
        if k is None or r.padres[k] < 0 or r.padres[r.padres[k]] < 0:
            continue
        base = r.padres[k]
        for c in r.clips.values():
            c["R"][:, k] = qmul(c["R"][:, base], c["R"][:, k])
        r.padres[k] = r.padres[base]
    return r


def abrir(ruta):
    """Un rig de un GLB, listo para llevarlo a otra malla: en el marco del juego y con los hombros en el tronco."""
    return hombros_al_tronco(al_juego(leer(ruta)))


def cargar(nombre):
    """Un rig de arte/fuentes/animales/rigs (caballo, hiena, aguila), listo (abrir)."""
    return abrir(os.path.join(RIGS, nombre + ".glb"))


# ------------------------------------------------------------------ llevar un rig a otra malla
def medida(v, f):
    """Lo medido de un cuadrupedo (de frente a +X, el suelo en y = 0, el medio en z = 0): sus puntos, sus medidas y el
    esqueleto simple de animales.py (donde estan la cadera, el cuello, la nuca, el hocico y la cola)."""
    P = animales.muestras(v, f)
    m = animales.medir(P)
    return P, m, animales.armar_esqueleto(P, m)


def partes(v, f, m, e):
    """La parte de cada vertice: 0 a 3, la pata (animales.PATAS: bajo el vientre, junto a su eje); 4, la cabeza con lo
    que sale de ella (cuernos, orejas, trompa: animales.cerca_de_la_cabeza); 5, el tronco con el cuello y la cola."""
    vb = m["vientre"]
    parte = np.full(len(v), 5)
    parte[animales.cerca_de_la_cabeza(v, f, e, m)] = 4
    bajo = v[:, 1] < 1.1 * vb
    dmin = np.full(len(v), np.inf)
    for k, n in enumerate(animales.PATAS):
        eje = m["ejes"][n]
        dd = np.hypot(v[:, 0] - np.interp(v[:, 1], eje[:, 1], eje[:, 0]), v[:, 2] - np.interp(v[:, 1], eje[:, 1], eje[:, 2]))
        mejor = bajo & (dd < dmin) & (dd < 1.2 * m["radio"])
        parte[mejor], dmin[mejor] = k, dd[mejor]
    return parte


def marcas(v, f, medido=None):
    """Las marcas de un cuadrupedo (de frente a +X, el suelo en y = 0, el medio en z = 0), siempre en el mismo orden."""
    P, m, e = medido or medida(v, f)
    L, A, W, vb = m["L"], m["A"], m["W"], m["vientre"]
    out = []
    for n in animales.PATAS:
        eje = m["ejes"][n]
        for y in (0.0, 0.5 * vb, vb):
            out.append([np.interp(y, eje[:, 1], eje[:, 0]), y, np.interp(y, eje[:, 1], eje[:, 2])])
    for h in ("cadera", "lomo", "pecho", "cuello", "cabeza", "hocico", "cola", "cola_2", "cola_punta"):
        out.append(e.pos[e.i(h)])
    for x in (m["xt"], m["xd"]):  # el lomo por arriba
        s = P[(np.abs(P[:, 0] - x) < 0.03 * L) & (np.abs(P[:, 2]) < 0.15 * W)]
        out.append([x, s[:, 1].max() if len(s) else m["lomo"], 0.0])
    xm = 0.5 * (m["xt"] + m["xd"])
    s = P[np.abs(P[:, 0] - xm) < 0.03 * L]
    yc = float(e.pos[e.i("lomo")][1])
    out.append([xm, s[:, 1].min() if len(s) else vb, 0.0])  # el vientre
    lado = s[np.abs(s[:, 1] - yc) < 0.08 * A]
    if len(lado):
        out += [[xm, yc, lado[:, 2].min()], [xm, yc, lado[:, 2].max()]]
    else:
        out += [[xm, yc, -0.3 * W], [xm, yc, 0.3 * W]]
    return np.array(out, float)


def warp(src, dst):
    """El warp de placas delgadas que lleva las marcas src a dst (con algo de suavizado: marcas casi juntas)."""
    from scipy.interpolate import RBFInterpolator
    esc = float(np.ptp(src, axis=0).max())
    f = RBFInterpolator(src / esc, dst, kernel="thin_plate_spline", smoothing=1e-5)
    return lambda p: f(np.asarray(p, float).reshape(-1, 3) / esc)


def pesos_de(r, vw, v, parte_ref, parte, plegar=()):
    """Los pesos de los vertices v: los del punto mas cercano de la malla de referencia deformada (vw), de la misma
    parte (partes: una pata, la cabeza o el tronco; asi la giba o la oreja grande no toman los de la pata de al lado),
    mezclados en su triangulo. plegar: huesos cuyo peso pasa a su padre (de las puntas hacia la cabeza)."""
    from scipy.spatial import cKDTree
    rng = np.random.default_rng(3)
    tri = vw[r.f]
    area = 0.5 * np.linalg.norm(np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0]), axis=1)
    n = 80000
    k = rng.choice(len(r.f), n, p=area / area.sum())
    s, u = np.sqrt(rng.random(n)), rng.random(n)
    bc = np.stack([1 - s, s * (1 - u), s * u], axis=1)
    pts = np.einsum("nc,ncj->nj", bc, tri[k])
    de = parte_ref[r.f[k, np.argmax(bc, axis=1)]]  # la parte de cada muestra: la de su esquina mas cercana
    i = np.zeros(len(v), np.int64)
    for p in np.unique(parte):
        quien = np.nonzero(parte == p)[0]
        cand = np.nonzero(de == p)[0]
        if len(cand) < 20:  # la referencia no tiene esa parte: de toda la malla
            cand = np.arange(n)
        i[quien] = cand[cKDTree(pts[cand]).query(v[quien])[1]]
    W = np.zeros((len(v), len(r.nombres)))
    filas = np.arange(len(v))
    for c in range(3):
        idx = r.f[k[i], c]
        for col in range(4):
            np.add.at(W, (filas, r.juntas[idx, col]), bc[i, c] * r.pesos[idx, col])
    for h in plegar:
        p = r.padres[h]
        W[:, p] += W[:, h]
        W[:, h] = 0.0
    top = np.argsort(-W, axis=1)[:, :4]
    w = np.clip(np.take_along_axis(W, top, axis=1), 0.0, None)
    w /= np.maximum(w.sum(1, keepdims=True), 1e-9)
    return top, w


def suavizar_pesos(juntas, pesos, f, n_huesos, veces=3, k=0.5):
    """Los pesos, suavizados sobre la malla (veces, cada vertice hacia el promedio de sus vecinos en k) y de vuelta a
    los cuatro mas fuertes: el paso de un hueso a otro se reparte en varios triangulos y no en uno (con un giro grande,
    como el del cuello al pastar, uno solo se estira en punta)."""
    from scipy.sparse import coo_matrix
    n = len(juntas)
    W = np.zeros((n, n_huesos))
    np.add.at(W, (np.repeat(np.arange(n), juntas.shape[1]), juntas.ravel()), pesos.ravel())
    a = np.concatenate([f[:, [0, 1]], f[:, [1, 2]], f[:, [2, 0]]])
    a = np.concatenate([a, a[:, ::-1]])
    A = coo_matrix((np.ones(len(a)), (a[:, 0], a[:, 1])), shape=(n, n)).tocsr()
    A.data[:] = 1.0
    grado = np.maximum(np.asarray(A.sum(axis=1)).ravel(), 1.0)[:, None]
    for _ in range(veces):
        W = (1.0 - k) * W + k * (A @ W) / grado
    top = np.argsort(-W, axis=1)[:, :4]
    w = np.clip(np.take_along_axis(W, top, axis=1), 0.0, None)
    w /= np.maximum(w.sum(1, keepdims=True), 1e-9)
    return top, w


def plegables(r):
    """Los huesos de las orejas (y lo que cuelga de ellas), de las puntas hacia la cabeza."""
    hijos_de = {}
    for k, p in enumerate(r.padres):
        hijos_de.setdefault(p, []).append(k)
    out = []
    def bajar(k):
        for c in hijos_de.get(k, []):
            bajar(c)
        out.append(k)
    for k, n in enumerate(r.nombres):
        if n.startswith(OREJAS) and not r.nombres[r.padres[k]].startswith(OREJAS):
            bajar(k)
    return out


def llevar(r, v, f, vr, fr, cuernos=False):
    """El rig r en la malla (v, f) (del marco del juego): las matrices de bind de sus huesos y los pesos de los vertices
    de la malla ya reducida (vr, fr). Devuelve tambien las alturas de la cadera (la de la referencia y la nueva)."""
    med_ref, med = medida(r.v, r.f), medida(v, f)
    w = warp(marcas(r.v, r.f, med_ref), marcas(v, f, med))
    B = r.bind.copy()
    B[:, :3, 3] = w(r.bind[:, :3, 3])
    parte_ref = partes(r.v, r.f, med_ref[1], med_ref[2])
    parte = partes(vr, fr, med[1], med[2])
    juntas, pesos = pesos_de(r, w(r.v), vr, parte_ref, parte, plegables(r) if cuernos else ())
    juntas, pesos = suavizar_pesos(juntas, pesos, fr, len(r.nombres))
    return B, juntas, pesos, float(r.bind[r.cadera, 1, 3]), float(B[r.cadera, 1, 3])


# ------------------------------------------------------------------ los clips del juego
def _clip(r, receta):
    """El clip de la referencia para una receta de MAPAS: {t, R, T}."""
    if isinstance(receta, str):
        c = r.clips[receta]
        return {"t": c["t"].copy(), "R": c["R"].copy(), "T": c["T"].copy()}
    tipo = receta[0]
    if tipo == "trozo":
        c = r.clips[receta[1]]
        dentro = (c["t"] >= receta[2] - 1e-9) & (c["t"] <= receta[3] + 1e-9)
        R, T, t = c["R"][dentro], c["T"][dentro], c["t"][dentro] - receta[2]
        n = len(t)
        u = (np.arange(1, n + 1) / n)[:, None, None]
        vuelta_R = nlerp(np.repeat(R[-1:], n, axis=0), np.repeat(R[:1], n, axis=0), u)
        vuelta_T = T[-1] + (T[0] - T[-1]) * u[:, :, 0]
        return {"t": np.concatenate([t, t[-1] + np.arange(1, n + 1) / FPS]), "R": np.concatenate([R, vuelta_R]),
                "T": np.concatenate([T, vuelta_T])}
    if tipo == "despacio":
        c = _clip(r, receta[1])
        c["t"] = c["t"] * receta[2]
        return c
    if tipo == "erguido":
        c, reposo = _clip(r, receta[1]), r.clips[receta[2]]
        media = np.stack([q_media(c["R"][:, k]) for k in range(c["R"].shape[1])])
        c["R"] = qmul(reposo["R"][0][None], qmul(qconj(media)[None], c["R"]))
        c["T"] = reposo["T"][0] + (c["T"] - c["T"].mean(0))
        c["t"] = c["t"] * receta[3]
        return c
    if tipo == "pastar":
        c, comer = _clip(r, receta[1]), r.clips[receta[2]]
        cuadro = np.round(np.linspace(0, len(comer["t"]) - 1, len(c["t"]))).astype(int)
        for k, n in enumerate(r.nombres):
            if n.lower().startswith(("mouth", "ear", "tail")):
                c["R"][:, k] = comer["R"][cuadro, k]
        return c
    raise ValueError(f"receta desconocida: {receta}")


def fk(L, R, T, padres, cadera):
    """Las matrices del mundo de cada hueso en cada cuadro (cuadros x huesos x 4 x 4): las locales de reposo L con las
    rotaciones R y la traslacion T de la cadera (los padres, antes que sus hijos)."""
    n, J = R.shape[:2]
    W = np.zeros((n, J, 4, 4))
    for k in range(J):
        M = np.tile(L[k], (n, 1, 1))
        M[:, :3, :3] = q_a_m(R[:, k])
        if k == cadera:
            M[:, :3, 3] = T
        W[:, k] = M if padres[k] < 0 else W[:, padres[k]] @ M
    return W


def q_eje(eje, grados):
    """El giro de grados alrededor de eje (... x 3, unitario)."""
    a = np.radians(np.asarray(grados, float))[..., None] * 0.5
    return np.concatenate([np.asarray(eje, float) * np.sin(a), np.cos(a) * np.ones_like(eje[..., :1])], axis=-1)


def cabeza_al_suelo(clip, B, nombres, padres, cadera, cabeza_mundo=-80.0, cuello_max=-70.0):
    """La cabeza de un clip, al suelo sin mover el cuerpo: el cuello baja en el plano del lomo (un giro alrededor del
    eje de costado, casi todo en la base, en la cruz) hasta que la boca toca el suelo en todo el clip, y la cabeza
    queda casi vertical (cabeza_mundo grados respecto de la horizontal, hasta 60 de giro propio). El cuello no pasa de
    cuello_max grados (de la cruz a la nuca): si no llega antes (el pivote de la referencia esta en la cruz, alto), la
    boca queda lo mas abajo que da esa pose, en el pasto alto. Devuelve el clip y el giro del cuello (grados)."""
    t, R, T = clip
    k_de = {n: k for k, n in enumerate(nombres)}
    boca = next((k_de[n] for n in ("mouth_tip", "nose") if n in k_de), None)
    nariz = k_de.get("nose", boca)
    cabeza = k_de.get("head")
    hombro = padres[k_de["front_scapula_l"]] if "front_scapula_l" in k_de else None
    if None in (boca, cabeza, hombro):
        return clip, 0.0
    cuello, k = [], padres[cabeza]
    while k >= 0 and k != hombro:  # de la nuca al tronco, de donde cuelgan los hombros (hombros_al_tronco)
        cuello.append(k)
        k = padres[k]
    cuello = cuello[::-1]  # de la base a la nuca
    L = local_de(B, padres)
    W = fk(L, R, T, padres, cadera)
    # El eje de costado del mundo en el marco de cada hueso: girar ahi es girar en el plano del lomo, y los giros de los
    # de arriba (alrededor del mismo eje) no lo cambian.
    eje = {c: W[:, c, 2, :3] for c in cuello + [cabeza]}
    peso = np.r_[0.8, 0.2 * np.arange(len(cuello) - 1, 0, -1) / max(1, sum(range(len(cuello))))]
    peso /= peso.sum()
    d = W[0, nariz, :3, 3] - W[0, cabeza, :3, 3]
    cabeza_reposo = float(np.degrees(np.arctan2(d[1], d[0])))
    d = W[0, cabeza, :3, 3] - W[0, cuello[0], :3, 3]
    g_max = min(0.0, cuello_max - float(np.degrees(np.arctan2(d[1], d[0]))))
    suelo = 0.03 * B[cadera, 1, 3]

    def posar(g):
        R2 = R.copy()
        giros = [(c, w * g) for c, w in zip(cuello, peso)]
        giros.append((cabeza, float(np.clip(cabeza_mundo - cabeza_reposo - g, -60.0, 60.0))))
        for c, a in giros:
            R2[:, c] = qmul(R[:, c], q_eje(eje[c], np.full(len(R), a)))
        return R2

    def altura(g):
        return float(fk(L, posar(g), T, padres, cadera)[:, boca, 1, 3].min())

    if altura(g_max) > suelo:
        return (t, posar(g_max), T), g_max
    lo, hi = 0.0, g_max  # lo: todavia arriba; hi: ya en el suelo
    for _ in range(14):
        mid = 0.5 * (lo + hi)
        lo, hi = (mid, hi) if altura(mid) > suelo else (lo, mid)
    return (t, posar(hi), T), hi


def clips_de(r, mapa, suyos, h_ref, h_nueva, B):
    """Los clips del juego (los de suyos que la referencia tiene en su mapa): {nombre: (t, R, T)}, con la traslacion
    de la cadera a la escala de su altura, las marchas a su ritmo y, en los de receta "pastar", la cabeza al suelo
    (cabeza_al_suelo). clips_de.cuellos: el giro del cuello de cada uno de esos (grados)."""
    k = h_nueva / h_ref
    ritmo = float(np.sqrt(k))
    reposo_ref = local_de(r.bind, r.padres)[r.cadera][:3, 3]
    cadera_reposo = local_de(B, r.padres)[r.cadera][:3, 3]
    out, clips_de.cuellos = {}, {}
    for nombre in sorted(suyos):
        if nombre not in mapa:
            continue
        c = _clip(r, mapa[nombre])
        t = c["t"] * (ritmo if nombre in MARCHAS else 1.0)
        out[nombre] = (t, c["R"], cadera_reposo + k * (c["T"] - reposo_ref))
        if isinstance(mapa[nombre], tuple) and mapa[nombre][0] == "pastar":
            out[nombre], clips_de.cuellos[nombre] = cabeza_al_suelo(out[nombre], B, r.nombres, r.padres, r.cadera)
    return out


# ------------------------------------------------------------------ el GLB
def escribir(nombres, padres, B, cadera, pos, nor, col, juntas, pesos, clips, nombre):
    """El GLB de un animal con su esqueleto (B: la matriz de bind de cada hueso en el mundo; los nodos, en esa pose)
    y sus clips ({nombre: (t, R, T)}: la rotacion local de cada hueso y la traslacion local de la cadera)."""
    buf = bytearray()
    views, accs = [], []

    def accesor(a, ctype, tipo, target=None, normalized=False, minmax=False):
        a = np.ascontiguousarray(a)
        while len(buf) % 4:
            buf.append(0)
        views.append({"buffer": 0, "byteOffset": len(buf), "byteLength": a.nbytes, **({"target": target} if target else {})})
        buf.extend(a.tobytes())
        d = {"bufferView": len(views) - 1, "componentType": ctype, "count": len(a), "type": tipo}
        if normalized:
            d["normalized"] = True
        if minmax:
            d["min"] = np.atleast_1d(a.min(axis=0)).tolist()
            d["max"] = np.atleast_1d(a.max(axis=0)).tolist()
        accs.append(d)
        return len(accs) - 1

    attrs = {"POSITION": accesor(pos.astype(np.float32), 5126, "VEC3", 34962, minmax=True),
             "NORMAL": accesor(nor.astype(np.float32), 5126, "VEC3", 34962),
             "COLOR_0": accesor(np.column_stack([col, np.full(len(col), 255)]).astype(np.uint8), 5121, "VEC4", 34962,
                                normalized=True),
             "JOINTS_0": accesor(juntas.astype(np.uint8), 5121, "VEC4", 34962),
             "WEIGHTS_0": accesor(pesos.astype(np.float32), 5126, "VEC4", 34962)}
    L = local_de(B, padres)
    q0 = [m_a_q(m[:3, :3]) for m in L]
    nodes = []
    for k, n in enumerate(nombres):
        nd = {"name": n, "translation": [float(x) for x in L[k][:3, 3]], "rotation": [float(x) for x in q0[k]]}
        hijos = [int(c) for c in np.nonzero(padres == k)[0]]
        if hijos:
            nd["children"] = hijos
        nodes.append(nd)
    nodes.append({"name": "cuerpo", "mesh": 0, "skin": 0})
    ibm = np.linalg.inv(B)
    ibm_acc = accesor(np.stack([m.T.reshape(-1) for m in ibm]).astype(np.float32), 5126, "MAT4")
    anims = []
    for clip, (t, R, T) in clips.items():
        entrada = accesor(t.astype(np.float32).reshape(-1, 1), 5126, "SCALAR", minmax=True)
        R = R.copy()
        for c in range(1, len(R)):  # el camino corto de un cuadro al siguiente
            R[c][np.sum(R[c] * R[c - 1], axis=1) < 0] *= -1
        samplers, channels = [], []
        for k in range(len(nombres)):
            samplers.append({"input": entrada, "output": accesor(R[:, k].astype(np.float32), 5126, "VEC4"),
                             "interpolation": "LINEAR"})
            channels.append({"sampler": len(samplers) - 1, "target": {"node": k, "path": "rotation"}})
        samplers.append({"input": entrada, "output": accesor(T.astype(np.float32), 5126, "VEC3"), "interpolation": "LINEAR"})
        channels.append({"sampler": len(samplers) - 1, "target": {"node": int(cadera), "path": "translation"}})
        anims.append({"name": clip, "samplers": samplers, "channels": channels})
    raices = [int(k) for k in np.nonzero(padres < 0)[0]]
    doc = {"asset": {"version": "2.0", "generator": "tools/modelos/rigs.py"},
           "scene": 0, "scenes": [{"nodes": raices + [len(nodes) - 1]}], "nodes": nodes,
           "meshes": [{"name": nombre, "primitives": [{"attributes": attrs, "mode": 4}]}],
           "skins": [{"joints": list(range(len(nombres))), "skeleton": raices[0], "inverseBindMatrices": ibm_acc}],
           "animations": anims, "accessors": accs, "bufferViews": views, "buffers": [{"byteLength": len(buf)}]}
    js = json.dumps(doc, separators=(",", ":")).encode()
    js += b" " * (-len(js) % 4)
    while len(buf) % 4:
        buf.append(0)
    total = 12 + 8 + len(js) + 8 + len(buf)
    return struct.pack("<III", 0x46546C67, 2, total) + struct.pack("<II", len(js), 0x4E4F534A) + js + \
        struct.pack("<II", len(buf), 0x004E4942) + bytes(buf)


# ------------------------------------------------------------------ armar un id con un rig de referencia
def cruz_de(v, f):
    """La altura a la cruz (de frente a +X, el suelo en y = 0, el medio en z = 0): lo mas alto del lomo sobre la parte
    de atras de las patas de adelante (donde la busca piezas.py en la vista de lado), sin el cuello, que sale delante."""
    P = animales.muestras(v, f)
    m = animales.medir(P)
    x = m["xd"] - 0.5 * m["radio"]
    s = P[(np.abs(P[:, 0] - x) < 0.025 * m["L"]) & (np.abs(P[:, 2]) < 0.2 * m["W"])]
    return float(s[:, 1].max())


def armar(malla_t, it, rig, nombre_rig, cruz=None, cuernos=False, mapa=None, log=print):
    """Como animales.armar (su tris_max, el color con la luz horneada), con el esqueleto, los pesos y los clips del rig
    de referencia. La medida: la altura a la cruz (cruz, docs/TAMANOS.md) o, sin ella, el alto del inventario; lo demas
    sale de la malla (armar.ultimo["medidas"]: la caja que resulta, para el inventario). mapa: lo que cambia de MAPAS
    para este animal (el elefante pasta con la trompa: idle)."""
    import lowres
    _, alto, _ = (float(x) for x in it["medidas_m"].split("x"))
    tmax = int(it["tris_max"]) or 800
    v, f, col = malla_t
    v = v - [0.0, v[:, 1].min(), 0.5 * (v[:, 2].min() + v[:, 2].max())]
    s = cruz / cruz_de(v, f) if cruz else alto / float(np.ptp(v[:, 1]))
    v = v * s
    m = animales.medir(animales.muestras(v, f))
    v = v - [0.5 * (m["xd"] + m["xt"]), 0.0, 0.0]
    vr, fr, cr = animales.reducir(v, f, col, tmax)
    B, juntas, pesos, h_ref, h_nueva = llevar(rig, v, f, vr, fr, cuernos)
    filas, _ = animales.indice.load_clips()
    suyos = {c["clip"] for c in filas if c["rig"] == "cuadrupedo" and any(animales.indice.matches(it, x) for x in c["filtros"])}
    padres = rig.padres
    clips = clips_de(rig, {**MAPAS[nombre_rig], **(mapa or {})}, suyos, h_ref, h_nueva, B)
    tri = vr[fr]
    n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    sol = np.array(lowres.SUN, float)
    sol /= np.linalg.norm(sol)
    luz = 0.72 + 0.38 * np.clip(n @ sol, 0.0, 1.0)
    caras = np.clip(cr[fr].mean(axis=1) * luz[:, None], 0, 255)
    datos = escribir(rig.nombres, padres, B, rig.cadera, tri.reshape(-1, 3), lowres.normales_aristas(vr, fr).reshape(-1, 3),
                     np.repeat(caras, 3, axis=0), juntas[fr].reshape(-1, 4), pesos[fr].reshape(-1, 4), clips, it["id"])
    c = vr.max(0) - vr.min(0)
    armar.ultimo = dict(B=B, nombres=rig.nombres, padres=padres, v=vr, f=fr, m=m, juntas=juntas, pesos=pesos,
                        medidas=f"{c[2]:.2f}x{c[1]:.2f}x{c[0]:.2f}")
    faltan = sorted(suyos - set(clips))
    cuellos = ", ".join(f"{n} con el cuello a {g:.0f} grados" for n, g in sorted(clips_de.cuellos.items()))
    log(f"{it['id']}: {len(fr)} triangulos, {c[0]:.2f} x {c[1]:.2f} x {c[2]:.2f} m (largo x alto x ancho), "
        f"cadera a {h_nueva:.2f} m, rig {nombre_rig}, {len(clips)} clips" + (f" (sin {', '.join(faltan)})" if faltan else "")
        + (f"; {cuellos}" if cuellos else ""))
    return datos, len(fr)


# ------------------------------------------------------------------ los animales de Better animals
MEJORES_DIR = os.path.join(ROOT, "arte", "fuentes", "animales", "mejores")
IMAGENES = os.path.join(ROOT, "arte", "fuentes", "animales", "imagenes")
PINTADAS = os.path.join(ROOT, "arte", "modelos", "para_riggear")
# id: (de donde vino la malla en IMPORTED/Better animals/Modelos, rig, altura a la cruz en m (docs/TAMANOS.md; None: el
# alto del inventario, la giba del camello), color, opciones). La malla, en arte/fuentes/animales/mejores/<lo ultimo
# del id>.glb. El color: (clave, sufijo) de la imagen de referencia (animales.imagen_de: la de silueta mas parecida),
# "pintada" (el de su malla de arte/modelos/para_riggear: no hay imagen del caballo ni del lobo) o un RGB liso (el
# mastin y el oso, como antes). Opciones: cuernos (los huesos de las orejas de la referencia, plegados en la cabeza:
# ahi caen los cuernos, que no se mueven) y mapa (lo que cambia de MAPAS).
MEJORES = {
    "animal.montura.caballo_estepario": ("24_11/white_mesh(5)", "caballo", 1.58, "pintada", {}),
    "animal.montura.caballo_estepario_2": ("25_11/white_mesh(8)", "caballo", 1.58, "pintada", {}),
    "animal.montura.caballo_estepario_3": ("24_11/hy3d-model", "caballo", 1.58, "pintada", {}),
    "animal.montura.caballo_guerra": ("25_11/white_mesh(1)", "caballo", 1.75, "pintada", {}),
    "animal.montura.burro": ("24_11/white_mesh", "caballo", 1.55, ("steppe donkey", ""), {}),
    "animal.montura.burro_2": ("25_11/hy3d-model(3)", "caballo", 1.55, ("steppe donkey", "-1"), {}),
    "animal.montura.camello_bactriano": ("24_11/hy3d-model(1)", "caballo", None, ("Bactrian camel", ""), {}),
    "animal.montura.dromedario": ("24_11/hy3d-model(2)", "caballo", 2.00, ("dromedary camel", "-1"), {}),
    "animal.montura.elefante_guerra": ("25_11/hy3d-model(5)", "caballo", 3.20, ("war elephant", "-1"),
                                       {"mapa": {"pastar": "Idle", "atacar": "Rear"}}),
    "animal.ganado.buey": ("25_11/white_mesh(9)", "caballo", 1.88, ("draft ox", ""), {"cuernos": True}),
    "animal.ganado.vaca": ("24_11/white_mesh(2)", "caballo", 1.62, ("steppe herd cow", ""), {}),
    "animal.ganado.perro_pastor": ("24_11/white_mesh(3)", "hiena", 0.78, (178, 142, 98), {}),
    "animal.salvaje.lobo": ("24_11/white_mesh(1)", "hiena", 0.85, "pintada", {}),
    "animal.salvaje.oso": ("25_11/white_mesh", "hiena", 1.50, (96, 68, 44), {}),
    "animal.salvaje.hiena": ("25_11/white_mesh(6)", "hiena", 0.90, ("striped hyena", ""), {}),
    "animal.salvaje.zorro": ("25_11/white_mesh(7)", "hiena", 0.50, ("red fox", "-1"), {}),
    "animal.salvaje.gacela": ("25_11/hy3d-model(4)", "caballo", 0.80, ("goitered gazelle", ""), {"cuernos": True}),
}
# Las variantes nuevas (id_2) que no estaban en el inventario: una fila como la de su especie.
VARIANTES = {"animal.montura.burro_2": "animal.montura.burro"}
NOTA = ("Malla de IMPORTED/Better animals/Modelos ({origen}.glb), con el esqueleto y los clips del rig de referencia "
        "{rig} (tools/modelos/rigs.py).")
DEL_RIG = {"caballo": "del caballo", "hiena": "de la hiena", "aguila": "del águila"}
MEDIDAS = "Medidas: {cruz} m a la cruz, la de los ejemplares más grandes de la especie (docs/TAMANOS.md); lo demás, de la malla."
MEDIDAS_ALTO = "Medidas: el alto, el de la giba (el del inventario); lo demás, de la malla."
# Lo que dejan las notas viejas de la medida y de la malla (animales.py, piezas.py y esta), para no repetirlas.
VIEJAS = (r"Medidas: .*?; lo demás, de la (?:imagen|malla)\. ?", r"Medidas: el alto, .*?; lo demás, de la malla\. ?",
          r"Malla .*?\(tools/modelos/[a-z]+\.py[^)]*\)\. ?", r"La hoja la dibuja mirando a la derecha \(--voltear-lado\)\. ?")


def color_de_pintada(v, ruta):
    """El color de cada vertice (de frente a +X) desde una malla pintada de para_riggear (la misma especie, de frente a
    +X): el de la cara mas cercana con las dos mallas en su caja (cada eje de 0 a 1, asi la cabeza va con la cabeza y
    las patas con las patas aunque cambien las proporciones)."""
    import trimesh
    from scipy.spatial import cKDTree
    sc = trimesh.load(ruta, force="scene")
    g = sc.to_geometry() if hasattr(sc, "to_geometry") else sc.dump(concatenate=True)
    v0, f0, uv = np.asarray(g.vertices, float), np.asarray(g.faces, np.int64), np.asarray(g.visual.uv, float)
    img = np.asarray(g.visual.material.baseColorTexture.convert("RGB"), float)
    h, w = img.shape[:2]
    c = uv[f0].mean(axis=1)
    px = np.clip(np.round((c[:, 0] % 1.0) * (w - 1)), 0, w - 1).astype(int)
    py = np.clip(np.round((1.0 - c[:, 1] % 1.0) * (h - 1)), 0, h - 1).astype(int)

    def caja(p, ref):
        lo, hi = ref.min(0), ref.max(0)
        return (p - lo) / np.maximum(hi - lo, 1e-9)

    _, cerca = cKDTree(caja(v0[f0].mean(axis=1), v0)).query(caja(v, v))
    return img[py, px][cerca]


def preparar(mid, log=print):
    """La malla de MEJORES de un id: limpia, de frente a +X, alisada y con su color (sin escala: va en armar)."""
    origen, _, _, color, _ = MEJORES[mid]
    ruta = os.path.join(MEJORES_DIR, mid.split(".")[-1] + ".glb")
    v, f, _ = animales.cargar(ruta)
    v, f, _, sueltas = animales.limpiar(v, f, None)
    if isinstance(color, tuple) and isinstance(color[0], str):
        col = animales.color_de_imagen(v, animales.imagen_de(IMAGENES, *color))  # de lado, la cabeza a -X
        v = animales.orientar(animales.suavizar(v, f), False)
    else:
        v = animales.orientar(animales.suavizar(v, f), False)
        col = color_de_pintada(v, os.path.join(PINTADAS, mid + ".glb")) if color == "pintada" else animales.color_liso(
            np.column_stack([-v[:, 0], v[:, 1], v[:, 2]]), color)  # color_liso: el hocico a -X
    log(f"  {origen}: {len(f)} triangulos, {sueltas} piezas sueltas fuera")
    return v, f, col


def main():
    """python3 tools/modelos/rigs.py [<id> ...]: arma los de MEJORES (todos si no se dice) en su GLB del inventario,
    con su fila al dia (la caja que resulta, importado y su nota)."""
    import re
    import inventario
    mids = sys.argv[1:] or list(MEJORES)
    for mid in mids:
        if mid not in MEJORES:
            raise SystemExit(f"{mid}: no esta en MEJORES")
    filas = open(inventario.TSV, encoding="utf-8").read().split("\n")

    def indice():
        return {f.split("\t")[0]: k for k, f in enumerate(filas) if f and not f.startswith("#")}

    for nuevo, base in VARIANTES.items():  # la fila de la variante, como la de su especie
        if nuevo not in indice():
            c = filas[indice()[base]].split("\t")
            c[0], c[1] = nuevo, c[1] + " (variante 2)"
            filas.insert(indice()[base] + 1, "\t".join(c))
    open(inventario.TSV, "w", encoding="utf-8").write("\n".join(filas))
    items, errores = inventario.load()
    if errores:
        raise SystemExit("\n".join(errores))
    por_id = {it["id"]: it for it in items}
    rigs_ = {}
    for mid in mids:
        origen, nombre_rig, cruz, _, op = MEJORES[mid]
        if nombre_rig not in rigs_:
            rigs_[nombre_rig] = cargar(nombre_rig)
        datos, _ = armar(preparar(mid), por_id[mid], rigs_[nombre_rig], nombre_rig, cruz, op.get("cuernos", False),
                         op.get("mapa"))
        ruta = os.path.join(ROOT, inventario.path_of(por_id[mid]))
        os.makedirs(os.path.dirname(ruta), exist_ok=True)
        open(ruta, "wb").write(datos)
        k = indice()[mid]
        c = filas[k].split("\t")
        c[3], c[5] = armar.ultimo["medidas"], "importado"
        nota = c[6]
        for patron_ in VIEJAS:
            nota = re.sub(patron_, "", nota)
        medidas = MEDIDAS.format(cruz=f"{cruz:g}") if cruz else MEDIDAS_ALTO
        c[6] = " ".join(x for x in (nota.strip(), medidas, NOTA.format(origen=origen, rig=DEL_RIG[nombre_rig])) if x)
        filas[k] = "\t".join(c)
    open(inventario.TSV, "w", encoding="utf-8").write("\n".join(filas))
    print("Sigue: python3 tools/assets/inventario.py doc && python3 tools/assets/animaciones.py doc")


if __name__ == "__main__":
    main()
