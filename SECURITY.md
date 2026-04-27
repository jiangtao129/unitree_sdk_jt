# Security Policy

## Scope

This is a personal fork of `unitreerobotics/unitree_sdk2` adding
keyDemo3 stair-climbing + supporting infrastructure
(`scripts/verify.sh`, `.github/workflows/`, `.cursor/`, etc.). The
security surface this policy covers is everything *under* this fork's
own commits — i.e. anything authored by `jiangtao129` on top of the
upstream baseline.

For vulnerabilities in upstream code (under `example/`, `include/`,
`lib/`), please report directly to
[`unitreerobotics/unitree_sdk2`](https://github.com/unitreerobotics/unitree_sdk2/security)
instead.

## What counts as a security issue here

In this fork, the *physical-safety-relevant* attack surfaces are
specifically:

- **Robot motion commands**: any path that produces an unbounded
  `SportClient::Move(vx, vy, vyaw)` call (missing clamp, race condition
  feeding garbage values, deserialization that bypasses validation).
  See `AGENTS.md` §7.2 P0-3.
- **DDS message handlers**: any callback that crashes the process on
  malformed input from `slam_server` (parse without try/catch, array
  index without bounds, etc.). See `AGENTS.md` §7.2 P0-4.
- **Persisted state corruption**: any path that can leave
  `f1.json` / `f2.json` (waypoint task lists) or saved PCDs in a state
  that, on next reload, makes the dog walk somewhere it should not.
- **Pipeline integrity**: anything that lets unauthenticated code reach
  CI / merge / release without going through the documented PR +
  required-checks flow.

Generic information-disclosure / DoS issues that do not chain to
physical-safety risk are also welcome but lower priority.

## How to report

**Do not** open a public issue for an unpatched vulnerability.

Use **GitHub Private Vulnerability Reporting** on this repo:
<https://github.com/jiangtao129/unitree_sdk_jt/security/advisories/new>

Include:
- Affected file(s) and commit SHA.
- Repro steps or proof-of-concept.
- Impact assessment (does it reach a SportClient::Move call? Crash?
  State corruption?).
- Whether the issue exists upstream or is unique to this fork.

## What to expect

This is a personal repo, not a funded project. Best-effort response
window:

- Initial acknowledgement: within 5 working days.
- Triage / CVE-worthy decision: within 2 weeks.
- Fix or accepted-risk decision: as soon as practical, no SLA.

I will credit reporters in the security advisory unless they request
anonymity.

## Supported versions

Only the current `main` branch is supported. The repo does not cut
release tags; if you need a stable point, pin to a specific commit SHA.

## What is explicitly out of scope

- Changes that would silently disable `bash scripts/verify.sh` or
  GitHub branch-protection: these are the integrity boundary, not a
  vuln to be reported privately. If you find a way to bypass branch
  protection on `main`, **that** is in scope (report it privately).
- Findings inside vendored closed-source binaries
  (`unitree_slam/lib/*.so`, `unitree_slam/bin/{mid360_driver,
  unitree_slam, xt16_driver}`): these are shipped as-is from Unitree
  and I do not have the source. Report to Unitree directly.
- Issues in dependencies pulled in by the upstream SDK
  (`unitree_sdk2`'s own deps): report upstream.
