#include "tests/tests.h"
#include "renderer/features.h"
#include "renderer/renderer.h"
#include "platform/logger.h"
#include <cstring>

// Persistent buffer mapping throughput test (GL 4.4+).
// Measures CPU->GPU streaming via persistent coherent mapping.

static const int PERSISTENT_MAP_PASSES = 100;

PersistentMapTest::PersistentMapTest(const PersistentMapParams& params)
    : params_(params), persistent_buf_(INVALID_BUFFER),
      buffer_size_(0), frame_idx_(0) {}

const char* PersistentMapTest::name() const { return "PersistentMap"; }
const char* PersistentMapTest::scoreUnit() const { return "GB/s"; }
const char* PersistentMapTest::description() const {
    return "Persistent buffer mapping throughput (GL 4.4+).\n"
           "Streams data via MAP_PERSISTENT_BIT | MAP_COHERENT_BIT.\n"
           "Measures CPU->GPU transfer with fence sync.";
}

void PersistentMapTest::setupGL4(Renderer&, GL4Features& gl4, int, int) {
    buffer_size_ = params_.buffer_size_mb * 1024 * 1024;
    persistent_buf_ = gl4.createPersistentBuffer(buffer_size_);
    frame_idx_ = 0;
    LOG_DBG("Test '%s': setup complete (%d MB buffer, %d frames in flight)", name(), params_.buffer_size_mb, params_.frames_in_flight);
}

void PersistentMapTest::renderGL4(Renderer&, GL4Features& gl4) {
    if (persistent_buf_ == INVALID_BUFFER) return;

    void* ptr = gl4.mapPersistentBuffer(persistent_buf_);
    if (!ptr) return;

    for (int pass = 0; pass < PERSISTENT_MAP_PASSES; pass++) {
        // Simulate streaming: write data to mapped buffer
        memset(ptr, static_cast<int>(frame_idx_ & 0xFF), static_cast<size_t>(buffer_size_));

        // Fence to ensure GPU consumed previous data
        gl4.persistentBufferFence(persistent_buf_);
        frame_idx_++;
    }
}

void PersistentMapTest::cleanupGL4(Renderer&, GL4Features& gl4) {
    if (persistent_buf_ != INVALID_BUFFER) {
        gl4.destroyPersistentBuffer(persistent_buf_);
        persistent_buf_ = INVALID_BUFFER;
    }
}

double PersistentMapTest::computeScore(const std::vector<double>& times, int, int) {
    double avg_ms = avgFrameMs(times);
    if (avg_ms <= 0.0) return 0;

    double bytes = static_cast<double>(buffer_size_) * PERSISTENT_MAP_PASSES;
    return bytes / (avg_ms / 1000.0) / 1e9; // GB/s
}
