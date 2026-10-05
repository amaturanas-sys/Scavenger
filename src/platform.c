#include "platform.h"

#include "raylib.h"

#include <stdio.h>
#include <string.h>

#define FAIL_MAX 8
static char g_fail[FAIL_MAX][64];
static int g_fail_count, g_loaded;

void platform_note_asset(const char *rel, bool ok) {
    if (ok) {
        g_loaded++;
        return;
    }
    TraceLog(LOG_WARNING, "ESTEPA: no se pudo cargar %s", rel);
    snprintf(g_fail[g_fail_count % FAIL_MAX], sizeof(g_fail[0]), "%s", rel);
    g_fail_count++;
}

int platform_asset_failures(const char **names, int max) {
    int n = g_fail_count < FAIL_MAX ? g_fail_count : FAIL_MAX;
    if (n > max) n = max;
    for (int i = 0; i < n; i++) names[i] = g_fail[(g_fail_count - 1 - i) % FAIL_MAX];
    return g_fail_count;
}

int platform_asset_loaded(void) { return g_loaded; }

#if defined(__ANDROID__)
#include <android/asset_manager.h>
#include <android/configuration.h>
#include <android_native_app_glue.h>

struct android_app *GetAndroidApp(void); // provisto por raylib (rcore_android.c)

const char *platform_asset_path(const char *rel) { return rel; }

bool platform_asset_exists(const char *rel) {
    struct android_app *app = GetAndroidApp();
    if (!app || !app->activity || !app->activity->assetManager || !rel) return false;
    AAsset *a = AAssetManager_open(app->activity->assetManager, rel, AASSET_MODE_UNKNOWN);
    if (a) AAsset_close(a);
    return a != NULL;
}

bool platform_touch_ui(void) { return true; }

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

bool platform_asset_exists(const char *rel) { return rel && FileExists(rel); }

bool platform_touch_ui(void) { return false; }

const char *platform_save_dir(void) {
    static char dir[512];
    snprintf(dir, sizeof(dir), "%ssaves", GetApplicationDirectory());
    if (!DirectoryExists(dir)) MakeDirectory(dir);
    return dir;
}

#endif
