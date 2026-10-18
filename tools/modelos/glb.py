"""Lector de GLB (glTF 2.0 binario) sin dependencias: lo que el chequeo de los modelos necesita.

Glb(ruta) lee el JSON y el bloque binario, y da:
  - triangulos(): cuantos triangulos dibuja (las primitivas de triangulos, tiras y abanicos);
  - limites(): la caja (min, max) en metros, con las transformaciones de los nodos;
  - imagenes(): (nombre, ancho, alto) de cada imagen embebida (PNG o JPEG);
  - animaciones(), pieles(), atributos(): los clips, si esta riggeado y que atributos traen las mallas.
Solo la biblioteca estandar de Python 3: corre en el CI sin instalar nada.
"""
import json
import math
import struct

MODE_TRIANGLES, MODE_STRIP, MODE_FAN = 4, 5, 6


class GlbError(Exception):
    pass


def _matmul(a, b):
    return [[sum(a[i][k] * b[k][j] for k in range(4)) for j in range(4)] for i in range(4)]


def _node_matrix(node):
    """La matriz local de un nodo (4x4, filas), de matrix o de traslacion, rotacion y escala."""
    if "matrix" in node:
        m = node["matrix"]  # glTF: por columnas
        return [[m[c * 4 + r] for c in range(4)] for r in range(4)]
    tx, ty, tz = node.get("translation", [0.0, 0.0, 0.0])
    qx, qy, qz, qw = node.get("rotation", [0.0, 0.0, 0.0, 1.0])
    sx, sy, sz = node.get("scale", [1.0, 1.0, 1.0])
    r = [
        [1 - 2 * (qy * qy + qz * qz), 2 * (qx * qy - qz * qw), 2 * (qx * qz + qy * qw)],
        [2 * (qx * qy + qz * qw), 1 - 2 * (qx * qx + qz * qz), 2 * (qy * qz - qx * qw)],
        [2 * (qx * qz - qy * qw), 2 * (qy * qz + qx * qw), 1 - 2 * (qx * qx + qy * qy)],
    ]
    return [
        [r[0][0] * sx, r[0][1] * sy, r[0][2] * sz, tx],
        [r[1][0] * sx, r[1][1] * sy, r[1][2] * sz, ty],
        [r[2][0] * sx, r[2][1] * sy, r[2][2] * sz, tz],
        [0.0, 0.0, 0.0, 1.0],
    ]


def _image_size(data):
    """(ancho, alto) de un PNG o un JPEG, o None."""
    if data[:8] == b"\x89PNG\r\n\x1a\n" and len(data) >= 24:
        return struct.unpack(">II", data[16:24])
    if data[:2] == b"\xff\xd8":
        i = 2
        while i + 9 < len(data):
            if data[i] != 0xFF:
                i += 1
                continue
            marker = data[i + 1]
            if marker in (0xC0, 0xC1, 0xC2, 0xC3, 0xC5, 0xC6, 0xC7, 0xC9, 0xCA, 0xCB, 0xCD, 0xCE, 0xCF):
                h, w = struct.unpack(">HH", data[i + 5:i + 9])
                return w, h
            length = struct.unpack(">H", data[i + 2:i + 4])[0]
            i += 2 + length
    return None


class Glb:
    def __init__(self, path):
        with open(path, "rb") as f:
            raw = f.read()
        if len(raw) < 20 or raw[:4] != b"glTF":
            raise GlbError(f"{path}: no es un GLB (glTF binario)")
        version, length = struct.unpack("<II", raw[4:12])
        if version != 2:
            raise GlbError(f"{path}: glTF version {version} (se espera 2)")
        self.json, self.bin, pos = None, b"", 12
        while pos + 8 <= min(length, len(raw)):
            size, kind = struct.unpack("<I4s", raw[pos:pos + 8])
            chunk = raw[pos + 8:pos + 8 + size]
            if kind == b"JSON":
                self.json = json.loads(chunk.decode("utf-8"))
            elif kind == b"BIN\x00":
                self.bin = chunk
            pos += 8 + size
        if self.json is None:
            raise GlbError(f"{path}: falta el bloque JSON")
        self.path = path
        self.size = len(raw)

    # ------------------------------------------------------------ mallas
    def _mesh_nodes(self):
        """(nodo, matriz del mundo) de cada nodo con malla, recorriendo la escena."""
        nodes = self.json.get("nodes", [])
        scenes = self.json.get("scenes", [])
        if scenes:
            roots = scenes[self.json.get("scene", 0)].get("nodes", [])
        else:  # sin escena: los nodos que no son hijos de nadie
            children = {c for n in nodes for c in n.get("children", [])}
            roots = [i for i in range(len(nodes)) if i not in children]
        ident = [[1.0 if r == c else 0.0 for c in range(4)] for r in range(4)]
        out, stack = [], [(i, ident) for i in roots]
        while stack:
            i, parent = stack.pop()
            node = nodes[i]
            world = _matmul(parent, _node_matrix(node))
            if "mesh" in node:
                out.append((node, world))
            stack.extend((c, world) for c in node.get("children", []))
        return out

    def triangulos(self):
        total = 0
        meshes = self.json.get("meshes", [])
        for node, _ in self._mesh_nodes():
            for prim in meshes[node["mesh"]].get("primitives", []):
                mode = prim.get("mode", MODE_TRIANGLES)
                if "indices" in prim:
                    n = self.json["accessors"][prim["indices"]]["count"]
                else:
                    n = self.json["accessors"][prim["attributes"]["POSITION"]]["count"]
                if mode == MODE_TRIANGLES:
                    total += n // 3
                elif mode in (MODE_STRIP, MODE_FAN):
                    total += max(0, n - 2)
        return total

    def limites(self):
        """La caja que ocupa ((minx, miny, minz), (maxx, maxy, maxz)), o None si no hay mallas."""
        lo, hi = [math.inf] * 3, [-math.inf] * 3
        meshes = self.json.get("meshes", [])
        for node, world in self._mesh_nodes():
            for prim in meshes[node["mesh"]].get("primitives", []):
                acc = self.json["accessors"][prim["attributes"]["POSITION"]]
                if "min" not in acc or "max" not in acc:
                    continue
                a, b = acc["min"], acc["max"]
                for corner in [(x, y, z) for x in (a[0], b[0]) for y in (a[1], b[1]) for z in (a[2], b[2])]:
                    p = [sum(world[r][c] * (corner[c] if c < 3 else 1.0) for c in range(4)) for r in range(3)]
                    for k in range(3):
                        lo[k], hi[k] = min(lo[k], p[k]), max(hi[k], p[k])
        if lo[0] == math.inf:
            return None
        return tuple(lo), tuple(hi)

    def atributos(self):
        """Los atributos de vertice que traen las primitivas (POSITION, NORMAL, COLOR_0...)."""
        names = set()
        for mesh in self.json.get("meshes", []):
            for prim in mesh.get("primitives", []):
                names.update(prim.get("attributes", {}).keys())
        return names

    # ------------------------------------------------------------ texturas, clips y esqueleto
    def imagenes(self):
        out = []
        for i, img in enumerate(self.json.get("images", [])):
            name = img.get("name", f"imagen {i}")
            if "bufferView" not in img:
                out.append((name, None, None))  # externa (uri): no se puede medir aca
                continue
            view = self.json["bufferViews"][img["bufferView"]]
            start = view.get("byteOffset", 0)
            size = _image_size(self.bin[start:start + view["byteLength"]])
            out.append((name, size[0] if size else None, size[1] if size else None))
        return out

    def animaciones(self):
        return [a.get("name", "") for a in self.json.get("animations", [])]

    def pieles(self):
        return len(self.json.get("skins", []))
