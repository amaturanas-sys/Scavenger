#include "settings.h"

#include <stdio.h>
#include <string.h>

#include "platform.h"
#include "raylib.h"

static const char *settings_path(void) { return TextFormat("%s/ajustes.txt", platform_save_dir()); }

static void save(void) {
    FILE *f = fopen(settings_path(), "w");
    if (!f) return;
    fprintf(f, "idioma=%s\n", lang_code(lang_get()));
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
    char *txt = FileExists(settings_path()) ? LoadFileText(settings_path()) : NULL;
    if (txt) {
        const char *k = strstr(txt, "idioma=");
        if (k)
            for (int l = 0; l < LANG_COUNT; l++)
                if (!strncmp(k + 7, lang_code((Lang)l), 2)) lang = (Lang)l;
        UnloadFileText(txt);
    }
    lang_set(lang);
}

void settings_next_lang(void) {
    lang_set((Lang)((lang_get() + 1) % LANG_COUNT));
    save();
}

void settings_free(void) { lang_free(); }
