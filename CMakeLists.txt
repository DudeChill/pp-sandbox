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
    src/sim/material.cpp
    src/sim/ragdoll.cpp
    src/sim/rope.cpp
    src/sim/alloc.cpp
)
target_include_directories(pp_sim PUBLIC src)

enable_testing()

add_executable(pp_tests tests/test_main.cpp tests/world_test.cpp)
target_link_libraries(pp_tests PRIVATE pp_sim)
add_test(NAME core COMMAND pp_tests)

add_executable(pp_bench tools/bench.cpp)
target_link_libraries(pp_bench PRIVATE pp_sim)

add_executable(pp_make_wiring tools/make_wiring.cpp)
target_link_libraries(pp_make_wiring PRIVATE pp_sim)

option(PP_BUILD_APP "Build the SDL2 application" ON)
set(PP_SDL2_ROOT "" CACHE PATH "Optional root of an extracted SDL2 development package")
set(PP_SDL2_LIBRARY "" CACHE FILEPATH "Optional explicit SDL2 library path (fallback)")

set(PP_SDL2_TARGET "")
find_package(SDL2 CONFIG QUIET)
if(TARGET SDL2::SDL2)
    set(PP_SDL2_TARGET SDL2::SDL2)
elseif(TARGET SDL2::SDL2-static)
    set(PP_SDL2_TARGET SDL2::SDL2-static)
elseif(PP_SDL2_ROOT AND EXISTS "${PP_SDL2_ROOT}/usr/include/SDL2/SDL.h")
    add_library(pp_sdl2_manual INTERFACE)
    target_include_directories(pp_sdl2_manual INTERFACE
        "${PP_SDL2_ROOT}/usr/include"
        "${PP_SDL2_ROOT}/usr/include/SDL2"
        "${PP_SDL2_ROOT}/usr/include/x86_64-linux-gnu"
    )
    if(PP_SDL2_LIBRARY)
        target_link_libraries(pp_sdl2_manual INTERFACE "${PP_SDL2_LIBRARY}")
    else()
        target_link_libraries(pp_sdl2_manual INTERFACE SDL2)
    endif()
    set(PP_SDL2_TARGET pp_sdl2_manual)
endif()

if(PP_BUILD_APP AND PP_SDL2_TARGET AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/app/main.cpp")
    add_executable(pp
        src/app/main.cpp
        src/render/renderer.cpp
        src/render/gl_api.cpp
    )
    target_include_directories(pp PRIVATE src)
    target_link_libraries(pp PRIVATE pp_sim ${PP_SDL2_TARGET})
    if(UNIX AND NOT APPLE)
        target_link_libraries(pp PRIVATE dl)
    endif()
elseif(PP_BUILD_APP)
    message(STATUS "SDL2 not found: the game target is skipped (set SDL2_DIR, PP_SDL2_ROOT, or use vcpkg).")
endif()
