#!/usr/bin/env python3
"""Pruebas de la tuberia de modelos (tools/modelos), sin GPU ni red: una fuente sintetica hace de
salida de TRELLIS.2 (una vasija densa con textura). Corre en el CI (con las dependencias de
requirements.txt) y a mano: python3 tools/modelos/test_modelos.py
"""
import os
import shutil
import sys
import tempfile
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "assets"))
sys.path.insert(0, os.path.join(HERE, ".."))

import numpy as np  # noqa: E402
import trimesh  # noqa: E402
from PIL import Image  # noqa: E402

import animaciones  # noqa: E402
import fetch_pipeline_asset  # noqa: E402
import glb  # noqa: E402
import inventario  # noqa: E402
import lowres  # noqa: E402
import modelos  # noqa: E402

RED, CLAY, BLACK = (200, 30, 30), (176, 98, 60), (30, 24, 20)


def fuente_trellis(path):
    """Una vasija de revolucion como las de TRELLIS: densa, con textura, +Y arriba, de un metro, con
    una mancha roja hacia -Z (para ver el giro) y un pedacito suelto (el ruido de la generacion)."""
    prof = np.array([[0.0, 0.0], [0.30, 0.0], [0.42, 0.25], [0.40, 0.55], [0.22, 0.80], [0.18, 0.95], [0.24, 1.0], [0.0, 1.0]])
    jar = trimesh.creation.revolve(prof, sections=96).subdivide().subdivide()
    jar.apply_transform(trimesh.transformations.rotation_matrix(-np.pi / 2, [1, 0, 0]))
    v = jar.vertices
    u = (np.arctan2(v[:, 0], v[:, 2]) / (2 * np.pi)) % 1.0  # 0: hacia +Z; 0.5: hacia -Z
    tex = np.zeros((128, 256, 3), np.uint8)
    tex[:] = CLAY
    tex[40:52, :] = BLACK
    tex[60:100, 112:144] = RED
    mat = trimesh.visual.material.PBRMaterial(baseColorTexture=Image.fromarray(tex), baseColorFactor=[255, 255, 255, 255])
    jar.visual = trimesh.visual.TextureVisuals(uv=np.column_stack([u, v[:, 1]]), material=mat)
    crumb = trimesh.creation.icosphere(subdivisions=1, radius=0.01)
    crumb.apply_translation([0.8, 0.5, 0.0])
    trimesh.Scene([jar, crumb]).export(path)
    return len(jar.faces)


def faces_of(data):
    """(posiciones por cara, color por cara) de un GLB de lowres."""
    with tempfile.NamedTemporaryFile(suffix=".glb", delete=False) as f:
        f.write(data)
        name = f.name
    try:
        g = glb.Glb(name)
    finally:
        os.unlink(name)
    acc = g.json["accessors"]
    view = g.json["bufferViews"]

    def read(i, dtype, width):
        a, v = acc[i], view[acc[i]["bufferView"]]
        start = v.get("byteOffset", 0) + a.get("byteOffset", 0)
        return np.frombuffer(g.bin[start:start + v["byteLength"]], dtype=dtype).reshape(-1, width)

    attrs = g.json["meshes"][0]["primitives"][0]["attributes"]
    pos = read(attrs["POSITION"], "<f4", 3).reshape(-1, 3, 3)
    col = read(attrs["COLOR_0"], np.uint8, 4)[::3, :3].astype(np.float64)
    return g, pos, col


def test_convertir(tmp):
    src = os.path.join(tmp, "vasija.glb")
    n = fuente_trellis(src)
    data, inf = lowres.convertir(src, (0.35, 0.5, 0.35), 120, girar=90, colores=12, nombre="prueba")
    g, pos, col = faces_of(data)
    assert inf["tris_fuente"] > n * 0.99 and inf["tris"] <= 120 and g.triangulos() == inf["tris"], inf
    lo, hi = g.limites()
    assert abs((hi[1] - lo[1]) - 0.5) < 1e-3, "el alto del inventario manda (es su medida mas larga)"
    assert abs(lo[1]) < 1e-4 and abs(lo[0] + hi[0]) < 1e-3 and abs(lo[2] + hi[2]) < 1e-3, "origen en el centro de la base"
    assert hi[0] - lo[0] < 0.6 and hi[2] - lo[2] < 0.6, "el pedacito suelto no cuenta"
    assert {"POSITION", "NORMAL", "COLOR_0"} <= g.atributos() and not g.imagenes(), "facetado de colores, sin texturas"
    dark = col.sum(axis=1) < 120
    assert dark.any(), "la franja negra sobrevive al diezmado"
    # El giro: la mancha roja estaba hacia -Z; con 90 grados alrededor de +Y queda hacia -X. Es chica:
    # con 120 caras se pierde (cada cara toma un solo color, el que mas cubre); con 400, no.
    _, pos400, col400 = faces_of(lowres.convertir(src, (0.35, 0.5, 0.35), 400, girar=90, colores=12)[0])
    red = (col400[:, 0] > 2.5 * col400[:, 1]) & (col400[:, 0] > 100)  # el barro tambien es rojizo (176, 98, 60)
    assert red.any(), "la mancha roja"
    centers = pos400.mean(axis=1)
    assert centers[red, 0].mean() < -0.1 and abs(centers[red, 2].mean()) < 0.08, centers[red].mean(axis=0)
    # La paleta corta (antes de la luz) y la luz del sol: las caras de arriba, mas claras.
    assert inf["paleta"] <= 12, inf
    flat, _ = lowres.convertir(src, (0.35, 0.5, 0.35), 120, girar=90, colores=12, luz=False)
    _, _, col_flat = faces_of(flat)
    assert (col <= col_flat + 1).all() and (col < col_flat - 2).any(), "la luz horneada oscurece, no aclara"


def test_rotar_y_escala(tmp):
    # Una espada acostada a lo largo de X (como sale de una foto) que va de pie (el inventario: alto).
    blade = trimesh.creation.box(extents=[1.0, 0.04, 0.12])
    blade.visual.face_colors = [180, 180, 190, 255]
    src = os.path.join(tmp, "espada.glb")
    blade.export(src)
    data, inf = lowres.convertir(src, (0.07, 0.6, 0.03), 100, rotar=[("z", 90)])
    g, _, _ = faces_of(data)
    lo, hi = g.limites()
    assert abs((hi[1] - lo[1]) - 0.6) < 1e-3 and hi[0] - lo[0] < 0.05, (lo, hi)
    data, inf = lowres.convertir(src, (0.07, 0.6, 0.03), 100, rotar=[("z", 90)], escala=0.5, origen=(0.0, 0.1, 0.0))
    g, _, _ = faces_of(data)
    lo, hi = g.limites()
    assert abs((hi[1] - lo[1]) - 0.5) < 1e-3 and abs(lo[1] + 0.1) < 1e-3, "escala a mano y el agarre en el origen"
    assert modelos.parse_rotations("z:90, y:-45") == [("z", 90.0), ("y", -45.0)]
    assert modelos.parse_origin("0,0.1,0") == [0.0, 0.1, 0.0] and modelos.parse_origin("") is None


def test_check_model(tmp):
    it = {"id": "utileria.objeto.caldero", "tags": [], "tris_max": "80", "medidas_m": "0.6x0.5x0.6", "categoria": "utileria"}
    src = os.path.join(tmp, "vasija.glb")
    if not os.path.isfile(src):
        fuente_trellis(src)
    ok_path = os.path.join(tmp, "bien.glb")
    with open(ok_path, "wb") as f:
        f.write(lowres.convertir(src, (0.6, 0.5, 0.6), 80, girar=90)[0])
    errors, warnings = modelos.check_model(it, ok_path)
    assert not errors and not warnings, (errors, warnings)
    big = dict(it, tris_max="20")
    errors, _ = modelos.check_model(big, ok_path)
    assert errors and "tris_max" in errors[0], errors
    # Una textura grande (la de TRELLIS sin bajar) es un error; el modelo flotando, un aviso.
    box = trimesh.creation.box(extents=[0.6, 0.5, 0.6])
    box.apply_translation([0, 0.6, 0])
    img = Image.fromarray(np.zeros((512, 512, 3), np.uint8) + 120)
    box.visual = trimesh.visual.TextureVisuals(uv=np.random.default_rng(1).random((len(box.vertices), 2)),
                                               material=trimesh.visual.material.PBRMaterial(baseColorTexture=img))
    tex_path = os.path.join(tmp, "textura.glb")
    box.export(tex_path)
    errors, warnings = modelos.check_model(it, tex_path)
    assert any("512x512" in e for e in errors) and any("base" in w for w in warnings), (errors, warnings)


def test_cline(tmp):
    items, _ = inventario.load()
    assert fetch_pipeline_asset.resolve(items, "akinakes", "prop") == "arma.corta.akinakes"
    assert fetch_pipeline_asset.resolve(items, "lobo", "creature") == "animal.salvaje.lobo"
    assert fetch_pipeline_asset.resolve(items, "arma.corta.akinakes", "other") == "arma.corta.akinakes"
    try:
        fetch_pipeline_asset.resolve(items, "lobo", "other")
        raise AssertionError("'lobo' sin categoria es ambiguo")
    except SystemExit as e:
        assert "ambiguous" in str(e)


def test_traer(tmp):
    """traer en una copia del repositorio: el inventario, los documentos y el manifiesto."""
    root = os.path.join(tmp, "repo")
    for rel in ("assets/inventario.tsv", "assets/animaciones.tsv", "docs/INVENTARIO.md",
                "assets/models/estructura/vivienda/yurta_comun.glb"):
        os.makedirs(os.path.dirname(os.path.join(root, rel)), exist_ok=True)
        shutil.copyfile(os.path.join(inventario.ROOT, rel), os.path.join(root, rel))
    saved = {}
    patches = [(inventario, "ROOT", root), (inventario, "TSV", os.path.join(root, "assets", "inventario.tsv")),
               (inventario, "DOC", os.path.join(root, "docs", "INVENTARIO.md")), (animaciones, "ROOT", root),
               (animaciones, "TSV", os.path.join(root, "assets", "animaciones.tsv")), (modelos, "ROOT", root),
               (modelos, "ARTE", os.path.join(root, "arte", "modelos")),
               (modelos, "FUENTES", os.path.join(root, "arte", "modelos", "fuentes")),
               (modelos, "PARA_RIGGEAR", os.path.join(root, "arte", "modelos", "para_riggear")),
               (modelos, "MANIFEST", os.path.join(root, "arte", "modelos", "models_manifest.json"))]
    for mod, name, value in patches:
        saved[(mod, name)] = getattr(mod, name)
        setattr(mod, name, value)
    try:
        src = os.path.join(tmp, "vasija.glb")
        if not os.path.isfile(src):
            fuente_trellis(src)
        quiet = lambda *_: None  # noqa: E731
        assert modelos.traer("utileria.objeto.caldero", archivo=src, girar=90, fuente="trellis2", log=quiet) == 0
        out = os.path.join(root, "assets", "models", "utileria", "objeto", "caldero.glb")
        assert os.path.isfile(out) and glb.Glb(out).triangulos() <= 80
        items, _ = inventario.load()
        assert modelos.find_item(items, "utileria.objeto.caldero")["estado"] == "importado"
        doc = open(os.path.join(root, "docs", "INVENTARIO.md"), encoding="utf-8").read()
        assert doc == inventario.render_doc(items) + "\n", "docs/INVENTARIO.md al dia"
        entry = modelos.load_manifest()["models"]["utileria.objeto.caldero"]
        assert entry["source"]["tool"] == "trellis2" and len(entry["source"]["sha256"]) == 64 and entry["params"]["girar"] == 90
        try:
            modelos.traer("utileria.objeto.caldero", archivo=src, log=quiet)
            raise AssertionError("sin --reemplazar no pisa un modelo")
        except SystemExit:
            pass
        # Un animal necesita esqueleto y clips: la version low-poly queda para riggear, sin entrar al juego.
        assert modelos.traer("animal.salvaje.lobo", archivo=src, log=quiet) == 0
        assert os.path.isfile(os.path.join(root, "arte", "modelos", "para_riggear", "animal.salvaje.lobo.glb"))
        items, _ = inventario.load()
        assert modelos.find_item(items, "animal.salvaje.lobo")["estado"] == "pendiente"
        assert not os.path.isfile(os.path.join(root, "assets", "models", "animal", "salvaje", "lobo.glb"))
        errors, _ = inventario.validate(items)
        clips, _ = animaciones.load_clips()
        aerrors, _ = animaciones.validate(items, clips)
        assert not errors and not aerrors, errors + aerrors
    finally:
        for (mod, name), value in saved.items():
            setattr(mod, name, value)


def main():
    tests = [test_convertir, test_rotar_y_escala, test_check_model, test_cline, test_traer]
    failed = 0
    tmp = tempfile.mkdtemp(prefix="modelos-")
    try:
        for t in tests:
            try:
                t(tmp)
                print(f"ok   {t.__name__}")
            except Exception:
                failed += 1
                print(f"FAIL {t.__name__}")
                traceback.print_exc()
    finally:
        shutil.rmtree(tmp, ignore_errors=True)
    print(f"{len(tests) - failed}/{len(tests)} pruebas de la tuberia de modelos")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
