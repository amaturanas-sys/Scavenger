#!/usr/bin/env python3
"""El cuerpo humano estandar: una malla sin esqueleto (en pose T) y un esqueleto con clips (sin malla), juntos.

  python3 tools/modelos/humanoide.py <esqueleto.glb> <malla.glb> <id> [<id> ...] [--ual <UAL1.glb> <UAL2.glb>]

(Las fuentes, en arte/fuentes: arte/fuentes/README.md. Los tres cuerpos con el de mujer, por ejemplo:
  python3 tools/modelos/humanoide.py arte/fuentes/humanoide/exported-model.glb arte/fuentes/humanoide/rigged-model.glb \
      personaje.base.cuerpo_comun ... --ual arte/fuentes/animaciones/UAL1/UAL1_Standard.glb \
      arte/fuentes/animaciones/UAL2/UAL2_Standard.glb
  y el maniqui femenino, arte/fuentes/animaciones/UAL2/Mannequin_F.glb.)

- El esqueleto y los clips (arte/fuentes/humanoide/exported-model.glb): 56 huesos con los nombres del maniqui de Unreal
  (pelvis, spine_01, upperarm_l...) y 50 clips con los nombres de la Universal Animation Library de Quaternius.
- La malla, en pose T: un cuerpo con textura de unos 300 000 triangulos. Desde la v0.15.8, arte/fuentes/humanoide/
  rigged-model.glb, ya riggeada (un esqueleto de 22 huesos de Mixamo, con sus pesos y 6 clips propios): se toma en la
  pose de su piel y con sus pesos, llevados a los huesos del maniqui (MIXAMO), y sus clips Idle, Walk y Sprint son
  idle, caminar y correr (de_la_malla; los otros 69, de la biblioteca). Antes, sample_*.glb, sin esqueleto: los
  pesos, automaticos (paso 3).
- El cuerpo de mujer (personaje.base.cuerpo_mujer): el maniqui femenino de la Universal Animation Library 2
  (Mannequin_F.glb, CC0), con el rig de la biblioteca (PAR_UAL: los huesos del maniqui; los dedos, a la mano).
- --ual: UAL1_Standard.glb y UAL2_Standard.glb (arte/fuentes/animaciones, CC0, el mismo rig): los clips de DE_UAL
  (llevar, hablar, lanzar, forjar, la estocada, el remate, construir, curar, encender, ensillar, cocinar), llevados al
  maniqui como los de la malla; a una malla sin clips (el maniqui femenino), tambien idle, caminar y correr.

Lo que hace, para cada id del inventario (personaje.base.cuerpo_comun, personaje.narrativo.protagonista...):
1. Pone el esqueleto en pose T (desde el primer cuadro de Idle_A, girando cada hueso de los brazos y las piernas lo
   minimo) con los brazos y las piernas del largo de los de la malla.
2. Reduce la malla a su tris_max, con el color de la textura en cada cara (y la luz del sol horneada, lowres.SUN).
3. Los pesos: los de la malla riggeada (cada vertice de la reducida, los del vertice mas cercano de la original); si
   no trae, automaticos: cada vertice, a los dos huesos mas cercanos de su parte del cuerpo (brazo, pierna, torso,
   cabeza), por la distancia a cada hueso.
4. La pose de reposo es la T con los brazos abajo: las medidas del inventario.
5. El frente a +X (el del juego, tambien para los animados) y el alto del inventario.
6. Los clips, con los nombres del juego (assets/animaciones.tsv); los que la biblioteca no tiene, con el mas
   parecido (CLIPS) o armados con poses (sintetizar: la patada, la caida y el jinete a horcajadas). Sin el
   desplazamiento de la raiz: el juego mueve al personaje.
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
# Los clips del juego que salen de los de la malla riggeada (de_la_malla), si los trae: los demas, de la biblioteca.
DE_LA_MALLA = {"idle": "Idle", "caminar": "Walk", "correr": "Sprint"}
# Los huesos de Mixamo (los de una malla ya riggeada, como rigged-model.glb) y los del maniqui que llevan sus pesos.
MIXAMO = {"Hips": "pelvis", "Spine": "spine_01", "Spine1": "spine_02", "Spine2": "spine_03", "Neck": "neck_01",
          "Head": "head", "LeftShoulder": "clavicle_l", "LeftArm": "upperarm_l", "LeftForeArm": "lowerarm_l",
          "LeftHand": "hand_l", "RightShoulder": "clavicle_r", "RightArm": "upperarm_r", "RightForeArm": "lowerarm_r",
          "RightHand": "hand_r", "LeftUpLeg": "thigh_l", "LeftLeg": "calf_l", "LeftFoot": "foot_l",
          "LeftToeBase": "ball_l", "RightUpLeg": "thigh_r", "RightLeg": "calf_r", "RightFoot": "foot_r",
          "RightToeBase": "ball_r"}
# Los huesos del rig de la Universal Animation Library de Quaternius (UAL1 y UAL2, CC0, y su maniqui femenino), que son
# los del maniqui salvo la cabeza: los dedos, a la mano (sus pesos; en los clips no se mueven).
PAR_UAL = {n: n for n in MIXAMO.values()}
PAR_UAL["Head"] = PAR_UAL.pop("head")
DEDOS_UAL = {f"{d}_0{k}_{lado}": "hand_" + lado for d in ("index", "middle", "pinky", "ring", "thumb")
             for k in ("1", "2", "3", "4_leaf") for lado in ("l", "r")}


def par_de(nombre):
    """El hueso del maniqui de un hueso de la malla riggeada (Mixamo o UAL), o None."""
    return MIXAMO.get(nombre) or PAR_UAL.get(nombre) or DEDOS_UAL.get(nombre)


# Los clips del juego que salen de UAL1 y UAL2 (--ual), llevados al maniqui como los de la malla (de_la_malla), con la
# ventana del clip que se usa (en segundos; None: entero). Los que la biblioteca no trae, o arreglaba con otro clip.
DE_UAL = {
    "llevar": ("Walk_Carry_Loop", None),            # los brazos delante, cargando (era Walk_Formal)
    "hablar": ("Idle_Talking_Loop", None),          # gesticula (eran los brazos cruzados)
    "lanzar": ("OverhandThrow", None),              # por encima del hombro (era Throw_Object)
    "lanzar_lazo": ("OverhandThrow", None),
    "lanzar_trepa": ("OverhandThrow", None),
    "forjar": ("TreeChopping_Loop", None),          # golpes desde arriba, uno tras otro (era un tajo de espada)
    "estocada_lanza": ("Sword_Dash", None),         # se tira adelante con el brazo extendido (era un puñetazo)
    "ataque_una_3": ("Sword_Regular_C", None),      # el remate del combo, distinto de los otros dos
    "construir": ("Fixing_Kneeling", (1.0, 4.1)),   # de rodillas, trabajando con las manos (sin pararse en el bucle)
    "agacharse_trabajar": ("Fixing_Kneeling", (1.0, 4.1)),
    "curar": ("Fixing_Kneeling", (1.0, 4.1)),
    "encender": ("Interact", None),                 # de pie, con las manos delante (era agacharse a cosechar)
    "ensillar": ("Interact", None),
    "cocinar": ("Interact", None),
}
# Los rapidos (golpes, lanzamientos, martillazos), a 24 cuadros por segundo; los lentos, a 15 (el GLB pesa menos y el
# juego interpola).
UAL_RAPIDOS = {"lanzar", "lanzar_lazo", "lanzar_trepa", "forjar", "estocada_lanza", "ataque_una_3", "correr"}
# Los de una malla sin clips propios (el maniqui femenino): los de su biblioteca, en vez de los de exported-model.glb.
DE_UAL_SIN_MALLA = {"idle": ("Idle_Loop", None), "caminar": ("Walk_Loop", None), "correr": ("Sprint_Loop", None)}
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
def mundo_nodos(doc, cambios=None):
    """La matriz de mundo de cada nodo del GLB (traslacion, giro y escala, o su matriz); con cambios ({nodo: {path:
    valor}}, los de un cuadro de un clip: muestrear), en esa pose."""
    nodos, cambios = doc["nodes"], cambios or {}
    padre = {c: i for i, n in enumerate(nodos) for c in n.get("children", [])}
    W = {}
    def mundo(i):
        if i not in W:
            n, c = nodos[i], cambios.get(i, {})
            if "matrix" in n and not c:
                M = np.array(n["matrix"], float).reshape(4, 4).T
            else:
                q = np.array(c.get("rotation", n.get("rotation", [0, 0, 0, 1])), float)
                M = np.eye(4)
                M[:3, :3] = qmat(q / np.linalg.norm(q)) * np.array(c.get("scale", n.get("scale", [1, 1, 1])), float)
                M[:3, 3] = c.get("translation", n.get("translation", [0, 0, 0]))
            W[i] = mundo(padre[i]) @ M if i in padre else M
        return W[i]
    return [mundo(i) for i in range(len(nodos))]


def mat_q(M):
    """El cuaternion (x, y, z, w) del giro de una matriz (con escala uniforme)."""
    m = M[:3, :3] / np.linalg.norm(M[:3, :3], axis=0)
    t = np.trace(m)
    if t > 0:
        r = 2.0 * np.sqrt(t + 1.0)
        q = [(m[2, 1] - m[1, 2]) / r, (m[0, 2] - m[2, 0]) / r, (m[1, 0] - m[0, 1]) / r, 0.25 * r]
    elif m[0, 0] > m[1, 1] and m[0, 0] > m[2, 2]:
        r = 2.0 * np.sqrt(1.0 + m[0, 0] - m[1, 1] - m[2, 2])
        q = [0.25 * r, (m[0, 1] + m[1, 0]) / r, (m[0, 2] + m[2, 0]) / r, (m[2, 1] - m[1, 2]) / r]
    elif m[1, 1] > m[2, 2]:
        r = 2.0 * np.sqrt(1.0 + m[1, 1] - m[0, 0] - m[2, 2])
        q = [(m[0, 1] + m[1, 0]) / r, 0.25 * r, (m[1, 2] + m[2, 1]) / r, (m[0, 2] - m[2, 0]) / r]
    else:
        r = 2.0 * np.sqrt(1.0 + m[2, 2] - m[0, 0] - m[1, 1])
        q = [(m[0, 2] + m[2, 0]) / r, (m[1, 2] + m[2, 1]) / r, 0.25 * r, (m[1, 0] - m[0, 1]) / r]
    q = np.array(q)
    return q / np.linalg.norm(q)


def canales(doc, binario, anim):
    """Los canales de un clip de un GLB: (nodo, path, tiempos, valores)."""
    out = []
    for ch in anim["channels"]:
        smp = anim["samplers"][ch["sampler"]]
        ts = acc(doc, binario, smp["input"])[:, 0].astype(float)
        vs = acc(doc, binario, smp["output"]).astype(float)
        if smp.get("interpolation") == "CUBICSPLINE":  # tangente, valor, tangente: el valor
            vs = vs.reshape(len(ts), 3, -1)[:, 1]
        out.append((ch["target"]["node"], ch["target"]["path"], ts, vs))
    return out


def muestrear(chans, t):
    """Los cambios de cada nodo animado a los t segundos (lineal; el giro, por el camino corto)."""
    out = {}
    for nodo, path, ts, vs in chans:
        k = int(np.clip(np.searchsorted(ts, t, side="right") - 1, 0, len(ts) - 1))
        v = vs[k]
        if k + 1 < len(ts):
            f = float(np.clip((t - ts[k]) / max(ts[k + 1] - ts[k], 1e-9), 0.0, 1.0))
            b = vs[k + 1]
            if path == "rotation" and v @ b < 0:
                b = -b
            v = v * (1 - f) + b * f
        out.setdefault(nodo, {})[path] = v
    return out


def de_la_malla(esq, fuente, clip, T, R, k, fps=24.0, mapa=MIXAMO, ventana=None):
    """Un clip del esqueleto de Mixamo de la malla riggeada (Idle, Walk, Sprint), o de uno de UAL (mapa=PAR_UAL), en el
    del maniqui (ventana: de que segundo a que segundo del clip; None, entero):
    - cada hueso con su par (MIXAMO) gira en el mundo lo mismo que el, desde la pose T de los dos (la de la malla: la
      del maniqui se ajusto a ella en armar);
    - los demas (los dedos, los de giro), quietos respecto de su padre;
    - la pelvis sube, baja y se mece como la cadera, sin avanzar (el juego mueve al personaje).
    T, R: la pose T del maniqui; k: de las medidas de la malla a las del esqueleto."""
    doc, binario, nodos_piel, nombres = fuente
    chans = canales(doc, binario, next(a for a in doc["animations"] if a["name"] == clip))
    dur = max(ts[-1] for _, _, ts, _ in chans)
    t0, t1 = ventana if ventana else (0.0, dur)
    W0 = mundo_nodos(doc)
    par = {esq.idx[mapa[n]]: nodo for n, nodo in zip(nombres, nodos_piel) if n in mapa}
    q0 = {i: mat_q(W0[nodo]) for i, nodo in par.items()}
    _, Q0 = esq.mundo(T, R)
    pel = esq.idx["pelvis"]
    cadera = par[pel]
    tiempos = np.linspace(0.0, t1 - t0, max(2, int(round((t1 - t0) * fps)) + 1))
    giros, mov = [], []
    for t in tiempos:
        W = mundo_nodos(doc, muestrear(chans, t0 + t))
        Q, Rl = np.zeros_like(Q0), R.copy()
        for i in esq.order:
            p = esq.parent[i]
            if i in par:
                Q[i] = qmul(qmul(mat_q(W[par[i]]), qconj(q0[i])), Q0[i])
            else:
                Q[i] = qmul(Q[p], R[i]) if p >= 0 else R[i]
            Rl[i] = qmul(qconj(Q[p]), Q[i]) if p >= 0 else Q[i]
        giros.append(Rl)
        mov.append((W[cadera][:3, 3] - W0[cadera][:3, 3]) * k)
    mov = np.array(mov)
    for eje in (0, 2):  # sin avanzar: lo de lado y lo de adelante, sin su tendencia ni su media
        mov[:, eje] -= np.linspace(mov[0, eje], mov[-1, eje], len(mov))
        mov[:, eje] -= mov[:, eje].mean()
    padre = esq.parent[pel]
    poses = []
    for Rl, d in zip(giros, mov):
        Tk = T.copy()
        Tk[pel] = T[pel] + (qrot(qconj(Q0[padre]), d) if padre >= 0 else d)
        poses.append((Tk, Rl))
    # Solo los huesos con par (y la pelvis): los demas no se mueven respecto de su padre, como en el reposo del GLB.
    return [c for c in clip_de_poses(esq, poses, tiempos) if c[0] in par or c[1] == "translation"]


def biblioteca_ual(path):
    """Una biblioteca de la Universal Animation Library (UAL1_Standard.glb, UAL2_Standard.glb: los clips sin root
    motion), para de_la_malla: su documento, sus huesos y la altura de su pelvis en el reposo (la pose T)."""
    doc, binario = leer(path)
    skin = doc["skins"][0]
    nombres = [doc["nodes"][k].get("name", "") for k in skin["joints"]]
    W = mundo_nodos(doc)
    return doc, binario, skin["joints"], nombres, float(W[skin["joints"][nombres.index("pelvis")]][1, 3])


def malla(path):
    """La malla en pose T: posiciones, triangulos, el color de su textura en cada vertice y, si ya viene riggeada, sus
    pesos (los huesos de cada vertice, sus pesos y los nombres de los huesos, sin el prefijo de Mixamo); si no, None.
    La riggeada, en la pose de su piel (la de reposo de su esqueleto)."""
    doc, binario = leer(path)
    prims = doc["meshes"][0]["primitives"]  # todas: el maniqui de UAL trae una por material
    prim = prims[0]
    pos = np.concatenate([acc(doc, binario, p["attributes"]["POSITION"]).astype(float) for p in prims])
    uv = acc(doc, binario, prim["attributes"]["TEXCOORD_0"]).astype(float)
    base = np.cumsum([0] + [doc["accessors"][p["attributes"]["POSITION"]]["count"] for p in prims])
    idx = np.concatenate([acc(doc, binario, p["indices"])[:, 0].astype(np.int64).reshape(-1, 3) + base[i]
                          for i, p in enumerate(prims)])
    color = np.full((len(pos), 3), 180.0)
    mat = doc.get("materials", [{}])[prim.get("material", 0)]
    tex = mat.get("pbrMetallicRoughness", {}).get("baseColorTexture") if len(prims) == 1 else None
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
    pesos = None
    if "JOINTS_0" in prim["attributes"] and doc.get("skins"):
        skin = doc["skins"][0]
        W = mundo_nodos(doc)
        ibm = acc(doc, binario, skin["inverseBindMatrices"]).astype(float).reshape(-1, 4, 4).transpose(0, 2, 1)
        piel = np.stack([W[k] @ ibm[x] for x, k in enumerate(skin["joints"])])
        jo = np.concatenate([acc(doc, binario, p["attributes"]["JOINTS_0"]).astype(np.int64) for p in prims])
        we = np.concatenate([acc(doc, binario, p["attributes"]["WEIGHTS_0"]).astype(float) for p in prims])
        vh = np.column_stack([pos, np.ones(len(pos))])
        pos = sum(we[:, q:q + 1] * np.einsum("nij,nj->ni", piel[jo[:, q]], vh)[:, :3] for q in range(4))
        nombres = [doc["nodes"][k].get("name", "").split(":")[-1].replace("mixamorig", "") for k in skin["joints"]]
        articulaciones = np.array([W[k][:3, 3] for k in skin["joints"]])  # en el espacio de la piel, como pos
        pesos = (jo, we, nombres, (doc, binario, skin["joints"], nombres), articulaciones)
    return pos, idx, color, pesos


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
    # Sin las astillas que deja la reduccion de una punta del cuerpo a otra (un pie con una mano, a mas de un tercio del
    # alto). Los lados largos de un cuerpo de 1200 triangulos (un muslo, el antebrazo: hasta un quinto del alto) se
    # quedan: sin ellos la malla quedaba con agujeros y el brazo, en tiras sueltas que en los golpes eran astillas.
    t = p2[f2]
    lado = np.max(np.linalg.norm(t - np.roll(t, 1, axis=1), axis=2), axis=1)
    f2 = f2[lado < 0.3 * (pos[:, 1].max() - pos[:, 1].min())]
    # Ni los restos sueltos de la reduccion: dos triangulos espalda con espalda, de milimetros, junto a los dedos.
    from scipy.sparse import coo_matrix
    from scipy.sparse.csgraph import connected_components
    e = np.concatenate([f2[:, [0, 1]], f2[:, [1, 2]]])
    _, pieza = connected_components(coo_matrix((np.ones(len(e)), (e[:, 0], e[:, 1])), shape=(len(p2), len(p2))), directed=False)
    f2 = f2[np.bincount(pieza)[pieza[f2[:, 0]]] >= 8]
    _, near = cKDTree(pos).query(p2)
    return p2.astype(float), f2.astype(np.int64), color[near], near


# ------------------------------------------------------------------ armar un id
def armar(esq, malla_t, it, log=print, ual=()):
    ancho, alto, largo = (float(x) for x in it["medidas_m"].split("x"))
    tmax = int(it["tris_max"]) or 1200
    T, R = esq.pose_inicial("Idle_A")
    P, _ = esq.mundo(T, R)
    alto_esq = P[esq.idx["head_leaf"]][1]  # del suelo (la raiz) a la punta de la cabeza
    log(f"  Idle_A: raiz {P[esq.idx['root']].round(3)}, pies {P[esq.idx['ball_leaf_l']][1]:.3f}/{P[esq.idx['ball_leaf_r']][1]:.3f}, cabeza {alto_esq:.3f}")
    # La malla a la escala del esqueleto: los pies en el suelo, la coronilla en la punta de la cabeza.
    pos, idx, color, pesos = malla_t
    lo, hi = pos.min(0), pos.max(0)
    k = alto_esq / (hi[1] - lo[1])
    torso = pos[(pos[:, 1] > lo[1] + 0.45 * (hi[1] - lo[1])) & (pos[:, 1] < lo[1] + 0.7 * (hi[1] - lo[1])) &
                (np.abs(pos[:, 0] - 0.5 * (lo[0] + hi[0])) < 0.08 * (hi[1] - lo[1]))]
    pel = P[esq.idx["pelvis"]]
    def a_esqueleto(p):  # de las medidas de la malla a las del esqueleto
        return np.column_stack([(p[:, 0] - torso[:, 0].mean()) * k + pel[0], (p[:, 1] - lo[1]) * k,
                                (p[:, 2] - torso[:, 2].mean()) * k + pel[2]])
    m = a_esqueleto(pos)
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
    if pesos is not None:
        # Cada hueso del maniqui con par, donde esta su articulacion en el esqueleto de la malla: los pesos se pintaron
        # alrededor de esas (si no, la piel gira alrededor de otro punto y se estira: el hombro estaba a 12 cm de la suya
        # y la rodilla, a 15). Solo cambia el largo y la direccion de cada hueso en el
        # reposo (la traslacion desde su padre), no sus giros; la pelvis queda en la del esqueleto (la de los clips).
        donde = a_esqueleto(pesos[4])
        for i in esq.order:
            n = esq.names[i]
            if n == "pelvis" or n not in MIXAMO.values():
                continue
            P, Q = esq.mundo(T, R)
            j = [par_de(h) for h in pesos[2]].index(n)
            T[i] = qrot(qconj(Q[esq.parent[i]]), donde[j] - P[esq.parent[i]])
        P, _ = esq.mundo(T, R)
        lejos = max(np.linalg.norm(P[esq.idx[par_de(h)]] - d) for h, d in zip(pesos[2], donde)
                    if (h in MIXAMO or h in PAR_UAL) and par_de(h) != "pelvis")  # sin los dedos (van a la mano)
        log(f"  articulaciones en las de la malla (a {lejos * 1000:.1f} mm como mucho)")
    PT, QT = esq.mundo(T, R)
    # Idle, Walk y Sprint de la malla riggeada, si los trae: idle, caminar y correr (los demas, de la biblioteca).
    del_juego = dict(CLIPS)
    if pesos is not None:
        fuente = pesos[3]
        tiene = {a["name"] for a in fuente[0].get("animations", [])}
        for juego, clip in DE_LA_MALLA.items():
            if clip in tiene:
                esq.clips["_malla_" + clip] = de_la_malla(esq, fuente, clip, T, R, k)
                del_juego[juego] = "_malla_" + clip
        log(f"  de la malla: {', '.join(j for j, c in DE_LA_MALLA.items() if del_juego[j] == '_malla_' + c) or 'ninguno'}")
    # Los de UAL1 y UAL2 (--ual), llevados al maniqui igual; sin clips propios en la malla, tambien idle, caminar y correr.
    if ual:
        de_ual = dict(DE_UAL)
        if not any(v.startswith("_malla_") for v in del_juego.values()):
            de_ual.update(DE_UAL_SIN_MALLA)
        pelvis_y = PT[esq.idx["pelvis"]][1]
        for juego, (clip, ventana) in de_ual.items():
            fuente = next((b for b in ual if clip in {a["name"] for a in b[0].get("animations", [])}), None)
            if fuente is None:
                log(f"  {juego}: {clip} no esta en las bibliotecas de UAL; queda {del_juego[juego]}")
                continue
            nombre = "_ual_" + clip + ("" if ventana is None else f"_{ventana[0]:g}_{ventana[1]:g}")
            if nombre not in esq.clips:
                k_ual = pelvis_y / fuente[4]  # de las medidas de UAL a las del esqueleto, por la altura de la pelvis
                esq.clips[nombre] = de_la_malla(esq, fuente[:4], clip, T, R, k_ual, fps=24.0 if juego in UAL_RAPIDOS else 15.0,
                                                mapa=PAR_UAL, ventana=ventana)
            del_juego[juego] = nombre
        log(f"  de UAL: {', '.join(j for j in de_ual if del_juego[j].startswith('_ual_'))}")
    # La malla reducida, cara por cara, y los pesos (en la pose T).
    v, f, col, cerca = reducir(m, idx, color, tmax - 20)
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
    if pesos is not None:  # la malla riggeada: los pesos de su vertice mas cercano, en los huesos del maniqui
        jo, we, huesos = pesos[:3]
        faltan = [huesos[u] for u in np.unique(jo[we > 1e-6]) if par_de(huesos[u]) is None]  # los que llevan peso
        if faltan:
            raise SystemExit(f"huesos de la malla sin su par en el maniqui (MIXAMO, PAR_UAL): {', '.join(faltan)}")
        mapa = np.array([esq.idx[par_de(h)] if par_de(h) else 0 for h in huesos])
        joints[:] = mapa[jo[cerca]]
        weights[:] = we[cerca] / np.maximum(we[cerca].sum(axis=1, keepdims=True), 1e-9)
        nombres = []  # sin los automaticos
        log(f"  pesos de la malla riggeada: {len(huesos)} huesos en los del maniqui")
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
    for kk in range(4):
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
                     joints[f].reshape(-1, 4), weights[f].reshape(-1, 4), ibm, it["id"], del_juego)
    armar.ultimo = dict(v=v, joints=joints, weights=weights, skin=skin, PT=PT, PI=PI, parte=parte)
    caja = vi.max(0) - vi.min(0)
    log(f"{it['id']}: {len(f)} triangulos, {caja[0]:.2f} x {caja[1]:.2f} x {caja[2]:.2f} m (largo x alto x ancho), "
        f"{len(CLIPS)} clips")
    return datos, len(f)


def escribir(esq, T, R, root, yaw, s, pos, nor, col, joints, weights, ibm, nombre, del_juego=None):
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
    # Los clips: los tiempos y los valores de la biblioteca, una vez por clip usado; los del juego, nombres. Los tiempos,
    # una vez por cada lista distinta (los canales de un clip llevado al maniqui comparten la suya: el GLB pesa menos).
    usados, anims, tiempos = {}, [], {}
    for juego, fuente in (del_juego or CLIPS).items():
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
                clave = t.astype(np.float32).tobytes()
                if clave not in tiempos:
                    tiempos[clave] = accesor(t.astype(np.float32).reshape(-1, 1), 5126, "SCALAR", minmax=True)
                samplers.append({"input": tiempos[clave],
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
    args, ual = sys.argv[1:], []
    if "--ual" in args:  # --ual <UAL1_Standard.glb> <UAL2_Standard.glb>: los clips de DE_UAL, de ahi
        i = args.index("--ual")
        ual = [biblioteca_ual(p) for p in args[i + 1:] if p.endswith(".glb")]
        args = args[:i] + [a for a in args[i + 1:] if not a.endswith(".glb")]
    if len(args) < 3:
        raise SystemExit(__doc__)
    esq = Esqueleto(args[0])
    sintetizar(esq)
    malla_t = malla(args[1])
    items, errores = inventario.load()
    if errores:
        raise SystemExit("\n".join(errores))
    por_id = {it["id"]: it for it in items}
    filas = open(inventario.TSV, encoding="utf-8").read().split("\n")
    malla_de = (f"la malla riggeada de arte/fuentes/humanoide ({os.path.basename(args[1])}), en la pose de su piel, reducida, con el "
                "color de su textura y sus pesos (los de su esqueleto, en los huesos del maniqui)" if malla_t[3] is not None
                else "la malla en pose T de IMPORTED/Humanoid (sample_*.glb), reducida y con el color de su textura, con "
                "pesos automaticos")
    con_clips = malla_t[3] is not None and any(a["name"] in DE_LA_MALLA.values() for a in malla_t[3][3][0].get("animations", []))
    de_malla = (" (salvo idle, caminar y correr: Idle, Walk y Sprint de la malla, llevados al maniqui)" if con_clips else "")
    if ual:
        de_malla += ("; llevar, hablar, lanzar, forjar, la estocada, el remate, construir, curar, encender, ensillar y "
                     "cocinar" + ("" if con_clips else ", idle, caminar y correr") + ", de UAL1 y UAL2 (Quaternius, CC0)")
    nota = (f" Cuerpo humano estandar (tools/modelos/humanoide.py): {malla_de}; el esqueleto y los clips, de "
            f"exported-model.glb (los nombres de la Universal Animation Library de Quaternius){de_malla}; los clips, con "
            "los nombres del juego.")
    vieja = re.compile(r" ?Cuerpo humano estandar \(tools/modelos/humanoide\.py\):.*?con los nombres del juego\.")
    for mid in args[2:]:
        it = por_id[mid]
        datos, tris = armar(esq, malla_t, it, ual=ual)
        ruta = os.path.join(ROOT, inventario.path_of(it))
        os.makedirs(os.path.dirname(ruta), exist_ok=True)
        open(ruta, "wb").write(datos)
        for i, fila in enumerate(filas):
            c = fila.split("\t")
            if c[0] == mid:
                c[5] = "importado"
                c[6] = vieja.sub("", c[6]) + nota
                filas[i] = "\t".join(c)
    open(inventario.TSV, "w", encoding="utf-8").write("\n".join(filas))
    print("Sigue: python3 tools/assets/inventario.py doc && python3 tools/assets/animaciones.py doc")


if __name__ == "__main__":
    main()
