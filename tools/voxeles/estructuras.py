#!/usr/bin/env python3
"""Las estructuras de voxeles, armadas a mano: cada una es una receta de cajas en celdas de 0.5 m, con la
distribucion de su hoja de tres vistas ajustada a las medidas del inventario, y el color sale de la hoja, piedra
por piedra.

  python3 tools/voxeles/estructuras.py templo --hoja HOJA.png -o assets/models/estructura/culto/templo.vox \\
      --vista-previa templo.png

Por que a mano: lo que sale de las siluetas (voxeles.py hoja) no tiene entrantes. Una escalera entre dos torres
sale como una rampa de lado a lado, la puerta queda pintada en la pared y las torres se funden con el monticulo.
Aca cada parte es una caja con nombre (el zocalo, las terrazas, los pilonos, la escalera, la puerta...).

El color: cada celda a la vista toma la vista de la hoja que mira su cara (el frente, el lado o la espalda; las
de arriba, la que diga su parte) y despues cada piedra (hiladas trabadas, del alto y el largo de su parte) queda
de un solo tono, la mediana de sus celdas con un poco de variacion. Asi se ven los sillares de la hoja y no el
ruido de sus pixeles. La hoja (frente | lado | espalda, como en voxeles.py hoja) no se sube; sin --hoja salen
los colores base de cada parte, para mirar la forma.

Ejes de la receta: u, el largo desde el frente (u = 0 es la cara de adelante, la que mira a +X en el juego);
z, el ancho desde -Z; y, el alto. Al guardar pasan a los del .vox (vox.py: x, y hacia atras y z arriba).
"""
import argparse
import os
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import vox  # noqa: E402
import voxeles  # noqa: E402

CELDA = 0.5  # metros por celda, como los sitios grandes del kit (src/world/voxstruct.c)
HUECO = (40, 32, 26)  # lo oscuro de una puerta, sin luz
# La hoja ya trae su luz y el juego sombrea cada cara otra vez (src/sim/voxel.c: 0.82 el frente, 0.68 a 0.88 los
# lados): sin aclarar, todo queda mas apagado que en la hoja.
LUZ = 1.15


class Obra:
    """Una rejilla (u, z, y) con la parte de cada celda (0: vacia) y el estilo de cada parte:
    (alto de la hilada, largo de la piedra, color sin hoja, vista de las caras de arriba)."""

    def __init__(self, largo, ancho, alto):
        self.parte = np.zeros((largo, ancho, alto), np.uint8)
        self.nombres = [""]
        self.estilo = {}

    @property
    def lleno(self):
        return self.parte > 0

    def id(self, nombre):
        if nombre not in self.nombres:
            if nombre not in self.estilo:
                raise KeyError(f"la parte {nombre} no tiene estilo")
            self.nombres.append(nombre)
        return self.nombres.index(nombre)

    def caja(self, nombre, u, z, y, espejo=False):
        """Llena la caja (rangos inclusivos de celdas) con la parte; espejo: tambien su reflejo en z."""
        for zz in ((z, self._refleja(z)) if espejo else (z,)):
            self.parte[u[0]:u[1] + 1, zz[0]:zz[1] + 1, y[0]:y[1] + 1] = self.id(nombre)

    def vaciar(self, u, z, y, espejo=False):
        for zz in ((z, self._refleja(z)) if espejo else (z,)):
            self.parte[u[0]:u[1] + 1, zz[0]:zz[1] + 1, y[0]:y[1] + 1] = 0

    def marcar(self, nombre, u, z, y, espejo=False):
        """Cambia la parte de lo que ya esta lleno en la caja (el fondo de un hueco, por ejemplo)."""
        k = self.id(nombre)
        for zz in ((z, self._refleja(z)) if espejo else (z,)):
            sub = self.parte[u[0]:u[1] + 1, zz[0]:zz[1] + 1, y[0]:y[1] + 1]
            sub[sub > 0] = k

    def _refleja(self, z):
        n = self.parte.shape[1]
        return (n - 1 - z[1], n - 1 - z[0])

    def metros(self):
        """(ancho Z, alto Y, largo X) en metros, como las medidas del inventario."""
        u, z, y = self.parte.shape
        return z * CELDA, y * CELDA, u * CELDA


# ---------------------------------------------------------------------------------------------- recetas
def templo():
    """estructura.culto.templo: 24 x 14 x 30 m. Zocalo, cuatro terrazas que bajan hacia atras, dos pilonos con
    cornisa y paneles rehundidos, el portico con la puerta y la escalera entre los pilonos (un escalon por celda,
    de 0.5 m), con sus pretiles. La hoja: frente con los pilonos y la escalera, lado con las terrazas, espalda con
    los pilonos asomando atras."""
    o = Obra(60, 48, 28)
    o.estilo.update({  # las hiladas de 3 celdas van con los escalones de las terrazas (cada una tiene 2)
        "zocalo": (3, 6, (150, 140, 122), "lado"),
        "terraza": (3, 5, (176, 140, 96), "lado"),
        "pilono": (2, 4, (184, 150, 106), "lado"),
        "panel": (2, 3, (150, 118, 80), "lado"),
        "cornisa": (1, 3, (198, 172, 130), "lado"),
        "portico": (2, 3, (182, 150, 110), "frente"),
        "pretil": (2, 4, (170, 150, 120), "frente"),
        "escalera": (1, 4, (152, 138, 118), "frente"),
        "puerta": (0, 0, HUECO, "frente"),
    })
    # El zocalo, y la terraza de abajo (D) con su escalinata de atras: baja un escalon por celda hasta el zocalo.
    o.caja("zocalo", (0, 59), (0, 47), (0, 5))
    o.caja("terraza", (16, 53), (3, 44), (6, 11))
    for k in range(5):
        o.caja("terraza", (54 + k, 54 + k), (3, 44), (6, 10 - k))
    # Las terrazas C, B y A (la de A es la que sostiene el portico) y el bloque de arriba.
    o.caja("terraza", (19, 49), (8, 39), (12, 17))
    o.caja("terraza", (19, 42), (13, 34), (18, 20))
    o.caja("terraza", (19, 41), (16, 31), (21, 22))
    o.caja("terraza", (28, 38), (18, 29), (23, 24))
    # Los pilonos: una repisa al frente (la vista de lado), dos escalones hacia afuera (la de frente), el cuerpo,
    # la faja y la cornisa que vuela una celda.
    o.caja("terraza", (8, 8), (5, 15), (6, 8), espejo=True)
    o.caja("terraza", (8, 15), (3, 4), (6, 11), espejo=True)
    o.caja("pilono", (8, 15), (5, 7), (6, 17), espejo=True)
    o.caja("pilono", (9, 15), (8, 15), (6, 25), espejo=True)
    o.caja("cornisa", (9, 15), (8, 15), (26, 26), espejo=True)
    o.caja("cornisa", (8, 16), (7, 16), (27, 27), espejo=True)
    # Paneles rehundidos una celda, solo donde se ven (un hueco tapado suma caras que nadie mira): en el frente
    # de cada pilono, arriba de la repisa, y en su cara de afuera, arriba de los escalones.
    o.vaciar((9, 9), (10, 12), (10, 23), espejo=True)
    o.marcar("panel", (10, 10), (10, 12), (10, 23), espejo=True)
    o.vaciar((11, 14), (8, 8), (18, 24), espejo=True)
    o.marcar("panel", (11, 14), (9, 9), (18, 24), espejo=True)
    # La escalera entre los pilonos: del suelo (u = 0) a la puerta, un escalon por celda; los pretiles un metro
    # mas altos que el escalon (y nunca mas bajos que el zocalo).
    for u in range(16):
        o.vaciar((u, u), (18, 29), (u + 1, 27))
        o.caja("escalera", (u, u), (18, 29), (0, u))
        o.caja("pretil", (u, u), (16, 17), (0, max(5, u + 2)), espejo=True)
    # El portico: avanza desde la terraza A hasta la escalera, con su faja y la cornisa que vuela; la puerta
    # (3 m de alto y 2 de ancho) entra hasta la terraza, y su fondo queda a oscuras.
    o.caja("portico", (16, 18), (18, 29), (6, 22))
    o.caja("cornisa", (16, 18), (18, 29), (22, 22))
    o.caja("cornisa", (15, 18), (17, 17), (22, 22), espejo=True)
    o.caja("cornisa", (15, 15), (18, 29), (22, 22))
    o.vaciar((16, 18), (22, 25), (16, 21))
    # Adentro del vano (el piso, las jambas y el fondo): la cara del portico y su techo quedan de piedra.
    o.marcar("puerta", (17, 19), (21, 26), (15, 21))
    return o


RECETAS = {"templo": templo}

# Donde cae cada altura en cada vista de la hoja: (celdas, pixeles desde el suelo de esa vista) y el alto total en
# pixeles de la vista recortada. La hoja del templo exagera lo alto en el frente (los pilonos miden 340 px de
# frente y 300 de lado y de espalda): por eso un tramo por terraza y no una sola escala.
ALTURAS = {
    "templo": {
        "frente": ([0, 6, 12, 18, 23, 28], [0, 67, 135, 195, 285, 340], 346),
        "lado": ([0, 6, 12, 18, 21, 23, 25, 28], [0, 66.5, 132, 191, 222, 248, 270, 299], 299),
        "espalda": ([0, 6, 12, 18, 21, 23, 25, 28], [0, 69, 138, 198, 228, 255, 268, 304], 305),
    },
}


# ---------------------------------------------------------------------------------------------- color
def rellenar(mask, rgb, borde=2):
    """El color de la vista sin el borde (el antialias del fondo blanco tiñe 1 o 2 pixeles): todo pixel toma el
    del pixel mas cercano del nucleo (la mascara erosionada)."""
    from scipy import ndimage

    core = ndimage.binary_erosion(mask, iterations=borde)
    if not core.any():
        core = mask
    _, (iy, ix) = ndimage.distance_transform_edt(~core, return_indices=True)
    return rgb[iy, ix]


def _caras(lleno):
    """Las caras al aire de cada celda: (frente -u, espalda +u, -z, +z, arriba +y)."""
    p = np.pad(lleno, 1)
    c = p[1:-1, 1:-1, 1:-1]
    return {
        "frente": c & ~p[:-2, 1:-1, 1:-1],
        "espalda": c & ~p[2:, 1:-1, 1:-1],
        "lado": c & (~p[1:-1, :-2, 1:-1] | ~p[1:-1, 2:, 1:-1]),
        "arriba": c & ~p[1:-1, 1:-1, 2:],
        "abajo": c & ~p[1:-1, 1:-1, :-2],
    }


def _tono(clave, amplitud=0.06):
    """Un factor de brillo estable para cada piedra (el mismo en cada corrida)."""
    h = zlib.crc32(repr(clave).encode()) / 0xFFFFFFFF
    return 1.0 + amplitud * (2.0 * h - 1.0)


def colorear(obra, vistas=None, alturas=None, luz=LUZ):
    """(rgb (u, z, y, 3) de las celdas a la vista, mascara de las celdas a la vista). vistas: [(mascara, rgb)] *
    3 de la hoja (frente, lado, espalda), o None para los colores base de cada parte. Las partes sin piedras (alto
    de hilada 0, como el hueco de la puerta) llevan siempre su color base. luz: cuanto se aclara lo de la hoja."""
    lleno = obra.lleno
    L, W, H = lleno.shape
    caras = _caras(lleno)
    vis = caras["frente"] | caras["espalda"] | caras["lado"] | caras["arriba"] | caras["abajo"]
    rgb = np.zeros(lleno.shape + (3,), np.float32)
    nombres = obra.nombres
    # Enteros de Python: el tono de cada piedra sale del texto de su clave, y el de los enteros de numpy cambia
    # con la version (np.int64(3) o 3); asi el .vox sale igual en cualquier maquina.
    celdas_vis = np.argwhere(vis).tolist()

    # La vista de cada celda: la de la cara mas importante que tiene al aire.
    cual = np.full(lleno.shape, "", dtype=object)
    for u, z, y in celdas_vis:
        estilo = obra.estilo[nombres[obra.parte[u, z, y]]]
        if caras["frente"][u, z, y]:
            cual[u, z, y] = "frente"
        elif caras["lado"][u, z, y]:
            cual[u, z, y] = "lado"
        elif caras["espalda"][u, z, y]:
            cual[u, z, y] = "espalda"
        else:
            cual[u, z, y] = "arriba-" + estilo[3]

    if vistas is not None:
        planas = {n: rellenar(m, c) for n, (m, c) in zip(("frente", "lado", "espalda"), vistas)}

        def fila(vista, y0, y1):
            celdas, px, alto = alturas[vista]
            h = planas[vista].shape[0]
            a, b = np.interp([y0, y1], celdas, px) * h / alto
            return int(np.clip(h - b, 0, h - 1)), int(np.clip(np.ceil(h - a), 1, h))

        def muestra(vista, c0, c1, y0, y1):
            img = planas[vista]
            w = img.shape[1]
            x0, x1 = int(np.clip(c0 * w, 0, w - 1)), int(np.clip(np.ceil(c1 * w), 1, w))
            r0, r1 = fila(vista, y0, y1)
            return np.median(img[r0:max(r1, r0 + 1), x0:max(x1, x0 + 1)].reshape(-1, 3), axis=0)

        for u, z, y in celdas_vis:
            estilo = obra.estilo[nombres[obra.parte[u, z, y]]]
            v = cual[u, z, y]
            if estilo[0] == 0:
                rgb[u, z, y] = estilo[2]
            elif v == "frente" or v == "arriba-frente":  # de frente, +Z queda a la izquierda de la imagen
                rgb[u, z, y] = muestra("frente", (W - 1 - z) / W, (W - z) / W, y, y + 1) * luz
            elif v == "espalda":  # de espalda, +Z a la derecha
                rgb[u, z, y] = muestra("espalda", z / W, (z + 1) / W, y, y + 1) * luz
            else:  # el lado (y lo de arriba que va con el lado): el frente a la izquierda
                rgb[u, z, y] = muestra("lado", u / L, (u + 1) / L, y, y + 1) * luz
    else:
        for u, z, y in celdas_vis:
            rgb[u, z, y] = obra.estilo[nombres[obra.parte[u, z, y]]][2]

    # Piedra por piedra: hiladas trabadas (cada hilada corre media piedra) y un tono por piedra.
    piedras = {}
    for u, z, y in celdas_vis:
        nombre = nombres[obra.parte[u, z, y]]
        alto, largo = obra.estilo[nombre][:2]
        if alto == 0:  # sin piedras (el hueco de la puerta)
            continue
        v = cual[u, z, y]
        hilada = y // alto
        corre = (hilada % 2) * (largo // 2)
        if v in ("frente", "espalda"):
            clave = (nombre, v, u, hilada, (z + corre) // largo)
        elif v == "lado":
            clave = (nombre, v, z, hilada, (u + corre) // largo)
        else:  # el piso de arriba: losas cuadradas
            clave = (nombre, "arriba", y, (u + corre) // largo, z // largo)
        piedras.setdefault(clave, []).append((u, z, y))
    for clave, celdas in piedras.items():
        idx = tuple(np.array(celdas).T)
        rgb[idx] = np.median(rgb[idx], axis=0) * _tono(clave)
    return np.clip(rgb, 0, 255), vis


def a_vox(obra, rgb, vis, ncolores=32):
    """(grid (sx, sy, sz) del .vox, paleta (k, 3)): x = el ancho al reves (de frente, +Z a la izquierda),
    y = u (0, el frente) y z = el alto. Los colores fijos (las partes sin piedras, como el hueco de la puerta) van
    tal cual a la paleta: son pocas celdas y el corte por la mediana los juntaria con la piedra."""
    fijo = np.zeros(obra.parte.shape, bool)
    for k, nombre in enumerate(obra.nombres[1:], start=1):
        if obra.estilo[nombre][0] == 0:
            fijo |= obra.parte == k
    libres, fijos = vis & ~fijo, vis & fijo
    col = np.clip(np.rint(rgb), 0, 255).astype(np.uint8)
    extra = np.unique(col[fijos], axis=0) if fijos.any() else np.zeros((0, 3), np.uint8)
    idx, pal = voxeles.cuantizar(col[libres], max(1, ncolores - len(extra)))
    g = np.zeros(obra.parte.shape, np.uint8)
    g[libres] = idx
    for j, c in enumerate(extra):
        g[fijos & np.all(col == c, axis=-1)] = len(pal) + j + 1
    pal = np.concatenate([pal, extra]) if len(extra) else pal
    g[obra.lleno & ~vis] = 1  # lo de adentro no se ve
    return np.ascontiguousarray(np.transpose(g, (1, 0, 2))[::-1]), pal


def triangulos(grid):
    """Los que arma el juego (src/sim/voxel.c, on_ground; como cuenta tools/modelos/modelos.py check): dos por
    cara al aire, sin la de abajo de la base donde tiene vecinas a los cuatro lados (la tapa el suelo)."""
    lleno = grid > 0
    _, d = voxeles.expuestas(lleno)
    base = lleno[:, :, 0]
    p = np.pad(base, 1)
    tapada = base & p[:-2, 1:-1] & p[2:, 1:-1] & p[1:-1, :-2] & p[1:-1, 2:]
    return 2 * (int(sum(m.sum() for m in d)) - int(tapada.sum()))


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("receta", choices=sorted(RECETAS))
    ap.add_argument("-o", "--salida", required=True)
    ap.add_argument("--hoja", help="la hoja de tres vistas (frente | lado | espalda); sin ella, los colores base")
    ap.add_argument("--umbral", type=int, default=40, help="cuanto se aparta del fondo (suma RGB)")
    ap.add_argument("--colores", type=int, default=32, help="colores de la paleta (1 a 255)")
    ap.add_argument("--vista-previa", help="PNG con el frente, el lado, arriba y la isometrica")
    a = ap.parse_args(argv)
    obra = RECETAS[a.receta]()
    vistas = voxeles.hoja(a.hoja, a.umbral) if a.hoja else None
    rgb, vis = colorear(obra, vistas, ALTURAS.get(a.receta))
    grid, pal = a_vox(obra, rgb, vis, a.colores)
    n = vox.escribir(a.salida, grid, pal)
    ancho, alto, largo = obra.metros()
    voxeles.log(f"[estructuras] {a.salida}: {a.receta}, {grid.shape[0]}x{grid.shape[1]}x{grid.shape[2]} celdas "
                f"({ancho:g} x {alto:g} x {largo:g} m), {n} llenas, {len(pal)} colores, {triangulos(grid)} triangulos")
    if a.vista_previa:
        voxeles.vista_previa(grid, np.concatenate([pal, np.full((len(pal), 1), 255, np.uint8)], axis=1), a.vista_previa)
    return 0


if __name__ == "__main__":
    sys.exit(main())
