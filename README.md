# unitree_sdk2

[![CI](https://github.com/jiangtao129/unitree_sdk_jt/actions/workflows/c-cpp.yml/badge.svg?branch=main)](https://github.com/jiangtao129/unitree_sdk_jt/actions/workflows/c-cpp.yml)

Unitree robot sdk version 2.

> This is a personal fork (`jiangtao129/unitree_sdk_jt`) of the upstream
> [unitreerobotics/unitree_sdk2](https://github.com/unitreerobotics/unitree_sdk2)
> with on-top work for Go2 EDU stair-climbing (`unitree_slam/example/src/keyDemo3.cpp`)
> and an agent-driven CI/PR pipeline. See `AGENTS.md` and `CONTRIBUTING.md`
> for fork-specific rules; everything else below is upstream.

### Prebuild environment
* OS  (Ubuntu 20.04 LTS)  
* CPU  (aarch64 and x86_64)   
* Compiler  (gcc version 9.4.0) 

### Environment Setup

Before building or running the SDK, ensure the following dependencies are installed:

- CMake (version 3.10 or higher)
- GCC (version 9.4.0)
- Make

You can install the required packages on Ubuntu 20.04 with:

```bash
apt-get update
apt-get install -y cmake g++ build-essential libyaml-cpp-dev libeigen3-dev libboost-all-dev libspdlog-dev libfmt-dev
```

### Build examples

To build the examples inside this repository:

```bash
mkdir build
cd build
cmake ..
make
```

### Installation

To build your own application with the SDK, you can install the unitree_sdk2 to your system directory:

```bash
mkdir build
cd build
cmake ..
sudo make install
```

Or install unitree_sdk2 to a specified directory:

```bash
mkdir build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/opt/unitree_robotics
sudo make install
```

You can refer to `example/cmake_sample` on how to import the unitree_sdk2 into your CMake project. 

Note that if you install the library to other places other than `/opt/unitree_robotics`, you need to make sure the path is added to "${CMAKE_PREFIX_PATH}" so that cmake can find it with "find_package()".

### Notice
For more reference information, please go to [Unitree Document Center](https://support.unitree.com/home/zh/developer).

---

## Fork-specific: Agent-driven dev pipeline

This fork (`jiangtao129/unitree_sdk_jt`) ships a one-command verify
script + a Cursor `/ship` slash command + GitHub Actions CI + Codex
auto-review. End-to-end:

1. Open Cursor in this repo, type `/ship <one-line need>`.
2. The local Cursor agent creates a `feature/...` branch, edits files,
   runs `bash scripts/verify.sh` (cmake build + climb-math unit test +
   `view_map.py --selftest`).
3. On success it pushes, opens a PR via the GitHub REST API, and
   enables auto-merge with `--squash --delete-branch`.
4. GitHub Actions CI runs the same `bash scripts/verify.sh`. ChatGPT
   Codex Connector posts a `+1` reaction (or written review) within
   ~1 minute.
5. Once required check `build` is green, GitHub auto-squash-merges
   and deletes the head branch. Local main fast-forwards.

Single source of truth: `bash scripts/verify.sh`. Nobody bypasses it,
not even the maintainer (branch protection enforces it).

Detailed reading order:
- `AGENTS.md`             – hard rules + Codex review guidelines
- `.cursor/commands/ship.md` – the 11-step `/ship` workflow
- `.cursor/rules/00-workflow.mdc` – Cursor `alwaysApply` constraints
- `scripts/verify.sh`     – the one verify entry point
- `doc/BACKLOG.md`        – current open improvement items
- `CONTRIBUTING.md`       – PR template + commit style
- `SECURITY.md`           – how to report vulnerabilities privately
- `doc/pipeline_test_report_20260424.md` – initial end-to-end test log

Onboarding a colleague to set up the same pipeline on their own repo:
- `doc/agent_pipeline_onboarding_for_teammate.md` – zero-assumption guide
