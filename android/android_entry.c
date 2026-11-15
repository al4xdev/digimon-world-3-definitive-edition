/* Android SDL3 main entry & JNI bridge:
 * 1. Resolves app external/internal storage dir and prepares <files>/game and <files>/logs.
 * 2. Runs the parallel touch launcher (launcher/main_android.c).
 * 3. Configures DW3_TOUCH_PAD, DW3_PAD_REMAP=1, DW3_LOG, and runs dw3_game_main() in-process. */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "touch_pad.h"

extern char game_dir[512];
extern int dw3_android_launcher_run(int kind, const char *script, int *out_touch_pad, int *out_log_level,
                                    char *out_disc, size_t out_disc_sz);
extern int dw3_game_main(int argc, char **argv);

static char android_files_dir[512];
static volatile int saf_done;
static volatile int saf_ok;

JNIEXPORT void JNICALL Java_com_al4xdev_dw2003_Dw3Activity_nativeSetFilesDir(JNIEnv *env, jclass cls, jstring jdir) {
    (void)cls;
    if (!jdir) return;
    const char *s = (*env)->GetStringUTFChars(env, jdir, NULL);
    if (s) {
        snprintf(android_files_dir, sizeof(android_files_dir), "%s", s);
        (*env)->ReleaseStringUTFChars(env, jdir, s);
    }
}

JNIEXPORT void JNICALL Java_com_al4xdev_dw2003_Dw3Activity_nativeOnDiscPicked(JNIEnv *env, jclass cls, jboolean ok) {
    (void)env;
    (void)cls;
    saf_ok = ok ? 1 : 0;
    saf_done = 1;
}

int android_pick_disc_files(void) {
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    jobject activity = (jobject)SDL_GetAndroidActivity();
    if (!env || !activity) return -1;

    jclass cls = (*env)->GetObjectClass(env, activity);
    jmethodID mid = (*env)->GetMethodID(env, cls, "pickDiscFiles", "()V");
    if (!mid) {
        (*env)->DeleteLocalRef(env, cls);
        (*env)->DeleteLocalRef(env, activity);
        return -1;
    }

    saf_done = 0;
    saf_ok = 0;
    (*env)->CallVoidMethod(env, activity, mid);
    (*env)->DeleteLocalRef(env, cls);
    (*env)->DeleteLocalRef(env, activity);

    while (!saf_done) {
        SDL_PumpEvents();
        SDL_Delay(25);
    }
    return saf_ok ? 0 : -1;
}

static void ensure_dir(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) mkdir(path, 0755);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    const char *base = android_files_dir[0] ? android_files_dir : SDL_GetAndroidExternalStoragePath();
    if (!base || !base[0]) base = SDL_GetAndroidInternalStoragePath();
    if (!base || !base[0]) base = ".";

    ensure_dir(base);
    chdir(base);
    setenv("XDG_CACHE_HOME", base, 1);
    snprintf(game_dir, sizeof(game_dir), "%s", base);

    char sub[640];
    snprintf(sub, sizeof(sub), "%s/game", base);
    ensure_dir(sub);
    snprintf(sub, sizeof(sub), "%s/logs", base);
    ensure_dir(sub);

    int touch_pad = 1;
    int log_level = 0;
    char disc[1200] = "";

    int lrc = dw3_android_launcher_run(1 /* PLAT_SDL */, NULL, &touch_pad, &log_level, disc, sizeof(disc));
    if (lrc != 0 || !disc[0]) {
        return 0;
    }

    dw3_touch_pad_set_enabled(touch_pad);
    setenv("DW3_TOUCH_PAD", touch_pad ? "1" : "0", 1);
    setenv("DW3_PAD_REMAP", "1", 1);
    setenv("DW3_LOG", log_level == 2 ? "heavy" : log_level == 1 ? "soft" : "off", 1);

    char *game_argv[] = {
        "dw2003",
        "--config", "./settings.json",
        "--window",
        "--fullscreen",
        "--watchdog", "0",
        NULL
    };
    return dw3_game_main(7, game_argv);
}
