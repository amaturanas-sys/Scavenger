#!/usr/bin/env python3
"""Pruebas de tools/modelos/piezas.py con hojas sinteticas (tres vistas dibujadas con PIL), sin red: las formas,
el origen, la textura y los errores que salieron en la auditoria de los modelos (el agarre de una daga de hoja
ancha, el lado de +Z espejado, el umbo hacia atras, la cruz bajo una cornamenta, la escala de volumen). Corre en
el CI con las dependencias de requirements.txt: python3 tools/modelos/test_piezas.py
"""
import io
import json
import os
import shutil
import struct
import sys
import tempfile
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "assets"))
sys.path.insert(0, os.path.join(HERE, "..", "voxeles"))

import numpy as np  # noqa: E402
from PIL import Image, ImageDraw  # noqa: E402

import piezas  # noqa: E402

GRIS, MARRON, ROJO, AZUL, BLANCO = (128, 128, 134), (110, 70, 40), (200, 30, 30), (30, 60, 200), (255, 255, 255)


def hoja(path, vistas, alto=200):
    """Una hoja de tres vistas: cada una, una funcion que dibuja en un ImageDraw de (ancho, alto); en fila, con
    fondo blanco y aire entre ellas."""
    anchos = [w for w, _ in vistas]
    W = sum(anchos) + 40 * (len(vistas) + 1)
    img = Image.new("RGB", (W, alto + 80), BLANCO)
    x = 40
    for w, dibujar in vistas:
        panel = Image.new("RGB", (w, alto), BLANCO)
        dibujar(ImageDraw.Draw(panel), w, alto)
        img.paste(panel, (x, 40))
        x += w + 40
    img.save(path)
    return path


def leer(data):
    """(posiciones por vertice, uv por vertice, textura RGB, sampler) de un GLB de piezas (sin indices)."""
    n = struct.unpack("<I", data[12:16])[0]
    js = json.loads(data[20:20 + n])
    binoff = 20 + n + 8
    bv, acc = js["bufferViews"], js["accessors"]

    def read(i, width):
        v = bv[acc[i]["bufferView"]]
        a = np.frombuffer(data[binoff + v["byteOffset"]: binoff + v["byteOffset"] + v["byteLength"]], "<f4")
        return a.reshape(-1, width).astype(np.float64)

    prim = js["meshes"][0]["primitives"][0]["attributes"]
    pos, uv = read(prim["POSITION"], 3), read(prim["TEXCOORD_0"], 2)
    iv = bv[js["images"][0]["bufferView"]]
    tex = np.asarray(Image.open(io.BytesIO(data[binoff + iv["byteOffset"]: binoff + iv["byteOffset"] + iv["byteLength"]]))
                     .convert("RGB"))
    return pos, uv, tex, js["samplers"][0]


def color_en(tex, uv):
    h, w = tex.shape[:2]
    return tex[np.clip((uv[1] * h).astype(int), 0, h - 1), np.clip((uv[0] * w).astype(int), 0, w - 1)]


def daga(d, w, h, ancho_hoja=24):
    """Una daga de pie: pomo abajo, agarre angosto, guarda, hoja casi tan ancha como la guarda (la de la hoja)."""
    c = w // 2
    d.rectangle([c - 13, h - 20, c + 13, h - 1], fill=GRIS)  # pomo
    d.rectangle([c - 9, h - 48, c + 9, h - 21], fill=MARRON)  # agarre
    d.rectangle([c - 21, h - 60, c + 21, h - 49], fill=GRIS)  # guarda
    d.rectangle([c - ancho_hoja // 2, 0, c + ancho_hoja // 2, h - 61], fill=GRIS)  # hoja


def test_plana_y_agarre(tmp):
    p = hoja(os.path.join(tmp, "daga.png"), [(60, daga), (16, lambda d, w, h: d.rectangle([5, 0, 10, h - 1], fill=GRIS)),
                                              (60, daga)])
    data, info = piezas.construir("arma.corta.daga", p, "plana", agarre="auto", grosor=0.03)
    assert info["tris"] <= info["tris_max"], info
    pos, uv, tex, sampler = leer(data)
    assert max(tex.shape[:2]) <= piezas.TEX_MAX and sampler["magFilter"] == 9728 and sampler["minFilter"] == 9728
    alto = pos[:, 1].max() - pos[:, 1].min()
    assert abs(alto - info["inventario_m"][1]) < 0.01, alto
    # El origen, en el agarre (entre el pomo y la guarda: 20 a 48 px de 200 desde abajo), no en la hoja.
    base = -pos[:, 1].min() / alto
    assert 0.10 < base < 0.24, base
    assert abs(pos[:, 2].max() + pos[:, 2].min()) < 0.01  # centrada de lado a lado


def test_lado_sin_espejo(tmp):
    """El lado de la hoja se ve desde -Z con el frente a la izquierda; las caras de +Z toman el mismo punto (por
    simetria), no el espejado: el rojo de adelante queda adelante en los dos lados."""
    def frente(d, w, h):
        d.ellipse([0, 0, w - 1, h - 1], fill=GRIS)

    def lado(d, w, h):
        d.ellipse([0, 0, w - 1, h - 1], fill=AZUL)
        d.pieslice([0, 0, w - 1, h - 1], 90, 270, fill=ROJO)  # la mitad de la izquierda (adelante), roja

    p = hoja(os.path.join(tmp, "casco.png"), [(140, frente), (160, lado), (140, frente)])
    data, info = piezas.construir("armadura.casco.laminar_cuero", p, "secciones", agarre="base", segmentos=16)
    pos, uv, tex, _ = leer(data)
    tri, tuv = pos.reshape(-1, 3, 3), uv.reshape(-1, 3, 2)
    n = np.cross(tri[:, 1] - tri[:, 0], tri[:, 2] - tri[:, 0])
    n /= np.linalg.norm(n, axis=1, keepdims=True) + 1e-12
    medio = (pos[:, 1].max() + pos[:, 1].min()) / 2
    for signo in (1, -1):  # el lado de +Z y el de -Z
        caras = np.flatnonzero((signo * n[:, 2] > 0.7) & (np.abs(n[:, 0]) <= 0.5))
        rojos = azules = 0
        for k in caras:
            centro = tuv[k].mean(axis=0)
            for p3, u in zip(tri[k], tuv[k]):
                if abs(p3[1] - medio) > 0.08 or abs(p3[0]) < 0.02:
                    continue
                r, _, b_ = color_en(tex, u + 0.15 * (centro - u)).astype(int)  # un poco adentro de la cara
                if p3[0] > 0:
                    rojos += r > b_
                    assert r > b_, (signo, p3, (r, b_))  # adelante, rojo
                else:
                    azules += b_ > r
                    assert b_ > r, (signo, p3, (r, b_))  # atras, azul
        assert rojos and azules, (signo, rojos, azules)


def test_disco_umbo_adelante(tmp):
    def frente(d, w, h):
        d.ellipse([0, 0, w - 1, h - 1], fill=MARRON)
        d.ellipse([w // 2 - 20, h // 2 - 20, w // 2 + 20, h // 2 + 20], fill=GRIS)

    def lado(d, w, h):  # de costado: la tabla, y el umbo hacia la izquierda (adelante)
        d.rectangle([20, 0, 30, h - 1], fill=MARRON)
        d.ellipse([0, h // 2 - 20, 40, h // 2 + 20], fill=GRIS)
        d.rectangle([20, 0, 30, h - 1], fill=MARRON)

    p = hoja(os.path.join(tmp, "escudo.png"), [(200, frente), (34, lado), (200, frente)])
    data, info = piezas.construir("escudo.mano.umbo", p, "disco", agarre="escudo", segmentos=12)
    assert info["tris"] <= info["tris_max"], info
    pos, _, _, _ = leer(data)
    assert abs(pos[:, 0].min()) < 1e-6  # el origen, atras
    assert pos[:, 0].max() > 0.1  # el umbo, adelante (+X)
    ext = pos.max(axis=0) - pos.min(axis=0)
    assert abs(ext[1] - 0.8) < 0.02 and abs(ext[2] - 0.8) < 0.05, ext  # redondo, del alto del inventario


def test_cruz_bajo_cornamenta():
    """La cruz se mide en el cuerpo, sobre las patas de adelante; una cornamenta que pasa por encima no cuenta."""
    sm = np.zeros((100, 160), bool)
    sm[40:70, 20:140] = True  # el cuerpo
    sm[70:100, 30:40] = sm[70:100, 120:130] = True  # las patas (adelante y atras)
    sm[20:40, 10:25] = True  # el cuello y la cabeza, adelante
    sm[0:3, 15:45] = True  # una punta de la cornamenta, por encima de la cruz
    fila, col = piezas.cruz_px(sm)
    assert fila == 40 and 30 <= col < 40, (fila, col)


def test_agarre_hoja_ancha():
    """La guarda de la daga de la hoja (42 px) no llega a 1.8 veces la mediana (la hoja, 24 px): la guarda se mide
    contra lo angosto de abajo."""
    img = Image.new("RGB", (60, 200), BLANCO)
    daga(ImageDraw.Draw(img), 60, 200)
    fm = np.asarray(img).sum(axis=2) < 700
    frac = piezas.agarre_auto(fm, False)
    assert 0.10 < frac < 0.24, frac


def test_volumen_escala(tmp):
    """La escala de volumen sale del volumen y de las vistas, no de la malla (que pierde las puntas): el largo es el
    de la vista de lado y la textura de arriba cae dentro del lomo."""
    def frente(d, w, h):
        d.rectangle([10, 0, w - 11, h - 61], fill=MARRON)
        d.rectangle([10, h - 60, 25, h - 1], fill=MARRON)
        d.rectangle([w - 26, h - 60, w - 11, h - 1], fill=MARRON)

    def lado(d, w, h):
        d.rectangle([0, 40, w - 1, h - 61], fill=MARRON)
        d.rectangle([20, h - 60, 40, h - 1], fill=MARRON)
        d.rectangle([w - 41, h - 60, w - 21, h - 1], fill=MARRON)
        d.rectangle([0, 0, 30, 40], fill=MARRON)  # la cabeza, adelante
        d.line([(5, 0), (5, 0)], fill=MARRON)

    p = hoja(os.path.join(tmp, "animal.png"), [(80, frente), (300, lado), (80, frente)])
    data, info = piezas.construir("animal.salvaje.lobo", p, "volumen", agarre="base", res=48, umbral=20, ancla="alto")
    assert info["tris"] <= info["tris_max"], info
    pos, uv, tex, _ = leer(data)
    ext = pos.max(axis=0) - pos.min(axis=0)
    alto = info["inventario_m"][1]
    assert abs(ext[1] - alto) < 0.05 * alto, ext
    assert abs(ext[0] - alto * 300 / 200) < 0.05 * alto * 300 / 200, ext  # el largo, el del lado (no estirado)
    assert abs(pos[:, 1].min()) < 1e-6  # la base en el suelo


def main():
    tests = [test_plana_y_agarre, test_lado_sin_espejo, test_disco_umbo_adelante, test_cruz_bajo_cornamenta,
             test_agarre_hoja_ancha, test_volumen_escala]
    failed = 0
    tmp = tempfile.mkdtemp(prefix="piezas-")
    try:
        for t in tests:
            try:
                t(tmp) if t.__code__.co_argcount else t()
                print(f"ok   {t.__name__}")
            except Exception:
                failed += 1
                print(f"FAIL {t.__name__}")
                traceback.print_exc()
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f"{len(tests) - failed}/{len(tests)} pruebas de piezas")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
