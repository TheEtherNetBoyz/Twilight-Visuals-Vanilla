cmake_minimum_required(VERSION 3.26)
project(twilight_visuals_vanilla CXX)

# The Dusklight version to build against: a release tag (e.g. "v1.5.0") or a commit SHA.
if (NOT DUSKLIGHT_VERSION)
    set(DUSKLIGHT_VERSION "edf42c6a7202647b56dd2fcdef02d17671bc814b")
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
            src/load_acceleration.cpp
            src/hotkeys.cpp
            src/platform.cpp
        MOD_JSON mod.json
        RES_DIR res
        BUNDLE
)

set(DUSKLIGHT_BUILD_DIR "" CACHE PATH
    "Dusklight build directory containing generated dependency headers")

if (NOT DUSKLIGHT_BUILD_DIR)
    if (WIN32)
        set(DUSKLIGHT_BUILD_DIR "${DUSKLIGHT_DIR}/build/windows-msvc" CACHE PATH
            "Dusklight build directory containing generated dependency headers" FORCE)
    elseif (APPLE)
        set(DUSKLIGHT_BUILD_DIR "${DUSKLIGHT_DIR}/build/macos-default-relwithdebinfo" CACHE PATH
            "Dusklight build directory containing generated dependency headers" FORCE)
    else ()
        set(DUSKLIGHT_BUILD_DIR "${DUSKLIGHT_DIR}/build/linux-default-relwithdebinfo" CACHE PATH
            "Dusklight build directory containing generated dependency headers" FORCE)
    endif ()
endif ()

if (WIN32)
    set(_twilight_visuals_sdl_include_dir
        "${DUSKLIGHT_BUILD_DIR}/_deps/sdl3_prebuilt-src/include")
elseif (APPLE)
    set(_twilight_visuals_sdl_include_dir
        "${DUSKLIGHT_BUILD_DIR}/_deps/sdl-src/include")
else ()
    set(_twilight_visuals_sdl_include_dir
        "${DUSKLIGHT_BUILD_DIR}/_deps/sdl-src/include")
endif ()

target_include_directories(twilight_visuals PRIVATE
    "${DUSKLIGHT_DIR}/src"
    "${DUSKLIGHT_BUILD_DIR}/_deps/rmlui-src/Include"
    "${_twilight_visuals_sdl_include_dir}")

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
