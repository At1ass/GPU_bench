#include "demo/material.h"

namespace Materials {

MaterialDef terrain() {
    MaterialDef m;
    m.color_a = Vec3(0.45f, 0.42f, 0.38f);
    m.color_b = Vec3(0.30f, 0.45f, 0.15f);  // grass tint
    m.specular = 0.05f;
    m.roughness = 0.85f;
    m.proc_type = ProceduralType::Terrain;
    return m;
}

MaterialDef stone(Vec3 tint) {
    MaterialDef m;
    m.color_a = tint;
    m.color_b = tint * 0.65f + Vec3(0.08f, 0.06f, 0.04f);  // darker, warmer crevice
    m.specular = 0.06f;
    m.metallic = 0.0f;
    m.roughness = 0.88f;
    m.proc_type = ProceduralType::Stone;
    m.noise_scale = 1.0f;
    m.noise_intensity = 1.0f;
    m.warp_strength = 0.3f;
    m.detail_scale = 1.0f;
    return m;
}

MaterialDef rock(Vec3 tint) {
    MaterialDef m;
    m.color_a = tint * 1.1f + Vec3(0.05f, 0.04f, 0.03f);  // lighter base
    m.color_b = tint * 0.8f + Vec3(0.06f, 0.04f, 0.02f);
    m.specular = 0.08f;
    m.metallic = 0.0f;
    m.roughness = 0.92f;
    m.proc_type = ProceduralType::Rock;
    m.noise_scale = 1.0f;
    m.noise_intensity = 1.0f;
    m.warp_strength = 0.2f;
    m.detail_scale = 1.5f;
    m.two_sided = true;
    return m;
}

MaterialDef foliage(Vec3 tint) {
    MaterialDef m;
    m.color_a = tint;
    m.color_b = tint * 1.4f + Vec3(0.05f, 0.08f, 0.0f);  // brighter canopy top
    m.specular = 0.03f;
    m.metallic = 0.0f;
    m.roughness = 0.85f;
    m.proc_type = ProceduralType::Foliage;
    m.noise_scale = 1.0f;
    m.noise_intensity = 1.0f;
    m.warp_strength = 0.4f;
    m.detail_scale = 2.0f;
    return m;
}

MaterialDef moss(Vec3 tint) {
    MaterialDef m;
    m.color_a = tint;
    m.color_b = Vec3(0.15f, 0.35f, 0.08f);  // deep moss green
    m.specular = 0.03f;
    m.metallic = 0.0f;
    m.roughness = 0.95f;
    m.proc_type = ProceduralType::Moss;
    m.noise_scale = 1.0f;
    m.noise_intensity = 1.0f;
    m.warp_strength = 0.6f;
    m.detail_scale = 1.0f;
    return m;
}

MaterialDef skin(Vec3 tint) {
    MaterialDef m;
    m.color_a = tint;
    m.color_b = tint * 0.8f;
    m.specular = 0.08f;
    m.metallic = 0.0f;
    m.roughness = 0.85f;
    m.proc_type = ProceduralType::Flat;
    return m;
}

MaterialDef water(Vec3 tint) {
    MaterialDef m;
    m.color_a = tint;
    m.color_b = tint;
    m.specular = 0.95f;
    m.metallic = 0.0f;
    m.roughness = 0.02f;
    m.proc_type = ProceduralType::Flat;
    m.two_sided = true;
    return m;
}

} // namespace Materials
