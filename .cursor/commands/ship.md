# /ship — 一句话触发整条本地流水线

> 用法: 在 Cursor chat 里输入 `/ship <一句话需求>`,例如:
> `/ship 给 view_map.py 加 --no-window 选项, 跑 selftest 时不走真窗口`

把下面的步骤当作脚本严格执行。**任何一步失败都先停下汇报、再决定下一步**,
**不要**为了凑流程而假装通过。

---

## 步骤 (按顺序执行)

### 1. 准备
- 确认当前在仓库根目录 `/home/jiangtao/huazhijian/project/old_go2/unitree_sdk2/`。
- 读 `AGENTS.md` 和 `.cursor/rules/00-workflow.mdc`,把硬规则装进上下文。
- `git status` 检查当前分支:
  - 如果在 `main` 且工作区干净 → 进入步骤 2。
  - 如果在 `main` 且有未提交改动 → 先 `git stash` 或先开分支(让用户选)。
  - 如果不在 `main` → 检查远端是否已有同名分支;如果是历史分支,问用户是要继续在
    上面改还是新开。

### 2. 拉新分支
```bash
git fetch origin
git checkout -b <type>/<topic> origin/main
```
分支命名规则:
- 新功能 → `feature/<short-topic>`,如 `feature/view-map-no-window`
- 修 bug → `fix/<short-topic>`,如 `fix/keyDemo3-cte-overshoot`
- 文档 → `docs/<short-topic>`
- CI / 工具链 → `chore/<short-topic>`

### 3. 制定最小变更计划
- 把用户的一句话需求展开成 3~5 步的 todo (用 todo_write 工具)。
- 计划要落在**最小必要文件集合**上,**禁止**顺手做无关重构。
- 如果发现需求很大 (要改 5+ 文件、跨多个模块),**停下问用户是否拆成多个 PR**。

### 4. 实现
- 严格按 todo 顺序改,每改完一个 todo 就标 completed。
- 改 C++ → 编译必须过;改 Python → import 必须过;改 yaml → yaml 解析必须过。
- 中间状态可以用 `cmake --build build` 局部验证,**不能**跳过最终 verify.sh。

### 5. 本地验证 (强制)
```bash
bash scripts/verify.sh
```
- 通过 → 进入步骤 6。
- 失败 → 进入调试循环:读报错 → 改最小代码 → 再跑 verify;**禁止**绕过 verify。
- 如果失败原因明显在仓库历史问题 (不是本次引入),**停下问用户**是否要修。

### 6. 整理变更
- `git diff --stat` 给出改动文件清单。
- `git add <仅本次改动相关的文件>`(**禁止** `git add -A` 把无关脏东西也带上)。
- 给本次改动写一段 1~3 行的 commit 描述,准备进入步骤 7。

### 7. Commit
- commit message 格式:
  ```
  <type>: <一句话>

  <可选: 1~3 行细节, 说"为什么改"而不是"改了啥">
  ```
- **禁止** `--amend` 已 push 的 commit;**禁止** `--no-verify`。

### 8. Push
```bash
git push -u origin <branch>
```
- 第一次 push 必须带 `-u` 建立 tracking。
- push 失败 (网络 / 权限) → 立刻停下汇报,**不要**重试 force push。

### 9. 创建 PR
```bash
gh pr create \
  --base main \
  --head <branch> \
  --title "<type>: <一句话>" \
  --body-file /tmp/pr-body-<branch>.md
```
PR body 必须按 `.github/PULL_REQUEST_TEMPLATE.md` 填:
- **Summary**: 改了什么、为什么改 (1~3 bullet)
- **Test plan**: 跑了 `bash scripts/verify.sh`,贴关键输出 (build OK / selftest OK)
- **Risk**: 是否需要硬件回归、是否影响其他模块
- **Related**: linked issue / 相关 PR

### 10. 开 Auto-merge (如果仓库已启用)
```bash
gh pr merge --auto --squash <pr-url>
```
- 如果 `gh` 报错说仓库未开启 auto-merge → 进入步骤 11 提示人类。

### 11. 收尾汇报
输出格式 (固定):
```
## 完成情况

### 改动
- <文件 1>: <一句话>
- <文件 2>: <一句话>

### 验证
- bash scripts/verify.sh: PASS / FAIL (附关键输出)

### PR
- <gh 输出的 PR URL>

### 还需要人类做
- [ ] <如果 auto-merge 没开,提示去开>
- [ ] <如果 main 分支保护没设,提示去设>
- [ ] <如果 Codex Review 没绑,提示去绑>
- [ ] <如果需要硬件回归,提示去 ssh dock 测>
```

---

## 失败 / 中止规则

- 任何步骤失败都先**汇报当前状态**,不要静默重试或回退。
- 如果用户说 `/cancel` 或类似中止意图,**保留当前分支不删**,让用户决定善后。
- 永远**不要**为了让流程走完而:
  - 跳过 verify
  - 删除测试或断言
  - 把别人的代码顺手改了
  - push --force
  - 直接 commit 到 main
