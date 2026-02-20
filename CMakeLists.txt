cmake_minimum_required(VERSION 3.20)
project(implc)
set(CMAKE_CXX_STANDARD 20)

# SDL3
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
add_subdirectory(ext/libs/SDL EXCLUDE_FROM_ALL)

# ImGui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ext/libs/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

# Particle demo
add_executable(implc main.c)
set_source_files_properties(main.c PROPERTIES LANGUAGE CXX)  # compile .c as C++
target_include_directories(implc PRIVATE ext ext/libs/imgui ext/libs/imgui/backends)
target_link_libraries(implc PRIVATE imgui SDL3::SDL3)

if(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    target_compile_options(implc PRIVATE
        -O3
        -march=armv8.5-a
        -mtune=apple-m2
        -ffast-math
    )
endif()
