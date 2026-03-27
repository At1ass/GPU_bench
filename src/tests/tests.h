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
    SanityType sanityType() const override { return SanityType::None; }
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
class InstancedDrawTest : public GL3BenchTest {
public:
    InstancedDrawTest(const InstancedDrawParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
protected:
    void setupGL3(Renderer& r, GL3Features& gl3, int vw, int vh) override;
    void renderGL3(Renderer& r, GL3Features& gl3) override;
    void cleanupGL3(Renderer& r, GL3Features& gl3) override;
private:
    InstancedDrawParams params_;
    int tri_count_;
    int vw_, vh_;
    MeshHandle mesh_;
    ShaderHandle shader_;
    int u_proj_loc_, u_view_loc_, u_instance_count_loc_;
};

// --- Compute FMA Test (GL4.3+ compute shaders) ---
class ComputeFMATest : public ComputeBenchTest {
public:
    ComputeFMATest(const ComputeFMAParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
    SanityType sanityType() const override { return SanityType::ComputeBuffer; }
    BufferHandle getOutputBuffer() const override { return ssbo_; }
protected:
    void setupCompute(Renderer& r, ComputeFeatures& comp, int vw, int vh) override;
    void renderCompute(Renderer& r, ComputeFeatures& comp) override;
    void cleanupCompute(Renderer& r, ComputeFeatures& comp) override;
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

// --- FBO Fillrate Test (GL3+) ---
class FBOFillrateTest : public GL3BenchTest {
public:
    FBOFillrateTest(const FBOFillrateParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
protected:
    void setupGL3(Renderer& r, GL3Features& gl3, int vw, int vh) override;
    void renderGL3(Renderer& r, GL3Features& gl3) override;
    void cleanupGL3(Renderer& r, GL3Features& gl3) override;
private:
    FBOFillrateParams params_;
    int rt_size_;
    MeshHandle quad_;
    RenderTargetHandle rt_;
};

// --- MRT Fill Test (GL3+) ---
class MRTFillTest : public GL3BenchTest {
public:
    MRTFillTest(const MRTFillParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
protected:
    void setupGL3(Renderer& r, GL3Features& gl3, int vw, int vh) override;
    void renderGL3(Renderer& r, GL3Features& gl3) override;
    void cleanupGL3(Renderer& r, GL3Features& gl3) override;
private:
    MRTFillParams params_;
    int rt_size_;
    MeshHandle quad_;
    RenderTargetHandle rt_;
    ShaderHandle shader_;
};

// --- Texture Array Sample Test (GL3+) ---
class TexArraySampleTest : public GL3BenchTest {
public:
    TexArraySampleTest(const TexArraySampleParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
protected:
    void setupGL3(Renderer& r, GL3Features& gl3, int vw, int vh) override;
    void renderGL3(Renderer& r, GL3Features& gl3) override;
    void cleanupGL3(Renderer& r, GL3Features& gl3) override;
private:
    TexArraySampleParams params_;
    int actual_tex_size_;
    int vw_, vh_;
    MeshHandle quad_;
    TextureHandle tex_array_;
    ShaderHandle shader_;
    int u_layer_loc_;
};

// --- Geometry Shader Test (GL3.2+) ---
class GeometryShaderTest : public GL3BenchTest {
public:
    GeometryShaderTest(const GeometryShaderParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
protected:
    void setupGL3(Renderer& r, GL3Features& gl3, int vw, int vh) override;
    void renderGL3(Renderer& r, GL3Features& gl3) override;
    void cleanupGL3(Renderer& r, GL3Features& gl3) override;
private:
    GeometryShaderParams params_;
    int vw_, vh_;
    MeshHandle points_mesh_;
    ShaderHandle shader_;
};

// --- UBO Switch Test (GL3+) ---
class UBOSwitchTest : public GL3BenchTest {
public:
    UBOSwitchTest(const UBOSwitchParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
protected:
    void setupGL3(Renderer& r, GL3Features& gl3, int vw, int vh) override;
    void renderGL3(Renderer& r, GL3Features& gl3) override;
    void cleanupGL3(Renderer& r, GL3Features& gl3) override;
private:
    UBOSwitchParams params_;
    int vw_, vh_;
    MeshHandle quad_;
    ShaderHandle shader_;
    std::vector<BufferHandle> ubos_;
};

// --- Transform Feedback Test (GL3+) ---
class TransformFeedbackTest : public GL3BenchTest {
public:
    TransformFeedbackTest(const TransformFeedbackParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
    SanityType sanityType() const override { return SanityType::None; }
protected:
    void setupGL3(Renderer& r, GL3Features& gl3, int vw, int vh) override;
    void renderGL3(Renderer& r, GL3Features& gl3) override;
    void cleanupGL3(Renderer& r, GL3Features& gl3) override;
private:
    TransformFeedbackParams params_;
    int actual_vertex_count_;
    MeshHandle mesh_;
    ShaderHandle shader_;
    BufferHandle tf_buffer_;
};

// --- Compute Bandwidth Test (GL4.3+) ---
class ComputeBandwidthTest : public ComputeBenchTest {
public:
    ComputeBandwidthTest(const ComputeBandwidthParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
    SanityType sanityType() const override { return SanityType::ComputeBuffer; }
    BufferHandle getOutputBuffer() const override { return ssbo_out_; }
protected:
    void setupCompute(Renderer& r, ComputeFeatures& comp, int vw, int vh) override;
    void renderCompute(Renderer& r, ComputeFeatures& comp) override;
    void cleanupCompute(Renderer& r, ComputeFeatures& comp) override;
private:
    ComputeBandwidthParams params_;
    ShaderHandle shader_;
    BufferHandle ssbo_in_;
    BufferHandle ssbo_out_;
    int u_count_loc_;
};

// --- Indirect Draw Test (GL4.3+) ---
class IndirectDrawTest : public GL4BenchTest {
public:
    IndirectDrawTest(const IndirectDrawParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
protected:
    void setupGL4(Renderer& r, GL4Features& gl4, int vw, int vh) override;
    void renderGL4(Renderer& r, GL4Features& gl4) override;
    void cleanupGL4(Renderer& r, GL4Features& gl4) override;
private:
    IndirectDrawParams params_;
    int vw_, vh_;
    MeshHandle mesh_;
    ShaderHandle shader_;
    BufferHandle indirect_buf_;
};

// --- Compute Shared Memory Test (GL4.3+) ---
class ComputeSharedMemTest : public ComputeBenchTest {
public:
    ComputeSharedMemTest(const ComputeSharedMemParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
    SanityType sanityType() const override { return SanityType::ComputeBuffer; }
    BufferHandle getOutputBuffer() const override { return ssbo_; }
protected:
    void setupCompute(Renderer& r, ComputeFeatures& comp, int vw, int vh) override;
    void renderCompute(Renderer& r, ComputeFeatures& comp) override;
    void cleanupCompute(Renderer& r, ComputeFeatures& comp) override;
private:
    ComputeSharedMemParams params_;
    ShaderHandle shader_;
    BufferHandle ssbo_;
    int u_iterations_loc_;
};

// --- SSBO Atomics Test (GL4.3+) ---
class SSBOAtomicsTest : public ComputeBenchTest {
public:
    SSBOAtomicsTest(const SSBOAtomicsParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
    SanityType sanityType() const override { return SanityType::ComputeBuffer; }
    BufferHandle getOutputBuffer() const override { return ssbo_; }
protected:
    void setupCompute(Renderer& r, ComputeFeatures& comp, int vw, int vh) override;
    void renderCompute(Renderer& r, ComputeFeatures& comp) override;
    void cleanupCompute(Renderer& r, ComputeFeatures& comp) override;
private:
    SSBOAtomicsParams params_;
    ShaderHandle shader_;
    BufferHandle ssbo_;
    int u_iterations_loc_;
};

// --- Tessellation Throughput Test (GL4.0+) ---
class TessellationTPTest : public GL4BenchTest {
public:
    TessellationTPTest(const TessellationTPParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
protected:
    void setupGL4(Renderer& r, GL4Features& gl4, int vw, int vh) override;
    void renderGL4(Renderer& r, GL4Features& gl4) override;
    void cleanupGL4(Renderer& r, GL4Features& gl4) override;
private:
    TessellationTPParams params_;
    int vw_, vh_;
    MeshHandle mesh_;
    ShaderHandle shader_;
};

// --- Texture Gather Test (GL4.0+) ---
class TextureGatherTest : public GL4BenchTest {
public:
    TextureGatherTest(const TextureGatherParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
protected:
    void setupGL4(Renderer& r, GL4Features& gl4, int vw, int vh) override;
    void renderGL4(Renderer& r, GL4Features& gl4) override;
    void cleanupGL4(Renderer& r, GL4Features& gl4) override;
private:
    TextureGatherParams params_;
    int vw_, vh_;
    MeshHandle quad_;
    TextureHandle texture_;
    ShaderHandle shader_;
    int u_iterations_loc_;
};

// --- Image Load/Store Test (GL4.2+) ---
class ImageLoadStoreTest : public ComputeBenchTest {
public:
    ImageLoadStoreTest(const ImageLoadStoreParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
    SanityType sanityType() const override { return SanityType::ComputeBuffer; }
    BufferHandle getOutputBuffer() const override { return ssbo_; }
protected:
    void setupCompute(Renderer& r, ComputeFeatures& comp, int vw, int vh) override;
    void renderCompute(Renderer& r, ComputeFeatures& comp) override;
    void cleanupCompute(Renderer& r, ComputeFeatures& comp) override;
private:
    ImageLoadStoreParams params_;
    ShaderHandle shader_;
    TextureHandle image_tex_;
    BufferHandle ssbo_;
};

// --- Compute Prefix Sum Test (GL4.3+) ---
class ComputePrefixTest : public ComputeBenchTest {
public:
    ComputePrefixTest(const ComputePrefixParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
    SanityType sanityType() const override { return SanityType::ComputeBuffer; }
    BufferHandle getOutputBuffer() const override { return ssbo_out_; }
protected:
    void setupCompute(Renderer& r, ComputeFeatures& comp, int vw, int vh) override;
    void renderCompute(Renderer& r, ComputeFeatures& comp) override;
    void cleanupCompute(Renderer& r, ComputeFeatures& comp) override;
private:
    ComputePrefixParams params_;
    ShaderHandle shader_;
    BufferHandle ssbo_in_;
    BufferHandle ssbo_out_;
    int u_count_loc_;
};

// --- Persistent Mapping Test (GL4.4+) ---
class PersistentMapTest : public GL4BenchTest {
public:
    PersistentMapTest(const PersistentMapParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
    SanityType sanityType() const override { return SanityType::None; }
protected:
    void setupGL4(Renderer& r, GL4Features& gl4, int vw, int vh) override;
    void renderGL4(Renderer& r, GL4Features& gl4) override;
    void cleanupGL4(Renderer& r, GL4Features& gl4) override;
private:
    PersistentMapParams params_;
    BufferHandle persistent_buf_;
    int buffer_size_;
    int frame_idx_;
};

// --- Bindless Texture Test (ARB ext) ---
class BindlessTexTest : public GL4BenchTest {
public:
    BindlessTexTest(const BindlessTexParams& params);
    const char* name() const override;
    const char* scoreUnit() const override;
    const char* description() const override;
    double computeScore(const std::vector<double>& t, int vw, int vh) override;
protected:
    void setupGL4(Renderer& r, GL4Features& gl4, int vw, int vh) override;
    void renderGL4(Renderer& r, GL4Features& gl4) override;
    void cleanupGL4(Renderer& r, GL4Features& gl4) override;
private:
    BindlessTexParams params_;
    int vw_, vh_;
    MeshHandle quad_;
    ShaderHandle shader_;
    std::vector<TextureHandle> textures_;
    std::vector<uint64_t> handles_;
    int u_tex_loc_;
};
