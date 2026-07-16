cmake_minimum_required(VERSION 3.22.1)
project(dreamengine C)

# DE_REPO = the dreamengine repo root, passed from app/build.gradle. The engine + the
# generated cart live OUTSIDE the android/ tree (shared with every other target); this
# host only adds main.c. Mirrors the source list in tools/build-nr.sh.
if(NOT DEFINED DE_REPO)
    message(FATAL_ERROR "DE_REPO not set (pass -DDE_REPO=<repo root> from Gradle)")
endif()

# The generated cart (cart.c + sprites_data.h + map_data.h) is STAGED into a private
# gen/ dir by android/build.sh — NOT read from the shared build/ dir, which sibling
# processes (the editor, other agents) overwrite (mirrors ios/gen/app). Default to the
# committed placeholder-free gen/ next to this file.
if(NOT DEFINED DE_GEN)
    set(DE_GEN ${CMAKE_CURRENT_SOURCE_DIR}/gen)
endif()

# Android's native_app_glue (bundled in the NDK) — the NativeActivity <-> C bridge.
add_library(native_app_glue STATIC
    ${ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c)
target_include_directories(native_app_glue PUBLIC
    ${ANDROID_NDK}/sources/android/native_app_glue)

add_library(cart SHARED
    main.c
    ${DE_REPO}/runtime/studio.c
    ${DE_REPO}/runtime/raylib_compat.c
    ${DE_GEN}/cart.c)

target_include_directories(cart PRIVATE
    ${DE_REPO}/runtime
    ${DE_GEN}
    ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(cart PRIVATE
    DE_NO_RAYLIB
    SCREEN_W=${DE_SCREEN_W} SCREEN_H=${DE_SCREEN_H} SCALE=1
    MAP_W=${DE_MAP_W} MAP_H=${DE_MAP_H}
    CELL_W=${DE_CELL_W} CELL_H=${DE_CELL_H})

target_compile_options(cart PRIVATE -O2)

# Keep ANativeActivity_onCreate (native_app_glue's entry) from being stripped.
set_target_properties(cart PROPERTIES LINK_FLAGS "-u ANativeActivity_onCreate")

target_link_libraries(cart
    native_app_glue
    android      # ANativeWindow, input
    log          # __android_log_print
    EGL GLESv2   # the framebuffer blit
    aaudio       # audio
    m)
