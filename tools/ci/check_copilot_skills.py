#!/usr/bin/env python3
"""Comprueba que las skills compartidas de Copilot no se separen de Claude."""

from pathlib import Path
import filecmp
import sys

ROOT = Path(__file__).resolve().parents[2]
PAIRS = (
    ("estepa", ROOT / ".claude/skills/estepa/SKILL.md"),
    ("cad", ROOT / ".claude/skills/cad/SKILL.md"),
    ("modelos", ROOT / ".claude/skills/modelos/SKILL.md"),
    ("animaciones", ROOT / ".claude/skills/animaciones/SKILL.md"),
    ("game-design", ROOT / ".claude/skills/game-design/SKILL.md"),
)

errors = []
for name, claude_path in PAIRS:
    copilot_path = ROOT / ".github/skills" / name / "SKILL.md"
    if not copilot_path.is_file():
        errors.append(f"falta Copilot: {copilot_path.relative_to(ROOT)}")
    elif not filecmp.cmp(claude_path, copilot_path, shallow=False):
        errors.append(f"desincronizada: {name}")

agent = ROOT / ".claude/agents/gamedev-asset-pipeline.md"
adapted = ROOT / ".github/skills/gamedev-asset-pipeline/SKILL.md"
if not agent.is_file() or not adapted.is_file():
    errors.append("falta la adaptación gamedev-asset-pipeline")

if errors:
    for error in errors:
        print(f"ERROR: {error}")
    sys.exit(1)

print(f"skills Copilot sincronizadas: {len(PAIRS)}; adaptación de assets: OK")
