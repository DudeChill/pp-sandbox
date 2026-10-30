cmake_minimum_required(VERSION 3.25)
project(pp LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(PP_WARNINGS -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wno-sign-conversion)
set(PP_FP_FLAGS -ffp-contract=off -fno-fast-math)

add_library(pp_sim STATIC
    src/sim/world.cpp
    src/sim/serialize.cpp
    src/sim/material.cpp
    src/sim/ragdoll.cpp
    src/sim/rope.cpp
    src/sim/alloc.cpp
)
target_include_directories(pp_sim PUBLIC src)
target_compile_options(pp_sim PRIVATE ${PP_WARNINGS} ${PP_FP_FLAGS})

enable_testing()

add_executable(pp_tests tests/test_main.cpp tests/world_test.cpp)
target_compile_options(pp_tests PRIVATE ${PP_WARNINGS} ${PP_FP_FLAGS})
target_link_libraries(pp_tests PRIVATE pp_sim)
add_test(NAME core COMMAND pp_tests)

option(PP_BUILD_APP "Build the SDL2 application" ON)
set(PP_SDL2_ROOT "$ENV{PP_SDL2_ROOT}" CACHE PATH "Extracted SDL2 dev package root")
set(PP_SDL2_LIBRARY "/usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0" CACHE FILEPATH "SDL2 shared library")

if(PP_BUILD_APP AND EXISTS "${PP_SDL2_ROOT}/usr/include/SDL2/SDL.h"
   AND EXISTS "${PP_SDL2_LIBRARY}" AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/app/main.cpp")
    add_executable(pp
        src/app/main.cpp
        src/render/renderer.cpp
        src/render/gl_api.cpp
    )
    target_include_directories(pp PRIVATE
        src
        "${PP_SDL2_ROOT}/usr/include"
        "${PP_SDL2_ROOT}/usr/include/x86_64-linux-gnu"
    )
    target_compile_options(pp PRIVATE ${PP_WARNINGS} ${PP_FP_FLAGS})
    target_link_libraries(pp PRIVATE pp_sim "${PP_SDL2_LIBRARY}" dl)
endif()
