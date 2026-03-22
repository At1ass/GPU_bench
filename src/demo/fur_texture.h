#pragma once
#include <vector>

// Simple deterministic LCG PRNG for fur texture generation.
struct FurRNG {
    unsigned int state;
    FurRNG(unsigned int seed) : state(seed) {}
    float next() {
        state = state * 1103515245u + 12345u;
        return static_cast<float>((state >> 16) & 0x7FFF) / 32767.0f;
    }
};

// Generate a tileable 2D fur strand texture.
// Each pixel: RGB = color variation, A = strand max height (0 = no strand).
// The same texture is reused for all shell layers -- the shader checks
// shellIndex < alpha to determine visibility (as per classic shell fur).
inline std::vector<unsigned char> generateFurTexture(int size, float coverage) {
    std::vector<unsigned char> pixels(static_cast<size_t>(size) * size * 4, 0);
    FurRNG rng(42u);

    // Scatter 2x2 strand dots (survive GL_LINEAR filtering better than 1x1)
    // Coverage formula: 1 - exp(-num_strands * 4 / total_pixels)
    // For 75% coverage on 128x128: num_strands ~ size*size*0.35
    int num_strands = static_cast<int>(size * size * coverage * 0.35f);

    for (int i = 0; i < num_strands; i++) {
        int cx = static_cast<int>(rng.next() * size) % size;
        int cy = static_cast<int>(rng.next() * size) % size;

        // Strand height: biased high [0.3, 1.0]
        float height = 0.3f + 0.7f * rng.next();
        // Color variation per strand [0.7, 1.0]
        float cvar = 0.7f + 0.3f * rng.next();

        unsigned char h_byte = static_cast<unsigned char>(height * 255.0f);
        unsigned char c_byte = static_cast<unsigned char>(cvar * 255.0f);

        // Place 2x2 dot (wraps around for seamless tiling)
        for (int dy = 0; dy < 2; dy++) {
            for (int dx = 0; dx < 2; dx++) {
                int px = (cx + dx) % size;
                int py = (cy + dy) % size;
                int idx = (py * size + px) * 4;
                // Keep the tallest strand at each pixel
                if (pixels[idx + 3] < h_byte) {
                    pixels[idx + 0] = c_byte;
                    pixels[idx + 1] = c_byte;
                    pixels[idx + 2] = c_byte;
                    pixels[idx + 3] = h_byte;
                }
            }
        }
    }

    return pixels;
}
