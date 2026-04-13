#pragma once
#include "renderer/renderer.h"
#include <cstdint>
#include <string>
#include <vector>

// Sanity check strategy per test type
enum class SanityType {
    Framebuffer,     // readPixels from center of framebuffer (default)
    ComputeBuffer,   // readSSBO 64 bytes, check non-zero
    None             // skip sanity check (TF, Vertex, PersistentMap — rely on CV stability)
};

struct BenchResult {
    std::string name;
    std::string unit;       // e.g. "Mpix/s", "Mtris/s"
    double score = 0;
    double avg_ms = 0;
    double min_ms = 0;
    double max_ms = 0;
    double median_ms = 0;
    double p1_ms = 0;           // 1st percentile
    double p99_ms = 0;          // 99th percentile
    double cv = 0;              // coefficient of variation (stddev/avg)
    double p99_median_ratio = 0; // p99/median ratio
    double gpu_ms = 0;          // GPU timer result (0 if unavailable)
    int    frames = 0;
    bool   valid = false;
    bool   sanity_ok = true;  // true if render output is non-black (sanity check passed)

    BenchResult() = default;
    BenchResult(const BenchResult&) = default;
    BenchResult& operator=(const BenchResult&) = default;
    BenchResult(BenchResult&&) noexcept = default;
    BenchResult& operator=(BenchResult&&) noexcept = default;
};

// Base class for benchmark tests
class BenchTest {
public:
    virtual ~BenchTest() = default;
    BenchTest(const BenchTest&) = delete;
    BenchTest& operator=(const BenchTest&) = delete;
    BenchTest(BenchTest&&) = delete;
    BenchTest& operator=(BenchTest&&) = delete;
protected:
    BenchTest() = default;
public:
    virtual const char* name() const = 0;
    virtual const char* scoreUnit() const = 0;
    virtual const char* description() const = 0;

    // Called once before measurement. Create GPU resources here.
    virtual void setup(Renderer* r, int viewport_w, int viewport_h) = 0;

    // Render one frame of the test. Called in a tight loop during measurement.
    virtual void render(Renderer* r) = 0;

    // Called once after measurement. Destroy GPU resources here.
    virtual void cleanup(Renderer* r) = 0;

    // Compute a test-specific score from the collected frame times.
    virtual double computeScore(const std::vector<double>& frame_times_ms,
                                int viewport_w, int viewport_h) = 0;

    // Override to change sanity check strategy for non-framebuffer tests.
    virtual SanityType sanityType() const { return SanityType::Framebuffer; }

    // For SanityType::ComputeBuffer: return the output SSBO handle to validate.
    virtual BufferHandle getOutputBuffer() const { return BufferHandle(); }

    // Returns false if setup() failed (missing features, resource creation error).
    bool setupSucceeded() const { return setup_ok_; }

protected:
    bool setup_ok_ = true;
};

struct GL3Features;
struct GL4Features;
struct ComputeFeatures;

class GL3BenchTest : public BenchTest {
protected:
    virtual void setupGL3(Renderer& r, GL3Features& gl3, int vw, int vh) = 0;
    virtual void renderGL3(Renderer& r, GL3Features& gl3) = 0;
    virtual void cleanupGL3(Renderer& r, GL3Features& gl3) = 0;
private:
    GL3Features* gl3_ = nullptr;
    void setup(Renderer* r, int vw, int vh) final;
    void render(Renderer* r) final;
    void cleanup(Renderer* r) final;
};

class ComputeBenchTest : public BenchTest {
protected:
    virtual void setupCompute(Renderer& r, ComputeFeatures& comp, int vw, int vh) = 0;
    virtual void renderCompute(Renderer& r, ComputeFeatures& comp) = 0;
    virtual void cleanupCompute(Renderer& r, ComputeFeatures& comp) = 0;
private:
    ComputeFeatures* comp_ = nullptr;
    void setup(Renderer* r, int vw, int vh) final;
    void render(Renderer* r) final;
    void cleanup(Renderer* r) final;
};

class GL4BenchTest : public BenchTest {
protected:
    virtual void setupGL4(Renderer& r, GL4Features& gl4, int vw, int vh) = 0;
    virtual void renderGL4(Renderer& r, GL4Features& gl4) = 0;
    virtual void cleanupGL4(Renderer& r, GL4Features& gl4) = 0;
private:
    GL4Features* gl4_ = nullptr;
    void setup(Renderer* r, int vw, int vh) final;
    void render(Renderer* r) final;
    void cleanup(Renderer* r) final;
};

// Computes statistics from raw frame times
BenchResult computeStats(const std::string& name,
                         const std::string& unit,
                         const std::vector<double>& times_ms,
                         double score);

// Composite score (geometric mean of normalized scores by category)
struct CompositeScore {
    double overall = 0;
    double fill = 0;       // Fillrate, Overdraw, Texturing
    double geometry = 0;   // Geometry, Vertex
    double compute = 0;    // ShaderALU, ShaderFMA
    double overhead = 0;   // DrawCall, DrawCallRaw, StateChange, TexUpload

    // Normalized scores (percentage of reference GPU, unitless)
    double fill_norm = 0;
    double geometry_norm = 0;
    double compute_norm = 0;
    double overhead_norm = 0;
    int categories_present = 0;  // 0..4 — how many categories contributed to overall

    CompositeScore() = default;
};

CompositeScore computeCompositeScores(const std::vector<BenchResult>& results);

// Bottleneck detection
struct BottleneckInfo {
    std::string weakest_category;  // "Fill", "Geometry", "Compute", "Overhead"
    std::string detail;            // Human-readable explanation
    double weakness_ratio = 1.0;   // How much weaker vs average (0..1 = bad..ok)

    BottleneckInfo() = default;
    BottleneckInfo(const BottleneckInfo&) = default;
    BottleneckInfo& operator=(const BottleneckInfo&) = default;
    BottleneckInfo(BottleneckInfo&&) noexcept = default;
    BottleneckInfo& operator=(BottleneckInfo&&) noexcept = default;
};

BottleneckInfo detectBottleneck(const std::vector<BenchResult>& results,
                                const CompositeScore& scores);

// GPU performance tier classification
enum class GPUTier {
    Legacy = 0,  // GL 2.x class, very old GPUs (GeForce 6/7, Radeon 9xxx)
    Low    = 1,  // Low-end / old discrete or integrated (GeForce 8/9, HD 4xxx)
    Mid    = 2,  // Mid-range (GTX 560, HD 7850)
    High   = 3,  // High-end (GT 1030, GTX 1060, RX 580)
    Ultra  = 4   // Enthusiast (RTX 3060+, RX 6700+)
};

const char* gpuTierName(GPUTier tier);

// Classify GPU tier from caps and optional probe scores (0 = no probe data)
GPUTier classifyGPUTier(const RenderCaps& caps,
                        uint32_t available_caps,
                        double probe_fill_mpixs = 0,
                        double probe_geom_ktris = 0);

// Map GPU tier to suggested preset index
int tierToPresetIndex(GPUTier tier);
