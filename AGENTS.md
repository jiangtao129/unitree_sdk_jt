# AGENTS.md — Repo-level rules for Cursor / Codex / any other coding agents

> 这个仓库 (`unitree_sdk_jt`) 是 `unitreerobotics/unitree_sdk2` 的个人 fork,
> 在它之上加了 `keyDemo3` 爬楼控制、CTE 路径跟随、任务列表、`view_map.py`
> 等本地开发资产。**任何 agent 在改本仓库时,必须遵循下面的规则**。

---

## 1. 仓库目标 (Definition of Done)

把"提需求 → 改代码 → 本地验证 → push 分支 → 创建 PR → CI 跑 verify → 自动合并到 main"
配成稳定可重复的流水线。任何任务的"完成"都意味着:

1. 代码改动可在本地通过 `bash scripts/verify.sh`。
2. 改动以 **feature/fix branch + PR** 进入 `main`,不直接推 `main`。
3. PR 的 CI 检查全绿。
4. 如果 GitHub 网页操作 (auto-merge / branch protection / Codex App)
   不能被 agent 自动完成,**必须明确输出"人工一次性待办清单"**。

---

## 2. 唯一验证命令 (Single Source of Truth)

```bash
bash scripts/verify.sh
```

它做两件事:

- **[1/2]** `cmake -B build && cmake --build build -j$(nproc)` (x86_64 host)
- **[2/2]** `python3 data/demo1/view_map.py --selftest` (Python 工具自测; 缺
  numpy/open3d 时优雅 skip,不算失败)

不在范围内 (设计如此,**禁止**塞进 verify.sh):

- aarch64 交叉编译 → 只在 Go2 dock 上手动跑
- 硬件 smoke (爬楼/导航/relocation) → 只能 SSH 到 dock 手动跑
- clang-format / clang-tidy → 后续 follow-up PR 再加

---

## 3. 强制规则 (任何 agent 不得违反)

### 3.1 Git / 分支
- **禁止直接 push `main`**。一律走 `feature/<topic>` 或 `fix/<bug>` 分支 + PR。
- **禁止 `git push --force` 到 `main`** 或任何被他人引用的远端分支。
- 一次 PR 聚焦一件事;不要把无关重构混进来。

### 3.2 Build / 验证
- 任何代码改动后,**必须**在本地跑过一次 `bash scripts/verify.sh` 才能 push。
- **禁止**为了让 verify / CI 变绿而注释掉测试、删掉真实检查或把失败硬改成 `exit 0`。
- 如果 verify 失败,先定位修复;**禁止**直接绕过它。

### 3.3 文件 / 体积
- **禁止**把以下东西 commit 进仓库 (已经在 `.gitignore` 里):
  - `unitree_slam/bin/` 下的 ELF 可执行文件、运行日志
  - `__pycache__/`、`*.pyc`、`*.log`
  - `build/`、`.cache/`、`.vscode/`
- 单文件 > 50 MB 必须先和 owner 确认是否走 Git LFS,**禁止**直接 commit。
- 闭源动态库 (`unitree_slam/lib/*.so`) 已入库,不要随意改。

### 3.4 配置 / 密钥
- **禁止**把 token、密码、私网 IP、SSH key 写进仓库 (`.env`, `credentials.*`,
  `*.pem` 等)。
- `~/.ssh/config` 里的 `192.168.x.x` 等 dock IP 不要进 git diff。

### 3.5 第一次合并的最小变更原则
- 流水线骨架 (verify.sh / AGENTS.md / .cursor / .github) 第一发上线时,
  **不要**顺手做大规模代码重构、文件改名、目录搬迁。
- 历史债务先记成 issue/TODO,不在第一发的 PR 里清理。

---

## 4. PR 规则

### 4.1 标题格式
```
<type>: <一句话>
```
`<type>` 取自: `feat` / `fix` / `chore` / `docs` / `refactor` / `ci` / `test`。

### 4.2 PR 描述必须包含
1. **Summary**: 改了什么、为什么改 (1~3 个 bullet)。
2. **Test plan**: 跑了哪些命令验证 (至少包含 `bash scripts/verify.sh`)。
3. **Risk / 影响面**: 这次改动可能影响哪些模块、是否需要硬件回归测试。
4. **Linked issue / context**: 如有。

### 4.3 不要做的事
- 不要在 PR 描述里贴未脱敏的日志(含 IP、Token、私有路径)。
- 不要在 commit/PR 里写 "fix everything"、"misc updates" 这种含糊标题。

---

## 5. 技术栈速查 (给新接手的 agent)

| 项 | 值 |
|---|---|
| 主语言 | C++17 + CMake |
| 主入口 | `unitree_slam/example/src/keyDemo3.cpp` (爬楼/CTE/任务列表) |
| 闭源依赖 | `unitree_slam/lib/*.so` (Unitree SLAM runtime) |
| 工具脚本 | `data/demo1/view_map.py` (PCD + 航点可视化, 内嵌 open3d.ml stub) |
| 配置 | `unitree_slam/config/pl_mapping/mid360.yaml` (Go2_W 段有详细中文注释) |
| 部署目标 | Go2 EDU/Developer dock @ `/home/unitree/jiangtao/unitree_sdk2/` (aarch64) |
| 上游同步 | `git fetch upstream && git merge upstream/main` (远端 `upstream` 指向官方) |

---

## 6. 角色分工 (建议)

| 角色 | 谁来 | 做什么 |
|---|---|---|
| Orchestrator + Coding + Debug + Git/PR | **Cursor 本地 Agent** | 识别任务、改代码、跑 verify、push、开 PR、开 auto-merge |
| Review (第二视角) | **Codex GitHub App** | PR 上做回归/破坏性/漏测检查,只评论不直接改同一分支 |
| 人类 (你) | **owner** | 提需求、看 PR、做 GitHub 网页一次性配置、跑硬件回归 |

不允许的越权行为:
- Review agent **不要**直接 push 同一 PR 分支去"顺手修一下"。
- 任何 agent **不要**替人勾 "Auto-merge" / "Branch protection" 这种网页设置;
  只能输出待办清单提醒人类去做。
