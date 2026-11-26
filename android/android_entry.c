/* Android SDL3 main entry & JNI bridge:
 * 1. Resolves app external/internal storage dir and prepares <files>/game and <files>/logs.
 * 2. Runs the parallel touch launcher (launcher/main_android.c).
 * 3. Configures DW3_TOUCH_PAD, DW3_LOG, and runs dw3_game_main() in-process. */
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

/* launcher/cloud.c's HTTP on Android: Dw3Activity.httpRequest (HttpURLConnection). headers is "Name: value\n" lines;
 * body may be NULL. On success *body_out is a malloc'd, NUL-terminated copy of the response body (any status) and
 * resp_headers holds "Name: value\n" lines; returns -1 when no HTTP response arrived at all. Runs on the SDL thread,
 * which never returns to Java, so every local reference lives in a frame popped before returning. */
int android_http_request(const char *method, const char *url, const char *headers, const void *body, size_t body_len,
                         int *status_out, char **body_out, size_t *body_len_out, char *resp_headers,
                         size_t resp_headers_n) {
    *status_out = 0;
    *body_out = NULL;
    *body_len_out = 0;
    if (resp_headers_n) resp_headers[0] = 0;
    JNIEnv *env = (JNIEnv *)SDL_GetAndroidJNIEnv();
    if (!env || (*env)->PushLocalFrame(env, 16) != 0) return -1;
    int rc = -1;
    jobject activity = (jobject)SDL_GetAndroidActivity();
    /* FindClass on a native thread only sees the system class loader; the activity's class comes from the app's. */
    jclass cls = activity ? (*env)->GetObjectClass(env, activity) : NULL;
    jmethodID mid = cls ? (*env)->GetStaticMethodID(env, cls, "httpRequest",
                                                     "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[B[I[Ljava/lang/String;)[B")
                        : NULL;
    if (!mid) goto out;
    jstring jm = (*env)->NewStringUTF(env, method);
    jstring ju = (*env)->NewStringUTF(env, url);
    jstring jh = (*env)->NewStringUTF(env, headers ? headers : "");
    jbyteArray jb = NULL;
    if (body) {
        jb = (*env)->NewByteArray(env, (jsize)body_len);
        if (jb) (*env)->SetByteArrayRegion(env, jb, 0, (jsize)body_len, (const jbyte *)body);
    }
    jintArray jst = (*env)->NewIntArray(env, 1);
    jclass str_cls = (*env)->FindClass(env, "java/lang/String");
    jobjectArray jhdr = str_cls ? (*env)->NewObjectArray(env, 1, str_cls, NULL) : NULL;
    if (!jm || !ju || !jh || (body && !jb) || !jst || !jhdr) goto out;

    jbyteArray jres = (jbyteArray)(*env)->CallStaticObjectMethod(env, cls, mid, jm, ju, jh, jb, jst, jhdr);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        goto out;
    }
    if (!jres) goto out;
    jint st = 0;
    (*env)->GetIntArrayRegion(env, jst, 0, 1, &st);
    jsize n = (*env)->GetArrayLength(env, jres);
    char *buf = malloc((size_t)n + 1);
    if (!buf) goto out;
    (*env)->GetByteArrayRegion(env, jres, 0, n, (jbyte *)buf);
    buf[n] = 0;
    jstring jrh = (jstring)(*env)->GetObjectArrayElement(env, jhdr, 0);
    if (jrh && resp_headers_n) {
        const char *h = (*env)->GetStringUTFChars(env, jrh, NULL);
        if (h) {
            snprintf(resp_headers, resp_headers_n, "%s", h);
            (*env)->ReleaseStringUTFChars(env, jrh, h);
        }
    }
    *status_out = st;
    *body_out = buf;
    *body_len_out = (size_t)n;
    rc = 0;
out:
    if ((*env)->ExceptionCheck(env)) (*env)->ExceptionClear(env);
    (*env)->PopLocalFrame(env, NULL);
    return rc;
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
    /* No DW3_PAD_REMAP: that is for the handheld's SDL2 runtime, which has no pad map of its own. This SDL3 runtime maps
     * the gamepad from settings.json (input.gamepad), so pad.map on top would permute it a second time, and would
     * permute the touch pad's buttons, which are already the PS1 buttons they show. */
    setenv("DW3_LOG", log_level == 2 ? "heavy" : log_level == 1 ? "soft" : "off", 1);

    {
        char lp[640];
        snprintf(lp, sizeof(lp), "%s/logs/game.log", base);
        FILE *lf = freopen(lp, "a", stdout);
        if (lf) {
            setvbuf(lf, NULL, _IOLBF, 0);
            dup2(fileno(lf), STDERR_FILENO);
        }
    }

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
