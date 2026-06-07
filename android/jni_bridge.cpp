/**
 * JNI bridge for udp2raw Android shared library plugin.
 *
 * This file implements the JNI entry points called from Kotlin's
 * Udp2RawPluginCore class. It manages the udp2raw event loop lifecycle
 * by running it on a dedicated pthread and using ev_async to safely
 * signal stop from the Kotlin side.
 *
 * Build flags required: -D__ANDROID__ -DUDP2RAW_LINUX
 */
#ifdef __ANDROID__

#include <jni.h>
#include <android/log.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdexcept>
#include <unistd.h>

#include "../misc.h"
#include "../log.h"
#include "../common.h"
#include "udp2raw_android.h"

#define JTAG "udp2raw-jni"
#define JLOGI(...) __android_log_print(ANDROID_LOG_INFO,  JTAG, __VA_ARGS__)
#define JLOGE(...) __android_log_print(ANDROID_LOG_ERROR, JTAG, __VA_ARGS__)

// Forward declarations from misc.cpp and log.cpp
extern void reset_udp2raw_globals();
extern void udp2raw_set_log_file(const char* path);
extern void udp2raw_close_log_file();

// Forward declaration from main.cpp
extern int udp2raw_run(int argc, char* argv[]);

// ── Global JNI state ────────────────────────────────────────────────────────

static pthread_mutex_t g_mutex       = PTHREAD_MUTEX_INITIALIZER;
static volatile int    g_running     = 0;
static ev_async        g_stop_watcher;
static struct ev_loop* g_loop        = nullptr;

// argv storage for the worker thread
static char** g_argv = nullptr;
static int    g_argc = 0;

static void free_args() {
    if (g_argv) {
        for (int i = 0; i < g_argc; i++) free(g_argv[i]);
        free(g_argv);
        g_argv = nullptr;
        g_argc = 0;
    }
}

// ev_async callback: safely breaks the libev event loop from another thread
static void stop_async_cb(struct ev_loop* loop, ev_async* w, int /*revents*/) {
    ev_break(loop, EVBREAK_ALL);
}

// Worker thread that runs the udp2raw event loop
static void* udp2raw_thread_fn(void* /*arg*/) {
    JLOGI("udp2raw worker thread started");

    try {
        // Attach an ev_async stop watcher to the default loop BEFORE the
        // udp2raw logic adds its own watchers and calls ev_run().
        struct ev_loop* loop = ev_default_loop(0);
        ev_async_init(&g_stop_watcher, stop_async_cb);
        ev_async_start(loop, &g_stop_watcher);

        pthread_mutex_lock(&g_mutex);
        g_loop = loop;
        pthread_mutex_unlock(&g_mutex);

        udp2raw_run(g_argc, g_argv);

    } catch (const Udp2RawExitException& e) {
        if (e.code != 0) {
            JLOGE("udp2raw exited with error code %d", e.code);
        } else {
            JLOGI("udp2raw exited normally");
        }
    } catch (const std::exception& e) {
        JLOGE("udp2raw std::exception: %s", e.what());
    } catch (...) {
        JLOGE("udp2raw unknown exception");
    }

    // Tear down: remove stop watcher and destroy loop for a clean restart
    pthread_mutex_lock(&g_mutex);
    struct ev_loop* loop = g_loop;
    g_loop    = nullptr;
    g_running = 0;
    pthread_mutex_unlock(&g_mutex);

    if (loop) {
        ev_async_stop(loop, &g_stop_watcher);
        ev_loop_destroy(loop);
    }

    udp2raw_close_log_file();
    free_args();

    JLOGI("udp2raw worker thread finished");
    return nullptr;
}

// ── JNI exports ─────────────────────────────────────────────────────────────
// Class: info.loveyu.mfca.plugin.Udp2RawPluginCore
// All functions use the standard Java_<package>_<Class>_<method> naming.

extern "C" JNIEXPORT jstring JNICALL
Java_info_loveyu_mfca_plugin_Udp2RawPluginCore_nativeGetVersion(
        JNIEnv* env, jobject /*thiz*/) {
    extern const char* gitversion;
    return env->NewStringUTF(gitversion ? gitversion : "unknown");
}

extern "C" JNIEXPORT jint JNICALL
Java_info_loveyu_mfca_plugin_Udp2RawPluginCore_nativeStart(
        JNIEnv* env, jobject /*thiz*/, jobjectArray jargs, jstring jlogFile) {

    pthread_mutex_lock(&g_mutex);
    if (g_running) {
        pthread_mutex_unlock(&g_mutex);
        JLOGE("nativeStart: already running");
        return -1;
    }

    // Convert Java String[] → C argv (prepend "udp2raw" as argv[0])
    int extra = env->GetArrayLength(jargs);
    int argc  = extra + 1;
    char** argv = static_cast<char**>(malloc((argc + 1) * sizeof(char*)));
    if (!argv) {
        pthread_mutex_unlock(&g_mutex);
        return -2;
    }
    argv[0] = strdup("udp2raw");
    for (int i = 0; i < extra; i++) {
        auto jstr = static_cast<jstring>(env->GetObjectArrayElement(jargs, i));
        const char* s = env->GetStringUTFChars(jstr, nullptr);
        argv[i + 1]   = strdup(s);
        env->ReleaseStringUTFChars(jstr, s);
        env->DeleteLocalRef(jstr);
    }
    argv[argc] = nullptr;

    free_args();
    g_argv = argv;
    g_argc = argc;

    // Set log file before resetting globals (which may clear state)
    if (jlogFile) {
        const char* logPath = env->GetStringUTFChars(jlogFile, nullptr);
        udp2raw_set_log_file(logPath);
        env->ReleaseStringUTFChars(jlogFile, logPath);
    }

    reset_udp2raw_globals();
    g_running = 1;
    pthread_mutex_unlock(&g_mutex);

    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int ret = pthread_create(&thread, &attr, udp2raw_thread_fn, nullptr);
    pthread_attr_destroy(&attr);

    if (ret != 0) {
        pthread_mutex_lock(&g_mutex);
        g_running = 0;
        pthread_mutex_unlock(&g_mutex);
        free_args();
        JLOGE("nativeStart: pthread_create failed: %d", ret);
        return -3;
    }

    JLOGI("nativeStart: worker thread launched");
    return 0;
}

extern "C" JNIEXPORT void JNICALL
Java_info_loveyu_mfca_plugin_Udp2RawPluginCore_nativeStop(
        JNIEnv* /*env*/, jobject /*thiz*/) {

    pthread_mutex_lock(&g_mutex);
    struct ev_loop* loop = g_loop;
    pthread_mutex_unlock(&g_mutex);

    if (loop) {
        JLOGI("nativeStop: sending stop signal to event loop");
        ev_async_send(loop, &g_stop_watcher);
    } else {
        JLOGI("nativeStop: not running");
    }
}

extern "C" JNIEXPORT jboolean JNICALL
Java_info_loveyu_mfca_plugin_Udp2RawPluginCore_nativeIsRunning(
        JNIEnv* /*env*/, jobject /*thiz*/) {
    pthread_mutex_lock(&g_mutex);
    jboolean running = g_running ? JNI_TRUE : JNI_FALSE;
    pthread_mutex_unlock(&g_mutex);
    return running;
}

#endif  // __ANDROID__
