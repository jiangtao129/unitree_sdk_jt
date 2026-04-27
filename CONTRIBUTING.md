# Contributing to `unitree_sdk_jt`

Thanks for your interest. This is a personal fork of
[`unitreerobotics/unitree_sdk2`](https://github.com/unitreerobotics/unitree_sdk2)
with on-top work for **Go2 EDU stair-climbing + CTE-based blind-walk +
multi-floor task lists** (`unitree_slam/example/src/keyDemo3.cpp`).

PRs that improve `keyDemo3` / `keyDemo2` / the supporting infrastructure
(`scripts/verify.sh`, `.cursor/`, `.github/`) are welcome. PRs that
modify upstream files (`example/`, `include/unitree/`, `lib/`) will be
closed and you should send them to upstream instead.

---

## TL;DR

```bash
git clone git@github.com:jiangtao129/unitree_sdk_jt.git
cd unitree_sdk_jt
bash scripts/verify.sh             # build + climb-math test + python smoke
git checkout -b feature/<topic>
# ...edit, commit
git push -u origin feature/<topic>
gh pr create                       # or use the REST API path documented below
```

The Cursor `/ship` command in `.cursor/commands/ship.md` automates all
of this if you're working from inside the IDE.

---

## Hard rules

These come from `AGENTS.md` and apply to **everyone**, including the
maintainer:

1. **No direct push to `main`.** Branch protection enforces it; `gh push
   origin main` will be rejected.
2. **No `git push --force`** to any shared branch.
3. **Every PR must pass `bash scripts/verify.sh` locally** before push.
   CI runs the same script; do not let CI catch what local should have.
4. **No secrets in commits.** No tokens, no PATs, no SSH keys, no
   private network IPs in committed files.
5. **No `unitree_slam/bin/keyDemo*` binaries.** Source is in
   `unitree_slam/example/src/keyDemo*.cpp`; binaries are rebuilt on the
   dock or via `verify.sh`.

---

## What to put in a PR

Use the template `.github/PULL_REQUEST_TEMPLATE.md` — it auto-loads in
the GitHub UI. Required sections:

- **Summary**: 1-3 bullets. Why, not what.
- **Test plan**: at minimum `bash scripts/verify.sh` output. If your
  change touches the climb control loop, also run
  `cmake --build /tmp/build_slam_example --target keyDemo3` and confirm
  Linking succeeds (verify.sh does not currently build keyDemo3
  directly; see `BACKLOG.md` #P0-08).
- **Risk**: which modules are affected? Hardware regression needed on
  the Go2 dock? Breaking change?
- **Related**: linked issues, BACKLOG.md item id (`#P0-XX` / `#P1-XX`),
  prior PRs.

---

## Commit message style

Conventional Commits: `<type>: <one-liner>`. Types: `feat`, `fix`,
`chore`, `docs`, `refactor`, `test`, `ci`. Body should explain *why*
the change exists, not what diff lines say.

If your PR closes a BACKLOG item, append `(BACKLOG #X-Y)` to the title
so the cross-reference shows up in `git log` searches.

Examples (from real history of this repo):

- `fix(keyDemo3): clamp climb_vx to [0, 1.0] m/s before SportClient::Move`
- `chore(ci): bump actions/checkout to v5 (BACKLOG #P1-C01)`
- `refactor(climb_control): mark free functions [[nodiscard]] (BACKLOG #P1-A06)`

---

## Codex review

This repo has the [ChatGPT Codex Connector] GitHub App installed and
"automatic code review" enabled for every PR. Within ~1 minute of
opening a PR, `chatgpt-codex-connector[bot]` will either:

- Leave a `+1` reaction on the PR description (means "looked, no
  blocking concerns") — most simple PRs land here, no action needed.
- Leave an `eyes` reaction and post a written review (means "I have
  P0/P1 concerns to flag") — read the review, address inline.

The repo's `AGENTS.md` §7 spells out which findings count as P0 (block
merge) vs P1 (recommended fix) vs P3 (do-not-flag noise) for this
codebase, so review output stays focused on real risks rather than
typo-hunting.

---

## Hardware regression

For changes that touch:

- the climb control loop (gains, frequency, command shape)
- SLAM yaml configs (`unitree_slam/config/pl_*/`)
- IMU/LiDAR extrinsics
- DDS topic names / SportClient call shape

…we expect a manual hardware check on a Go2 EDU dock before merge.
Note that fact in the PR Risk section so reviewers know.

For docs / tooling / CI changes, no hardware test is needed.

---

## BACKLOG and the 100-commit milestone

`doc/BACKLOG.md` tracks 80+ small improvement items split by priority
(P0 > P1 > P2 > P3). Pick an open item, open one PR per item, and tick
the checkbox in the same PR. Do not grade-jump (don't burn through P3
typo fixes when P0 fixes are still open).

---

## Questions

Open an issue or ping the maintainer (jiangtao129) on GitHub.

[ChatGPT Codex Connector]: https://github.com/apps/chatgpt-codex-connector
