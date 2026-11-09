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


def surcos(mascara, rgb, ventana=41, umbral=8.0, minimo=400):  # noqa: C901
    """Lo tallado de una cara de la hoja: lo que queda mas oscuro que su alrededor. La mediana de 9 px borra el
    granulado del dibujo sin correr los bordes; el alrededor, el promedio de una caja de `ventana` px; despues se
    cierran los cortes de los trazos y se tiran las manchas de menos de `minimo` px. Los contornos no se rellenan:
    en las piedras-ciervo lo tallado es el surco, no el cuerpo del animal."""
    from scipy import ndimage

    lum = rgb.mean(axis=2)
    nucleo = ndimage.binary_erosion(mascara, iterations=10)  # el borde de la piedra no es un surco
    lisa = ndimage.median_filter(np.where(mascara, lum, lum[mascara].mean()), size=9)
    s = ((ndimage.uniform_filter(lisa, ventana) - lisa) > umbral) & nucleo
    s = ndimage.binary_closing(s, iterations=2) & nucleo
    lab, n = ndimage.label(s)
    if n == 0:
        return s
    area = ndimage.sum(np.ones_like(lab), lab, range(1, n + 1))
    return np.isin(lab, np.flatnonzero(area >= minimo) + 1)


def _a_celdas(m, ancho, alto, cuanto=0.5):
    """Una mascara de la hoja (filas de arriba abajo) a celdas (ancho x alto, y = 0 abajo): la celda va si lo que
    cubre tiene al menos `cuanto` de la mascara."""
    from PIL import Image

    f = np.asarray(Image.fromarray(m.astype(np.float32), "F").resize((ancho, alto), Image.BOX))
    return (f >= cuanto)[::-1].T  # (ancho, alto)


def _trazo_seguido(m):
    """Un trazo de celdas en diagonal queda punteado (dos celdas que solo se tocan por la esquina no hacen un surco):
    se agrega la celda de al lado que las une."""
    m = m.copy()
    a, b = m.shape
    for z in range(a - 1):
        for y in range(b):
            for dy in (1, -1):
                y2 = y + dy
                if 0 <= y2 < b and m[z, y] and m[z + 1, y2] and not m[z + 1, y] and not m[z, y2]:
                    m[z + 1, y] = True
    return m


def losa_tallada(vistas, alto_m, celda, piedra=(120, 108, 96), grano=4):
    """Una estela de piedra desde su hoja: la silueta del frente y la del lado (corte recto: una losa), los surcos del
    frente tallados una celda (el dibujo) y las grietas de la espalda, otra. Sin hoja, una losa lisa de 0.6 x 0.4."""
    H = int(round(alto_m / celda))
    if not vistas:
        W, D = int(round(0.6 / celda)), int(round(0.4 / celda))
        o = Obra(D, W, H, celda=celda)
        o.estilo.update({"piedra": (grano, grano, piedra, "lado"), "surco": (1, 1, piedra, "frente"), "grieta": (1, 1, piedra, "espalda")})
        o.caja("piedra", (0, D - 1), (0, W - 1), (0, H - 1))
        return o
    (fm, fc), (sm, sc), (bm, bc) = vistas
    W = int(round(fm.shape[1] / fm.shape[0] * H))
    D = int(round(sm.shape[1] / sm.shape[0] * H))
    frente = _a_celdas(fm, W, H)[::-1]  # de frente, +Z queda a la izquierda de la imagen: z = 0 es la derecha
    lado = _a_celdas(sm, D, H)  # el frente a la izquierda: u = 0
    o = Obra(D, W, H, celda=celda)
    o.estilo.update({
        "piedra": (grano, grano, piedra, "lado"),  # manchas de la piedra de `grano` celdas (menos, y no se juntan)
        "surco": (1, 1, tuple(int(v * 0.6) for v in piedra), "frente"),
        "grieta": (1, 1, tuple(int(v * 0.65) for v in piedra), "espalda"),
    })
    lleno = frente[None, :, :] & lado[:, None, :]
    o.parte[lleno] = o.id("piedra")
    # Los surcos del frente: la celda de adelante de cada columna, afuera; la de atras queda como el fondo del surco.
    tallado = _trazo_seguido(_a_celdas(surcos(fm, fc), W, H, 0.25)[::-1])
    grietas = _trazo_seguido(_a_celdas(surcos(bm, bc, umbral=10.0, minimo=600), W, H, 0.25))  # de espalda, +Z a la derecha
    for z, y in zip(*np.nonzero(tallado)):
        us = np.flatnonzero(o.parte[:, z, y])
        if len(us) > 2:
            o.parte[us[0], z, y] = 0
            o.parte[us[1], z, y] = o.id("surco")
    for z, y in zip(*np.nonzero(grietas)):
        us = np.flatnonzero(o.parte[:, z, y])
        if len(us) > 2 and o.parte[us[-1], z, y] == o.id("piedra"):
            o.parte[us[-1], z, y] = 0
            o.parte[us[-2], z, y] = o.id("grieta")
    return o


def piedra_ciervo(vistas=None):
    """mapa.hito.piedra_ciervo: la estela con tres ciervos de la hoja, 3 m de alto (el inventario) en celdas de 4 cm.
    Una losa de corte recto (antes salia de seccion eliptica: una lente vista de arriba), con los tres ciervos y
    sus astas tallados como surcos de una celda en el frente, y las grietas de la espalda. El ancho y el grueso, los
    de la hoja."""
    return losa_tallada(vistas, 3.0, 0.04)


def piedra_ciervo_sol(vistas=None):
    """arte/voxeles/estructuras/piedra_ciervo_sol: otra estela (sin id todavia), 3 m en celdas de 4 cm: dos ciervos
    encabritados frente a frente, con sus astas, un sol arriba y otro abajo, y la guarda en zigzag."""
    return losa_tallada(vistas, 3.0, 0.04)


def piedra_ciervo_felinos(vistas=None):
    """arte/voxeles/estructuras/piedra_ciervo_felinos: otra estela (sin id todavia), 3 m en celdas de 4 cm: dos
    felinos frente a frente, el sol, las espirales, el rombo del medio y la guarda en zigzag."""
    return losa_tallada(vistas, 3.0, 0.04)


def _linea(o, parte, a, b, plano):
    """Una viga en diagonal de la celda a a la b (dos coordenadas del plano; la tercera fija en `plano`, que dice
    cual es: ("z", valor) o ("u", valor)), como una escalera de celdas que se tocan por el costado."""
    (p0, y0), (p1, y1) = a, b
    n = max(abs(p1 - p0), abs(y1 - y0))
    ult = None
    for k in range(n + 1):
        p, y = int(round(p0 + (p1 - p0) * k / n)), int(round(y0 + (y1 - y0) * k / n))
        celdas = [(p, y)] if ult is None or (ult[0] == p or ult[1] == y) else [(p, ult[1]), (p, y)]
        for (pp, yy) in celdas:
            if plano[0] == "z":
                o.caja(parte, (pp, pp), (plano[1], plano[1]), (yy, yy))
            else:
                o.caja(parte, (plano[1], plano[1]), (pp, pp), (yy, yy))
        ult = (p, y)


def tonos_de_madera(vistas):
    """Los colores de una armazon de madera de la hoja (las tres vistas): la madera (la mediana), las tablas (lo mas
    claro de la madera: el percentil 70 del brillo) y la soga de las ataduras (lo claro y amarillento), aclarados
    como lo demas (LUZ). Colores fijos: en piezas finas, el color de la hoja por tramos no deja juntar las caras."""
    px = np.concatenate([c[m] for m, c in vistas])
    brillo = px.sum(axis=1)
    madera = np.median(px, axis=0)
    tabla = np.median(px[(brillo >= np.percentile(brillo, 60)) & (brillo <= np.percentile(brillo, 80))], axis=0)
    claro = px[(brillo >= np.percentile(brillo, 92)) & (px[:, 0] > px[:, 2] + 15)]
    soga = np.median(claro, axis=0) if len(claro) > 20 else np.array([176, 160, 120], float)
    aclara = lambda c: tuple(int(v) for v in np.clip(np.asarray(c) * LUZ, 0, 255))  # noqa: E731
    return aclara(madera), aclara(tabla), aclara(soga)


def atalaya(vistas=None, celda=0.15):
    """estructura.campamento.atalaya: la atalaya de madera de la hoja, 6 m de alto (el inventario), en celdas de
    15 cm (los postes de la hoja miden 0.3 m: 2 x 2 celdas; las vigas, 0.15: una). La torre de 2.5 m de lado: cuatro
    postes, tres pisos (1.9, 3.7 y 5.4 m) con su viga de borde, el piso de tablas con el hueco de la escalera, la
    baranda arriba y una baranda baja en cada piso; las cruces de refuerzo de la hoja (de costado, una diagonal por
    piso; de frente y de espalda, en lambda abajo y en V arriba); la escalera de mano al medio del frente; y atras,
    la escalera larga que baja de la plataforma hasta el suelo a 3.3 m de la torre, sobre las vigas de los pisos que
    se alargan y sus postes. Las ataduras de soga, en cada encuentro de poste y viga. Los colores, de la hoja."""
    c = lambda m: int(round(m / celda))  # noqa: E731 - de metros a celdas
    H, W = c(6.0), c(2.5) | 1  # impar: la escalera de mano, al medio
    torre = c(2.5)
    u0, u1 = 1, torre  # la torre: la escalera de mano va delante, en u 0
    pie = u1 + c(3.3)  # donde la escalera larga toca el suelo
    o = Obra(pie + 1, W, H, celda=celda)
    madera, tabla, soga = tonos_de_madera(vistas) if vistas else ((110, 92, 70), (128, 106, 80), (176, 160, 120))
    o.estilo.update({"madera": (0, 0, madera, "lado"), "tabla": (0, 0, tabla, "lado"), "soga": (0, 0, soga, "lado")})
    medio = W // 2
    pisos = [(c(h) - 1, c(h)) for h in (1.9, 3.7)] + [(c(5.4) - 2, c(5.4))]
    postes = [(u, z) for u in (u0, u1 - 1) for z in (0, W - 2)]
    for (u, z) in postes:  # los postes, hasta arriba de la baranda
        o.caja("madera", (u, u + 1), (z, z + 1), (0, H - 1))
    for (a, b) in pisos:
        o.caja("madera", (u0, u1), (0, 0), (a, b), espejo=True)  # las vigas de borde
        o.caja("madera", (u0, u0), (0, W - 1), (a, b))
        o.caja("madera", (u1, u1), (0, W - 1), (a, b))
        o.caja("tabla", (u0 + 1, u1 - 1), (1, W - 2), (b, b))  # el piso de tablas, con el hueco de la escalera
        o.vaciar((u0 + 1, u0 + 2), (medio - 1, medio + 1), (b, b))
        for (u, z) in postes:  # la soga, arriba y abajo de cada viga
            for y in (a - 1, b + 1):
                o.caja("soga", (u, u + 1), (z, z + 1), (y, y))
    for h in (2.35, 4.15, 5.8):  # las barandas: baja en cada piso, la de arriba a la altura del pecho
        y = c(h)
        o.caja("madera", (u0, u1), (0, 0), (y, y), espejo=True)
        o.caja("madera", (u0, u0), (0, W - 1), (y, y))
        o.caja("madera", (u1, u1), (0, W - 1), (y, y))
    tramos = [(1, pisos[0][0] - 1), (pisos[0][1] + 1, pisos[1][0] - 1), (pisos[1][1] + 1, pisos[2][0] - 1)]
    for z in (0, W - 1):  # las cruces de los costados: una diagonal por piso, que sube hacia atras
        for (y0, y1) in tramos:
            _linea(o, "madera", (u0 + 2, y0), (u1 - 2, y1), ("z", z))
    for u in (u0, u1):  # al frente y atras: en lambda abajo, en V arriba (se juntan al medio)
        y0, y1 = tramos[0]
        _linea(o, "madera", (2, y0), (medio, y1), ("u", u))
        _linea(o, "madera", (W - 3, y0), (medio, y1), ("u", u))
        for (y0, y1) in tramos[1:]:
            _linea(o, "madera", (2, y1), (medio, y0 + 1), ("u", u))
            _linea(o, "madera", (W - 3, y1), (medio, y0 + 1), ("u", u))
    # La escalera de mano, delante del frente: dos largueros y un peldaño cada 0.3 m.
    arriba = pisos[2][1]
    o.caja("madera", (0, 0), (medio - 2, medio - 2), (0, arriba), espejo=True)
    for y in range(c(0.3), arriba, c(0.3)):
        o.caja("tabla", (0, 0), (medio - 1, medio + 1), (y, y))
    # Atras: las vigas de los pisos que se alargan, sus postes, la diagonal y la escalera larga.
    (a1, b1), (a2, b2) = pisos[0], pisos[1]
    p2, p1 = u1 + c(1.6), u1 + c(2.6)  # los postes de atras (sobre la viga del primer piso y desde el suelo)
    o.caja("madera", (u1, p1 + 1), (0, 0), (a1, b1), espejo=True)
    o.caja("madera", (u1, p2 + 1), (0, 0), (a2, b2), espejo=True)
    o.caja("madera", (p2, p2 + 1), (0, 0), (b1 + 1, b2 + c(0.5)), espejo=True)
    o.caja("madera", (p1, p1 + 1), (0, 0), (0, a1 - 1), espejo=True)
    for z in (0, W - 1):
        _linea(o, "madera", (u1 + 1, a1 - 1), (p1 - 1, 1), ("z", z))
    for u in range(u1 + 1, pie + 1):
        y = int(round(arriba - arriba * (u - u1 - 1) / (pie - u1 - 1)))
        y_sig = int(round(arriba - arriba * (u - u1) / (pie - u1 - 1))) if u < pie else 0
        o.caja("madera", (u, u), (medio - 2, medio - 2), (max(y_sig, 0), y), espejo=True)  # los largueros
        o.caja("tabla", (u, u), (medio - 1, medio + 1), (y, y))  # el escalon
    return o


# ------------------------------------------------------------------------------------------ el muro en ruinas
BASE_MURO = 9  # la base: dos hiladas de piedra de 4 celdas con su junta (la fila 4); la fila 9, la junta de asiento


def _ladrillo_px(m, c, umbral=38.0):
    """Los pixeles de ladrillo de una vista: lo rojizo (R - G de la mediana de 7 px, que borra el granulado) sobre el
    barro y la piedra de la base (en la hoja del muro, el ladrillo da 45 a 55 y el barro y la piedra, 20 a 30)."""
    from scipy import ndimage

    return (ndimage.median_filter(c[..., 0] - c[..., 1], size=7) > umbral) & m


def _sin_ladrillos(m, c, lad):
    """El color de la vista con barro donde hay ladrillos (y donde hay sombra): cada pixel toma el del barro mas
    cercano (el color del nucleo que queda a la vista donde se cayeron)."""
    from scipy import ndimage

    barro = m & ~ndimage.binary_dilation(lad, iterations=3)
    if not barro.any():
        return c
    lum = c.mean(axis=2)
    barro &= lum >= np.percentile(lum[barro], 15)  # sin las sombras ni las grietas (quedarian como agujeros)
    _, (iy, ix) = ndimage.distance_transform_edt(~barro, return_indices=True)
    return c[iy, ix]


def _celdas_de(img, n, filas):
    """img (h, w[, 3]) a celdas: el promedio de cada rectangulo, con n columnas parejas y las filas dadas (sus bordes,
    en pixeles desde abajo). Devuelve (n, len(filas) - 1[, 3]), con y = 0 abajo."""
    img = np.asarray(img, np.float32)
    h, w = img.shape[:2]
    cols = np.linspace(0, w, n + 1)
    out = np.zeros((n, len(filas) - 1) + img.shape[2:], np.float32)
    for y in range(len(filas) - 1):
        r0 = int(np.clip(round(h - filas[y + 1]), 0, h - 1))
        r1 = int(np.clip(round(h - filas[y]), r0 + 1, h))
        for a in range(n):
            c0 = int(np.clip(round(cols[a]), 0, w - 1))
            c1 = int(np.clip(round(cols[a + 1]), c0 + 1, w))
            out[a, y] = img[r0:r1, c0:c1].reshape((-1,) + img.shape[2:]).mean(axis=0)
    return out


def _aparejo(a, y, desfase, traba, paso=6):
    """El ladrillo de la celda (a, y) de una cara del muro, o None si es junta. a: a lo largo de la cara; y: el alto.
    Hiladas de 4 celdas desde BASE_MURO (la junta abajo y 3 de ladrillo) y ladrillos de `paso` celdas (paso - 1 y la
    junta), trabados media pieza; desfase y traba corren el aparejo para que calce con el de la hoja."""
    h, r = divmod(y - BASE_MURO, 4)
    if r == 0:
        return None
    j, p = divmod(a + desfase + ((h + traba) % 2) * (paso // 2), paso)
    return None if p == paso - 1 else (h, j)


class _Cara:
    """Una cara del muro vista en la hoja: su silueta, su ladrillo y su color en celdas (a, y), y el aparejo que mejor
    calza con los ladrillos de la hoja, con los que estan (la mitad de sus celdas, al menos, son de ladrillo)."""

    def __init__(self, m, c, n, H, voltear, juntas_base=True):
        lad = _ladrillo_px(m, c)
        h = m.shape[0]
        fr = lad.sum(axis=1) / np.maximum(m.sum(axis=1), 1)
        bajo = int(np.flatnonzero(fr > 0.05).max()) if (fr > 0.05).any() else int(0.84 * h)
        base_px = h - bajo - 4  # el ultimo ladrillo y la junta de asiento: ahi empieza la base
        filas = np.interp(np.arange(H + 1), [0, BASE_MURO, H], [0, base_px, h])
        orden = (lambda x: x[::-1]) if voltear else (lambda x: x)  # de frente, +Z queda a la izquierda de la imagen
        self.sil = orden(_celdas_de(m, n, filas)) >= 0.5
        self.frac = orden(_celdas_de(lad, n, filas))
        self.barro = orden(_celdas_de(_sin_ladrillos(m, c, lad), n, filas))
        con = _celdas_de(c * lad[..., None], n, filas)
        self.ladrillo = orden(con / np.maximum(_celdas_de(lad, n, filas), 1e-4)[..., None])
        self.juntas_base = juntas_base
        self._calzar(n, H)

    def _calzar(self, n, H):
        mejor = None
        for traba in (0, 1):
            for desfase in range(6):
                suma, cuenta, juntas = {}, {}, 0.0
                for a in range(n):
                    for y in range(BASE_MURO, H):
                        k = _aparejo(a, y, desfase, traba)
                        if k is None:
                            juntas += 1.0 - self.frac[a, y]
                            continue
                        suma[k] = suma.get(k, 0.0) + float(self.frac[a, y])
                        cuenta[k] = cuenta.get(k, 0) + 1
                presentes = {k for k in suma if suma[k] >= 0.5 * cuenta[k]}
                acuerdo = juntas + sum(suma[k] if k in presentes else cuenta[k] - suma[k] for k in suma)
                if mejor is None or acuerdo > mejor[0]:
                    mejor = (acuerdo, desfase, traba, presentes)
        _, self.desfase, self.traba, self.presentes = mejor

    def material(self, a, y):
        """"piedra", "junta", ("ladrillo", clave) o "barro" de la celda (a, y) de la cara. Las juntas del aparejo van
        solo al lado de un ladrillo que esta; donde se cayeron todos, el barro es parejo."""
        if y < BASE_MURO:  # piedras de 22 celdas (1.2 m, como en la hoja: tres por hilada); de costado, una sola
            return "junta" if y == 4 or (self.juntas_base and (a + 3) % 23 == 22) else "piedra"
        k = _aparejo(a, y, self.desfase, self.traba)
        if k is not None:
            return ("ladrillo", k) if k in self.presentes else "barro"
        for da in (-1, 0, 1):  # los ocho vecinos: la junta de asiento bajo la junta de arriba, tambien
            for dy in (-1, 0, 1):
                if y + dy >= BASE_MURO and _aparejo(a + da, y + dy, self.desfase, self.traba) in self.presentes:
                    return "junta"
        return "barro"


def muro_en_ruinas(vistas=None, alto_m=3.0, hiladas=12, tonos=(4, 3, 8), mancha=3, tallado="hiladas"):  # noqa: C901
    """estructura.ruina.muro: el muro de ladrillo de la hoja, 3 m de alto (el inventario) en celdas de 5.3 cm (la
    base de dos hiladas de piedra de 0.47 m y 12 hiladas de ladrillo, como en la hoja). La forma, de las tres
    siluetas (frente, lado y espalda): lo alto a un lado, la rotura en escalones y el grueso que va de 1.1 m en la
    base a 0.4 m arriba. En el frente y la espalda, el aparejo trabado de la hoja (ladrillos de 5 x 3 celdas con su
    junta), con los ladrillos que la hoja muestra; el barro del nucleo, donde se cayeron, queda una celda hundido.
    tallado: que juntas se hunden tambien ("juntas": todas; "hiladas": las de cada hilada y las de la base, y las de
    entre ladrillos van de color; "barro": ninguna). Con todas pasa el aviso de triangulos de Android (8498). Los
    costados llevan el aparejo de la vista de lado, sin hundir (los escalones de la rotura son chicos). Los colores,
    de la hoja: cada ladrillo, su promedio; el barro y la piedra de la base, a manchas de `mancha` celdas; arriba
    (sin vista), el barro comun con un tono por mancha. tonos = (barro, piedra, ladrillos). Sin hoja, un muro
    entero."""
    H = BASE_MURO + 4 * hiladas
    celda = alto_m / H
    if vistas:
        (fm, fc), (sm, sc), (bm, bc) = vistas
        W = int(round(fm.shape[1] / fm.shape[0] * H))
        D = int(round(sm.shape[1] / sm.shape[0] * H))
        caras = {"frente": _Cara(fm, fc, W, H, True), "lado": _Cara(sm, sc, D, H, False, juntas_base=False),
                 "espalda": _Cara(bm, bc, W, H, False)}
        # La forma: el perfil de arriba del frente (cada columna, llena desde el suelo: el pie roto de la hoja no
        # deja el muro en el aire) y el grueso del lado, desde el frente (una cara plana: lo que sobresale son los
        # ladrillos) hasta atras, en escalones que solo se angostan hacia arriba. La espalda es el mismo dibujo
        # visto de atras, con diferencias de una celda: no recorta la forma, solo pone sus ladrillos.
        fr = caras["frente"].sil
        alto = np.where(fr.any(axis=1), H - np.argmax(fr[:, ::-1], axis=1), 0)
        junta_arriba = (alto - 1 >= BASE_MURO) & ((alto - 1 - BASE_MURO) % 4 == 0)
        alto[junta_arriba] -= 1  # la junta de asiento de una hilada que se cayo, se cae con ella
        lado = caras["lado"].sil
        fondo = np.where(lado.any(axis=0), D - np.argmax(lado[::-1], axis=0), 0)
        fondo = np.maximum.accumulate(fondo[::-1])[::-1]
        ys, us = np.arange(H), np.arange(D)
        lleno = (ys[None, None, :] < alto[None, :, None]) & (us[:, None, None] < fondo[None, None, :])
    else:
        W, D = 65, 22
        m = np.ones((H * 8, W * 8), bool)
        c = np.ones(m.shape + (3,), np.float32) * np.array([150, 80, 50], np.float32)
        c[-BASE_MURO * 8:] = (130, 115, 95)
        caras = {"frente": _Cara(m, c, W, H, True), "espalda": _Cara(m, c, W, H, False),
                 "lado": _Cara(m[:, :D * 8], c[:, :D * 8], D, H, False, juntas_base=False)}
        lleno = np.ones((D, W, H), bool)

    def cara_de(expo, u, z, y):
        """(cara, a, b, plano) de la celda a la vista: la cara de la hoja que la mira, sus coordenadas en esa vista
        y el plano donde esta (para las manchas). Lo que solo se ve de arriba no tiene vista (va el barro comun)."""
        if expo["frente"][u, z, y]:
            return "frente", z, y, ("frente", u)
        if expo["espalda"][u, z, y]:
            return "espalda", z, y, ("espalda", u)
        if expo["lado"][u, z, y]:
            return "lado", u, y, ("lado", z)
        return "lado", u, y, ("arriba", y)

    # El frente y la espalda: lo que no es ladrillo ni piedra (las juntas y el barro) queda una celda mas adentro.
    expo = _caras(lleno)
    mat, quitar = {}, []
    for u, z, y in np.argwhere(expo["frente"] | expo["espalda"] | expo["lado"]).tolist():
        cara, a, _, _ = cara_de(expo, u, z, y)
        mat[(u, z, y)] = (cara, caras[cara].material(a, y))
    for (u, z, y), (cara, m_) in mat.items():
        hunde = m_ == "barro" or (m_ == "junta" and (tallado == "juntas" or (tallado == "hiladas" and (
            y < BASE_MURO or (y - BASE_MURO) % 4 == 0))))
        if cara != "lado" and hunde:
            ad = (u + 1, z, y) if cara == "frente" else (u - 1, z, y)
            if 0 <= ad[0] < D and lleno[ad] and ad not in mat:
                quitar.append(((u, z, y), ad, m_))
    for celda_, _, _ in quitar:
        lleno[celda_] = False
    juntas = {ad for _, ad, m_ in quitar if m_ == "junta"}

    # Que va en cada celda a la vista: el ladrillo (su tono), la junta, la piedra de la base o el barro (a manchas).
    expo = _caras(lleno)
    vis = expo["frente"] | expo["espalda"] | expo["lado"] | expo["arriba"] | expo["abajo"]
    lad_col, lad_de, manchas, de_mancha, fijas = {}, {}, {"barro": {}, "piedra": {}}, {}, {}
    for u, z, y in np.argwhere(vis).tolist():
        cara, a, b, plano = cara_de(expo, u, z, y)
        m_ = mat.get((u, z, y), (cara, None))
        if isinstance(m_[1], tuple):  # un ladrillo (de la cara que lo puso)
            clave = (m_[0], m_[1][1])
            a_l = z if m_[0] != "lado" else u
            lad_col.setdefault(clave, []).append(caras[m_[0]].ladrillo[a_l, y])
            lad_de[(u, z, y)] = clave
            continue
        if (u, z, y) in juntas or m_[1] == "junta":
            fijas[(u, z, y)] = "junta"
            continue
        tipo = "piedra" if y < BASE_MURO else "barro"
        if plano[0] == "arriba":  # lo de arriba no tiene vista: el barro de siempre, con un tono por mancha
            clave = (tipo, plano, u // (mancha + 1), z // (mancha + 1))  # (mas grandes: es mucho y se ve poco)
            manchas[tipo].setdefault(clave, [])
        else:
            clave = (tipo, plano, a // mancha, b // mancha)
            manchas[tipo].setdefault(clave, []).append(caras[cara].barro[a, y])
        de_mancha[(u, z, y)] = (tipo, clave)
    for tipo, ms in manchas.items():
        hay = [np.asarray(v, np.float32) for v in ms.values() if v]
        comun = np.median(np.concatenate(hay), axis=0) if hay else np.array([140, 112, 84], np.float32)
        for clave, v in ms.items():
            if not v:
                v.append(comun * _tono(clave, 0.08))

    def a_tonos(cols, n):
        cols = np.clip(np.asarray(cols, np.float32).reshape(-1, 3) * LUZ, 0, 255).astype(np.uint8)
        return voxeles.cuantizar(cols, n)

    tupla = lambda c: tuple(int(v) for v in c)  # noqa: E731
    o = Obra(D, W, H, celda=celda)
    claves = sorted(lad_col)
    i_lad, p_lad = a_tonos([np.median(lad_col[k], axis=0) for k in claves], tonos[2])
    for k, c in enumerate(p_lad):
        o.estilo[f"ladrillo{k}"] = (0, 0, tupla(c), "frente")
    tono = {}
    for tipo, n in (("barro", tonos[0]), ("piedra", tonos[1])):
        ks = sorted(manchas[tipo], key=repr)
        if not ks:
            continue
        idx, pal = a_tonos([np.mean(manchas[tipo][k], axis=0) for k in ks], n)
        for k, c in enumerate(pal):
            o.estilo[f"{tipo}{k}"] = (0, 0, tupla(c), "lado")
        tono.update({k: f"{tipo}{int(i) - 1}" for k, i in zip(ks, idx)})
    todo_barro = np.concatenate([np.asarray(v, np.float32).reshape(-1, 3) for v in manchas["barro"].values()])
    o.estilo["junta"] = (0, 0, tupla(np.clip(np.median(todo_barro, axis=0) * 0.62 * LUZ, 0, 255)), "lado")

    o.parte[lleno] = o.id(min(tono.values()) if tono else "junta")  # lo de adentro (no se ve)
    tono_lad = dict(zip(claves, (int(i) - 1 for i in i_lad)))
    for celda_, clave in lad_de.items():
        o.parte[celda_] = o.id(f"ladrillo{tono_lad[clave]}")
    for celda_, (tipo, clave) in de_mancha.items():
        o.parte[celda_] = o.id(tono[clave])
    for celda_, nombre in fijas.items():
        o.parte[celda_] = o.id(nombre)
    return o

RECETAS = {"templo": templo, "forja": forja, "pino": pino, "piedra_ciervo": piedra_ciervo, "atalaya": atalaya,
           "piedra_ciervo_sol": piedra_ciervo_sol, "piedra_ciervo_felinos": piedra_ciervo_felinos,
           "muro_en_ruinas": muro_en_ruinas}

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
    "piedra_ciervo": {v: ([0, 75], [0, a], a) for v, a in (("frente", 830), ("lado", 836), ("espalda", 829))},
    # Las otras estelas, a una sola escala (las vistas recortadas, de punta a punta).
    "piedra_ciervo_sol": {v: ([0, 75], [0, 1.0], 1.0) for v in ("frente", "lado", "espalda")},
    "piedra_ciervo_felinos": {v: ([0, 75], [0, 1.0], 1.0) for v in ("frente", "lado", "espalda")},
    "atalaya": {v: ([0, 40], [0, 1.0], 1.0) for v in ("frente", "lado", "espalda")},
    "muro_en_ruinas": {v: ([0, 57], [0, 1.0], 1.0) for v in ("frente", "lado", "espalda")},  # (colores fijos)
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
