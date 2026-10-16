#!/usr/bin/env python3
"""
Pipeline Asset Downloader for SCAVENGERS THRIVE (Estepa)

Downloads GLB assets directly from HuggingFace (TRELLIS 2, RigNet) or GRAIL
into assets/models/ and updates assets/data/models_manifest.json.

Usage:
    python tools/fetch_pipeline_asset.py --url <URL> --name <NAME> --category <CAT> [--animated]

Options:
    --url        Direct download link to the GLB file
    --name       Identifier for the asset (used as filename)
    --category   Asset category: character, creature, prop, yurt, etc.
    --animated   Boolean flag: mark asset as having rig/animation
"""
import argparse
import json
import os
import sys
import urllib.request
from datetime import datetime, timezone


def parse_args():
    parser = argparse.ArgumentParser(
        description="Download pipeline assets from HuggingFace/GRAIL into assets/models/"
    )
    parser.add_argument("--url", required=True, help="Direct download link to GLB file")
    parser.add_argument("--name", required=True, help="Asset identifier (used as filename)")
    parser.add_argument(
        "--category",
        required=True,
        choices=["character", "creature", "prop", "yurt", "environment", "other"],
        help="Asset category",
    )
    parser.add_argument(
        "--animated",
        action="store_true",
        help="Mark asset as having rig/animation tracks",
    )
    return parser.parse_args()


def download_file(url: str, dest_path: str) -> int:
    """Download a file from URL with browser User-Agent header. Returns file size in bytes."""
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
            "(KHTML, like Gecko) Chrome/120.0 Safari/537.36",
            "Accept": "application/octet-stream, model/gltf-binary, */*",
        },
    )
    with urllib.request.urlopen(req) as response:
        total_size = int(response.headers.get("Content-Length", 0))
        downloaded = 0
        chunk_size = 8192
        with open(dest_path, "wb") as f:
            while True:
                chunk = response.read(chunk_size)
                if not chunk:
                    break
                f.write(chunk)
                downloaded += len(chunk)
    return downloaded


def update_manifest(manifest_path: str, name: str, filepath: str, category: str, has_rig: bool):
    """Update models_manifest.json with asset metadata."""
    manifest = {}
    if os.path.exists(manifest_path):
        try:
            with open(manifest_path, "r", encoding="utf-8") as f:
                manifest = json.load(f)
        except json.JSONDecodeError:
            manifest = {}

    if "models" not in manifest:
        manifest["models"] = {}

    manifest["models"][name] = {
        "file": filepath,
        "category": category,
        "has_rig": has_rig,
        "updated_at": datetime.now(timezone.utc).isoformat(),
    }

    os.makedirs(os.path.dirname(manifest_path), exist_ok=True)
    with open(manifest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2, ensure_ascii=False)


def main():
    args = parse_args()

    # Determine project root (tools/ is under repo root)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(script_dir)

    models_dir = os.path.join(repo_root, "assets", "models")
    manifest_path = os.path.join(repo_root, "assets", "data", "models_manifest.json")

    os.makedirs(models_dir, exist_ok=True)
    os.makedirs(os.path.dirname(manifest_path), exist_ok=True)

    dest_path = os.path.join(models_dir, f"{args.name}.glb")

    print(f"[Pipeline] Downloading {args.name} ({args.category})")
    print(f"[Pipeline] URL: {args.url}")
    print(f"[Pipeline] Destination: {dest_path}")
    print(f"[Pipeline] Animated: {args.animated}")

    try:
        size_bytes = download_file(args.url, dest_path)
    except Exception as e:
        print(f"[Pipeline] ERROR: Download failed: {e}", file=sys.stderr)
        sys.exit(1)

    size_mb = size_bytes / (1024.0 * 1024.0)
    print(f"[Pipeline] Download complete: {size_mb:.2f} MB ({size_bytes} bytes)")

    # Verify file integrity
    if not os.path.exists(dest_path):
        print("[Pipeline] ERROR: File not found after download", file=sys.stderr)
        sys.exit(1)

    actual_size = os.path.getsize(dest_path)
    if actual_size != size_bytes:
        print(
            f"[Pipeline] WARNING: Size mismatch ({actual_size} vs {size_bytes})",
            file=sys.stderr,
        )

    # Update manifest
    rel_path = os.path.relpath(dest_path, repo_root)
    update_manifest(manifest_path, args.name, rel_path, args.category, args.animated)

    print(f"[Pipeline] Manifest updated: {manifest_path}")
    print(f"[Pipeline] Asset '{args.name}' ready for engine use")


if __name__ == "__main__":
    main()
