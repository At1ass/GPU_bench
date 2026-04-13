#pragma once

// All render pass headers — auto-included by pass_factory.
// Keep in sync with pass_registry.def.

#include "demo/passes/sky_pass.h"
#include "demo/passes/shadow_pass.h"
#include "demo/passes/opaque_pass.h"
#include "demo/passes/grass_instanced_pass.h"
#include "demo/passes/fur_pass.h"
#include "demo/passes/particle_pass.h"
#include "demo/passes/torch_pass.h"
#include "demo/passes/ssao_pass.h"
#include "demo/passes/bloom_pass.h"
#include "demo/passes/composite_pass.h"
#include "demo/passes/hdr_composite_pass.h"
#include "demo/passes/compute_particles_pass.h"
#include "demo/passes/compute_particles_draw_pass.h"
#include "demo/passes/gtao_pass.h"
#include "demo/passes/bloom_compute_pass.h"
#include "demo/passes/auto_exposure_pass.h"
#include "demo/passes/volumetric_fog_pass.h"
#include "demo/passes/water_pass.h"
#include "demo/passes/ssr_pass.h"
#include "demo/passes/ssr_copy_pass.h"
#include "demo/passes/dof_pass.h"
