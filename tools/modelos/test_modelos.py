#!/usr/bin/env python3
"""Pruebas de la tuberia de modelos (tools/modelos), sin GPU ni red: una fuente sintetica hace de
salida de TRELLIS.2 (una vasija densa con textura). Corre en el CI (con las dependencias de
requirements.txt) y a mano: python3 tools/modelos/test_modelos.py
"""
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
    items, _ = inventario.load()
    imported = [inventario.path_of(it) for it in items if it["estado"] != "pendiente"]  # sus GLB y sus .vox
    for rel in ["assets/inventario.tsv", "assets/animaciones.tsv", "docs/INVENTARIO.md"] + imported:
        if not os.path.isfile(os.path.join(inventario.ROOT, rel)):
            continue
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
        assert modelos.traer("utileria.objeto.yunque", archivo=src, girar=90, fuente="trellis2", log=quiet) == 0
        out = os.path.join(root, "assets", "models", "utileria", "objeto", "yunque.glb")
        assert os.path.isfile(out) and glb.Glb(out).triangulos() <= 80
        items, _ = inventario.load()
        assert modelos.find_item(items, "utileria.objeto.yunque")["estado"] == "importado"
        doc = open(os.path.join(root, "docs", "INVENTARIO.md"), encoding="utf-8").read()
        assert doc == inventario.render_doc(items) + "\n", "docs/INVENTARIO.md al dia"
        entry = modelos.load_manifest()["models"]["utileria.objeto.yunque"]
        assert entry["source"]["tool"] == "trellis2" and len(entry["source"]["sha256"]) == 64 and entry["params"]["girar"] == 90
        try:
            modelos.traer("utileria.objeto.yunque", archivo=src, log=quiet)
            raise AssertionError("sin --reemplazar no pisa un modelo")
        except SystemExit:
            pass
        # Un animal necesita esqueleto y clips: la version low-poly queda para riggear, sin entrar al juego.
        assert modelos.traer("animal.salvaje.liebre", archivo=src, log=quiet) == 0
        assert os.path.isfile(os.path.join(root, "arte", "modelos", "para_riggear", "animal.salvaje.liebre.glb"))
        items, _ = inventario.load()
        assert modelos.find_item(items, "animal.salvaje.liebre")["estado"] == "pendiente"
        assert not os.path.isfile(os.path.join(root, "assets", "models", "animal", "salvaje", "liebre.glb"))
        errors, _ = inventario.validate(items)
        clips, _ = animaciones.load_clips()
        aerrors, _ = animaciones.validate(items, clips)
        assert not errors and not aerrors, errors + aerrors
    finally:
        for (mod, name), value in saved.items():
            setattr(mod, name, value)


def test_suave(tmp):
    """--suave (estilo PS2): vertices compartidos con indices, normales unitarias y el color por vertice; el
    mismo presupuesto, la base en el suelo y la mancha roja (hacia -Z en la fuente) hacia +X."""
    src = os.path.join(tmp, "vasija.glb")
    if not os.path.isfile(src):
        fuente_trellis(src)
    data, info = lowres.convertir(src, (0.6, 0.5, 0.6), 200, girar=90, colores=0, suave=True)
    assert info["suave"] and info["tris"] <= 200, info
    with tempfile.NamedTemporaryFile(suffix=".glb", delete=False) as f:
        f.write(data)
        name = f.name
    try:
        g = glb.Glb(name)
    finally:
        os.unlink(name)
    assert g.triangulos() == info["tris"]
    prim = g.json["meshes"][0]["primitives"][0]
    assert "indices" in prim and {"POSITION", "NORMAL", "COLOR_0"} <= set(prim["attributes"])
    acc, view = g.json["accessors"], g.json["bufferViews"]

    def read(i, dtype, width):
        a, v = acc[i], view[acc[i]["bufferView"]]
        start = v.get("byteOffset", 0) + a.get("byteOffset", 0)
        return np.frombuffer(g.bin[start:start + a["count"] * width * np.dtype(dtype).itemsize], dtype=dtype).reshape(-1, width)

    pos = read(prim["attributes"]["POSITION"], "<f4", 3)
    nor = read(prim["attributes"]["NORMAL"], "<f4", 3)
    col = read(prim["attributes"]["COLOR_0"], "u1", 4)[:, :3].astype(int)
    assert len(pos) < 3 * info["tris"]  # comparte vertices (el facetado repite tres por cara)
    assert np.allclose(np.linalg.norm(nor, axis=1), 1.0, atol=1e-3)
    assert abs(pos[:, 1].min()) < 1e-4 and abs(pos[:, 1].max() - 0.5) < 0.12  # la base en el suelo
    red = (col[:, 0] > col[:, 1] + 30) & (col[:, 0] > col[:, 2] + 30)
    assert red.any() and pos[red][:, 0].mean() > 0


def test_refs_anigen(tmp):
    """Las referencias de AniGen (preparar_refs_anigen.py), sin rembg: separar las tres vistas de una hoja, limpiar el
    recorte, elegir el fondo y componer el cuadrado con el fondo exacto."""
    import preparar_refs_anigen as pra

    # Una hoja de tres vistas en columnas, sobre un gris con viñeta: las cajas, en orden y cada una con su sujeto.
    h, w = 300, 900
    yy, xx = np.mgrid[0:h, 0:w]
    hoja = np.stack([170 + 20 * xx / w] * 3, axis=2).astype(np.float32)
    for k, cx in enumerate((150, 450, 750)):
        hoja[60:260, cx - 40 - 20 * k:cx + 40 + 20 * k] = (120, 70, 30)
    cajas = pra.recortes(hoja, "columnas")
    assert len(cajas) == 3
    for (x0, y0, x1, y1), cx in zip(cajas, (150, 450, 750)):
        assert x0 < cx - 60 and cx + 60 < x1 and y0 == 0 and y1 == h
    assert cajas[0][2] <= cajas[1][0] and cajas[1][2] <= cajas[2][0]  # no se pisan
    # En filas (las aves) y en paneles con líneas blancas (las personas).
    filas = np.transpose(hoja, (1, 0, 2)).copy()
    assert [c[1] < c[3] for c in pra.recortes(filas, "filas")] == [True] * 3
    paneles = hoja.copy()
    paneles[:, 299:302] = 250
    paneles[:, 599:602] = 250
    assert [(c[0], c[2]) for c in pra.recortes(paneles, "paneles")] == [(3, 296), (304, 596), (604, 897)]

    # Limpiar: se van las motas (un texto) y se rellena un hueco chico dentro del sujeto; los grandes quedan.
    alfa = np.zeros((200, 200), np.float32)
    alfa[40:180, 40:160] = 1.0
    alfa[5:9, 5:30] = 1.0          # un texto arriba
    alfa[100:103, 100:103] = 0.0   # una mancha blanca del pelaje
    alfa[60:160, 70:90] = 0.0      # el hueco entre las patas: abierto abajo, no se rellena
    alfa[160:180, 70:90] = 0.0
    limpio, toca, piezas = pra.limpiar(alfa)
    assert limpio[5:9, 5:30].max() == 0 and limpio[100:103, 100:103].min() == 1.0
    assert limpio[100:150, 75:85].max() == 0 and not toca and piezas == 1
    _, toca, _ = pra.limpiar(np.pad(np.ones((50, 50), np.float32), ((0, 0), (0, 20))))
    assert toca  # el sujeto toca el borde de su vista: avisa

    # El fondo: blanco a lo pardo, negro a lo blanco, verde a lo de dos tonos; los de la tabla, el suyo.
    def sujeto(*colores):
        c = np.zeros((10, 10 * len(colores), 3), np.float32)
        for i, col in enumerate(colores):
            c[:, i * 10:(i + 1) * 10] = col
        return c, np.ones(c.shape[:2], np.float32)

    assert pra.elegir_fondo(*sujeto((120, 80, 40)), "animal.salvaje.oso")[0] == pra.BLANCO
    assert pra.elegir_fondo(*sujeto((235, 232, 220)), "animal.ganado.oveja")[0] == pra.NEGRO
    assert pra.elegir_fondo(*sujeto((120, 120, 120), (40, 40, 40), (240, 240, 240)), "animal.salvaje.lobo")[0] == pra.VERDE
    assert pra.elegir_fondo(*sujeto((120, 80, 40)), "animal.salvaje.tigre")[0] == pra.VERDE

    # Componer: un cuadrado de 256 con el fondo exacto alrededor, el sujeto centrado y con margen; el chequeo lo pasa.
    color, a = sujeto((120, 80, 40), (60, 40, 20))
    a = np.pad(a, 5)
    color = np.pad(color, ((5, 5), (5, 5), (0, 0)), mode="edge")
    img = pra.componer(color, a, pra.VERDE, tam=256)
    assert img.shape == (256, 256, 3) and (img[0] == pra.VERDE).all() and (img[:, -1] == pra.VERDE).all()
    ruta = os.path.join(tmp, "unica_01.png")
    Image.fromarray(img).save(ruta)
    assert pra.revisar(ruta, pra.VERDE) == []
    assert pra.revisar(ruta, pra.BLANCO)  # con otro fondo, el chequeo falla


def test_refs_anigen_registro(tmp):
    """Las referencias de AniGen que están en el repo siguen bien: el archivo con su sha256, el fondo exacto, los
    márgenes y una sola pieza (lo mismo que preparar_refs_anigen.py --check)."""
    import preparar_refs_anigen as pra

    assert pra.chequear(pra.cargar_registro()) == 0


def colores_glb(data):
    """El COLOR_0 (RGB) de un GLB facetado de lowres.py, un color por vértice."""
    largo = struct.unpack_from("<I", data, 12)[0]
    doc = json.loads(data[20:20 + largo])
    acc = doc["accessors"][doc["meshes"][0]["primitives"][0]["attributes"]["COLOR_0"]]
    vista = doc["bufferViews"][acc["bufferView"]]
    return np.frombuffer(data[20 + largo + 8:], np.uint8, acc["count"] * 4, vista.get("byteOffset", 0)).reshape(-1, 4)[:, :3]


def test_adaptar_oga(tmp):
    """adaptar_oga: color por partes (la pieza chica, de hierro), luz horneada, la altura pedida con el origen
    en el centro de la base, y repetirlo da lo mismo."""
    import adaptar_oga
    cuerpo = trimesh.creation.icosphere(subdivisions=1, radius=1.0)
    aro = trimesh.creation.box(extents=(0.3, 0.3, 0.3))
    aro.apply_translation((3.0, 0.5, 0.0))
    pos = np.concatenate([cuerpo.vertices[cuerpo.faces], aro.vertices[aro.faces]]).reshape(-1, 3)
    normales = np.concatenate([cuerpo.face_normals, aro.face_normals])
    gris = np.full((len(pos), 3), 60)
    data = lowres._glb_bytes(pos, np.repeat(normales, 3, axis=0), gris, "prueba")
    salida, informe = adaptar_oga.adaptar(data, "madera_herrajes", alto=0.5)
    assert informe["partes"] == 2, informe
    nuevo, _ = adaptar_oga.leer_glb(salida)
    lo, hi = nuevo.min(axis=0), nuevo.max(axis=0)
    assert abs(hi[1] - lo[1] - 0.5) < 1e-5 and abs(lo[1]) < 1e-6, (lo, hi)
    assert abs(lo[0] + hi[0]) < 1e-5 and abs(lo[2] + hi[2]) < 1e-5, (lo, hi)
    col = colores_glb(salida).reshape(-1, 3, 3)[:, 0].astype(int)
    n = len(cuerpo.faces)
    madera, hierro = col[:n], col[n:]
    assert (madera[:, 0] - madera[:, 2]).min() > 30, madera[:3]  # parda, no gris
    assert np.abs(hierro[:, 0] - hierro[:, 2]).max() < 12 and hierro.max() < 90, hierro[:3]  # hierro
    arriba, abajo = normales[n:, 1].argmax(), normales[n:, 1].argmin()
    assert hierro[arriba].sum() > hierro[abajo].sum()  # la luz del sol, horneada
    assert adaptar_oga.adaptar(salida, "madera_herrajes", alto=0.5)[0] == salida


def test_animales(tmp):
    """animales.py: sin las piezas sueltas, el color desde una imagen con fondo de color, y el esqueleto de un
    cuadrupedo de cajas (cuatro patas, la cabeza adelante, la cola atras) con sus clips."""
    import animales
    import mcubes
    partes = [((0.0, 1.0, 0.0), (1.2, 0.5, 0.4)),    # el cuerpo
              ((0.85, 1.35, 0.0), (0.5, 0.25, 0.22)),  # el cuello y la cabeza, adelante (+X)
              ((-0.8, 1.05, 0.0), (0.45, 0.08, 0.08))]  # la cola
    for x in (0.45, -0.45):
        for z in (0.13, -0.13):
            partes.append(((x, 0.4, z), (0.12, 0.8, 0.12)))  # las patas
    # Una sola malla, cerrada y soldada (como las de Hunyuan3D): las cajas en una grilla, con marching cubes.
    paso = 0.04
    xs, ys, zs = (np.arange(a, b, paso) for a, b in ((-1.2, 1.3), (-0.1, 2.8), (-0.4, 0.4)))
    X, Y, Z = np.meshgrid(xs, ys, zs, indexing="ij")
    lleno = np.zeros(X.shape, bool)
    for (cx, cy, cz), (lx, ly, lz) in partes + [((0.0, 2.5, 0.0), (0.12, 0.12, 0.12))]:  # y una pieza suelta, arriba
        lleno |= (np.abs(X - cx) <= lx / 2) & (np.abs(Y - cy) <= ly / 2) & (np.abs(Z - cz) <= lz / 2)
    vv, ff = mcubes.marching_cubes(lleno.astype(float), 0.5)
    m = trimesh.Trimesh(vv * paso + [xs[0], ys[0], zs[0]], ff)
    v, f = np.asarray(m.vertices, float), np.asarray(m.faces, np.int64)
    v2, f2, _, quitadas = animales.limpiar(v, f, None)
    assert quitadas == 1 and v2[:, 1].max() < 2.0, (quitadas, v2[:, 1].max())
    # El color: un rectangulo rojo sobre fondo verde, que cubre la silueta de lado; nada del verde en el animal.
    img = np.zeros((200, 300, 3), np.uint8)
    img[:] = (90, 220, 120)
    img[40:180, 30:270] = RED
    ruta = os.path.join(tmp, "ref.png")
    Image.fromarray(img).save(ruta)
    col = animales.color_de_imagen(v2, ruta)
    assert np.abs(col - np.array(RED)).max() < 2, col[:3]
    # El esqueleto y los clips (las patas, unidas al cuerpo; las cajas, con el frente a +X, como despues de orientar).
    it = {"id": "animal.salvaje.puma", "tags": ["faccion:salvaje", "rig:cuadrupedo"], "medidas_m": "0.5x1.5x2.1",
          "tris_max": "400", "categoria": "animal"}
    datos, tris = animales.armar((v2, f2, np.full((len(v2), 3), 150.0)), it, log=lambda *a: None)
    assert tris <= 400, tris
    u = animales.armar.ultimo
    e, med = u["e"], u["m"]
    pie = {n: e.pos[e.i(n + "_punta")] for n in animales.PATAS}
    assert pie["di"][0] > 0 and pie["ti"][0] < 0 and pie["di"][2] < 0 and pie["dd"][2] > 0, pie
    assert e.pos[e.i("hocico")][0] > e.pos[e.i("pecho")][0] > e.pos[e.i("cadera")][0] > e.pos[e.i("cola_punta")][0]
    assert 0.6 < med["vientre"] < 1.0, med["vientre"]  # las patas, de 0.8 m en la caja (escaladas)
    n = struct.unpack_from("<I", datos, 12)[0]
    doc = json.loads(datos[20:20 + n])
    clips = {a["name"] for a in doc["animations"]}  # los del puma en assets/animaciones.tsv: caza, no pasta
    assert {"idle", "caminar", "trotar", "galopar", "atacar", "morir", "acechar", "comer_presa"} <= clips, clips
    assert "pastar" not in clips and "montado_idle" not in clips, clips
    assert len(doc["skins"][0]["joints"]) == len(e.nombres), len(doc["skins"][0]["joints"])


def test_animales_fantasma(tmp):
    """animales.en_voxeles: el volumen de una hoja de tres vistas con las patas de atras escalonadas trae dos columnas de
    cada lado (dos fantasma): quedan las de mas volumen; las de adelante, pegadas de un lado al otro, se separan."""
    import animales
    import mcubes
    partes = [((0.0, 1.0, 0.0), (1.2, 0.5, 0.4)), ((0.85, 1.35, 0.0), (0.5, 0.25, 0.22)),
              ((0.45, 0.4, 0.0), (0.12, 0.8, 0.38))]  # el cuerpo, la cabeza y las patas de adelante, pegadas
    for z in (0.13, -0.13):
        partes += [((-0.45, 0.4, z), (0.12, 0.8, 0.12)), ((-0.15, 0.4, z), (0.08, 0.8, 0.08))]  # la pata y la fantasma
    paso = 0.02
    xs, ys, zs = (np.arange(a, b, paso) for a, b in ((-0.8, 1.2), (-0.05, 1.6), (-0.3, 0.3)))
    X, Y, Z = np.meshgrid(xs, ys, zs, indexing="ij")
    lleno = np.zeros(X.shape, bool)
    for (cx, cy, cz), (lx, ly, lz) in partes:
        lleno |= (np.abs(X - cx) <= lx / 2) & (np.abs(Y - cy) <= ly / 2) & (np.abs(Z - cz) <= lz / 2)
    vv, ff = mcubes.marching_cubes(lleno.astype(float), 0.5)
    m = trimesh.Trimesh(vv * paso + [xs[0], ys[0], zs[0]], ff)
    v, f = np.asarray(m.vertices, float), np.asarray(m.faces, np.int64)
    v2, f2, col, fantasmas, separadas = animales.en_voxeles(v, f, np.full((len(f), 3), 150.0))
    assert fantasmas == 2 and separadas > 0 and len(col) == len(v2), (fantasmas, separadas)
    P = animales.muestras(v2, f2)
    sl = P[np.abs(P[:, 1] - 0.2) < 0.02]
    patas = sorted(round(float(sl[:, 0].min() + gx.mean() * 0.02), 1) for gx, _ in animales._componentes(sl, sl.min(0), 0.02))
    assert patas == [-0.5, -0.5, 0.4, 0.4] or patas == [-0.4, -0.4, 0.5, 0.5] or len(patas) == 4 and \
        all(abs(abs(x) - 0.45) < 0.08 for x in patas), patas


def test_capa_de_afuera(tmp):
    """Una cascara con grosor (como rigged-model.glb): una esfera hacia afuera y otra, 4 cm adentro, mirando al hueco.
    Quedan solo las caras de la de afuera; una esfera sola queda entera. Y un agujero chico, tapado con el sentido de
    sus vecinas."""
    import humanoide
    afuera = trimesh.creation.icosphere(subdivisions=3, radius=1.0)
    adentro = trimesh.creation.icosphere(subdivisions=3, radius=0.96)
    v = np.concatenate([afuera.vertices, adentro.vertices])
    f = np.concatenate([afuera.faces, adentro.faces[:, ::-1] + len(afuera.vertices)])
    m = lowres.capa_de_afuera(v, f)
    n = len(afuera.faces)
    assert m[:n].all() and not m[n:].any(), (m[:n].mean(), m[n:].mean())
    assert lowres.capa_de_afuera(afuera.vertices, afuera.faces).all()
    f2 = humanoide.tapar_agujeros(np.asarray(afuera.faces[1:], dtype=np.int64))
    e = np.concatenate([f2[:, [0, 1]], f2[:, [1, 2]], f2[:, [2, 0]]])
    dirigidas = set(map(tuple, e.tolist()))
    assert len(f2) == n and all((b, a) in dirigidas for a, b in e.tolist()), len(f2)


def test_tapar_rendijas(tmp):
    """Un tubo abierto (la cana de una bota) con un agujero chico en el costado: el agujero se tapa con el sentido de
    sus vecinas y las dos bocas, de contorno mas largo que el maximo, quedan abiertas."""
    import patron
    # 9 anillos de 16 vertices, de z = -0,2 a 0,2 m, sin tapas: dos bocas de 0,38 m de contorno
    a = np.linspace(0, 2 * np.pi, 16, endpoint=False)
    v = np.array([(0.06 * np.cos(t), 0.06 * np.sin(t), z) for z in np.linspace(-0.2, 0.2, 9) for t in a])
    f = []
    for r in range(8):
        for s in range(16):
            p, q, w, x = r * 16 + s, r * 16 + (s + 1) % 16, (r + 1) * 16 + (s + 1) % 16, (r + 1) * 16 + s
            f += [(p, q, w), (p, w, x)]
    f = np.array(f, np.int64)
    f = np.delete(f, [4 * 32 + 6, 4 * 32 + 7], axis=0)  # un agujero en el costado (un cuadrado: 0,15 m de contorno)
    v2, f2 = patron.tapar_rendijas(v, f, 0.3)
    e = np.concatenate([f2[:, [0, 1]], f2[:, [1, 2]], f2[:, [2, 0]]])
    dirigidas = set(map(tuple, e.tolist()))
    borde = [(p, q) for p, q in e.tolist() if (q, p) not in dirigidas]
    assert len(f2) == len(f) + 4 and len(v2) == len(v) + 1, (len(f2), len(f), len(v2))  # el abanico desde su centro
    assert len(borde) == 32 and all(abs(abs(v2[p, 2]) - 0.2) < 1e-9 for p, _ in borde), len(borde)  # solo las bocas
    repetidas = len(e) - len(dirigidas)  # una arista dos veces en el mismo sentido: una cara al reves
    assert repetidas == 0, repetidas
    # Una raja: una columna de cuatro cuadrados del costado del tubo entero (0,45 m de contorno, 2 cm de ancho medio).
    # Mas larga que el maximo: abierta, salvo que sea mas angosta que angosto (y entonces, un abanico de 10 caras).
    entero = []
    for r in range(8):
        for s in range(16):
            p, q, w, x = r * 16 + s, r * 16 + (s + 1) % 16, (r + 1) * 16 + (s + 1) % 16, (r + 1) * 16 + s
            entero += [(p, q, w), (p, w, x)]
    raja = np.delete(np.array(entero, np.int64), [r * 32 + 2 * 10 + k for r in range(1, 5) for k in (0, 1)], axis=0)
    assert len(patron.tapar_rendijas(v, raja, 0.3)[1]) == len(raja)
    assert len(patron.tapar_rendijas(v, raja, 0.3, angosto=0.03)[1]) == len(raja) + 10


def test_sanear(tmp):
    """Un tubo con una aleta (una cara chica de mas sobre una arista: tres caras en ella) y una cara dada vuelta:
    saneado, ninguna arista con mas de dos caras y todas las caras con el sentido de sus vecinas."""
    import patron
    a = np.linspace(0, 2 * np.pi, 12, endpoint=False)
    v = np.array([(0.06 * np.cos(t), 0.06 * np.sin(t), z) for z in np.linspace(0, 0.3, 4) for t in a])
    f = []
    for r in range(3):
        for s in range(12):
            p, q, w, x = r * 12 + s, r * 12 + (s + 1) % 12, (r + 1) * 12 + (s + 1) % 12, (r + 1) * 12 + s
            f += [(p, q, w), (p, w, x)]
    f = np.array(f, np.int64)
    n = len(f)
    v = np.concatenate([v, [[0.0, 0.0, 0.1]]])  # la aleta: hacia adentro, sobre la arista (0, 13)
    f = np.concatenate([f, [[0, 13, len(v) - 1]]])
    f[5] = f[5][[0, 2, 1]]  # una cara al reves
    v2, f2 = patron.sanear(v, f)
    e = np.concatenate([f2[:, [0, 1]], f2[:, [1, 2]], f2[:, [2, 0]]])
    _, cuantas = np.unique(np.sort(e, axis=1), axis=0, return_counts=True)
    assert len(f2) == n and cuantas.max() <= 2, (len(f2), cuantas.max())
    assert len(set(map(tuple, e.tolist()))) == len(e), "quedo una cara al reves"


def _casquete(radio, adentro=False, sub=3):
    """La mitad de arriba (y > 0) de una esfera: las normales afuera, o adentro (la cara de adentro de un casco)."""
    s = trimesh.creation.icosphere(subdivisions=sub, radius=radio)
    v, f = np.asarray(s.vertices, float), np.asarray(s.faces, np.int64)
    f = f[v[f][:, :, 1].mean(1) > 0]
    return v, (f[:, ::-1] if adentro else f)


def _juntar(*mallas):
    vs, fs, n = [], [], 0
    for v, f in mallas:
        vs.append(v)
        fs.append(f + n)
        n += len(v)
    return np.concatenate(vs), np.concatenate(fs)


def test_girar_casco(tmp):
    """Un casco simetrico de oreja a oreja (una caja con una visera adelante) girado 12 grados de mas: girar_casco
    encuentra la correccion y lo deja de frente a +X."""
    import patron
    caja = trimesh.creation.box(extents=(0.22, 0.2, 0.16))
    visera = trimesh.creation.box(extents=(0.04, 0.05, 0.04))
    visera.apply_translation((0.13, 0.0, 0.0))
    B, f = _juntar((np.asarray(caja.vertices, float), np.asarray(caja.faces)), (np.asarray(visera.vertices, float), np.asarray(visera.faces)))
    o = B @ patron.rot_y(90 + 12)  # lo que hace falta para que rot_y(90 + az + g) lo traiga, con az = 0 y g = 12
    R, g = patron.girar_casco(o, f, 0)
    assert abs(g - 12) < 0.6, g
    assert (o @ R.T)[len(caja.vertices):, 0].mean() > 0.1  # la visera, adelante (+X)


def test_cabeza_signo(tmp):
    """La distancia con signo a la piel de la cabeza (una esfera): negativa adentro y positiva afuera, con su modulo, y
    el mismo signo con caras dadas vuelta (los pliegues de las orejas miran adentro: el signo no sale de las normales)."""
    import patron
    s = trimesh.creation.icosphere(subdivisions=3, radius=0.1)
    c = np.array([0.0, 1.6, 0.0])
    v, f = np.asarray(s.vertices, float) + c, np.asarray(s.faces, np.int64)
    q = c + np.array([[0.05, 0, 0], [0.15, 0, 0], [0, 0.07, 0], [0, -0.13, 0.0]])
    for caras in (f, np.concatenate([f[:100, ::-1], f[100:]])):
        d, _ = patron.Cabeza(v, caras, c).distancia(q)
        assert np.allclose(d, [-0.05, 0.05, -0.03, 0.03], atol=0.004), d


def test_abertura(tmp):
    """El tope de la abertura de la cara: una cupula con una ventana adelante hasta 5 cm de alto (la nasal y las
    carrilleras no cuentan: abajo de la ventana no hay casco)."""
    import patron
    v, f = _casquete(0.12, sub=4)
    cen = v[f].mean(1)
    f = f[~((cen[:, 0] > 0) & (cen[:, 1] < 0.05) & (np.abs(cen[:, 2]) < 0.07))]
    b = patron.abertura(v, f, 0.0, 0.0, 0.03)
    assert b is not None and abs(b - 0.05) < 0.012, b
    cerrada = patron.abertura(*_casquete(0.12), 0.0, 0.0, 0.03)  # sin ventana: a lo sumo, el borde de abajo
    assert cerrada is None or cerrada < 0.01, cerrada


def test_forro(tmp):
    """Un casco hueco con la boca cerrada por un forro (una capa de 3 mm) echado 20 grados atras: forro_de lo encuentra
    y pendiente_forro mide cuanto sube la boca hacia adelante; sin el forro, ninguno de los dos."""
    import patron
    t = np.linspace(0, 2 * np.pi, 24, endpoint=False)
    anillo = np.column_stack([0.075 * np.cos(t), np.zeros(24), 0.075 * np.sin(t)])

    def disco(y, arriba):
        p = np.concatenate([[[0.0, 0, 0]], anillo]) @ patron.rot_z(20).T + [0, y, 0]
        fan = np.array([(0, 1 + (i + 1) % 24, 1 + i) if arriba else (0, 1 + i, 1 + (i + 1) % 24) for i in range(24)])
        return p, fan
    hueco = _juntar(_casquete(0.12), _casquete(0.1, adentro=True))
    v, f = _juntar(hueco, disco(0.02, False), disco(0.023, True))
    assert abs(patron.pendiente_forro(v, f) - 20) < 1.0, patron.pendiente_forro(v, f)
    en_forro = patron.forro_de(v, f)
    assert en_forro is not None and en_forro(np.array([[0.0, 0.03, 0.0]]))[0] and not en_forro(np.array([[0.0, 0.11, 0.0]]))[0]
    assert patron.pendiente_forro(*hueco) is None and patron.forro_de(*hueco) is None


def test_penetran(tmp):
    """Los vertices del craneo (arriba de la barbilla) a menos de la holgura de un casco (una cupula alrededor de una
    cabeza esferica de 10 cm): a 1 mm, todos los de arriba; a 5 mm, ninguno."""
    import patron
    s = trimesh.creation.icosphere(subdivisions=3, radius=0.1)
    c = np.array([0.0, 1.6, 0.0])
    cr = dict(centro=c, v=np.asarray(s.vertices, float) + c, f=np.asarray(s.faces, np.int64), barbilla=c[1] + 0.01)
    arriba = int((cr["v"][:, 1] > cr["barbilla"]).sum())
    assert patron.penetran(*_casquete(0.101, adentro=True, sub=4), cr) == arriba
    assert patron.penetran(*_casquete(0.105, adentro=True, sub=4), cr) == 0


def _cabeza(radios=(0.1, 0.1, 0.1), sub=3):
    """Una cabeza de prueba (un elipsoide en 1,6 m) con las medidas que usa el calce de los cascos (patron.craneo)."""
    import patron
    s = trimesh.creation.icosphere(subdivisions=sub, radius=1.0)
    c = np.array([0.0, 1.6, 0.0])
    v, f = np.asarray(s.vertices, float) * radios + c, np.asarray(s.faces, np.int64)
    return dict(id="prueba", centro=c, v=v, f=f, piel=patron.Cabeza(v, f, c), barbilla=c[1] - 0.6 * radios[1],
                cejas=c[1], alto=c[1] + radios[1], borde=c[1] + patron.BORDE_CEJAS, fondo=2 * radios[0], ancho=2 * radios[2])


def _cascara(r_afuera, r_adentro, sub=4):
    """Un casco de prueba: la mitad de arriba de una esfera hueca (la cara de afuera y la de adentro)."""
    return _juntar(_casquete(r_afuera, sub=sub), _casquete(r_adentro, adentro=True, sub=sub))


def test_asoman(tmp):
    """La piel de una cabeza esferica de 10 cm contra un casco hueco de 7 mm de grosor: dentro del hueco, no asoma; metida
    en el grosor (entre la cara de adentro y la de afuera), tampoco: no se ve; pasando la cara de afuera, si."""
    import patron
    cr = _cabeza()
    for (afuera, adentro), asoma in (((0.112, 0.105), False), ((0.102, 0.095), False), ((0.097, 0.09), True)):
        v, f = _cascara(afuera, adentro)
        n = patron.asoman(v + cr["centro"], f, cr)
        assert (n > 0) == asoma, (afuera, adentro, n)


def test_contacto(tmp):
    """Lo que la piel entra en el casco: con la cara de adentro a 9,5 cm de una cabeza de 10, 5 mm; afuera, nada."""
    import patron
    cr = _cabeza(sub=4)
    v, f = _cascara(0.102, 0.095)
    assert abs(patron.contacto(v + cr["centro"], f, cr) + 0.005) < 0.0012, patron.contacto(v + cr["centro"], f, cr)
    v, f = _cascara(0.112, 0.105)
    assert patron.contacto(v + cr["centro"], f, cr) > 0.003


def test_regla_casco(tmp):
    """La regla de los cascos colocados por el usuario con un casco redondo (una esfera hueca de 7 % de grosor, abierta
    abajo, con la ventana de la cara adelante, a +Z como en Props) en una cabeza ovalada (19 cm de fondo, 15 de ancho):
    el fondo se estira (la cabeza es mas honda que ancha), queda apoyado (la piel entra lo que deja CONTACTO_CASCO), la
    cabeza no asoma y el borde de la ventana va BORDE_CEJAS sobre las cejas."""
    import patron
    cr = _cabeza((0.095, 0.09, 0.075), sub=4)
    s = trimesh.creation.icosphere(subdivisions=4, radius=1.0)
    v1, f1 = np.asarray(s.vertices, float), np.asarray(s.faces, np.int64)
    v, f = _juntar((v1 * 0.5, f1), (v1 * 0.465, f1[:, ::-1]))  # en otras unidades, como sale de la reconstruccion
    cen = v[f].mean(1) / 0.5
    ventana = (cen[:, 2] > 0) & (cen[:, 1] < 0.1) & (np.abs(cen[:, 0]) < 0.45)
    o, f = v, f[(cen[:, 1] > -0.45) & ~ventana]
    M, info = patron.calzar_casco(o, f, {"id": "prueba", "az": 0}, cr)
    w = patron.aplicar(M, o)
    assert info["estirado"] > 1.1, info["estirado"]
    d = patron.contacto(w, f, cr)
    assert -patron.CONTACTO_CASCO - 0.0005 <= d <= -patron.CONTACTO_CASCO + 0.0025, d
    assert patron.asoman(w, f, cr) == 0
    b = patron.abertura(w, f, cr["centro"][0], cr["centro"][2], patron.OJOS)
    assert b is not None and abs(b - cr["borde"]) < 0.004, (b, cr["borde"])


def test_glb_mallas(tmp):
    """Los cascos colocados se guardan como mallas sueltas con su nombre y se leen igual."""
    import patron
    a, b = _casquete(0.1), _casquete(0.2, sub=2)
    ruta = os.path.join(tmp, "colocados.glb")
    open(ruta, "wb").write(patron.glb_mallas([("ref_02:casco", *a), ("ref_03:otro", *b)], "prueba"))
    leidas = patron.mallas(ruta)
    assert [n for n, _, _ in leidas] == ["ref_02:casco", "ref_03:otro"]
    for (_, v, f), (va, fa) in zip(leidas, (a, b)):
        assert np.allclose(v, va, atol=1e-6) and np.array_equal(f, fa)


def _cuadrupedo(patas, paso=0.04):
    """Un cuadrupedo de cajas en una sola malla cerrada (como las de Hunyuan3D), de frente a +X: el cuerpo, la cabeza y
    la cola, con las patas (x, z) dadas."""
    import mcubes
    partes = [((0.0, 1.0, 0.0), (1.2, 0.5, 0.4)), ((0.85, 1.35, 0.0), (0.5, 0.25, 0.22)), ((-0.8, 1.05, 0.0), (0.45, 0.08, 0.08))]
    partes += [((x, 0.4, z), (0.12, 0.8, 0.12)) for x, z in patas]
    xs, ys, zs = (np.arange(a, b, paso) for a, b in ((-1.2, 1.3), (-0.1, 1.8), (-0.4, 0.4)))
    X, Y, Z = np.meshgrid(xs, ys, zs, indexing="ij")
    lleno = np.zeros(X.shape, bool)
    for (cx, cy, cz), (lx, ly, lz) in partes:
        lleno |= (np.abs(X - cx) <= lx / 2) & (np.abs(Y - cy) <= ly / 2) & (np.abs(Z - cz) <= lz / 2)
    vv, ff = mcubes.marching_cubes(lleno.astype(float), 0.5)
    m = trimesh.Trimesh(vv * paso + [xs[0], ys[0], zs[0]], ff)
    v = np.asarray(m.vertices, float)
    return v - [0.0, v[:, 1].min(), 0.0], np.asarray(m.faces, np.int64)


def test_medir_paso(tmp):
    """animales.medir con las patas en un paso (las de adelante y las de atras escalonadas, como el burro de referencia de
    Better animals): las dos de mas adelante son las delanteras, y de cada par, la de -Z es la izquierda."""
    import animales
    v, f = _cuadrupedo([(0.55, -0.13), (0.22, 0.13), (-0.25, -0.13), (-0.55, 0.13)])
    m = animales.medir(animales.muestras(v, f))
    pie = {n: m["ejes"][n][0] for n in animales.PATAS}
    assert pie["di"][0] > 0.4 and pie["dd"][0] > 0.1 and pie["ti"][0] < -0.1 and pie["td"][0] < -0.4, pie
    assert pie["di"][2] < 0 < pie["dd"][2] and pie["ti"][2] < 0 < pie["td"][2], pie


def test_rigs_cuaterniones(tmp):
    """rigs.py: el giro de un eje (q_eje), el de vuelta y adelante entre cuaternion y matriz, y nlerp en sus extremos."""
    import rigs
    q = rigs.q_eje(np.array([[0.0, 0.0, 1.0]]), np.array([90.0]))[0]
    assert np.allclose(rigs.q_a_m(q) @ [1.0, 0.0, 0.0], [0.0, 1.0, 0.0], atol=1e-9)  # +X a +Y, alrededor de +Z
    r = rigs.qmul(q, rigs.q_eje(np.array([[1.0, 0.0, 0.0]]), np.array([35.0]))[0])
    assert np.allclose(np.abs(rigs.m_a_q(rigs.q_a_m(r)) @ r), 1.0, atol=1e-9)
    a, b = np.array([[0.0, 0.0, 0.0, 1.0]]), q[None]
    assert np.allclose(rigs.nlerp(a, b, 0.0), a) and np.allclose(rigs.nlerp(a, b, 1.0), b)


def _rig_de_prueba():
    """Un rig de caballo minimo (rigs.Rig): la cadera, el tronco, la base del cuello con el omoplato, el cuello y la
    cabeza; un clip de dos cuadros con la base del cuello girada 30 grados."""
    import rigs
    r = rigs.Rig()
    r.nombres = ["root", "hips", "spine_3", "spine_4", "front_scapula_l", "spine_5", "spine_6", "head", "nose", "mouth_tip"]
    r.padres = np.array([-1, 0, 1, 2, 3, 3, 5, 6, 7, 8])
    pos = [(0, 0, 0), (-0.5, 1.0, 0), (0.3, 1.0, 0), (0.4, 1.1, 0), (0.35, 1.0, -0.1), (0.6, 1.3, 0), (0.8, 1.45, 0),
           (1.0, 1.5, 0), (1.3, 1.3, 0), (1.35, 1.25, 0)]
    r.bind = np.tile(np.eye(4), (len(pos), 1, 1))
    r.bind[:, :3, 3] = pos
    r.cadera = 1
    L = rigs.local_de(r.bind, r.padres)
    R = np.tile([0.0, 0.0, 0.0, 1.0], (2, len(pos), 1))
    R[1, 3] = rigs.q_eje(np.array([[0.0, 0.0, 1.0]]), np.array([-30.0]))[0]
    r.clips = {"Idle": {"t": np.array([0.0, 0.5]), "R": R, "T": np.tile(L[1][:3, 3], (2, 1))}}
    return r


def test_rigs_hombros_y_asiento(tmp):
    """rigs.hombros_al_tronco: el omoplato pasa a colgar del tronco con la misma orientacion en el mundo en cada cuadro;
    con_asiento: el hueso asiento, hijo del hueso del tronco mas cercano, quieto en su padre y donde se pidio."""
    import rigs
    r = _rig_de_prueba()
    c = r.clips["Idle"]
    W0 = rigs.fk(rigs.local_de(r.bind, r.padres), c["R"], c["T"], r.padres, r.cadera)
    rigs.hombros_al_tronco(r)
    k = r.nombres.index("front_scapula_l")
    assert r.nombres[r.padres[k]] == "spine_3", r.padres
    W1 = rigs.fk(rigs.local_de(r.bind, r.padres), c["R"], c["T"], r.padres, r.cadera)
    assert np.allclose(W0[:, k, :3, :3], W1[:, k, :3, :3], atol=1e-9)
    clips = {"idle": (c["t"], c["R"], c["T"])}
    B, nombres, padres, clips = rigs.con_asiento(r.bind, list(r.nombres), r.padres, clips, np.array([0.25, 1.4, 0.0]))
    j = nombres.index("asiento")
    assert nombres[padres[j]] == "spine_3" and clips["idle"][1].shape[1] == len(nombres), (nombres[padres[j]], clips["idle"][1].shape)
    W = rigs.fk(rigs.local_de(B, padres), clips["idle"][1], clips["idle"][2], padres, r.cadera)
    assert np.allclose(W[:, j, :3, 3], [0.25, 1.4, 0.0], atol=1e-9)


def test_rigs_cabeza_al_suelo(tmp):
    """rigs.cabeza_al_suelo: el cuello baja (giro negativo, hasta su tope) y la boca queda mucho mas abajo, sin mover
    el omoplato (colgado del tronco) ni la cadera."""
    import rigs
    r = rigs.hombros_al_tronco(_rig_de_prueba())
    c = r.clips["Idle"]
    R = np.tile(c["R"][:1], (2, 1, 1))  # quieto: el reposo
    clip, g = rigs.cabeza_al_suelo((c["t"], R, c["T"]), r.bind, r.nombres, r.padres, r.cadera)
    # El tope: el cuello (de su base a la nuca, que en reposo sube 33.7 grados) hasta 70 bajo la horizontal.
    assert -103.7 - 1e-6 <= g < -20.0, g
    L = rigs.local_de(r.bind, r.padres)
    antes, despues = rigs.fk(L, R, c["T"], r.padres, r.cadera), rigs.fk(L, clip[1], clip[2], r.padres, r.cadera)
    boca, omoplato = r.nombres.index("mouth_tip"), r.nombres.index("front_scapula_l")
    assert despues[:, boca, 1, 3].max() < 0.6 * antes[:, boca, 1, 3].min(), (antes[:, boca, 1, 3], despues[:, boca, 1, 3])
    assert np.allclose(antes[:, omoplato], despues[:, omoplato], atol=1e-9)


def test_rigs_suavizar_pesos(tmp):
    """rigs.suavizar_pesos: en una tira de triangulos que pasa de golpe de un hueso al otro, el paso se reparte en
    varios vertices, y los pesos de cada uno siguen sumando 1."""
    import rigs
    n = 20
    v = np.array([[i * 0.1, y, 0.0] for i in range(n) for y in (0.0, 0.1)])
    f = np.array([[2 * i, 2 * i + 2, 2 * i + 1] for i in range(n - 1)] + [[2 * i + 1, 2 * i + 2, 2 * i + 3] for i in range(n - 1)])
    juntas = np.zeros((2 * n, 4), np.int64)
    juntas[2 * (n // 2):, 0] = 1
    pesos = np.zeros((2 * n, 4))
    pesos[:, 0] = 1.0
    j2, p2 = rigs.suavizar_pesos(juntas, pesos, f, 2)
    assert np.allclose(p2.sum(axis=1), 1.0)
    w1 = np.array([p2[i][j2[i] == 1].sum() for i in range(2 * n)])
    mezclados = ((w1 > 0.05) & (w1 < 0.95)).sum()
    assert mezclados >= 4 and w1[0] < 0.05 and w1[-1] > 0.95, w1


def test_jinete(tmp):
    """jinete.py (con la referencia del usuario guardada en arte/fuentes/patron/jinete.glb): el asiento de la silla
    entre las patas y sobre el lomo, como en la referencia; los estribos bajo la pelvis y a los costados; la silla, con
    su origen en la base y el asiento adentro."""
    import inventario
    import jinete
    med = jinete.medido()
    assert 0.55 < med["parte"] < 0.75 and 0.08 < med["sobre_lomo"] < 0.25, (med["parte"], med["sobre_lomo"])
    assert 0.45 < med["estribo_abajo"] < 0.7 and 0.2 < med["estribo_lado"] < 0.35, (med["estribo_abajo"], med["estribo_lado"])
    v, f = _cuadrupedo([(0.45, -0.13), (0.45, 0.13), (-0.45, -0.13), (-0.45, 0.13)])
    asiento = jinete.asiento_de(v, f)
    assert 0.05 < asiento[0] < 0.4 and asiento[1] > 1.25 + med["sobre_lomo"], asiento  # el lomo de las cajas, a 1.25 m
    adelante, arriba, costado = jinete.estribos_del_jinete()
    assert arriba < -0.5 and abs(adelante) < 0.3 and 0.2 < costado < 0.35, (adelante, arriba, costado)
    items, _ = inventario.load()
    it = {i["id"]: i for i in items}[jinete.SILLA]
    datos, en_silla, caja, n = jinete.silla(it)
    assert n <= int(it["tris_max"]), n
    largo = float(caja.split("x")[2])
    assert 0.0 < en_silla[1] < float(caja.split("x")[1]) and abs(en_silla[0]) < 0.5 * largo, (en_silla, caja)


def main():
    tests = [test_convertir, test_rotar_y_escala, test_check_model, test_cline, test_traer, test_suave, test_refs_anigen,
             test_refs_anigen_registro, test_adaptar_oga, test_animales, test_animales_fantasma, test_capa_de_afuera,
             test_tapar_rendijas, test_sanear, test_girar_casco, test_cabeza_signo, test_abertura, test_forro, test_penetran,
             test_asoman, test_contacto, test_regla_casco, test_glb_mallas, test_medir_paso, test_rigs_cuaterniones,
             test_rigs_hombros_y_asiento, test_rigs_cabeza_al_suelo, test_rigs_suavizar_pesos, test_jinete]
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
