# dimos 项目流水线配置 — 给另一个 Cursor Agent 的执行说明书

> **写给**: 在 `~/huazhijian/gitlab/dimos` 工作的 Cursor 本地 Agent
> **作者**: 由 unitree_sdk_jt 仓库的 Cursor Agent 整理(已在 unitree_sdk_jt 上完整跑通同一流程)
> **目的**: 把 dimos 仓库配置成"提需求 → 改代码 → verify → push → PR → CI 跑 verify → Codex 自动 review → 自动合并"的稳定流水线
> **使用方式**: 用户把这份 md **整体粘贴**到你的 chat 窗口,然后说"按这份指南执行"

---

## 0. 你必须先理解的事

### 0.1 这不是从零开始
我已经在另一个仓库 `unitree_sdk_jt` 完整跑通过同一套流水线(7 个 PR 全 squash-merged,Codex review 集成生效)。所有踩过的坑、所有决策选项,都在本指南**第 5 节"已知坑速查表"** 里。**不要重新发明轮子,直接照做**。

### 0.2 dimos 跟 unitree_sdk_jt 的关键差异
| 维度 | unitree_sdk_jt(参考) | dimos(你要做的) |
|---|---|---|
| 主语言 | C++ + CMake | **Python 3.12 + uv** |
| Build / Test | `cmake -B build && cmake --build build` | **`uv sync --all-extras --no-extra dds` + `uv run pytest dimos -q`** |
| Lint | 没有 | **有** `.pre-commit-config.yaml` / `.style.yapf` / `pyproject.toml` 可能配 ruff |
| 依赖锁 | `lib/*.so` 闭源库 | `pyproject.toml` + `uv.lock` |
| Git LFS | 没有 | **有**(`data/.lfs/`),克隆时必须 `GIT_LFS_SKIP_SMUDGE=1` |
| 已有 AGENTS.md | 没有(我新建了) | **已有**(README 提到 agent 应该读它),**只能追加,不能覆盖** |
| 已有 .github/ | 有上游写好的 c-cpp.yml | 大概率有 GitHub Actions / 工作流 |
| 已有 scripts/ | 没有 | **已有** `scripts/install.sh` 等 |
| 当前 git origin | 之前是上游(已切) | **当前是公司 GitLab `git@gitlab.topsun:topsun/dimos_huazhijian.git`** |
| 目标 GitHub origin | `jiangtao129/unitree_sdk_jt` | **`jiangtao129/dimos`** |

### 0.3 已知用户上下文(不要重复问)
- 本地路径: `/home/jiangtao/huazhijian/gitlab/dimos`
- 目标 GitHub: `git@github.com:jiangtao129/dimos.git`(已建空仓库)
- 当前 git remotes:
  - `origin`  → `git@gitlab.topsun:topsun/dimos_huazhijian.git`(公司 GitLab,要保留)
  - `upstream` → `https://github.com/dimensionalOS/dimos.git`(社区上游)
- 用户 GitHub 账号: `jiangtao129`
- 用户机器: ThinkBook x86_64 / Ubuntu / 国内网络
- 用户 sudo 需要密码(非交互场景没法用 sudo)
- **以下东西在用户机器上都已就绪**(我已经在 unitree_sdk_jt 配过):
  - SSH-over-443 已配好(`~/.ssh/config` 含 `Host github.com / Port 443 / HostName ssh.github.com`)
  - `~/.ssh/known_hosts` 含 `[ssh.github.com]:443` 的 host key
  - `gh` CLI 2.91.0 已装在 `~/.local/bin/gh`,已登录 jiangtao129,git_protocol=ssh
  - **ChatGPT Codex Connector GitHub App 已装在 jiangtao129 账号,Repository access = All repositories**(所以 jiangtao129/dimos **自动包含**,你不需要让用户再装)
  - 用户已在 ChatGPT Codex 后台勾过自动 review,但**只对 unitree_sdk_jt 这一个仓库生效**;**你需要让用户去 https://chatgpt.com/codex/settings/code-review 给 dimos 也开启**

### 0.4 角色分工
- **你(Cursor Agent)**: Orchestrator + Coding + Debug + Git/PR
- **Codex GitHub App**: PR 自动 review(comment-only,不阻塞)
- **用户**: 提需求、做 GitHub 网页一次性配置(branch protection / Codex 后台勾选 / etc)
- **任何 GitHub 网页设置**: 你不能替用户点,只能输出"人工待办清单",然后等用户截图确认完成

---

## 1. 启动前必问用户的 5 个决策点

**不要自作主张,先问完再开干**。复述以下 5 个问题给用户,等回答:

### Q1: 公司 GitLab origin 怎么处理?
- **A. 保留 GitLab,新建 `github` remote 并指向 jiangtao129/dimos**(同时维护两个 remote,**推荐** — 公司代码不丢,GitHub 是个人 fork)
- B. 把 GitLab 改名为 `gitlab`,新建 `origin` 指向 GitHub(GitHub 当主,GitLab 当镜像)
- C. 完全替换为 GitHub,GitLab 不要了(**警告: 公司代码可能丢**)

### Q2: 当前 main 分支可能有大量本地未提交的改动,baseline 怎么处理?
- **A. 先把所有未提交改动 + 当前 origin/main 的 head 一起 push 到 GitHub 当 baseline,之后所有改动走 PR**(推荐,跟 unitree_sdk_jt 一样)
- B. 只 push 干净的当前 origin/main,本地未提交改动每个都走单独的 PR(更严但慢)

### Q3: verify.sh 第一版包含哪些检查?
- A. 最小集: `uv sync` + `uv run pytest dimos -q`(快速)
- **B. 中等集: 上面 + `pre-commit run --all-files`(覆盖 lint/format)**(推荐,dimos 已经有 pre-commit 配置)
- C. 完整集: 上面 + `mypy` + `ruff` + 跑 examples 里某个 demo

### Q4: 因为 dimos 用 Git LFS,本地很可能有 LFS 大文件,baseline push 时如何处理?
- **A. 先 `git lfs ls-files` 列出所有 LFS 跟踪文件,再 `git lfs push origin --all` 单独推 LFS,然后再 `git push origin main`(推荐,正确做法)**
- B. 用 `GIT_LFS_SKIP_SMUDGE=1` 跳过 LFS,正常 push(可能丢 LFS 内容)

### Q5: branch protection 第一版严格度?
- A. 严: 必须 PR + CI 绿 + 1 个 approving review(Codex 默认是 comment 不算 approval,会卡)
- **B. 中: 必须 PR + CI 绿,无需 review(推荐 — 第一次跑通用这个)**
- C. 松: 只要求 PR,CI 不绿也能强合(不推荐)

**等用户回答完 5 个问题,把答案整理成 todo,再进入第 2 节执行。**

---

## 2. 完整执行步骤(11 阶段)

### 阶段 0: 仓库探索(必须先做)

**目标**: 你手上没有 dimos 的具体现状,**必须先探索**。不要盲目套 unitree_sdk_jt 的脚本。

执行(用 Shell + Read + Grep 工具):
```bash
cd ~/huazhijian/gitlab/dimos
git status --short                                    # 看未提交改动
git branch --show-current                             # 看当前分支
git log --oneline -5                                  # 看最近历史
git remote -v                                         # 确认 remote
git ls-files --others --exclude-standard | head -30   # 看 untracked
git lfs ls-files | head -20                          # 看 LFS 跟踪文件
ls -la .github/workflows/ 2>/dev/null                 # 看现有 CI
ls -la .pre-commit-config.yaml .style.yapf .python-version pyproject.toml uv.lock 2>/dev/null
cat AGENTS.md | head -50                              # 看现有 AGENTS.md 风格
cat .gitignore | head -50
```

把看到的东西汇报给用户,**特别要汇报**:
- 当前 origin/upstream 是什么
- 是否在 main 分支上,是否有未提交改动
- LFS 跟踪了多少 MB 数据
- 现有 .github/workflows 里都有什么 job(PR 模板里 required check 的名字必须从这里出)
- 现有 AGENTS.md 主要在讲什么(**为了之后 append review guidelines 时风格统一**)

**做完探索 + 把 5 个 Q 问完,才能继续阶段 1**。

### 阶段 1: SSH-over-443 复用(可能已 OK)

```bash
ssh -T git@github.com 2>&1 | head -3
```
- 如果返回 `Hi jiangtao129!` → 已通,直接进入阶段 2
- 如果失败 → 大概率 ssh config 没复制过来,需要追加(见第 5 节坑 #1)

### 阶段 2: Remote 切换

按 Q1 的答案执行。**推荐方案 A 的命令**:
```bash
cd ~/huazhijian/gitlab/dimos
# upstream 已经存在不动
# 给 GitHub 新加一个 remote 叫 github,不动 origin (公司 GitLab)
git remote add github git@github.com:jiangtao129/dimos.git
git remote -v   # 验证 3 个 remote: origin (gitlab) / upstream (社区) / github (jiangtao129)
```

> ⚠️ 如果用户选 B/C,把 GitHub 当 origin,记得 `git remote rename origin gitlab` 后 `git remote add origin git@github.com:jiangtao129/dimos.git`。下面阶段所有 `push github` 改成 `push origin` 即可。

### 阶段 3: 清理 .gitignore + staged 区(类比 unitree 那边)

按 Q2 答案执行。**推荐 A**:
```bash
git status --ignored --short   # 完整看 ignored
# 检查 staged 区是否有大文件 / log / __pycache__ / .venv 等不该入库的
git diff --cached --name-only -z | xargs -0 du -m 2>/dev/null | awk '$1 >= 10'
```

dimos 项目典型该忽略但容易漏的:
```gitignore
__pycache__/
*.pyc
.venv/
venv/
.env
*.log
.pytest_cache/
.ruff_cache/
.mypy_cache/
*.bak
*.swp
*~
```
对比现有 `.gitignore`,**缺什么补什么**(用 Read + StrReplace,不要直接 cat > 覆盖)。

### 阶段 4: LFS 准备 + Baseline push

```bash
# 4.1 确认 git-lfs 已装
git lfs version || sudo apt install -y git-lfs   # 如果没装,提示用户去装

# 4.2 看 LFS 跟踪了哪些文件,有多大
git lfs ls-files
git lfs ls-files | awk '{print $3}' | xargs -I{} du -sh "{}" 2>/dev/null | sort -h | tail
```

**baseline push 顺序(关键)**:
```bash
# 4.3 先把改动 commit (按 Q2 选项)
git add <仅必要的文件>
git -c user.name="jiangtao129" -c user.email="jiangtao129@users.noreply.github.com" commit -m "baseline: ..."

# 4.4 推 LFS objects 到 GitHub LFS server (这一步可能很慢,看 LFS 数据量)
git lfs push github main --all

# 4.5 推普通 commit + 引用
git push -u github main
```

⚠️ **如果 4.4 这一步 GitHub LFS 配额不够**(免费账号 1GB),会报错。这种情况要让用户决定:
- 升级 GitHub LFS 配额(花钱)
- 或者用 `BFG` / `git lfs migrate export` 把 LFS 文件转成普通 git 对象(代价: 历史重写)
- 或者把 LFS 文件改用其他存储(Releases attachment / S3 / etc)

如果真踩到,**先停下来汇报用户**,不要自作主张。

### 阶段 5: scripts/verify.sh

按 Q3 答案。**推荐 B 的内容**(根据 dimos 实际现状再调):
```bash
#!/usr/bin/env bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

echo "=== verify.sh @ $REPO_ROOT ==="
echo " host: $(uname -srm)"
echo

echo ">>> [1/3] uv sync (deps + dev tools)"
uv sync --all-extras --no-extra dds
echo "<<< uv sync OK"
echo

echo ">>> [2/3] pre-commit run --all-files (lint + format)"
if [ -f .pre-commit-config.yaml ]; then
    uv run pre-commit run --all-files
    echo "<<< pre-commit OK"
else
    echo "[skip] no .pre-commit-config.yaml"
fi
echo

echo ">>> [3/3] pytest (fast tests under dimos/)"
uv run pytest dimos -q --maxfail=5
echo "<<< pytest OK"
echo

echo "================================================================"
echo " verify.sh: ALL OK"
echo "================================================================"
```

**注意**:
- 必须 `chmod +x scripts/verify.sh`
- **跑一遍**确认能过(`bash scripts/verify.sh`)。如果 pre-commit 第一次跑会大量 auto-format,要把 format 后的文件也 commit 进 baseline,不然 CI 永远红
- 如果 `uv sync` 在国内网络很慢,也要考虑加 mirror(`UV_INDEX_URL`)

### 阶段 6: AGENTS.md(append,不覆盖)

dimos **已有** `AGENTS.md`,绝对不能覆盖。先 Read 看现有内容,在末尾追加一节 `## N. Pipeline rules + Codex review guidelines`(N 取下一个数字)。内容参考 `unitree_sdk_jt` 的 AGENTS.md 第 1-7 节(我已写好,你可以参考它的 P0/P1/P2/P3 分级思路),但 **dimos 特定的 P0/P1 清单**要重新写,例子:

**dimos 的 P0**(任何一项命中 Codex 必须 BLOCK):
- 控制循环 / 实时 loop 里出现没有 timeout 的阻塞 IO
- `cmd_vel` / 力矩 / 速度 publish 前没有限幅(robot 会失控)
- `subprocess` / `os.system` 用了用户输入未转义(命令注入)
- secret(OPENAI_API_KEY / OAuth token)写进代码或 .env 入 git
- LCM/DDS/ROS topic 名变更但没同步 schema
- Python 类型 hint 跟 `In[T]` / `Out[T]` 实际泛型不一致(运行时才崩)

**dimos 的 P1**(flag 但不一定 BLOCK):
- 改 Module 类的 RPC 签名(向后不兼容)
- 改 Blueprint 的 autoconnect 默认 wiring
- 改 transport(LCM/SHM/DDS/ROS2)默认选项
- 测试覆盖率明显下降
- `pyproject.toml` 增加新依赖但没解释为啥

**P2/P3 — 不要 flag**:
- Python typo / 文档 typo
- f-string 格式风格
- import 顺序(让 isort/ruff 处理)

⚠️ **AGENTS.md 编辑必须用 StrReplace**,old_string 选你看到的现有 AGENTS.md 真实结尾,new_string 在那之后追加。**绝不可** Write 整个文件覆盖。

### 阶段 7: .cursor/ 规则与 ship 命令

跟 `unitree_sdk_jt` 完全一样的结构:
```
.cursor/
├── rules/
│   └── 00-workflow.mdc       # alwaysApply 规则
└── commands/
    └── ship.md               # /ship 11 步流程
```

但内容里把 unitree_sdk_jt 替换成 dimos,把 cmake build 替换成 `bash scripts/verify.sh`,其余照抄。**特别**: ship.md 里**禁止**写 `git push --force` / `直推 main` / `跳过 verify` 之类。

### 阶段 8: .github/workflows/ci.yml

dimos 大概率已经有现成 workflow,**先 Read 现有的**,看名字、看 jobs。

**两种情况**:
- 如果现有 workflow 已经做了 lint + test → 在末尾追加 `bash scripts/verify.sh` 步骤,统一为同一真相来源
- 如果没有,**新建** `.github/workflows/ci.yml`:

```yaml
name: ci

on:
  pull_request:
  push:
    branches: [main]

jobs:
  verify:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
        with:
          lfs: true   # 因为 dimos 用 LFS

      - name: Install uv
        uses: astral-sh/setup-uv@v3
        with:
          enable-cache: true
          version: latest

      - name: Set up Python
        run: uv python install 3.12

      - name: Run unified verify
        run: bash ./scripts/verify.sh
```

确认 job name(这里是 `verify`),后面 branch protection 要把它加到 required checks 列表。

### 阶段 9: .github/PULL_REQUEST_TEMPLATE.md

直接抄 unitree_sdk_jt 的版本,无需大改。

### 阶段 10: 用 ship 流程开第一发 PR

到这一步所有文件都准备好了。**这次开第一发 PR 时严格走 .cursor/commands/ship.md 流程**,把所有 pipeline 文件一次性合到 main:

```
分支名: chore/agent-pipeline-bootstrap
commit: chore: bootstrap agent-driven dev pipeline
PR base: main, head: chore/agent-pipeline-bootstrap
```

⚠️ **这一发 PR 因为 main 分支保护还没设,会立即合**。这是预期行为(跟 unitree_sdk_jt PR #1 一样)。**之后**才让用户去设 branch protection。

### 阶段 11: 输出"人工一次性待办清单"给用户

这一节是**最关键的产出**,必须**精准、可点击**。模板:

```markdown
## 你现在需要在 GitHub 网页上做这 5 件事

### 1. 启用 Auto-merge
https://github.com/jiangtao129/dimos/settings → Pull Requests → 勾 Allow auto-merge + Automatically delete head branches

### 2. 设置 main 分支保护
https://github.com/jiangtao129/dimos/settings/branches → Add classic branch protection rule
- Branch name pattern: `main`
- ✅ Require pull request before merging
- ❌ 不勾 Require approvals(模式 B,如果用户选了 A 就改这里)
- ✅ Require status checks to pass before merging
  - 搜索框输入 `verify` (或者你 CI workflow 里 job 的真实 name) → 选中
- ✅ Require branches to be up to date before merging
- ✅ Do not allow bypassing(让 admin 也走流程)
- Save

### 3. 在 ChatGPT Codex 后台为 dimos 开启自动 review
https://chatgpt.com/codex/settings/code-review → 找 jiangtao129/dimos 这一行 → 把"自动代码审查"改成"审查所有拉取请求"

### 4. (可选)创建 Codex 环境
https://chatgpt.com/codex → Settings → Environments → New Environment → Repository: jiangtao129/dimos
- 设置脚本切到"手动",填: `apt-get update && apt-get install -y python3-pip git git-lfs && pip3 install uv && uv sync --all-extras --no-extra dds`
- 这一步不影响自动 review 本身,但让以后 `@codex fix CI` 能在容器里跑测试

### 5. 验证
完成 1-4 之后,告诉我"配置完了",我开第二个 demo PR 验证流水线端到端通畅(branch protection 真挡得住 + Codex 自动反应)。
```

---

## 3. demo PR 验证清单

跟 unitree_sdk_jt 的做法一样,跑 3 个连续小 PR 验证流水线生效:

| PR | 内容 | 预期 |
|---|---|---|
| **demo PR #1** | 给 AGENTS.md 加 review guidelines 节(append) | 不需要 hardware,纯 docs,Codex 应该 `+1` |
| **demo PR #2** | 改 `.gitignore` 加几条防误模式(`*.bak` / `*.swp` 之类) | 同上,Codex 应该 `+1` |
| **demo PR #3** | 修一个 dimos 真实代码里的小 bug(可让用户提需求 / `/ship`) | 这次会触发 Codex 真正 review,看它能不能识别 P0/P1 |

每一发 PR 都用 `gh api repos/jiangtao129/dimos/pulls -X POST` **直接走 REST API**(见第 5 节坑 #2),开完后立即 `gh pr merge --auto --squash --delete-branch`。

---

## 4. 完成验收

跟 unitree_sdk_jt 一样,把 demo 跑完后写一份 `docs/pipeline_test_report_<日期>.md` 留档,内容必须包含:
1. 三个 PR 的 created→merged 时间矩阵
2. CI run id + conclusion
3. Codex 反应(reactions / reviews / inline comments 都要 dump)
4. 怎么在 GitHub 上看 Codex 反应的 3 个位置说明(PR description 右下角 / Conversation timeline / Reviewers 列表)
5. main 历史(`git log --oneline`)

参考 unitree_sdk_jt 那边的报告: `doc/pipeline_test_report_20260424.md`(用户可以贴给你看)。

---

## 5. 已知坑速查表(★ 必看,我都踩过)

### 坑 #1: SSH 连 github.com:22 被国内网络阻断
**症状**: `ssh -T git@github.com` 返回 `kex_exchange_identification: Connection closed by remote host`
**修法**: 给 `~/.ssh/config` 追加(unitree_sdk_jt 那边已经加过,你直接看现有 config,如果已有 `Host github.com / Port 443` 就跳过):
```
Host github.com
    HostName ssh.github.com
    Port 443
    User git
    ForwardAgent yes
```
然后 `ssh-keyscan -p 443 ssh.github.com >> ~/.ssh/known_hosts` 加 host key。

### 坑 #2: `gh pr create` 因 GraphQL 索引延迟报 "No commits between..."
**症状**: 刚 push 完立即 `gh pr create` 报 `Head sha can't be blank, No commits between main and ...`
**修法**: 改用 GitHub REST API 直接开 PR:
```bash
gh api repos/jiangtao129/dimos/pulls \
  -X POST \
  -f title="..." \
  -f head="<branch>" \
  -f base="main" \
  -F body=@/tmp/pr-body.md
```
`-F body=@<file>` 让 body 走文件而不是命令行,避免特殊字符问题。

### 坑 #3: branch protection 勾了 checkbox 但没把具体 check 加到 contexts
**症状**: PR 用 `gh pr merge --auto` 后**立即合**(没等 CI),因为 contexts 是 `[]` 空集 → "所有 required checks 通过" vacuously true
**诊断**: `gh api repos/jiangtao129/dimos/branches/main/protection` 看 `required_status_checks.contexts` 是不是空的
**修法**: 让用户在 Branch protection rule 编辑页 → "Require status checks" 下方搜索框 → **真的把 `verify`(或你 CI job name)加到已选列表**

### 坑 #4: enforce_admins 默认 false,owner 能绕过保护
**修法**: branch protection rule 最底下勾 **Do not allow bypassing the above settings**(等价 `enforce_admins.enabled: true`)

### 坑 #5: PR 太快被 merge,Codex 来不及 review
**症状**: PR 19 秒就合,`reactions=0, reviews=0, comments=0` 永远不会变
**原因**: Codex 的反应需要 30-90 秒,PR 已 closed 后它跳过
**修法**: 让 branch protection 真生效(必须等 CI 绿才能合,CI 通常 2 分钟+),Codex 就有时间反应。**或者**故意 sleep 几分钟再 merge

### 坑 #6: Codex 反应有两种语义,别弄混
- `+1` reaction(👍): "我看完了,没问题,**不需要写 review**" — 终态
- `eyes` reaction(👀): "我开始 review 了,稍后贴评论" — 中间态
- 正式 `COMMENTED` review with body: 真发现问题,看 body 里的 P0/P1 列表
- 没反应也没 review: 大概率是上面坑 #5

### 坑 #7: dimos 的 LFS 大对象 push 时
- 必须 **先 `git lfs push github --all`** 再 `git push github main`
- 不然 push 会报 LFS object missing
- GitHub 免费账号 LFS 配额 1 GB,超了要付费

### 坑 #8: `verify.sh` / CI 漏编子项目
**这个我在 unitree_sdk_jt 顺手发现的**: 顶层 build 系统可能不覆盖某些子目录的代码(比如它 cmake 漏了 unitree_slam/example/)。
**dimos 类比**: 检查 `pytest dimos` 是不是真的覆盖了所有需要测试的代码,有没有 `examples/` 之类没被纳入。**先用 `pytest --collect-only` 看实际收集了多少 test**,再决定要不要扩范围。

### 坑 #9: pre-commit 第一次跑会大量 auto-format
**症状**: 跑 `pre-commit run --all-files` 改了几百个文件,verify.sh fail
**修法**: baseline 时**先跑一次 pre-commit 把所有 format 都做了**,把 format 后的文件作为 baseline 一起 commit。之后每个 PR 才不会被卡在"format 不一致"。

### 坑 #10: CI 容器装 Python 依赖很慢 / 失败
**症状**: GitHub Actions 跑 `uv sync` 5 分钟还在装 / 报国外 PyPI 镜像超时
**修法**: 加 cache(setup-uv 自带 `enable-cache: true`),如果还慢考虑用 `astral-sh/setup-uv@v3` 而不是 pip。第一次跑可能 5 分钟,有 cache 后 30 秒。

---

## 6. 给 agent 的硬性纪律(违反就停)

1. **不要直推 main**。一律 feature/fix branch + PR + auto-merge。
2. **不要跳过 verify.sh**。CI 失败先修代码,**禁止**改 verify.sh 让它"放水"。
3. **不要替用户点 GitHub 网页设置**。把待办清单写好让用户自己点。
4. **不要 `git push --force`** 到任何被引用的远端分支。
5. **不要把 secret 入仓库**。`.env` / token / API key 全在 .gitignore。
6. **不要修改 `~/.ssh/config` 里现有条目**。只允许追加(unitree_sdk_jt 那边的 `Host github.com` 块用户已加,你不要动)。
7. **不要批量重构无关代码**。聚焦原则,一发 PR 一件事。
8. **AGENTS.md 必须 append 不能覆盖**。dimos 已有的内容是真实有效的,你的内容追加在末尾。
9. **每完成一阶段在 chat 里汇报 + 等用户确认**。不要一口气跑完 11 个阶段没人监督。

---

## 7. 完成定义(Definition of Done)

只有同时满足**所有以下条件**才算配通:

- [ ] `git remote -v` 显示 origin (gitlab) + upstream (社区) + github (jiangtao129/dimos)(或按 Q1 选项调整)
- [ ] `bash scripts/verify.sh` 本地 ALL OK
- [ ] `.github/workflows/ci.yml` 调用 `bash scripts/verify.sh` 作为终点
- [ ] AGENTS.md 末尾有"Pipeline rules + Codex review guidelines"节(append,不覆盖)
- [ ] `.cursor/rules/00-workflow.mdc` + `.cursor/commands/ship.md` + `.github/PULL_REQUEST_TEMPLATE.md` 全部就位
- [ ] **bootstrap PR 已合**(commit 在 origin/main 或 github/main 上)
- [ ] 至少跑通**1 个真实 demo PR**: 走完整流程(branch → verify → push → PR via REST → auto-merge → CI 等绿 → Codex 反应 → squash merge)
- [ ] Branch protection 已设,API 验证 `contexts: ['verify']`(或你 job 真名) 且 `enforce_admins: true`
- [ ] Codex 后台为 dimos 开启自动 review,demo PR 上能看到 `chatgpt-codex-connector[bot]` 的 reaction 或 review
- [ ] 写了 `docs/pipeline_test_report_<日期>.md` 留档

---

## 8. 一句话总结

> 把这份 md **整段贴给另一个 cursor agent**,然后说"严格按这份指南执行,先做第 0 节探索 + 问完第 1 节 5 个问题再开干"。**不要让它脑补**——dimos 的真实状态(LFS / pre-commit / 现有 CI)必须探索后再决定具体命令。

---

**附录: 在 unitree_sdk_jt 已经验证过的产出**
- `AGENTS.md`(120+ 行,含 P0/P1/P2/P3 分级)
- `scripts/verify.sh`(C++ 版,你写 Python 版)
- `.cursor/rules/00-workflow.mdc`(可直接复用,改下仓库名)
- `.cursor/commands/ship.md`(11 步流程,可直接复用)
- `.github/workflows/c-cpp.yml`(C++ 版,你写 ci.yml Python 版)
- `.github/PULL_REQUEST_TEMPLATE.md`(可直接复用)
- `doc/pipeline_test_report_20260424.md`(测试报告写法参考)

如果用户要,可以让他把上面任意文件的内容贴给你做参考,你按 dimos 上下文改写。
