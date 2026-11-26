#!/usr/bin/env python3
"""La silla de montar y donde se sienta el jinete, desde la referencia del usuario (IMPORTED/References 3d/Model
horserider 01.glb: un caballo con su silla y estribos, y un cuerpo en pose T con la cadera en el asiento).

  python3 tools/modelos/jinete.py [--fuente "<Model horserider 01.glb>"]

Con --fuente guarda en arte/fuentes/patron/jinete.glb lo que se usa de la referencia: el caballo y el cuerpo,
reducidos, y la silla entera, en el marco del juego (el frente a +X, la izquierda a -Z, el suelo del caballo en y = 0)
y en metros (el cuerpo mide 1.70 m). Sin el, lo lee de ahi. Despues:
- mide (medir) el asiento: lo mas bajo de arriba de la silla, en el medio (ahi va la entrepierna del jinete); en el
  caballo, que parte del camino de las patas de atras a las de adelante y cuanto sobre el lomo (lo que la silla se
  levanta: su arco sobre la cruz y su relleno); y los estribos: cuanto bajo el asiento, cuanto atras y a que lado;
- arma la silla (accesorio.arreo.silla_montar): la de la referencia, a la escala del cuerpo, reducida, de cuero y con
  los estribos de hierro; el frente a +X y el origen en la base (lo mas bajo de los estribos);
- escribe src/world/monturas.inc: donde esta el asiento en el modelo de la silla y cuanto sube la pelvis del jinete
  sobre el asiento (holgura: la del protagonista, de la entrepierna a la pelvis).
El juego pone la silla y al jinete en el hueso asiento de cada montura (rigs.py, con asiento_de: el mismo lugar del
lomo que en la referencia y la pelvis del jinete encima). La pose del jinete (humanoide.py, _montar) lleva los pies a
los estribos (estribos_del_jinete)."""
import os
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import animales  # noqa: E402
import inventario  # noqa: E402  (animales pone tools/assets en el camino)
import lowres  # noqa: E402
import patron  # noqa: E402

ROOT = patron.ROOT
REFERENCIA = os.path.join(ROOT, "arte", "fuentes", "patron", "jinete.glb")
INC = os.path.join(ROOT, "src", "world", "monturas.inc")
SILLA = "accesorio.arreo.silla_montar"
PROTAGONISTA = os.path.join(ROOT, "assets", "models", "personaje", "narrativo", "protagonista.glb")
ALTO_CUERPO = 1.70  # m: el cuerpo de la referencia (el comun del juego)
NODOS = {"cuerpo": 0, "silla": 10, "caballo": 12}  # en la referencia del usuario
CUERO, HIERRO = np.array([112.0, 70.0, 40.0]), np.array([82.0, 80.0, 78.0])
# Del marco de la referencia (el frente a +Z, la izquierda a +X) al del juego (rigs.G).
G = np.array([[0.0, 0.0, 1.0], [0.0, 1.0, 0.0], [-1.0, 0.0, 0.0]])


# ------------------------------------------------------------------ la referencia
def fuente(ruta, tris=6000):
    """La referencia del usuario, reducida a lo que se usa, en arte/fuentes/patron/jinete.glb."""
    import trimesh
    doc, b = patron.leer(ruta)
    piezas = {}
    for nombre, nodo in NODOS.items():
        vs, fs, base = [], [], 0
        for pr in doc["meshes"][doc["nodes"][nodo]["mesh"]]["primitives"]:
            v = patron.acc(doc, b, pr["attributes"]["POSITION"]).astype(float)
            vs.append(v)
            fs.append(patron.acc(doc, b, pr["indices"]).reshape(-1, 3).astype(np.int64) + base)
            base += len(v)
        piezas[nombre] = [np.vstack(vs) @ G.T, np.vstack(fs)]
    suelo = piezas["caballo"][0][:, 1].min()
    zc = 0.5 * (piezas["caballo"][0][:, 2].min() + piezas["caballo"][0][:, 2].max())
    k = ALTO_CUERPO / float(np.ptp(piezas["cuerpo"][0][:, 1]))
    escena = trimesh.Scene()
    for nombre, (v, f) in piezas.items():
        v = (v - [0.0, suelo, zc]) * k
        if nombre != "silla" and len(f) > tris:
            v, f, _ = animales.reducir(v, f, np.zeros((len(v), 3)), tris)
        escena.add_geometry(trimesh.Trimesh(v, f, process=False), node_name=nombre, geom_name=nombre)
    os.makedirs(os.path.dirname(REFERENCIA), exist_ok=True)
    escena.export(REFERENCIA)
    print(f"{os.path.relpath(REFERENCIA, ROOT)}: el cuerpo, la silla y el caballo ({k:.4f} m por unidad)")


def cargar():
    """{nombre: (v, f)} de arte/fuentes/patron/jinete.glb."""
    import trimesh
    sc = trimesh.load(REFERENCIA, force="scene")
    return {n: (np.asarray(g.vertices, float), np.asarray(g.faces, np.int64)) for n, g in sc.geometry.items()}


def _lomo(P, m, x):
    """Lo mas alto del lomo en x (el medio del ancho)."""
    s = P[(np.abs(P[:, 0] - x) < 0.03 * m["L"]) & (np.abs(P[:, 2]) < 0.15 * m["W"])]
    return float(s[:, 1].max())


def medir(piezas=None):
    """Lo medido en la referencia (metros, marco del juego): el asiento de la silla y su parte del camino de las patas
    de atras a las de adelante (parte), cuanto sobre el lomo (sobre_lomo), los estribos respecto del asiento (atras,
    abajo y al costado) y la silla (v, f, su caja)."""
    piezas = piezas or cargar()
    sv, sf = piezas["silla"]
    cv, cf = piezas["caballo"]
    P = animales.muestras(cv, cf, n=60000)
    m = animales.medir(P)
    # El cuerpo de la silla (sin lo que cuelga) y su perfil de arriba en el medio: el asiento, lo mas bajo entre el
    # borrén de adelante y el de atras (en puntos de su superficie: los vertices son pocos y en una franja angosta
    # pueden quedar solo los de abajo).
    S = animales.muestras(sv, sf, n=120000)
    alto = S[:, 1] > np.percentile(S[:, 1], 35)
    x0, x1 = np.percentile(S[alto, 0], [5, 95])
    medio = S[(np.abs(S[:, 2]) < 0.04) & (S[:, 0] > x0) & (S[:, 0] < x1)]
    xs = np.linspace(x0 + 0.25 * (x1 - x0), x1 - 0.25 * (x1 - x0), 15)
    arriba = np.array([medio[np.abs(medio[:, 0] - x) < 0.02][:, 1].max() for x in xs])
    asiento = np.array([xs[np.argmin(arriba)], arriba.min(), 0.0])
    estribos = []
    for lado in (-1.0, 1.0):
        s = sv[sv[:, 2] * lado > 0.1]
        estribos.append(s[s[:, 1] < s[:, 1].min() + 0.03].mean(axis=0))
    e = np.mean([[p[0], p[1], abs(p[2])] for p in estribos], axis=0)
    return dict(parte=float((asiento[0] - m["xt"]) / (m["xd"] - m["xt"])), sobre_lomo=float(asiento[1] - _lomo(P, m, asiento[0])),
                asiento=asiento, estribo_atras=float(asiento[0] - e[0]), estribo_abajo=float(asiento[1] - e[1]),
                estribo_lado=float(e[2]), silla=(sv, sf), caballo_cruz=m, caja=(sv.min(0), sv.max(0)))


_MEDIDO = {}


def medido():
    if not _MEDIDO:
        _MEDIDO.update(medir())
    return _MEDIDO


def _pelvis(ruta=PROTAGONISTA):
    """La pelvis del protagonista en su pose de reposo (m) y su malla."""
    doc, b = patron.leer(ruta)
    sk = doc["skins"][0]
    nombres = [doc["nodes"][j].get("name") for j in sk["joints"]]
    B = np.linalg.inv(patron.acc(doc, b, sk["inverseBindMatrices"]).reshape(-1, 4, 4).transpose(0, 2, 1))
    return B[nombres.index("pelvis")][:3, 3], patron.acc(doc, b, doc["meshes"][0]["primitives"][0]["attributes"]["POSITION"]).astype(float)


def pelvis_del_protagonista():
    """Lo alto de la pelvis del protagonista en su pose de reposo (m): la escala de estribos_del_jinete."""
    return float(_pelvis()[0][1])


def holgura(ruta=PROTAGONISTA):
    """Cuanto sube la pelvis del protagonista sobre su entrepierna (en su pose de reposo): sentado, la entrepierna va en
    el asiento y la pelvis, eso mas arriba."""
    pelvis, v = _pelvis(ruta)
    cerca = v[(np.abs(v[:, 0] - pelvis[0]) < 0.05) & (np.abs(v[:, 2] - pelvis[2]) < 0.025) & (v[:, 1] > 0.5 * pelvis[1])]
    return float(pelvis[1] - cerca[:, 1].min())


def asiento_de(v, f, valle=False):
    """Donde va la pelvis del jinete en una montura (v, f: de frente a +X, el suelo en y = 0, el medio en z = 0): el
    asiento, en la misma parte del lomo que en la referencia (valle: en el camello bactriano, entre las gibas) y la
    silla encima, y la holgura del jinete sobre el."""
    med = medido()
    P = animales.muestras(v, f, n=60000)
    m = animales.medir(P)
    x = m["xt"] + med["parte"] * (m["xd"] - m["xt"])
    if valle:  # lo mas bajo del lomo entre las gibas (entre el 30 y el 80 % del camino de las patas de atras a las de adelante)
        xs = np.linspace(m["xt"] + 0.3 * (m["xd"] - m["xt"]), m["xt"] + 0.8 * (m["xd"] - m["xt"]), 26)
        x = float(xs[np.argmin([_lomo(P, m, a) for a in xs])])
    return np.array([x, _lomo(P, m, x) + med["sobre_lomo"] + holgura(), 0.0])


def estribos_del_jinete():
    """Donde van los tobillos del jinete respecto de su pelvis (adelante, arriba, al costado; m): bajo el asiento como
    en la referencia, con el pie apoyado en el estribo por la planta (el tobillo, 9 cm atras y 5 cm arriba)."""
    med = medido()
    return np.array([-med["estribo_atras"] - 0.09, -(med["estribo_abajo"] + holgura()) + 0.05, med["estribo_lado"]])


# ------------------------------------------------------------------ la silla
def silla(it):
    """El GLB de la silla (la de la referencia, a la escala del cuerpo): reducida a su tris_max, el cuero y el hierro
    de los estribos con la luz del sol horneada, el frente a +X y el origen en la base. Devuelve el GLB, el asiento en
    su marco y su caja (ancho x alto x largo)."""
    med = medido()
    v, f = med["silla"]
    lo, hi = v.min(0), v.max(0)
    origen = np.array([0.5 * (lo[0] + hi[0]), lo[1], 0.5 * (lo[2] + hi[2])])
    v = v - origen
    hierro = v[:, 1] < 0.17  # los estribos: lo de abajo (sus correas, de cuero, arriba de eso)
    col = np.where(hierro[:, None], HIERRO[None], CUERO[None])
    vr, fr, cr = animales.reducir(v, f, col, int(it["tris_max"]) or 400)
    tri = vr[fr]
    n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    n /= np.maximum(np.linalg.norm(n, axis=1, keepdims=True), 1e-12)
    sol = np.array(lowres.SUN, float)
    sol /= np.linalg.norm(sol)
    caras = np.clip(cr[fr].mean(axis=1) * (0.72 + 0.38 * np.clip(n @ sol, 0.0, 1.0))[:, None], 0, 255)
    datos = lowres._glb_bytes(tri.reshape(-1, 3), lowres.normales_aristas(vr, fr).reshape(-1, 3), np.repeat(caras, 3, axis=0),
                              it["id"])
    c = vr.max(0) - vr.min(0)
    return datos, med["asiento"] - origen, f"{c[2]:.2f}x{c[1]:.2f}x{c[0]:.2f}", len(fr)


def escribir_inc(asiento_silla, h):
    med = medido()
    lineas = [
        "// Generado por tools/modelos/jinete.py desde arte/fuentes/patron/jinete.glb (la referencia del jinete del",
        "// usuario): no editar a mano.",
        "// El asiento en el modelo de la silla (accesorio.arreo.silla_montar: el frente a +X, el origen en la base), en m:",
        "// lo mas bajo de arriba de la silla, en el medio, donde va la entrepierna del jinete.",
        f"#define SADDLE_SEAT_X {asiento_silla[0]:.4f}f",
        f"#define SADDLE_SEAT_Y {asiento_silla[1]:.4f}f",
        "// Cuanto sube la pelvis del jinete sobre el asiento (la del protagonista sobre su entrepierna). El hueso asiento",
        "// de cada montura (tools/modelos/rigs.py) va ahi: donde va la pelvis del jinete.",
        f"#define RIDER_SEAT_CLEARANCE {h:.4f}f",
        f"// En la referencia: el asiento, a {med['parte']:.3f} del camino de las patas de atras a las de adelante y",
        f"// {med['sobre_lomo']:.3f} m sobre el lomo; los estribos, {med['estribo_abajo']:.3f} m bajo el asiento, "
        f"{med['estribo_atras']:.3f} m atras y {med['estribo_lado']:.3f} m",
        "// a cada lado (la pose del jinete: tools/modelos/humanoide.py).",
    ]
    open(INC, "w", encoding="utf-8").write("\n".join(lineas) + "\n")
    print(f"{os.path.relpath(INC, ROOT)}: el asiento en la silla y la holgura del jinete")


def main():
    if len(sys.argv) > 2 and sys.argv[1] == "--fuente":
        fuente(sys.argv[2])
    elif len(sys.argv) > 1:
        raise SystemExit(__doc__)
    med = medido()
    h = holgura()
    print(f"asiento: a {med['parte']:.3f} de las patas de atras a las de adelante, {med['sobre_lomo']:.3f} m sobre el lomo; "
          f"estribos {med['estribo_abajo']:.3f} m abajo, {med['estribo_atras']:.3f} m atras, {med['estribo_lado']:.3f} m al costado; "
          f"holgura del jinete {h:.3f} m")
    filas = open(inventario.TSV, encoding="utf-8").read().split("\n")
    k = next(i for i, f in enumerate(filas) if f.startswith(SILLA + "\t"))
    items, _ = inventario.load()
    it = {i["id"]: i for i in items}[SILLA]
    datos, asiento_silla, caja, n = silla(it)
    ruta = os.path.join(ROOT, inventario.path_of(it))
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    open(ruta, "wb").write(datos)
    print(f"{os.path.relpath(ruta, ROOT)}: {n} triangulos, {caja} m (ancho x alto x largo); el asiento a "
          f"{np.round(asiento_silla, 3)}")
    c = filas[k].split("\t")
    c[3], c[5] = caja, "importado"
    c[6] = ("La de la referencia del jinete del usuario (IMPORTED/References 3d/Model horserider 01.glb), a la escala del "
            "cuerpo y con los estribos colgando; va en el asiento de la montura (tools/modelos/jinete.py).")
    filas[k] = "\t".join(c)
    open(inventario.TSV, "w", encoding="utf-8").write("\n".join(filas))
    escribir_inc(asiento_silla, h)


if __name__ == "__main__":
    main()
