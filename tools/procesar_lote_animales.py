#!/usr/bin/env python3
"""
Preprocesador de lotes '2do intento' para SCAVENGERS THRIVE.
- Renombra renders individuales de animales usando IDs de assets/inventario.tsv.
- Recorta hojas de equipamiento (cuadrícula 3x2) en piezas individuales centradas
  y genera/aplica el mapeo contra assets/inventario.tsv.

Las fuentes, en arte/fuentes (arte/fuentes/README.md): los renders de animales en arte/fuentes/animales/imagenes y
las hojas de equipamiento en arte/fuentes/equipo (en JPEG; los renders se copian con su formato, los recortes salen
en PNG).
"""
import argparse
import csv
import re
import shutil
import sys
from pathlib import Path

try:
    from PIL import Image, ImageChops
except ImportError:
    sys.exit("Error: falta Pillow. Ejecuta: pip install pillow (o usa .venv/bin/python)")


ANIMAL_RULES = [
    ("Bactrian camel", "montura", "camello_bactriano", ["camello", "bactriano"]),
    ("dromedary camel", "montura", "dromedario", ["dromedario", "camello"]),
    ("steppe donkey", "montura", "burro", ["burro", "asno"]),
    ("steppe pack mule", "montura", "mula", ["mula"]),
    ("Asian war elephant", "montura", "elefante_guerra", ["elefante"]),
    ("steppe draft ox", "ganado", "buey", ["buey"]),
    ("steppe herd cow", "ganado", "vaca", ["vaca"]),
    ("young steppe calf", "ganado", "becerro", ["becerro", "ternero"]),
    ("cashmere goat", "ganado", "cabra", ["cabra"]),
    ("highland yak", "ganado", "yak", ["yak"]),
    ("Caspian steppe tiger", "salvaje", "tigre", ["tigre"]),
    ("snow leopard", "salvaje", "leopardo_nieves", ["leopardo"]),
    ("steppe red fox", "salvaje", "zorro", ["zorro"]),
    ("Altai red deer stag", "salvaje", "ciervo", ["ciervo"]),
    ("red deer hind", "salvaje", "cierva", ["cierva", "ciervo"]),
    ("Siberian mountain ibex", "salvaje", "ibice", ["ibice", "cabra_montes"]),
    ("mountain cougar", "salvaje", "puma", ["puma"]),
    ("steppe coyote", "salvaje", "coyote", ["coyote"]),
    ("striped hyena", "salvaje", "hiena", ["hiena"]),
    ("Eurasian wild boar", "salvaje", "jabali", ["jabali"]),
    ("saiga antelope", "salvaje", "saiga", ["saiga", "antilope"]),
    ("goitered gazelle", "salvaje", "gacela", ["gacela"]),
    ("brown steppe hare", "salvaje", "liebre", ["liebre"]),
    ("steppe reindeer", "salvaje", "reno", ["reno"]),
    ("golden eagle", "ave", "aguila", ["aguila"]),
    ("saker falcon", "ave", "halcon", ["halcon"]),
    ("cinereous vulture", "ave", "buitre", ["buitre"]),
    ("steppe raven", "ave", "cuervo", ["cuervo"]),
    ("steppe lake fish", "acuatico", "pez", ["pez", "trucha", "carpa"]),
    ("river crocodile", "acuatico", "cocodrilo", ["cocodrilo"]),
    ("sea turtle", "acuatico", "tortuga", ["tortuga"]),
    ("pit viper", "salvaje", "vibora", ["vibora", "serpiente"]),
    ("desert scorpion", "insecto", "escorpion", ["escorpion"]),
    ("wolf spider", "insecto", "arana", ["arana"]),
    ("carrion blowfly", "insecto", "mosca", ["mosca"]),
    ("wild honeybee", "insecto", "abeja", ["abeja"]),
    ("hornet wasp", "insecto", "avispa", ["avispa"]),
    ("steppe mosquito", "insecto", "mosquito", ["mosquito"]),
    ("honeycomb hive", "insecto", "colmena", ["colmena"]),
    ("wasp nest", "insecto", "avispero", ["avispero"]),
]

PROP_GROUP_RULES = [
    ("helmets", "casco", ["casco", "yelmo"]),
    ("torso cuirasses", "coraza", ["chaleco", "coraza", "peto", "armadura"]),
    ("armored waist belts", "cinturon", ["cinturon", "faja"]),
    ("boots and greaves", "botas_grebas", ["greba", "bota", "pierna"]),
    ("shields", "escudo", ["escudo"]),
    ("short melee weapons", "arma_corta", ["cuchillo", "daga", "espada_corta", "arma"]),
    ("short one-handed impact", "arma_impacto", ["maza", "hacha", "martillo", "garrote"]),
    ("long swords", "espada_larga", ["espada", "sable"]),
    ("polearms and spears", "lanza", ["lanza", "pica", "alabarda", "asta"]),
]


def load_inventory_ids(tsv_path: Path) -> list[str]:
    """Lee todos los IDs oficiales desde assets/inventario.tsv."""
    if not tsv_path.is_file():
        print(f"[AVISO] No se encontró {tsv_path}. Se usarán IDs normalizados por defecto.")
        return []

    ids = []
    with tsv_path.open("r", encoding="utf-8") as f:
        reader = csv.reader(f, delimiter="\t")
        id_col = 0
        for row in reader:
            if not row or row[0].startswith("#"):
                continue
            if "id" in [c.strip().lower() for c in row]:
                id_col = [c.strip().lower() for c in row].index("id")
                continue
            val = row[id_col].strip()
            if val:
                ids.append(val)
    return ids


def find_best_inventory_id(
    inventory_ids: list[str],
    expected_prefix: str,
    keywords: list[str],
    fallback_id: str,
) -> tuple[str, bool]:
    """Busca coincidencia exacta o parcial en assets/inventario.tsv."""
    if fallback_id in inventory_ids:
        return fallback_id, True

    scoped = [i for i in inventory_ids if i.startswith(expected_prefix)]
    for kw in keywords:
        for item_id in scoped:
            if kw in item_id.lower():
                return item_id, True

    for kw in keywords:
        for item_id in inventory_ids:
            if kw in item_id.lower():
                return item_id, True

    return fallback_id, False


def trim_and_pad_square(img: Image.Image, margin_ratio: float = 0.15) -> Image.Image:
    """Recorta fondo blanco sobrante y centra el objeto en lienzo cuadrado."""
    rgb = img.convert("RGB")
    bg = Image.new("RGB", rgb.size, (255, 255, 255))
    diff = ImageChops.difference(rgb, bg)
    diff = diff.point(lambda p: 255 if p > 10 else 0)
    bbox = diff.getbbox()

    if bbox is None:
        return rgb

    cropped = rgb.crop(bbox)
    cw, ch = cropped.size
    side = int(max(cw, ch) * (1.0 + 2.0 * margin_ratio))
    side = max(side, 64)

    canvas = Image.new("RGB", (side, side), (255, 255, 255))
    offset_x = (side - cw) // 2
    offset_y = (side - ch) // 2
    canvas.paste(cropped, (offset_x, offset_y))
    return canvas


def process_animals(src_dir: Path, out_dir: Path, inventory_ids: list[str]) -> None:
    """Renombra las imágenes de animales según su ID de inventario y numera variantes."""
    if not src_dir.is_dir():
        print(f"[!] Carpeta de animales no encontrada: {src_dir}")
        return

    out_dir.mkdir(parents=True, exist_ok=True)
    files = sorted([*src_dir.glob("*.png"), *src_dir.glob("*.jpg")])
    counters: dict[str, int] = {}
    unmatched = []

    print(f"\n=== Procesando Animales ({len(files)} archivos) ===")
    for png in files:
        name_lower = png.name.lower()
        matched_rule = None
        for eng_key, subcat, slug, kws in ANIMAL_RULES:
            if eng_key.lower() in name_lower:
                matched_rule = (subcat, slug, kws)
                break

        if matched_rule is None:
            unmatched.append(png.name)
            continue

        subcat, slug, kws = matched_rule
        fallback_id = f"animal.{subcat}.{slug}"
        inv_id, found_in_tsv = find_best_inventory_id(
            inventory_ids, f"animal.{subcat}.", kws, fallback_id
        )

        counters[inv_id] = counters.get(inv_id, 0) + 1
        variant = counters[inv_id]
        dest_name = f"{inv_id}_v{variant}{png.suffix}"  # con su formato: un JPEG no se llama .png
        shutil.copy2(png, out_dir / dest_name)

        tag = "TSV" if found_in_tsv else "NUEVO"
        print(f"  [{tag}] {dest_name}  <-  {png.name[:55]}...")

    print(f"[OK] Animales exportados en: {out_dir} ({sum(counters.values())} archivos, {len(counters)} especies)")
    if unmatched:
        print(f"[AVISO] Sin regla coincidente ({len(unmatched)}): {unmatched}")


def process_props(
    src_dir: Path,
    out_dir: Path,
    tsv_map_path: Path,
    inventory_ids: list[str],
    margin_ratio: float,
    keep_both_rows: bool,
) -> None:
    """Recorta hojas de equipamiento en cuadrícula 3x2 en piezas individuales cuadradas."""
    if not src_dir.is_dir():
        print(f"[!] Carpeta de props no encontrada: {src_dir}")
        return

    out_dir.mkdir(parents=True, exist_ok=True)
    tsv_map_path.parent.mkdir(parents=True, exist_ok=True)
    files = sorted([*src_dir.glob("*.png"), *src_dir.glob("*.jpg")])

    equip_candidates: dict[str, list[str]] = {}
    for _, group_slug, kws in PROP_GROUP_RULES:
        matches = [
            i for i in inventory_ids
            if not i.startswith("animal.")
            and any(kw in i.lower() for kw in kws)
        ]
        equip_candidates[group_slug] = matches

    group_counters: dict[str, int] = {}
    map_rows = []

    print(f"\n=== Recortando Hojas de Props/Equipamiento ({len(files)} hojas 3x2) ===")
    for png in files:
        name_lower = png.name.lower()
        group_slug = "prop_general"
        for eng_key, slug, _ in PROP_GROUP_RULES:
            if eng_key.lower() in name_lower:
                group_slug = slug
                break

        with Image.open(png) as img:
            w, h = img.size
            col_w = w // 3
            row_h = h // 2
            rows_to_export = [(0, "front"), (1, "back")] if keep_both_rows else [(0, "front")]

            for col in range(3):
                group_counters[group_slug] = group_counters.get(group_slug, 0) + 1
                idx = group_counters[group_slug]

                candidates = equip_candidates.get(group_slug, [])
                suggested_id = (
                    candidates[idx - 1]
                    if idx - 1 < len(candidates)
                    else f"equipo.{group_slug}.{idx:02d}"
                )

                for row_idx, view_label in rows_to_export:
                    left = col * col_w
                    upper = row_idx * row_h
                    right = w if col == 2 else (col + 1) * col_w
                    lower = h if row_idx == 1 else (row_idx + 1) * row_h

                    cell = img.crop((left, upper, right, lower))
                    clean_cell = trim_and_pad_square(cell, margin_ratio=margin_ratio)

                    out_filename = f"{group_slug}_{idx:02d}_{view_label}.png"
                    clean_cell.save(out_dir / out_filename, format="PNG")

                    map_rows.append([
                        out_filename,
                        png.name,
                        f"col_{col + 1}_{view_label}",
                        group_slug,
                        suggested_id,
                        suggested_id,
                    ])

    with tsv_map_path.open("w", encoding="utf-8", newline="") as f:
        writer = csv.writer(f, delimiter="\t")
        writer.writerow([
            "archivo_recortado",
            "hoja_origen",
            "celda",
            "grupo",
            "id_inventario_sugerido",
            "id_inventario_final",
        ])
        writer.writerows(map_rows)

    print(f"[OK] {len(map_rows)} recortes individuales guardados en: {out_dir}")
    print(f"[OK] Tabla de mapeo generada en: {tsv_map_path}")


def apply_mapping(cuts_dir: Path, final_dir: Path, tsv_map_path: Path) -> None:
    """Renombra los recortes aprobados usando la columna 'id_inventario_final' del TSV."""
    if not tsv_map_path.is_file():
        sys.exit(f"Error: no existe {tsv_map_path}. Ejecuta primero el recorte.")

    final_dir.mkdir(parents=True, exist_ok=True)
    used_names: dict[str, int] = {}
    copied = 0

    with tsv_map_path.open("r", encoding="utf-8") as f:
        reader = csv.DictReader(f, delimiter="\t")
        for row in reader:
            src_file = cuts_dir / row["archivo_recortado"].strip()
            target_id = row["id_inventario_final"].strip()
            if not target_id or target_id.startswith("#") or not src_file.is_file():
                continue

            suffix = ""
            if src_file.stem.endswith("_back"):
                suffix = "_back"

            base_key = f"{target_id}{suffix}"
            used_names[base_key] = used_names.get(base_key, 0) + 1
            if used_names[base_key] > 1:
                dest_name = f"{base_key}_v{used_names[base_key]}.png"
            else:
                dest_name = f"{base_key}.png"

            shutil.copy2(src_file, final_dir / dest_name)
            copied += 1

    print(f"[OK] {copied} piezas renombradas con su ID oficial en: {final_dir}")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Recorta cuadrículas 3x2 de props y renombra lotes '2do intento' con IDs de inventario."
    )
    parser.add_argument(
        "--dir-animales",
        type=Path,
        default=Path("arte/fuentes/animales/imagenes"),
        help="Carpeta local con los PNG de animales",
    )
    parser.add_argument(
        "--dir-props",
        type=Path,
        default=Path("arte/fuentes/equipo"),
        help="Carpeta local con las hojas 3x2 de equipamiento",
    )
    parser.add_argument(
        "--inventario",
        type=Path,
        default=Path("assets/inventario.tsv"),
        help="Ruta a assets/inventario.tsv",
    )
    parser.add_argument(
        "--salida",
        type=Path,
        default=Path("arte/referencias"),
        help="Carpeta base de salida",
    )
    parser.add_argument(
        "--margen",
        type=float,
        default=0.15,
        help="Margen blanco cuadrado alrededor de cada recorte (default: 0.15)",
    )
    parser.add_argument(
        "--solo-frontal",
        action="store_true",
        help="Exportar solo la fila superior (vista frontal) de cada hoja 3x2",
    )
    parser.add_argument(
        "--aplicar-mapeo",
        action="store_true",
        help="Aplica los IDs de 'id_inventario_final' del TSV generado a los recortes",
    )

    args = parser.parse_args()
    cuts_dir = args.salida / "equipamiento_recortado"
    final_props_dir = args.salida / "equipamiento_listo"
    tsv_map = args.salida / "mapeo_equipamiento.tsv"

    if args.aplicar_mapeo:
        apply_mapping(cuts_dir, final_props_dir, tsv_map)
        return

    inv_ids = load_inventory_ids(args.inventario)
    print(f"[*] IDs cargados desde {args.inventario}: {len(inv_ids)}")

    process_animals(args.dir_animales, args.salida / "animales_listos", inv_ids)
    process_props(
        args.dir_props,
        cuts_dir,
        tsv_map,
        inv_ids,
        margin_ratio=args.margen,
        keep_both_rows=not args.solo_frontal,
    )


if __name__ == "__main__":
    main()