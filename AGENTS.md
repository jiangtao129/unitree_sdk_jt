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

---

## 7. Review guidelines (for Codex)

> 这一节专门给 OpenAI Codex GitHub Review 看。Codex 默认只 flag P0/P1,本节
> 把"什么是本仓库的 P0/P1"写清楚,避免它对真实 SLAM/控制风险欠 review、
> 又把 typo 当成阻塞。

### 7.1 严重等级定义

| 级别 | 含义 | Codex 必须 |
|---|---|---|
| **P0** | 上机会出事故 / 损坏机器人 / 仓库不可恢复 | **明确写 BLOCK,要求 PR 修复后再合** |
| **P1** | 上机会回归功能 / 显著影响精度 / CI 一定红 | flag 出来,标"建议修复" |
| **P2** | 代码质量问题 (可读性、重复) | 可选评论,不阻塞 |
| **P3** | typo / 注释拼写 / 个人风格偏好 | **不要 flag** |

### 7.2 本仓库的 P0 清单 (任何一项命中,Codex 必须 BLOCK)

1. **闭源动态库改动**:`unitree_slam/lib/*.so` 任何修改/替换 → 拒绝
2. **IMU↔LiDAR 外参乱改**:`unitree_slam/config/pl_mapping/*.yaml` 中
   `pose_imu_lidar` 数组数值变化 (除非 PR Risk 段明说重新做了标定)
3. **控制循环未限幅写**:`SportClient::Move(vx, vy, vyaw)` 调用前没有对
   `vx` / `vyaw` 做 `std::clamp` 或 abs 上限保护 (上限不写死会让狗子失控)
4. **C++ UB**:野指针 / use-after-free / 数组越界 / 整数除零 / data race
   (尤其 `keyDemo3` 里的 `std::atomic` 配套使用错误)
5. **直推 main / `--force` / 删 main 历史**:出现在 PR diff 或 commit history
6. **Secret 入仓库**:任何看起来像 token/PAT/SSH key/密码的字符串

### 7.3 本仓库的 P1 清单 (flag 但不一定 BLOCK)

1. **爬楼控制增益巨变**:`keyDemo3.cpp` 的 `climb_vx` / `K_y` /
   `max_yaw_offset` / `K_psi` / `vyaw_max` / `align_limit` / `align_tol` /
   `align_timeout` 任何 ≥30% 的调整 → 提示"需要硬件回归"
2. **SLAM 关键参数**:`cube_len` / `det_range` / `lidar_scan_max_range` /
   `map_resolution` / `scan_resolution` / `save_pcd_res` 任何调整 → 提示
   "影响建图/重定位行为,需要重建图验证"
3. **DDS topic 名变化**:`rt/sportmodestate` / `rt/utlidar/...` /
   `rt/unitree/slam_lidar/...` 等订阅/发布名改动 → 必须确认 dock 端
   也同步 (Codex 提示即可)
4. **任务列表持久化逻辑**:`dirty` flag / `loadFloorListFromDisk` /
   `saveTaskListFun` / case 's'/S'/d'/f' 的状态机改动 → 提醒可能丢点
5. **verify.sh 跳过/绕过**:任何修改让 `bash scripts/verify.sh` 不再
   覆盖原有检查 → BLOCK 候选
6. **CMakeLists 的 link 顺序/缺库**:可能编过但运行时 dlopen 报错的
   依赖问题
7. **Python view_map.py 的 stub 顺序**:`_install_open3d_ml_stubs()`
   必须在 `import open3d` **之前**调用,顺序错会重新触发 ABI 错

### 7.4 P2/P3 — 不要 flag 的事

- 中文注释 typo / 标点
- markdown 文档拼写 / 排版
- C++ 代码风格 (花括号位置、空格、命名风格);本仓库**没有**强制 clang-format
- `.plan.md` 计划文档里的过期内容 (它们就是历史记录,不需要更新)
- `unitree_sdk2` 上游 (即 `untreerobotics/unitree_sdk2` 同步过来的代码)
  本身的代码风格;只 review jiangtao 在 fork 上加的部分

### 7.5 Review 输出格式

Codex 在 PR 上贴 review 时,**优先按下面格式分组**,方便人快速扫:

```
## P0 (Blocking)
- <文件:行号> 一句话, 说"为什么会出事"

## P1 (Recommend fix)
- <文件:行号> 一句话, 给修复方向

## P2 (Optional)
- <文件:行号> 一句话, 可选优化

## 通过 / 风险评估
- 这次改动是否需要硬件回归 (是 / 否 / 不确定)
- 是否修改了关键 SLAM/控制参数 (是 / 否)
```

### 7.6 触发完整 cloud task (而不仅是 review)

如果 Codex review 发现需要它**动手修**而不是只评论,人类可以在 PR 评论
里写:
- `@codex fix the CI failures` — 让 Codex 开新分支修 CI
- `@codex add unit test for <function>` — 让 Codex 补测试
- `@codex review for security regressions` — 重 review 一次,焦点是安全

普通 `@codex` (后面什么都不加 review) 等同于触发 cloud task,会消耗
更多额度,**评审请明确写 `@codex review`**。
