# Ghidra en el proyecto

[Ghidra](https://github.com/NationalSecurityAgency/ghidra) es una suite de ingeniería inversa. Aquí la usamos para **optimizar nuestros propios builds**.

## Usos previstos
- **Tamaño del ejecutable:** identificar qué funciones o librerías inflan el binario (Windows `.exe`, Android `.so`).
- **Rutas calientes:** revisar el código máquina que genera el compilador en funciones críticas (generación de chunks, simulación de la tropa) y confirmar vectorización, inlining y ausencia de llamadas inesperadas.
- **Comparar compiladores y flags** (GCC, Clang, MSVC; `-O2` vs `-Os`) sobre el mismo código.

## Análisis sin interfaz

```bash
# Requiere Ghidra instalado y Java 21.
$GHIDRA_HOME/support/analyzeHeadless /tmp/ghidra_proyecto estepa \
  -import build/estepa -overwrite
```

## Política sobre software de terceros
No descompilamos juegos ni software comercial para copiar su código o sus assets al proyecto: infringe derechos de autor y licencias, y expondría el juego a ser retirado de Steam o Google Play. Para optimizar nos apoyamos en código abierto (raylib y librerías con licencias permisivas) y en técnicas publicadas, reimplementadas por nosotros.
