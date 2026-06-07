cmake_minimum_required(VERSION 3.22)
project(udp2raw_plugin)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files shared with the desktop build
set(UDP2RAW_SOURCES
    ../main.cpp
    ../lib/md5.cpp
    ../lib/pbkdf2-sha1.cpp
    ../lib/pbkdf2-sha256.cpp
    ../encrypt.cpp
    ../log.cpp
    ../network.cpp
    ../common.cpp
    ../connection.cpp
    ../misc.cpp
    ../fd_manager.cpp
    ../client.cpp
    ../server.cpp
    ../lib/aes_faster_c/aes.cpp
    ../lib/aes_faster_c/wrapper.cpp
    ../my_ev.cpp
    jni_bridge.cpp
)

# Generate git_version.h if not present
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../git_version.h")
    file(WRITE "${CMAKE_CURRENT_SOURCE_DIR}/../git_version.h"
         "const char *gitversion = \"unknown\";\n")
endif()

add_library(udp2raw_plugin SHARED ${UDP2RAW_SOURCES})

target_include_directories(udp2raw_plugin PRIVATE
    ..
    ../libev
)

target_compile_definitions(udp2raw_plugin PRIVATE
    UDP2RAW_LINUX   # enables raw socket support (client mode)
    __ANDROID__     # already set by NDK, but explicit for clarity
)

target_compile_options(udp2raw_plugin PRIVATE
    -Wall
    -Wextra
    -Wno-unused-variable
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -O2
    -fvisibility=hidden
)

# Link against Android system libraries
target_link_libraries(udp2raw_plugin
    android
    log        # __android_log_print / __android_log_vprint
)
