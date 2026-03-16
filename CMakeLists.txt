cmake_minimum_required(VERSION 3.0...3.31)
project(GPU_benchmark VERSION 0.5.0 LANGUAGES C CXX)

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
    # App
    src/main.cpp
    src/app.cpp

    # Renderer
    src/renderer/gl_funcs.cpp
    src/renderer/gl2_renderer.cpp
    src/renderer/gl3_renderer.cpp
    src/renderer/gl4_renderer.cpp
    src/renderer/gles_renderer.cpp
    src/renderer/render_context.cpp
    src/renderer/gl_render_context.cpp
    src/renderer/gl_loader.cpp
    src/renderer/renderer_factory.cpp
    src/renderer/gpu_timer.cpp

    # Geometry
    src/geometry/mesh_gen.cpp

    # Bench engine
    src/bench/bench.cpp
    src/bench/bench_runner.cpp
    src/bench/stress_runner.cpp
    src/bench/preset.cpp
    src/bench/results.cpp
    src/bench/preset_io.cpp

    # Tests
    src/tests/test_registry.cpp
    src/tests/test_fillrate.cpp
    src/tests/test_geometry.cpp
    src/tests/test_texturing.cpp
    src/tests/test_scene.cpp
    src/tests/test_drawcall.cpp
    src/tests/test_drawcall_raw.cpp
    src/tests/test_overdraw.cpp
    src/tests/test_texupload.cpp
    src/tests/test_statechange.cpp
    src/tests/test_vertex.cpp
    src/tests/test_shader_alu.cpp
    src/tests/test_shader_fma.cpp
    src/tests/test_instanced_draw.cpp
    src/tests/test_compute_fma.cpp
    src/tests/test_fbo_fillrate.cpp
    src/tests/test_mrt_fill.cpp
    src/tests/test_texarray_sample.cpp
    src/tests/test_geom_shader.cpp
    src/tests/test_ubo_switch.cpp
    src/tests/test_transform_feedback.cpp
    src/tests/test_compute_bw.cpp
    src/tests/test_indirect_draw.cpp
    src/tests/test_compute_smem.cpp
    src/tests/test_ssbo_atomics.cpp
    src/tests/test_tessellation.cpp
    src/tests/test_texture_gather.cpp
    src/tests/test_image_load_store.cpp
    src/tests/test_compute_prefix.cpp
    src/tests/test_persistent_map.cpp
    src/tests/test_bindless_tex.cpp

    # Demo
    src/demo/demo_camera.cpp
    src/demo/demo_shaders.cpp
    src/demo/demo_scene.cpp
    src/demo/demo_runner.cpp
    src/demo/demo_ui.cpp
    src/demo/demo_export.cpp

    # UI
    src/ui/bench_ui.cpp

    # Platform
    src/platform/hwinfo.cpp
    src/platform/gpu_select.cpp
    src/platform/logger.cpp

    # ImGui backend
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

if(APPLE)
    target_link_libraries(gpu_benchmark PRIVATE pthread)
elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
    target_link_libraries(gpu_benchmark PRIVATE pthread)
elseif(UNIX)
    target_link_libraries(gpu_benchmark PRIVATE dl pthread)
endif()

if(WIN32)
    target_link_libraries(gpu_benchmark PRIVATE advapi32)
    # Force console subsystem so stderr/stdout work in terminal.
    # SDL2's .dll.a pulls -mwindows (GUI subsystem) which suppresses console I/O.
    # We need console for --headless mode and log output.
    if(MINGW)
        target_link_libraries(gpu_benchmark PRIVATE -mconsole -static-libgcc -static-libstdc++)
    elseif(MSVC)
        set_target_properties(gpu_benchmark PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
    endif()
endif()

# --- Install ---
install(TARGETS gpu_benchmark RUNTIME DESTINATION bin)
