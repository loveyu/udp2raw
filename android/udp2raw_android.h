#pragma once
#ifdef __ANDROID__

/**
 * Exception thrown by myexit() when running as an Android JNI shared library.
 * Replaces exit() so the JVM process is not killed on fatal errors.
 */
struct Udp2RawExitException {
    int code;
    explicit Udp2RawExitException(int c) : code(c) {}
};

#endif  // __ANDROID__
