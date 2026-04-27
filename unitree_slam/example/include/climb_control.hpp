// =============================================================================
// climb_control.hpp
//
// Pure C++17, header-only reference implementation of the math used by
// `unitree::robot::slam::TestClient::climbStairsFun()` in
// `unitree_slam/example/src/keyDemo3.cpp` (keyDemo3 stair-climb CTE controller).
//
// WHY THIS HEADER EXISTS
//   - keyDemo3.cpp is built only on the Go2 dock (aarch64) against closed-source
//     SLAM .so libs and DDS, so it is NOT compiled by the host `verify.sh`.
//   - The control law itself (CTE projection, yaw offset clamp, vyaw clamp,
//     pre-align step) is plain math with no DDS or hardware dependency.
//   - To get *some* CI coverage on that math, this header re-states it as
//     pure functions and `test_climb_control.cpp` exercises them on host.
//
// CONTRACT WITH keyDemo3.cpp
//   - Every helper here MUST stay byte-for-byte equivalent to the corresponding
//     expression in keyDemo3.cpp::climbStairsFun(). If you tweak the math in
//     keyDemo3.cpp, mirror the change here AND update the unit test, otherwise
//     verify.sh stops being a meaningful gate.
//   - Hardware regression on the dog (climb stairs end-to-end) is still
//     required for any change to the *gains* (climb_vx, K_y, K_psi, vyaw_max,
//     max_yaw_offset, align_limit, align_tol, align_timeout). This unit test
//     only locks down algebraic correctness, not tuning.
// =============================================================================

#ifndef UNITREE_SLAM_CLIMB_CONTROL_HPP
#define UNITREE_SLAM_CLIMB_CONTROL_HPP

#include <algorithm>
#include <cmath>

namespace unitree {
namespace slam {
namespace climb {

// Hard upper bound on forward velocity actually fed to SportClient::Move on the
// dog. Mirrors `TestClient::kClimbVxMax` in keyDemo3.cpp. Keeping it here so
// the unit test can lock down the value we ship.
inline constexpr float kClimbVxMax = 1.0f;

// Wrap angle to (-pi, pi] in a numerically robust way. Handles any multiples
// of 2*pi, NaN-safe within whatever atan2 of the host libm guarantees.
[[nodiscard]] inline float wrapPi(float a) {
    return std::atan2(std::sin(a), std::cos(a));
}

// Extract z-axis yaw from a unit quaternion (qx, qy, qz, qw). Result in
// radians, in (-pi, pi]. Same formula as keyDemo3.cpp.
[[nodiscard]] inline float yawFromQuat(float qx, float qy, float qz, float qw) {
    float siny_cosp = 2.0f * (qw * qz + qx * qy);
    float cosy_cosp = 1.0f - 2.0f * (qy * qy + qz * qz);
    return std::atan2(siny_cosp, cosy_cosp);
}

// Projection of a body-frame point (xc, yc) into the stair-aligned frame
// rooted at (x0, y0) with stair_yaw described by (cos_s, sin_s). Returns:
//   dx_path = signed along-stair distance (forward = positive)
//   dy_path = signed cross-track error (left of stair-axis = positive)
// Mirrors the inline math in the CTE main loop.
struct CteProj {
    float dx_path;
    float dy_path;
};

[[nodiscard]] inline CteProj cteProject(float xc, float yc,
                                        float x0, float y0,
                                        float cos_s, float sin_s) {
    float dpx = xc - x0;
    float dpy = yc - y0;
    CteProj out;
    out.dx_path =  dpx * cos_s + dpy * sin_s;
    out.dy_path = -dpx * sin_s + dpy * cos_s;
    return out;
}

// CTE -> desired yaw offset relative to stair_yaw_body. Clamped at
// +-max_yaw_offset to keep the dog from steering too aggressively even when
// dy_path blows up (e.g. transient SLAM jump).
//   raw_offset = -K_y * dy_path
//   return clamp(raw_offset, +-max_yaw_offset)
[[nodiscard]] inline float cteYawOffset(float dy_path, float K_y, float max_yaw_offset) {
    float raw = -K_y * dy_path;
    return std::max(-max_yaw_offset, std::min(max_yaw_offset, raw));
}

// Compose stair_yaw_body + clamped CTE offset, wrapped to (-pi, pi].
[[nodiscard]] inline float cteDesiredYaw(float stair_yaw_body,
                                         float dy_path,
                                         float K_y,
                                         float max_yaw_offset) {
    return wrapPi(stair_yaw_body + cteYawOffset(dy_path, K_y, max_yaw_offset));
}

// Yaw error in (-pi, pi], properly wrapped. Mirrors `wrapPi(desired_yaw - yawc)`.
[[nodiscard]] inline float yawError(float desired_yaw, float current_yaw) {
    return wrapPi(desired_yaw - current_yaw);
}

// P controller on yaw error -> vyaw, clamped to +-vyaw_max. Mirrors:
//   vyaw = clamp(K_psi * yerr, +-vyaw_max)
// Used in BOTH the pre-align loop and the CTE main loop.
[[nodiscard]] inline float clampVyaw(float yerr, float K_psi, float vyaw_max) {
    float raw = K_psi * yerr;
    return std::max(-vyaw_max, std::min(vyaw_max, raw));
}

// Forward velocity safety clamp. Even if climb_vx is mistakenly tuned higher
// than kClimbVxMax (typo / runtime patch), this clamp keeps the actual
// SportClient::Move command inside the stairs-safe envelope.
//   return clamp(vx, [0, vmax])
[[nodiscard]] inline float clampClimbVx(float vx, float vmax) {
    return std::clamp(vx, 0.0f, vmax);
}

// Pre-align single-shot heading-error clipper. The pre-align step turns the
// dog by at most +-align_limit radians on entry; the CTE loop then handles
// the residual error. Mirrors:
//   clipped_err = clamp(real_err, +-align_limit)
[[nodiscard]] inline float preAlignClippedErr(float real_err, float align_limit) {
    return std::max(-align_limit, std::min(align_limit, real_err));
}

}  // namespace climb
}  // namespace slam
}  // namespace unitree

#endif  // UNITREE_SLAM_CLIMB_CONTROL_HPP
