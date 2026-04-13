#include <doctest.h>
#include "geometry/math_types.h"
#include <cmath>

static const float EPS = 1e-5f;

// ---- Vec3 ----

TEST_SUITE("Vec3") {

TEST_CASE("Vec3::length") {
    CHECK(Vec3(1, 0, 0).length() == doctest::Approx(1.0f));
    CHECK(Vec3(0, 1, 0).length() == doctest::Approx(1.0f));
    CHECK(Vec3(0, 0, 1).length() == doctest::Approx(1.0f));
    CHECK(Vec3(0, 0, 0).length() == doctest::Approx(0.0f));
    CHECK(Vec3(3, 4, 0).length() == doctest::Approx(5.0f));
    CHECK(Vec3(1, 1, 1).length() == doctest::Approx(sqrtf(3.0f)));
}

TEST_CASE("Vec3::normalized") {
    Vec3 n = Vec3(3, 4, 0).normalized();
    CHECK(n.length() == doctest::Approx(1.0f));
    CHECK(n.x == doctest::Approx(0.6f));
    CHECK(n.y == doctest::Approx(0.8f));

    // Unit vector stays unit
    Vec3 u = Vec3(1, 0, 0).normalized();
    CHECK(u.x == doctest::Approx(1.0f));
    CHECK(u.y == doctest::Approx(0.0f));

    // Zero vector → zero (not NaN)
    Vec3 z = Vec3(0, 0, 0).normalized();
    CHECK(z.x == 0.0f);
    CHECK(z.y == 0.0f);
    CHECK(z.z == 0.0f);
}

TEST_CASE("Vec3::cross") {
    // Right-hand rule: i×j = k
    Vec3 k = Vec3::cross(Vec3(1, 0, 0), Vec3(0, 1, 0));
    CHECK(k.x == doctest::Approx(0.0f));
    CHECK(k.y == doctest::Approx(0.0f));
    CHECK(k.z == doctest::Approx(1.0f));

    // Parallel vectors → zero
    Vec3 z = Vec3::cross(Vec3(1, 0, 0), Vec3(2, 0, 0));
    CHECK(z.length() == doctest::Approx(0.0f));

    // Anti-commutative: a×b = -(b×a)
    Vec3 a(1, 2, 3), b(4, 5, 6);
    Vec3 ab = Vec3::cross(a, b);
    Vec3 ba = Vec3::cross(b, a);
    CHECK(ab.x == doctest::Approx(-ba.x));
    CHECK(ab.y == doctest::Approx(-ba.y));
    CHECK(ab.z == doctest::Approx(-ba.z));
}

TEST_CASE("Vec3::dot") {
    // Orthogonal → 0
    CHECK(Vec3::dot(Vec3(1, 0, 0), Vec3(0, 1, 0)) == doctest::Approx(0.0f));

    // Parallel → |a|·|b|
    CHECK(Vec3::dot(Vec3(2, 0, 0), Vec3(3, 0, 0)) == doctest::Approx(6.0f));

    // Self-dot → length²
    Vec3 v(3, 4, 0);
    CHECK(Vec3::dot(v, v) == doctest::Approx(25.0f));
}

} // TEST_SUITE("Vec3")

// ---- Mat4 ----

TEST_SUITE("Mat4") {

TEST_CASE("Mat4 identity") {
    Mat4 I;
    // Diagonal = 1, rest = 0
    CHECK(I.m[0] == 1.0f); CHECK(I.m[5] == 1.0f);
    CHECK(I.m[10] == 1.0f); CHECK(I.m[15] == 1.0f);
    CHECK(I.m[1] == 0.0f); CHECK(I.m[4] == 0.0f);
}

TEST_CASE("Mat4 A * I = A") {
    Mat4 A = Mat4::translate(3, 5, 7);
    Mat4 I;
    Mat4 R = A * I;
    for (int i = 0; i < 16; i++)
        CHECK(R.m[i] == doctest::Approx(A.m[i]));
}

TEST_CASE("Mat4 multiply associativity") {
    Mat4 A = Mat4::translate(1, 2, 3);
    Mat4 B = Mat4::scale(2, 2, 2);
    Mat4 C = Mat4::rotateY(45.0f);

    Mat4 AB_C = (A * B) * C;
    Mat4 A_BC = A * (B * C);
    for (int i = 0; i < 16; i++)
        CHECK(AB_C.m[i] == doctest::Approx(A_BC.m[i]).epsilon(EPS));
}

TEST_CASE("Mat4 multiply non-commutative") {
    Mat4 T = Mat4::translate(5, 0, 0);
    Mat4 S = Mat4::scale(2, 2, 2);
    Mat4 TS = T * S;
    Mat4 ST = S * T;
    // TS ≠ ST in general
    bool different = false;
    for (int i = 0; i < 16; i++) {
        if (fabsf(TS.m[i] - ST.m[i]) > EPS) { different = true; break; }
    }
    CHECK(different);
}

TEST_CASE("Mat4::transformPoint translate") {
    Mat4 T = Mat4::translate(10, 20, 30);
    Vec3 p = T.transformPoint(Vec3(1, 2, 3));
    CHECK(p.x == doctest::Approx(11.0f));
    CHECK(p.y == doctest::Approx(22.0f));
    CHECK(p.z == doctest::Approx(33.0f));
}

TEST_CASE("Mat4::transformPoint scale") {
    Mat4 S = Mat4::scale(2, 3, 4);
    Vec3 p = S.transformPoint(Vec3(1, 1, 1));
    CHECK(p.x == doctest::Approx(2.0f));
    CHECK(p.y == doctest::Approx(3.0f));
    CHECK(p.z == doctest::Approx(4.0f));
}

TEST_CASE("Mat4::transformDirection ignores translation") {
    Mat4 T = Mat4::translate(100, 200, 300);
    Vec3 d = T.transformDirection(Vec3(1, 0, 0));
    CHECK(d.x == doctest::Approx(1.0f));
    CHECK(d.y == doctest::Approx(0.0f));
    CHECK(d.z == doctest::Approx(0.0f));
}

TEST_CASE("Mat4::transformDirection with scale") {
    Mat4 S = Mat4::scale(2, 2, 2);
    Vec3 d = S.transformDirection(Vec3(1, 0, 0));
    // transformDirection normalizes result
    CHECK(d.length() == doctest::Approx(1.0f));
}

TEST_CASE("Mat4::rotateY 90 degrees") {
    Mat4 R = Mat4::rotateY(90.0f);
    Vec3 p = R.transformPoint(Vec3(1, 0, 0));
    // 90° Y rotation: (1,0,0) → (0,0,-1)
    CHECK(p.x == doctest::Approx(0.0f).epsilon(EPS));
    CHECK(p.y == doctest::Approx(0.0f).epsilon(EPS));
    CHECK(p.z == doctest::Approx(-1.0f).epsilon(EPS));
}

TEST_CASE("Mat4::perspective basic properties") {
    Mat4 P = Mat4::perspective(90.0f, 1.0f, 0.1f, 100.0f);
    // FOV=90, aspect=1 → f = 1/tan(45°) = 1
    CHECK(P.m[0] == doctest::Approx(1.0f).epsilon(EPS));   // f/aspect
    CHECK(P.m[5] == doctest::Approx(1.0f).epsilon(EPS));   // f
    CHECK(P.m[15] == doctest::Approx(0.0f));                // perspective divide
    CHECK(P.m[11] == doctest::Approx(-1.0f));               // -1 for w
}

TEST_CASE("Mat4::lookAt basic") {
    // Camera at (0,0,5) looking at origin, up=(0,1,0)
    Mat4 V = Mat4::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
    // Should translate origin to (0,0,-5) in view space
    Vec3 p = V.transformPoint(Vec3(0, 0, 0));
    CHECK(p.z == doctest::Approx(-5.0f).epsilon(EPS));
    CHECK(fabsf(p.x) < EPS);
    CHECK(fabsf(p.y) < EPS);
}

} // TEST_SUITE("Mat4")
