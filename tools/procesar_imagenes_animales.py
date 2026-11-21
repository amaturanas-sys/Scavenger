#!/usr/bin/env python3
"""
Procesa imágenes referenciales de animales desde Google Drive.

Toma imágenes con 3 vistas (frente/lado/espalda), selecciona la vista más
conveniente según el tipo de esqueleto (rig), cambia el fondo al color de
alto contraste apropiado y guarda al menos 2 imágenes por animal.
"""
import os, sys, json, argparse
from PIL import Image
import numpy as np

FONDO_BLANCO = (255, 255, 255)
FONDO_NEGRO = (0, 0, 0)
FONDO_VERDE = (0, 255, 0)

RUTA_SALIDA = "arte/referencias/animales_procesados"

def detectar_color_predominante(imagen):
    arr = np.array(imagen.convert("RGB"))
    h, w, _ = arr.shape
    centro = arr[h//4:3*h//4, w//4:3*w//4]
    return centro.mean(axis=2).mean()

def elegir_fondo_por_brillo(imagen, es_bicolor=False, es_pelaje_claro=False):
    if es_bicolor:
        return FONDO_VERDE
    brillo = detectar_color_predominante(imagen)
    if es_pelaje_claro or brillo > 200:
        return FONDO_NEGRO
    return FONDO_BLANCO

def recortar_vista(imagen, fraccion_x, orientacion="horizontal"):
    """Recorta una tercera parte de la imagen según la orientación."""
    w, h = imagen.size
    if orientacion == "horizontal":
        tercio = w // 3
        x0 = fraccion_x * tercio
        x1 = (fraccion_x + 1) * tercio if fraccion_x < 2 else w
        return imagen.crop((x0, 0, x1, h))
    else:
        tercio = h // 3
        y0 = fraccion_x * tercio
        y1 = (fraccion_x + 1) * tercio if fraccion_x < 2 else h
        return imagen.crop((0, y0, w, y1))

def cambiar_fondo(imagen, color_fondo, tolerancia=30):
    imagen = imagen.convert("RGBA")
    arr = np.array(imagen)
    h, w, _ = arr.shape
    esquina_tl = arr[0, 0, :3]
    esquina_tr = arr[0, w-1, :3]
    esquina_bl = arr[h-1, 0, :3]
    esquina_br = arr[h-1, w-1, :3]
    color_fondo_actual = np.mean([esquina_tl, esquina_tr, esquina_bl, esquina_br], axis=0)
    rgb = arr[:, :, :3].astype(np.int32)
    diff = np.abs(rgb - color_fondo_actual).sum(axis=2)
    mascara_fondo = diff < tolerancia * 3
    nuevo_fondo = np.array(color_fondo + (255,), dtype=np.uint8)
    arr[mascara_fondo] = nuevo_fondo
    return Image.fromarray(arr, "RGBA").convert("RGB")


def procesar_imagen_tres_vistas(ruta_entrada, id_animal, rig, es_bicolor=False, es_pelaje_claro=False, orientacion="horizontal"):
    """Procesa una imagen con 3 vistas y extrae la vista adecuada según el rig."""
    imagen = Image.open(ruta_entrada)

    # Recortar las 3 vistas según la orientación
    vistas = {
        "frente": recortar_vista(imagen, 0, orientacion),
        "lado": recortar_vista(imagen, 1, orientacion),
        "espalda": recortar_vista(imagen, 2, orientacion),
    }

    # Seleccionar vistas según el rig - al menos 2 imágenes por animal
    vistas_seleccionadas = {}
    if rig == "humanoide":
        # Vista frontal ortográfica en A-pose -> usar "frente" + "lado" como referencia
        vistas_seleccionadas["frontal_apose"] = vistas["frente"]
        vistas_seleccionadas["lateral_referencia"] = vistas["lado"]
    elif rig == "cuadrupedo":
        # Vista lateral izquierda (perfil mirando a la izquierda) + frente como referencia
        vistas_seleccionadas["lateral_izquierda"] = vistas["lado"]
        vistas_seleccionadas["frontal_referencia"] = vistas["frente"]
    elif rig == "ave":
        # Vista dorsal/frontal superior con alas extendidas -> frente + lado
        vistas_seleccionadas["dorsal_tpose"] = vistas["frente"]
        vistas_seleccionadas["lateral_referencia"] = vistas["lado"]
    elif rig == "ninguno":
        # Vista cenital + lateral para invertebrados/peces
        vistas_seleccionadas["cenital"] = vistas["frente"]
        vistas_seleccionadas["lateral"] = vistas["lado"]

    # Elegir fondo de alto contraste
    fondo = elegir_fondo_por_brillo(imagen, es_bicolor=es_bicolor, es_pelaje_claro=es_pelaje_claro)

    # Procesar y guardar cada vista seleccionada
    os.makedirs(RUTA_SALIDA, exist_ok=True)
    archivos_generados = []

    for nombre_vista, vista_img in vistas_seleccionadas.items():
        vista_procesada = cambiar_fondo(vista_img, fondo)
        nombre_archivo = f"{id_animal}_{nombre_vista}.png"
        ruta_salida = os.path.join(RUTA_SALIDA, nombre_archivo)
        vista_procesada.save(ruta_salida, "PNG")
        archivos_generados.append({
            "archivo": ruta_salida,
            "vista": nombre_vista,
            "fondo": fondo,
            "rig": rig,
            "dimensiones": vista_procesada.size,
        })

    return archivos_generados


def main():
    parser = argparse.ArgumentParser(description="Procesa imágenes referenciales de animales")
    parser.add_argument("--archivo-local", required=True, help="Ruta a una imagen local con 3 vistas")
    parser.add_argument("--rig", required=True, choices=["humanoide", "cuadrupedo", "ave", "ninguno"], help="Tipo de esqueleto")
    parser.add_argument("--nombre", required=True, help="ID del animal (ej: animal.salvaje.lobo)")
    parser.add_argument("--bicolor", action="store_true", help="Sujeto bicolor o con manchas contrastadas (fondo verde)")
    parser.add_argument("--pelaje-claro", action="store_true", help="Pelaje blanco o muy claro (fondo negro)")
    parser.add_argument("--orientacion", choices=["horizontal", "vertical"], default="horizontal", help="Orientación de las 3 vistas en la imagen")
    args = parser.parse_args()

    resultado = procesar_imagen_tres_vistas(
        args.archivo_local, args.nombre, args.rig,
        es_bicolor=args.bicolor, es_pelaje_claro=args.pelaje_claro,
        orientacion=args.orientacion
    )

    print(f"\nProcesado: {args.nombre} (rig: {args.rig})")
    for r in resultado:
        print(f"  {r['archivo']} - vista: {r['vista']}, fondo: {r['fondo']}, tamaño: {r['dimensiones']}")

if __name__ == "__main__":
    main()