// Ajustes del jugador que duran entre partidas (hoy: el idioma de la interfaz).
// Se guardan en ajustes.txt, en la carpeta de las partidas (platform_save_dir).
#ifndef ESTEPA_SETTINGS_H
#define ESTEPA_SETTINGS_H

#include "sim/lang.h"

// Lee los ajustes (si no hay archivo, español) y carga las traducciones.
void settings_init(void);
// Pasa al idioma siguiente y lo guarda.
void settings_next_lang(void);
void settings_free(void);

#endif
