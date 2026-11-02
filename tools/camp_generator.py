#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Generador Nodal de Estructuras

import argparse
import json
import sys
import math
import random

# Constantes
CAMP_MAX_NODES = 256
CAMP_DEFAULT_RADIUS = 50.0

CAMP_YURT = "yurt"
CAMP_CARAVAN = "caravan"
CAMP_PALISADE = "palisade"
CAMP_FORT = "fort"

# Tipos de nodos
CAMP_TYPE_MAP = {
    CAMP_YURT: 0,
    CAMP_CARAVAN: 1,
    CAMP_PALISADE: 2,
    CAMP_FORT: 3
}

CAMP_TYPE_MAP_REVERSE = {v: k for k, v in CAMP_TYPE_MAP.items()}

# Modelo de nodo
class CampNode:
    def __init__(self, x, y, z, yaw, pitch, scale, node_type, seed):
        self.x = x
        self.y = y
        self.z = z
        self.yaw = yaw
        self.pitch = pitch
        self.scale = scale
        self.type = node_type
        self.seed = seed

    def to_dict(self):
        return {
            "x": self.x,
            "y": self.y,
            "z": self.z,
            "yaw": self.yaw,
            "pitch": self.pitch,
            "scale": self.scale,
            "type": self.type,
            "seed": self.seed
        }

class CampGenConfig:
    def __init__(self, seed, yurt_count, caravan_count, palisade_count, fort_count,
                 circular, circle_radius):
        self.seed = seed
        self.yurt_count = yurt_count
        self.caravan_count = caravan_count
        self.palisade_count = palisade_count
        self.fort_count = fort_count
        self.circular = circular
        self.circle_radius = circle_radius

    def to_dict(self):
        return {
            "seed": self.seed,
            "yurt_count": self.yurt_count,
            "caravan_count": self.caravan_count,
            "palisade_count": self.palisade_count,
            "fort_count": self.fort_count,
            "circular": self.circular,
            "circle_radius": self.circle_radius
        }

def _get_node_type(i, config):
    """Determina el tipo de nodo según su posición en la secuencia."""
    if i < config.yurt_count:
        return CAMP_YURT
    elif i < config.yurt_count + config.caravan_count:
        return CAMP_CARAVAN
    elif i < config.yurt_count + config.caravan_count + config.palisade_count:
        return CAMP_PALISADE
    else:
        return CAMP_FORT

def generate_circular(camp, config, node_seed):
    """Genera nodos en un círculo."""
    total = (config.yurt_count + config.caravan_count +
             config.palisade_count + config.fort_count)
    
    if total == 0 or total > CAMP_MAX_NODES:
        return
    
    camp["nodes"] = []
    angle_step = 2.0 * math.pi / total
    radius = config.circle_radius
    
    rng = random.Random(node_seed)
    
    for i in range(total):
        node_type = _get_node_type(i, config)
        
        angle = i * angle_step
        offset = rng.random() * 0.3
        r = radius * (1.0 + offset - 0.15)
        
        node = CampNode(
            x=r * math.cos(angle),
            y=0.0,
            z=r * math.sin(angle),
            yaw=angle + math.pi,
            pitch=0.0,
            scale=1.0,
            node_type=node_type,
            seed=rng.randint(0, 2**32 - 1)
        )
        camp["nodes"].append(node)

def generate_linear(camp, config, node_seed):
    """Genera nodos en línea."""
    total = (config.yurt_count + config.caravan_count +
             config.palisade_count + config.fort_count)
    
    if total == 0 or total > CAMP_MAX_NODES:
        return
    
    camp["nodes"] = []
    spacing = 12.0
    start_x = -((total - 1) * spacing) * 0.5
    
    rng = random.Random(node_seed)
    
    for i in range(total):
        node_type = _get_node_type(i, config)
        
        node = CampNode(
            x=start_x + i * spacing,
            y=0.0,
            z=0.0,
            yaw=0.0,
            pitch=0.0,
            scale=1.0,
            node_type=node_type,
            seed=rng.randint(0, 2**32 - 1)
        )
        camp["nodes"].append(node)

def generate_camp(config, node_seed):
    """Genera un campamento completo."""
    camp = {
        "config": config.to_dict(),
        "nodes": [],
        "area_min_x": 0.0,
        "area_min_z": 0.0,
        "area_max_x": 0.0,
        "area_max_z": 0.0
    }
    
    if config.circular:
        generate_circular(camp, config, node_seed)
    else:
        generate_linear(camp, config, node_seed)
    
    # Calcular bounding box
    nodes = camp.get("nodes", [])
    if nodes:
        camp["area_min_x"] = min(n.x for n in nodes)
        camp["area_max_x"] = max(n.x for n in nodes)
        camp["area_min_z"] = min(n.z for n in nodes)
        camp["area_max_z"] = max(n.z for n in nodes)
    
    return camp

def export_json(camp, output_file):
    """Exporta el campamento a JSON."""
    data = {
        "config": camp["config"],
        "nodes": [n.to_dict() for n in camp["nodes"]],
        "area_min_x": camp["area_min_x"],
        "area_min_z": camp["area_min_z"],
        "area_max_x": camp["area_max_x"],
        "area_max_z": camp["area_max_z"]
    }
    
    with open(output_file, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, ensure_ascii=False)

def validate_camp(camp):
    """Valida el campamento generado."""
    errors = []
    
    config = camp["config"]
    nodes = camp.get("nodes", [])
    
    total_expected = (config["yurt_count"] + config["caravan_count"] +
                     config["palisade_count"] + config["fort_count"])
    total_actual = len(nodes)
    
    if total_expected != total_actual:
        errors.append(f"Mismatch: expected {total_expected} nodes, got {total_actual}")
    
    if total_actual > CAMP_MAX_NODES:
        errors.append(f"Too many nodes: {total_actual} > {CAMP_MAX_NODES}")
    
    for i, node in enumerate(nodes):
        if not isinstance(node, CampNode):
            errors.append(f"Node {i} is not a CampNode instance")
        else:
            if node.x < camp["area_min_x"] or node.x > camp["area_max_x"]:
                errors.append(f"Node {i} x={node.x} outside bounding box")
            if node.z < camp["area_min_z"] or node.z > camp["area_max_z"]:
                errors.append(f"Node {i} z={node.z} outside bounding box")
    
    return errors

def main():
    parser = argparse.ArgumentParser(
        description="Generador Nodal de Estructuras (Yurtas y Campamentos)"
    )
    
    parser.add_argument("--seed", type=int, default=42,
                        help="Semilla del generador")
    parser.add_argument("--yurt-count", type=int, default=3,
                        help="Número de yurtas")
    parser.add_argument("--caravan-count", type=int, default=2,
                        help="Número de carros")
    parser.add_argument("--palisade-count", type=int, default=2,
                        help="Número de empalizadas")
    parser.add_argument("--fort-count", type=int, default=0,
                        help="Número de fortalezas")
    parser.add_argument("--circular", action="store_true",
                        help="Diseño circular")
    parser.add_argument("--radius", type=float, default=CAMP_DEFAULT_RADIUS,
                        help="Radio del círculo")
    parser.add_argument("--output", "-o", default="campamento.json",
                        help="Archivo de salida (JSON)")
    parser.add_argument("--validate", action="store_true",
                        help="Validar el resultado")
    
    args = parser.parse_args()
    
    config = CampGenConfig(
        seed=args.seed,
        yurt_count=args.yurt_count,
        caravan_count=args.caravan_count,
        palisade_count=args.palisade_count,
        fort_count=args.fort_count,
        circular=args.circular,
        circle_radius=args.radius
    )
    
    camp = generate_camp(config, args.seed)
    
    if args.validate:
        errors = validate_camp(camp)
        if errors:
            print("Validation errors:")
            for error in errors:
                print(f"  - {error}")
            sys.exit(1)
        else:
            print("✓ Campamento validado correctamente")
    
    export_json(camp, args.output)
    print(f"✓ Campamento generado: {args.output}")

if __name__ == "__main__":
    main()