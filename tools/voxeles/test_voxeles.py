#!/usr/bin/env python3
"""Pruebas de tools/voxeles, sin GPU ni red: vistas, splat y malla sinteticos, con lo que tiene que salir.
Corre en el CI (con tools/modelos/requirements.txt) y a mano: python3 tools/voxeles/test_voxeles.py
"""
import os
import struct
import sys
import tempfile
import traceback

import numpy as np
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import estructuras  # noqa: E402
import vox  # noqa: E402
import voxeles  # noqa: E402

RED, GREEN, BLUE = (200, 40, 40), (40, 170, 60), (40, 70, 200)
PX = 8  # pixeles por celda en las vistas sinteticas


def vistas_de_una_L(tmp, transparente=False):
    """Una L de frente (abajo 20 de ancho, arriba 10), 10 de fondo y 30 de alto: frente rojo, lado verde, arriba azul."""
    W, D, H = 20, 10, 30
    bg = (0, 0, 0, 0) if transparente else (255, 255, 255, 255)

    def lienzo(w, h):
        return Image.new("RGBA", ((w + 4) * PX, (h + 4) * PX), bg)

    front = lienzo(W, H)
    for x in range(W):
        for z in range(H):
            if z < H // 2 or x < W // 2:  # la L
                front.paste(RED + (255,), ((x + 2) * PX, (H - 1 - z + 2) * PX, (x + 3) * PX, (H - z + 2) * PX))
    side = lienzo(D, H)
    side.paste(GREEN + (255,), (2 * PX, 2 * PX, (D + 2) * PX, (H + 2) * PX))
    top = lienzo(W, D)
    top.paste(BLUE + (255,), (2 * PX, 2 * PX, (W + 2) * PX, (D + 2) * PX))
    paths = []
    for name, img in (("frente", front), ("lado", side), ("arriba", top)):
        p = os.path.join(tmp, name + ".png")
        (img if transparente else img.convert("RGB")).save(p)
        paths.append(p)
    return paths, (W, D, H)


def cerca(c, ref, tol=40):
    return all(abs(int(a) - int(b)) <= tol for a, b in zip(c, ref))


def test_vox_ida_y_vuelta():
    rng = np.random.default_rng(3)
    grid = rng.integers(0, 6, size=(5, 7, 9)).astype(np.uint8)
    pal = rng.integers(0, 256, size=(5, 3)).astype(np.uint8)
    with tempfile.TemporaryDirectory() as tmp:
        p = os.path.join(tmp, "a.vox")
        n = vox.escribir(p, grid, pal)
        g2, p2 = vox.leer(p)
    assert n == int((grid > 0).sum())
    assert g2.shape == grid.shape and np.array_equal(g2, grid)
    assert np.array_equal(p2[:5, :3], pal) and (p2[:, 3] == 255).all()


def test_vistas_L(transparente):
    with tempfile.TemporaryDirectory() as tmp:
        (f, l, a), (W, D, H) = vistas_de_una_L(tmp, transparente)
        filled, rgb = voxeles.desde_vistas(f, l, a, res=H)
        assert filled.shape == (W, D, H), filled.shape
        esperado = (W * (H // 2) + (W // 2) * (H - H // 2)) * D
        assert abs(int(filled.sum()) - esperado) <= esperado * 0.03, (int(filled.sum()), esperado)
        assert not filled[W - 1, 0, H - 1] and filled[0, 0, H - 1] and filled[W - 1, 0, 0]  # la L
        assert cerca(rgb[3, 0, 3], RED)  # el frente, rojo
        assert cerca(rgb[W - 1, D // 2, 3], GREEN)  # el lado derecho (sin ser frente), verde
        assert cerca(rgb[3, D // 2, H - 1], BLUE)  # arriba, azul
        out = os.path.join(tmp, "L.vox")
        grid, pal = voxeles.guardar(out, filled, rgb, 8)
        g2, p2 = vox.leer(out)
        assert (g2 > 0).sum() == filled.sum() and len(pal) <= 8
        prev = os.path.join(tmp, "L.png")
        voxeles.vista_previa(g2, p2, prev)
        im = Image.open(prev)
        assert im.width > im.height > 0


def test_vistas_voltear():
    """El lado con el frente a la derecha, con --voltear-lado, da lo mismo."""
    with tempfile.TemporaryDirectory() as tmp:
        (f, l, a), _ = vistas_de_una_L(tmp)
        img = Image.open(l)
        img.transpose(Image.FLIP_LEFT_RIGHT).save(l)
        f1, _ = voxeles.desde_vistas(f, l, a, res=30, voltear_lado=True)
        (f, l2, a), _ = vistas_de_una_L(tmp)
        f2, _ = voxeles.desde_vistas(f, l2, a, res=30)
        assert np.array_equal(f1, f2)


def hoja_de_dos_objetos(tmp):
    """Una hoja como las de Canva: dos filas (dos objetos), cada una frente | lado | espalda, con una raya
    gris de punta a punta entre las vistas. Arriba: un marco (una caja con un hueco grande en el medio, de
    frente); abajo: un cilindro de 16 de diametro y 24 de alto, rojo de frente y azul de espalda."""
    W, H = 3 * 40 * PX, 2 * 34 * PX
    img = Image.new("RGB", (W, H), (255, 255, 255))
    d = ImageDraw.Draw(img)
    for c in (40 * PX, 80 * PX):
        d.rectangle([c - 1, 0, c + 1, H - 1], fill=(200, 200, 200))  # las rayas entre vistas
    def caja(col, row, x0, z0, w, h, color):
        X0, Y0 = col * 40 * PX + 4 * PX + x0 * PX, row * 34 * PX + 4 * PX + (24 - z0 - h) * PX
        d.rectangle([X0, Y0, X0 + w * PX - 1, Y0 + h * PX - 1], fill=color)
    # Fila 1: el marco de 20 x 24 con un hueco de 12 x 14; de lado, 6 de fondo.
    for col in (0, 2):
        caja(col, 0, 0, 0, 20, 24, GREEN)
        caja(col, 0, 4, 5, 12, 14, (255, 255, 255))
    caja(1, 0, 0, 0, 6, 24, GREEN)
    # Fila 2: el cilindro (de frente y de lado, un rectangulo de 16 x 24).
    caja(0, 1, 0, 0, 16, 24, RED)
    caja(1, 1, 0, 0, 16, 24, GREEN)
    caja(2, 1, 0, 0, 16, 24, BLUE)
    p = os.path.join(tmp, "hoja.png")
    img.save(p)
    return p


def test_hoja():
    with tempfile.TemporaryDirectory() as tmp:
        p = hoja_de_dos_objetos(tmp)
        marco, _ = voxeles.desde_hoja(p, res=24, filas=2, fila=1)
        assert marco.shape == (20, 6, 24), marco.shape
        assert not marco[10, 3, 12] and marco[1, 3, 12]  # el hueco queda hueco
        cil, rgb = voxeles.desde_hoja(p, res=24, filas=2, fila=2, seccion="elipse")
        caja, _ = voxeles.desde_hoja(p, res=24, filas=2, fila=2)
        assert cil.shape == (16, 16, 24) and caja.sum() == 16 * 16 * 24
        area = cil[:, :, 12].sum()
        assert abs(area - np.pi * 8 * 8) < 0.12 * np.pi * 8 * 8, area  # una elipse (un circulo), no la caja
        assert cil[:, :, 12].any(axis=1).all() and cil[:, :, 12].any(axis=0).all()  # las siluetas, enteras
        assert cerca(rgb[8, 0, 12], RED) and cerca(rgb[8, 15, 12], BLUE)  # el frente rojo, la espalda azul
        assert cerca(rgb[15, 8, 12], GREEN)  # el costado, del lado


def test_superficie_suave():
    """-o algo.glb: la superficie suave del volumen, cerrada, con color por vertice y el frente hacia +X."""
    import trimesh

    with tempfile.TemporaryDirectory() as tmp:
        p = hoja_de_dos_objetos(tmp)
        out = os.path.join(tmp, "cil.glb")
        assert voxeles.main(["hoja", p, "--filas", "2", "--fila", "2", "--res", "32", "--seccion", "elipse", "-o", out]) == 0
        m = trimesh.load(out, force="mesh")
    assert m.is_watertight and len(m.faces) > 200
    ext = m.extents  # un cilindro de 16 x 16 x 24 celdas (a res 32: 21 x 21 x 32)
    assert abs(ext[1] - 32) < 2.5 and abs(ext[0] - ext[2]) < 2.5, ext
    cols = np.asarray(m.visual.vertex_colors)[:, :3].astype(int)
    v = np.asarray(m.vertices)
    front = v[:, 0] > v[:, 0].max() - 1.0  # el frente (rojo) hacia +X; la espalda (azul) hacia -X
    back = v[:, 0] < v[:, 0].min() + 1.0
    assert cerca(np.median(cols[front], axis=0), RED, 50) and cerca(np.median(cols[back], axis=0), BLUE, 50)


def escribir_ply(path, pts, f_dc, opacity_logit):
    n = len(pts)
    head = ("ply\nformat binary_little_endian 1.0\nelement vertex %d\nproperty float x\nproperty float y\n"
            "property float z\nproperty float f_dc_0\nproperty float f_dc_1\nproperty float f_dc_2\n"
            "property float opacity\nend_header\n" % n).encode("ascii")
    rows = np.zeros(n, dtype=[("x", "<f4"), ("y", "<f4"), ("z", "<f4"), ("f0", "<f4"), ("f1", "<f4"), ("f2", "<f4"), ("o", "<f4")])
    rows["x"], rows["y"], rows["z"] = pts[:, 0], pts[:, 1], pts[:, 2]
    rows["f0"], rows["f1"], rows["f2"] = f_dc[:, 0], f_dc[:, 1], f_dc[:, 2]
    rows["o"] = opacity_logit
    with open(path, "wb") as fh:
        fh.write(head + rows.tobytes())


def test_splat_esfera():
    rng = np.random.default_rng(7)
    n = 40000
    v = rng.normal(size=(n, 3))
    pts = v / np.linalg.norm(v, axis=1, keepdims=True)  # la superficie de una esfera de radio 1
    c0 = 0.28209479177387814
    target = np.array(RED) / 255.0
    f_dc = np.tile((target - 0.5) / c0, (n, 1))
    logit = np.full(n, 4.0)
    logit[: n // 10] = -6.0  # un 10 % casi transparente: se descarta
    with tempfile.TemporaryDirectory() as tmp:
        p = os.path.join(tmp, "e.ply")
        escribir_ply(p, pts.astype(np.float32), f_dc.astype(np.float32), logit.astype(np.float32))
        filled, rgb = voxeles.desde_splat(p, res=24)
    r = 24 / 2.0  # el radio en celdas: la caja queda justa a la esfera
    esperado = 4.0 / 3.0 * np.pi * r ** 3
    assert abs(filled.sum() - esperado) < esperado * 0.25, (int(filled.sum()), esperado)
    surf, _ = voxeles.expuestas(filled)
    assert cerca(np.median(rgb[surf], axis=0), RED, 12)


def test_malla_caja():
    import trimesh

    box = trimesh.creation.box(extents=(2.0, 3.0, 1.0))  # x 2 (el frente mira a +x), y 3 (arriba), z 1
    box.visual.vertex_colors = np.tile(np.array(GREEN + (255,), np.uint8), (len(box.vertices), 1))
    with tempfile.TemporaryDirectory() as tmp:
        p = os.path.join(tmp, "caja.glb")
        box.export(p)
        filled, rgb = voxeles.desde_malla(p, res=30, frente="+x")
    sx, sy, sz = filled.shape  # en el .vox: el ancho (z del GLB), el fondo (x) y el alto (y)
    assert abs(sx - 10) <= 2 and abs(sy - 20) <= 2 and abs(sz - 30) <= 2, filled.shape
    assert filled.sum() >= 0.85 * sx * sy * sz
    surf, _ = voxeles.expuestas(filled)
    assert cerca(np.median(rgb[surf], axis=0), GREEN, 12)


def test_frente_a_menos_y():
    a = np.zeros((4, 3, 1), bool)
    a[3, 1, 0] = True  # en +x
    b = voxeles.frente_a_menos_y(a, "+x")
    assert b.shape == (3, 4, 1) and b[1, 0, 0]  # paso al frente (y = 0)
    a = np.zeros((4, 3, 1), bool)
    a[0, 1, 0] = True  # en -x
    b = voxeles.frente_a_menos_y(a, "-x")
    assert b[b.shape[0] - 1 - 1, 0, 0]
    a = np.zeros((4, 3, 1), bool)
    a[2, 2, 0] = True  # en +y
    assert voxeles.frente_a_menos_y(a, "+y")[1, 0, 0]


def test_cli():
    with tempfile.TemporaryDirectory() as tmp:
        (f, l, a), _ = vistas_de_una_L(tmp)
        out, prev = os.path.join(tmp, "o.vox"), os.path.join(tmp, "o.png")
        assert voxeles.main(["vistas", f, l, a, "-o", out, "--res", "24", "--vista-previa", prev]) == 0
        assert os.path.getsize(out) > 0 and os.path.getsize(prev) > 0
        with open(out, "rb") as fh:
            head = fh.read(8)
        assert head[:4] == b"VOX " and struct.unpack("<i", head[4:])[0] == 150
        assert voxeles.main(["ver", out, "-o", prev]) == 0


def test_templo_forma():
    """El templo armado a mano: las medidas del inventario, simetrico, la escalera de un escalon por celda hasta la
    puerta abierta, sin aire encerrado (sus caras no las ve nadie) y con los triangulos que admite modelos.py."""
    from scipy import ndimage

    o = estructuras.templo()
    assert o.metros() == (24.0, 14.0, 30.0), o.metros()
    lleno = o.lleno
    assert (lleno == lleno[:, ::-1]).all()  # simetrico de lado a lado
    for u in range(16):  # el escalon u pisa a u + 1 celdas y arriba hay aire hasta la cornisa del portico
        col = lleno[u, 23]
        assert col[:u + 1].all() and not col[u + 1:22].any(), u
    assert not lleno[16:19, 22:26, 16:22].any() and lleno[19, 22:26, 16:22].all()  # el vano y su fondo
    vacio = np.pad(~lleno, 1, constant_values=True)
    lab, _ = ndimage.label(vacio)
    assert (lab[vacio] == lab[0, 0, 0]).all()  # todo el aire llega de afuera
    rgb, vis = estructuras.colorear(o)
    grid, pal = estructuras.a_vox(o, rgb, vis)
    assert grid.shape == (48, 60, 28) and (grid[:, :, 27] > 0).any()  # 14 m: los pilonos llegan arriba
    assert (grid[:, 0] > 0).any() and (grid[:, 59] > 0).any()
    assert estructuras.triangulos(grid) < 20000


def test_templo_colores():
    """Con una hoja sintetica (frente rojo, lado verde, espalda azul): cada cara toma su vista, cada piedra queda
    de un solo tono y el hueco de la puerta llega tal cual a la paleta."""
    with tempfile.TemporaryDirectory() as tmp:
        img = Image.new("RGB", (1700, 500), (255, 255, 255))
        d = ImageDraw.Draw(img)
        d.rectangle([40, 100, 459, 445], fill=RED)  # frente, 420 x 346 (como la hoja del templo)
        d.rectangle([500, 147, 1197, 445], fill=GREEN)  # lado, 698 x 299
        d.rectangle([1240, 141, 1671, 445], fill=BLUE)  # espalda, 432 x 305
        p = os.path.join(tmp, "hoja.png")
        img.save(p)
        o = estructuras.templo()
        rgb, vis = estructuras.colorear(o, voxeles.hoja(p), estructuras.ALTURAS["templo"], luz=1.0)
        dom = np.argmax(rgb, axis=-1)
        assert vis[0, 40, 2] and dom[0, 40, 2] == 0  # el frente del zocalo: rojo
        assert vis[40, 0, 2] and dom[40, 0, 2] == 1  # su costado: verde
        assert vis[59, 40, 2] and dom[59, 40, 2] == 2  # su espalda: azul
        for u0 in range(6, 54, 6):  # las piedras del costado del zocalo (6 x 3 celdas): de un solo tono cada una
            piedra = rgb[u0:u0 + 6, 0, 0:3].reshape(-1, 3)
            assert np.ptp(piedra, axis=0).max() < 1e-3, u0
        tonos = {round(float(rgb[u0, 0, 0, 1]), 1) for u0 in range(6, 54, 6)}
        assert len(tonos) >= 5, tonos  # y no todas del mismo
        grid, pal = estructuras.a_vox(o, rgb, vis)
        fondo = grid[47 - 23, 19, 18]  # el fondo de la puerta (x del .vox: el ancho al reves)
        assert tuple(pal[fondo - 1]) == estructuras.HUECO, pal[fondo - 1]


def test_triangulos_juntos():
    """modelos.tris_vox cuenta lo que arma el juego (src/sim/voxel.c: las caras juntas y la base en el suelo): la
    figura de prueba de tests/test_main.c (merge_fixture) da los mismos triangulos que alla (MERGE_FIXTURE_TRIS)."""
    sys.path.insert(0, os.path.join(HERE, "..", "modelos"))
    import modelos

    nx, ny, nz = 8, 7, 7  # en los ejes del juego (y arriba), como en la prueba en C
    celdas = {}
    for (x0, y0, z0, x1, y1, z1, m) in ((0, 0, 0, 7, 1, 6, 1), (2, 2, 2, 5, 4, 4, 2), (1, 5, 1, 6, 5, 5, 1),
                                        (6, 0, 2, 7, 1, 4, 0), (3, 3, 0, 4, 3, 6, 0)):
        for x in range(x0, x1 + 1):
            for y in range(y0, y1 + 1):
                for z in range(z0, z1 + 1):
                    if m:
                        celdas[(x, y, z)] = m
                    else:
                        celdas.pop((x, y, z), None)
    vox_cells = {(nz - 1 - z, nx - 1 - x, y): m for (x, y, z), m in celdas.items()}
    assert modelos.tris_vox(vox_cells, (nz, nx, ny)) == 274
    g = np.ones((3, 3, 3), np.uint8)
    # El cubo apoyado: arriba 2; de la base queda el anillo del borde (4 rectangulos, 12), y sus esquinas parten
    # el borde de abajo de tres de los lados (12 en los costados): 26, lo mismo que arma el juego.
    assert estructuras.triangulos(g) == 26
    o = estructuras.templo()
    grid, _ = estructuras.a_vox(o, *estructuras.colorear(o))
    assert estructuras.triangulos(grid) < 8000  # el templo, bajo el aviso de Android


def main():
    tests = [
        ("test_vox_ida_y_vuelta", test_vox_ida_y_vuelta),
        ("test_vistas_L (fondo blanco)", lambda: test_vistas_L(False)),
        ("test_vistas_L (transparente)", lambda: test_vistas_L(True)),
        ("test_vistas_voltear", test_vistas_voltear),
        ("test_hoja", test_hoja),
        ("test_superficie_suave", test_superficie_suave),
        ("test_splat_esfera", test_splat_esfera),
        ("test_malla_caja", test_malla_caja),
        ("test_frente_a_menos_y", test_frente_a_menos_y),
        ("test_cli", test_cli),
        ("test_templo_forma", test_templo_forma),
        ("test_templo_colores", test_templo_colores),
        ("test_triangulos_juntos", test_triangulos_juntos),
    ]
    fails = 0
    for name, fn in tests:
        try:
            fn()
            print(f"ok   {name}")
        except Exception:
            fails += 1
            print(f"FALLA {name}")
            traceback.print_exc()
    print(f"{len(tests) - fails}/{len(tests)} pruebas de voxeles")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
