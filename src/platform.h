// Diferencias por plataforma (escritorio vs Android).
//
// El port de Android replica la experiencia de PC: se juega con teclado
// fisico (tablets, Chromebooks), con raton o mando opcionales. En Android hay
// ademas dos botones tactiles (pausa y controles) por si el teclado no trae Esc.
#ifndef ESTEPA_PLATFORM_H
#define ESTEPA_PLATFORM_H

#include <stdbool.h>

// Ruta de un asset. En Android raylib lo lee desde el APK.
const char *platform_asset_path(const char *rel);

// ¿Existe el asset? En Android los assets viven dentro del APK: FileExists() no
// los ve; esta funcion pregunta al AssetManager.
bool platform_asset_exists(const char *rel);
// Anota un asset que no se pudo cargar (o que si) para el diagnostico.
void platform_note_asset(const char *rel, bool ok);
int platform_asset_failures(const char **names, int max); // los ultimos que fallaron
int platform_asset_loaded(void);                          // cuantos cargaron
// Android (tactil): se dibujan los botones de pausa y controles.
bool platform_touch_ui(void);

// true si hay un teclado fisico disponible. En escritorio siempre true;
// en Android consulta la configuracion del dispositivo (se actualiza al
// conectar o desconectar un teclado, sin reiniciar la actividad).
bool platform_has_keyboard(void);

// Carpeta de las partidas guardadas (existe al volver). En escritorio, "saves"
// junto al ejecutable; en Android, el almacenamiento interno de la app.
const char *platform_save_dir(void);

#endif
