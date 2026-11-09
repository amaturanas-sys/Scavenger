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

CELDA = 0.5  # metros por celda (el templo), como los sitios grandes del kit (src/world/voxstruct.c)
HUECO = (40, 32, 26)  # lo oscuro de una puerta, sin luz
# La hoja ya trae su luz y el juego sombrea cada cara otra vez (src/sim/voxel.c: 0.82 el frente, 0.68 a 0.88 los
# lados): sin aclarar, todo queda mas apagado que en la hoja.
LUZ = 1.15


class Obra:
    """Una rejilla (u, z, y) con la parte de cada celda (0: vacia) y el estilo de cada parte:
    (alto de la hilada, largo de la piedra, color sin hoja, vista de las caras de arriba[, vista de todas sus caras]).
    La ultima, cuando la hoja dibuja otra cosa donde va la parte (una pared que en la hoja tiene un hueco)."""

    def __init__(self, largo, ancho, alto, celda=CELDA):
        self.parte = np.zeros((largo, ancho, alto), np.uint8)
        self.celda = celda  # metros por celda: solo para medir (el juego la escala al alto del inventario)
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
        """(ancho Z, alto Y, largo X) en metros, como las medidas del inventario: lo que ocupa de lado a lado y de
        adelante atras (los margenes vacios no van al .vox) y el alto entero (el suelo es la celda 0)."""
        us, zs = np.nonzero(self.lleno.any(axis=2))
        return (round((zs.max() - zs.min() + 1) * self.celda, 3), round(self.parte.shape[2] * self.celda, 3),
                round((us.max() - us.min() + 1) * self.celda, 3))


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


def forja():
    """estructura.campamento.forja: la forja portatil, en celdas de 6 cm con el alto del inventario (1.5 m) y las
    proporciones de la hoja. Un horno cerrado: el zocalo de piedra con la boca del cenicero; la camara de ladrillo,
    abierta solo al frente en arco, con la llamarada adentro (brasas, llamas y su centro); la campana de barro, que
    se angosta y retrocede hilada por hilada hasta la chimenea, con su ventana. Al costado (-Z, donde lo dibuja la
    hoja), un fuelle de acordeon sobre sus patas: los pliegues entre dos tablas, la de arriba inclinada hacia la
    boquilla, y la tobera de hierro que dobla y entra en la pared, junto a las brasas."""
    o = Obra(31, 22, 25, celda=0.06)
    o.estilo.update({
        "piedra": (2, 3, (112, 100, 86), "lado"),
        "ladrillo": (1, 2, (158, 80, 50), "frente"),
        "ladrillo_ciego": (1, 2, (158, 80, 50), "frente", "frente"),  # los costados, cerrados: la hoja tiene ahi un hueco
        "barro": (2, 3, (192, 122, 74), "lado"),
        "madera": (1, 5, (150, 105, 60), "lado"),
        "cuero": (1, 1, (110, 64, 42), "lado"),
        "pliegue": (0, 0, (62, 36, 26), "lado"),
        "hierro": (1, 6, (70, 70, 74), "lado"),
        "hollin": (0, 0, (34, 26, 22), "frente"),
        "brasa": (0, 0, (200, 64, 24), "frente"),
        "llama": (0, 0, (246, 136, 32), "frente"),
        "llama_viva": (0, 0, (255, 214, 96), "frente"),
    })
    # El zocalo y la camara (paredes de dos celdas: afuera ladrillo, adentro hollin), abierta solo al frente.
    o.caja("piedra", (8, 30), (4, 17), (0, 3))
    o.caja("ladrillo", (10, 29), (5, 16), (4, 12))
    o.vaciar((12, 27), (7, 14), (5, 11))
    o.marcar("hollin", (11, 28), (6, 15), (4, 12))
    o.marcar("ladrillo_ciego", (12, 28), (5, 5), (5, 12), espejo=True)
    o.vaciar((10, 11), (8, 13), (5, 11))
    o.vaciar((10, 11), (9, 12), (12, 12))
    # La llamarada: la cama de brasas, las llamas que se angostan hacia arriba y su centro, mas claro.
    o.caja("brasa", (13, 24), (8, 13), (5, 5))
    o.caja("llama", (14, 20), (9, 12), (6, 6))
    o.caja("llama", (15, 19), (9, 12), (7, 7))
    o.caja("llama", (16, 18), (10, 11), (8, 8))
    o.caja("llama_viva", (15, 19), (10, 11), (6, 7))
    # El cenicero, en el frente del zocalo, y la ranura de atras con su caño.
    o.vaciar((8, 9), (9, 12), (0, 2))
    o.marcar("hollin", (10, 10), (9, 12), (0, 2))
    o.vaciar((29, 29), (9, 9), (4, 11), espejo=True)
    o.marcar("hierro", (29, 29), (10, 11), (4, 11))
    # La campana: cada hilada empieza mas atras y es mas angosta, hasta el ancho de la chimenea.
    for y, (u0, z0) in zip(range(13, 21), ((11, 5), (12, 5), (12, 6), (14, 6), (16, 7), (18, 7), (21, 8), (23, 8))):
        o.caja("barro", (u0, 29), (z0, 21 - z0), (y, y))
    o.vaciar((12, 12), (9, 12), (14, 15))  # la ventana de la campana
    o.marcar("hollin", (13, 13), (9, 12), (14, 15))
    o.caja("barro", (24, 28), (8, 13), (21, 24))  # la chimenea, con su boca oscura arriba
    o.vaciar((25, 27), (9, 12), (24, 24))
    o.marcar("hollin", (25, 27), (9, 12), (23, 23))
    # El fuelle de acordeon: cuatro patas, la tabla de abajo, los pliegues (cresta y hondura, una celda cada uno,
    # mas bajos hacia la boquilla), la tabla de arriba con su manija y la boquilla.
    for u in (2, 8):
        for z in (0, 4):
            o.caja("madera", (u, u), (z, z), (0, 3))
    o.caja("madera", (1, 9), (0, 4), (4, 4))
    for u in range(1, 9):
        alto = 10 - (u - 1) // 2
        if u % 2:
            o.caja("cuero", (u, u), (0, 4), (5, alto))
        else:
            o.caja("pliegue", (u, u), (1, 3), (5, alto))
        o.caja("madera", (u, u), (0, 4), (alto + 1, alto + 1))
    o.caja("madera", (0, 0), (1, 3), (11, 11))  # la manija
    o.caja("madera", (9, 9), (1, 3), (5, 6))  # la boquilla
    # La tobera: de la boquilla hacia atras y, doblando, a traves de la pared hasta las brasas.
    o.caja("hierro", (10, 13), (2, 2), (6, 6))
    o.caja("hierro", (13, 13), (2, 6), (6, 6))
    return o


def tonos_de_arbol(vistas):
    """Los colores de un arbol de la hoja (las tres vistas): el tronco (lo marron del pie, al medio) y tres verdes de
    la copa (sombra, medio y puntas: los percentiles 20, 55 y 85 del brillo de lo verde), aclarados como lo demas
    (LUZ)."""
    verdes, troncos = [], []
    for m, c in vistas:
        h, w = m.shape
        sel = m & (c[..., 1] > c[..., 0] + 4) & (c[..., 1] > c[..., 2])
        verdes.append(c[sel])
        pie = np.zeros_like(m)
        pie[int(0.9 * h):, int(0.4 * w):int(0.6 * w)] = True
        troncos.append(c[pie & m & ~sel])
    verdes, troncos = np.concatenate(verdes), np.concatenate(troncos)
    brillo = verdes.sum(axis=1)
    tonos = [np.median(verdes[(brillo >= np.percentile(brillo, q - 10)) & (brillo <= np.percentile(brillo, q + 10))], axis=0)
             for q in (20, 55, 85)]
    tronco = np.median(troncos, axis=0) if len(troncos) else np.array([88, 70, 54], float)
    aclara = lambda c: tuple(int(v) for v in np.clip(np.asarray(c) * LUZ, 0, 255))  # noqa: E731
    return aclara(tronco), [aclara(t) for t in tonos]


def pino(vistas=None):
    """mapa.vegetacion.pino: 10 m de alto (el inventario) y 5 m de copa (la de los pinos anchos de la hoja; el
    usuario: en el paisaje iran con alturas y tonos que varian, hasta 12 m), simetrico: cada celda se llena segun su
    distancia al eje,
    asi el arbol es el mismo de los cuatro lados y en el espejo. La hoja trae tres pinos distintos (dos anchos y uno
    angosto), no tres vistas de uno: de ahi salia antes una copa achatada, mas ancha de frente que de costado. El
    tronco de 2 x 2 celdas; la copa en ocho pisos de ramas, cada uno una falda que se angosta hacia arriba, con ocho
    lobulos que se corren media rama de un piso al otro. El color, de la hoja: el tronco y tres verdes (las puntas
    de abajo de cada piso, claras; lo de adentro, a la sombra)."""
    H, W = 52, 26  # 10 m de alto: celdas de 0.192 m; 26 de ancho = 5 m
    o = Obra(W, W, H, celda=10.0 / H)
    tronco, (sombra, medio, puntas) = tonos_de_arbol(vistas) if vistas else ((88, 70, 54), [(40, 58, 36), (58, 82, 46), (86, 112, 64)])
    o.estilo.update({
        "tronco": (0, 0, tronco, "lado"),
        "copa_sombra": (0, 0, sombra, "lado"),
        "copa": (0, 0, medio, "lado"),
        "copa_puntas": (0, 0, puntas, "lado"),
    })
    c = W / 2.0
    m = W // 2 - 1  # el tronco: las 2 x 2 celdas del medio
    # La copa empieza a 2.3 m: el tronco se ve aun con la camara de siempre (a unos 30 grados, una falda de 2.5 m
    # de radio tapa lo que esta a menos de 1.4 m bajo ella).
    base, cima, pisos = 12, H - 1, 8
    alto_piso = (cima - base + 1) / pisos
    rmax = c
    for y in range(base, H):
        k = min(int((y - base) / alto_piso), pisos - 1)
        y0 = base + k * alto_piso
        t = (y - y0) / alto_piso  # 0 abajo del piso, 1 arriba
        r_piso = rmax * (cima + 1 - y0) / (cima + 1 - base)
        for u in range(W):
            for z in range(W):
                du, dz = u + 0.5 - c, z + 0.5 - c
                d, th = np.hypot(du, dz), np.arctan2(dz, du)
                r = r_piso * (1.0 - 0.38 * t) * (1.0 + 0.09 * np.cos(8 * th + (k % 2) * np.pi))  # media rama: fase pi
                if d > r:  # la punta termina donde la copa ya no llega a las celdas del medio
                    continue
                if t < 0.25 and d > r - 1.6:
                    parte = "copa_puntas"
                elif d < r - 2.4:
                    parte = "copa_sombra"
                else:
                    parte = "copa"
                o.caja(parte, (u, u), (z, z), (y, y))
    o.caja("tronco", (m, m + 1), (m, m + 1), (0, base + 1))  # lo que se ve, debajo de la copa (adentro va tapado)
    return o


RECETAS = {"templo": templo, "forja": forja, "pino": pino}

# Donde cae cada altura en cada vista de la hoja: (celdas, pixeles desde el suelo de esa vista) y el alto total en
# pixeles de la vista recortada. La hoja del templo exagera lo alto en el frente (los pilonos miden 340 px de
# frente y 300 de lado y de espalda): por eso un tramo por terraza y no una sola escala.
ALTURAS = {
    "templo": {
        "frente": ([0, 6, 12, 18, 23, 28], [0, 67, 135, 195, 285, 340], 346),
        "lado": ([0, 6, 12, 18, 21, 23, 25, 28], [0, 66.5, 132, 191, 222, 248, 270, 299], 299),
        "espalda": ([0, 6, 12, 18, 21, 23, 25, 28], [0, 69, 138, 198, 228, 255, 268, 304], 305),
    },
    "forja": {v: ([0, 25], [0, 511], 511) for v in ("frente", "lado", "espalda")},  # las tres vistas, a la misma escala
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
            v = estilo[4] if len(estilo) > 4 else cual[u, z, y]  # una parte puede tomar el color de otra vista
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
    g = np.transpose(g, (1, 0, 2))[::-1]
    # Sin margenes vacios a los costados ni adelante y atras: el juego pone el origen en el centro de la base.
    xs, ys = np.nonzero((g > 0).any(axis=2))
    return np.ascontiguousarray(g[xs.min():xs.max() + 1, ys.min():ys.max() + 1]), pal


def triangulos(grid):
    """Los que arma el juego del .vox (src/sim/voxel.c: las caras planas juntas y la base en el suelo), contados
    como en tools/modelos/modelos.py check."""
    sys.path.insert(0, os.path.join(HERE, "..", "modelos"))
    import modelos

    xs, ys, zs = np.nonzero(grid)
    cells = {(int(x), int(y), int(z)): int(grid[x, y, z]) for x, y, z in zip(xs, ys, zs)}
    return modelos.tris_vox(cells, grid.shape)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("receta", choices=sorted(RECETAS))
    ap.add_argument("-o", "--salida", required=True)
    ap.add_argument("--hoja", help="la hoja de tres vistas (frente | lado | espalda); sin ella, los colores base")
    ap.add_argument("--umbral", type=int, default=40, help="cuanto se aparta del fondo (suma RGB)")
    ap.add_argument("--colores", type=int, default=32, help="colores de la paleta (1 a 255)")
    ap.add_argument("--vista-previa", help="PNG con el frente, el lado, arriba y la isometrica")
    a = ap.parse_args(argv)
    vistas = voxeles.hoja(a.hoja, a.umbral) if a.hoja else None
    receta = RECETAS[a.receta]
    obra = receta(vistas) if receta.__code__.co_argcount else receta()  # las que sacan algo de la hoja (el pino)
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
