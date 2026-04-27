# Changelog

All notable changes to the `jiangtao129/unitree_sdk_jt` fork are
listed here. Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning is informal — there are no release tags yet, just a
chronological log of merged PRs grouped by date.

The repository tracks `unitreerobotics/unitree_sdk2` as `upstream`;
pure upstream merges are not listed here.

---

## Unreleased

### Added
- Initial fork-on-top work: keyDemo3 stair climbing with CTE-based
  blind-walk, multi-floor task lists, explicit save/reload semantics,
  `view_map.py` PCD visualizer with open3d.ml stub workaround.
- Agent-driven development pipeline: `scripts/verify.sh` as the
  single source of truth, `AGENTS.md` with P0/P1/P2/P3 review
  guidelines for Codex, `.cursor/rules/00-workflow.mdc` and
  `.cursor/commands/ship.md`.
- GitHub Actions CI: cmake build + verify.sh, with concurrency,
  timeout, least-privilege permissions, build-info logging, build
  cache.
- ChatGPT Codex auto-review on every PR, configured per `AGENTS.md`
  P0/P1 guidelines.
- Repo hygiene: `CONTRIBUTING.md`, `SECURITY.md`, `.editorconfig`,
  `.gitattributes`.
- Vendored Unitree closed-source aarch64 binaries
  (`mid360_driver`, `unitree_slam`, `xt16_driver`).
- `doc/BACKLOG.md` tracking 80+ small improvement candidates split by
  priority for incremental work.

### Fixed
- `keyDemo3` / `keyDemo2`: clamp `climb_vx` to `[0, 1.0] m/s` before
  `SportClient::Move` (PR #6, PR #12).
- `keyDemo3` slam DDS handlers: guard `nlohmann::json::parse` against
  malformed payloads so a bad SLAM-server packet no longer aborts the
  process (PR #13).
- `keyDemo3` `saveTaskListFun`: atomic write via `.tmp + rename` so a
  crash mid-save no longer wipes the previous f*.json (PR #14).
- `keyDemo3` / `keyDemo2`: use `return EXIT_FAILURE` instead of
  `exit(-1)` (which truncates to 255) on the missing-arg path (PR #8).
- `keyDemo3` destructor: defensive `try/catch` around teardown so a
  throw in one cleanup step does not cause `std::terminate` to skip
  the rest (PR #32).

### Refactored
- `std::clamp` everywhere in the climb path (PR #15, #16, #17),
  replacing nested `std::max(-x, std::min(x, val))`.
- `kClimbLoopPeriodMs = 20` named constant (PR #24).
- `[[nodiscard]]` on every free function in `climb_control.hpp`
  (PR #25).
- `TeeBuf` class marked `final` (PR #26).
- Dead global `currentKey` removed (PR #31).

### Tested
- Host-only unit test for keyDemo3 stair-climb math
  (`unitree_slam/example/test/test_climb_control.cpp`, PR #9), wired
  into `verify.sh` step [2/3].

### Documentation
- Pipeline test report (PR #4).
- Cross-agent handoff guide for setting up the same pipeline on
  `dimos` (PR #7).
- Teammate onboarding guide (PR #10).

---

## How to update this file

When opening a PR that warrants an entry (any user-visible change,
fix, or refactor), append a bullet to the appropriate section under
**Unreleased**. Keep the format `- <change> (PR #N).` so cross-referencing
back to the GitHub PR is one click.
