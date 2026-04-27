# Cursor Agent 流水线配置 — 给新同事的实施指南

> **文档定位**: 把"提需求 → Cursor Agent 改代码 → 跑 verify → 推 PR → CI 跑 verify → Codex 自动 review → 自动 squash merge"这条流水线**完整复现**到你的电脑 + 你的 GitHub 仓库上
> **使用方式**: 把这份 md **整段** 粘贴到你 Cursor IDE 的 chat 窗口里,然后说 "**严格按这份指南执行**"
> **作者**: 这套流水线的原作者(jiangtao129),已在自己的 unitree_sdk_jt 仓库跑通 9 个 PR、踩过 12 个坑,本文把所有踩过的坑都标出来了,你不用再踩
> **预计耗时**: 同事(你)花 30-60 分钟做 5 件人工事 + Cursor agent 自动化跑 60-90 分钟

---

## 0. 你拿到这份文档后第一件事(必读)

### 0.1 整条链路上有 **3 类卡点**,Cursor agent 没法替你做
1. **花钱 / 注册**: GitHub 账号、ChatGPT 订阅(Plus 及以上)
2. **网页点选**: GitHub 仓库的 Branch protection、Auto-merge 开关、Codex 自动 review 开关
3. **本地用浏览器交互**: gh CLI 登录、SSH key 上传 GitHub

这 3 类事 agent 会**清晰列出来给你**,但只有**你自己点**才能完成。

### 0.2 整条链路上 agent **可以代你做** 的事
- 写所有仓库内文件(`scripts/verify.sh` / `AGENTS.md` / `.cursor/` / `.github/`)
- 配 `~/.ssh/config`(走 443 绕过国内屏蔽)
- 装 `gh` CLI 命令行工具
- 跑 `git remote` / `git branch` / `git push` 等所有 git 操作
- 跑 verify、监控 CI、合并 PR、清理分支
- 写测试报告留档

### 0.3 三个不同操作主体的角色

| 谁 | 做什么 | 触发方式 |
|---|---|---|
| **你(同事)** | 答 7 个决策问题、做 5 件人工配置、看 PR、发起需求 | 主动 |
| **Cursor 本地 Agent** | Orchestrator + Coding + Debug + Git/PR | 你在 chat 输入 |
| **Codex GitHub App** | PR 自动 review(comment-only) | PR opened 时自动触发 |

**绝对不允许** agent 做的事:
- 直接 `git push --force` 到 main
- 替你点 GitHub 网页上的开关(它没权限,而且违规)
- 跳过 `verify.sh` 让 CI 假绿
- 把任何看起来像 token / SSH key / 密码的字符串 commit 进仓库

---

## 1. 同事(你)必须先确认的 5 件硬性前置条件

把这 5 件确认完才有可能跑通。**任何一项 No → 流水线不可用**。

| # | 必须 | 没有怎么办 |
|---|---|---|
| 1 | **GitHub 个人账号** | 去 https://github.com/signup 注册;**记住 username**,后面所有命令都要替换它 |
| 2 | **目标仓库**(GitHub 上你能 push 的某个 repo) | 在 https://github.com/new 新建一个空仓库,或者 fork 一个已有项目;**记住仓库 owner/name** |
| 3 | **ChatGPT 订阅 Plus / Pro / Business / Enterprise / Edu**(Codex 自动 review 必须有,API key 不行) | https://chatgpt.com/pricing 升级。**没订阅就只能用 Cursor 本地 + GitHub Actions,没有 Codex review** — 也能用,只是少一个第二视角 |
| 4 | **Cursor IDE 已装** | https://cursor.com 下载 |
| 5 | **本地代码已拉到本地某目录** | `git clone git@github.com:USERNAME/REPO.git ~/projects/REPO` 之类 |

**没确认完不要往下走**。如果第 3 件没有(没 ChatGPT 订阅),那 Codex review 那一节跳过即可,其他流程照走。

---

## 2. 在你电脑上要装的工具(agent 会帮你装,但需要 sudo)

| 工具 | 必须 | 装法 | agent 能不能代劳 |
|---|---|---|---|
| `git` | ✅ | `sudo apt install git`(Linux)/ `brew install git`(mac) | 需要 sudo,你输密码 |
| `gh` CLI | ✅ | 见 § 5.4(可以无 sudo 装到 `~/.local/bin`) | **能无 sudo,agent 代你跑** |
| Cursor IDE | ✅ | 已装 | — |
| 项目对应 toolchain | ✅ | Python 项目 → `uv` 或 `pip3` ; C++ → `cmake g++` ; Node → `npm` | sudo 装,你输密码 |
| `git-lfs`(如仓库用 LFS) | 视情况 | `sudo apt install git-lfs && git lfs install` | sudo 装 |

---

## 3. Cursor Agent 启动前必问你的 7 个决策点

打开 Cursor,在 chat 里粘贴这份文档后,**agent 会先问下面 7 个问题**。你在这一节先想好答案,到时候直接回。

### Q1: 网络环境
- A. 国内,GitHub `:22` SSH 会被屏蔽 → **必须配 SSH-over-443**(agent 会改 `~/.ssh/config`)
- B. 国外 / 有稳定 VPN → 直连 22 可用
- C. 我用 HTTPS + token 不用 SSH(可以,但要装 git-credential-helper)

> **国内大多数家庭/办公网络选 A**;海外或公司专线 VPN 选 B。

### Q2: SSH key
- A. **新生成一对 ed25519 key 给 GitHub 用**(推荐,跟你公司 / GitLab 的 key 隔离)
- B. 用现有 `~/.ssh/id_rsa` / `~/.ssh/id_ed25519`(简单,但所有平台共用一把 key)

### Q3: 仓库 git remote 策略
- A. 这是个 **从零开始的新仓库**(GitHub 上空的)→ 直接 push baseline
- B. 这是个 **已有大量代码的仓库**(从公司 GitLab / 其它源迁过来)→ 先把所有未提交改动收成一个 baseline commit,再 push 到 GitHub
- C. 这是个 **fork 自某上游开源项目** → 保留 `upstream` remote 指向上游,新增 `origin`(或保留 origin)指向你 fork

### Q4: 项目技术栈(决定 verify.sh 内容)
- A. C++ + CMake → `cmake -B build && cmake --build build -j$(nproc)`
- B. Python + uv → `uv sync --all-extras && uv run pytest -q`
- C. Python + pip + venv → `pip install -e . && pytest -q`
- D. Node / TypeScript → `npm ci && npm test && npm run build`
- E. Go → `go test ./...`
- F. **混合栈**(比如 C++ + 一些 Python 工具)→ 让 agent 多步组合
- G. 我自己写 verify.sh 内容,告诉 agent 跑啥

### Q5: branch protection 严格度
- A. **严**: PR + CI 绿 + 1 个 approving review(Codex 默认是 comment 不算 approval,会卡)
- B. **中**: PR + CI 绿,无需 review(**推荐第一次跑用这个**)
- C. **松**: 只要求 PR,CI 失败也能强合(不推荐)

### Q6: Codex 自动 review 是否启用
- A. **启用**(需要 ChatGPT Plus 及以上;如果 § 1 第 3 件你已有,选这个)
- B. **不启用**(没 ChatGPT 订阅 / 暂时不要 Codex,后面随时可加)

### Q7: 第一次 demo PR 想测什么改动
- A. **agent 自动找一个最小漏洞修**(推荐;agent 会扫代码找 typo / `exit(-1)` / 缺 clamp 之类的简单漏洞,合一个真实修复 PR 验证流水线)
- B. **我自己提一个具体需求**(比如 "给 README 加一行说明")
- C. **跑一个空改动 demo**(只改一个 markdown 文件,纯粹验证管道)

---

## 4. 14 阶段执行计划(agent 要按顺序跑)

> 这一节是给 agent 看的"剧本"。每完成一个阶段 agent 必须**汇报一次**,然后等你确认/抛新决策才继续下一个。**不允许** agent 一口气跑完 14 个不停顿。

### 阶段 0: 仓库探索(必做,不能跳)

**目的**: agent 必须先知道你仓库当前长什么样,才能给出靠谱的方案。**不要让 agent 凭空设计**。

agent 要跑:
```bash
cd <你的仓库根目录>
git remote -v
git status --short
git branch --show-current
git log --oneline -5
ls -la .github/ .cursor/ scripts/ AGENTS.md 2>/dev/null
cat .gitignore | head -20
```

汇报清单:
- 当前 remotes 是什么
- 是否在 main 上 / 有多少未提交改动
- 是否已有 `.github/workflows/` / `AGENTS.md` / `scripts/` / `.cursor/`(如果有,**要 append 不能覆盖**)
- 技术栈识别(看 `pyproject.toml` / `package.json` / `CMakeLists.txt` / `go.mod`)

### 阶段 1: SSH-over-443 配置(国内必须)

按 Q1 答案。如果选 A,agent 会:
1. 在 `~/.ssh/config` **追加**(不覆盖已有内容):
   ```
   Host github.com
       HostName ssh.github.com
       Port 443
       User git
       ForwardAgent yes
   ```
2. `chmod 600 ~/.ssh/config`
3. `ssh-keyscan -p 443 -t rsa,ecdsa,ed25519 ssh.github.com >> ~/.ssh/known_hosts`(把 GitHub 在 443 端口的 host key 加进信任列表)
4. `ssh -T git@github.com` 验证 → 期望返回 `Hi USERNAME!`

### 阶段 2: SSH key 准备 + 上传 GitHub

按 Q2 答案。如果选 A(新生成):
```bash
ssh-keygen -t ed25519 -C "you@example.com" -f ~/.ssh/id_ed25519_github -N ""
```
然后 agent 把公钥内容打印给你,**你** 去 https://github.com/settings/keys 点 **New SSH key**,粘贴公钥,Save。

阶段 1 + 阶段 2 完成后,再跑一次 `ssh -T git@github.com`,**必须看到** `Hi <你的 github 用户名>!`。如果看到的是别人的用户名,说明系统选了别的 key,需要在 `~/.ssh/config` 里指定 `IdentityFile ~/.ssh/id_ed25519_github`。

### 阶段 3: gh CLI 安装(无 sudo 版)

**为什么要 gh**: 后续创建 PR、开 auto-merge、查 CI 状态都靠它,不用浏览器。

agent 跑:
```bash
GH_VERSION=$(curl -s https://api.github.com/repos/cli/cli/releases/latest \
  | python3 -c "import json,sys; print(json.load(sys.stdin)['tag_name'].lstrip('v'))")
curl -L -o /tmp/gh.tar.gz \
  "https://github.com/cli/cli/releases/download/v${GH_VERSION}/gh_${GH_VERSION}_linux_amd64.tar.gz"
tar -xzf /tmp/gh.tar.gz -C /tmp
mkdir -p ~/.local/bin
cp /tmp/gh_${GH_VERSION}_linux_amd64/bin/gh ~/.local/bin/gh
chmod +x ~/.local/bin/gh
~/.local/bin/gh --version
```

确认 `~/.local/bin` 在 `$PATH` 里(`echo $PATH | tr ':' '\n' | grep local/bin`),不在的话改 `~/.bashrc`。

### 阶段 4: gh CLI 登录(浏览器交互)

**这一步必须你亲自操作**,agent 没法替你点浏览器。

在另一个终端跑(不要让 agent 跑,因为它没法弹浏览器):
```bash
gh auth login --hostname github.com --git-protocol ssh --web
```

它会:
1. 打印一个 8 位 one-time code
2. 自动开浏览器到 https://github.com/login/device
3. 你粘贴 code → 选 jiangtao129 等账号(**确认是你想登录的账号!**)→ Authorize
4. 终端显示 `✓ Logged in as <你的账号>` → 完成

完成后告诉 agent。

### 阶段 5: git remote 配置

按 Q3 答案。常见命令(让 agent 按你的具体情况跑):
```bash
# 新仓库
git remote add origin git@github.com:USERNAME/REPO.git

# 从 GitLab 迁移过来 (推荐: 保留公司 GitLab 当 backup)
git remote rename origin gitlab          # 公司 GitLab 改名
git remote add origin git@github.com:USERNAME/REPO.git

# Fork 项目
# origin → 你的 fork; upstream → 上游
git remote add upstream https://github.com/UPSTREAM_OWNER/REPO.git
```

### 阶段 6: 清理 .gitignore + 准备 baseline

agent 检查仓库里**不该入库的东西**:
- `__pycache__/`、`*.pyc`、`.venv/`、`.env`、`*.log`
- `build/`、`*.o`、`*.so`(看技术栈)
- `node_modules/`(Node 项目)
- `*.bak`、`*.swp`、`*~`(编辑器临时文件)
- 单个 > 50 MB 的文件(GitHub 限制 100 MB,LFS 才能放大文件)

agent 帮你扩 `.gitignore`,然后 `git rm --cached -r` 把已被 staged 的垃圾移出。**绝对不要** `git add -A` 然后 push,会把垃圾入库。

### 阶段 7: 写 scripts/verify.sh

按 Q4 答案。**verify.sh 是流水线唯一真相**,本地和 CI 都跑同一个。模板:

```bash
#!/usr/bin/env bash
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

echo "================================================================"
echo " verify.sh @ $REPO_ROOT"
echo " host: $(uname -srm)"
echo "================================================================"

echo ">>> [1/N] <step name>"
# 具体命令: cmake / pytest / npm test / go test 之类
echo "<<< OK"
```

`chmod +x scripts/verify.sh` 然后 `bash scripts/verify.sh` 必须本地跑通。**没跑通不要 push**。

### 阶段 8: 写 AGENTS.md(可能要 append 不能覆盖)

如果仓库已经有 `AGENTS.md`(很多上游开源项目自带),**用 StrReplace 在末尾追加**,不要覆盖。

`AGENTS.md` 必须包含的章节(参考 jiangtao129/unitree_sdk_jt 的 AGENTS.md):
1. **仓库目标 / Definition of Done**
2. **唯一验证命令** = `bash scripts/verify.sh`
3. **强制规则**(不直推 main / 不 force push / 不跳 verify / 不 commit secrets)
4. **PR 规则**(标题格式、Summary / Test plan / Risk 三段必填)
5. **角色分工**(Cursor agent / Codex App / 你)
6. **Codex review guidelines**(P0/P1/P2/P3 分级,告诉 Codex 什么是真风险什么是 typo)

**P0 / P1 清单 必须根据你的项目特点定制**,通用模板:
- **P0**: secrets 入库 / `--force` push / 控制循环未限幅 / use-after-free / SQL 注入 / 命令注入
- **P1**: 关键参数 ≥30% 调整 / API 不向后兼容改动 / 测试被注释掉
- **P2**: 可选优化(可读性、重复)
- **P3**: typo / 风格 — **不要 flag**

参考: jiangtao129/unitree_sdk_jt 的 `AGENTS.md` § 7 章节(可让 agent fetch 这个 URL 作为模板:`https://raw.githubusercontent.com/jiangtao129/unitree_sdk_jt/main/AGENTS.md`)。

### 阶段 9: 写 .cursor/rules + .cursor/commands/ship.md

`.cursor/rules/00-workflow.mdc`(alwaysApply 规则,锁死 agent 行为)
`.cursor/commands/ship.md`(`/ship <一句话>` 触发完整流程)

参考模板:`https://raw.githubusercontent.com/jiangtao129/unitree_sdk_jt/main/.cursor/commands/ship.md`

### 阶段 10: 写 .github/workflows/ci.yml + PR 模板

CI workflow 必须以 `bash scripts/verify.sh` 为终点,**不要**写一套和本地不一样的逻辑。最小模板:

```yaml
name: ci
on:
  pull_request:
  push:
    branches: [main]
jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install deps
        run: <按技术栈>
      - name: Verify
        run: bash ./scripts/verify.sh
```

记住 job name(这里是 `build`),阶段 12 配 branch protection 时要把它加进 required checks。

`.github/PULL_REQUEST_TEMPLATE.md` 包含 4 段:Summary / Test plan / Risk / Related。

### 阶段 11: bootstrap PR(把上面所有文件一次性合到 main)

agent 创建分支 `chore/agent-pipeline-bootstrap`,commit 所有阶段 6-10 产物,push,开 PR,squash merge。

⚠️ **这一发 PR 因为 main 分支保护还没设,会立即合**。这是预期。**之后**才让你去网页设保护,从下一发 PR 开始保护才生效。

### 阶段 12: GitHub 网页一次性配置(★ 你必须亲自做)

agent 输出待办清单,**你按下一节(§ 5)操作**。做完告诉 agent。

### 阶段 13: ChatGPT Codex 后台一次性配置(★ 你必须亲自做)

如果 Q6 选 A,**你按下下一节(§ 6)操作**。做完告诉 agent。

### 阶段 14: demo PR 验证 + 写测试报告

按 Q7 答案。agent 走完整 ship 流程做一次 demo PR,这次因为 branch protection 已生效,会**真等 CI 绿才能合**,**Codex 会在 1 分钟内反应**。合完写一份 `doc/pipeline_test_report_<日期>.md` 留档。

---

## 5. § 5 GitHub 网页一次性配置清单(你必须亲自点)

每一项都给了**直达 URL**和**精确操作**。把 `USERNAME/REPO` 替换成你自己的。

### 5.1 启用 Auto-merge + Auto-delete

🔗 https://github.com/USERNAME/REPO/settings

滚动到 **Pull Requests** 区域:
- ✅ 勾 **Allow auto-merge**
- ✅ 勾 **Automatically delete head branches**(PR 合并后远端 head 分支自动删,保持仓库整洁)

### 5.2 设置 main 分支保护(★ 最关键的一步)

🔗 https://github.com/USERNAME/REPO/settings/branches

点 **Add classic branch protection rule**(注意是 classic,不是新版 Rulesets,classic 更稳):

- **Branch name pattern**: 输入 **`main`**(纯 4 个字母,不要写"main 分支保护"这种描述)
  - 输完下面应该显示 `Applies to 1 branch` → ✓
  - 如果显示 `Applies to 0 branches` → pattern 写错了

- ✅ **Require a pull request before merging**(展开)
  - 按 Q5 答案勾 / 不勾 **Require approvals**(模式 B 选 0,模式 A 选 1)

- ✅ **Require status checks to pass before merging**(展开)
  - ✅ **Require branches to be up to date before merging**
  - **搜索框搜 `build`**(或者你 CI workflow 里 job 真名)→ 点选 → 让它出现在 **"Status checks that are required"** 列表
  - ⚠️ **这一步最容易漏**: 光勾 checkbox 没用,**必须搜索框里把具体 check 名加进去**,否则 contexts 是空集 → "所有 required checks 通过" 在空集时自动为真 → auto-merge 立即合并不等 CI

- ✅ **Do not allow bypassing the above settings**(让 admin 也走流程,关键防止"自己绕过自己")

- 不要勾 **Allow force pushes**
- 不要勾 **Allow deletions**

最后 **Save changes**。

### 5.3 验证 branch protection 真生效(命令行,你也可以让 agent 跑)

```bash
gh api repos/USERNAME/REPO/branches/main/protection | python3 -c "
import json,sys
d=json.load(sys.stdin)
rsc = d.get('required_status_checks',{})
print('contexts:', rsc.get('contexts'))
print('enforce_admins:', d.get('enforce_admins',{}).get('enabled'))
"
```

期望输出:
```
contexts: ['build']        # 必须非空!
enforce_admins: True
```

如果 contexts 是 `[]` 空集 → 回 § 5.2 重新做(你漏了搜索框那一步)。

### 5.4 (如适用)启用 GitHub Actions

🔗 https://github.com/USERNAME/REPO/settings/actions

确认 **Actions permissions** 是 `Allow all actions and reusable workflows`,否则 CI 跑不起来。

---

## 6. § 6 ChatGPT Codex 后台一次性配置(你必须亲自点)

(如果 Q6 选 B "不启用 Codex",**整节跳过**)

### 6.1 在你的 GitHub 账号上装 ChatGPT Codex Connector App

🔗 https://chatgpt.com/codex

如果是第一次进:
- 登录你的 ChatGPT 账号(Plus 及以上)
- 左下角头像 → **Settings** → **Connectors** → 找 **GitHub** → **Connect**
- 弹出 GitHub 授权页 → 选你的账号 → 选 **Repository access**:
  - **All repositories**(简单,所有仓库都给 Codex 看)
  - 或 **Only select repositories** → 勾 `USERNAME/REPO`
- 点 **Install & Authorize**

### 6.2 验证 GitHub App 装好了

🔗 https://github.com/settings/installations

应该能看到一行 **ChatGPT Codex Connector**(不是别的名字),点 **Configure**:
- 权限应该有 ✓ Read access to checks, commit statuses, and metadata
- ✓ Read and write access to actions, code, issues, pull requests, and workflows
- Repository access 配对你想用的范围

### 6.3 给目标仓库开启自动 review

🔗 https://chatgpt.com/codex/settings/code-review

页面顶部: **个人自动代码审查首选项** 打开
个人审查触发器偏好: **创建 PR 时**

下面的代码仓库列表里找到 `USERNAME/REPO` 这一行:
- **自动代码审查** 列改成 **审查所有拉取请求**
- **审查触发器** 列保持 **创建 PR 时**

(可选)创建一个 Codex 环境,让 `@codex fix CI` 这种 cloud task 能跑。
🔗 https://chatgpt.com/codex 左侧 Environments → New Environment
- 选你的仓库
- 设置脚本切到"手动",填一段适配你技术栈的安装命令(Python: `pip3 install uv && uv sync` ; C++: `apt-get install -y cmake g++ build-essential`)

### 6.4 验证 Codex 能 react 你的 PR

阶段 14 的 demo PR opened 之后,1 分钟内 Codex 应该会:
- 给 PR 留个 reaction(`+1` / `eyes` / 写正式 review 三种之一)
- 反应人是 `chatgpt-codex-connector[bot]`

如果 5 分钟没动静 → 见 § 8 故障 #5。

---

## 7. 完成验收(Definition of Done)

下面**全部** ✅ 才算配通:

- [ ] `ssh -T git@github.com` 返回 `Hi <你的账号>!`
- [ ] `gh auth status` 显示 logged in
- [ ] `git remote -v` 显示正确的 origin
- [ ] `bash scripts/verify.sh` 本地通过
- [ ] `.github/workflows/<ci>.yml` 在末尾调用 `bash scripts/verify.sh`
- [ ] `AGENTS.md` 含 P0/P1/P2/P3 分级 review guidelines
- [ ] `.cursor/rules/00-workflow.mdc` 和 `.cursor/commands/ship.md` 已就位
- [ ] **bootstrap PR 已合并**到 main
- [ ] `gh api repos/USERNAME/REPO/branches/main/protection` 返回 `contexts: ['build']` 且 `enforce_admins: True`
- [ ] **demo PR 已合并**(端到端)
- [ ] demo PR 上能看到 `chatgpt-codex-connector[bot]` 的 reaction(如启用 Codex)
- [ ] `doc/pipeline_test_report_<日期>.md` 已写完留档

---

## 8. 已知坑速查表(★★★ 我们实战踩过的 12 个,你优先看这一节)

### 坑 #1: SSH 连 github.com:22 在国内被屏蔽
- **症状**: `ssh -T git@github.com` 返回 `kex_exchange_identification: Connection closed by remote host`
- **修法**: 见阶段 1,改走 SSH-over-443
- **验证**: 修完 `ssh -T git@github.com` 应该返回 `Hi USERNAME!`(exit code 1 是正常的,GitHub 不允许 shell 但确认了身份)

### 坑 #2: gh pr create 因 GitHub GraphQL 索引延迟报 "No commits between..."
- **症状**: 刚 push 完立即 `gh pr create` 报 `Head sha can't be blank, No commits between main and ...`
- **原因**: GitHub 那边 push 数据还在内部 indexing,GraphQL 端没看到
- **修法**: 改用 REST API:
  ```bash
  gh api repos/USERNAME/REPO/pulls \
    -X POST \
    -f title="<title>" \
    -f head="<branch>" \
    -f base="main" \
    -F body=@/tmp/pr-body.md
  ```
  REST 比 GraphQL 反应快得多,几乎不会撞这个坑

### 坑 #3: branch protection 勾了 checkbox 但 contexts 是空集
- **症状**: `gh pr merge --auto` 触发后 PR **立即合**(没等 CI),CI 还在跑
- **原因**: § 5.2 那一步你漏了"搜索框搜 build → 选中"那个动作
- **修法**: 见 § 5.2 第二条,**必须** 把具体 check 加到 "Status checks that are required" 列表
- **诊断**: `gh api repos/USERNAME/REPO/branches/main/protection` 看 `required_status_checks.contexts`

### 坑 #4: enforce_admins 默认 false,你 owner 能绕过保护
- **症状**: 已经设了 require PR,但你这个 owner 直接 `git push origin main` 还是能推上去
- **修法**: § 5.2 最底下勾 **Do not allow bypassing the above settings**

### 坑 #5: PR 太快被合,Codex 来不及 review
- **症状**: PR 几十秒就合,reactions=0, reviews=0
- **原因**: Codex 反应需要 30-90 秒,PR 已 closed 后它跳过
- **修法**: 让 branch protection 真生效(必须等 CI 绿才合,CI 通常 2 分钟+),Codex 自然有时间反应

### 坑 #6: Codex `+1` vs `eyes` 语义别弄混
| 它贴 | 含义 | 后续 |
|---|---|---|
| 👀 `eyes` | 我开始 review,稍后贴评论 | **会再有评论** |
| 👍 `+1` | 我看完了,没问题,**不需要写 review** | **不会再有了**,这是终态 |
| 💬 COMMENTED review with body | 真发现 P0/P1 问题 | 看 body 里列的具体行 |
| ✗ Request changes | 找到 P0 BLOCK 项 | 必须修了再合 |

### 坑 #7: 仓库用 LFS 时 baseline push 顺序
- **症状**: `git push origin main` 报 `LFS object missing`
- **修法**: **先 push LFS 对象再 push 普通 commit**:
  ```bash
  git lfs push origin --all
  git push -u origin main
  ```
- **配额**: GitHub 免费账号 LFS 配额 1 GB,超了要付费

### 坑 #8: verify.sh / CI 漏覆盖某些子项目
- **症状**: 你改了某个文件,verify.sh 通过了,push 上去 CI 也通过,但实际那个文件**根本没编**
- **原因**: 顶层 build 系统(CMakeLists / package.json scripts / Makefile)可能不覆盖某些子目录
- **诊断**: 故意往那个文件加一行明显错(`asdfg;`),跑 verify.sh,**应该报错**;如果通过 → 你的 verify 没覆盖到它
- **修法**: 让 verify.sh 跑一次"全量 collect-only"(`pytest --collect-only` / `cmake --build --target all_tests` / `cargo check --all-targets`),确认所有相关代码都被纳入

### 坑 #9: pre-commit 第一次跑会大量 auto-format 文件
- **症状**: baseline 后跑 verify.sh 时 pre-commit 改了几百个文件,verify fail
- **修法**: baseline commit 之前先跑一次 `pre-commit run --all-files`,把所有 format 都做了,然后把 format 后的文件作为 baseline 一起 commit。**之后**每个 PR 才不会被卡

### 坑 #10: CI 装 Python 依赖很慢
- **症状**: GitHub Actions 跑 `pip install` / `uv sync` 5 分钟还在装
- **修法**: 加 cache(`actions/cache@v4` 或 `astral-sh/setup-uv@v3` 的 `enable-cache: true`)。第一次跑 5 分钟,有 cache 后 30 秒

### 坑 #11: SSH key 走 443 后 host key verification failed
- **症状**: 改完 ssh config 第一次连 ssh.github.com:443 报 `Host key verification failed`
- **原因**: `~/.ssh/known_hosts` 里只有 `github.com`(22 端口)的 key,没有 `[ssh.github.com]:443`
- **修法**: `ssh-keyscan -p 443 -t rsa,ecdsa,ed25519 ssh.github.com >> ~/.ssh/known_hosts`

### 坑 #12: 工作区有别的 agent / 别的 cursor 窗口的并行未提交工作
- **症状**: `git status` 出现你不认得的修改
- **修法**: **绝对不要** `git add -A`。先问那是谁干的,要么 stash 要么不动。从 `origin/main` 直接拉新分支,只 add 你自己的改动。多 agent 并行 OK,但要互相尊重。

---

## 9. 故障诊断(常见症状 → 修法)

| 症状 | 怀疑 | 第一步排查 |
|---|---|---|
| `ssh -T git@github.com` 报错 | SSH-443 没配 / known_hosts 缺 | 见坑 #1 #11 |
| `gh pr create` 报错 | GraphQL race | 见坑 #2,改 REST API |
| PR 立即合不等 CI | branch protection 漏配 | 见坑 #3 #4 |
| Codex 不 react PR | App 没装 / 仓库不在白名单 / 订阅级别不够 | https://github.com/settings/installations 看;https://chatgpt.com/codex/settings/code-review 看仓库行 |
| CI 红:`bash: scripts/verify.sh: Permission denied` | 没 chmod +x | `chmod +x scripts/verify.sh && git update-index --chmod=+x scripts/verify.sh && git commit -am "fix: chmod +x"` |
| CI 红:某 apt 包缺 | workflow 里 install deps 没装够 | 修 `.github/workflows/ci.yml` |
| `gh pr merge --auto` 报 "auto-merge not enabled" | 仓库 settings 没勾 Allow auto-merge | § 5.1 |
| push 提示 LFS object missing | LFS push 顺序错 | 见坑 #7 |

---

## 10. 上线后日常用法

### 10.1 提需求 = 一句话

在 Cursor chat 里:
```
/ship 给 view_map.py 加 --no-window 选项, 跑 selftest 时不弹真窗口
```

agent 会按 `.cursor/commands/ship.md` 11 步全自动跑完:
1. 拉 feature 分支 → 2. 写代码 → 3. verify → 4. commit → 5. push → 6. 开 PR → 7. enable auto-merge → 8. 等 CI + Codex → 9. 自动 squash merge → 10. 清理本地 → 11. 收尾汇报

你只需要看着 PR 飘过去 + 偶尔在 Codex 提醒"这个改动可能需要硬件回归"时点点头。

### 10.2 怎么读 Codex 的 review

打开 PR 页面,3 个位置看 Codex 反应:
- **PR description 框右下角**: 有 emoji reaction(`+1` / `eyes`)→ hover 显示 `chatgpt-codex-connector reacted with X emoji`
- **Conversation timeline**: 有时间戳的 reaction event
- **Files changed tab**: 如果 Codex 写了文字 review,会有 inline comment + 在 Reviewers 列表出现 Codex 头像

### 10.3 上游同步(如果是 fork 项目)

```bash
git fetch upstream
git checkout main
git merge upstream/main          # 或 git rebase upstream/main
# 解决冲突后用 ship 流程开 PR
```

### 10.4 Codex 写文字 review 时怎么处理

| 情况 | 怎么做 |
|---|---|
| Codex flag 真 bug | 在同一分支 push fix commit,Codex 会重新 review |
| Codex 误报 | 在 PR 评论 reply 解释,或在 AGENTS.md 微调 review guidelines |
| Codex 没看见你的 PR(>5 min) | 见 § 8 故障 #5 / 检查 § 6.2 |

### 10.5 触发 Codex 不只 review,而是写代码

在 PR 评论里:
- `@codex review` — 主动要求 review(默认自动开了就不用)
- `@codex fix the CI failures` — 让 Codex 开新分支修 CI
- `@codex add unit test for foo()` — 让 Codex 补测试
- `@codex review for security regressions` — 重 review,焦点是安全

`@codex` 不带 `review` 关键字会触发 cloud task,**消耗更多 ChatGPT 额度**,普通审查请明确写 `@codex review`。

---

## 11. 给 agent 的硬性纪律(违反就停)

1. **不要直推 main**。一律 feature/fix branch + PR + auto-merge。
2. **不要跳过 verify.sh**。CI 失败先修代码,**禁止**改 verify.sh 让它"放水"。
3. **不要替同事点 GitHub 网页设置**。把待办清单写好让他自己点,等他确认完成。
4. **不要 git push --force** 到任何被引用的远端分支。
5. **不要把 secret 入仓库**。`.env` / token / API key 全在 .gitignore。
6. **不要修改 `~/.ssh/config` 里现有条目**。只允许追加(同事可能有公司 GitLab / 内部服务器的 host 条目)。
7. **不要批量重构无关代码**。聚焦原则,一发 PR 一件事。
8. **AGENTS.md / .gitignore 等已有文件要 append 不能覆盖**。
9. **每完成一阶段在 chat 里汇报 + 等同事确认**。不要一口气跑完 14 个阶段没人监督。
10. **遇到工作区有不属于本次任务的脏文件**(可能是同事自己在另一个窗口的并行工作)→ **不动它,问清楚**。

---

## 12. 一句话总结

把这份 md **整段贴到同事 Cursor 的 chat**,然后说:

> 严格按这份指南执行。先做 § 3 的 7 个决策点(我已经想好答案了:Q1=A,Q2=A,Q3=A,Q4=B,Q5=B,Q6=A,Q7=A — 你按你的实际情况改),再按 14 阶段顺序跑,每完成一阶段汇报一次。**不要一口气跑完不停顿,不要替我点 GitHub 网页**。

---

## 附录: 参考材料

文档作者已经在 `jiangtao129/unitree_sdk_jt` 仓库跑通这条流水线,9 个 PR 全 squash-merged。如果同事的 agent 想看具体文件长什么样,可以直接 fetch 这些 URL 作为模板:

| 文件 | URL |
|---|---|
| AGENTS.md(含 P0/P1 review guidelines) | https://raw.githubusercontent.com/jiangtao129/unitree_sdk_jt/main/AGENTS.md |
| scripts/verify.sh(C++ 版) | https://raw.githubusercontent.com/jiangtao129/unitree_sdk_jt/main/scripts/verify.sh |
| .cursor/rules/00-workflow.mdc | https://raw.githubusercontent.com/jiangtao129/unitree_sdk_jt/main/.cursor/rules/00-workflow.mdc |
| .cursor/commands/ship.md | https://raw.githubusercontent.com/jiangtao129/unitree_sdk_jt/main/.cursor/commands/ship.md |
| .github/workflows/c-cpp.yml | https://raw.githubusercontent.com/jiangtao129/unitree_sdk_jt/main/.github/workflows/c-cpp.yml |
| .github/PULL_REQUEST_TEMPLATE.md | https://raw.githubusercontent.com/jiangtao129/unitree_sdk_jt/main/.github/PULL_REQUEST_TEMPLATE.md |
| 测试报告写法参考 | https://raw.githubusercontent.com/jiangtao129/unitree_sdk_jt/main/doc/pipeline_test_report_20260424.md |

让你的 agent 用 `WebFetch` 这些 URL 作为本地模板的参考,避免凭空写。

---

**完**。**有问题随时问 jiangtao129**(原作者)。
