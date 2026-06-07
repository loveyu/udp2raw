#include "log.h"
#include "log.h"
#include "misc.h"
#include <android/log.h>
#endif

int log_level = log_info;

int enable_log_position = 0;
int enable_log_color = 1;

// Optional log file path set by JNI bridge (Android only)
#ifdef __ANDROID__
static FILE* g_log_file = nullptr;
static char g_log_file_path[4096] = {0};

void udp2raw_set_log_file(const char* path) {
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = nullptr;
    }
    if (path && path[0]) {
        strncpy(g_log_file_path, path, sizeof(g_log_file_path) - 1);
        g_log_file = fopen(path, "a");
    } else {
        g_log_file_path[0] = 0;
    }
}

void udp2raw_close_log_file() {
    if (g_log_file) {
        fclose(g_log_file);
        g_log_file = nullptr;
    }
}
#endif

void log0(const char* file, const char* function, int line, int level, const char* str, ...) {
    if (level > log_level) return;
    if (level > log_trace || level < 0) return;

    va_list vlist;
    va_start(vlist, str);

#ifdef __ANDROID__
    int priority;
    switch (level) {
        case log_fatal:
        case log_error: priority = ANDROID_LOG_ERROR; break;
        case log_warn:  priority = ANDROID_LOG_WARN;  break;
        case log_info:  priority = ANDROID_LOG_INFO;  break;
        case log_debug: priority = ANDROID_LOG_DEBUG; break;
        default:        priority = ANDROID_LOG_VERBOSE; break;
    }
    __android_log_vprint(priority, "udp2raw", str, vlist);
    // Also write to log file if set
    if (g_log_file) {
        time_t timer;
        char tbuf[100];
        struct tm* tm_info;
        time(&timer);
        tm_info = localtime(&timer);
        strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", tm_info);
        fprintf(g_log_file, "[%s][%s] ", tbuf, log_text[level]);
        vfprintf(g_log_file, str, vlist);
        fflush(g_log_file);
    }
#else
    time_t timer;
    char buffer[100];
    struct tm* tm_info;

    time(&timer);
    tm_info = localtime(&timer);

    if (enable_log_color)
        printf("%s", log_color[level]);

    strftime(buffer, 100, "%Y-%m-%d %H:%M:%S", tm_info);
    printf("[%s][%s]", buffer, log_text[level]);

    if (enable_log_position) printf("[%s,func:%s,line:%d]", file, function, line);

    vfprintf(stdout, str, vlist);
    if (enable_log_color)
        printf("%s", RESET);

    fflush(stdout);
#endif

    va_end(vlist);

    if (log_level == log_fatal) {
        about_to_exit = 1;
    }
}

void log_bare(int level, const char* str, ...) {
    if (level > log_level) return;
    if (level > log_trace || level < 0) return;

    va_list vlist;
    va_start(vlist, str);

#ifdef __ANDROID__
    int priority;
    switch (level) {
        case log_fatal:
        case log_error: priority = ANDROID_LOG_ERROR; break;
        case log_warn:  priority = ANDROID_LOG_WARN;  break;
        case log_info:  priority = ANDROID_LOG_INFO;  break;
        default:        priority = ANDROID_LOG_DEBUG; break;
    }
    __android_log_vprint(priority, "udp2raw", str, vlist);
    if (g_log_file) {
        vfprintf(g_log_file, str, vlist);
        fflush(g_log_file);
    }
#else
    if (enable_log_color)
        printf("%s", log_color[level]);
    vfprintf(stdout, str, vlist);
    if (enable_log_color)
        printf("%s", RESET);
    fflush(stdout);
#endif

    va_end(vlist);
}