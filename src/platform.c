#include "platform.h"

#include "raylib.h"

#include <stdio.h>

#if defined(__ANDROID__)
#include <android/configuration.h>
#include <android_native_app_glue.h>

struct android_app *GetAndroidApp(void); // provisto por raylib (rcore_android.c)

const char *platform_asset_path(const char *rel) { return rel; }

const char *platform_save_dir(void) {
    struct android_app *app = GetAndroidApp();
    return app && app->activity && app->activity->internalDataPath ? app->activity->internalDataPath : ".";
}

bool platform_has_keyboard(void) {
    struct android_app *app = GetAndroidApp();
    if (!app || !app->config) return false;
    return AConfiguration_getKeyboard(app->config) == ACONFIGURATION_KEYBOARD_QWERTY;
}

#else

const char *platform_asset_path(const char *rel) { return TextFormat("%s%s", GetApplicationDirectory(), rel); }

bool platform_has_keyboard(void) { return true; }

const char *platform_save_dir(void) {
    static char dir[512];
    snprintf(dir, sizeof(dir), "%ssaves", GetApplicationDirectory());
    if (!DirectoryExists(dir)) MakeDirectory(dir);
    return dir;
}

#endif
