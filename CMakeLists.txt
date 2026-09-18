cmake_minimum_required(VERSION 3.26)
project(twilight_visuals_vanilla CXX)

# The Dusklight version to build against: a release tag (e.g. "v1.5.0") or a commit SHA.
if (NOT DUSKLIGHT_VERSION)
    set(DUSKLIGHT_VERSION "8caab1a6ba4aad5513bc9fe402a76dce53964e9c")
endif ()
# Fetches the Dusklight sources into ./dusklight and provides DUSKLIGHT_DIR.
# Use -DDUSKLIGHT_DIR= to point at an existing Dusklight checkout.
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/FetchDusklight.cmake")
add_subdirectory("${DUSKLIGHT_DIR}/sdk" dusklight-sdk EXCLUDE_FROM_ALL)

add_mod(twilight_visuals
        FEATURES game
        SOURCES
            src/mod.cpp
            src/compat.cpp
            src/sky.cpp
            src/boundary.cpp
            src/blood.cpp
            src/running.cpp
            src/wall_run.cpp
            src/run_trail.cpp
            src/sequencing.cpp
            src/geometry.cpp
            src/monochrome.cpp
            src/hooks.cpp
            src/music.cpp
            src/environment.cpp
            src/postprocess.cpp
            src/particles.cpp
            src/native_particles.cpp
            src/runtime.cpp
            src/settings.cpp
            src/cursor.cpp
            src/menu_scaling.cpp
            src/facial.cpp
            src/native_face_tuner.cpp
        MOD_JSON mod.json
        RES_DIR res
        BUNDLE
)

target_include_directories(twilight_visuals PRIVATE
    "${DUSKLIGHT_DIR}/src"
    "${DUSKLIGHT_DIR}/build/windows-msvc/_deps/rmlui-src/Include"
    "${DUSKLIGHT_DIR}/build/windows-msvc/_deps/sdl3_prebuilt-src/include")

# The generic Windows SDK import library omits J3DAnmBase's virtual destructor.
# Dusklight itself exports it; this import-only library makes the owned custom
# animation objects link without adding or replacing any game implementation.
if (WIN32 AND MSVC)
    set(_twilight_visuals_compat_lib
        "${CMAKE_CURRENT_BINARY_DIR}/twilight_visuals_dusklight_compat.lib")
    add_custom_command(
        OUTPUT "${_twilight_visuals_compat_lib}"
        COMMAND "${CMAKE_AR}" /nologo
                "/def:${CMAKE_CURRENT_SOURCE_DIR}/cmake/dusklight_compat.def"
                /machine:x64
                /name:dusklight.exe
                "/out:${_twilight_visuals_compat_lib}"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/dusklight_compat.def"
        VERBATIM
    )
    add_custom_target(twilight_visuals_dusklight_compat
        DEPENDS "${_twilight_visuals_compat_lib}")
    add_dependencies(twilight_visuals twilight_visuals_dusklight_compat)
    target_link_libraries(twilight_visuals PRIVATE "${_twilight_visuals_compat_lib}")
endif ()
