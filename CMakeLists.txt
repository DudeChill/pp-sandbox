cmake_minimum_required(VERSION 3.25)
project(pp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/W4 /permissive- /fp:precise /Zc:__cplusplus)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wconversion
                        -Wno-sign-conversion -ffp-contract=off -fno-fast-math)
endif()

add_library(pp_sim STATIC
    src/sim/world.cpp
    src/sim/serialize.cpp
    src/sim/ammo.cpp
    src/sim/weapon.cpp
    src/sim/material.cpp
    src/sim/ragdoll.cpp
    src/sim/rope.cpp
    src/sim/cloth.cpp
    src/sim/alloc.cpp
)
target_include_directories(pp_sim PUBLIC src)
find_package(Threads REQUIRED)
target_link_libraries(pp_sim PUBLIC Threads::Threads)

enable_testing()

add_executable(pp_tests tests/test_main.cpp tests/world_test.cpp)
target_link_libraries(pp_tests PRIVATE pp_sim)
add_test(NAME core COMMAND pp_tests)

add_executable(pp_bench tools/bench.cpp)
target_link_libraries(pp_bench PRIVATE pp_sim)

add_executable(pp_make_wiring tools/make_wiring.cpp)
target_link_libraries(pp_make_wiring PRIVATE pp_sim)

option(PP_BUILD_APP "Build the SDL2 application" ON)
set(PP_SDL2_ROOT "" CACHE PATH "Root of an extracted SDL2 development package")
set(PP_SDL2_LIBRARY "" CACHE FILEPATH "Explicit SDL2 library path (optional)")
option(PP_FETCH_SDL2 "Download prebuilt SDL2 on Windows when it is not installed" ON)

# Convenience fallback: scripts/setup_deps.sh installs headers here without root.
if(NOT PP_SDL2_ROOT AND EXISTS "$ENV{HOME}/.local/opt/ppdeps/usr/include/SDL2/SDL.h")
    set(PP_SDL2_ROOT "$ENV{HOME}/.local/opt/ppdeps" CACHE PATH
        "Root of an extracted SDL2 development package" FORCE)
endif()

set(PP_SDL2_TARGET "")
if(PP_SDL2_ROOT AND EXISTS "${PP_SDL2_ROOT}/usr/include/SDL2/SDL.h")
    # Extracted distro package: the multiarch config header lives next to the
    # normal headers, and there is no SDL2Config.cmake on the search path.
    add_library(pp_sdl2_manual INTERFACE)
    target_include_directories(pp_sdl2_manual INTERFACE
        "${PP_SDL2_ROOT}/usr/include"
        "${PP_SDL2_ROOT}/usr/include/SDL2"
        "${PP_SDL2_ROOT}/usr/include/${CMAKE_LIBRARY_ARCHITECTURE}"
    )
    if(PP_SDL2_LIBRARY)
        target_link_libraries(pp_sdl2_manual INTERFACE "${PP_SDL2_LIBRARY}")
    else()
        set(_pp_suffixes ${CMAKE_FIND_LIBRARY_SUFFIXES})
        if(UNIX AND NOT APPLE)
            set(CMAKE_FIND_LIBRARY_SUFFIXES ".so")
        endif()
        find_library(PP_SDL2_LIBRARY_AUTO NAMES SDL2 SDL2-2.0
            HINTS
                "${PP_SDL2_ROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
                "${PP_SDL2_ROOT}/usr/lib")
        if(NOT PP_SDL2_LIBRARY_AUTO)
            # A runtime-only install has libSDL2-2.0.so.0 but no -dev symlink.
            find_file(PP_SDL2_RUNTIME_AUTO NAMES libSDL2-2.0.so.0 libSDL2.so.0
                HINTS
                    "${PP_SDL2_ROOT}/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}"
                    "/usr/lib/${CMAKE_LIBRARY_ARCHITECTURE}")
        endif()
        set(CMAKE_FIND_LIBRARY_SUFFIXES ${_pp_suffixes})
        if(PP_SDL2_LIBRARY_AUTO)
            target_link_libraries(pp_sdl2_manual INTERFACE "${PP_SDL2_LIBRARY_AUTO}")
        elseif(PP_SDL2_RUNTIME_AUTO)
            target_link_libraries(pp_sdl2_manual INTERFACE "${PP_SDL2_RUNTIME_AUTO}")
        else()
            target_link_libraries(pp_sdl2_manual INTERFACE SDL2)
        endif()
    endif()
    set(PP_SDL2_TARGET pp_sdl2_manual)
else()
    find_package(SDL2 CONFIG QUIET)
    if(TARGET SDL2::SDL2)
        set(PP_SDL2_TARGET SDL2::SDL2)
    elseif(TARGET SDL2::SDL2-static)
        set(PP_SDL2_TARGET SDL2::SDL2-static)
    endif()
    if(NOT PP_SDL2_TARGET)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL2_PC QUIET IMPORTED_TARGET sdl2)
            if(TARGET PkgConfig::SDL2_PC)
                set(PP_SDL2_TARGET PkgConfig::SDL2_PC)
            endif()
        endif()
    endif()
endif()

if(PP_BUILD_APP AND NOT PP_SDL2_TARGET AND WIN32 AND PP_FETCH_SDL2)
    set(PP_SDL2_URL
        "https://github.com/libsdl-org/SDL/releases/download/release-2.32.10/SDL2-devel-2.32.10-VC.zip")
    set(PP_SDL2_SHA256 "af347939395a58b365846aaea27391e69f9ec9d4dd650d6ac40802159b418a6e")
    set(_pp_sdl_archive "${CMAKE_BINARY_DIR}/_deps/SDL2-devel-2.32.10-VC.zip")
    set(_pp_sdl_root "${CMAKE_BINARY_DIR}/_deps/SDL2-2.32.10")
    if(NOT EXISTS "${_pp_sdl_root}/include/SDL.h")
        message(STATUS "SDL2 not installed; downloading the prebuilt VC package...")
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/_deps")
        file(DOWNLOAD "${PP_SDL2_URL}" "${_pp_sdl_archive}"
            EXPECTED_HASH "SHA256=${PP_SDL2_SHA256}"
            TIMEOUT 180
            STATUS _pp_dl_status)
        list(GET _pp_dl_status 0 _pp_dl_code)
        if(NOT _pp_dl_code EQUAL 0)
            list(GET _pp_dl_status 1 _pp_dl_message)
            message(FATAL_ERROR "SDL2 download failed: ${_pp_dl_message}")
        endif()
        file(ARCHIVE_EXTRACT INPUT "${_pp_sdl_archive}" DESTINATION "${CMAKE_BINARY_DIR}/_deps")
    endif()
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(_pp_sdl_arch x64)
    else()
        set(_pp_sdl_arch x86)
    endif()
    add_library(pp_sdl2_fetched INTERFACE)
    target_include_directories(pp_sdl2_fetched INTERFACE "${_pp_sdl_root}/include")
    target_link_libraries(pp_sdl2_fetched INTERFACE
        "${_pp_sdl_root}/lib/${_pp_sdl_arch}/SDL2.lib")
    set(PP_SDL2_RUNTIME_DLL "${_pp_sdl_root}/lib/${_pp_sdl_arch}/SDL2.dll")
    set(PP_SDL2_TARGET pp_sdl2_fetched)
endif()

if(PP_BUILD_APP AND NOT PP_SDL2_TARGET)
    message(FATAL_ERROR
        "SDL2 development files were not found.\n"
        "Linux: run ./scripts/setup_deps.sh (no root) or install libsdl2-dev.\n"
        "Windows: install SDL2 with vcpkg and pass the toolchain, or run run.cmd.\n"
        "Tests only: configure with -DPP_BUILD_APP=OFF.")
endif()

if(PP_BUILD_APP AND PP_SDL2_TARGET AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/app/main.cpp")
    add_executable(pp
        src/app/main.cpp
        src/render/renderer.cpp
        src/render/gl_api.cpp
        src/app/audio.cpp
    )
    target_include_directories(pp PRIVATE src)
    target_link_libraries(pp PRIVATE pp_sim ${PP_SDL2_TARGET})
    if(UNIX AND NOT APPLE)
        target_link_libraries(pp PRIVATE dl)
    endif()
    if(PP_SDL2_RUNTIME_DLL)
        add_custom_command(TARGET pp POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${PP_SDL2_RUNTIME_DLL}" "$<TARGET_FILE_DIR:pp>/SDL2.dll")
    endif()
endif()
