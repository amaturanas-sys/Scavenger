// Ajustes del jugador que duran entre partidas: el idioma de la interfaz y la vista de la camara (N).
// Se guardan en ajustes.txt, en la carpeta de las partidas (platform_save_dir).
#ifndef ESTEPA_SETTINGS_H
#define ESTEPA_SETTINGS_H

#include "sim/lang.h"

// Lee los ajustes (si no hay archivo, español) y carga las traducciones.
void settings_init(void);
// Pasa al idioma siguiente y lo guarda.
void settings_next_lang(void);
// La vista de la camara guardada (src/sim/camview.h; de serie, cerca) y cambiarla (se guarda).
int settings_camera(void);
void settings_set_camera(int view);
void settings_free(void);

#endif
