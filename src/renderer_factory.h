#pragma once
#include "renderer.h"

// Detect GL major version from current context
int detectGLMajorVersion();

// Create best available renderer (auto-detect)
Renderer* createBestRenderer();

// Create specific renderer: 0=auto, 2=GL2, 3=GL3
Renderer* createRenderer(int force_gl);
