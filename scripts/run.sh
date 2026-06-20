#!/usr/bin/env bash
# Arranca SCAVENGER (API + frontend) en http://localhost:8000
set -euo pipefail
cd "$(dirname "$0")/.."

if [ ! -d ".venv" ] && ! python3 -c "import fastapi" 2>/dev/null; then
  echo "Instalando dependencias..."
  pip3 install -r requirements.txt
fi

echo "SCAVENGER disponible en http://localhost:8000 (docs: /docs)"
exec uvicorn backend.main:app --host 0.0.0.0 --port 8000 "$@"
