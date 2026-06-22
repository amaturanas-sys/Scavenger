# SCAVENGER - atajos de desarrollo
.PHONY: help install run test seed refresh enrich export docker-build docker-run

help:
	@echo "Comandos disponibles:"
	@echo "  make install      Instala dependencias (pip)"
	@echo "  make run          Levanta API + frontend en http://localhost:8000"
	@echo "  make test         Corre la suite de pruebas (pytest)"
	@echo "  make seed         Carga/actualiza el catalogo local en la BD"
	@echo "  make refresh      Refresca precios reales por cadena (scraping)"
	@echo "  make enrich       Completa nutricion via FatSecret"
	@echo "  make export       Exporta el catalogo de la BD a data/chilean_foods.json"
	@echo "  make docker-build Construye la imagen Docker"
	@echo "  make docker-run   Corre la app en Docker (puerto 8000)"

install:
	pip install -r requirements.txt

run:
	uvicorn backend.main:app --host 0.0.0.0 --port 8000

test:
	python -m pytest -q

seed:
	python -m backend.seed local

refresh:
	python -m backend.refresh_prices

enrich:
	python -m backend.enrich_nutrition

export:
	python -m backend.export_catalog

docker-build:
	docker build -t scavenger .

docker-run:
	docker run --rm -p 8000:8000 scavenger
