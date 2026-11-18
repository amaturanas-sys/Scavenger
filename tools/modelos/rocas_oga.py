#!/usr/bin/env python3
"""Las rocas de OpenGameArt del decorado (src/sim/decor.c, de la cresta a la roca baja), cerradas y con bloques de
voxeles: cada hueco de la malla (el lado vacio que se veia al girarlas o en una ladera) se tapa con un abanico desde
el centro de su borde, y se incrustan unos bloques cubicos (la mitad adentro de la roca), con el color de la cara
donde caen y la luz horneada del sol (lowres.SUN), dentro de la caja original: las medidas del inventario no cambian.

  python3 tools/modelos/rocas_oga.py          # las que faltan (las hechas llevan "rocas_oga" en su nota)
"""
import hashlib
import json
import os
import re
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "assets"))
import inventario  # noqa: E402
import lowres  # noqa: E402

ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
NOTA = " Cerrada (sin lados vacíos) y con bloques de vóxeles (tools/modelos/rocas_oga.py)."


def ids_rocas():
    """Los ids de las rocas del decorado: de [DECOR_CRESTA] a [DECOR_ROCA_BAJA] en src/sim/decor.c."""
    src = open(os.path.join(ROOT, "src", "sim", "decor.c"), encoding="utf-8").read()
    tramo = src[src.index("[DECOR_CRESTA]"):src.index("[DECOR_ACACIA]")]
    return re.findall(r'\] = \{ "([^"]+)"', tramo)


def leer(data):
    largo = struct.unpack_from("<I", data, 12)[0]
    doc = json.loads(data[20:20 + largo])
    binario = data[20 + largo + 8:]
    prim = doc["meshes"][0]["primitives"][0]

    def acc(i, n, dtype):
        a = doc["accessors"][i]
        v = doc["bufferViews"][a["bufferView"]]
        return np.frombuffer(binario, dtype, a["count"] * n, v.get("byteOffset", 0) + a.get("byteOffset", 0)).reshape(-1, n)

    pos = acc(prim["attributes"]["POSITION"], 3, np.float32).astype(np.float64)
    ca = doc["accessors"][prim["attributes"]["COLOR_0"]]
    ncol = 4 if ca["type"] == "VEC4" else 3
    col = acc(prim["attributes"]["COLOR_0"], ncol, np.uint8)[:, :3].astype(np.float64)
    return pos, col, doc["meshes"][0].get("name", "roca")


def caras_normales(pos):
    t = pos.reshape(-1, 3, 3)
    n = np.cross(t[:, 1] - t[:, 0], t[:, 2] - t[:, 0])
    largo = np.linalg.norm(n, axis=1, keepdims=True)
    return n / np.maximum(largo, 1e-12), largo[:, 0] * 0.5


def tapar(pos, col):
    """Tapa los huecos: los bordes que usa una sola cara, en lazos, con un abanico al revés de su cara."""
    clave = np.round(pos / 1e-4).astype(np.int64)
    _, idx = np.unique(clave, axis=0, return_inverse=True)
    idx = idx.reshape(-1, 3)
    cuenta = {}
    for f in idx:
        for a, b in ((f[0], f[1]), (f[1], f[2]), (f[2], f[0])):
            k = (min(a, b), max(a, b))
            cuenta[k] = cuenta.get(k, 0) + 1
    sale = {}  # el borde, al reves: b -> a
    punto, color = {}, {}
    for fi, f in enumerate(idx):
        for j, (a, b) in enumerate(((f[0], f[1]), (f[1], f[2]), (f[2], f[0]))):
            punto[a], color[a] = pos[fi * 3 + j], col[fi * 3 + j]
            if cuenta[(min(a, b), max(a, b))] == 1:
                sale.setdefault(b, []).append(a)
    tris, cols = [], []
    while sale:
        ini = next(iter(sale))
        lazo, v = [ini], ini
        while True:
            sig = sale[v].pop()
            if not sale[v]:
                del sale[v]
            if sig == ini or sig not in sale:
                break
            lazo.append(sig)
            v = sig
        if len(lazo) < 3:
            continue
        p = np.array([punto[i] for i in lazo])
        c = p.mean(axis=0)
        cm = np.array([color[i] for i in lazo]).mean(axis=0) * 0.8
        for i in range(len(lazo)):
            tris.append([p[i], p[(i + 1) % len(lazo)], c])
            cols.append(cm)
    if not tris:
        return pos, col, 0
    t = np.array(tris).reshape(-1, 3)
    return np.vstack([pos, t]), np.vstack([col, np.repeat(np.array(cols), 3, axis=0)]), len(tris)


def bloques(pos, col, semilla):
    """Bloques de voxeles incrustados: 6 a 10, de 6 a 11 % de la diagonal, la mitad adentro, en la caja original."""
    rng = np.random.default_rng(semilla)
    lo, hi = pos.min(axis=0), pos.max(axis=0)
    diag = float(np.linalg.norm(hi - lo))
    normales, areas = caras_normales(pos)
    arriba = np.clip(normales[:, 1], 0.0, 1.0) + 0.15  # mas en lo que mira arriba y a los lados
    peso = areas * arriba
    peso /= peso.sum()
    sol = np.array(lowres.SUN, dtype=np.float64)
    sol /= np.linalg.norm(sol)
    caras = np.array([[0, 1, 0], [0, -1, 0], [1, 0, 0], [-1, 0, 0], [0, 0, 1], [0, 0, -1]], dtype=np.float64)
    tris, cols = [], []
    for _ in range(int(rng.integers(6, 11))):
        f = int(rng.choice(len(peso), p=peso))
        a, b = rng.random(2)
        if a + b > 1.0:
            a, b = 1.0 - a, 1.0 - b
        t = pos[f * 3:f * 3 + 3]
        p = t[0] + a * (t[1] - t[0]) + b * (t[2] - t[0])
        lado = diag * float(rng.uniform(0.06, 0.11))
        centro = p - normales[f] * lado * 0.25
        base = col[f * 3] * float(rng.uniform(0.86, 1.08)) / (lowres.AMBIENT + lowres.DIFFUSE * max(0.0, float(normales[f] @ sol)))
        for n in caras:
            u = np.roll(n, 1) if n[1] == 0 else np.array([1.0, 0, 0])
            v = np.cross(n, u)
            c0 = centro + n * lado / 2
            q = [c0 + (su * u + sv * v) * lado / 2 for su, sv in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
            if np.cross(q[1] - q[0], q[2] - q[0]) @ n < 0:
                q = q[::-1]
            luz = lowres.AMBIENT + lowres.DIFFUSE * max(0.0, float(n @ sol))
            for tri in ((q[0], q[1], q[2]), (q[0], q[2], q[3])):
                tris.append(np.clip(np.array(tri), lo, hi))
                cols.append(np.clip(base * luz, 0, 255))
    t = np.array(tris).reshape(-1, 3)
    return np.vstack([pos, t]), np.vstack([col, np.repeat(np.array(cols), 3, axis=0)])


def main():
    items, errores = inventario.load()
    if errores:
        raise SystemExit("\n".join(errores))
    por_id = {it["id"]: it for it in items}
    filas = open(inventario.TSV, encoding="utf-8").read().split("\n")
    hechas = 0
    for mid in ids_rocas():
        it = por_id.get(mid)
        if not it or "rocas_oga" in it.get("nota", ""):
            continue
        ruta = os.path.join(ROOT, inventario.path_of(it))
        pos, col, nombre = leer(open(ruta, "rb").read())
        pos, col, huecos = tapar(pos, col)
        semilla = int(hashlib.sha256(mid.encode()).hexdigest()[:8], 16)
        pos, col = bloques(pos, col, semilla)
        normales, _ = caras_normales(pos)
        data = lowres._glb_bytes(pos.astype(np.float32), np.repeat(normales, 3, axis=0).astype(np.float32),
                                 col.astype(np.uint8), nombre)
        open(ruta, "wb").write(data)
        tris = len(pos) // 3
        for i, fila in enumerate(filas):
            c = fila.split("\t")
            if c[0] == mid:
                if c[4].isdigit() and tris > int(c[4]):
                    c[4] = str(tris)
                c[6] = c[6] + NOTA
                filas[i] = "\t".join(c)
        print(f"{mid}: {huecos} tapas, {tris} triángulos")
        hechas += 1
    open(inventario.TSV, "w", encoding="utf-8").write("\n".join(filas))
    print(f"{hechas} rocas. Sigue: python3 tools/assets/inventario.py doc")


if __name__ == "__main__":
    main()
