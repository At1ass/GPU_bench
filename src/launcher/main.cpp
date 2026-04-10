// GPU Benchmark Launcher
// SDL2 + OpenGL + ImGui. Collects user settings and launches gpu_benchmark or gpu_demo.

#include "launcher/launcher_ui.h"
#include "launcher/process_launch.h"
#include "platform/logger.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>
#include <SDL.h>

#if defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <SDL_opengl.h>
#endif

#ifndef _WIN32
#include <signal.h>
#endif

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    Log::init("gpu_launcher.log");

#ifndef _WIN32
    signal(SIGCHLD, SIG_IGN);
#endif

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        LOG_ERR("SDL_Init failed: %s", SDL_GetError());
        Log::shutdown();
        return 1;
    }

#ifdef SDL_HINT_IME_SHOW_UI
    SDL_SetHint(SDL_HINT_IME_SHOW_UI, "1");
#endif

    // Minimal GL context — just enough for ImGui
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);

    SDL_Window* window = SDL_CreateWindow(
        "GPU Benchmark Launcher",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        720, 560,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    if (!window) {
        LOG_ERR("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        Log::shutdown();
        return 1;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) {
        LOG_ERR("SDL_GL_CreateContext failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        Log::shutdown();
        return 1;
    }
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(NULL);

    // Detect hardware once (read-only after init)
    HWInfo hw_info = HWInfo::detect();
    std::vector<GPUDevice> gpus = enumerateGPUs();

    // View: read-only snapshot for UI display
    LauncherView view;
    view.hw_info = &hw_info;
    view.gpus = &gpus;

    // State: mutable widget data
    LauncherState state;

    // UI renderer (stateless — all data through View/State)
    LauncherUI ui;

    bool running = true;
    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_CLOSE &&
                event.window.windowID == SDL_GetWindowID(window))
                running = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        LaunchAction action = ui.render(view, state);

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.12f, 0.12f, 0.14f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);

        // Dispatch action
        if (action.type != LaunchAction::None) {
            const char* exe = (action.type == LaunchAction::LaunchBenchmark)
                ? "gpu_benchmark" : "gpu_demo";
            launchProcess(exe, action.args);
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    Log::shutdown();
    return 0;
}
