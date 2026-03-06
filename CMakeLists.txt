cmake_minimum_required(VERSION 3.20)
project(implc)
set(CMAKE_CXX_STANDARD 20)

set(EXAMPLE_DIRS
        "examples/sdl_gpu_voxel"
        "examples/sdl_gpu_triangle"
        "examples/sdl_cpu_triangle"
        "examples/sdl_cpu_model"
        "examples/sdl_gpu_model"
        "examples/x11_model"
)

# Architecture (Apple)
if(APPLE)
    if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64" AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architecture" FORCE)
    endif()
endif()

# SDL3
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS        OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES     OFF CACHE BOOL "" FORCE)
add_subdirectory(wrapper/libs/SDL EXCLUDE_FROM_ALL)

# ImGui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/wrapper/libs/imgui)
add_library(imgui STATIC
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_demo.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp
        ${IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

# Optional library detection
# Each lib exposes: OPT_<NAME>_FOUND, OPT_<NAME>_INCLUDE_DIR, OPT_<NAME>_LINK

# X11 / XQuartz
if(APPLE)
    set(_X11_ROOT "/opt/X11")
    if(NOT EXISTS "${_X11_ROOT}/lib/libX11.dylib")
        set(_X11_ROOT "/usr/X11")   # older XQuartz fallback
    endif()
    if(EXISTS "${_X11_ROOT}/lib/libX11.dylib" OR EXISTS "${_X11_ROOT}/lib/libX11.a")
        set(OPT_X11_FOUND       TRUE)
        set(OPT_X11_INCLUDE_DIR "${_X11_ROOT}/include")
        set(OPT_X11_LINK        "-L${_X11_ROOT}/lib;-lX11")
        message(STATUS "X11: found XQuartz at ${_X11_ROOT}")
    else()
        message(STATUS "X11: XQuartz not found — x11 examples will be skipped (https://www.xquartz.org)")
    endif()
else()
    find_package(X11 QUIET)
    if(X11_FOUND)
        set(OPT_X11_FOUND       TRUE)
        set(OPT_X11_INCLUDE_DIR "${X11_INCLUDE_DIR}")
        set(OPT_X11_LINK        "${X11_LIBRARIES}")
    endif()
endif()

# Vulkan
find_package(Vulkan QUIET)
if(Vulkan_FOUND)
    set(OPT_VULKAN_FOUND       TRUE)
    set(OPT_VULKAN_INCLUDE_DIR "${Vulkan_INCLUDE_DIRS}")
    set(OPT_VULKAN_LINK        "${Vulkan_LIBRARIES}")
endif()

# OpenGL
find_package(OpenGL QUIET)
if(OpenGL_FOUND OR OPENGL_FOUND)
    set(OPT_OPENGL_FOUND       TRUE)
    set(OPT_OPENGL_INCLUDE_DIR "${OPENGL_INCLUDE_DIR}")
    set(OPT_OPENGL_LINK        "${OPENGL_LIBRARIES}")
    if(APPLE)
        list(APPEND OPT_OPENGL_LINK "-framework OpenGL")
    endif()
endif()

# OpenCL
find_package(OpenCL QUIET)
if(OpenCL_FOUND)
    set(OPT_OPENCL_FOUND       TRUE)
    set(OPT_OPENCL_INCLUDE_DIR "${OpenCL_INCLUDE_DIRS}")
    set(OPT_OPENCL_LINK        "${OpenCL_LIBRARIES}")
endif()

# Metal (Apple only — always available, no detection needed)
if(APPLE)
    set(OPT_METAL_FOUND TRUE)
    set(OPT_METAL_LINK  "-framework Metal;-framework MetalKit;-framework Foundation;-framework QuartzCore")
endif()

# Helper: collect all source text for a file, recursively following local
# #include "..." directives up to a bounded depth.  This ensures that
# libraries pulled in via wrapper headers (e.g. core.h → <X11/Xlib.h>)
# are visible to the pattern matcher even though they live in headers.
function(_collect_source_text root_file search_dirs depth out_text)
    if(depth LESS 1)
        set(${out_text} "" PARENT_SCOPE)
        return()
    endif()

    file(READ "${root_file}" _text)

    # Find every   #include "something"   in this file
    string(REGEX MATCHALL "#include[ \t]+\"[^\"]+\"" _local_includes "${_text}")
    foreach(_inc_line ${_local_includes})
        string(REGEX REPLACE "#include[ \t]+\"([^\"]+)\"" "\\1" _inc_name "${_inc_line}")
        # Try each search dir until we find the file
        foreach(_dir ${search_dirs})
            set(_candidate "${_dir}/${_inc_name}")
            if(EXISTS "${_candidate}")
                math(EXPR _next_depth "${depth} - 1")
                _collect_source_text("${_candidate}" "${search_dirs}" "${_next_depth}" _sub_text)
                string(APPEND _text "\n${_sub_text}")
                break()
            endif()
        endforeach()
    endforeach()

    set(${out_text} "${_text}" PARENT_SCOPE)
endfunction()

# Helper: detect which optional libraries a source file needs.
#
# Detection rules (checked against the fully expanded source + headers):
#
#   X11    — core.h is included AND SDL_IMPLEMENTATION is NOT defined
#             (core.h routes to X11 when SDL_IMPLEMENTATION is absent)
#   Vulkan — explicit #include <vulkan/...> or <Vulkan/...>
#   OpenGL — explicit #include <GL/...> or <OpenGL/...>
#   OpenCL — explicit #include <CL/...>
#   Metal  — explicit #include/#import <Metal/...>
#
# Sets out_includes, out_links, out_skip (TRUE = missing required lib).
function(detect_optional_libs source_file out_includes out_links out_skip)
    # Build search dirs: directory of the source file + wrapper root
    get_filename_component(_src_dir "${source_file}" DIRECTORY)
    set(_search_dirs "${_src_dir}" "${CMAKE_CURRENT_SOURCE_DIR}/wrapper")

    # Collect fully expanded text (recurse 4 levels deep — enough for core.h)
    _collect_source_text("${source_file}" "${_search_dirs}" 4 _full_src)

    set(_inc  "")
    set(_lnk  "")
    set(_skip FALSE)

    # --- X11 ---
    # core.h includes <X11/Xlib.h> when SDL_IMPLEMENTATION is not defined.
    # So we trigger X11 when the source includes core.h but does NOT define
    # SDL_IMPLEMENTATION anywhere in the translation unit.
    if(_full_src MATCHES "#include.*[\"<][^\">/]*core\\.h[\">]" AND
            NOT _full_src MATCHES "#define[ \t]+SDL_IMPLEMENTATION")
        if(OPT_X11_FOUND)
            list(APPEND _inc "${OPT_X11_INCLUDE_DIR}")
            list(APPEND _lnk "${OPT_X11_LINK}")
            message(STATUS "  [x11_model] X11 detected via core.h (no SDL_IMPLEMENTATION)")
        else()
            message(STATUS "  -> requires X11 (not found) — skipping")
            set(_skip TRUE)
        endif()
    endif()

    # Generic pattern-based checks for everything else
    macro(check_lib NAME PATTERN)
        if(_full_src MATCHES "${PATTERN}")
            if(OPT_${NAME}_FOUND)
                if(DEFINED OPT_${NAME}_INCLUDE_DIR)
                    list(APPEND _inc "${OPT_${NAME}_INCLUDE_DIR}")
                endif()
                list(APPEND _lnk "${OPT_${NAME}_LINK}")
            else()
                message(STATUS "  -> requires ${NAME} (not found) — skipping")
                set(_skip TRUE)
            endif()
        endif()
    endmacro()

    check_lib(VULKAN  "#include[ \t]*<[Vv]ulkan/")
    check_lib(OPENGL  "#include[ \t]*<(GL|OpenGL)/")
    check_lib(OPENCL  "#include[ \t]*<CL/")
    check_lib(METAL   "[#include|#import][ \t]*<Metal/")

    set(${out_includes} "${_inc}"  PARENT_SCOPE)
    set(${out_links}    "${_lnk}"  PARENT_SCOPE)
    set(${out_skip}     "${_skip}" PARENT_SCOPE)
endfunction()

# Build each example
foreach(example_dir ${EXAMPLE_DIRS})
    get_filename_component(example_name ${example_dir} NAME)
    set(source_file "${CMAKE_CURRENT_SOURCE_DIR}/${example_dir}/${example_name}.c")

    if(NOT EXISTS "${source_file}")
        message(WARNING "Skipping '${example_name}': source not found at ${source_file}")
        continue()
    endif()

    message(STATUS "Configuring example: ${example_name}")

    detect_optional_libs("${source_file}" _extra_inc _extra_lnk _skip)
    if(_skip)
        continue()
    endif()

    add_executable(${example_name} ${source_file})
    set_source_files_properties(${source_file} PROPERTIES LANGUAGE CXX)

    target_include_directories(${example_name} PRIVATE
            wrapper
            wrapper/libs/imgui
            wrapper/libs/imgui/backends
            ${_extra_inc}
    )

    target_link_libraries(${example_name} PRIVATE
            imgui
            SDL3::SDL3
            ${_extra_lnk}
    )

    # Apple arm64 optimizations
    if(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        target_compile_options(${example_name} PRIVATE
                -O3
                -march=armv8.5-a
                -mtune=apple-m2
                -ffast-math
        )
    endif()
endforeach()

# Shader compilation
add_custom_target(generate_shaders
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_shaders.sh ${CMAKE_BINARY_DIR}/shaders
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating GPU shaders (SPIR-V + optional MSL/DXIL via SDL_shadercross)"
        VERBATIM
)

foreach(example_dir ${EXAMPLE_DIRS})
    get_filename_component(example_name ${example_dir} NAME)
    if(TARGET ${example_name})
        add_dependencies(${example_name} generate_shaders)
    endif()
endforeach()