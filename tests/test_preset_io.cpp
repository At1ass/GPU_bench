#include <doctest.h>
#include "bench/preset.h"
#include "bench/preset_io.h"
#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

static const char* tmpPath() {
    static char buf[256];
#ifdef _WIN32
    const char* tmp = getenv("TEMP");
    if (!tmp) tmp = ".";
    snprintf(buf, sizeof(buf), "%s\\gpubench_test_preset_%d.ini", tmp, static_cast<int>(getpid()));
#else
    snprintf(buf, sizeof(buf), "/tmp/gpubench_test_preset_%d.ini", static_cast<int>(getpid()));
#endif
    return buf;
}

TEST_SUITE("PresetIO") {

TEST_CASE("Preset I/O: roundtrip save/load") {
    BenchPreset orig = getPreset(static_cast<int>(PresetIndex::Medium));
    const char* path = tmpPath();
    CHECK(saveConfig(path, orig));

    BenchPreset loaded;
    CHECK(loadConfig(path, loaded));

    CHECK(loaded.warmup_frames == orig.warmup_frames);
    CHECK(loaded.measure_frames == orig.measure_frames);
    CHECK(loaded.fillrate.layers == orig.fillrate.layers);
    CHECK(loaded.geometry.grid_size == orig.geometry.grid_size);
    CHECK(loaded.texturing.tex_size == orig.texturing.tex_size);

    remove(path);
}

TEST_CASE("Preset I/O: malformed input preserves defaults") {
    const char* path = tmpPath();
    // Write garbage
    FILE* f = fopen(path, "w");
    fprintf(f, "not_a_section\nno_equals_sign\n=no_key\n[bogus]\nfoo=bar\n");
    fclose(f);

    BenchPreset p;
    BenchPreset defaults = getPreset(static_cast<int>(PresetIndex::Medium));
    CHECK(loadConfig(path, p));
    // Should be defaults (Medium base)
    CHECK(p.fillrate.layers == defaults.fillrate.layers);

    remove(path);
}

TEST_CASE("Preset I/O: boundary clamping") {
    const char* path = tmpPath();
    FILE* f = fopen(path, "w");
    fprintf(f, "[fillrate]\nlayers=-5\n");
    fclose(f);

    BenchPreset p;
    CHECK(loadConfig(path, p));
    CHECK(p.fillrate.layers >= 1);  // clamped to minimum

    remove(path);
}

TEST_CASE("Preset I/O: unknown sections ignored") {
    const char* path = tmpPath();
    FILE* f = fopen(path, "w");
    fprintf(f, "[unknown_section]\nfoo=42\n[fillrate]\nlayers=100\n");
    fclose(f);

    BenchPreset p;
    CHECK(loadConfig(path, p));
    CHECK(p.fillrate.layers == 100);

    remove(path);
}

} // TEST_SUITE("PresetIO")
