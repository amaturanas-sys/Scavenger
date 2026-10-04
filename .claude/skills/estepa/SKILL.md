---
name: estepa
description: Convenciones de SCAVENGERS THRIVE (C11 + raylib, Windows y Android). Usar antes de tocar código del juego, al diseñar sistemas, y al elegir entre las skills de juego del repo (game-developer, engine-patterns, game-design, cpp-pro, test-master...).
---

# SCAVENGERS THRIVE - THEY COME FROM THE STEPPES

Las skills genéricas de juego (Unity/Unreal, ECS, multijugador) se aplican con estas reglas.

## Arquitectura
- `src/sim/`: C puro, sin raylib; todo lo que tiene reglas va aquí y se prueba en `tests/test_main.c`.
- `src/game/`: une la simulación con raylib (entrada, dibujo, diálogos).
- Sin asignaciones por cuadro: arreglos fijos (`*_MAX`), estructuras planas, nada de ECS genérico.
- 640x360 de baja resolución; Android: táctil y rendimiento modesto (ver `mobile-games`).

## Reglas del juego
- El jugador nunca muere. Los compañeros sí.
- Toda la interfaz pasa por `T("...")` / `N_("...")`; traducciones en `assets/i18n/en.tsv`.
- Objetos nuevos: `assets/inventario.tsv`. Iconos: `tools/assets/iconos.py generar`.
- Ghidra solo sobre nuestros propios builds.

## Antes de subir
```sh
cmake --build build && ./build/sim_tests
python3 tools/assets/i18n.py extraer && python3 tools/assets/i18n.py check
python3 tools/assets/inventario.py check && python3 tools/assets/iconos.py check
```

## Qué skill usar
- Diseño de un sistema o mecánica: `game-design`, `game-mechanics-designer`, `gd-design-game-loop`, `feature-forge`.
- Implementación: `game-developer`, `engine-patterns`, `cpp-pro` (aplicar en C11).
- Pruebas y fallos: `test-master`, `debugging-wizard`; revisión: `code-reviewer`.
- Documentación: `code-documenter` (los documentos del proyecto van en español, en `docs/`).
- Equipo de agentes: `/gamedev <tarea>` (agentes `gamedev-*`).
