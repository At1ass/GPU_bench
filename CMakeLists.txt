cmake_minimum_required(VERSION 3.0...3.31)
project(GPU_benchmark VERSION 0.4.0 LANGUAGES C CXX)

# C++11: CMake 3.0 doesn't have CMAKE_CXX_STANDARD
if(CMAKE_VERSION VERSION_LESS "3.1")
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
    endif()
else()
    set(CMAKE_CXX_STANDARD 11)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()

# --- Compiler warnings ---
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wno-unused-parameter)
elseif(MSVC)
    add_compile_options(/W4 /wd4100 /wd4244 /wd4267)
endif()

# --- Dependencies ---
find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

# ImGui from submodule
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/extern/imgui)
add_library(imgui_lib STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# Handle both modern (SDL2::SDL2) and legacy (SDL2_LIBRARIES) CMake SDL2
if(TARGET SDL2::SDL2)
    target_link_libraries(imgui_lib PUBLIC SDL2::SDL2)
else()
    target_include_directories(imgui_lib PUBLIC ${SDL2_INCLUDE_DIRS})
    target_link_libraries(imgui_lib PUBLIC ${SDL2_LIBRARIES})
endif()

if(TARGET OpenGL::GL)
    target_link_libraries(imgui_lib PUBLIC OpenGL::GL)
else()
    target_include_directories(imgui_lib PUBLIC ${OPENGL_INCLUDE_DIR})
    target_link_libraries(imgui_lib PUBLIC ${OPENGL_LIBRARIES})
endif()

# --- Main executable ---
set(SOURCES
    src/main.cpp
    src/app.cpp
    src/gl_funcs.cpp
    src/gl2_renderer.cpp
    src/gl3_renderer.cpp
    src/renderer_factory.cpp
    src/mesh_gen.cpp
    src/bench.cpp
    src/test_fillrate.cpp
    src/test_geometry.cpp
    src/test_texturing.cpp
    src/test_scene.cpp
    src/test_drawcall.cpp
    src/test_overdraw.cpp
    src/test_texupload.cpp
    src/test_statechange.cpp
    src/test_vertex.cpp
    src/test_shader_alu.cpp
    src/test_shader_fma.cpp
    src/test_drawcall_raw.cpp
    src/gpu_timer.cpp
    src/hwinfo.cpp
    src/preset.cpp
    src/config.cpp
    src/results.cpp
    src/gpu_select.cpp
    src/logger.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

add_executable(gpu_benchmark ${SOURCES})
target_include_directories(gpu_benchmark PRIVATE src)

# --- SDL2 linking ---
if(TARGET SDL2::SDL2)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(gpu_benchmark PRIVATE SDL2::SDL2 SDL2::SDL2main imgui_lib)
    else()
        target_link_libraries(gpu_benchmark PRIVATE SDL2::SDL2 imgui_lib)
    endif()
else()
    target_include_directories(gpu_benchmark PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(gpu_benchmark PRIVATE ${SDL2_LIBRARIES} imgui_lib)
endif()

# --- OpenGL linking ---
if(TARGET OpenGL::GL)
    target_link_libraries(gpu_benchmark PRIVATE OpenGL::GL)
else()
    target_link_libraries(gpu_benchmark PRIVATE ${OPENGL_LIBRARIES})
endif()

if(UNIX AND NOT APPLE)
    target_link_libraries(gpu_benchmark PRIVATE dl pthread)
endif()

if(WIN32)
    target_link_libraries(gpu_benchmark PRIVATE advapi32)
    # MinGW: statically link C/C++ runtime to reduce DLL dependencies.
    # libwinpthread-1.dll is still needed (posix thread model) — ship it alongside the exe.
    if(MINGW)
        target_link_libraries(gpu_benchmark PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()

# --- Install ---
install(TARGETS gpu_benchmark RUNTIME DESTINATION bin)
