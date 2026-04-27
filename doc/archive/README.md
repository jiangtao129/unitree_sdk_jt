# `doc/archive/`

Historical documents kept for traceability. Don't follow them as
current operational guidance — they reflect early planning that has
since been superseded by what landed on `main`.

## Files

- `dimos_huazhijian_multi_agent_setup_guide_zh.md` /
  `dimos_huazhijian_multi_agent_setup_guide_zh.pdf` — the original
  agent-pipeline setup template (生成于早期, 当时假设的仓库名是
  `dimos_huazhijian`). The actual pipeline that shipped on this fork
  diverged in several places (LFS handling, branch protection details,
  Codex `+1` vs `eyes` semantics, REST-API PR creation to dodge the
  `gh pr create` GraphQL race). For the up-to-date version see:
  - `AGENTS.md`
  - `.cursor/commands/ship.md`
  - `doc/agent_pipeline_onboarding_for_teammate.md`
  - `doc/dimos_pipeline_setup_for_agent.md`
