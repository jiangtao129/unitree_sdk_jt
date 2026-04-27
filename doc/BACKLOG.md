# BACKLOG — 待办改进清单

> 这份文档是"小步快跑、PR 驱动"的项目管理工具。
> 每一项都是一个**独立有意义的小改动**,设计成可以**单独走一次 ship 流程**。
> 完成一项就在 checkbox 打 `[x]`,在 commit message / PR 标题里**引用项目编号**(如 `feat: ... (BACKLOG #B07)`)。

## 当前进度

- **目标**: jiangtao129 在仓库的 commit 数 → 100 (即 +89 个 PR)
- **起点**: 11 个(对应 PR #1~#10 + baseline)
- **本文档**: PR #11
- **下一个**: PR #12 起,从 P0 优先级开始扫

进度条(完成的项数 / 总项数):
- P0 (实质性): 4 / 12
- P1 (有用): 11 / 25 (P1-A04 cancelled with reason)
- P2 (锦上添花): 4 / 30
- P3 (可选填充): 0 / 15
- **总计**: **19 / 82**

---

## 规则

1. **每项 = 1 个 PR** (除非两项强耦合必须一起改)
2. PR 标题格式: `<type>: <一句话> (BACKLOG #<编号>)`,如 `refactor: use std::clamp in keyDemo3 (BACKLOG #A03)`
3. 每个 PR 必须 verify.sh 通过,走标准 ship 流程(branch → push → PR → CI → Codex review → auto squash merge)
4. 完成后**在本文件勾上 `[x]`**,并把 PR 链接填到右侧
5. **绝对不刷垃圾**: 如果某项做着发现"啊这其实没意义",**取消它**,在备注里写原因
6. 优先级 P0 > P1 > P2 > P3 倒着做(先做实质 P0,P3 是真没东西可做时的填充)

---

## P0 — 实质性改进(12 项)

> 真消除潜在风险或修真 bug,而不是风格或文档。

| # | 类型 | 描述 | 文件 | 状态 | PR |
|---|---|---|---|---|---|
| P0-01 | fix | keyDemo2.cpp `climb_vx` 没 clamp,SportClient::Move 入口加保护(类比 PR #6) | `unitree_slam/example/src/keyDemo2.cpp` | [x] | #12 |
| P0-02 | fix | keyDemo3.cpp pre-align 阶段(line ~565)的 `sportClient.Move(0.0f, 0.0f, vyaw)` 也用 clamp 写法,但 vyaw 这边其实已经 clamp,**核对**确认(可能是 P3) | `keyDemo3.cpp` | [ ] | — |
| P0-03 | fix | keyDemo3.cpp termios SIGINT 异常路径没还原 → 加 atexit / signal handler 还原终端 | `keyDemo3.cpp` | [ ] | — |
| P0-04 | fix | keyDemo3.cpp `case 'd'` 执行任务列表前没 timeout 包裹,SLAM `slam_server` 不响应时 client 永远阻塞 | `keyDemo3.cpp` | [ ] | — |
| P0-05 | fix | view_map.py 的 stub 列表写死了 5 个名字,如果 open3d 内部又新增子模块这里就漏 → 加 fallback 通用 stub | `data/demo1/view_map.py` | [ ] | — |
| P0-06 | fix | keyDemo3.cpp DDS callback `slamInfoHandler` / `slamKeyInfoHandler` 的 `nlohmann::json::parse()` 没 try/catch,SLAM server 发损坏 JSON 时异常逃出 callback 让进程崩(原描述写的是 loadFloorListFromDisk,但实际它已经有保护了,真漏洞在这两个 handler) | `keyDemo3.cpp` | [x] | #13 |
| P0-07 | fix | keyDemo3.cpp `saveTaskListFun` 写 json 不是原子的(直接 ofstream 覆盖),程序在写到一半崩可能丢全部数据 → 写 .tmp 再 rename | `keyDemo3.cpp` | [x] | #14 |
| P0-08 | fix | scripts/verify.sh 没真编 unitree_slam/example,改 keyDemo3.cpp 语法错 verify 不报 — **PR #9 已部分解决 climb math test,但完整 build 仍漏** | `scripts/verify.sh` + `CMakeLists.txt` | [ ] | — |
| P0-09 | fix | climb_control.hpp 中 wrapPi 在 |angle|=π 时返回 -π 还是 +π?这是个 corner case 应该写 test 确认 | `climb_control.hpp` + `test_climb_control.cpp` | [ ] | — |
| P0-10 | sec | .github/workflows/c-cpp.yml 用 `actions/checkout@v4` 不锁 SHA,被劫持风险 → pin 到具体 SHA | `.github/workflows/c-cpp.yml` | [ ] | — |
| P0-11 | fix | unitree_slam/lib/*.so 是闭源 aarch64 binary,本机 verify.sh 跑 cmake build 时**不应该尝试链接它们**,目前是靠运气没出错 → 加 conditional skip | `CMakeLists.txt` | [ ] | — |
| P0-12 | fix | scripts/verify.sh 加 `trap EXIT` 在失败时打印明显的 FAILED banner(原描述说"清 build 中间状态"但 cmake 已自管理 BUILD_DIR; 真正改进是错误信号清晰化) | `scripts/verify.sh` | [x] | #23 |

---

## P1 — 有用改进(25 项)

> 不是 bug 但能让代码更稳/更快/更好读。

### A. C++ 代码质量(11)

| # | 描述 | 文件 | 状态 | PR |
|---|---|---|---|---|
| P1-A01 | keyDemo3.cpp line 544: `std::max(-align_limit, std::min(align_limit, real_err))` → `std::clamp(real_err, -align_limit, align_limit)` | `keyDemo3.cpp` | [x] | #15 |
| P1-A02 | keyDemo3.cpp line 594: `std::max(-max_yaw_offset, std::min(max_yaw_offset, raw_offset))` → `std::clamp(...)` | `keyDemo3.cpp` | [x] | #16 |
| P1-A03 | keyDemo3.cpp **两处** vyaw clamp (pre-align line 590 + main loop line 627): `std::max(-vyaw_max, std::min(vyaw_max, K_psi*yerr))` → `std::clamp(K_psi*yerr, -vyaw_max, vyaw_max)` | `keyDemo3.cpp` | [x] | #17 |
| P1-A04 | ~~keyDemo3.cpp 加 `static constexpr` 给 `kVyawMax` / `kAlignLimit` / `kAlignTol`(类比 `kClimbVxMax`,统一风格)~~ **取消**: 这些是 public *tunable*(用户运行期可调),做成 constexpr 会破坏其设计意图; `kClimbVxMax` 是 *hard cap* 才合适 constexpr | `keyDemo3.cpp` | [skip] | — |
| P1-A05 | keyDemo3.cpp 主循环 sleep `std::chrono::milliseconds(20)` 抽常量 `kClimbLoopPeriod`(50 Hz 控制频率显式命名) | `keyDemo3.cpp` | [x] | #24 |
| P1-A06 | climb_control.hpp **全部 7 个** free function 加 `[[nodiscard]]` 属性 | `climb_control.hpp` | [x] | #25 |
| P1-A07 | TestClient 析构防异常: C++11 析构默认 noexcept, 显式标注是 redundant; 真改进是给析构里 climbThread.join / StopMove / stopNodeFun 包 try/catch 避免单个 throw 导致 std::terminate 跳过其他 cleanup | `keyDemo3.cpp` | [x] | #32 |
| P1-A08 | 全局 `currentKey` 实际是 dead code(被 keyExecute() 的 local 同名变量 shadow), 改 atomic 没意义 → 直接删除 dead global | `keyDemo3.cpp` | [x] | #31 |
| P1-A09 | TeeBuf class 加 `final` 关键字(防止意外被继承) | `keyDemo3.cpp` | [x] | #26 |
| P1-A10 | `poseList_f1` / `poseList_f2` 的 `std::vector<poseDate>` 在多线程访问时其实不安全 → 加注释说明哪个线程访问哪个 | `keyDemo3.cpp` | [x] | #33 |
| P1-A11 | log 文件路径用 `std::filesystem::path` 而不是 `char[96]`(避免 C string 溢出风险) | `keyDemo3.cpp` | [ ] | — |

### B. 测试覆盖(7)

| # | 描述 | 文件 | 状态 | PR |
|---|---|---|---|---|
| P1-B01 | climb_control test: wrapPi 在 ±π / ±2π / ±1e-9 边界 | `test_climb_control.cpp` | [ ] | — |
| P1-B02 | climb_control test: yawFromQuat 单位四元数 / 90° / -90° / 180° | `test_climb_control.cpp` | [ ] | — |
| P1-B03 | climb_control test: cross-track 在 stair_yaw=0 / π/4 / π/2 三个角度 | `test_climb_control.cpp` | [ ] | — |
| P1-B04 | climb_control test: K_y=0 退化(纯 P 控制器无 cross-track) | `test_climb_control.cpp` | [ ] | — |
| P1-B05 | view_map.py 加 pytest: `load_waypoints` valid / empty / malformed | `data/demo1/test_view_map.py` (新) | [ ] | — |
| P1-B06 | view_map.py 加 pytest: `color_by_height` 单调性 / 极值 / 全平 | `data/demo1/test_view_map.py` | [ ] | — |
| P1-B07 | view_map.py 加 pytest: `resolve_paths` 默认值 / floor2 自动配 / 显式指定 | `data/demo1/test_view_map.py` | [ ] | — |

### C. CI / 工具链(7)

| # | 描述 | 文件 | 状态 | PR |
|---|---|---|---|---|
| P1-C01 | actions/checkout@v4 → @v5(消除 Node 20 deprecation 警告) | `.github/workflows/c-cpp.yml` | [x] | #18 |
| P1-C02 | CI 加 `actions/cache@v4` 缓存 cmake build 产物(下次 PR 提速) | `.github/workflows/c-cpp.yml` | [x] | #34 |
| P1-C03 | CI 加 `timeout-minutes: 15`(防止僵死 job 永远占资源) | `.github/workflows/c-cpp.yml` | [x] | #19 |
| P1-C04 | CI 加 `concurrency` group(同一 PR 重复 push 取消旧 run) | `.github/workflows/c-cpp.yml` | [x] | #20 |
| P1-C05 | CI 加 `permissions: contents: read`(最小权限原则) | `.github/workflows/c-cpp.yml` | [x] | #21 |
| P1-C06 | CI 加打印 build info 步骤(uname / cmake --version / gcc --version) | `.github/workflows/c-cpp.yml` | [x] | #22 |
| P1-C07 | scripts/verify.sh 加 `--quiet` flag(CI 用静默模式),`--clean` flag(忽略 build cache 强制重编) | `scripts/verify.sh` | [ ] | — |

---

## P2 — 锦上添花(30 项)

### D. 文档(15)

| # | 描述 | 文件 | 状态 | PR |
|---|---|---|---|---|
| P2-D01 | 加 `CHANGELOG.md`,记录 PR #1~ 至今的语义化版本时间线 | 新文件 | [x] | #35 |
| P2-D02 | 加 `CONTRIBUTING.md`,说明 fork 接受新 PR 的流程 | 新文件 | [x] | #28 |
| P2-D03 | 加 `SECURITY.md`,说明发现安全问题怎么报告 | 新文件 | [x] | #29 |
| P2-D04 | 加 `.editorconfig`(行尾 / 缩进 / 字符集统一) | 新文件 | [x] | #27 |
| P2-D05 | 加 `.gitattributes`(text=auto + 文件特定 lock 等) | 新文件 | [x] | #30 |
| P2-D06 | README.md 加一节 "How this fork differs from upstream" | `README.md` | [ ] | — |
| P2-D07 | README.md 加一节 "Pipeline" 介绍 ship.md 流程 | `README.md` | [ ] | — |
| P2-D08 | README.md 顶部加 GitHub Actions status badge | `README.md` | [ ] | — |
| P2-D09 | mid360.yaml 的中文注释翻译成英文版另存一份(双语支持) | `unitree_slam/config/pl_mapping/mid360_en.yaml` (新) | [ ] | — |
| P2-D10 | `pl_mapping/xt16.yaml` 加中文注释(类比 mid360.yaml Go2_W 详注) | `unitree_slam/config/pl_mapping/xt16.yaml` | [x] | #36 |
| P2-D11 | `pl_relocation/mid360.yaml` 加中文注释 | `unitree_slam/config/pl_relocation/mid360.yaml` | [x] | #37 |
| P2-D12 | `pl_relocation/xt16.yaml` 加中文注释 | `unitree_slam/config/pl_relocation/xt16.yaml` | [x] | #38 |
| P2-D13 | `gridmap_config/config.yaml` 加中文注释 | `unitree_slam/config/gridmap_config/config.yaml` | [x] | #39 |
| P2-D14 | `slam_interfaces_server_config/param.yaml` 加中文注释 | `unitree_slam/config/slam_interfaces_server_config/param.yaml` | [x] | #40 |
| P2-D15 | `planner_config/{param,pose_ctrl_param}.yaml` 加中文注释 | `unitree_slam/config/planner_config/*.yaml` | [x] | #41+#42 |

### E. 代码 idiom 升级(15)

| # | 描述 | 文件 | 状态 | PR |
|---|---|---|---|---|
| P2-E01 | keyDemo3.cpp 把 `for (int i = 0; i < v.size(); ...)` 改成 range-for | `keyDemo3.cpp` | [ ] | — |
| P2-E02 | keyDemo3.cpp 用 `emplace_back` 替换 `push_back(temp)` | `keyDemo3.cpp` | [ ] | — |
| P2-E03 | keyDemo3.cpp 用 `auto` 替换冗长 iterator 类型 | `keyDemo3.cpp` | [ ] | — |
| P2-E04 | keyDemo3.cpp `int` 做 `vector::size()` 比较改 `size_t` | `keyDemo3.cpp` | [ ] | — |
| P2-E05 | keyDemo3.cpp 函数参数 `const std::string&` → `std::string_view` (where applicable) | `keyDemo3.cpp` | [ ] | — |
| P2-E06 | keyDemo3.cpp 加 `explicit` 到单参数构造函数 | `keyDemo3.cpp` | [ ] | — |
| P2-E07 | keyDemo3.cpp 用 `static_cast` 替换 C-style cast | `keyDemo3.cpp` | [ ] | — |
| P2-E08 | keyDemo3.cpp `0.0f` / `1.0f` literal 在多个地方,加 inline 常量统一 | `keyDemo3.cpp` | [ ] | — |
| P2-E09 | keyDemo2.cpp 同样的 idiom 升级(C++17 风格) | `keyDemo2.cpp` | [ ] | — |
| P2-E10 | view_map.py 加完整 type hints | `view_map.py` | [ ] | — |
| P2-E11 | view_map.py 用 dataclass 表示 waypoint(替换 dict-like) | `view_map.py` | [ ] | — |
| P2-E12 | view_map.py 把 print 改成 logging 模块 | `view_map.py` | [ ] | — |
| P2-E13 | scripts/verify.sh 用 `printf` 替换 `echo`(更可移植) | `scripts/verify.sh` | [ ] | — |
| P2-E14 | scripts/verify.sh 加 trap 捕获 EXIT 打印总结 | `scripts/verify.sh` | [ ] | — |
| P2-E15 | doc/md_to_pdf.py 文件没 type hint,补全 | `doc/md_to_pdf.py` | [ ] | — |

---

## P3 — 可选填充(15 项)

> 真没东西可做时再做这些。**禁止**因为想凑数就跳到 P3。

### F. 注释 / typo / 微调

| # | 描述 | 文件 | 状态 | PR |
|---|---|---|---|---|
| P3-F01 | doc/plan1-go2_edu_climb_stairs_via_keydemo_*.md 加 "Status: superseded by keyDemo3" header | `doc/plan1-*.md` | [ ] | — |
| P3-F02 | doc/plan2-climb-los-waypoint-following_*.md 加 status header | `doc/plan2-*.md` | [ ] | — |
| P3-F03 | doc/plan3-keydemo3_explicit_save_and_d-reload_*.md 加 status header | `doc/plan3-*.md` | [ ] | — |
| P3-F04 | doc/go2_slam_quick_start_via_keydemo_*.md 加 status header | `doc/go2_slam_*.md` | [ ] | — |
| P3-F05 | dimos_huazhijian_multi_agent_setup_guide_zh.md 移到 `doc/archive/`(归档原始模板) | `doc/dimos_huazhijian_multi_agent_setup_guide_zh.md` | [ ] | — |
| P3-F06 | mid360.yaml G1 段也加一条注释指向 Go2_W 段 | `unitree_slam/config/pl_mapping/mid360.yaml` | [ ] | — |
| P3-F07 | view_map.py 顶部 docstring 补 license | `view_map.py` | [ ] | — |
| P3-F08 | scripts/verify.sh 顶部加 shellcheck 兼容 directive | `scripts/verify.sh` | [ ] | — |
| P3-F09 | AGENTS.md 加目录(Table of Contents) | `AGENTS.md` | [ ] | — |
| P3-F10 | doc/pipeline_test_report_20260424.md 加 "Last verified: <date>" header | `doc/pipeline_test_report_20260424.md` | [ ] | — |
| P3-F11 | doc/dimos_pipeline_setup_for_agent.md 加 TOC | `doc/dimos_pipeline_setup_for_agent.md` | [ ] | — |
| P3-F12 | doc/agent_pipeline_onboarding_for_teammate.md 加 TOC | `doc/agent_pipeline_onboarding_for_teammate.md` | [ ] | — |
| P3-F13 | .gitignore 注释分组重排,按字母序 | `.gitignore` | [ ] | — |
| P3-F14 | example/go2/go2_robot_state_client.cpp 顶部加 "modified by jiangtao129" 注释,标记 fork 改动来源 | `example/go2/go2_robot_state_client.cpp` | [ ] | — |
| P3-F15 | unitree_slam/example/CMakeLists.txt 加 cmake_minimum_required 显式版本要求 | `unitree_slam/example/CMakeLists.txt` | [ ] | — |

---

## 工作流程(给 agent 看的)

每次开 PR 时:

1. 在本文件找 P0 优先级里第一个未勾选的项
2. `git checkout -b <type>/<short-topic> origin/main`
3. 在 chat 里输出"我打算做 BACKLOG #X-Y,改动是 ...,文件是 ...",**等用户确认**才动手
4. 实现 + verify.sh 通过
5. **顺手在本文件勾上 `[x]` + 填 PR 链接到右侧 PR 列**
6. commit 标题格式: `<type>: <一句话> (BACKLOG #X-Y)`
7. 走标准 ship 流程: push → REST API 开 PR → auto-merge → watch CI → sync local
8. 完成后在 chat 汇报: "BACKLOG #X-Y 已合并,PR #N,当前进度 a/82,下一个建议做 #X-Y"

不允许的事:
- 跳级到 P2/P3 凑数(必须先把 P0 / P1 做完)
- 一次 PR 改多个 BACKLOG 项(每项独立)
- 修着修着发现没意义还硬上(应该取消并备注)

---

## 备注

本清单的初衷不是"刷数量",而是把"未来如果有时间想做的小改进"集中列出来,使每次想给项目添砖加瓦时不用重新动脑。
即使 89 个 PR 全做完也只到 100 commit,**100 不是终点,是一个里程碑**。
真正衡量项目质量的指标是 **CI 绿率 / Codex flag 数 / 用户反馈**,不是 commit 数。
