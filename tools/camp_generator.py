#!/usr/bin/env python3
"""
Procedural Camp Generator for Scavenger
Generates a nomadic camp layout with circular yurt arrangement and scattered trees.

Layout:
- Central hearth at (0, 0, 0)
- Yurts arranged in concentric circles pointing toward center
- Trees scattered on the outskirts
"""

import json
import math
import random
import sys
from pathlib import Path

# Constants
MAX_YURTS = 50
MAX_TREES = 100
CAMP_RADIUS = 45.0  # meters
YURT_SPACING = 3.5  # meters between yurts
TREE_DENSITY = 0.6  # trees per square meter on outskirts

class CampGenerator:
    def __init__(self, seed=None):
        if seed is not None:
            random.seed(seed)
        self.yurts = []
        self.trees = []
        self.fire = {'x': 0.0, 'y': 0.0, 'z': 0.0}
        
    def generate(self):
        """Generate the camp layout."""
        self._generate_yurts()
        self._generate_trees()
        return {
            'fire': self.fire,
            'yurts': self.yurts,
            'yurt_count': len(self.yurts),
            'trees': self.trees,
            'tree_count': len(self.trees)
        }
    
    def _generate_yurts(self):
        """Generate yurts in concentric circles pointing toward center."""
        # Number of concentric circles
        num_circles = max(1, int(CAMP_RADIUS / (YURT_SPACING * 1.5)))
        
        for circle in range(num_circles):
            # Radius for this circle
            radius = (circle + 1) * YURT_SPACING * 0.8
            
            # Yurts in this circle
            num_yurts = max(1, int(2 * math.pi * radius / YURT_SPACING))
            
            # Offset to avoid placing yurts exactly at the same position
            angle_offset = random.random() * 2 * math.pi
            
            for i in range(num_yurts):
                # Calculate position on circle
                angle = angle_offset + (i / num_yurts) * 2 * math.pi
                
                # Position (on ground level, y will be set by terrain later)
                x = radius * math.cos(angle)
                z = radius * math.sin(angle)
                
                # Yurt rotation: point toward center (subtract 90 degrees for +X facing)
                # When angle is 0 (pointing +X), yurt should face -X (toward center)
                # When angle is PI/2 (pointing +Z), yurt should face -Z (toward center)
                # The model's +X faces toward the fire, so we rotate to face the center
                rotation = angle - math.pi / 2
                
                self.yurts.append({
                    'x': round(x, 2),
                    'y': 0.0,  # Will be set by terrain
                    'z': round(z, 2),
                    'rotation': round(math.degrees(rotation), 1)  # Store as degrees
                })
    
        # Sort by distance from center (optional, for consistent ordering)
        self.yurts.sort(key=lambda y: math.sqrt(y['x']**2 + y['z']**2))
        
        # Limit to MAX_YURTS
        self.yurts = self.yurts[:MAX_YURTS]
    
    def _generate_trees(self):
        """Generate trees scattered on the outskirts."""
        # Generate trees in concentric rings outside the camp area
        min_tree_radius = CAMP_RADIUS * 1.1
        max_tree_radius = CAMP_RADIUS * 2.5
        
        num_rings = int((max_tree_radius - min_tree_radius) / 5.0)
        
        for ring in range(num_rings):
            radius = min_tree_radius + (ring + 1) * 5.0
            
            # Trees in this ring
            circumference = 2 * math.pi * radius
            num_trees = max(1, int(circumference * TREE_DENSITY))
            
            angle_offset = random.random() * 2 * math.pi
            
            for i in range(num_trees):
                angle = angle_offset + (i / num_trees) * 2 * math.pi
                
                # Position
                x = radius * math.cos(angle)
                z = radius * math.sin(angle)
                
                # Add some randomness to position
                x += random.uniform(-2.0, 2.0)
                z += random.uniform(-2.0, 2.0)
                
                self.trees.append({
                    'x': round(x, 2),
                    'y': 0.0,  # Will be set by terrain
                    'z': round(z, 2)
                })
        
        # Limit to MAX_TREES
        self.trees = self.trees[:MAX_TREES]
        
        # Sort by distance from center (optional, for consistent ordering)
        self.trees.sort(key=lambda t: math.sqrt(t['x']**2 + t['z']**2))


def main():
    # Parse command line arguments
    seed = None
    output_file = Path('assets/camp_data.json')
    
    if len(sys.argv) > 1:
        try:
            seed = int(sys.argv[1])
        except ValueError:
            print(f"Warning: Invalid seed '{sys.argv[1]}', using random seed")
    
    # Generate camp
    generator = CampGenerator(seed=seed)
    camp_data = generator.generate()
    
    # Write to JSON file
    output_file.parent.mkdir(parents=True, exist_ok=True)
    with open(output_file, 'w') as f:
        json.dump(camp_data, f, indent=2)
    
    print(f"Camp layout generated successfully!")
    print(f"  Yurts: {camp_data['yurt_count']}")
    print(f"  Trees: {camp_data['tree_count']}")
    print(f"  Output: {output_file}")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())
