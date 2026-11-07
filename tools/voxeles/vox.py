"""El formato .vox de MagicaVoxel (el editor gratis), un solo modelo: leer y escribir.

La convencion del juego (src/sim/voxfile.h): z hacia arriba y el frente del objeto mirando a -y, como en la
vista de frente de MagicaVoxel; el juego lo gira para que mire a +X, como los GLB del inventario. Una rejilla
es un arreglo de numpy de forma (sx, sy, sz) con el indice de color de cada celda (0: vacia; 1..255: el color
paleta[indice - 1]). La paleta: 256 colores RGBA.
"""
import struct

import numpy as np

MAX = 256  # celdas por eje que admite el formato (el juego lee hasta VOXFILE_MAX)


def _chunk(cid, content, children=b""):
    return cid + struct.pack("<ii", len(content), len(children)) + content + children


def escribir(path, grid, palette):
    """grid: (sx, sy, sz) uint8; palette: (n, 3 o 4) uint8 con n <= 255 (el color i va en el indice i + 1)."""
    grid = np.asarray(grid, dtype=np.uint8)
    sx, sy, sz = grid.shape
    if max(sx, sy, sz) > MAX or min(sx, sy, sz) < 1:
        raise ValueError(f"la rejilla {grid.shape} no entra en un .vox (1 a {MAX} por eje)")
    pal = np.zeros((256, 4), dtype=np.uint8)
    pal[:, 3] = 255
    p = np.asarray(palette, dtype=np.uint8)
    if len(p) > 255:
        raise ValueError("la paleta tiene mas de 255 colores")
    pal[: len(p), : p.shape[1]] = p
    xs, ys, zs = np.nonzero(grid)
    xyzi = np.stack([xs, ys, zs, grid[xs, ys, zs]], axis=1).astype(np.uint8)
    size = _chunk(b"SIZE", struct.pack("<iii", sx, sy, sz))
    voxels = _chunk(b"XYZI", struct.pack("<i", len(xyzi)) + xyzi.tobytes())
    rgba = _chunk(b"RGBA", pal.tobytes())
    data = b"VOX " + struct.pack("<i", 150) + _chunk(b"MAIN", b"", size + voxels + rgba)
    with open(path, "wb") as f:
        f.write(data)
    return len(xyzi)


def leer(path):
    """(grid (sx, sy, sz) uint8, palette (256, 4) uint8) del primer modelo del archivo."""
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < 20 or data[:4] != b"VOX ":
        raise ValueError(f"{path}: no es un .vox")
    pos, size, grid, pal = 8, None, None, None
    end = len(data)
    if data[pos : pos + 4] == b"MAIN":
        pos += 12  # el MAIN: sus hijos siguen enseguida
    while pos + 12 <= end:
        cid = data[pos : pos + 4]
        n, m = struct.unpack_from("<ii", data, pos + 4)
        body = data[pos + 12 : pos + 12 + n]
        if cid == b"SIZE" and size is None:
            size = struct.unpack_from("<iii", body)
        elif cid == b"XYZI" and grid is None and size is not None:
            count = struct.unpack_from("<i", body)[0]
            v = np.frombuffer(body[4 : 4 + 4 * count], dtype=np.uint8).reshape(-1, 4)
            grid = np.zeros(size, dtype=np.uint8)
            ok = (v[:, 0] < size[0]) & (v[:, 1] < size[1]) & (v[:, 2] < size[2])
            v = v[ok]
            grid[v[:, 0], v[:, 1], v[:, 2]] = v[:, 3]
        elif cid == b"RGBA" and pal is None:
            pal = np.frombuffer(body[:1024], dtype=np.uint8).reshape(256, 4).copy()
        pos += 12 + n + m
    if grid is None:
        raise ValueError(f"{path}: sin modelo (SIZE y XYZI)")
    if pal is None:  # sin paleta: grises (el juego hace lo mismo)
        g = np.linspace(40, 230, 256).astype(np.uint8)
        pal = np.stack([g, g, g, np.full(256, 255, np.uint8)], axis=1)
    return grid, pal
