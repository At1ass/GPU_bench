#pragma once
#include "renderer.h"
#include "renderer_backend.h"

// Create renderer for the given backend (RENDERER_AUTO auto-detects)
Renderer* createRenderer(RendererBackend backend);
