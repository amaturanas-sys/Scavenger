#!/usr/bin/env python3
"""Cliente para Hunyuan3D-2.1 (Tencent) en Hugging Face Spaces.

Genera un modelo 3D (GLB) a partir de una imagen usando la API remota de
https://tencent-hunyuan3d-2-1.hf.space (Gradio 4).

Uso:
    python3 tools/modelos/hunyuan3d.py <imagen.jpg> <id_de_inventario> [--url API_URL]

El script:
  1. Sube la imagen a la API de Hunyuan3D-2.1
  2. Llama a generation_all para generar el modelo 3D
  3. Descarga el GLB resultante a arte/modelos/fuentes/<id>.glb
  4. Devuelve la ruta para que se procese con:
     python3 tools/modelos/modelos.py traer <id> --archivo arte/modelos/fuentes/<id>.glb --fuente trellis2 --girar 90

No instala dependencias nuevas: usa solo urllib.request y json de la stdlib.
"""
import argparse
import json
import os
import sys
import time
import uuid
import urllib.request
import urllib.error

API_URL = "https://tencent-hunyuan3d-2-1.hf.space"
FN_INDEX_GENERATION_ALL = 5

INPUT_IMAGE = 6
INPUT_INFERENCE_STEPS = 33
INPUT_GUIDANCE_SCALE = 37
INPUT_SEED = 31
INPUT_OCTREE_RES = 34
INPUT_REMOVE_BG = 28
INPUT_NUM_CHUNKS = 38
INPUT_RANDOMIZE_SEED = 29

OUTPUT_FILE_1 = 19
OUTPUT_FILE_2 = 20
OUTPUT_JSON = 59


def http_post(url, data=None, headers=None, timeout=120):
    """POST con urllib. Devuelve (status, body_bytes)."""
    if headers is None:
        headers = {}
    if data is not None and not isinstance(data, (bytes, bytearray)):
        data = json.dumps(data).encode("utf-8")
        headers.setdefault("Content-Type", "application/json")
    req = urllib.request.Request(url, data=data, headers=headers, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()


def http_get(url, timeout=120):
    """GET con urllib. Devuelve (status, body_bytes)."""
    req = urllib.request.Request(url, headers={}, method="GET")
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.status, r.read()
    except urllib.error.HTTPError as e:
        return e.code, e.read()


def upload_image(api_url, image_path):
    """Sube una imagen a /gradio_api/upload y devuelve el path remoto."""
    url = f"{api_url}/gradio_api/upload"
    with open(image_path, "rb") as f:
        body = f.read()
    boundary = "----WebKitFormBoundary" + uuid.uuid4().hex
    parts = []
    parts.append(f"--{boundary}".encode())
    parts.append(b'Content-Disposition: form-data; name="files"; filename="' +
                 os.path.basename(image_path).encode() + b'"')
    parts.append(b"Content-Type: application/octet-stream")
    parts.append(b"")
    parts.append(body)
    parts.append(f"--{boundary}--".encode())
    payload = b"\r\n".join(parts)
    headers = {
        "Content-Type": f"multipart/form-data; boundary={boundary}",
    }
    status, resp = http_post(url, data=payload, headers=headers, timeout=60)
    if status != 200:
        raise RuntimeError(f"upload falló ({status}): {resp[:500]}")
    try:
        j = json.loads(resp)
        if isinstance(j, list) and len(j) > 0:
            return j[0]["path"]
        if isinstance(j, dict) and "path" in j:
            return j["path"]
        raise RuntimeError(f"respuesta de upload inesperada: {resp[:500]}")
    except json.JSONDecodeError:
        raise RuntimeError(f"respuesta de upload no es JSON: {resp[:500]}")


def join_queue(api_url, session_hash, image_path_remote, fn_index=FN_INDEX_GENERATION_ALL):
    """Se une a la cola de Gradio y devuelve el event_id."""
    url = f"{api_url}/gradio_api/queue/join"
    data = [
        "",          # 7 state (vacío)
        image_path_remote,  # 6 Image
        None,        # 10 Front (no usada)
        None,        # 11 Back
        None,        # 13 Left
        None,        # 14 Right
        50,          # 33 Inference Steps
        3.0,         # 37 Guidance Scale
        42,          # 31 Seed
        256,         # 34 Octree Resolution
        True,        # 28 Remove Background
        1,           # 38 Number of Chunks
        False,       # 29 Randomize seed
    ]
    payload = {
        "data": data,
        "fn_index": fn_index,
        "session_hash": session_hash,
    }
    status, resp = http_post(url, data=payload, timeout=60)
    if status != 200:
        raise RuntimeError(f"queue/join falló ({status}): {resp[:500]}")
    try:
        j = json.loads(resp)
        return j["id"]
    except (json.JSONDecodeError, KeyError) as e:
        raise RuntimeError(f"respuesta de queue/join inesperada: {resp[:500]} ({e})")


def poll_queue(api_url, session_hash, event_id, timeout=300):
    """Espera resultados en /gradio_api/queue/data. Devuelve los mensajes."""
    url = f"{api_url}/gradio_api/queue/data?session_hash={session_hash}"
    req = urllib.request.Request(url, headers={}, method="GET")
    start = time.time()
    results = []
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            for line in r:
                line = line.decode("utf-8", errors="replace").strip()
                if line.startswith("data:"):
                    try:
                        msg = json.loads(line[5:].strip())
                        results.append(msg)
                    except json.JSONDecodeError:
                        pass
                if time.time() - start > timeout:
                    raise RuntimeError("timeout esperando resultados")
    except urllib.error.HTTPError as e:
        raise RuntimeError(f"queue/data falló ({e.code}): {e.read()[:500]}")
    return results


def extract_file_urls(api_url, results):
    """Extrae las URLs de archivo de los resultados de Gradio."""
    urls = []
    for msg in results:
        if isinstance(msg, dict):
            outputs = msg.get("output", [])
            if isinstance(outputs, list):
                for out in outputs:
                    if isinstance(out, dict) and "path" in out:
                        urls.append(out["path"])
            if "path" in msg:
                urls.append(msg["path"])
    return urls


def download_file(api_url, file_path, dest):
    """Descarga un archivo generado por Gradio."""
    if file_path.startswith("http"):
        url = file_path
    elif file_path.startswith("/"):
        url = f"{api_url}{file_path}"
    else:
        url = f"{api_url}/{file_path}"
    status, resp = http_get(url, timeout=120)
    if status != 200:
        raise RuntimeError(f"descarga falló ({status}): {resp[:500]}")
    os.makedirs(os.path.dirname(dest), exist_ok=True)
    with open(dest, "wb") as f:
        f.write(resp)
    return dest


def generate(api_url, image_path, out_glb):
    """Flujo completo: subir imagen, generar, descargar GLB."""
    session_hash = str(uuid.uuid4())

    print(f"[hunyuan3d] subiendo imagen: {image_path}")
    remote_path = upload_image(api_url, image_path)
    print(f"[hunyuan3d] imagen subida: {remote_path}")

    print(f"[hunyuan3d] uniéndose a la cola (session={session_hash})")
    event_id = join_queue(api_url, session_hash, remote_path)
    print(f"[hunyuan3d] evento creado: {event_id}")

    print(f"[hunyuan3d] esperando resultados...")
    results = poll_queue(api_url, session_hash, event_id)

    file_urls = extract_file_urls(api_url, results)
    if not file_urls:
        print(f"[hunyuan3d] resultados crudos:")
        for msg in results[:10]:
            print(f"  {msg}")
        raise RuntimeError("no se encontró archivo 3D en los resultados")

    print(f"[hunyuan3d] archivos encontrados: {len(file_urls)}")
    download_file(api_url, file_urls[0], out_glb)
    print(f"[hunyuan3d] modelo guardado en: {out_glb}")
    return out_glb


def main(argv):
    ap = argparse.ArgumentParser(
        description="Genera un modelo 3D GLB desde una imagen usando Hunyuan3D-2.1 (Hugging Face Spaces)."
    )
    ap.add_argument("imagen", help="Ruta a la imagen de entrada (JPG/PNG)")
    ap.add_argument("id", help="Id de inventario del animal (p. ej. animal.montura.camello_bactriano)")
    ap.add_argument("--url", default=API_URL, help="URL del espacio de Hugging Face")
    ap.add_argument("--out", default=None, help="Ruta de salida del GLB (default: arte/modelos/fuentes/<id>.glb)")
    a = ap.parse_args(argv[1:])

    if not os.path.isfile(a.imagen):
        print(f"ERROR: {a.imagen}: no existe", file=sys.stderr)
        return 2

    out = a.out or os.path.join(
        os.path.dirname(os.path.abspath(__file__)),
        "..", "..", "arte", "modelos", "fuentes", a.id + ".glb",
    )

    try:
        generate(a.url, a.imagen, out)
    except Exception as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))