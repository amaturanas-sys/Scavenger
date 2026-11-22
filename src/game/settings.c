#include "settings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "platform.h"
#include "raylib.h"

static const char *settings_path(void) { return TextFormat("%s/ajustes.txt", platform_save_dir()); }
static int g_camera; // CamView (src/sim/camview.h)
static int g_volume = 75; // el volumen general, en %: 0, 25, 50, 75 o 100

static void save(void) {
    FILE *f = fopen(settings_path(), "w");
    if (!f) return;
    fprintf(f, "idioma=%s\ncamara=%d\nsonido=%d\n", lang_code(lang_get()), g_camera, g_volume);
    fclose(f);
}

void settings_init(void) {
    // Las traducciones (assets/i18n/<codigo>.tsv): se cargan todas al arrancar, son pequeñas.
    for (int l = LANG_ES + 1; l < LANG_COUNT; l++) {
        char *txt = LoadFileText(platform_asset_path(TextFormat("assets/i18n/%s.tsv", lang_code((Lang)l))));
        if (txt) {
            lang_load((Lang)l, txt);
            UnloadFileText(txt);
        }
    }
    Lang lang = LANG_ES;
    // Con fopen (no LoadFileText): en Android raylib busca las lecturas en el APK.
    char txt[256] = "";
    FILE *f = fopen(settings_path(), "r");
    if (f) {
        size_t n = fread(txt, 1, sizeof(txt) - 1, f);
        txt[n] = 0;
        fclose(f);
        const char *k = strstr(txt, "idioma=");
        if (k)
            for (int l = 0; l < LANG_COUNT; l++)
                if (!strncmp(k + 7, lang_code((Lang)l), 2)) lang = (Lang)l;
        const char *cam = strstr(txt, "camara=");
        if (cam && cam[7] >= '0' && cam[7] <= '9') g_camera = cam[7] - '0'; // fuera de rango: camview lo lleva a cerca
        const char *snd = strstr(txt, "sonido=");
        if (snd && snd[7] >= '0' && snd[7] <= '9') { // 0, 25, 50, 75 o 100 (lo de en medio, al escalon de abajo)
            int vol = atoi(snd + 7);
            g_volume = vol > 100 ? 100 : vol / 25 * 25;
        }
    }
    lang_set(lang);
}

void settings_next_lang(void) {
    lang_set((Lang)((lang_get() + 1) % LANG_COUNT));
    save();
}

int settings_camera(void) { return g_camera; }

void settings_set_camera(int view) {
    if (view == g_camera) return;
    g_camera = view;
    save();
}

float settings_volume(void) { return (float)g_volume / 100.0f; }
int settings_volume_percent(void) { return g_volume; }

void settings_next_volume(void) {
    g_volume = g_volume >= 100 ? 0 : g_volume + 25;
    save();
}

void settings_free(void) { lang_free(); }
