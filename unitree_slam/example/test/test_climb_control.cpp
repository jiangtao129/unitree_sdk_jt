// =============================================================================
// test_climb_control.cpp
//
// Host-buildable unit test for the keyDemo3 stair-climb math (see
// `unitree_slam/example/include/climb_control.hpp`). Plain assert-based test;
// no GoogleTest / Catch dependency on purpose so this links fine on a fresh
// CI machine with only libc++/libstdc++.
//
// Coverage philosophy:
//   * Algebraic correctness only (wrapping, projection, clamps).
//   * NOT a tuning regression test - actual gains live in keyDemo3.cpp and
//     still need on-dock hardware regression after any change.
//
// Run mode:
//   * Returns 0 on success.
//   * On any failure, assert() aborts (typical libc behaviour) and the test
//     binary returns a non-zero exit code, so verify.sh fails loudly.
// =============================================================================

#include "climb_control.hpp"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace {

// Tolerant float compare. Trig identities introduce ULP-scale noise; we use
// a generous epsilon because nothing in this control loop cares about
// sub-microradian precision.
constexpr float kEps = 1e-5f;

bool nearEq(float a, float b, float eps = kEps) {
    return std::fabs(a - b) <= eps;
}

// Convenience: assert nearEq with a printable message on failure. We can't
// use std::ostringstream-from-assert tricks portably, so we print first then
// assert. Once asserts are disabled (-DNDEBUG) all this becomes a no-op,
// hence we also force-abort manually.
void requireNear(float got, float want, const char *what) {
    if (!nearEq(got, want)) {
        std::fprintf(stderr,
                     "[FAIL] %s: got %.9g, want %.9g (diff %.3g)\n",
                     what, got, want, got - want);
        std::abort();
    }
}

void requireBool(bool cond, const char *what) {
    if (!cond) {
        std::fprintf(stderr, "[FAIL] %s\n", what);
        std::abort();
    }
}

// ---------------------------------------------------------------------------
// 1. wrapPi
// ---------------------------------------------------------------------------
void test_wrapPi() {
    using unitree::slam::climb::wrapPi;

    requireNear(wrapPi(0.0f),                 0.0f,                "wrapPi(0)");
    requireNear(wrapPi( static_cast<float>(M_PI) - 0.01f),
                       static_cast<float>(M_PI) - 0.01f,           "wrapPi(pi-0.01)");
    requireNear(wrapPi(-static_cast<float>(M_PI) + 0.01f),
                      -static_cast<float>(M_PI) + 0.01f,           "wrapPi(-pi+0.01)");

    // Multiples of 2pi must collapse onto the same canonical value.
    float twoPi = 2.0f * static_cast<float>(M_PI);
    requireNear(wrapPi( twoPi + 0.5f), 0.5f, "wrapPi(2pi+0.5)");
    requireNear(wrapPi(-twoPi - 0.5f), -0.5f, "wrapPi(-2pi-0.5)");
    requireNear(wrapPi(3.0f * twoPi + 0.25f), 0.25f, "wrapPi(6pi+0.25)");

    // Exactly +pi must map to either +pi or -pi (atan2 picks +pi here on
    // glibc / musl). Either is fine for our control law, so we just check
    // the absolute value.
    float w = wrapPi(static_cast<float>(M_PI));
    requireBool(nearEq(std::fabs(w), static_cast<float>(M_PI), 1e-5f),
                "wrapPi(pi) maps to +-pi");

    // Tiny non-zero angles (numerical noise after long integration). These
    // should round-trip without being snapped to 0 by spurious clamping.
    requireNear(wrapPi( 1e-7f),  1e-7f, "wrapPi(+1e-7)");
    requireNear(wrapPi(-1e-7f), -1e-7f, "wrapPi(-1e-7)");

    // Many-turn multiples (a robot rotating in place for a while). 10 turns
    // is enough to exercise the modulo behavior without accumulating
    // float-precision noise above kEps; >100 turns hits ULP at ~1e-5 which
    // is below any control-loop relevance but above this test's eps.
    requireNear(wrapPi(10.0f * twoPi),         0.0f, "wrapPi(10 turns) -> 0");
    requireNear(wrapPi(10.0f * twoPi + 0.5f),  0.5f, "wrapPi(10 turns + 0.5) -> 0.5");

    // Idempotence: wrapPi is a projection, applying it twice is the same
    // as once. Without this property a feedback loop can oscillate.
    for (float a : {0.0f, 0.1f, 1.5f, 3.14f, -3.14f, 7.5f, -50.0f}) {
        requireNear(wrapPi(wrapPi(a)), wrapPi(a), "wrapPi idempotent");
    }

    std::printf("[ok] test_wrapPi\n");
}

// ---------------------------------------------------------------------------
// 2. yawFromQuat
// ---------------------------------------------------------------------------
void test_yawFromQuat() {
    using unitree::slam::climb::yawFromQuat;
    using unitree::slam::climb::wrapPi;

    // Identity quaternion -> yaw 0.
    requireNear(yawFromQuat(0, 0, 0, 1), 0.0f, "yawFromQuat(identity)");

    // Pure z-axis rotation: q = (0, 0, sin(yaw/2), cos(yaw/2)).
    auto qZ = [](float yaw) {
        struct Q { float qx, qy, qz, qw; };
        return Q{0.0f, 0.0f, std::sin(yaw * 0.5f), std::cos(yaw * 0.5f)};
    };

    {
        float yaw = static_cast<float>(M_PI) * 0.25f;  // +45 deg
        auto q = qZ(yaw);
        requireNear(yawFromQuat(q.qx, q.qy, q.qz, q.qw), yaw, "yawFromQuat(+45)");
    }
    {
        float yaw = -static_cast<float>(M_PI) * 0.5f;  // -90 deg
        auto q = qZ(yaw);
        requireNear(yawFromQuat(q.qx, q.qy, q.qz, q.qw), yaw, "yawFromQuat(-90)");
    }
    {
        float yaw = static_cast<float>(M_PI) * 0.5f;   // +90 deg
        auto q = qZ(yaw);
        requireNear(yawFromQuat(q.qx, q.qy, q.qz, q.qw), yaw, "yawFromQuat(+90)");
    }
    {
        // 180 deg yaw: result must be +-pi (libm picks +pi here).
        float yaw = static_cast<float>(M_PI);
        auto q = qZ(yaw);
        float got = yawFromQuat(q.qx, q.qy, q.qz, q.qw);
        requireBool(nearEq(std::fabs(got), static_cast<float>(M_PI), 1e-5f),
                    "yawFromQuat(180) maps to +-pi");
    }

    // Sanity: feeding a non-identity quat through wrapPi(yaw) should give
    // back the same value (yawFromQuat already normalizes via atan2).
    {
        float yaw = 0.7f;
        auto q = qZ(yaw);
        float got = yawFromQuat(q.qx, q.qy, q.qz, q.qw);
        requireNear(wrapPi(got), yaw, "yawFromQuat round-trip via wrapPi");
    }

    std::printf("[ok] test_yawFromQuat\n");
}

// ---------------------------------------------------------------------------
// 3. cteProject
// ---------------------------------------------------------------------------
void test_cteProject() {
    using unitree::slam::climb::cteProject;

    // Stair frame aligned with body x-axis (yaw=0): cos_s=1, sin_s=0.
    // Then dx_path = (xc - x0), dy_path = (yc - y0). Trivial check.
    {
        auto p = cteProject(2.5f, 0.3f, 0.0f, 0.0f, 1.0f, 0.0f);
        requireNear(p.dx_path, 2.5f, "cteProject yaw=0 dx");
        requireNear(p.dy_path, 0.3f, "cteProject yaw=0 dy");
    }

    // Stair frame at +90 deg (cos=0, sin=1). A point straight ahead in
    // body frame (xc=1, yc=0) is "to the right" of stair axis -> dy_path
    // should be -1, dx_path should be 0.
    {
        auto p = cteProject(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
        requireNear(p.dx_path,  0.0f, "cteProject yaw=+90 dx");
        requireNear(p.dy_path, -1.0f, "cteProject yaw=+90 dy");
    }

    // Translate the start point: dy must measure error relative to (x0, y0),
    // not to the body origin.
    {
        auto p = cteProject(2.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f);
        requireNear(p.dx_path, 1.0f, "cteProject translated dx");
        requireNear(p.dy_path, 0.0f, "cteProject translated dy");
    }

    // 45-deg stair: a body point exactly along the stair axis should have
    // dy_path = 0 (signed cross-track is zero on-axis).
    {
        float yaw = static_cast<float>(M_PI) * 0.25f;
        float c = std::cos(yaw), s = std::sin(yaw);
        // point 2 metres along stair axis from origin
        float xc = 2.0f * c, yc = 2.0f * s;
        auto p = cteProject(xc, yc, 0.0f, 0.0f, c, s);
        requireNear(p.dx_path, 2.0f, "cteProject yaw=45 on-axis dx");
        requireNear(p.dy_path, 0.0f, "cteProject yaw=45 on-axis dy");
    }

    std::printf("[ok] test_cteProject\n");
}

// ---------------------------------------------------------------------------
// 4. cteYawOffset / cteDesiredYaw
// ---------------------------------------------------------------------------
void test_cteYawOffset() {
    using unitree::slam::climb::cteYawOffset;
    using unitree::slam::climb::cteDesiredYaw;
    using unitree::slam::climb::wrapPi;

    const float K_y = 0.5f;
    const float maxOff = 0.52f;  // ~30 deg, mirrors keyDemo3 defaults

    // Small dy_path: not yet saturated, raw = -K_y * dy.
    requireNear(cteYawOffset(0.5f, K_y, maxOff), -0.25f,
                "cteYawOffset small +dy");
    requireNear(cteYawOffset(-0.4f, K_y, maxOff), 0.20f,
                "cteYawOffset small -dy");

    // Large positive dy_path: should saturate to -maxOff (steer right).
    requireNear(cteYawOffset(10.0f, K_y, maxOff), -maxOff,
                "cteYawOffset saturates to -maxOff");
    // Large negative dy_path: should saturate to +maxOff (steer left).
    requireNear(cteYawOffset(-10.0f, K_y, maxOff), maxOff,
                "cteYawOffset saturates to +maxOff");

    // dy_path == 0: offset is zero, desired_yaw == stair_yaw_body.
    {
        float stair = 0.4f;
        float desired = cteDesiredYaw(stair, 0.0f, K_y, maxOff);
        requireNear(desired, stair, "cteDesiredYaw on-axis = stair_yaw");
    }

    // wrapPi composition: stair_yaw close to +pi, large +dy -> steer right
    // means desired_yaw drops below pi. Make sure result stays inside
    // (-pi, pi].
    {
        float stair = 3.0f;  // close to pi (~3.14159)
        float desired = cteDesiredYaw(stair, 5.0f, K_y, maxOff);
        // raw offset = -2.5, clamped to -0.52; desired = wrapPi(stair-0.52)
        requireNear(desired, wrapPi(stair - maxOff),
                    "cteDesiredYaw near +pi, +dy");
        requireBool(desired >  -static_cast<float>(M_PI) - 1e-5f &&
                    desired <=  static_cast<float>(M_PI) + 1e-5f,
                    "cteDesiredYaw stays in (-pi, pi]");
    }

    std::printf("[ok] test_cteYawOffset\n");
}

// ---------------------------------------------------------------------------
// 5. yawError
// ---------------------------------------------------------------------------
void test_yawError() {
    using unitree::slam::climb::yawError;

    // Trivial.
    requireNear(yawError(0.5f, 0.5f), 0.0f, "yawError equal");
    requireNear(yawError(0.7f, 0.5f), 0.2f, "yawError +0.2");
    requireNear(yawError(0.5f, 0.7f), -0.2f, "yawError -0.2");

    // Wrap-around: desired just past +pi, current just before -pi -> the
    // "shortest" rotation is small and negative, NOT ~2pi.
    {
        float pi = static_cast<float>(M_PI);
        float desired = -pi + 0.1f;
        float current =  pi - 0.1f;
        // wrapPi(desired - current) = wrapPi(-2pi+0.2) = 0.2
        requireNear(yawError(desired, current), 0.2f, "yawError wraps over +-pi");
    }

    std::printf("[ok] test_yawError\n");
}

// ---------------------------------------------------------------------------
// 6. clampVyaw
// ---------------------------------------------------------------------------
void test_clampVyaw() {
    using unitree::slam::climb::clampVyaw;

    const float K_psi = 1.5f;
    const float vyaw_max = 0.4f;

    // Linear region: |K_psi * yerr| < vyaw_max
    requireNear(clampVyaw(0.1f, K_psi, vyaw_max),  0.15f, "clampVyaw linear +");
    requireNear(clampVyaw(-0.1f, K_psi, vyaw_max), -0.15f, "clampVyaw linear -");

    // Saturation region: huge yerr -> +-vyaw_max.
    requireNear(clampVyaw(10.0f,  K_psi, vyaw_max),  vyaw_max, "clampVyaw saturate +");
    requireNear(clampVyaw(-10.0f, K_psi, vyaw_max), -vyaw_max, "clampVyaw saturate -");

    // Boundary: exactly at +vyaw_max / K_psi -> should equal +vyaw_max.
    requireNear(clampVyaw(vyaw_max / K_psi, K_psi, vyaw_max), vyaw_max,
                "clampVyaw exactly on +bound");
    requireNear(clampVyaw(-vyaw_max / K_psi, K_psi, vyaw_max), -vyaw_max,
                "clampVyaw exactly on -bound");

    std::printf("[ok] test_clampVyaw\n");
}

// ---------------------------------------------------------------------------
// 7. clampClimbVx
// ---------------------------------------------------------------------------
void test_clampClimbVx() {
    using unitree::slam::climb::clampClimbVx;
    using unitree::slam::climb::kClimbVxMax;

    // Lock down kClimbVxMax: 1.0 m/s is the contract with hardware.
    requireNear(kClimbVxMax, 1.0f, "kClimbVxMax constant");

    // Negative input must clamp to 0 (we never want to back the dog off the
    // stairs by pretending negative vx is fine).
    requireNear(clampClimbVx(-0.5f, kClimbVxMax), 0.0f, "clampClimbVx negative -> 0");

    // Inside [0, kClimbVxMax].
    requireNear(clampClimbVx(0.35f, kClimbVxMax), 0.35f, "clampClimbVx default 0.35");
    requireNear(clampClimbVx(0.0f,  kClimbVxMax), 0.0f,  "clampClimbVx 0");

    // Above kClimbVxMax: clamp down hard. This is the safety the comment in
    // keyDemo3 calls out explicitly.
    requireNear(clampClimbVx(2.5f,  kClimbVxMax), kClimbVxMax,
                "clampClimbVx above kClimbVxMax");
    requireNear(clampClimbVx(1.0001f, kClimbVxMax), kClimbVxMax,
                "clampClimbVx just above kClimbVxMax");

    std::printf("[ok] test_clampClimbVx\n");
}

// ---------------------------------------------------------------------------
// 8. preAlignClippedErr
// ---------------------------------------------------------------------------
void test_preAlignClippedErr() {
    using unitree::slam::climb::preAlignClippedErr;

    const float align_limit = 0.0873f;  // ~5 deg, mirrors keyDemo3 default

    // Inside the band: passes through.
    requireNear(preAlignClippedErr(0.05f, align_limit), 0.05f,
                "preAlign inside band +");
    requireNear(preAlignClippedErr(-0.05f, align_limit), -0.05f,
                "preAlign inside band -");

    // Beyond the band: clipped at +-align_limit.
    requireNear(preAlignClippedErr(0.5f, align_limit), align_limit,
                "preAlign clip +");
    requireNear(preAlignClippedErr(-0.5f, align_limit), -align_limit,
                "preAlign clip -");

    // Exactly on the boundary.
    requireNear(preAlignClippedErr(align_limit, align_limit), align_limit,
                "preAlign exact +bound");
    requireNear(preAlignClippedErr(-align_limit, align_limit), -align_limit,
                "preAlign exact -bound");

    std::printf("[ok] test_preAlignClippedErr\n");
}

// ---------------------------------------------------------------------------
// 9. Integration: a single CTE step end-to-end
// ---------------------------------------------------------------------------
void test_ctePipelineIntegration() {
    using namespace unitree::slam::climb;

    // Scenario: dog is climbing, stair_yaw_body = 0 (i.e. straight ahead in
    // body frame), gain set is the keyDemo3 default. Body has drifted 0.4 m
    // to the +y side (left of stair axis). Expected behaviour:
    //   * dy_path = +0.4
    //   * cteYawOffset = -K_y * 0.4 = -0.2 (not yet saturated since |0.2| < 0.52)
    //   * desired_yaw = wrapPi(0 + -0.2) = -0.2
    //   * yerr = wrapPi(-0.2 - current_yaw)
    //   * vyaw = clamp(K_psi * yerr, +-vyaw_max)
    const float K_y = 0.5f;
    const float maxOff = 0.52f;
    const float K_psi = 1.5f;
    const float vyaw_max = 0.4f;

    float stair_yaw_body = 0.0f;
    float x0 = 0.0f, y0 = 0.0f;
    float cos_s = std::cos(stair_yaw_body), sin_s = std::sin(stair_yaw_body);

    float xc = 1.0f, yc = 0.4f;          // 1 m forward, 0.4 m off-axis (+y)
    float current_yaw = 0.0f;            // dog hasn't yet started turning

    auto proj = cteProject(xc, yc, x0, y0, cos_s, sin_s);
    requireNear(proj.dx_path, 1.0f, "integration dx");
    requireNear(proj.dy_path, 0.4f, "integration dy");

    float desired = cteDesiredYaw(stair_yaw_body, proj.dy_path, K_y, maxOff);
    requireNear(desired, -0.2f, "integration desired_yaw");

    float yerr = yawError(desired, current_yaw);
    requireNear(yerr, -0.2f, "integration yerr");

    float vyaw = clampVyaw(yerr, K_psi, vyaw_max);
    // K_psi*yerr = -0.3, |-0.3| < vyaw_max, so passes through.
    requireNear(vyaw, -0.3f, "integration vyaw linear");

    // Now push body way off-axis: dy_path large positive should saturate
    // both the offset clamp AND the vyaw clamp.
    {
        float yc_far = 5.0f;
        auto p2 = cteProject(xc, yc_far, x0, y0, cos_s, sin_s);
        float d2 = cteDesiredYaw(stair_yaw_body, p2.dy_path, K_y, maxOff);
        requireNear(d2, -maxOff, "integration desired saturated");
        float ye2 = yawError(d2, current_yaw);
        float vy2 = clampVyaw(ye2, K_psi, vyaw_max);
        requireNear(vy2, -vyaw_max, "integration vyaw saturated");
    }

    std::printf("[ok] test_ctePipelineIntegration\n");
}

}  // namespace

int main() {
    std::printf("=== test_climb_control: keyDemo3 stair-climb math ===\n");

    test_wrapPi();
    test_yawFromQuat();
    test_cteProject();
    test_cteYawOffset();
    test_yawError();
    test_clampVyaw();
    test_clampClimbVx();
    test_preAlignClippedErr();
    test_ctePipelineIntegration();

    std::printf("=== test_climb_control: ALL OK ===\n");
    return 0;
}
