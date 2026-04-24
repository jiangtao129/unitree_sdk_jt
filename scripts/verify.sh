#!/usr/bin/env bash
# =============================================================================
# Unique verification entry point for the unitree_sdk_jt repo.
#
# This is the *single source of truth* for "is this repo healthy?".
# Both human contributors and CI MUST invoke this exact script. Do NOT
# bypass it by running cmake / pytest by hand in different ways.
#
# What it does:
#   [1/2] cmake configure + build (x86_64 host build, Release)
#   [2/2] Python tooling smoke (data/demo1/view_map.py --selftest)
#         Skipped gracefully if numpy/open3d not installed.
#
# Out of scope (by design, NOT done here):
#   - aarch64 cross compile  -> runs only on the Go2 dock manually
#   - Hardware smoke         -> requires SSH to dock, manual only
#   - Style checks (clang-format etc.) -> add later in a follow-up PR
# =============================================================================
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

echo "================================================================"
echo " verify.sh @ $REPO_ROOT"
echo " host: $(uname -srm)"
echo "================================================================"
echo

# ---------------------------------------------------------------------
# [1/2] cmake build (x86_64 host)
# ---------------------------------------------------------------------
echo ">>> [1/2] cmake build (x86_64 host)"
BUILD_DIR="${BUILD_DIR:-build}"
cmake -B "$BUILD_DIR" -S . -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD_DIR" -j"$(nproc)"
echo "<<< cmake build OK  (artifacts in $BUILD_DIR/)"
echo

# ---------------------------------------------------------------------
# [2/2] Python tooling smoke
# ---------------------------------------------------------------------
echo ">>> [2/2] Python tooling smoke (view_map.py --selftest)"
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
