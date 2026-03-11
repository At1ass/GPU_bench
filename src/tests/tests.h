#pragma once
#include "bench/bench.h"
#include "geometry/mesh.h"
#include "bench/preset.h"
#include <vector>

// Helper: compute average frame time from measurements
inline double avgFrameMs(const std::vector<double>& times) {
    if (times.empty()) return 0.0;
    double total = 0.0;
    for (size_t i = 0; i < times.size(); i++) total += times[i];
    return total / static_cast<double>(times.size());
}

// Texture generation helpers (shared across tests)
std::vector<unsigned char> genCheckerboard(int size, int check_size);
std::vector<unsigned char> genColorNoise(int size, unsigned int seed);
int clampTexSize(int requested, int max_size);

// --- Fillrate Test ---
class FillrateTest : public BenchTest {
public:
    FillrateTest(const FillrateParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    void setup(Renderer* r, int vw, int vh) override;
    void render(Renderer* r) override;
    void cleanup(Renderer* r) override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
private:
    int layers_;
    int vw_, vh_;
    MeshHandle quad_;
};

// --- Geometry Test ---
class GeometryTest : public BenchTest {
public:
    GeometryTest(const GeometryParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    void setup(Renderer* r, int vw, int vh) override;
    void render(Renderer* r) override;
    void cleanup(Renderer* r) override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
private:
    int grid_size_;
    int tri_count_;
    int vw_, vh_;
    MeshHandle mesh_;
};

// --- Texturing Test ---
class TexturingTest : public BenchTest {
public:
    TexturingTest(const TexturingParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    void setup(Renderer* r, int vw, int vh) override;
    void render(Renderer* r) override;
    void cleanup(Renderer* r) override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
private:
    int tex_size_;
    int actual_tex_size_;
    int layers_;
    int vw_, vh_;
    MeshHandle quad_;
    TextureHandle texture_;
};

// --- Scene Test ---
class SceneTest : public BenchTest {
public:
    SceneTest(const SceneParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    void setup(Renderer* r, int vw, int vh) override;
    void render(Renderer* r) override;
    void cleanup(Renderer* r) override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
private:
    SceneParams params_;
    int vw_, vh_;
    MeshHandle terrain_;
    MeshHandle sphere_;
    MeshHandle cube_;
    TextureHandle terrain_tex_;
    TextureHandle obj_tex_;
    float angle_;
};

// --- Draw Call Overhead Test ---
class DrawCallTest : public BenchTest {
public:
    DrawCallTest(const DrawCallParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    void setup(Renderer* r, int vw, int vh) override;
    void render(Renderer* r) override;
    void cleanup(Renderer* r) override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
private:
    DrawCallParams params_;
    int vw_, vh_;
    std::vector<MeshHandle> meshes_;
};

// --- Overdraw Test ---
class OverdrawTest : public BenchTest {
public:
    OverdrawTest(const OverdrawParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    void setup(Renderer* r, int vw, int vh) override;
    void render(Renderer* r) override;
    void cleanup(Renderer* r) override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
private:
    OverdrawParams params_;
    int vw_, vh_;
    MeshHandle quad_;
};

// --- Texture Upload Test ---
class TexUploadTest : public BenchTest {
public:
    TexUploadTest(const TexUploadParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    void setup(Renderer* r, int vw, int vh) override;
    void render(Renderer* r) override;
    void cleanup(Renderer* r) override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
private:
    TexUploadParams params_;
    int actual_tex_size_;
    int vw_, vh_;
    MeshHandle quad_;
    TextureHandle texture_;
    std::vector<unsigned char> upload_data_;
};

// --- State Change Test ---
class StateChangeTest : public BenchTest {
public:
    StateChangeTest(const StateChangeParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    void setup(Renderer* r, int vw, int vh) override;
    void render(Renderer* r) override;
    void cleanup(Renderer* r) override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
private:
    StateChangeParams params_;
    int vw_, vh_;
    MeshHandle quad_;
    std::vector<ShaderHandle> shaders_;
    std::vector<TextureHandle> textures_;
};

// --- Vertex Throughput Test ---
class VertexTest : public BenchTest {
public:
    VertexTest(const VertexParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    void setup(Renderer* r, int vw, int vh) override;
    void render(Renderer* r) override;
    void cleanup(Renderer* r) override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
private:
    VertexParams params_;
    int actual_vertex_count_;
    int vw_, vh_;
    MeshHandle mesh_;
};

// --- Shader ALU Test ---
class ShaderALUTest : public BenchTest {
public:
    ShaderALUTest(const ShaderALUParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    void setup(Renderer* r, int vw, int vh) override;
    void render(Renderer* r) override;
    void cleanup(Renderer* r) override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
private:
    ShaderALUParams params_;
    int vw_, vh_;
    MeshHandle quad_;
    ShaderHandle shader_;
    int u_iterations_loc_;
    int u_time_loc_;
    float time_;
};

// --- Shader FMA Test (pure ALU, no transcendentals) ---
class ShaderFMATest : public BenchTest {
public:
    ShaderFMATest(const ShaderFMAParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    void setup(Renderer* r, int vw, int vh) override;
    void render(Renderer* r) override;
    void cleanup(Renderer* r) override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
private:
    ShaderFMAParams params_;
    int vw_, vh_;
    MeshHandle quad_;
    ShaderHandle shader_;
    int u_iterations_loc_;
    int u_time_loc_;
    float time_;
};

// --- Instanced Draw Test (GL3+ instancing) ---
class InstancedDrawTest : public BenchTest {
public:
    InstancedDrawTest(const InstancedDrawParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    void setup(Renderer* r, int vw, int vh) override;
    void render(Renderer* r) override;
    void cleanup(Renderer* r) override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
private:
    InstancedDrawParams params_;
    int tri_count_;
    int vw_, vh_;
    MeshHandle mesh_;
    ShaderHandle shader_;
    int u_proj_loc_, u_view_loc_, u_instance_count_loc_;
};

// --- Compute FMA Test (GL4.3+ compute shaders) ---
class ComputeFMATest : public BenchTest {
public:
    ComputeFMATest(const ComputeFMAParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    void setup(Renderer* r, int vw, int vh) override;
    void render(Renderer* r) override;
    void cleanup(Renderer* r) override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
private:
    ComputeFMAParams params_;
    ShaderHandle shader_;
    BufferHandle ssbo_;
    int u_iterations_loc_;
};

// --- Draw Call Raw Test (no per-draw uniform updates) ---
class DrawCallRawTest : public BenchTest {
public:
    DrawCallRawTest(const DrawCallParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    void setup(Renderer* r, int vw, int vh) override;
    void render(Renderer* r) override;
    void cleanup(Renderer* r) override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
private:
    DrawCallParams params_;
    int vw_, vh_;
    std::vector<MeshHandle> meshes_;
};
