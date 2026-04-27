#!/usr/bin/env bash
# =============================================================================
# Unique verification entry point for the unitree_sdk_jt repo.
#
# This is the *single source of truth* for "is this repo healthy?".
# Both human contributors and CI MUST invoke this exact script. Do NOT
# bypass it by running cmake / pytest by hand in different ways.
#
# What it does:
#   [1/3] cmake configure + build (x86_64 host build, Release)
#   [2/3] keyDemo3 stair-climb math unit test (host-only, pure C++17)
#   [3/3] Python tooling smoke (data/demo1/view_map.py --selftest)
#         Skipped gracefully if numpy/open3d not installed.
#
# Out of scope (by design, NOT done here):
#   - aarch64 cross compile  -> runs only on the Go2 dock manually
#   - Hardware smoke         -> requires SSH to dock, manual only
#   - Style checks (clang-format etc.) -> add later in a follow-up PR
# =============================================================================
set -euo pipefail

# Print a clear summary on every exit path. With set -e enabled, an early
# failure normally surfaces as a generic non-zero exit and a wall of cmake
# stderr; the trap makes sure both success and failure end with one obvious
# line stating what happened, so CI logs and humans can scan it instantly.
_verify_summary() {
    local code=$?
    if [ "$code" -eq 0 ]; then
        :  # success path prints its own banner below
    else
        echo
        echo "================================================================"
        echo " verify.sh: FAILED (exit $code)"
        echo "================================================================"
    fi
}
trap _verify_summary EXIT

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

echo "================================================================"
echo " verify.sh @ $REPO_ROOT"
echo " host: $(uname -srm)"
echo "================================================================"
echo

# ---------------------------------------------------------------------
# [1/3] cmake build (x86_64 host)
# ---------------------------------------------------------------------
echo ">>> [1/3] cmake build (x86_64 host)"
BUILD_DIR="${BUILD_DIR:-build}"
cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j"$(nproc)"
echo "<<< cmake build OK  (artifacts in $BUILD_DIR/)"
echo

# ---------------------------------------------------------------------
# [2/3] keyDemo3 stair-climb math unit test (host-only)
# ---------------------------------------------------------------------
echo ">>> [2/3] keyDemo3 stair-climb math unit test"
TEST_BIN="$BUILD_DIR/bin/test_climb_control"
if [ -x "$TEST_BIN" ]; then
    "$TEST_BIN"
    echo "<<< stair-climb math unit test OK"
else
    echo "ERROR: $TEST_BIN not found - did the cmake build skip BUILD_CLIMB_TESTS?" >&2
    exit 1
fi
echo

# ---------------------------------------------------------------------
# [3/3] Python tooling smoke
# ---------------------------------------------------------------------
echo ">>> [3/3] Python tooling smoke (view_map.py --selftest)"
if command -v python3 >/dev/null 2>&1; then
    if [ -f data/demo1/view_map.py ]; then
        # selftest 自身处理依赖缺失: 缺 numpy/open3d 时 [skip]+exit 0;
        # 真错误 (PCD 缺失/损坏) 才会失败.
        python3 data/demo1/view_map.py --selftest
    else
        echo "[skip] data/demo1/view_map.py not found"
    fi
else
    echo "[skip] python3 not in PATH"
fi
echo

echo "================================================================"
echo " verify.sh: ALL OK"
echo "================================================================"
