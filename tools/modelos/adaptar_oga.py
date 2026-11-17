#!/usr/bin/env python3
"""Adapta a la estética del juego los modelos de OpenGameArt que entraron grises.

Los modelos del paquete FREE Remastered Meadows (EmacEArt, CC0) se importaron sin sus texturas
(tools/importar_oga_masivo.py): lowres.convertir no encontró color, todos quedaron gris carbón, y
varios quedaron a la escala del paquete (cercas de 7 a 13 m de alto, un banco de 6,6 m de largo).
Este script no toca la forma:
  - les da color por partes con la paleta del juego: la madera de las estructuras de voxeles, la roca y
    el pasto del terreno, la corteza y la hoja de los árboles (src/world/terrain.c, trees.c), con un poco
    de variación por cara;
  - hornea la luz del sol como el terreno y lowres.py (SUN, AMBIENT, DIFFUSE);
  - y, si la receta da una altura, lo pone a esa altura real (escala uniforme desde el centro de la base).
Reescribe el GLB en su ruta y, en assets/inventario.tsv, las medidas y la nota de su fila. Después:
  python3 tools/assets/inventario.py doc        (docs/INVENTARIO.md sale de ahí: no editarlo a mano)
y mirar cada modelo en el juego (skill estetica). Repetirlo da lo mismo: el color se calcula de nuevo
desde la forma, y la escala va a una altura, no por un factor.

Uso:
  python3 tools/modelos/adaptar_oga.py            # todos los de RECETAS
  python3 tools/modelos/adaptar_oga.py <id> ...   # solo esos
  python3 tools/modelos/adaptar_oga.py --girar <id> <x|y|z> <grados>
      un arreglo de una vez (no se repite solo): gira el modelo antes de adaptarlo, con la base otra vez en el
      origen. Lo ya girado queda anotado en GIRADOS.
"""
import json
import os
import struct
import sys

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, HERE)
import lowres  # noqa: E402  (SUN, AMBIENT, DIFFUSE y el GLB facetado)

INVENTARIO = os.path.join(ROOT, "assets", "inventario.tsv")
NOTA = "Color por partes con la paleta del juego y luz horneada (tools/modelos/adaptar_oga.py)"

# La paleta, antes de la luz (la luz horneada la multiplica por 0,55 a 1).
MADERA = [(138, 114, 86), (126, 104, 80), (148, 122, 92)]         # madera vieja: cercas, puentes, tablas
MADERA_TIBIA = [(150, 106, 64), (138, 98, 60), (158, 114, 70)]    # cofres, barril, carro
HIERRO = [(84, 82, 80), (74, 72, 70)]
ROCA = [(138, 134, 124), (126, 122, 114), (146, 142, 130)]        # la roca del terreno
LIQUEN = (132, 136, 106)
ARENISCA = [(194, 166, 128), (182, 154, 118), (202, 176, 138)]
ARENISCA_ARRIBA = (210, 190, 150)
ROCA_PANTANO = [(128, 128, 116), (118, 120, 106), (136, 136, 122)]
MUSGO_ARRIBA = (110, 124, 86)
ESTRATOS = [(150, 144, 132), (136, 130, 120), (158, 150, 136), (128, 122, 112)]
PASTO = [(176, 162, 118), (166, 152, 110), (184, 170, 126)]       # la estepa seca
TIERRA = [(150, 120, 86), (140, 112, 80)]
CORTEZA = [(100, 74, 50), (92, 68, 46), (110, 82, 56)]
CORTEZA_MUERTA = [(120, 106, 90), (110, 96, 80), (128, 114, 96)]
HOJA = [(80, 110, 64), (70, 100, 58), (90, 118, 70)]
MUSGO = [(106, 118, 76), (96, 110, 70)]
RAMA_SECA = [(160, 136, 100), (146, 124, 92), (170, 146, 108)]
SETA_PIE, SETA_PUNTA = (222, 208, 178), (214, 160, 100)

# id: (material, alto en m o None para dejar la escala)
RECETAS = {
    "estructura.cerca.fence_wood_01a": ("madera", 1.5),
    "estructura.cerca.fence_wood_01b": ("madera", 1.6),
    "estructura.cerca.fence_wood_01c": ("madera", 1.4),
    "estructura.cerca.fence_wood_02b": ("madera", 1.8),
    "estructura.cerca.fence_wood_02c": ("madera", 1.6),
    "estructura.cerca.fence_wood_03a": ("madera", 1.7),
    "estructura.cerca.fence_wood_03b": ("madera", 1.6),
    "estructura.cerca.fence_wood_04a": ("madera", 1.2),
    "estructura.cerca.wood_fence3": ("madera", 1.6),
    "estructura.puente.bridge_02_1": ("madera", None),
    "estructura.puente.bridge_pier_04b": ("madera", None),
    "estructura.puente.bridge_pier_04c": ("madera", None),
    "estructura.puente.bridge_pier_end_04a": ("madera", None),
    "mapa.roca.rock_01c": ("roca", None),
    "mapa.roca.rock_flat_01c": ("roca", None),
    "mapa.roca.rock_sand_01c": ("arenisca", None),
    "mapa.roca.rock_sand_01e": ("arenisca", None),
    "mapa.roca.swamp_rockbig_02a": ("roca_pantano", None),
    "mapa.roca.swamp_rockbig_02c": ("roca_pantano", None),
    "mapa.roca_acantilado.clif_01c": ("acantilado", None),
    "mapa.roca_acantilado.clif_01d": ("acantilado", None),
    "mapa.roca_acantilado.clif_01e": ("acantilado", None),
    "mapa.terreno_colina.hill_01a": ("colina", None),
    "mapa.terreno_colina.hill_01b": ("colina", None),
    "mapa.terreno_colina.hill_01d": ("colina", None),
    "mapa.terreno_colina.hill_half_01a": ("colina", None),
    "mapa.terreno_colina.hill_half_01c": ("colina", None),
    "mapa.terreno_colina.hill_low_01c": ("colina", None),
    # Los árboles, a la escala de los del mundo (el pino mide 10 m, el sauce 9 y el arce 11: assets/inventario.tsv).
    "mapa.vegetacion_arbol.tree_05d_1": ("arbol", 9.0),
    "mapa.vegetacion_arbol.tree_06d_2": ("arbol_musgo", 8.0),
    "mapa.vegetacion_arbol.tree_old_06_1": ("tronco_muerto", 7.0),
    "mapa.vegetacion_arbusto.bush_dried_02": ("arbusto_seco", 1.6),
    "mapa.vegetacion_seta.mushroom_09": ("seta", 0.35),
    "utileria.generico.prop_wehicule_cart_01": ("madera_tibia", 1.9),
    "utileria.generico.props_bench_02": ("madera", 0.95),
    "utileria.generico.props_container_barell_01a": ("madera_herrajes", None),
    "utileria.generico.props_container_chest_01a": ("madera_herrajes", 0.75),
    "utileria.generico.props_container_chest_01b": ("madera_herrajes", 0.9),
    "utileria.objeto_generico.ea01_env_branch_04": ("corteza", None),
    "utileria.objeto_generico.ea01_env_road_wooden_planks_03b": ("madera", None),
    "utileria.objeto_generico.ea01_env_treeroot_01c": ("corteza", None),
}


# Los que ya se giraron con --girar (2026-11-18), con el giro neto desde como entraron: los muelles (los pilares
# del puente) tenían la plataforma parada como una pared (quedan con la plataforma abajo y los pilotes arriba),
# y el cofre abierto, acostado de espaldas.
GIRADOS = {
    "estructura.puente.bridge_pier_04c": ("x", 90),
    "estructura.puente.bridge_pier_end_04a": ("x", 90),
    "utileria.generico.props_container_chest_01b": ("x", 90),
}


def leer_glb(data):
    """Las posiciones (3 por cara) de un GLB facetado de lowres.py: una malla, sin índices."""
    if data[:4] != b"glTF":
        raise ValueError("no es un GLB")
    largo = struct.unpack_from("<I", data, 12)[0]
    doc = json.loads(data[20:20 + largo])
    binario = data[20 + largo + 8:]
    prims = [p for m in doc["meshes"] for p in m["primitives"]]
    if len(prims) != 1 or "indices" in prims[0]:
        raise ValueError("se espera una sola malla facetada sin índices (la salida de lowres.py)")
    acc = doc["accessors"][prims[0]["attributes"]["POSITION"]]
    vista = doc["bufferViews"][acc["bufferView"]]
    ini = vista.get("byteOffset", 0) + acc.get("byteOffset", 0)
    pos = np.frombuffer(binario, "<f4", acc["count"] * 3, ini).reshape(-1, 3).astype(np.float64)
    if len(pos) % 3:
        raise ValueError("las posiciones no son múltiplo de 3")
    return pos, doc["nodes"][0].get("name", "modelo")


def partes(pos):
    """La parte de cada cara: las caras que comparten vértices (en la misma posición) son una parte.
    Numeradas de la mayor a la menor."""
    claves = np.round(pos / 1e-4).astype(np.int64)
    _, vert = np.unique(claves, axis=0, return_inverse=True)
    vert = vert.ravel()
    padre = np.arange(vert.max() + 1)

    def raiz(x):
        while padre[x] != x:
            padre[x] = padre[padre[x]]
            x = padre[x]
        return x

    tri = vert.reshape(-1, 3)
    for a, b, c in tri:
        ra = raiz(a)
        padre[raiz(b)] = ra
        padre[raiz(c)] = ra
    etiqueta = np.array([raiz(t[0]) for t in tri])
    _, inv, cuenta = np.unique(etiqueta, return_inverse=True, return_counts=True)
    orden = np.argsort(-cuenta, kind="stable")
    rango = np.empty_like(orden)
    rango[orden] = np.arange(len(orden))
    return rango[inv.ravel()], cuenta[orden]


def _mezcla(a, b, t):
    return np.asarray(a, float) * (1.0 - t) + np.asarray(b, float) * t


def _por_parte(parte, paleta):
    """Un tono de la paleta por parte (la madera de cada tabla, la piedra de cada bloque)."""
    return np.asarray(paleta, float)[(parte * 5 + 1) % len(paleta)]


def colores_base(material, normal, centro, parte, cuenta, alto01):
    """El color de cada cara antes de la luz."""
    ny = normal[:, 1]
    if material in ("madera", "madera_tibia"):
        return _por_parte(parte, MADERA if material == "madera" else MADERA_TIBIA)
    if material == "madera_herrajes":  # las piezas chicas (aros, asas, cerraduras) son de hierro
        base = _por_parte(parte, MADERA_TIBIA)
        hierro = cuenta[parte] < 0.25 * cuenta[0]
        base[hierro] = _por_parte(parte[hierro], HIERRO)
        return base
    if material in ("roca", "arenisca", "roca_pantano"):
        paleta, arriba, t = {"roca": (ROCA, LIQUEN, 0.35), "arenisca": (ARENISCA, ARENISCA_ARRIBA, 0.5),
                             "roca_pantano": (ROCA_PANTANO, MUSGO_ARRIBA, 0.5)}[material]
        base = _por_parte(parte, paleta)
        tope = ny > 0.8
        base[tope] = _mezcla(base[tope], arriba, t)
        return base
    if material == "acantilado":  # estratos de unos 2,2 m, como los muros del borde; arriba, pasto
        banda = np.floor((centro[:, 1] - centro[:, 1].min()) / 2.2).astype(int)
        base = np.asarray(ESTRATOS, float)[banda % len(ESTRATOS)]
        tope = ny > 0.8
        base[tope] = _mezcla(base[tope], PASTO[0], 0.7)
        return base
    if material == "colina":  # pasto en lo llano, tierra en la ladera, roca en lo empinado
        base = _por_parte(parte, ROCA)
        ladera = (ny > 0.4) & (ny <= 0.75)
        base[ladera] = _por_parte(parte[ladera], TIERRA)
        llano = ny > 0.75
        base[llano] = _por_parte(parte[llano], PASTO)
        return base
    if material in ("arbol", "arbol_musgo"):  # las partes grandes son el tronco y las ramas
        tronco = cuenta[parte] >= 300
        base = _por_parte(parte, HOJA if material == "arbol" else MUSGO)
        base[tronco] = _por_parte(parte[tronco], CORTEZA if material == "arbol" else CORTEZA_MUERTA)
        if material == "arbol":
            base[~tronco] *= (0.92 + 0.16 * alto01[~tronco])[:, None]  # la copa, más clara arriba
        return base
    if material == "tronco_muerto":
        return _por_parte(parte, CORTEZA_MUERTA)
    if material == "arbusto_seco":
        return _por_parte(parte, RAMA_SECA)
    if material == "corteza":
        return _por_parte(parte, CORTEZA)
    if material == "seta":  # el pie claro y las puntas ocre, en cada hongo
        t = np.zeros(len(parte))
        for p in np.unique(parte):
            m = parte == p
            y = centro[m, 1]
            t[m] = (y - y.min()) / max(y.max() - y.min(), 1e-9)
        return _mezcla(SETA_PIE, SETA_PUNTA, (t ** 1.5)[:, None])
    raise ValueError(f"material desconocido: {material}")


def adaptar(data, material, alto=None, girar=None):
    """Devuelve (bytes del GLB adaptado, informe). girar: (eje, grados), antes de todo lo demás."""
    pos, nombre = leer_glb(data)
    if girar is not None:  # con la base otra vez en el origen
        pos = pos @ lowres._rotation(girar[0], girar[1]).T
        lo, hi = pos.min(axis=0), pos.max(axis=0)
        pos = (pos - np.array([(lo[0] + hi[0]) * 0.5, lo[1], (lo[2] + hi[2]) * 0.5])).astype(np.float32).astype(np.float64)
    escala = 1.0
    if alto is not None:  # primero la escala, así el color sale de la forma final y repetirlo da lo mismo
        lo, hi = pos.min(axis=0), pos.max(axis=0)
        escala = float(alto / (hi[1] - lo[1]))
        if abs(escala - 1.0) > 1e-6:
            centro_base = np.array([(lo[0] + hi[0]) * 0.5, lo[1], (lo[2] + hi[2]) * 0.5])
            pos = (pos - centro_base) * escala  # el origen, en el centro de la base
            pos = pos.astype(np.float32).astype(np.float64)  # como queda en el GLB: el color sale igual al repetir
        else:
            escala = 1.0
    tri = pos.reshape(-1, 3, 3)
    normal = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    largo = np.linalg.norm(normal, axis=1)
    normal = np.where(largo[:, None] > 1e-12, normal / np.maximum(largo, 1e-12)[:, None], [0.0, 1.0, 0.0])
    centro = tri.mean(axis=1)
    lo, hi = pos.min(axis=0), pos.max(axis=0)
    alto01 = (centro[:, 1] - lo[1]) / max(hi[1] - lo[1], 1e-9)
    parte, cuenta = partes(pos)
    base = colores_base(material, normal, centro, parte, cuenta, alto01)
    # Un poco de variación por cara (fija: sale de la posición), como la textura de los demás modelos.
    ruido = np.modf(np.abs(np.sin(centro @ np.array([12.9898, 78.233, 37.719])) * 43758.5453))[0]
    base = base * (0.96 + 0.08 * ruido)[:, None]
    color = base * (lowres.AMBIENT + lowres.DIFFUSE * np.clip(normal @ lowres.SUN, 0.0, 1.0))[:, None]
    vcolor = np.repeat(np.clip(np.rint(color), 0, 255), 3, axis=0)
    salida = lowres._glb_bytes(pos, np.repeat(normal, 3, axis=0), vcolor, nombre)
    lo, hi = pos.min(axis=0), pos.max(axis=0)
    caja = hi - lo  # X largo, Y alto, Z ancho
    return salida, {"caras": len(tri), "partes": int(len(cuenta)), "escala": escala,
                    "medidas": f"{caja[2]:.2f}x{caja[1]:.2f}x{caja[0]:.2f}"}


def ruta_glb(mid):
    return os.path.join(ROOT, "assets", "models", *mid.split(".")) + ".glb"


def actualizar_inventario(cambios):
    """cambios: {id: (medidas, a escala, girado)}. Reescribe las medidas y agrega la nota (una vez) en su fila."""
    with open(INVENTARIO, encoding="utf-8") as fh:
        lineas = fh.read().split("\n")
    vistos = set()
    for i, linea in enumerate(lineas):
        col = linea.split("\t")
        if len(col) >= 7 and col[0] in cambios:
            medidas, escalado, girado = cambios[col[0]]
            col[3] = medidas
            if "adaptar_oga.py" not in col[6]:
                col[6] = (col[6].rstrip(". ") + ". " + NOTA + ".").lstrip(". ")
            for hecho, texto in ((escalado, "a escala real"), (girado, "reorientado")):
                if hecho and texto not in col[6]:
                    col[6] = col[6][:-1] + ", " + texto + "." if col[6].endswith(".") else col[6] + ", " + texto
            lineas[i] = "\t".join(col)
            vistos.add(col[0])
    faltan = set(cambios) - vistos
    if faltan:
        raise SystemExit(f"no están en el inventario: {', '.join(sorted(faltan))}")
    with open(INVENTARIO, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lineas))


def main(argv=None):
    args = list(argv if argv is not None else sys.argv[1:])
    girar = None
    if args[:1] == ["--girar"]:
        if len(args) != 4 or args[2] not in ("x", "y", "z"):
            raise SystemExit("uso: --girar <id> <x|y|z> <grados>")
        girar = (args[2], float(args[3]))
        args = [args[1]]
    ids = args or list(RECETAS)
    desconocidos = [m for m in ids if m not in RECETAS]
    if desconocidos:
        raise SystemExit(f"sin receta: {', '.join(desconocidos)}")
    cambios = {}
    for mid in ids:
        material, alto = RECETAS[mid]
        ruta = ruta_glb(mid)
        with open(ruta, "rb") as fh:
            data, informe = adaptar(fh.read(), material, alto, girar)
        with open(ruta, "wb") as fh:
            fh.write(data)
        cambios[mid] = (informe["medidas"], alto is not None, mid in GIRADOS)
        print(f"{mid}: {material}, {informe['partes']} partes, escala {informe['escala']:.3f}, {informe['medidas']} m")
    actualizar_inventario(cambios)
    print(f"{len(ids)} modelos adaptados. Sigue: python3 tools/assets/inventario.py doc")
    return 0


if __name__ == "__main__":
    sys.exit(main())
