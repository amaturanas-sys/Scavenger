// Diferencias por plataforma (escritorio vs Android).
//
// El port de Android replica la experiencia de PC: se juega con teclado
// fisico (tablets, Chromebooks), con raton o mando opcionales. No hay
// controles tactiles.
#ifndef ESTEPA_PLATFORM_H
#define ESTEPA_PLATFORM_H

#include <stdbool.h>

// Ruta de un asset. En Android raylib lo lee desde el APK.
const char *platform_asset_path(const char *rel);

// true si hay un teclado fisico disponible. En escritorio siempre true;
// en Android consulta la configuracion del dispositivo (se actualiza al
// conectar o desconectar un teclado, sin reiniciar la actividad).
bool platform_has_keyboard(void);

#endif
