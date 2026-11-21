#!/usr/bin/env python3
"""El cuerpo humano estandar: una malla sin esqueleto (en pose T) y un esqueleto con clips (sin malla), juntos.

  python3 tools/modelos/humanoide.py <esqueleto.glb> <malla.glb> <id> [<id> ...]

- El esqueleto y los clips (IMPORTED/Humanoid/exported-model.glb): 56 huesos con los nombres del maniqui de Unreal
  (pelvis, spine_01, upperarm_l...) y 50 clips con los nombres de la Universal Animation Library de Quaternius.
- La malla (IMPORTED/Humanoid/sample_*.glb, la de pose T): un cuerpo con textura de unos 300 000 triangulos.

Lo que hace, para cada id del inventario (personaje.base.cuerpo_comun, personaje.narrativo.protagonista...):
1. Pone el esqueleto en pose T (desde el primer cuadro de Idle_A, girando cada hueso de los brazos y las piernas lo
   minimo) con los brazos y las piernas del largo de los de la malla.
2. Reduce la malla a su tris_max, con el color de la textura en cada cara (y la luz del sol horneada, lowres.SUN).
3. Pesos automaticos: cada vertice, a los dos huesos mas cercanos de su parte del cuerpo (brazo, pierna, torso,
   cabeza), por la distancia a cada hueso.
4. La pose de reposo es la T con los brazos abajo: las medidas del inventario.
5. El frente a +X (el del juego, tambien para los animados) y el alto del inventario.
6. Los clips, con los nombres del juego (assets/animaciones.tsv); los que la biblioteca no tiene, con el mas
   parecido (CLIPS) o armados con poses (sintetizar: la patada, la caida y el jinete a horcajadas). Sin el
   desplazamiento de la raiz: el juego mueve al personaje.
"""
import json
import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "assets"))
import inventario  # noqa: E402
import lowres  # noqa: E402

# Los clips del juego (assets/animaciones.tsv, rig humanoide) y el de la biblioteca que se usa para cada uno.
CLIPS = {
    "idle": "Idle_A", "caminar": "Walk", "correr": "Jog", "acechar": "Crouch_Walk", "acechar_idle": "Crouch_Idle",
    "saltar": "Jump_Land", "caer": "_caer", "aterrizar": "Jump_Land", "trepar_cuerda": "Ledge_Hang",
    "trepar_cima": "ClimbUp_1m_RM", "nadar": "Swim_Fwd", "montar_subir": "ClimbUp_1m_RM", "montar_bajar": "Jump_Land",
    "jinete_idle": "_montar", "jinete_paso": "_montar", "jinete_galope": "_montar", "jinete_arco": "Bow_Pull_Hold",
    "tomar": "Farm_Harvest", "llevar": "Walk_Formal", "lanzar": "Throw_Object", "empunar_una": "Idle_Sword",
    "empunar_doble": "Idle_Sword", "empunar_dos_manos": "Fighting_Idle", "empunar_escudo": "Idle_Shield",
    "empunar_arco": "Bow_Pull_Hold", "cambiar_empunadura": "Sword_Regular_A_Rec", "enfundar": "Sword_Regular_A_Rec",
    "desenfundar": "Sword_Regular_A_Rec", "agacharse_trabajar": "Farm_Harvest", "cavar": "Farm_Harvest",
    "lanzar_trepa": "Throw_Object", "lanzar_lazo": "Throw_Object", "forcejear_lazo": "Bow_Pull_Hold",
    "encender": "Farm_Harvest", "ensillar": "Farm_Harvest", "construir": "Farm_Harvest", "forjar": "Sword_Regular_B",
    "cocinar": "Farm_Harvest", "curar": "Farm_Harvest", "ataque_una_1": "Sword_Regular_A", "ataque_una_2": "Sword_Regular_B",
    "ataque_una_3": "Sword_Attack", "ataque_dos_1": "Sword_Attack", "ataque_dos_2": "Sword_Regular_B",
    "ataque_dos_3": "Sword_Regular_A", "estocada_lanza": "Punch_Jab", "bloquear": "Sword_Block", "agarre": "Punch_Cross",
    "derribo": "Shield_Dash_RM", "proyeccion": "Throw_Object", "ataque_pesado": "Sword_Attack_RM", "patada": "_patada",
    "patada_carrera": "_patada", "golpe_escudo": "Shield_OneShot", "carga_escudo": "Shield_Dash_RM",
    "enganchar_escudo": "Defend", "parry": "Sword_Block", "desvio": "Defend", "apunalar_espalda": "Punch_Jab",
    "sujetar_rehen": "Idle_FoldArms", "rehen": "Kneeling_Tired", "rematar": "Sword_Attack", "derribado": "Kneeling_Tired",
    "cambiar_mano": "Sword_Regular_A_Rec", "disparar_arco": "Bow_Release", "disparar_ballesta": "Bow_Release",
    "recargar_ballesta": "Bow_Pull_Back", "recibir_golpe": "Hit_Chest", "morir": "Death_A", "abatido": "Kneeling_Tired",
    "cojear": "Zombie_Walk", "hablar": "Idle_FoldArms",
}
# Los huesos que mueven la malla (los dedos, las puntas y la raiz, no) y la parte del cuerpo de cada uno.
PARTS = {
    "torso": ["pelvis", "spine_01", "spine_02", "spine_03", "clavicle_l", "clavicle_r", "upperarm_l", "upperarm_r", "neck_01"],
    "cabeza": ["neck_01", "head"],
    "brazo_l": ["clavicle_l", "upperarm_l", "lowerarm_l", "hand_l"],
    "brazo_r": ["clavicle_r", "upperarm_r", "lowerarm_r", "hand_r"],
    "pierna_l": ["pelvis", "thigh_l", "calf_l", "foot_l", "ball_l"],
    "pierna_r": ["pelvis", "thigh_r", "calf_r", "foot_r", "ball_r"],
}
# El hueso, hasta su hijo (el segmento al que se mide la distancia).
TIP = {"pelvis": "spine_01", "spine_01": "spine_02", "spine_02": "spine_03", "spine_03": "neck_01", "neck_01": "head",
       "head": "head_leaf", "clavicle_l": "upperarm_l", "upperarm_l": "lowerarm_l", "lowerarm_l": "hand_l",
       "hand_l": "middle_02_l", "clavicle_r": "upperarm_r", "upperarm_r": "lowerarm_r", "lowerarm_r": "hand_r",
       "hand_r": "middle_02_r", "thigh_l": "calf_l", "calf_l": "foot_l", "foot_l": "ball_l", "ball_l": "ball_leaf_l",
       "thigh_r": "calf_r", "calf_r": "foot_r", "foot_r": "ball_r", "ball_r": "ball_leaf_r"}


# ------------------------------------------------------------------ cuaterniones (x, y, z, w) y GLB
def qmul(a, b):
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return np.array([aw * bx + ax * bw + ay * bz - az * by, aw * by - ax * bz + ay * bw + az * bx,
                     aw * bz + ax * by - ay * bx + az * bw, aw * bw - ax * bx - ay * by - az * bz])


def qconj(q):
    return np.array([-q[0], -q[1], -q[2], q[3]])


def qrot(q, v):
    return qmul(qmul(q, np.array([v[0], v[1], v[2], 0.0])), qconj(q))[:3]


def qmat(q):
    x, y, z, w = q
    return np.array([[1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w)],
                     [2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w)],
                     [2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y)]])


def qbetween(u, v):
    u, v = u / np.linalg.norm(u), v / np.linalg.norm(v)
    c = float(u @ v)
    if c < -0.9999:  # opuestos: media vuelta sobre un eje cualquiera perpendicular
        ax = np.cross(u, [1.0, 0, 0]) if abs(u[0]) < 0.9 else np.cross(u, [0, 1.0, 0])
        ax /= np.linalg.norm(ax)
        return np.array([ax[0], ax[1], ax[2], 0.0])
    a = np.cross(u, v)
    q = np.array([a[0], a[1], a[2], 1.0 + c])
    return q / np.linalg.norm(q)


def leer(path):
    data = open(path, "rb").read()
    n = struct.unpack_from("<I", data, 12)[0]
    doc = json.loads(data[20:20 + n])
    return doc, data[20 + n + 8:]


def acc(doc, binario, i):
    a = doc["accessors"][i]
    v = doc["bufferViews"][a["bufferView"]]
    k = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[a["type"]]
    dt = {5126: np.float32, 5125: np.uint32, 5123: np.uint16, 5121: np.uint8}[a["componentType"]]
    stride = v.get("byteStride", 0)
    off = v.get("byteOffset", 0) + a.get("byteOffset", 0)
    if stride and stride != k * np.dtype(dt).itemsize:
        raw = np.frombuffer(binario, np.uint8, a["count"] * stride, off).reshape(-1, stride)[:, :k * np.dtype(dt).itemsize]
        return np.frombuffer(raw.tobytes(), dt).reshape(a["count"], k)
    return np.frombuffer(binario, dt, a["count"] * k, off).reshape(a["count"], k)


# ------------------------------------------------------------------ el esqueleto
class Esqueleto:
    def __init__(self, path):
        self.doc, self.bin = leer(path)
        self.nodes = self.doc["nodes"]
        self.names = [n.get("name", str(i)) for i, n in enumerate(self.nodes)]
        self.idx = {n: i for i, n in enumerate(self.names)}
        self.parent = [-1] * len(self.nodes)
        for i, n in enumerate(self.nodes):
            for c in n.get("children", []):
                self.parent[c] = i
        self.order = []  # padres antes que hijos
        def walk(i):
            self.order.append(i)
            for c in self.nodes[i].get("children", []):
                walk(c)
        for r in self.doc["scenes"][0]["nodes"]:
            walk(r)
        self.T = np.array([n.get("translation", [0, 0, 0]) for n in self.nodes], float)
        self.R = np.array([n.get("rotation", [0, 0, 0, 1]) for n in self.nodes], float)
        self.clips = {}
        for a in self.doc["animations"]:
            chans = []
            for ch in a["channels"]:
                s = a["samplers"][ch["sampler"]]
                chans.append((ch["target"]["node"], ch["target"]["path"], acc(self.doc, self.bin, s["input"])[:, 0].astype(float),
                              acc(self.doc, self.bin, s["output"]).astype(float), s.get("interpolation", "LINEAR")))
            self.clips[a["name"]] = chans

    def pose_inicial(self, clip):
        """Las rotaciones y traslaciones locales del primer cuadro de un clip."""
        T, R = self.T.copy(), self.R.copy()
        for node, path, _, vals, _ in self.clips[clip]:
            if path == "rotation":
                R[node] = vals[0] / np.linalg.norm(vals[0])
            elif path == "translation":
                T[node] = vals[0]
        return T, R

    def mundo(self, T, R):
        """Posicion y rotacion de cada nodo en el mundo."""
        P, Q = np.zeros((len(self.nodes), 3)), np.zeros((len(self.nodes), 4))
        for i in self.order:
            p = self.parent[i]
            if p < 0:
                P[i], Q[i] = T[i], R[i]
            else:
                Q[i] = qmul(Q[p], R[i])
                P[i] = P[p] + qrot(Q[p], T[i])
        return P, Q


def apuntar(esq, T, R, hueso, hijo, destino):
    """Gira el hueso lo minimo para que su hijo quede en la direccion de destino (en el mundo)."""
    P, Q = esq.mundo(T, R)
    i, c = esq.idx[hueso], esq.idx[hijo]
    giro = qbetween(P[c] - P[i], destino - P[i])
    nueva = qmul(giro, Q[i])
    p = esq.parent[i]
    R[i] = qmul(qconj(Q[p]), nueva) if p >= 0 else nueva
    R[i] /= np.linalg.norm(R[i])


def clip_de_poses(esq, poses, tiempos, interp="LINEAR"):
    """Un clip con una pose (T, R locales) en cada tiempo: la rotacion de cada hueso y la traslacion de la pelvis."""
    chans = []
    t = np.array(tiempos, float)
    for i in range(len(esq.nodes)):
        vals = [poses[0][1][i] / np.linalg.norm(poses[0][1][i])]
        for _, R in poses[1:]:
            q = R[i] / np.linalg.norm(R[i])
            vals.append(-q if q @ vals[-1] < 0 else q)  # el camino corto entre una pose y la siguiente
        chans.append((i, "rotation", t, np.array(vals), interp))
    pel = esq.idx["pelvis"]
    chans.append((pel, "translation", t, np.array([T[pel] for T, _ in poses]), interp))
    return chans


def sintetizar(esq):
    """Los clips que la biblioteca no tiene, armados con poses (en el espacio del esqueleto: el frente a +Z, la
    derecha a -X):
    - _patada: de la guardia (Fighting_Idle), la rodilla derecha arriba, la pierna extendida al frente y de vuelta;
    - _caer: el primer cuadro de Jump_Land (en el aire), quieto;
    - _montar: a horcajadas, los muslos adelante y abiertos y las piernas colgando (desde Idle_A)."""
    def pierna(T, R, lado, muslo, pantorrilla):
        P, _ = esq.mundo(T, R)
        cadera = P[esq.idx["thigh_" + lado]]
        apuntar(esq, T, R, "thigh_" + lado, "calf_" + lado, cadera + np.array(muslo, float))
        P, _ = esq.mundo(T, R)
        rodilla = P[esq.idx["calf_" + lado]]
        apuntar(esq, T, R, "calf_" + lado, "foot_" + lado, rodilla + np.array(pantorrilla, float))
    T0, R0 = esq.pose_inicial("Fighting_Idle")
    T1, R1 = T0.copy(), R0.copy()
    pierna(T1, R1, "r", (0.0, -0.1, 1.0), (0.0, -1.0, 0.2))   # la rodilla arriba, el pie colgando
    T2, R2 = T0.copy(), R0.copy()
    pierna(T2, R2, "r", (0.0, 0.12, 1.0), (0.0, 0.15, 1.0))   # la pierna extendida al frente
    esq.clips["_patada"] = clip_de_poses(esq, [(T0, R0), (T1, R1), (T2, R2), (T2, R2), (T0, R0)], [0.0, 0.16, 0.3, 0.38, 0.62])
    Tc, Rc = esq.pose_inicial("Jump_Land")
    esq.clips["_caer"] = clip_de_poses(esq, [(Tc, Rc), (Tc, Rc)], [0.0, 0.5])
    Tm, Rm = esq.pose_inicial("Idle_A")
    for lado, sg in (("l", 1.0), ("r", -1.0)):
        pierna(Tm, Rm, lado, (sg * 0.45, -0.25, 1.0), (sg * 0.12, -1.0, -0.15))
    Tm[esq.idx["pelvis"]] = Tm[esq.idx["pelvis"]] - np.array([0.0, 0.0, 0.0])
    esq.clips["_montar"] = clip_de_poses(esq, [(Tm, Rm), (Tm, Rm)], [0.0, 0.5])


# ------------------------------------------------------------------ la malla
def malla(path):
    doc, binario = leer(path)
    prim = doc["meshes"][0]["primitives"][0]
    pos = acc(doc, binario, prim["attributes"]["POSITION"]).astype(float)
    uv = acc(doc, binario, prim["attributes"]["TEXCOORD_0"]).astype(float)
    idx = acc(doc, binario, prim["indices"])[:, 0].astype(np.int64).reshape(-1, 3)
    color = np.full((len(pos), 3), 180.0)
    mat = doc.get("materials", [{}])[prim.get("material", 0)]
    tex = mat.get("pbrMetallicRoughness", {}).get("baseColorTexture")
    if tex is not None:
        t = doc["textures"][tex["index"]]
        src = t.get("source", t.get("extensions", {}).get("EXT_texture_webp", {}).get("source"))
        view = doc["bufferViews"][doc["images"][src]["bufferView"]]
        import io
        from PIL import Image
        img = np.asarray(Image.open(io.BytesIO(binario[view.get("byteOffset", 0):view.get("byteOffset", 0) + view["byteLength"]])).convert("RGB"), float)
        h, w = img.shape[:2]
        px = np.clip((uv[:, 0] % 1.0) * (w - 1), 0, w - 1).astype(int)
        py = np.clip((uv[:, 1] % 1.0) * (h - 1), 0, h - 1).astype(int)
        color = img[py, px]
    return pos, idx, color


def reducir(pos, idx, color, tris):
    import fast_simplification
    from scipy.spatial import cKDTree
    # Los vertices repetidos (las costuras de la textura), soldados: si no, sus bordes no se reducen.
    u, inv = np.unique(np.round(pos, 5), axis=0, return_inverse=True)
    f0 = inv.reshape(-1)[idx]
    f0 = f0[(f0[:, 0] != f0[:, 1]) & (f0[:, 1] != f0[:, 2]) & (f0[:, 0] != f0[:, 2])]
    p2, f2 = u.astype(np.float32), f0.astype(np.int32)
    for _ in range(12):  # de a poco hasta el tope (de una vez, se queda corto)
        if len(f2) <= tris:
            break
        p2, f2 = fast_simplification.simplify(p2, f2, min(0.9, 1.0 - tris / len(f2)))
    # Sin las astillas que deja la reduccion de una punta del cuerpo a otra (un pie con una mano).
    t = p2[f2]
    lado = np.max(np.linalg.norm(t - np.roll(t, 1, axis=1), axis=2), axis=1)
    f2 = f2[lado < 0.12 * (pos[:, 1].max() - pos[:, 1].min())]
    _, near = cKDTree(pos).query(p2)
    return p2.astype(float), f2.astype(np.int64), color[near]


# ------------------------------------------------------------------ armar un id
def armar(esq, malla_t, it, log=print):
    ancho, alto, largo = (float(x) for x in it["medidas_m"].split("x"))
    tmax = int(it["tris_max"]) or 1200
    T, R = esq.pose_inicial("Idle_A")
    P, _ = esq.mundo(T, R)
    alto_esq = P[esq.idx["head_leaf"]][1]  # del suelo (la raiz) a la punta de la cabeza
    log(f"  Idle_A: raiz {P[esq.idx['root']].round(3)}, pies {P[esq.idx['ball_leaf_l']][1]:.3f}/{P[esq.idx['ball_leaf_r']][1]:.3f}, cabeza {alto_esq:.3f}")
    # La malla a la escala del esqueleto: los pies en el suelo, la coronilla en la punta de la cabeza.
    pos, idx, color = malla_t
    lo, hi = pos.min(0), pos.max(0)
    k = alto_esq / (hi[1] - lo[1])
    torso = pos[(pos[:, 1] > lo[1] + 0.45 * (hi[1] - lo[1])) & (pos[:, 1] < lo[1] + 0.7 * (hi[1] - lo[1])) &
                (np.abs(pos[:, 0] - 0.5 * (lo[0] + hi[0])) < 0.08 * (hi[1] - lo[1]))]
    pel = P[esq.idx["pelvis"]]
    m = np.column_stack([(pos[:, 0] - torso[:, 0].mean()) * k + pel[0], (pos[:, 1] - lo[1]) * k,
                         (pos[:, 2] - torso[:, 2].mean()) * k + pel[2]])
    H = alto_esq
    # La pose T: los brazos hasta las munecas de la malla, las piernas hasta sus tobillos (con el largo de cada uno).
    for lado, sg in (("l", 1.0), ("r", -1.0)):
        tip = m[:, 0].max() if sg > 0 else m[:, 0].min()
        brazo = m[sg * m[:, 0] > 0.75 * abs(tip)]
        muneca = np.array([tip - sg * 0.11 * H, brazo[:, 1].mean(), brazo[:, 2].mean()])
        P, _ = esq.mundo(T, R)
        hombro = P[esq.idx["upperarm_" + lado]]
        largo_brazo = np.linalg.norm(P[esq.idx["lowerarm_" + lado]] - hombro) + np.linalg.norm(P[esq.idx["hand_" + lado]] - P[esq.idx["lowerarm_" + lado]])
        f = np.linalg.norm(muneca - hombro) / largo_brazo
        T[esq.idx["lowerarm_" + lado]] *= f
        T[esq.idx["hand_" + lado]] *= f
        for b, c in (("upperarm_", "lowerarm_"), ("lowerarm_", "hand_"), ("hand_", "middle_01_")):
            apuntar(esq, T, R, b + lado, c + lado, muneca + (muneca - hombro) * (0.3 if b == "hand_" else 0.0))
        pie = m[(m[:, 1] < 0.06 * H) & (sg * m[:, 0] > 0.0)]
        P, _ = esq.mundo(T, R)
        cadera, tobillo = P[esq.idx["thigh_" + lado]], P[esq.idx["foot_" + lado]]
        destino = np.array([pie[:, 0].mean(), tobillo[1], cadera[2]])  # derecha; el largo, el del esqueleto (la pelvis de los clips)
        log(f"  brazo {lado}: x{f:.2f}; tobillo de {tobillo.round(3)} a {destino.round(3)}")
        for b, c in (("thigh_", "calf_"), ("calf_", "foot_")):
            apuntar(esq, T, R, b + lado, c + lado, destino)
        P, _ = esq.mundo(T, R)  # los pies, hacia adelante (+Z) como los de la malla
        tob, bola = P[esq.idx["foot_" + lado]], P[esq.idx["ball_" + lado]]
        apuntar(esq, T, R, "foot_" + lado, "ball_" + lado, np.array([tob[0], bola[1], tob[2] + 1.0]))
        P, _ = esq.mundo(T, R)
        bola = P[esq.idx["ball_" + lado]]
        apuntar(esq, T, R, "ball_" + lado, "ball_leaf_" + lado, bola + np.array([0.0, 0.0, 1.0]))
    PT, QT = esq.mundo(T, R)
    # La malla reducida, cara por cara, y los pesos (en la pose T).
    v, f, col = reducir(m, idx, color, tmax - 20)
    def seg(b):
        return PT[esq.idx[b]], PT[esq.idx[TIP[b]]]
    def dist(p, a, b):
        ab = b - a
        t = np.clip(((p - a) @ ab) / max(ab @ ab, 1e-12), 0.0, 1.0)
        return np.linalg.norm(p - (a + t[:, None] * ab), axis=1)
    sx_l, sx_r = PT[esq.idx["upperarm_l"]][0], PT[esq.idx["upperarm_r"]][0]
    entrepierna = PT[esq.idx["thigh_l"]][1] - 0.05 * H
    cuello = PT[esq.idx["neck_01"]][1] + 0.01 * H
    alto_brazo = v[:, 1] > 0.6 * H  # los brazos, en pose T, a la altura de los hombros (los pies tambien salen al lado)
    parte = np.where(alto_brazo & (v[:, 0] > sx_l + 0.02 * H), 2, np.where(alto_brazo & (v[:, 0] < sx_r - 0.02 * H), 3,
             np.where(v[:, 1] < entrepierna, np.where(v[:, 0] > PT[esq.idx["pelvis"]][0], 4, 5), np.where(v[:, 1] > cuello, 1, 0))))
    nombres = list(PARTS)
    joints = np.zeros((len(v), 4), np.uint8)
    weights = np.zeros((len(v), 4), np.float32)
    for pi, pn in enumerate(nombres):
        sel = np.where(parte == pi)[0]
        if not len(sel):
            continue
        huesos = PARTS[pn]
        d = np.stack([dist(v[sel], *seg(b)) for b in huesos], axis=1)
        orden = np.argsort(d, axis=1)[:, :2]
        d2 = np.take_along_axis(d, orden, axis=1)
        w = 1.0 / (d2 + 0.02 * H) ** 4
        w /= w.sum(axis=1, keepdims=True)
        for kk in range(2):
            joints[sel, kk] = [esq.idx[huesos[o]] for o in orden[:, kk]]
            weights[sel, kk] = w[:, kk]
    # El frente a +X, el de todos los modelos del juego: los animados se dibujan girados yaw - PI/2 y miran a +X
    # (con +Z, como en la v0.15.4, caminaban mirando a su derecha). Un cuarto de vuelta sobre Y a la raiz.
    yaw = np.array([0.0, np.sin(np.pi / 4), 0.0, np.cos(np.pi / 4)])
    root = esq.idx["root"]
    # La pose de reposo: la T con los brazos abajo (un poco abiertos), parejos.
    TI, RI = T.copy(), R.copy()
    for lado, sg in (("l", 1.0), ("r", -1.0)):
        P, _ = esq.mundo(TI, RI)
        hombro = P[esq.idx["upperarm_" + lado]]
        abajo = hombro + np.array([sg * 0.2, -1.0, 0.0])
        for b, c in (("upperarm_", "lowerarm_"), ("lowerarm_", "hand_"), ("hand_", "middle_01_")):
            apuntar(esq, TI, RI, b + lado, c + lado, abajo + (abajo - hombro) * (0.3 if b == "hand_" else 0.0))
    RI[root] = qmul(yaw, RI[root])
    PI, QI = esq.mundo(TI, RI)
    # Cada vertice, llevado por sus huesos de la pose T a la de reposo.
    def matriz(p, q):
        M = np.eye(4)
        M[:3, :3] = qmat(q)
        M[:3, 3] = p
        return M
    MT = [matriz(PT[i], QT[i]) for i in range(len(PT))]
    MI = [matriz(PI[i], QI[i]) for i in range(len(PI))]
    skin = [MI[i] @ np.linalg.inv(MT[i]) for i in range(len(MT))]
    vh = np.column_stack([v, np.ones(len(v))])
    vi = np.zeros_like(v)
    for kk in range(2):
        mats = np.stack([skin[j] for j in joints[:, kk]])
        vi += weights[:, kk:kk + 1] * np.einsum("nij,nj->ni", mats, vh)[:, :3]
    # Todo al alto del inventario.
    s = alto / (vi[:, 1].max() - vi[:, 1].min())
    log(f"  reposo: de {vi[:, 1].min():+.3f} a {vi[:, 1].max():.3f} (esqueleto), escala {s:.3f}")
    vi *= s
    TI = TI * s
    PI, QI = esq.mundo(TI, RI)
    ibm = [np.linalg.inv(matriz(PI[i], QI[i])) for i in range(len(PI))]
    # Cara por cara: el color medio de la textura y la luz del sol horneada.
    tri = vi[f]
    n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    sol = np.array(lowres.SUN, float)
    sol /= np.linalg.norm(sol)
    luz = 0.72 + 0.38 * np.clip(n @ sol, 0.0, 1.0)  # mas suave que la de lo quieto: el cuerpo se mueve
    caras = np.clip(col[f].mean(axis=1) * luz[:, None], 0, 255)
    datos = escribir(esq, TI, RI, root, yaw, s, vi[f].reshape(-1, 3), np.repeat(n, 3, axis=0), np.repeat(caras, 3, axis=0),
                     joints[f].reshape(-1, 4), weights[f].reshape(-1, 4), ibm, it["id"])
    armar.ultimo = dict(v=v, joints=joints, weights=weights, skin=skin, PT=PT, PI=PI, parte=parte)
    caja = vi.max(0) - vi.min(0)
    log(f"{it['id']}: {len(f)} triangulos, {caja[0]:.2f} x {caja[1]:.2f} x {caja[2]:.2f} m (largo x alto x ancho), "
        f"{len(CLIPS)} clips")
    return datos, len(f)


def escribir(esq, T, R, root, yaw, s, pos, nor, col, joints, weights, ibm, nombre):
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
            d["min"] = a.min(axis=0).tolist()
            d["max"] = a.max(axis=0).tolist()
        accs.append(d)
        return len(accs) - 1
    attrs = {"POSITION": accesor(pos.astype(np.float32), 5126, "VEC3", 34962, minmax=True),
             "NORMAL": accesor(nor.astype(np.float32), 5126, "VEC3", 34962),
             "COLOR_0": accesor(np.column_stack([col, np.full(len(col), 255)]).astype(np.uint8), 5121, "VEC4", 34962, normalized=True),
             "JOINTS_0": accesor(joints.astype(np.uint8), 5121, "VEC4", 34962),
             "WEIGHTS_0": accesor(weights.astype(np.float32), 5126, "VEC4", 34962)}
    nodes = []
    for i, n in enumerate(esq.nodes):
        nd = {"name": esq.names[i], "translation": [float(x) for x in T[i]], "rotation": [float(x) for x in R[i]]}
        if n.get("children"):
            nd["children"] = n["children"]
        nodes.append(nd)
    nodes.append({"name": "cuerpo", "mesh": 0, "skin": 0})
    ibm_acc = accesor(np.stack([m.T.reshape(-1) for m in ibm]).astype(np.float32), 5126, "MAT4")
    # Los clips: los tiempos y los valores de la biblioteca, una vez por clip usado; los del juego, nombres.
    usados, anims = {}, []
    for juego, fuente in CLIPS.items():
        if fuente not in esq.clips:
            raise SystemExit(f"falta el clip {fuente} en el esqueleto")
        if fuente not in usados:
            samplers, channels = [], []
            for node, path, t, vals, interp in esq.clips[fuente]:
                if node == root and path == "translation":
                    continue  # sin el desplazamiento de la raiz
                if path == "translation":
                    vals = vals * s
                if node == root and path == "rotation":
                    vals = np.stack([qmul(yaw, q / np.linalg.norm(q)) for q in vals])
                samplers.append({"input": accesor(t.astype(np.float32).reshape(-1, 1), 5126, "SCALAR", minmax=True),
                                 "output": accesor(vals.astype(np.float32), 5126, "VEC4" if path == "rotation" else "VEC3"),
                                 "interpolation": interp})
                channels.append({"sampler": len(samplers) - 1, "target": {"node": node, "path": path}})
            usados[fuente] = (samplers, channels)
        samplers, channels = usados[fuente]
        anims.append({"name": juego, "samplers": samplers, "channels": channels})
    doc = {"asset": {"version": "2.0", "generator": "tools/modelos/humanoide.py"},
           "scene": 0, "scenes": [{"nodes": [root, len(nodes) - 1]}], "nodes": nodes,
           "meshes": [{"name": nombre, "primitives": [{"attributes": attrs, "mode": 4}]}],
           "skins": [{"joints": list(range(len(esq.nodes))), "skeleton": root, "inverseBindMatrices": ibm_acc}],
           "animations": anims, "accessors": accs, "bufferViews": views, "buffers": [{"byteLength": len(buf)}]}
    js = json.dumps(doc, separators=(",", ":")).encode()
    js += b" " * (-len(js) % 4)
    while len(buf) % 4:
        buf.append(0)
    total = 12 + 8 + len(js) + 8 + len(buf)
    return struct.pack("<III", 0x46546C67, 2, total) + struct.pack("<II", len(js), 0x4E4F534A) + js + \
        struct.pack("<II", len(buf), 0x004E4942) + bytes(buf)


def main():
    if len(sys.argv) < 4:
        raise SystemExit(__doc__)
    esq = Esqueleto(sys.argv[1])
    sintetizar(esq)
    malla_t = malla(sys.argv[2])
    items, errores = inventario.load()
    if errores:
        raise SystemExit("\n".join(errores))
    por_id = {it["id"]: it for it in items}
    filas = open(inventario.TSV, encoding="utf-8").read().split("\n")
    nota = (" Cuerpo humano estandar (tools/modelos/humanoide.py): la malla en pose T de IMPORTED/Humanoid (sample_*.glb), "
            "reducida y con el color de su textura, y el esqueleto y los clips de exported-model.glb (los nombres de la "
            "Universal Animation Library de Quaternius), con pesos automaticos; los clips, con los nombres del juego.")
    for mid in sys.argv[3:]:
        it = por_id[mid]
        datos, tris = armar(esq, malla_t, it)
        ruta = os.path.join(ROOT, inventario.path_of(it))
        os.makedirs(os.path.dirname(ruta), exist_ok=True)
        open(ruta, "wb").write(datos)
        for i, fila in enumerate(filas):
            c = fila.split("\t")
            if c[0] == mid:
                c[5] = "importado"
                if "humanoide.py" not in c[6]:
                    c[6] = c[6] + nota
                filas[i] = "\t".join(c)
    open(inventario.TSV, "w", encoding="utf-8").write("\n".join(filas))
    print("Sigue: python3 tools/assets/inventario.py doc && python3 tools/assets/animaciones.py doc")


if __name__ == "__main__":
    main()
