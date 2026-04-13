#include <doctest.h>
#include "platform/data_path.h"
#include "platform/logger.h"
#include <cstring>

// ---- hasPathTraversal ----

TEST_SUITE("PathTraversal") {

TEST_CASE("hasPathTraversal: parent directory") {
    CHECK(hasPathTraversal("../etc/passwd") == true);
}

TEST_CASE("hasPathTraversal: safe path") {
    CHECK(hasPathTraversal("models/bunny.obj") == false);
}

TEST_CASE("hasPathTraversal: embedded traversal") {
    CHECK(hasPathTraversal("a/../../b") == true);
}

TEST_CASE("hasPathTraversal: null") {
    CHECK(hasPathTraversal(nullptr) == true);
}

TEST_CASE("hasPathTraversal: dots in filename (not traversal)") {
    CHECK(hasPathTraversal("data/..shader") == false);  // ".." followed by 's', not separator
    CHECK(hasPathTraversal("data/my.file.txt") == false);
    CHECK(hasPathTraversal("data/.hidden") == false);
}

TEST_CASE("hasPathTraversal: trailing dotdot") {
    CHECK(hasPathTraversal("foo/..") == true);
}

} // TEST_SUITE("PathTraversal")

// ---- Logger: extractSubsystem ----

TEST_SUITE("Logger::extractSubsystem") {

TEST_CASE("extractSubsystem: renderer") {
    auto tag = Log::extractSubsystem("src/renderer/gl2.cpp");
    CHECK(strncmp(tag.name, "renderer", tag.len) == 0);
    CHECK(tag.len == 8);
}

TEST_CASE("extractSubsystem: demo") {
    auto tag = Log::extractSubsystem("src/demo/scene.cpp");
    CHECK(strncmp(tag.name, "demo", tag.len) == 0);
    CHECK(tag.len == 4);
}

TEST_CASE("extractSubsystem: default (no src/ prefix)") {
    auto tag = Log::extractSubsystem("main.cpp");
    CHECK(strncmp(tag.name, "app", tag.len) == 0);
    CHECK(tag.len == 3);
}

} // TEST_SUITE("Logger::extractSubsystem")

// ---- Logger: extractFilename ----

TEST_SUITE("Logger::extractFilename") {

TEST_CASE("extractFilename: absolute path") {
    CHECK(strcmp(Log::extractFilename("/home/user/src/foo.cpp"), "foo.cpp") == 0);
}

TEST_CASE("extractFilename: relative path") {
    CHECK(strcmp(Log::extractFilename("renderer/gl2.cpp"), "gl2.cpp") == 0);
}

TEST_CASE("extractFilename: bare filename") {
    CHECK(strcmp(Log::extractFilename("main.cpp"), "main.cpp") == 0);
}

} // TEST_SUITE("Logger::extractFilename")
