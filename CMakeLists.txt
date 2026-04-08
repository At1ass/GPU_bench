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
    add_compile_options(
        -Wall -Wextra -Wshadow
        -Wconversion -Wsign-conversion -Wdouble-promotion
        -Wformat=2 -Wundef -Woverloaded-virtual
        -Wno-unused-parameter
    )
    # GCC-specific warnings
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options(-Wnull-dereference -Wduplicated-cond -Wduplicated-branches)
    endif()
elseif(MSVC)
    add_compile_options(/W4 /wd4100 /wd4244 /wd4267)
    add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

# --- Release optimizations ---
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # -O3 + LTO for release builds; x86-64-v2 covers SSE4.2+ (~2010 CPUs)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
    if(NOT CMAKE_CROSSCOMPILING AND CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64|i686")
        set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=x86-64-v2")
    endif()
    # Link-Time Optimization
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

# --- Portable static build option ---
option(PORTABLE_BUILD "Static-link SDL2 and libstdc++ for portable binary" OFF)

# --- Dependencies ---
if(PORTABLE_BUILD AND NOT WIN32)
    # Build SDL2 from source as static library
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST OFF CACHE BOOL "" FORCE)
    set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)

    # Disable everything we don't need — only video + events + GL
    set(SDL_AUDIO OFF CACHE BOOL "" FORCE)
    set(SDL_HAPTIC OFF CACHE BOOL "" FORCE)
    set(SDL_SENSOR OFF CACHE BOOL "" FORCE)
    set(SDL_JOYSTICK OFF CACHE BOOL "" FORCE)
    set(SDL_POWER OFF CACHE BOOL "" FORCE)
    set(SDL_FILESYSTEM OFF CACHE BOOL "" FORCE)
    set(SDL_LOCALE OFF CACHE BOOL "" FORCE)
    set(SDL_RENDER OFF CACHE BOOL "" FORCE)

    # Disable unused video backends (keep X11, Wayland, KMSDRM, Offscreen)
    set(SDL_DIRECTFB OFF CACHE BOOL "" FORCE)
    set(SDL_VIVANTE OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(sdl2
        URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.12/SDL2-2.30.12.tar.gz
        URL_HASH SHA256=ac356ea55e8b9dd0b2d1fa27da40ef7e238267ccf9324704850d5d47375b48ea
    )
    FetchContent_MakeAvailable(sdl2)
    # Static SDL2 target is SDL2-static
else()
    find_package(SDL2 REQUIRED)
endif()
find_package(OpenGL REQUIRED)

# ImGui from submodule
set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/extern/imgui)
add_library(imgui_lib STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl2.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui_lib PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
)

# Handle SDL2 linking: portable (static from source) vs system (shared)
if(PORTABLE_BUILD AND TARGET SDL2-static)
    target_link_libraries(imgui_lib PUBLIC SDL2-static)
elseif(TARGET SDL2::SDL2)
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

# Suppress warnings for third-party code (ImGui)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(imgui_lib PRIVATE -w)
endif()

# --- Core library (renderer, platform, geometry) ---
set(CORE_LIB_SOURCES
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
    src/renderer/gl_extensions.cpp
    src/renderer/gl_debug.cpp
    src/engine/state_cache.cpp
    src/geometry/mesh_gen.cpp
    src/geometry/obj_loader.cpp
    src/platform/hwinfo.cpp
    src/platform/gpu_select.cpp
    src/platform/logger.cpp
    src/platform/data_path.cpp
    src/platform/stb_image_impl.cpp
)

# Suppress warnings for third-party source files
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set_source_files_properties(
        src/platform/stb_image_impl.cpp
        PROPERTIES COMPILE_FLAGS "-w"
    )
endif()

add_library(gpubench_core STATIC ${CORE_LIB_SOURCES})
target_include_directories(gpubench_core PUBLIC src extern)
target_link_libraries(gpubench_core PUBLIC imgui_lib)
if(TARGET OpenGL::GL)
    target_link_libraries(gpubench_core PUBLIC OpenGL::GL)
else()
    target_link_libraries(gpubench_core PUBLIC ${OPENGL_LIBRARIES})
endif()

if(PORTABLE_BUILD AND NOT WIN32)
    target_compile_definitions(gpubench_core PUBLIC CB_STATIC_GL)
endif()

# --- Engine convenience layer (between core and demo) ---
set(ENGINE_SOURCES
    src/engine/pass_context.cpp
    src/engine/fullscreen_pass.cpp
    src/engine/geometry_pass.cpp
    src/engine/compute_pass.cpp
    src/engine/draw_list.cpp
)
add_library(gpubench_engine STATIC ${ENGINE_SOURCES})
target_include_directories(gpubench_engine PUBLIC src extern)
target_link_libraries(gpubench_engine PUBLIC gpubench_core)

# --- Benchmark executable ---
# Bench library sources (shared between gpu_benchmark and unit tests)
set(BENCH_LIB_SOURCES
    # Bench engine
    src/bench/bench.cpp
    src/bench/bench_runner.cpp
    src/bench/stress_runner.cpp
    src/bench/preset.cpp
    src/bench/results.cpp
    src/bench/preset_io.cpp

    # Tests (30)
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

    # UI
    src/bench/bench_ui.cpp
)

add_executable(gpu_benchmark src/bench/main.cpp ${BENCH_LIB_SOURCES})
target_link_libraries(gpu_benchmark PRIVATE gpubench_core)

# --- Demo executable ---
set(DEMO_SOURCES
    src/demo/main.cpp
    src/demo/demo_camera.cpp
    src/demo/shader_loader.cpp
    src/demo/demo_scene.cpp
    src/demo/scene_loader.cpp
    src/demo/materials.cpp
    src/demo/demo_tier_config.cpp
    src/demo/tier_config_validate.cpp
    src/demo/demo_runner.cpp
    src/demo/demo_ui.cpp
    src/demo/demo_export.cpp
    src/demo/demo_resources.cpp
    src/demo/pass_factory.cpp
    src/demo/pipeline_builder.cpp
    src/demo/shader_cache.cpp
    src/demo/passes/sky_pass.cpp
    src/demo/passes/shadow_pass.cpp
    src/demo/passes/particle_pass.cpp
    src/demo/passes/torch_pass.cpp
    src/demo/passes/opaque_pass.cpp
    src/demo/passes/grass_instanced_pass.cpp
    src/demo/passes/fur_pass.cpp
    src/demo/passes/ssao_pass.cpp
    src/demo/passes/bloom_pass.cpp
    src/demo/passes/composite_pass.cpp
    src/demo/passes/scene_to_fbo_pass.cpp
    src/demo/passes/hdr_composite_pass.cpp
    src/demo/passes/compute_particles_pass.cpp
    src/demo/passes/compute_particles_draw_pass.cpp
    src/demo/passes/tessellated_model_pass.cpp
    src/demo/passes/gtao_pass.cpp
    src/demo/passes/bloom_compute_pass.cpp
    src/demo/passes/auto_exposure_pass.cpp
    src/demo/passes/volumetric_fog_pass.cpp
    src/demo/passes/water_pass.cpp
    src/demo/passes/ssr_pass.cpp
    src/demo/passes/ssr_copy_pass.cpp
    src/demo/passes/dof_pass.cpp
)

add_executable(gpu_demo ${DEMO_SOURCES})
target_link_libraries(gpu_demo PRIVATE gpubench_engine)

# --- Platform link flags (both executables) ---
foreach(target gpu_benchmark gpu_demo)
    if(TARGET SDL2::SDL2main)
        target_link_libraries(${target} PRIVATE SDL2::SDL2main)
    endif()
    if(APPLE)
        target_link_libraries(${target} PRIVATE pthread)
    elseif(CMAKE_SYSTEM_NAME STREQUAL "FreeBSD")
        target_link_libraries(${target} PRIVATE pthread)
    elseif(UNIX)
        target_link_libraries(${target} PRIVATE dl pthread)
        if(PORTABLE_BUILD AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_link_libraries(${target} PRIVATE -static-libgcc -static-libstdc++)
        endif()
    endif()
    if(WIN32)
        target_link_libraries(${target} PRIVATE advapi32)
        if(MINGW)
            target_link_libraries(${target} PRIVATE -mconsole -static-libgcc -static-libstdc++)
        elseif(MSVC)
            set_target_properties(${target} PROPERTIES LINK_FLAGS "/SUBSYSTEM:CONSOLE")
        endif()
    endif()
endforeach()

# --- Unit tests (no GPU required, native builds only) ---
if(EXISTS "${CMAKE_SOURCE_DIR}/extern/doctest/doctest.h" AND NOT CMAKE_CROSSCOMPILING)
    add_executable(gpubench_tests
        tests/test_main.cpp
        tests/test_bench_stats.cpp
        tests/test_bench_scoring.cpp
        ${BENCH_LIB_SOURCES}
    )
    target_include_directories(gpubench_tests PRIVATE src extern/doctest)
    target_link_libraries(gpubench_tests PRIVATE gpubench_core)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(gpubench_tests PRIVATE -Wall -Wextra -Wno-unused-parameter)
    endif()
endif()

# --- Install ---
install(TARGETS gpu_benchmark gpu_demo RUNTIME DESTINATION bin)
install(DIRECTORY data/ DESTINATION share/gpu_benchmark/data
        FILES_MATCHING
        PATTERN "*.glsl"
        PATTERN "*.vert"
        PATTERN "*.frag"
        PATTERN "*.comp"
        PATTERN "*.tesc"
        PATTERN "*.tese"
        PATTERN "*.obj")
