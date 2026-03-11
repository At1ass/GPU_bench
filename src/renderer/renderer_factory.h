#pragma once
#include "renderer/renderer.h"
#include "renderer/renderer_backend.h"
#include <memory>

// Create renderer for the given backend (RENDERER_AUTO auto-detects)
std::unique_ptr<Renderer> createRenderer(RendererBackend backend);
