# Agent-Driven Dev Pipeline — 端到端测试报告

> **状态**: 历史快照, 反映 2026-04-24 当时配置. 流水线机制本身没有变,
> 之后的 PR (#11 起) 沿用同一套. 数字 (PR 编号 / commit SHA / Codex react
> 时延) 仅指当时. 想看最新进度: `git log main --oneline | head -30` 或
> `doc/BACKLOG.md` 顶部进度条.

- **首次验证日期**: 2026-04-24
- **报告最后更新**: 2026-04-27
- **仓库**: `git@github.com:jiangtao129/unitree_sdk_jt.git`
- **本地路径**: `/home/jiangtao/huazhijian/project/old_go2/unitree_sdk2/`
- **当时 main HEAD**: `46ca98b` (PR #3 squash commit)
- **结论**: ✅ **流水线完整跑通**;branch protection 生效;Codex 自动 review 集成生效

---

## 1. 基础设施配置(全部完成)

### 1.1 网络与远端
| 配置项 | 状态 | 证据 |
|---|---|---|
| SSH-over-443 走通 (绕国内 22 端口屏蔽) | ✅ | `~/.ssh/config` 含 `Host github.com / Port 443` 条目;`ssh -T git@github.com` 返回 `Hi jiangtao129!` |
| `origin` = `git@github.com:jiangtao129/unitree_sdk_jt.git` | ✅ | `git remote -v` 校验 |
| `upstream` = `https://github.com/unitreerobotics/unitree_sdk2.git` | ✅ | 保留上游连接,可 `git fetch upstream` 同步 |
| `gh` CLI 2.91.0 已装于 `~/.local/bin/gh`,登录账号 `jiangtao129`,git_protocol=ssh | ✅ | `gh auth status` 验证 |

### 1.2 仓库内文件骨架
| 文件 | 作用 | 状态 |
|---|---|---|
| `scripts/verify.sh` | **唯一真相验证入口**(cmake build + view_map selftest) | ✅ 本地 + CI 双跑通过 |
| `AGENTS.md` | 仓库级 agent 行为规范 + Codex review guidelines (P0/P1/P2/P3 分级) | ✅ |
| `.cursor/rules/00-workflow.mdc` | Cursor alwaysApply 规则,锁住 branch+PR 流程 | ✅ |
| `.cursor/commands/ship.md` | `/ship <一句话需求>` slash 命令,11 步自动跑完 | ✅ |
| `.github/workflows/c-cpp.yml` | CI 跑 cmake build,**末尾追加** `bash scripts/verify.sh` | ✅ |
| `.github/PULL_REQUEST_TEMPLATE.md` | 标准化 PR 描述 (Summary / Test plan / Risk / Related) | ✅ |
| `.gitignore` | 排除 `unitree_slam/bin/` / `__pycache__` / `*.pyc` / `*.log` / `*.bak` / `*.swp` / `*~` | ✅ |

### 1.3 GitHub 仓库设置
| 设置 | 状态 | API 证据 |
|---|---|---|
| Branch protection rule on `main` | ✅ | API: `branches/main/protection` 返回 200 |
| - Require PR before merging | ✅ | `required_pull_request_reviews` 存在 |
| - Required status checks: **`build`** (来自 C/C++ Build Workflow) | ✅ | `required_status_checks.contexts: ['build']` |
| - Strict (require up-to-date branch) | ✅ | `required_status_checks.strict: true` |
| - **Do not allow bypassing**(admin 也挡) | ✅ | `enforce_admins.enabled: true` |
| - 不允许 force push / 删 main | ✅ | `allow_force_pushes: false`, `allow_deletions: false` |
| Approvals required count | 0 (按"模式 A,CI 绿即合") | `required_approving_review_count: 0` |
| Allow auto-merge | ✅ | `gh pr merge --auto` 工作正常 |
| Auto-delete head branches | ✅ | PR 合并后远端 head 分支被自动清掉 |

### 1.4 Codex GitHub App
| 设置 | 状态 | 证据 |
|---|---|---|
| `ChatGPT Codex Connector` 已装在 jiangtao129 个人账号 | ✅ | 用户截图 `Settings → Applications → Installed GitHub Apps` |
| Repository access: All repositories | ✅ | 用户截图 |
| 权限: Read checks/commit-status/metadata + Read+Write actions/code/issues/PRs/workflows | ✅ | 用户截图 |
| ChatGPT 后台: `unitree_sdk_jt` 已开"审查所有拉取请求" | ✅ | 用户截图 chatgpt.com/codex/settings/code-review |

---

## 2. 三个 PR 端到端验证(全部 MERGED, CI 全绿)

| PR | 标题 | 创建 → 合并 | CI 结果 | Codex 反应 | merge commit |
|---|---|---|---|---|---|
| **#1** | `chore: bootstrap agent-driven dev pipeline` | 08:08:00 → 08:10:43 (2m43s) | ✅ build success | 0 (此时 Codex 后台 review 开关刚开,**未触发**) | `0771563` |
| **#2** | `docs: add Codex review guidelines to AGENTS.md` | 09:27:57 → 09:28:16 (**19s**) | ✅ success (PR 已合后 CI 才跑完) | 0 (PR 19 秒就合,Codex **来不及**) | `e747cba` |
| **#3** | `chore: ignore *.bak and editor swap files` | 09:45:19 → 09:47:38 (2m19s) | ✅ success | **1 个 `+1` reaction** by `chatgpt-codex-connector[bot]` @ 09:46:20 | `46ca98b` |

PR 链接(全部已合,可点开看原始页面):
- https://github.com/jiangtao129/unitree_sdk_jt/pull/1
- https://github.com/jiangtao129/unitree_sdk_jt/pull/2
- https://github.com/jiangtao129/unitree_sdk_jt/pull/3

### 2.1 三个 PR 的关键差异分析

#### PR #1: 流水线骨架 PR
- **场景**: 把 `scripts/verify.sh` / `AGENTS.md` / `.cursor/` / `.github/workflows` 一次性引入
- **关键现象**: 这是流水线**还没启动**前合的,所以 branch protection 还没设,直接走 `gh pr merge --squash` 即合
- **教训**: bootstrap PR 是特殊情况,**之后的 PR 必须走完整保护**

#### PR #2: 加 Codex review guidelines 的 PR
- **场景**: 给 AGENTS.md 追加 88 行 review guidelines(给 Codex 看的)
- **关键现象**: 此时 branch protection 规则**存在但 contexts 为 `[]` 空** ⇒ "所有 required checks 通过" 在空集时自动为真 ⇒ `gh pr merge --auto` **立即合并**(19 秒)
- **暴露的 bug**: 截图里 "Require status checks" 勾了 checkbox 但搜索框里没把 `build` 加进列表
- **教训**: 仅勾 checkbox 不够,**必须在搜索框里把具体 check 名字加到 "Status checks that are required" 列表**

#### PR #3: 真实流水线运行 PR (★关键验证)
- **场景**: 给 `.gitignore` 加 4 个 pattern (`*.bak` / `*.bak.*` / `*.swp` / `*~`)
- **branch protection 已修好**(`contexts: ['build']` + `enforce_admins: true`)
- **关键现象**:
  - PR opened @ 09:45:25
  - **09:45:36 `gh pr merge --auto` 触发** ⇒ PR 状态 `OPEN`,`autoMergeRequest: SQUASH`(**没立即合,因为 branch protection 真生效了**)
  - **09:46:20 `chatgpt-codex-connector[bot]` 给 PR 贴 `+1` reaction**(55 秒反应,真实 Codex 集成生效)
  - 09:47:37 CI build 完成 `success`
  - **09:47:38 GitHub 自动 squash merge**(满足"PR + CI 绿"两个条件后立即合)
  - 远端 `chore/cleanup-bak-and-ignore` 分支自动删除
- **教训**: 流水线行为完全符合预期

---

## 3. 三大验证点的证据

### 3.1 ✅ Branch protection 真挡得住

PR #3 关键时间线证明:

```
09:45:25  PR opened
09:45:36  gh pr merge --auto 触发
          ↓
          PR 状态变 "auto-merge enabled, waiting for required status checks"
          ↓ (没像 PR #2 那样秒合)
09:47:37  CI build SUCCESS
09:47:38  GitHub 检测到所有 required checks 满足 → 自动 squash merge
```

如果 branch protection 没生效,gh pr merge --auto 会和 PR #2 一样秒合,不会等 CI。

### 3.2 ✅ CI 与本地 verify.sh 一致

每个 PR 的 CI run 都做了完全相同的两步:
1. `cmake build`(原 c-cpp.yml 已有)
2. `bash scripts/verify.sh`(我们新追加的)

`verify.sh` 内部 = cmake build + `view_map.py --selftest`。所以 CI 包含 cmake build 跑两次(一次原步骤,一次 verify.sh 内部),第二次因为 build 已 cached 几乎瞬完;再加 selftest,总耗时仍然只有 ~2 分 10 秒。

3 次 CI 全部 `conclusion: success`。

### 3.3 ✅ Codex 自动 review 真在工作 (★)

**PR #3 上的关键证据**:
- API: `repos/jiangtao129/unitree_sdk_jt/issues/3/reactions` 返回:
  ```json
  [{"user": {"login": "chatgpt-codex-connector[bot]"},
    "content": "+1",
    "created_at": "2026-04-24T09:46:20Z"}]
  ```

**为什么是 `+1` 不是 `eyes`,这不是 bug 是预期**:
- `eyes` (👀) = "我开始 review 了" (Codex 处理大 PR / 复杂 PR 时常见的中间态)
- `+1` (👍) = "我看完了,觉得没问题,不需要写正式评论"
- PR #3 只改 `.gitignore` 加 4 个 pattern,Codex 按 AGENTS.md 里的 Review guidelines:
  - P3 (typo / 风格 / 个人偏好) → 不要 flag
  - 这种 PR 没触发任何 P0/P1/P2 → 直接 `+1` 通过
- 这正是想要的行为: **没问题就 +1, 不噪声** ✓

**怎么逼 Codex 写完整 review** (验证它真会"较真"):
- 故意改 `keyDemo3.cpp` 把 `climb_vx` 0.5 → 1.5(>30% 巨变,触发 P1)
- 故意删某个 `sportClient.Move()` 调用前的限幅(触发 P0)
- 这种 PR 应该会看到 Codex 贴文字 review 列出 P0/P1 项

---

## 4. 在 GitHub 上看 Codex 反应的 3 个位置

(以 PR #3 为例: https://github.com/jiangtao129/unitree_sdk_jt/pull/3)

### 位置 1: PR description 框的右下角 reaction 工具栏

打开 PR #3 → 顶部那块大文本框就是 PR description → **右下角**有一行小 emoji 工具栏:

```
[😀 添加 reaction]   👍 1   <-- 这就是 Codex 贴的 +1
```

- hover 到 `👍 1` 上 → 弹出气泡,显示 **"chatgpt-codex-connector[bot] reacted with thumbs up emoji"**
- 这就是 Codex 看完 PR 觉得没问题的最终结论

### 位置 2: Conversation timeline (PR 主页面正文流)

PR 页面默认就在 **Conversation** tab。滚动看时间线,会看到一条灰色的事件行:

```
chatgpt-codex-connector reacted with 👍 emoji      55 minutes ago
```

GitHub 把所有 reaction event 都记录在 timeline,时间精确到秒。

### 位置 3: Files changed tab → Reviewers (有正式 review 时才有)

如果 Codex 写了**文字 review**(评论级),会:
- 出现在 PR 右上角的 **Reviewers** 列表里,Codex 头像旁边带 ✓ 或 ✗ 或 💬
- 点 **Files changed** tab → 在具体代码行上有 inline comment
- 点回 **Conversation** tab → review summary 会以一整块 review card 形式插入 timeline

PR #3 因为是 `+1` 不是 review,**Reviewers 列表里没有 Codex,Files changed 里也没 inline comment**。这是预期行为(Codex 选了"轻量过审")。

### 位置 4(命令行) — 用 gh 抓任意 PR 的 Codex 反应

不开浏览器,直接命令行查:

```bash
# 看某个 PR 上所有 reactions
gh api repos/jiangtao129/unitree_sdk_jt/issues/3/reactions

# 看正式 reviews
gh api repos/jiangtao129/unitree_sdk_jt/pulls/3/reviews

# 看 issue 评论(包括 Codex 的文字评论)
gh api repos/jiangtao129/unitree_sdk_jt/issues/3/comments
```

`user.login` 显示 `chatgpt-codex-connector[bot]` 的就是 Codex 留的。

---

## 5. main 历史 (4 个 commit, 全部走 PR)

```
46ca98b chore: ignore *.bak / *.swp / *~ to prevent backup files leaking in (#3)
e747cba docs: add Codex review guidelines to AGENTS.md (#2)
0771563 chore: bootstrap agent-driven dev pipeline (#1)
724e534 baseline: jiangtao fork of unitree_sdk2 with keyDemo3 climbing & SLAM extras
```

---

## 6. 后续怎么用 (你日常的 workflow)

### 日常: 一句话 = 一条 PR

在 Cursor chat 里:

```
/ship 给 keyDemo3 的 climb_vx 加一个上限 1.0 的 std::clamp
```

我会按 `.cursor/commands/ship.md` 的 11 步全部走完:
1. 读 AGENTS.md / 检查工作区
2. 从 main 拉 `feature/<topic>` 分支
3. 实现最小变更 + 跑 verify
4. commit + push
5. 开 PR (REST API,标题/body 按模板)
6. `gh pr merge --auto --squash --delete-branch`
7. 你不用管,自动等 CI + Codex 反应
8. CI 绿 + Codex `+1` 或安静 → GitHub squash merge → 远端分支删
9. 我顺手 fast-forward 本地 main + 删本地 feature

### 偶尔: Codex 写文字 review 怎么处理

如果某个 PR 你看到 Codex 贴了文字 review (P0/P1 项),两种应对:

| 情况 | 怎么做 |
|---|---|
| Codex flag 的是真 bug | 在同一分支 push fix commit → Codex 会重新 react/review |
| Codex flag 的是误报 | 在 PR 评论里 reply,或在 AGENTS.md 微调 review guidelines |
| Codex flag P0 但你坚持要合 | (不推荐) `gh pr merge --squash --admin` 用 admin 强合,但因为 enforce_admins=true,你也得先临时关 protection |

### 上游同步 (将来 unitreerobotics/unitree_sdk2 出新版本)

```bash
git fetch upstream
git checkout main
git merge upstream/main          # 或 git rebase upstream/main
# 如果有冲突,解决后用 ship 流程开 PR 合到 origin/main
```

---

## 7. 已知限制 / 暂未做

| 项 | 现状 | 后续 follow-up |
|---|---|---|
| aarch64 交叉编译 CI | 不做 (Go2 dock 上手动 build) | 可加 ARM runner 或 qemu-user-static |
| 硬件 smoke 测试自动化 | 不做 (PR Review 必须由人在 dock 上跑) | 不规划,这本来就该人工 |
| `clang-format` / `clang-tidy` | 没接 | 等真有风格冲突时再加 |
| Codex Cloud 环境 setup script | 用户当前选了"自动" | 真要让 `@codex fix CI` 时再切手动并填 apt install 行 |
| Required reviews | 0 (模式 A) | 后期如果想严,可启 `Require approvals=1` 但 Codex 默认不算 approval,需配真人 reviewer |

---

## 8. 一句话结论

> 三个 PR 全部 MERGED,CI 全部 SUCCESS,branch protection 真挡得住,Codex 自动 review 在 1 分钟内反应——**整条流水线的每个环节都拿到了实证**。后续你只需要 `/ship <一句话>`,剩下的 git/CI/PR/review/merge/cleanup 全自动。
