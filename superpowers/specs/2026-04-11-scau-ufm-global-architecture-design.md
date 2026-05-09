# SCAU-UFM 全局架构设计规格书 v5.3.3

> 基于非结构化网格 + STCF v5 兼容数据标准 + 各向异性双孔隙率 DPM 的城市洪涝模型

**日期**: 2026-04-11 (v5.3.3 updated 2026-04-26)
**状态**: Draft v5.3.3
**来源**: 白皮书 + gemini/ 技术文档 (16篇) + model/ 技术文档 (5篇) + 深度审计 + 三方对抗审计 + v5.1 实施收口修订 + 严苛复核 V1.0 硬化 + 硬核复核 V2.0 吸收 + 架构复核与漏洞修补

---

## 0. 文档定位与版本边界

### 0.1 文档边界规则

本主 Spec 只定义系统架构、数据契约、接口契约、数值与物理不变式，以及耦合核心语义。
任何 CI 阻断、运行时升级、发布治理与人工复核后果，均由配套稳定性协议定义。
符号、单位、退役别名与 machine-facing 命名约束由符号表统一收口；当前权威入口以 `superpowers/INDEX.md` 为准。
配套文档路径分别为 `superpowers/INDEX.md`、`superpowers/specs/2026-04-14-scau-ufm-stability-reliability-protocol.md`、`superpowers/specs/2026-04-22-symbols-and-terms-reference.md` 与 `superpowers/specs/project-layout-design.md`。

复核报告、修文计划、修文状态核销与历史设计书均为非规范性历史记录；不得从其中提取与本文、稳定性协议或符号表冲突的实现规则。

### 0.2 版本边界

- 本文档当前以 `v5.3.3` 为主收口版本边界。
- 文档前部优先建立章节主轴、Phase 1 最小可实现基线与系统总拓扑。
- `## 目录骨架（v5.3.3 物理重整）` 当前用于定义 v5.3.3 的逻辑主轴与过渡映射，不表示后文旧编号正文已全部完成搬运。
- 后续深层技术正文继续保留原位，不在本任务中大规模搬运至 4~12 章下；后续任务将继续按该骨架逐步收口。

## 目录骨架（v5.3.3 物理重整）

0. 文档定位与版本边界
1. 项目目标、范围与非目标
2. Phase 1 最小可实现基线
3. 系统总架构与模块拓扑
4. 核心数据契约
5. 数值与物理不变式（含 5.A 双层阻力/干湿/安全阀，5.B AI 双阶校核）
6. 耦合核心协议
7. 引擎适配器契约
8. 故障恢复与状态管理
9. 预处理与 STCF 生产规范
10. 验证矩阵与正确性基线
11. 参数与默认值索引
12. 未来能力与保留扩展

历史展开附录（旧编号 8~16，降级为补充材料）

> 说明：当前正文仍保留部分旧编号展开；本节仅用于建立 v5.3.3 物理重整的逻辑主轴与映射关系，后续任务将继续完成正文收口。

## 1. 项目目标、范围与非目标

### 1.1 目标

构建一套从原始 LiDAR 点云到 CUDA 高并行求解器的完整城市洪涝模拟技术链路。v5 的核心升级是：

1. **三角/四边形混合非结构化网格 + STCF v5 兼容数据标准**
2. **各向异性双孔隙率 DPM 主方程骨架**
3. **双层阻力结构**：底摩擦（Manning）+ 亚网格方向性拖曳（`C_d + A_w`）
4. **SWMM 嵌入式 1D-2D 双向耦合**
5. **AI 双阶校核**：PINN 离线反演 + 贝叶斯在线微调
6. **保守算子重构 + GPU 高性能实现**

### 1.2 关键决策

| 决策项 | 选择 | 理由 |
|--------|------|------|
| 网格体系 | 三角/四边形混合非结构化网格 + STCF 亚网格修正 | 复杂边界适配 + 局部结构化效率 + 亚网格精度 |
| 主方程 | **各向异性双孔隙率 DPM** | 比 v4 的 IPM/STCF 扩展骨架更清晰、更正统 |
| 数据格式 | **STCF v5 兼容 DPM 字段映射** | 保留 STCF 生态，升级物理骨架 |
| 阻力结构 | **双层阻力**：Manning + 方向性 Drag 张量 `C_d A_w` | 区分地表材质摩擦与亚网格建筑阻塞，并保留方向性 |
| 管网模拟 | OWA SWMM 5.2.x 嵌入 | 成熟稳定、全球验证 |
| AI 集成 | PINN + 贝叶斯优化 + 语义-物理映射 | 双阶校核、可解释性优先 |
| 目标硬件 | 主要: A100/H100；兼容 RTX 3090/4090 | 数据中心 GPU 为主，消费级需混合精度路径 |

### 1.3 范围与非目标

- 本文档当前优先收口主架构、数据契约、接口边界、数值不变式与耦合语义。
- Phase 1 以前部最小可实现基线作为交付下界，用于约束实现主轴与后续验收入口。
- 本文档不在此处重复定义 CI 阻断、发布治理、人工复核与运行时升级后果。

---

## 2. Phase 1 最小可实现基线

**主权声明**：本章是 SCAU-UFM release 级 Phase 1/2/3 的 capability 与 roadmap 的唯一规范来源；目录落位见 `superpowers/specs/project-layout-design.md §6`，里程碑级实施排序见 `legacy.12`（M1–M5，沿用历史命名 "Phase 1–5"）。本章中 "Phase" 一律指 release 级阶段；其他文档中如出现冲突，以本章为准。

### 2.1 Phase 1 必须交付的最小 capability

Phase 1 是 SCAU-UFM 的首个 release 级阶段，必须交付以下 capability 的最小实现，才能进入 Phase 2。每条 capability 与 `project-layout-design.md §6.1` 的目录落位一一对应：

| Capability | 目录落位 |
|---|---|
| Core 运行时骨架 | `libs/core/` |
| MeshLib 三角/四边形混合网格最小骨架 | `libs/mesh/` |
| STCF / DPM 数据层 | `libs/stcf/` |
| CPU 参考求解器（state / dpm / riemann / source_terms / time_integration / backends/cpu） | `libs/surface2d/` |
| 最小 CouplingLib 骨架（exchange / arbitration / conservation / scheduler / mapping / deficit / snapshot / rollback / fault） | `libs/coupling/core/` |
| 排水侧最小耦合 + MockSwmmEngine | `libs/coupling/drainage/` |
| 公共 IO 最小路径 | `libs/io/` |
| SimDriver 最小驱动入口 | `apps/sim_driver/` |
| 最小配置落位（runtime / surface2d / coupling） | `configs/` |
| 最小数值验证 + GoldenSuite + 参考结果 + 实证 | `tests/numerical/`, `tests/golden/{suite_manifest,reference,evidence}/` |

D-Flow FM 河网耦合、CUDA backend 主推进路径、PreProc 完整链路与 AI 双阶校核不在 Phase 1 必交付范围内，但目录骨架必须按 `project-layout-design.md §6.2` 占位。

### 2.2 Phase release roadmap

| Release | 必交付 capability |
|---|---|
| Phase 1 | §2.1 全部 capability + GoldenSuite 最小集 + SWMM Mock 通路 + CPU 正确性路径 |
| Phase 2 | D-Flow FM 河网耦合 + CUDA backend 主推进路径 + GoldenSuite 完整性 + 双 1D 引擎共享单元仲裁证据 |
| Phase 3 | PreProc 完整链路 + AI 双阶校核 + 城市真实算例 + 性能扩展与压力场景 |

Phase N 的 release 准入以 GoldenSuite 通过证据 + 配套稳定性协议要求的证据链为准；Phase 1 内部里程碑级实施排序（M1–M5）见 `legacy.12`。

## 3. 系统总架构与模块拓扑

### 3.1 模块角色

- `SimDriver`: 总调度器
- `Surface2DCore`: 二维地表核心
- `CouplingLib`: 耦合核心协议执行层
- `SWMMAdapter` / `DFlowFMAdapter`: 引擎适配器
- `PreProc`: STCF/DPM 数据生产

### 3.2 控制权边界

- `SimDriver` 拥有主时间循环
- `Surface2DCore` 拥有 2D 数值推进
- `CouplingLib` 拥有交换裁定与账本语义
- Adapter 只负责引擎交互，不重新定义耦合语义

**双 1D 引擎运行拓扑（v5.3.4 文档固化）**：

```text
                         +----------------------+
                         |      SimDriver       |
                         |  main time loop      |
                         +----------+-----------+
                                    |
                                    v
+----------------------+    +-------+--------+    +----------------------+
|     SWMMAdapter      |<-->| CouplingLib    |<-->|   DFlowFMAdapter     |
| drainage network     |    | core           |    | river / structures  |
| manholes / inlets    |    | arbitration    |    | RTC / gates / weirs |
+----------+-----------+    | Q_limit        |    +----------+-----------+
           |                | deficit ledger |               |
           v                | FaultController|               v
+----------------------+    +-------+--------+    +----------------------+
| embedded SWMM engine |            |             | D-Flow FM / BMI      |
+----------------------+            v             +----------------------+
                           +--------+---------+
                           |  Surface2DCore   |
                           |  DPM-HLLC 2D FVM |
                           +------------------+
```

SWMM 与 D-Flow FM 是并列 1D 引擎：SWMM 负责城市排水管网、检查井、雨水口与封闭管渠；D-Flow FM 负责河网、水工建筑物、RTC、堤防与漫滩口门。二者均通过 Adapter 接入 `CouplingLib core`，共享同一套交换仲裁、`Q_limit`、`priority_weight`、`mass_deficit_account`、`FaultController` 与 snapshot/replay 语义。

### 3.3 项目目录结构与嵌入式第三方布局

- `libs/coupling/core/`: 共享流量池、账本、仲裁、故障控制、snapshot/replay 契约
- `libs/coupling/drainage/`: SWMM 适配器与接口边界
- `libs/coupling/river/`: D-Flow FM 适配器与项目自研 BMI 抽象接口边界；自研 BMI 接口位于 `libs/coupling/river/bmi_interface/`
- `extern/swmm5/`: 嵌入式 SWMM 第三方来源
- `extern/dflowfm/`: D-Flow FM 第三方 vendor 本体快照
- `extern/dflowfm-bmi/`: D-Flow FM 第三方 vendor BMI bridge 快照；不得承载项目自研耦合语义
- `cmake/third_party/`: 第三方发现、版本锁定、兼容性开关

### 3.4 第三方版本边界

- 明确 `SWMM` 版本锁定方式
- 明确 `D-Flow FM` / `BMI bridge` 版本锁定方式
- 明确升级第三方时必须补交的回归与兼容性证据

### 3.5 分层模块化总览（保留现有技术展开）

```text
+-------------------------------------------------------------+
|                       Python 工具层                           |
|        scau_preproc / scau_postproc / scau_ai                 |
+-------------------------------------------------------------+
|                       C++ 应用层                              |
|        SimDriver (时间循环 / IO 调度 / 检查点)                 |
+--------------+--------------+-------------------------------+
| MeshLib      | STCFLib      | Surface2DCore                 |
| 网格生成      | DPM 数据 IO   | DPM-HLLC 2D FVM               |
| 拓扑管理      | UGRID        | flux / source_terms / CFL     |
| 局部加密      | 属性映射      | CPU reference / CUDA backend  |
+--------------+--------------+-------------------------------+
|                 CouplingLib: 1D-2D 全耦合层                  |
| exchange / arbitration / conservation / deficit / replay     |
| SWMMAdapter                    DFlowFMAdapter                |
+-------------------------------------------------------------+
|                       基础设施层                              |
|        日志 / 配置 / 错误处理 / 域分解接口                    |
+-------------------------------------------------------------+
```

### 3.6 模块依赖关系

```text
C++ 层:
  core <- mesh <- stcf -+-> surface2d (DPM-HLLC 数值推进)
                        |       ├─ source_terms (降雨 / 下渗 / Manning / Drag)
                        |       ├─ backends/cpu (确定性参考路径)
                        |       └─ backends/cuda (性能路径)
                        +-> coupling/core (共享耦合语义)
                                ├─ drainage -> extern/swmm5
                                └─ river    -> extern/dflowfm / extern/dflowfm-bmi

  surface2d + coupling/core --> SimDriver (apps/)

Python 层:
  scau_preproc (依赖 mesh + stcf 数据格式)
       |
  scau_ai (依赖 stcf NetCDF 数据契约 + preproc 输出)
       |
  scau_postproc (依赖求解器输出格式)

跨语言桥接: pybind11 (SimDriver <-> Python)
```

---

### 3.7 项目目录结构（现有详细展开）

> **重要**：本章描述目标态（target-state）项目目录布局；当前仓库为 Python 原型，实际目录见 `docs/architecture/repository-layout.md`。读者请勿将本章目录树当作当前已落地事实。

#### 3.7.1 顶层目录树

```text
SCAU-UFM/
├── CMakeLists.txt
├── vcpkg.json
├── README.md
├── cmake/
│   ├── toolchains/
│   ├── modules/
│   └── third_party/
│       ├── swmm.cmake
│       ├── dflowfm.cmake
│       └── bmi_bridge.cmake
├── libs/
│   ├── core/
│   ├── mesh/
│   ├── stcf/
│   │   ├── include/stcf/
│   │   │   ├── schema.hpp
│   │   │   ├── io_netcdf.hpp
│   │   │   ├── dpm_consistency.hpp
│   │   │   ├── tensor.hpp
│   │   │   ├── chebyshev.hpp
│   │   │   └── soil_params.hpp
│   ├── surface2d/
│   │   ├── include/surface2d/
│   │   │   ├── state/
│   │   │   ├── geometry/
│   │   │   ├── boundary/
│   │   │   ├── dpm/
│   │   │   ├── riemann/
│   │   │   ├── flux/
│   │   │   ├── source_terms/
│   │   │   ├── wetting_drying/
│   │   │   ├── time_integration/
│   │   │   ├── cfl/
│   │   │   └── diagnostics/
│   │   └── backends/
│   │       ├── cpu/
│   │       └── cuda/
│   └── coupling/
│       ├── core/
│       │   ├── exchange/
│       │   ├── arbitration/
│       │   ├── deficit/
│       │   ├── snapshot/
│       │   ├── rollback/
│       │   └── fault/
│       ├── drainage/                    (SWMM)
│       │   ├── engine_interface/        (排水域 / SWMM 接口边界层)
│       │   ├── swmm_adapter/
│       │   └── mock/
│       └── river/                       (D-Flow FM / BMI)
│           ├── bmi_interface/           (BMI 协议抽象适配层)
│           ├── dflowfm_adapter/
│           └── river_exchange/
├── extern/
│   ├── swmm5/
│   ├── dflowfm/
│   └── dflowfm-bmi/
├── third_party/
│   ├── manifest/
│   │   ├── swmm5.version
│   │   ├── dflowfm.version
│   │   └── dflowfm-bmi.version
│   ├── patches/
│   │   ├── swmm5/
│   │   └── dflowfm/
│   ├── licenses/
│   │   ├── SWMM_LICENSE.txt
│   │   └── DFLOWFM_LICENSE.txt
│   └── compatibility/
│       ├── matrix.md
│       └── upgrade-policy.md
├── apps/
│   ├── sim_driver/
│   ├── preproc_cli/
│   └── diagnostics/
├── python/
│   ├── scau_preproc/
│   ├── scau_postproc/
│   └── scau_ai/
├── configs/
│   ├── runtime/
│   ├── surface2d/
│   ├── coupling/
│   └── third_party/
├── tests/
│   ├── unit/
│   ├── numerical/
│   ├── integration/
│   ├── golden/
│   │   ├── suite_manifest/
│   │   ├── reference/
│   │   └── evidence/
│   ├── fault_replay/
│   ├── regression/
│   ├── compatibility/
│   └── performance/
├── examples/
│   ├── minimal/
│   ├── swmm_coupling/
│   └── dflowfm_coupling/
└── docs/
    ├── architecture/
    ├── specs/
    ├── plans/
    └── protocols/
```

#### 3.7.2 目录职责约束

- `libs/surface2d/`：只负责二维地表数值推进，包括状态量、几何、边界、DPM 字段、HLLC / flux、`source_terms`、润湿干涸、时间积分、`max_cell_cfl` 诊断与 CPU/CUDA 后端；不得依赖 SWMM、D-Flow FM 或任何 1D 引擎 ABI。
- `libs/surface2d/backends/cpu/`：承载确定性参考路径，是 GoldenTest 与 replay 正确性证据的默认数值路径。
- `libs/surface2d/backends/cuda/`：承载性能路径，必须复用 `libs/surface2d/` 的状态、通量、源项、CFL 与诊断契约；不得另行定义 `Q_limit`、deficit、rollback 或 replay 语义。
- `libs/coupling/core/`：只负责共享流量池、仲裁、账本、snapshot/replay 公共契约，以及与具体 1D 引擎无关的耦合语义；不得承载 SWMM 或 D-Flow FM 专属适配实现。
- `libs/coupling/drainage/`：只负责 `ISwmmEngine`、`SWMMAdapter`、`MockSwmmEngine` 及其配套排水侧接入逻辑；不得放置河网侧 BMI 适配，也不得放置共享流量池、仲裁、账本、snapshot/replay 等公共实现。
- `libs/coupling/river/`：只负责 `IDFlowFMEngine`、`DFlowFMAdapter`、`RiverExchangePoint` 及其配套河网侧接入逻辑；此处 BMI 仅指项目内 BMI 封装与交换点映射，不包含第三方 BMI bridge 源码，后者位于 `extern/dflowfm-bmi/`；不得放置排水侧 SWMM 适配，也不得放置共享流量池、仲裁、账本、snapshot/replay 等公共实现。
- `extern/swmm5/`：只存放嵌入式 SWMM 第三方本体源码；不得混入项目自研耦合逻辑。
- `extern/dflowfm/`：只存放嵌入式 D-Flow FM 第三方本体源码；不得混入项目自研业务或耦合语义。
- `extern/dflowfm-bmi/`：只存放 D-Flow FM BMI bridge 相关第三方桥接层实现；不得承担上层交换仲裁职责。
- `third_party/manifest/`：只存放第三方组件清单与版本事实记录，用于声明当前引入的 SWMM、D-Flow FM 与 BMI bridge 依赖基线。
- `third_party/patches/`：只存放第三方组件补丁及其配套补丁说明；不得放置项目业务代码副本。
- `third_party/compatibility/`：只存放第三方组件之间及与本工程之间的兼容性矩阵、适配说明与兼容结论；不得承载源码实现。
- `cmake/third_party/`：只存放第三方组件的发现、配置、编译与链接接入逻辑；不得承载运行期耦合策略或版本治理正文。

**ABI / header firewall（v5.3.2 第二轮收口）**：
- `libs/coupling/core/` 的公共头文件只允许暴露项目自定义 DTO、抽象接口、状态码与纯值类型；不得包含 `extern/swmm5/`、`extern/dflowfm/`、`extern/dflowfm-bmi/` 的头文件。
- `libs/coupling/core/` 不得在公共 ABI 中暴露第三方句柄、结构体、枚举、回调签名、STL 容器嵌套第三方类型或任何依赖第三方二进制布局的字段。
- 第三方对象的生命周期、指针、异常与错误码只能在 `libs/coupling/drainage/` 或 `libs/coupling/river/` 内部消化，并转换为 `ExchangeRequest`、`ExchangeDecision`、`EngineReport`、`FaultState` 等共享 DTO 后进入 core。
- 跨引擎数据交换必须经共享 DTO 与账本语义完成；禁止让 SWMM 与 D-Flow FM 的原生对象彼此可见，也禁止让 `Surface2DCore` 依赖任一 1D 引擎的 ABI。

**ABI 兼容路径决策（v5.3.3）**：
- Phase 1 / 2：`libs/coupling/core` / `drainage` / `river` 必须**同进程同编译单元**（即静态链接 + 单一 stdlib），DTO 可使用 STL；该约束写入 CMake 强制检查
- Phase 3+（若引入跨动态库 / 跨进程通信）：DTO 必须重写为 POD + C ABI，并新增专门 IPC 适配层；不允许 STL 跨 .so 边界传递
- 当前 STL DTO（`std::vector` / `std::string`）属于 Phase 1/2 适用范围；不构成跨阶段技术债

#### 3.7.3 第三方嵌入与版本锁定原则

1. 嵌入式第三方必须有明确目录落位，并与项目自研实现保持物理隔离：
   - `SWMM` → `extern/swmm5/`
   - `D-Flow FM` → `extern/dflowfm/`
   - `D-Flow FM BMI bridge` → `extern/dflowfm-bmi/`

2. 每个第三方组件必须有独立版本声明文件，用于记录当前纳入基线的版本事实：
   - `third_party/manifest/swmm5.version`
   - `third_party/manifest/dflowfm.version`
   - `third_party/manifest/dflowfm-bmi.version`

3. 每个 `.version` 文件至少必须包含以下字段：
   - 版本标识（`release` / `tag` / `commit` 之一）
   - 来源
   - 获取方式
   - 校验值
   - 校验算法
   - 是否应用 patch
   - 兼容性备注

4. 任一第三方版本升级都不得只更新源码或构建脚本，必须同步更新对应 `.version` 文件；有关回归证据、兼容矩阵、发布治理与人工复核留痕要求，见配套稳定性协议相应章节。本节只保留版本边界与目录落位定义，不重复定义执行后果。

#### 3.7.4 Phase 1 最低落位要求

#### Phase 1 必须有最小实现的目录
- `libs/core/`：必须具备基础设施最小骨架，至少承载类型、配置、日志或错误处理等主线启动所需能力。
- `libs/mesh/`：必须具备三角/四边形混合非结构化网格与拓扑最小骨架。
- `libs/stcf/`：必须具备 STCF / DPM 数据契约的最小实现，至少能承载基础 schema 或 IO 边界。
- `libs/surface2d/state/`、`libs/surface2d/dpm/`、`libs/surface2d/riemann/`、`libs/surface2d/source_terms/`、`libs/surface2d/time_integration/` 与 `libs/surface2d/backends/cpu/`：必须具备二维地表 DPM-HLLC 最小求解主干，至少保留主方程离散、源项、时间推进与 CPU 参考求解入口。
- `libs/coupling/core/`：必须具备共享交换语义、仲裁或账本等公共耦合骨架。
- `libs/coupling/drainage/`：必须具备排水侧最小接入实现，至少能承载 SWMM 适配接口或 Mock 保底路径。
- `apps/sim_driver/`：必须具备最小驱动入口，用于串联配置加载、时间推进与主流程调度。
- `tests/numerical/`：必须具备二维地表数值正确性最小测试落位。
- `tests/golden/`、`tests/golden/suite_manifest/`、`tests/golden/reference/` 与 `tests/golden/evidence/`：必须具备最小黄金测试、参考结果与可复现实证落位。
- `configs/runtime/`：必须具备最小运行时配置落位。
- `configs/surface2d/`：必须具备二维地表数值参数配置落位。
- `configs/coupling/`：必须具备最小耦合配置落位。

#### Phase 1 必须显式存在、但可先最小占位的目录
- `libs/surface2d/backends/cuda/`：必须提前保留 CUDA 后端目录，即使 Phase 1 仅先放置接口、空实现或占位说明。
- `libs/coupling/river/`：必须提前保留河网侧接入目录，即使 Phase 1 仅先放置接口、空实现或占位说明。
- `extern/dflowfm/`：必须显式预留 D-Flow FM 第三方本体落位，可先为空壳目录或最小占位内容。
- `extern/dflowfm-bmi/`：必须显式预留 D-Flow FM BMI bridge 落位，可先最小占位。
- `cmake/third_party/`：必须显式存在，以提前承接第三方接入逻辑，即使 Phase 1 尚未完成全部构建集成。
- `third_party/manifest/`：必须显式存在，以提前承接版本锁定事实，即使首批清单仍为最小集合。
- `tests/compatibility/`：必须显式存在，以提前保留第三方兼容性验证与升级回归的测试落位。
- `tests/fault_replay/`：必须显式存在，以提前保留 snapshot、rollback、replay 与 `mass_deficit_account` 一致性验证落位。

#### Phase 1 可暂时保留未来扩展语义、且不纳入 Phase 1 必须交付/验收的目录
本组目录在 Phase 1 可暂不创建，仅保留未来扩展语义；其要求不同于上一组“必须显式存在、但可先最小占位”的目录。
- `python/scau_ai/`：可先保留为后续 AI 校核链路扩展语义。
- `apps/diagnostics/`：可先保留为后续诊断与分析工具扩展语义。
- `examples/dflowfm_coupling/`：可先保留为后续 D-Flow FM 联调示例扩展语义。

关键约束：即使 Phase 1 尚未完成 D-Flow FM 的完整联调，目录结构中也必须提前体现其并列 1D 引擎地位。

#### 3.7.5 与系统总架构和双引擎耦合章节的联动要求

- 第 3 章架构图中的一级模块，必须能在目录结构中找到明确落位；不得出现架构图有定义、目录结构无落位的孤立模块。
- 第 8 章双引擎耦合章节涉及的实现目录，必须在本节被显式列出，以保证共享耦合语义、排水侧适配、河网侧适配之间的职责边界可追踪。
- 详细目标工程落位见 `superpowers/specs/project-layout-design.md`；若后续历史 mapping note 与该文件冲突，以 `project-layout-design.md` 为准。

| 架构概念 | 目录落位 | 说明 |
|---|---|---|
| `Surface2DCore` | `libs/surface2d/` | 二维地表 DPM-HLLC 数值推进、源项、CFL 与诊断 |
| `Surface2DCore CPU backend` | `libs/surface2d/backends/cpu/` | 确定性参考路径 |
| `Surface2DCore CUDA backend` | `libs/surface2d/backends/cuda/` | 性能路径；不得独立定义耦合语义 |
| `CouplingLib core` | `libs/coupling/core/` | 共享流量池、仲裁、账本、回滚契约 |
| `SWMMAdapter` | `libs/coupling/drainage/` | 城市排水 1D 引擎适配 |
| `DFlowFMAdapter` | `libs/coupling/river/` | 河网 / 水工建筑物 1D 引擎适配 |
| `SWMM vendor` | `extern/swmm5/` | 第三方本体 |
| `D-Flow FM vendor` | `extern/dflowfm/` | 第三方本体 |
| `BMI bridge` | `extern/dflowfm-bmi/` | 第三方桥接层 |

---

## 4. 核心数据结构

**基础类型约定：**
- `Real` = `double`
- `RealF` = `float`
- `Index` = `int32_t`
- `Vector2` = `{Real x, Real y}`

### 4.1 三角/四边形混合非结构化网格 (MeshLib)

```cpp
namespace scau::mesh {

enum class CellType {
    Triangle,
    Quadrilateral
};

struct Node {
    Index id;
    Real x, y;
    Real elevation;
};

struct Edge {
    Index id;
    Index nodes[2];
    Index cells[2];
    Real length;
    Real normal[2];
    Real midpoint[2];
    Index color;
};

struct Cell {
    Index id;
    CellType type;
    Index node_count;
    Index edge_count;
    Index nodes[4];
    Index edges[4];
    Index neighbors[4];
    Real area;
    Real centroid[2];
};

class UnstructuredMesh {
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::vector<Cell> cells;
    std::vector<std::vector<Index>> edge_color_groups;
    Index num_colors;
};

} // namespace scau::mesh
```

`CellType::Triangle` 使用 `node_count = edge_count = 3`，仅 `nodes[0..2]`、`edges[0..2]` 与 `neighbors[0..2]` 有效；`CellType::Quadrilateral` 使用 `node_count = edge_count = 4`，`nodes[0..3]`、`edges[0..3]` 与 `neighbors[0..3]` 全部有效。任何几何遍历、边通量累加、分区 halo 与质量检查都必须以 `node_count` / `edge_count` 为边界，不得假设所有单元均为四边形。


### 4.2 STCF v5 / DPM 数据结构

```cpp
namespace scau::stcf {

// 逻辑 schema 定义；GPU 端一律使用 SoA 数组
struct CellSTCF {
    // --- DPM 主字段 ---
    Real phi_t;                     // 储流孔隙率 (容积孔隙率)
    Real phi_xx, phi_xy, phi_yy;    // 导流孔隙率张量 Phi_c

    // --- 阻塞/拖曳字段 ---
    // v5.2: a_w 从标量升级为方向性拖曳主轴表示
    Real drag_theta;                // 拖曳主轴方向
    Real drag_a_parallel;           // 沿主轴迎流面积密度
    Real drag_a_perp;               // 垂主轴迎流面积密度
    Real drag_cd;                   // 拖曳系数 C_d
    Real manning_n;                 // 地表材质粗糙度

    // --- 兼容字段 / 过渡层 ---
    Real clogging_factor;           // C_f (排水堵塞)
    Real coupling_friction_multiplier;  // M_local

    // --- 土壤/下渗 ---
    uint8_t soil_type;

    // --- AI 特征 ---
    uint8_t semantic_label;
};

// 界面字段：由 PreProc 直接按边计算
struct EdgeSTCF {
    Real omega_edge;                // 连通性因子 omega_ij [0,1]
    Real phi_e_n;                   // 法向有效导流能力 phi_{e,n} (几何计算)
    Real phi_et;                    // 切向有效导流能力
};

struct ChebyshevLUT {
    static constexpr int MAX_ORDER = 8;
    Real v_coeffs[MAX_ORDER];
    Real a_coeffs[MAX_ORDER];
    Real p_coeffs[MAX_ORDER];
    Real h_min, h_max;
    int order;
    int split_index;                // 分段点 (-1 表示不分段)
};

class STCFDataset {
    std::vector<CellSTCF>     cell_attrs;
    std::vector<EdgeSTCF>     edge_attrs;
    std::vector<ChebyshevLUT> cell_luts;
    // 注: ∇phi_t 不作为文件字段存储，由 Surface2DCore 在通量/源项路径中基于
    // cell.phi_t + mesh topology 采用界面中心差分即时构造。
    // 这是离散实现契约的一部分，避免冗余存储。 

    struct TopoMoments {
        Real mean, variance, skewness, kurtosis;
    };
    std::vector<TopoMoments> topo_moments;
};

// DPM 一致性校验
inline void validate_dpm_consistency(const CellSTCF& cell) {
    // 关键极值约束: phi_t >= max(phi_xx, phi_yy)
    if (cell.phi_t < std::max(cell.phi_xx, cell.phi_yy)) {
        throw DpmConsistencyError("phi_t must be >= max(phi_xx, phi_yy)");
    }
    if (cell.phi_t > Real(1)) {
        throw DpmConsistencyError("phi_t must be <= 1");
    }
    // 正定性约束: Phi_c 必须为对称正定张量
    if (cell.phi_xx <= 0 || cell.phi_yy <= 0) {
        throw DpmConsistencyError("Phi_c diagonal entries must be positive");
    }
    if (cell.phi_xx * cell.phi_yy - cell.phi_xy * cell.phi_xy <= 0) {
        throw DpmConsistencyError("Phi_c must be positive definite");
    }
    // v5.3.2 微调: 条件数 / 最小特征值保护
    Real trace = cell.phi_xx + cell.phi_yy;
    Real det = cell.phi_xx * cell.phi_yy - cell.phi_xy * cell.phi_xy;
    Real disc = std::sqrt(std::max(Real(0), trace * trace - Real(4) * det));
    Real lambda_min = Real(0.5) * (trace - disc);
    Real lambda_max = Real(0.5) * (trace + disc);
    if (lambda_min < 1e-6) {
        throw DpmConsistencyError("Phi_c minimum eigenvalue is too small");
    }
    if (lambda_max > Real(1)) {
        throw DpmConsistencyError("Phi_c eigenvalues must be <= 1");
    }
    if (lambda_max / lambda_min > 1e4) {
        throw DpmConsistencyError("Phi_c condition number is too large");
    }
}

struct SoilParamsLUT {
    static constexpr int MAX_SOIL_TYPES = 16;
    struct Entry {
        float K_s;
        float psi_f;
        float theta_s;
        float theta_i;
    };
    Entry entries[MAX_SOIL_TYPES];  // float4 向量化读取
};

} // namespace scau::stcf
```

### 4.3 求解器状态

```cpp
namespace scau::solver {

struct FieldArrays {
    std::vector<Real>  h;
    std::vector<Real>  hu;
    std::vector<Real>  hv;
    std::vector<Real>  F_inf;       // v5: 改为 double
    std::vector<RealF> t_ponding;
};

struct FluxResult {
    Real f_h;
    Real f_hu;
    Real f_hv;
};

inline Real desingularized_velocity(Real h, Real hu, Real eps = 1e-6) {
    Real h4 = h * h * h * h;
    Real eps4 = eps * eps * eps * eps;
    return (std::sqrt(2.0) * h * hu) / std::sqrt(h4 + std::max(h4, eps4));
}

} // namespace scau::solver
```

---

## 5. DPM 主方程与界面离散

### 5.1 DPM 主方程骨架

采用 **Anisotropic Dual-Porosity Model (DPM)**：

- `phi_t`：储流孔隙率（控制储流能力）
- `Phi_c`：导流孔隙率张量（控制界面过流与方向性）
- `C_d, A_w`：亚网格建筑/障碍物形阻参数（方向性拖曳张量，控制定向动量耗散）
- `manning_n`：地表材质底摩擦参数（控制底面剪应力）

**职责边界（v5.1 锁定）：**
- `Phi_c` 只负责“几何导流/界面过流”
- `C_d + A_w` 只负责“亚网格形阻/建筑侧壁耗散”
- `manning_n` 只负责“地表材质摩擦”
- 三者不得在实现中重复表达同一物理作用

**连续方程：**

a) 数学形式：
\[
\frac{\partial(\phi_t h)}{\partial t} + \nabla \cdot (\mathbf{\Phi}_c\, h\mathbf{u}) = S_{mass}
\]

**动量方程：**
\[
\frac{\partial(\mathbf{\Phi}_c h\mathbf{u})}{\partial t}
+ \nabla \cdot \left(\mathbf{\Phi}_c h\mathbf{u} \otimes \mathbf{u} + \frac{1}{2} g \phi_t h^2 \mathbf{I}\right)
= \mathbf{S}_{topo} + \mathbf{S}_{\phi_t} + \mathbf{S}_f + \mathbf{S}_d + \mathbf{S}_{drain}
\]

其中孔隙率梯度补偿项显式写为：
\[
\mathbf{S}_{\phi_t} = \frac{1}{2} g h^2 \nabla \phi_t
\]

地形坡度项显式写为：
\[
\mathbf{S}_{topo} = - g\, \phi_t\, h\, \nabla z_b
\]
其中 `S_topo` 与 `S_phi_t` 都是二维向量源项，分别作用于 x/y 动量方程；二者必须与压力项采用同一界面模板离散，组成同一套 Well-Balanced 压力-源项对。

**守恒变量实现契约（v5.3.2 数值闭合修订）：**
- 物理推导层：储流量写作 `\phi_t h`，过流量写作 `\mathbf{\Phi}_c h\mathbf{u}`
- 实现层：**主状态仍存储几何水深与速度动量 `h, hu, hv`，不直接存储 `\mathbf{\Phi}_c h\mathbf{u}`**
- 但所有质量收支、耦合可提取水量、守恒审计一律以 `\phi_t h A` 为准
- `\mathbf{\Phi}_c` 仅在界面通量、压力项与源项离散中显式进入；`\phi_t` 仅在储量、静水压力与可用水量约束中显式进入
- 原因：直接存储张量化动量会显著增加状态复杂度与 GPU 寄存器压力，且不利于与标准 HLLC / SWMM 接口兼容；但质量闭合不得再用纯几何体积 `hA` 代替物理储量 `\phi_t h A`

**`Phi_c` 时间状态与扩展边界（v5.3.3 收口）**：
- Phase 1 / v5.3.3 主线中，`\phi_t`、`\mathbf{\Phi}_c`、`\phi_{e,n}` 与 `omega_edge` 均视为 PreProc 生成的静态 STCF 场；单次模拟推进过程中不得由求解器、AI 校核或耦合器运行时改写。
- 当前主方程默认 `\partial \mathbf{\Phi}_c / \partial t = 0`。因此动量方程中的 `\partial(\mathbf{\Phi}_c h\mathbf{u})/\partial t` 只表达水深与速度动量随时间变化，不包含建筑破坏、滑坡堵塞、闸控开口变化等动态几何项。
- 若未来引入动态输送孔隙率，必须先升级主方程与审计链：显式加入 `h\mathbf{u}\,\partial\mathbf{\Phi}_c/\partial t` 或等价源项，定义动态几何事件日志，重算 `phi_{e,n}` / `omega_edge` 的 edge-local 更新顺序，并给出与 `mass_deficit_account`、snapshot/replay、GoldenSuite 的一致性证据。
- 未完成上述升级前，任何运行时修改 `\mathbf{\Phi}_c` 或 `phi_{e,n}` 的实现都不得进入正确性路径，也不得作为发布级结果。

**一致离散要求：**
- `∇(0.5 g phi_t h²)` 中的 `phi_t`
- `S_{phi_t}` 中的 `∇phi_t`
- `S_{topo}` 中的 `∇z_b`
- 必须使用**同一套界面重构/差分模板**，否则无法保持静水平衡

### 5.2 DPM 双层阻力结构

#### 底摩擦层 `S_f`
- 表示地表材质摩擦
- 采用 Manning 形式
- 使用有效水深：
\[
h_{eff} = \frac{h}{\phi_t}
\]

#### 亚网格拖曳层 `S_d`
- 表示建筑侧壁/围墙/密集障碍物的**定向形阻**
- v5.2 升级为方向性拖曳张量：
\[
\mathbf{A}_w = \mathbf{R}(\theta_{drag})
\begin{pmatrix}
a_{\parallel} & 0 \\
0 & a_{\perp}
\end{pmatrix}
\mathbf{R}^{-1}(\theta_{drag})
\]

拖曳项采用深度平均形式：
\[
\mathbf{S}_d = -\frac{1}{2} C_d\, \mathbf{A}_w\, h\, \mathbf{u}|\mathbf{u}|
\]

量纲与作用点约束（v5.3.2 收口）：
- `\mathbf{A}_w` 单位固定为 `[1/m]`
- `C_d` 为无量纲系数
- `\mathbf{S}_d` 量纲为 `[m^2/s^2]`，作用于 `d(hu, hv)/dt`（实现层主状态方程）；与物理推导 `d(\mathbf{\Phi}_c h \mathbf{u})/dt` 在 `∂Phi_c/∂t = 0` 假设下等价（见 §5.A.1 作用对象明示）

物理意义：
- 沿街道/优势通道方向：`a_parallel` 小，阻力弱
- 横穿建筑群方向：`a_perp` 大，阻力强

这样 `Phi_c` 负责“能不能过”，`A_w` 负责“过的时候损耗多少”。

**重复阻塞风险约束（最初引入于 v5.3.1，现纳入 v5.3.2 规范）**：
当某区域 `Phi_c` 已接近闭塞（例如 `phi_{e,n} < 0.05`）时，PreProc 应同步限制 `A_w` 的增长，
避免 `Phi_c` 与 `A_w` 对同一障碍效应重复惩罚。若检测到“低 Phi_c + 极高 A_w”组合，视为异常组合并纳入审计对象；具体异常分级与处置见配套稳定性协议相应章节。

#### `S_drain` 策略
- 主方案：只进入质量方程（连续方程）
- CouplingLib 预留接口：
  - `drain_momentum_model = none | vertical_jet_alpha | calibrated`
- v5.3 默认：`none`

### 5.3 非结构网格界面导流定义

对任意边 `e_ij`，法向量 `n_ij = (n_x, n_y)`，切向量 `t_ij = (-n_y, n_x)`。

**符号防混淆条款（v5.3.2 收口）**：
- `phi_t`：储流孔隙率标量，决定储量 `phi_t h A`
- `Phi_c`：单元导流孔隙率张量，决定单元内部方向性过流能力
- `phi_{e,n}`：由 `Phi_c` 投影得到的边法向导流标量，决定界面对流通量缩放
- 本文禁止脱离符号上下文单独使用“孔隙率”一词，以防将储流能力与导流能力混淆

**储量 / 输送主权分工（v5.3.2 第二轮收口）**：
- 储量主权只属于 `phi_t`：质量守恒、可提取水量、`V_limit`、deficit 账本与审计体积均使用 `phi_t h A`。
- 输送主权只属于 `Phi_c` / `phi_{e,n}`：界面通量、方向性传导、edge-local 压力-源项配对与零速稳态判据均使用张量投影结果。
- 二者不得互相代偿：不得用增大 `Phi_c` 修正储量缺口，也不得用调低 `phi_t` 表达界面堵塞。

界面法向有效导流能力：
\[
\phi_{e,n} = \omega_{ij}\, \mathbf{n}_{ij}^T \mathbf{\Phi}_{c,e} \mathbf{n}_{ij}
\]

界面切向投影：
\[
\phi_{e,t} = \mathbf{t}_{ij}^T \mathbf{\Phi}_{c,e} \mathbf{t}_{ij}
\]

#### 界面张量取值规则
1. **首选**：PreProc 基于边界几何/射线扫描直接计算 `phi_{e,n}`
2. **退化**：若没有几何直接计算，则取保守规则：
\[
\phi_{e,n} = \omega_{ij} \cdot \min(\mathbf{n}^T \Phi_{c,i} \mathbf{n},\ \mathbf{n}^T \Phi_{c,j} \mathbf{n})
\]

**Edge-Local 评估锁定（v5.3.2 新增）**：
- 所有 `Phi_c` 投影操作必须在**边局部坐标系（Edge-Local frame）**下一次性完成评估
- 适用对象包括：`phi_{e,n}`、`phi_{e,t}`、界面压力项、`S_topo` / `S_phi_t` 的局部配对项
- 禁止先在 cell center 预估这些量，再平均到边；这种两步式处理会破坏张量投影与源项评估点的一致性

### 5.4 重构对象与 Hydrostatic Reconstruction

DPM 下仍然重构：
\[
\eta = h + z_b
\]

而不是 `h` 或 `phi_t h`。

给定界面两侧重构值：
\[
z_b^* = \max(z_{b,L}, z_{b,R})
\]
\[
h_L^* = \max(\eta_L - z_b^*, 0),\quad h_R^* = \max(\eta_R - z_b^*, 0)
\]

若原始单元速度定义为 `u = hu / h`、`v = hv / h`，则重构后必须同步得到：
\[
(hu)_L^* = h_L^* u_L, \quad (hv)_L^* = h_L^* v_L
\]
\[
(hu)_R^* = h_R^* u_R, \quad (hv)_R^* = h_R^* v_R
\]

即 HR 不仅重构水深，也必须同步重构传入黎曼求解器的保守变量对 `(h^*, h^*u, h^*v)`；禁止将 `h^*` 与未重构动量 `(hu, hv)` 直接拼接，否则静水台阶下会产生伪速度。

**速度恢复安全规程（防除零）**：
- 原始单元速度必须在重构前提取：`u_L = hu_L / max(h_L, h_dry)`，`v_L` 同理
- 若 `h_L < h_dry`（干单元），则 `u_L = v_L = 0`（已由 5.A.2 节干湿规则强制）
- 重构后保守变量 `(hu)_L^* = h_L^* * u_L` 在 `h_L^*` 或 `u_L` 为零时自然为零，无需额外分支
- 禁止在重构后使用 `(hu)_L^* / h_L^*` 来回解速度——这会在 `h_L^* → 0` 时产生除零

### 5.5 DPM-HLLC 调用顺序

1. 重构 `eta_L`, `eta_R`
2. Hydrostatic reconstruction 得到 `h_L^*, h_R^*`
3. 由单元侧速度恢复重构后的保守状态 `(h_L^*, (hu)_L^*, (hv)_L^*)` 与 `(h_R^*, (hu)_R^*, (hv)_R^*)`
4. 在边法向局部坐标系调用**标准 HLLC**，其输入状态必须是 HR 后的保守变量
5. 先得到未乘孔隙率的**对流基准通量** `F_{adv,HLLC}^*`
6. 再使用 `phi_{e,n}` 仅对**质量与平流动量通量**做界面导流缩放，并与同模板离散的静水压力/源项配对：
\[
F_e^{adv} = \phi_{e,n} F_{adv,HLLC}^*
\]
7. 界面阻塞判据分两类：
   - 若 `omega_{ij} = 0`：按 hard-block 墙边界处理，不计算界面对流通量，且不计算界面中心 WB 压力/源项配对贡献
   - 若 `0 < omega_{ij} < epsilon_omega` 或 `phi_{e,n} < phi_{edge,min}`：按 soft-block 处理，令 `F_e^{adv} = 0`，但保留界面中心 WB 压力/源项配对项

#### 5.5.1 波速估算锁定（DPM-HLLC）

`S_L`、`S_R` 必须遵循 Einfeldt/Toro 路径，`S_*` 必须使用标准接触波公式；dry-bed simple-wave fallback 必须显式实现。CPU reference / CUDA backend / 正确性路径 禁止替换为其他波速估算。

\[
S_L = \min\left(u_{n,L}-\sqrt{g h_L^*},\ u_{n,R}-\sqrt{g h_R^*}\right)
\]
\[
S_R = \max\left(u_{n,L}+\sqrt{g h_L^*},\ u_{n,R}+\sqrt{g h_R^*}\right)
\]
\[
S_* = \frac{\frac{1}{2}g\left((h_R^*)^2 - (h_L^*)^2\right) + h_L^* u_{n,L}(S_L-u_{n,L}) - h_R^* u_{n,R}(S_R-u_{n,R})}{h_L^* (S_L-u_{n,L}) - h_R^* (S_R-u_{n,R})}
\]

其中 dry-bed simple-wave fallback 至少覆盖 `h_L^* \to 0` 或 `h_R^* \to 0` 的退化分支，且不得以“standard HLLC”字样替代明确公式与分支。

**重要**：
- 波速估算使用重构后的 `h_L^*, h_R^*`，但**不在波速公式内部额外引入 phi**，即：
\[
c = \sqrt{g h}
\]
- `phi_{e,n}` 代表界面导流能力，不代表储量；它不能替代 `phi_t`
- HLLC 本身不感知地形，静水平衡必须依赖 **HR 后保守状态 + 同模板压力/源项离散** 共同成立

**`phi_t` 跨界跳变下波速估算的适用域（v5.3.3 收口）**：
- 当 `|phi_{t,L} - phi_{t,R}| / max(phi_{t,L}, phi_{t,R}) > 0.5` 时，`c = sqrt(g h)` 仅作为低阶估计；该工况下 HLLC 中间通量精度退化为 O(`Δx`)
- G2 GoldenTest 通过判据（`u_hydro_tol`）仍然保证，但动态扰动下的守恒误差可能超出 §16 验收阈值
- 实现侧必须输出诊断字段 `count_phi_t_jump_events`，并在审计报告中显式记录跨界跳变工况的算例
- 未来 Cea-Vázquez-Cendón 升级路径已登记，其触发条件为 G2 + 动态扰动联合 GoldenTest 失败

#### 5.5.2 DPM 一致性引理与边平均算子（v5.3.3 收口）

**边平均算子锁定**：
- `Phi_c` 在边 `e_ij` 上的边值 `Phi_{c,e}` 唯一锁定为**算术平均**：
\[
\Phi_{c,e} = \frac{1}{2}(\Phi_{c,i} + \Phi_{c,j})
\]
- `phi_t` 在边上的边值 `phi_{t,e}` 同样使用算术平均：`phi_{t,e} = 0.5 * (phi_{t,i} + phi_{t,j})`
- 所有需要 `Phi_c` / `phi_t` 边值的离散位置（`phi_{e,n}` 投影、Edge-Local 压力配对、`S_topo` / `S_phi_t`）必须使用同一算子；禁止局部切换到调和平均或几何平均。

**DPM 一致性引理**：

"标准 HLLC × `phi_{e,n}` 缩放" 等价于真实 DPM 通量，**仅**在以下条件下成立：
1. `Phi_c` 沿边法向各向同性（`Phi_{c,i} n = λ_i n` 与 `Phi_{c,j} n = λ_j n` 仅相差标量）
2. 几何对称（边重心位于两侧单元几何中心连线上）
3. `phi_t` 沿边连续可导（不存在阶跃跳变）

满足上述条件时，5.5 第 6 步给出的 `F_e^{adv} = phi_{e,n} F_{adv,HLLC}^*` 与 DPM 真实通量在 O(`Δx²`) 精度内一致。

**适用域限制与发布边界**：
- G2 GoldenTest（`phi_t` 阶跃静水）保证零速静水平衡仍然成立——Edge-Local 压力配对承担补偿
- **`phi_t` 阶跃 + 动态扰动**（非零速度）属于已知验证空白区；本版本不保证守恒律 O(`Δx²`) 一致性，也不得作为发布级高阶守恒能力宣称
- 后续若需要在该工况下达到守恒等价，必须按 Cea-Vázquez-Cendón 路线引入 augmented HLLC（含 `phi_t` 跳变源项的 fluctuation form），并补 GoldenTest、确定性 replay 证据与稳定性协议执行后果。

**实现层强约束**：
- PreProc 必须在生成 `Phi_c` 时检查各向同性度量 `cond(Phi_c) <= cond_max`（已在 §16 约束）
- 边法向各向异性度量 `|n^T Phi_c n - 0.5 trace(Phi_c)| / λ_max(Phi_c) > 0.3` 时，必须输出 WARN 并标记该边为"DPM 一致性弱保证"，进入审计但不阻断

### 5.6 `S_{phi_t}` 的界面中心差分结构

**界面分类前置**（v5.3.3）：
- `omega_{ij} = 0`（hard-block）边：本节给出的 `S_{phi_t,e}` 与 `S_{topo,e}` 配对项 **不**装配；该边视为壁面，无贡献
- `0 < omega_{ij} < epsilon_omega` 或 `phi_{e,n} < phi_{edge,min}`（soft-block）边：配对项必须装配，与 5.5 第 7 步规则一致
- 仅当 `omega_{ij} >= epsilon_omega` 且 `phi_{e,n} >= phi_{edge,min}` 时，配对项作为常规 well-balanced 离散参与

为了在 `phi_t` 剧烈变化处维持 Well-Balanced，边贡献写成：
\[
\mathbf{S}_{\phi_t,e} \sim \frac{1}{2} g \left(\frac{h_L^* + h_R^*}{2}\right)^2 (\phi_{t,j} - \phi_{t,i}) \mathbf{n}_{ij}
\]

并与地形项一起组成界面压力-源项配对：
\[
\mathbf{S}_{topo,e} \sim - g \, \phi_{t,e} \, \bar{h}_e \, (z_{b,j} - z_{b,i}) \, \mathbf{n}_{ij}
\]

其中 `phi_{t,e}`、`\bar{h}_e`、`h_L^* / h_R^*` 必须与压力通量使用同一界面模板；任何一处单独改用不同平均或不同限幅器，都视为破坏数值闭合。
此外，`S_topo` / `S_phi_t` 与压力项的空间评估点必须统一锁定在同一 Edge-Local 界面位置；禁止使用 cell-centered 源项后再平均到边的实现路径。

### 5.7 v5 版时间推进与 CUDA 组织

#### Kernel 0：CFL Reduction
- 只对 `h > h_wet` 单元参与归约
- `c = sqrt(g*h)`
- CFL 用物理速度 `u = hu/h`
- **v5.1 锁定**：默认每个时间步都重算 CFL
- 性能降级备选：每 `N_cfl = 2~5` 步重算一次，并乘安全系数 `CFL_safety = 0.8`

#### Kernel 1：单元梯度 / 水位重构
- 重构 `eta`
- Green-Gauss + Venkat 限制器

#### Kernel 2：边通量 (Thread-per-Edge)
- 读取左右单元
- Hydrostatic reconstruction
- 计算 `phi_{e,n}`
- `omega_ij = 0`：hard-block，零对流通量，且不组装界面中心 WB 配对项
- `0 < omega_ij < epsilon_omega` 或 `phi_{e,n} < phi_edge_min`：soft-block，零对流通量，但保留 WB 配对项
- 否则 HLLC + `phi_{e,n}` 缩放
- **默认策略（v5.1 锁定）**：单次 launch + atomicAdd + CUDA Graphs
- 备选：边着色（仅在 CUDA backend benchmark（release 级 §2.2 Phase 2 / legacy.12 M4）明确优于默认策略时才允许切换）
- 开发期不得私自切换策略，避免实现漂移

#### Kernel 3：单元更新 (Thread-per-Cell)
1. 汇总边通量散度
2. 加入 `S_topo + S_{phi_t}`
3. 质量项更新（雨、下渗、SWMM）
4. 先得到 `h_mass`
5. 用 `h_mass` 计算双层阻力
6. 半隐式更新动量
7. 干湿处理 + 安全阀

**张量运算防御（补充条款 D.4）**：
- 所有 GPU 张量求逆/除法前必须先做非零与病态检查
- 快速除法（如 `__fdividef`）仅允许在通过非零检查后使用
- 对 `det(I + dt C_total)` 和关键分母执行显式阈值判断

**寄存器压力风险项（v5.3.2 微调）**：
在考虑拆分 `K3a/K3b` 之前，优先尝试：
1. 模板元编程消减分支
2. warp shuffle / `shfl` 通信减少临时寄存器占用
3. 常量折叠与只读缓存优化

若 `nvcc --ptxas-options=-v` 仍报告 Kernel 3 寄存器数 > 80，
再允许拆分为：
- `K3a`: 质量项 + h_mass 更新
- `K3b`: 动量项 + 阻力 + 干湿处理
以换取更高 occupancy。

---

## 5.A （保留展开）双层阻力、干湿处理与数值保险

### 5.A.1 双层阻力的半隐式系数构成

#### Manning 部分

Manning 摩阻系数基于有效水深 `h_eff = h / phi_t`（见 5.2 节定义），推导如下：
\[
C_f = \frac{g\, n^2\, |\mathbf{u}^n|}{h_{eff}^{4/3}} = \frac{g\, n^2\, |\mathbf{u}^n|\, \phi_t^{4/3}}{h^{4/3}}
\]
说明：`phi_t` 以 `4/3` 次幂进入分子（而非 1 次幂进入分母），这与 `h_eff = h / phi_t` 的定义一致。当 `phi_t → 1`（开阔地带），`C_f` 退化为标准 Manning 形式 `g n^2 |u| / h^{4/3}`。

**密集区摩阻保护（v5.3.3）**：
- 当 `phi_t < phi_t_min_friction`（默认 `0.1`，登记于 §16）时，Manning 摩阻按 `phi_t = phi_t_min_friction` 计算，避免 `phi_t → 0` 时摩阻消失
- 该保护不影响连续方程中的储量主权（仍按真实 `phi_t`）；仅作用于摩阻系数计算

#### 拖曳部分
方向性拖曳矩阵：
\[
\mathbf{C}_{drag} = \frac{1}{2} C_d\, \mathbf{A}_w\, |\mathbf{u}^n|
\]

总阻力矩阵：
\[
\mathbf{C}_{total} = C_f \mathbf{I} + \mathbf{C}_{drag}
\]

**作用对象明示（v5.3.3）**：
- 半隐式更新方程 `(I + dt C_total) (hu, hv)^{n+1} = (hu, hv)^{exp}` 中，`C_total` 不含 `Phi_c` 调制
- 即：`S_d` 实际作用于 `d(hu)/dt`，不是 `d(Phi_c h u)/dt`
- 由于 Phase 1 主线锁定 `∂Phi_c/∂t = 0`（见 §5.1），上述等价于"标量 `Phi_c` × 标准拖曳"，物理一致
- 若未来引入动态 `Phi_c`，必须显式重写 `C_total` 含 `Phi_c` 调制

半隐式更新：
\[
(\mathbf{I} + \Delta t\, \mathbf{C}_{total})
\begin{pmatrix}hu \\ hv\end{pmatrix}^{n+1}
=
\begin{pmatrix}hu \\ hv\end{pmatrix}^{exp}
\]

由于 `A_w` 是 2×2 对称张量，仍可用 2×2 解析求逆实现，
与 v5 的张量半隐式框架兼容。

若 `det(I + \Delta t C_total) < epsilon_det`：
- 禁止走标准求逆
- 默认优先退化为**标量等效阻力**更新
- 标量等效阻力唯一锁定为：
\[
C_{eq} = C_f + \lambda_{max}(\mathbf{C}_{drag})
\]
- 退化更新写为：
\[
(1 + \Delta t \, C_{eq})
\begin{pmatrix}hu \\ hv\end{pmatrix}^{n+1}
=
\begin{pmatrix}hu \\ hv\end{pmatrix}^{exp}
\]
- 该退化路径只允许比原张量更新**更耗散或等耗散**，禁止更激进
- 对角近似仅允许作为调试/对照分支，不得作为发布默认路径
- 执行 `count_drag_matrix_degenerate += 1`

### 5.A.2 干湿边界判据

- `h < h_dry`：干单元
- `h_dry <= h < h_wet`：近干单元
- `h >= h_wet`：湿单元

#### 处理规则
1. **干单元**
   - 不参与 CFL
   - 不输出界面对流通量
   - `hu = hv = 0`
2. **近干单元**
   - 允许继续更新质量项
   - 不再允许“冻结或强限制”二选一；统一执行规则为：若子步结束时 `h < h_wet`，则强制 `hu = hv = 0`
   - 任何界面抽吸通量都必须以 donor 侧物理储量 `phi_t h A` 为上界；只要该通量会使 donor 侧预测后 `phi_t h A < 0`，就必须裁剪到非负储量边界
3. **低 `phi_t` 区域**
   - 使用更严格干阈值：
     - `h_dry_dense = 5e-4 m`（v5.3.3）
   - 建筑密集区避免数值薄膜流
   - **硬约束**：`h_dry_dense > h_wet`；若配置使 `h_dry_dense <= h_wet`，PreProc 必须拒绝该参数集

### 5.A.3 通量裁剪与堵塞保险

按界面堵塞语义分两类处理：
- `omega_ij = 0`：hard-block，直接令界面对流通量为零，且不组装界面中心 WB 压力/源项配对项
- `0 < omega_ij < epsilon_omega` 或 `phi_{e,n} < phi_edge_min`：soft-block，直接令界面对流通量为零，但保留界面中心 WB 压力/源项配对项

对应对流通量裁剪为：
\[
F_e^{adv} = 0
\]

**v5.3.2 闭合修订**：
- 不再将 `min(h_L^*, h_R^*) < h_cut` 作为唯一的“一刀切零通量”判据
- 取而代之，采用“近干冻结 + 非负储量约束”联合规则：
  - 若界面任一侧 `h^* < h_wet`，仅允许不致使 donor 侧 `\phi_t h A` 变负的通量通过
  - 若预测子步后任一单元 `\phi_t h A < 0`，则按可用储量回退该界面抽吸通量
- 目的：避免在湿润锋附近因粗暴 `h_cut` 切断而破坏质量闭合或制造假干湿振荡

推荐默认值：
- `epsilon_omega = 1e-4`
- `phi_edge_min = 0.01`
- `h_cut` 仅保留为调试诊断阈值，不再作为主闭合判据

### 5.A.4 安全阀

#### 负水深保护
若更新后 `h < 0`：
- 强制 `h = 0`
- `hu = hv = 0`
- 视为异常状态并纳入日志审计；具体异常分级与处置见配套稳定性协议相应章节

#### 最大物理流速安全阀
在耦合单元附近：
```cpp
if (is_coupling_cell[i] && velocity_mag > V_max_physical) {
    Real scale = V_max_physical / velocity_mag;
    hu[i] *= scale;
    hv[i] *= scale;
}
```

默认：`V_max_physical = 10.0 m/s`

---

## 5.B （保留展开）AI 双阶校核系统

### 5.B.1 架构概述

```
Stage 1 (离线): PINN 阻力场反演
   输入: 网格坐标 + STCF 属性 + PIV/历史观测
   输出: 基础 Manning 系数场 n(x,y)
   框架: PyTorch, 物理损失 = DPM 残差 + topo_moments 约束
         |
         v
Stage 2 (在线): 贝叶斯参数微调
   输入: 实时降雨 + 监测点水位
   方法: GP 代理模型 + Optuna 优化
   输出: 分类阻力修正系数
```

**AI 边界约束（v5.2 强化）**：
- AI **不直接输出** `Phi_c` 或 `omega_edge`
- AI 只输出：`manning_n`、分类修正系数、语义标签
- `Phi_c` / `omega_edge` 仍由几何/GIS/PreProc 规则计算
- 这样保证可解释性、正定性与静水平衡不被黑盒破坏

**AI 先验围栏（v5.2 新增）**：
不同语义类别的 Manning n 必须落在物理可接受区间内，例如：
- 柏油路: `[0.012, 0.018]`
- 水泥路: `[0.011, 0.017]`
- 草地: `[0.03, 0.08]`
- 密集植被: `[0.06, 0.15]`

优化器/PINN 若给出超界值：
- 先 clamp 到边界
- 视为先验违规并纳入后处理审计；具体异常分级与处置见配套稳定性协议相应章节
- 将该样本标记为“先验违规”供后处理审计

**AI 冲突仲裁机制（v5.3 新增）**：
- 优先级：`物理约束 > AI 建议`
- 若 AI 连续 3 次对同一区域提出的修正都被 clamp 截断：
  1. 触发 `count_ai_prior_conflict += 1`
  2. 将该区域的 AI 校核权重置零
  3. 维持默认物理参数；该状态的后续判定与处置见配套稳定性协议相应章节
- **最初补充于 v5.3.1，现纳入 v5.3.2 规范**：连续 3 次的判据采用滑动时间窗口，而非单步瞬时计数
- 在置零前先执行一次 Learning Rate Backoff，避免合法修正被瞬时震荡误伤
- 目的：防止局部 AI 故障污染全局模型

### 5.B.2 PINN Loss

```python
def physics_loss(x, y, t, h, hu, hv, n, phi_t, topo_moments):
    res_mass = phi_t * dh_dt + div(Phi_c * F) - S
    sf_x = (n**2 * hu * sqrt(hu**2 + hv**2)) / (h**(7/3) + 1e-6)
    moment_reg = correlate(n, topo_moments.variance, topo_moments.skewness)
    return res_mass, sf_x, sf_y, moment_reg

L_total = L_obs + alpha*L_physics + lambda_tv*TV(n) + lambda_m*L_moments
```

### 5.B.3 运行时参数更新 API

```cpp
class SimDriver {
public:
    // 生命周期
    void initialize(const SimConfig& config);    // 加载配置、分配 GPU 显存、初始化所有子系统
    void step();                                  // 执行一个 dt_couple 周期（含 2D 子步、1D 耦合、诊断）
    void finalize();                              // 释放资源、输出最终诊断报告

    // 快照与回滚
    void snapshot();                              // 保存当前 2D 状态 + deficit 账本 + exchange buffer
    void rollback();                              // 恢复到最近一次 snapshot 状态
    void replay(Real new_dt_couple);              // 以缩减后的 dt_couple 重新执行本周期

    // AI 运行时更新
    void update_friction_multipliers(const std::vector<Real>& class_multipliers);
    void trigger_online_calibration(Real current_time);

    // 诊断
    RuntimeCounters get_counters() const;
    DiagnosticReport export_diagnostics() const;
};
```

---

## 6. 耦合核心协议

本章定义 1D-2D 耦合交换的完整语义，包括共享流量池、仲裁、交换数据契约、deficit 体积账与 rollback / replay 覆盖范围。

### 6.1 共享流量池与优先级仲裁

SCAU-UFM 采用**并列双引擎**，由 `SimDriver` 统一调度，外层共享耦合时钟 `dt_couple`，内部时间步独立（`dt_2d`, `dt_swmm`, `dt_dfm`）。

- `SWMMAdapter`：城市排水管网 / 检查井 / 雨水口 / 封闭管渠
- `DFlowFMAdapter`：一维河网 / 水工建筑物 / RTC / 堤防与漫滩口门
- `Surface2DCore`：GPU 上的二维 DPM 地表求解器

**并列 1D 引擎交换数据流（v5.3.4 文档固化）**：

```text
SWMMAdapter.request_exchange()      DFlowFMAdapter.request_exchange()
             |                                  |
             +---------------+------------------+
                             v
                    ExchangeRequest[]
                             |
                             v
                    CouplingLib core
             +---------------+------------------+
             |                                  |
      Q_limit / V_limit                 priority_weight
             |                                  |
             +---------------+------------------+
                             v
                    ExchangeDecision[]
                             |
             +---------------+------------------+
             |                                  |
SWMMAdapter.accept_exchange()       DFlowFMAdapter.accept_exchange()
             |                                  |
             v                                  v
       SWMM lateral inflow            D-Flow FM BMI set_value()
```

上述流程只表达数据责任边界，不新增耦合算法；所有 request、grant、deficit、write-off、rollback 与 replay 语义仍以 `CouplingLib core` 为唯一事实源。

共享流量池以 `Q_swmm_req / Q_dfm_req` 作为并列 1D 引擎的意向申报入口。仲裁规则：

1. **安全上界**：每个耦合单元可提取量受 `Q_limit` 约束，按两步定义：
\[
V_{limit} = 0.9\, \phi_t\, h\, A
\]
\[
Q_{limit} = \frac{V_{limit}}{dt_{sub}}
\]
其中 `V_limit` 单位为 `m^3`，`Q_limit` 单位为 `m^3/s`。`Q_max_safe` 仅作为历史别名保留，已弃用，不得作为 machine-facing 名称使用。

1.1. **多引擎共享单元的 V_limit 切分**（v5.3.3 新增）：当同一耦合单元同时挂多个 1D 引擎交换口（SWMM 节点 + D-Flow FM 河网交换点）时，单元的 `V_limit` 按引擎 `priority_weight` 分摊：
\[
V_{limit,k} = \frac{priority\_weight_k}{\sum_j priority\_weight_j} \times V_{limit}
\]
\[
Q_{limit,k} = V_{limit,k} / dt_{sub}
\]
本规则定义 `priority_weight` 为 machine-facing 仲裁字段，必须由 adapter 上报并进入审计、回放与测试证据链。
2. **deficit 清偿优先**：`mass_deficit_account` 中未偿还的体积优先于新意向交换量；新申报只能在清偿路径之后竞争剩余额度。
3. **比例缩放**：当总意向超出安全上界时，按各引擎 `priority_weight` 做线性比例缩放（先缩放，再硬切断）。

**`priority_weight` 在仲裁中的明确角色**（v5.3.3）：
- `priority_weight` 同时参与：
  1. 多引擎共享单元的 `V_limit` 切分（见规则 1.1）
  2. 总意向超出安全上界时的比例缩放
- 比例缩放公式扩展为：
\[
Q_{granted,k} = priority\_weight_k \times Q_{req,k} \times \max\!\left(0,\; \min\!\left(1,\; \frac{Q_{limit} - Q_{repay}}{\sum_j priority\_weight_j \times Q_{req,j}}\right)\right)
\]
- `priority_weight` 是 §7.5 `RiverExchangePoint.priority_weight` 的 machine-facing 仲裁字段；字段注释必须与本节保持一致

当 `Q_repay >= Q_limit` 时本子步 `Q_granted,k = 0`，全部供给用于清偿；当步未满足的新意向自动进入 `mass_deficit_account`。`Q_granted,k` 必须满足 `0 <= Q_granted,k <= Q_req,k`，违反 → ERROR。
4. **drain_split 补充**：若仲裁后单个子步 `Q_granted * dt > 0.2 * phi_t * h * A`，则将该子步拆分为多个微步执行（20% 平滑规则），微步数不超过 5。该阈值的参考底显式锁定为整体几何储量 `phi_t * h * A`（**而非** `V_limit` 或 `V_limit_k`）；多 donor 共享单元下，drain_split 阈值仍按整体 `phi_t * h * A` 评估，不按 `priority_weight` 切分后的 `V_limit_k` 评估。
5. **非负储量回退（硬闸门组成部分）**：即使经过上述裁剪与 split，若任一 donor 侧预测后 `\phi_t h A < 0`，必须继续回退到非负储量边界。多 donor 共享单元下的非负回退按线性规划解算：
\[
\max \sum_k Q_{granted,k} \quad \text{s.t.} \quad \sum_k Q_{granted,k} \cdot dt_{sub} \le V_{limit},\ 0 \le Q_{granted,k} \le Q_{req,k}
\]
该解必须在单步内闭式解算（KKT 单步），禁止迭代裁剪——迭代不收敛风险下必须 `ABORT`。本规则是硬闸门组成部分。

### 6.2 交换数据契约（共享 DTO）

以下 DTO 由 `libs/coupling/core/` 统一定义，两侧 adapter 共用：

```cpp
struct ExchangeRequest {
    std::vector<int>  node_ids;         // 参与交换的节点列表
    std::vector<Real> requested_q;      // 各节点期望交换量 (m³/s)
    Real              dt_sub_seconds;   // 当前交换子步宽 (s)；命名直接绑定语义（v5.3.3）
    [[deprecated("use dt_sub_seconds")]] Real dt; // 一个版本周期后移除；保留期间必须与 dt_sub_seconds 同步赋值
};

// ExchangeRequest 字段不变量（v5.3.3）：
// - dt_sub_seconds > 0
// - dt_sub_seconds <= dt_2d（adapter 端必须断言，违反 → ERROR）
// - 兼容期：dt（如设置）必须与 dt_sub_seconds 数值相同；运行时检测不一致则 → ERROR

struct ExchangeDecision {
    std::vector<int>  node_ids;
    std::vector<Real> granted_q;        // 经仲裁后允许的交换量 (m³/s)
    std::vector<Real> deficit_volume;   // 本次未满足的体积 (m³)
};

struct EngineReport {
    bool   healthy;
    Real   max_node_head;               // 最大节点水头 (m)
    int    surcharge_count;             // 当前超载节点数
    Real   total_lateral_inflow;        // 总侧向入流 (m³/s)
};
```

### 6.3 交换账本语义

- `exchange_buffer` 负责记录 request / granted / deficit 等本周期交换账目，是仲裁与回放的唯一交换缓冲语义载体。
- `mass_deficit_account` 负责保存未完成交换的**体积账（m³）**，并定义 deficit 的滚动、清偿与审计语义：
\[
V_{deficit,new} = V_{deficit,old} + (Q_{target} \times dt_{sub} - Q_{applied} \times dt_{sub})
\]
  - `Q_target = Q_req_k`，`Q_applied = Q_granted_k`，两者在每个节点/每个子步都必须可追溯。
  - 后续 2D 子步优先偿还 `V_deficit`，偿还时按当前子步 `dt_sub` 换算为 `Q_repay`。
  - 若一个 SWMM 大步结束仍未清偿，则残余 deficit 反馈路径必须绑定到机器可读字段/接口（例如 adapter/core DTO 字段）；禁止仅以自然语言“反馈给节点状态”而无结构化落点。
  - `write-off` 仅用于连续 `N_writeoff_steps`（默认 3）个大步后仍无法清偿的坏账核销；核销体积必须计入 `count_writeoff_volume_total`，并在守恒报表中显式扣减，禁止 silent discard。
- `rollback / replay` 的覆盖范围至少包括 2D 状态、`exchange_buffer`、`mass_deficit_account`、1D 节点/变量状态与运行时计数；不得只回滚局部状态。

### 6.4 CouplingLib 三层结构

`libs/coupling/` 划分为：
- `core/`：共享流量池、`mass_deficit_account`、冲突仲裁、`FaultController`、黑匣子、rollback 协议、共享 DTO
- `drainage/`：`SWMMAdapter`
- `river/`：`DFlowFMAdapter`

### 6.5 Snapshot 覆盖范围

```cpp
struct CouplingSnapshot {
    StateSnapshot          surface_2d;       // 2D 守恒量 h, hu, hv
    ExchangeBufferState    exchange_buffer;   // 本周期交换账目
    MassDeficitAccount     deficit_account;   // deficit 体积账
    SwmmNodeStateSnapshot  swmm_nodes;        // SWMM 节点状态
    FaultControllerState   fault_state;       // 熔断/恢复状态
    RuntimeCounters        counters;          // 运行时计数器
};
```

- snapshot 必须在每个 `dt_couple` 周期起始前完成，且早于任一 1D 引擎的 `request_exchange()`
- 任何 rollback 都必须同时恢复上述全部状态；禁止只恢复 2D 水深而不恢复耦合账本或 1D 节点状态
- replay 期间 exchange buffer 更新顺序必须保持确定性；运行时计数器在 rollback 后也必须恢复到快照值
- 确定性 replay 严格规则（v5.3.3）：同一 snapshot、输入事件序列与 `dt_couple` 切分下，`h/hu/hv`、exchange buffer、`mass_deficit_account`、FaultController 状态与 RuntimeCounters 的逐字段差异必须严格为 0；任何非零差异均判定 replay 失败，不得使用容差掩盖

## 7. 引擎适配器契约

本章定义 1D 引擎抽象接口与适配器边界。`ExchangeRequest` / `ExchangeDecision` / `EngineReport` 等共享 DTO 由耦合 core 统一定义（见第 6.2 节）；adapter 只负责请求、裁定与状态的映射，不负责定义 deficit、交换账本或 rollback / replay 语义。

### 7.1 ISwmmEngine

```cpp
class ISwmmEngine {
public:
    virtual ~ISwmmEngine() = default;

    // 生命周期
    virtual void initialize(const std::string& inp_path) = 0;
    virtual void step(Real dt_swmm) = 0;     // 推进一个 SWMM 内部步长
    virtual void finalize() = 0;

    // 节点状态读写
    virtual Real get_node_head(int node_id) const = 0;
    virtual Real get_node_lateral_inflow(int node_id) const = 0;
    virtual void set_node_lateral_inflow(int node_id, Real q) = 0;

    // 管段状态查询
    virtual Real get_link_flow(int link_id) const = 0;
    virtual bool is_surcharged(int node_id) const = 0;

    // 错误语义：任何调用失败抛出 SwmmEngineError
};

class SwmmEngineError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};
```

**`MockSwmmEngine`（Phase 1 保底实现，v5.3.3 行为完整化）**：

继承 `ISwmmEngine`；7 个接口方法的 Mock 行为如下：
- `initialize(inp_path)`：忽略 inp_path，将所有节点初始水头记为构造时配置值
- `step(dt_swmm)`：递增内部时间计数器；不进行水力计算
- `finalize()`：清空内部状态
- `get_node_head(node_id)`：返回 `initialize` 时记录的初始水头（不随 `set_node_lateral_inflow` 变化）
- `get_node_lateral_inflow(node_id)`：返回最近一次 `set_node_lateral_inflow` 累加后的本步总注入量
- `set_node_lateral_inflow(node_id, q)`：**累加**当前 step 内的总注入（与接口名 `set_*` 的覆盖语义不一致是有意行为；对应实接口语义见下条）
- `get_link_flow(link_id)`：固定返回 0
- `is_surcharged(node_id)`：固定返回 `false`

**接口语义补正（v5.3.3）**：
- `ISwmmEngine::set_node_lateral_inflow` 在真实 `SWMMAdapter` 中对应"覆盖当前侧向入流"语义（与 SWMM C-API `swmm_setNodeInflow` 一致）
- `MockSwmmEngine` 的"累加"行为是 Phase 1 测试便利，**不是接口契约**
- 测试集必须显式区分 `Mock` 与 `Real` 行为的预期；耦合 core 的接入代码必须按"覆盖语义"实现，禁止依赖 Mock 的累加
- 后续版本可能新增显式 `add_node_lateral_inflow` / `reset_node_lateral_inflow` 接口对，届时 Mock 行为重新对齐

### 7.2 SWMMAdapter

```cpp
class SWMMAdapter {
public:
    SWMMAdapter(std::unique_ptr<ISwmmEngine> engine, const CouplingConfig& cfg);

    // 耦合接口（与 DFlowFMAdapter 对称）
    ExchangeRequest request_exchange(const Surface2DState& state_2d);
    void accept_exchange(const ExchangeDecision& decision);
    EngineReport report_state() const;

    // 故障通知
    bool is_healthy() const;
    void enter_fault_isolation();
    void attempt_reconnect();
};
```

### 7.3 IDFlowFMEngine

```cpp
class IDFlowFMEngine {
public:
    virtual ~IDFlowFMEngine() = default;

    // 生命周期
    virtual void initialize(const std::string& mdu_path) = 0;
    virtual void update(Real dt_dfm) = 0;
    virtual void finalize() = 0;

    // BMI 风格读写
    virtual Real get_value(const std::string& var_name, int location_id) const = 0;
    virtual void set_value(const std::string& var_name, int location_id, Real value) = 0;

    // 标准 BMI 变量名表：
    //   "water_level"       — 节点水位 (m)
    //   "discharge"         — 管段/断面流量 (m³/s)
    //   "lateral_discharge" — 侧向入流量 (m³/s)
    //   "gate_opening"      — 闸门开度 (0~1)

    // 错误语义：任何调用失败抛出 DFlowFMEngineError
};

class DFlowFMEngineError : public std::runtime_error {
    using std::runtime_error::runtime_error;
};
```

**D-Flow FM BMI 边界约束（v5.3.4 文档固化）**：
- `IDFlowFMEngine` 只封装 D-Flow FM / BMI 的生命周期、状态读取与状态写入，不承载 `Q_limit`、`V_limit`、deficit、rollback 或仲裁语义。
- `get_value()` / `set_value()` 的变量名必须来自本节标准 BMI 变量名表；未知变量名或非法 `location_id` 必须抛出 `DFlowFMEngineError`，不得静默返回默认值。
- RTC、闸门、堰流或 culvert 的非法状态必须在 adapter 边界转化为 unhealthy engine report，并进入 `FaultController`；不得直接绕过 `CouplingLib core` 改写 2D 状态或 `Phi_c`。

### 7.4 DFlowFMAdapter

```cpp
class DFlowFMAdapter {
public:
    DFlowFMAdapter(std::unique_ptr<IDFlowFMEngine> engine, const CouplingConfig& cfg);

    // 耦合接口（与 SWMMAdapter 对称）
    ExchangeRequest request_exchange(const Surface2DState& state_2d);
    void accept_exchange(const ExchangeDecision& decision);
    EngineReport report_state() const;

    // 故障通知
    bool is_healthy() const;
    void enter_fault_isolation();
    void attempt_reconnect();
};
```

`DFlowFMAdapter` 的职责是把 D-Flow FM 的河网、水工建筑物与 RTC 状态映射为共享 DTO；它不得定义独立于 `CouplingLib core` 的交换限流、deficit 清偿或故障恢复规则。`request_exchange()` 必须只申报意向交换量；`accept_exchange()` 必须只执行 `ExchangeDecision` 已裁定的交换量；`report_state()` 必须把 BMI / RTC 异常转化为健康状态报告，供 `FaultController` 统一处理。

### 7.5 RiverExchangePoint

河网/边带交换点契约，由 `DFlowFMAdapter` 上报给 core：

```cpp
struct RiverExchangePoint {
    int         branch_id;
    int         link_id;
    std::string exchange_type;      // "bank_overtop" | "lateral_weir" | "gate_outlet" | "culvert_link"
    Real        crest_level;        // 堰顶高程 (m)
    Real        exchange_width;     // 交换宽度 (m)
    Real        priority_weight;    // 参与 §6.1 仲裁的 V_limit 切分与比例缩放系数；machine-facing 字段，必须 > 0
    bool        rtc_controlled;
    std::vector<int> neighbor_2d_edges;
};
```

`RiverExchangePoint` 必须进入审计、回放与 GoldenSuite 证据链。`exchange_type`、`priority_weight`、`rtc_controlled` 与 `neighbor_2d_edges` 是河道交换点复核的最小字段集合；任何 D-Flow FM 河网 / 水工建筑物交换不得只以自然语言配置存在。

### 7.6 错误边界与 const 正确性约定

- `ISwmmEngine` / `IDFlowFMEngine` 的所有 getter 方法标记为 `const`；`node_id` / `link_id` 越界时抛出对应 `EngineError`，不做静默返回。
- `SWMMAdapter` / `DFlowFMAdapter` 的 `request_exchange()` 不修改引擎内部状态；`accept_exchange()` 可修改引擎状态。
- 所有 `EngineError` 都继承 `std::runtime_error`，并携带 `engine_id` 与 `error_code` 字段。

## 8. 故障恢复与状态管理

本章定义 FaultController 状态机、snapshot / rollback / replay 的触发判据与行为约束、黑匣子转储规范与显存退化降级协议。

### 8.1 FaultController 有限状态机

```
状态定义：
  HEALTHY       — 引擎正常运行，可交换
  FAULTED       — 已熔断，交换口冻结为静止边界
  COOLDOWN      — 冷却期，不交换但监视 1D 引擎状态
  RECOVERING    — 软启动恢复中，gamma 从 0 线性回升
  ISOLATED      — 长期隔离，放弃重连，等待人工介入

状态转移：
  HEALTHY → FAULTED
    触发：数值发散 / BMI 接口调用失败 / RTC 返回异常 / 非法水位流量
    动作：切断该引擎本周期交换；对应交换口退化为冻结边界；
          2D 主场继续运行；另一 1D 引擎继续运行；
          转储熔断前 5 个步长的黑匣子数据

  FAULTED → COOLDOWN
    触发：熔断动作执行完毕
    动作：开始计数 cooldown_steps（默认 5）

  COOLDOWN → RECOVERING
    触发：cooldown_steps 结束 且 1D 引擎 report_state().healthy == true
    动作：初始化 gamma = 0；开始 recovery_steps 计数

  COOLDOWN → ISOLATED
    触发：cooldown_steps 结束 但 1D 引擎仍 unhealthy，
          或连续 max_retry_count（默认 3）次 COOLDOWN→RECOVERING 失败
    动作：冻结该引擎交换口；输出 [ENGINE_ISOLATED] 警报；
          递增 count_engine_unreachable[engine_id]

  ISOLATED → COOLDOWN
    触发：人工确认外部 1D 引擎已恢复后调用 force_clear_isolation(engine_id, operator_id)
    动作：记录 operator_id 与解除时间戳；输出 [ENGINE_ISOLATION_CLEARED] 审计事件；
          重新进入冷却期并从零开始健康探测

  RECOVERING → HEALTHY
    触发：gamma == 1.0 且本周期无 CFL 越限
    动作：恢复正常交换

  RECOVERING → FAULTED
    触发：恢复期间再次触发失稳
    动作：重新熔断；cooldown_steps *= 2（指数退避，上界 60）
```

```cpp
enum class FaultState { HEALTHY, FAULTED, COOLDOWN, RECOVERING, ISOLATED };

class FaultController {
public:
    FaultController(int cooldown_steps = 5, int recovery_steps = 10, int max_retry_count = 3);

    void on_fault(const std::string& engine_id);         // HEALTHY → FAULTED
    void tick(const std::string& engine_id, const EngineReport& report) noexcept; // 每个 dt_couple 调用一次
    void force_clear_isolation(const std::string& engine_id, const std::string& operator_id);
    FaultState state(const std::string& engine_id) const;
    Real       gamma(const std::string& engine_id) const; // RECOVERING 时 ∈ (0,1)
    bool       is_isolated(const std::string& engine_id) const;
};
```

`FaultController::tick()` 的异常语义（v5.3.3）：
- `tick()` 必须为 `noexcept`；状态推进不得因 adapter 异常半途退出
- `report_state()` / BMI / RTC 调用异常必须在 adapter 边界转换为 `EngineReport{healthy=false, error_code=ENGINE_UNREACHABLE}` 后进入 `tick()`
- 每次 `ENGINE_UNREACHABLE` 递增 `count_engine_unreachable[engine_id]`；该计数器属于 §6.5 snapshot 的 `RuntimeCounters`
- `ISOLATED` 不是永久不可逆状态；只能通过 `force_clear_isolation(engine_id, operator_id)` 人工解除，禁止自动重连绕过审计

### 8.2 软启动恢复

1D 引擎从熔断状态恢复后，交换通量不得一步跳回目标值。采用恢复因子 `gamma ∈ [0,1]`：
\[
Q_{actual} = \gamma \, Q_{target}
\]
`gamma` 在 `recovery_steps` 个耦合步内**线性**回升（`gamma_n = n / recovery_steps`，`n = 0, 1, ..., recovery_steps`；默认 10，可在 5~20 范围内调整）。线性是唯一锁定路径；smoothstep 仅作可选诊断模式，禁止用于正确性路径（破坏确定性 replay）。若恢复期间再次触发失稳，自动延长 `cooldown_steps` 并重新进入熔断状态。

**`gamma` 在 rollback / replay 期的行为（v5.3.3）**：`gamma` 必须随 `FaultController` 状态进入 §6.5 `CouplingSnapshot`；rollback 期间禁止重置或外部覆写 `gamma`；replay 必须按 snapshot 保存的 `gamma` 值精确重放。该规则是 §6.5 line 1220 "确定性 replay 严格规则"对 FaultController 状态的具体落点之一。

### 8.3 Rollback 触发判据与行为

- 若仲裁后的交换通量导致局部 CFL > 0.8（默认阈值），向 `SimDriver` 发送 `SIG_ROLLBACK`
- `SimDriver` 执行：**snapshot → rollback → reduce dt_couple → replay**
- 采用二分法或自适应缩减因子减小 `dt_couple`
- `dt_couple` 缩减下界为 `dt_couple_min`（默认 `1e-3 s`，登记于 §11.3）；若已触底（`dt_couple <= dt_couple_min`）仍触发 rollback，必须 `ABORT` 并写出黑匣子，禁止继续缩减
- 1D 引擎在 rollback 后必须恢复到本 `dt_couple` 周期起点的快照状态
- replay 时 1D 引擎必须从快照状态重新推进；禁止使用"快进但不重放耦合交换"的近似路径

**双 1D 引擎 rollback / replay 时序（v5.3.4 文档固化）**：

```text
[dt_couple boundary]
        |
        v
snapshot(surface_2d, exchange_buffer, deficit_account,
         swmm_nodes, fault_state, counters)
        |
        v
SWMMAdapter.request_exchange() + DFlowFMAdapter.request_exchange()
        |
        v
CouplingLib arbitration
        |
        +--> if max_cell_cfl <= C_rollback: accept decisions
        |
        +--> if max_cell_cfl > C_rollback for trigger window:
                  rollback(snapshot)
                  reduce dt_couple
                  replay from snapshot
                  if dt_couple <= dt_couple_min and still unstable: ABORT
```

D-Flow FM 的 BMI 不响应、RTC 异常或水工建筑物非法状态必须进入同一 `FaultController` 与 snapshot/replay 治理链，不得使用 D-Flow FM 专属旁路绕过 `CouplingLib core`。

### 8.4 黑匣子转储规范

`.crash` 文件必须包含：`exchange_state`、地形梯度、RTC 状态、局部水力坡降、熔断前 5 个时间步的关键交换上下文。

### 8.5 显存压力退化

当 GPU 显存占用 > 85% 时：
- 触发 `count_mem_pressure_events += 1`
- **Level 1 资源（必保留）**：`h, hu, hv`、地形、snapshot buffer
- **Level 2 资源（优先保留）**：耦合交换 buffer、CFL 中间量
- **Level 3 资源（可释放）**：非核心 AI 特征场、细粒度诊断 heatmap buffer
- 输出 `[MEM_PRESSURE_WARN]`

若 Level 3 降级后仍不足以安全保留全场 snapshot buffer，允许进入二级降级（分片 snapshot）——仅保留关键区域状态镜像，其余区域通过确定性 replay 重构。分片模式必须满足：不破坏总质量守恒审计、不破坏 `mass_deficit_account` 一致性、replay 保持确定性。

### 8.6 Snapshot / Checkpoint Schema

snapshot/checkpoint 文件必须按下表 schema 写入，G10 `snapshot_replay_mass_deficit` 的可复现性由 schema 约束而非口头约定承担：

| 字段 | 类型 | 必填 | 说明 |
|---|---|---|---|
| `schema_version` | string (semver) | 是 | minor 升版可读，major 升版必须 reject |
| `simulation_id` | uuid | 是 | 算例唯一标识 |
| `dt_couple_index` | int | 是 | 当前耦合大步序号 |
| `state_vector` | binary blob | 是 | 二维地表状态量 (h, hu, hv, eta)，SoA 布局 |
| `mass_deficit_account` | array | 是 | 单元级 deficit 账，与活动节点对应 |
| `exchange_state` | struct | 是 | ExchangeRequest / ExchangeDecision / EngineReport 三元组 |
| `swmm_state` | binary blob | SWMM 启用时必填 | SWMM hot-start 状态 |
| `dflowfm_state` | binary blob | D-Flow FM 启用时必填 | D-Flow FM BMI 写出的内部状态 |
| `rtc_state` | struct | D-Flow FM 启用时必填 | RTC 控制器、闸门、堰、泵当前位置/动作 |
| `recent_dt_couple_frames` | array of struct | 是 | 最近 5 个 `dt_couple` 的诊断摘要（`max_cell_cfl`、`count_*`、`Q_limit`） |
| `content_hash` | sha256 | 是 | 上述字段规约哈希；replay 起点必须匹配 |

写入语义：

- 写入路径：先写 `<basename>.snapshot.tmp`，fsync，原子 rename 为 `<basename>.snapshot`。
- 中途中止不得污染上一份成功 snapshot；rename 是原子边界。
- replay 起点必须验证 `content_hash` 与 `schema_version`：hash 不匹配立即 `ABORT`；`schema_version` major 不兼容立即 `ABORT`。

GoldenTest G10 evidence 必须包含 snapshot 起点 `content_hash`、replay 终点 `content_hash` 与 deficit 账差值；正确性路径下 deficit 账差值必须严格为 0。

---

## 9. 预处理与 STCF 生产规范

**主权说明**：本章为前部骨架第 9 章；详细生产链路与 PreProc 默认/可替换算法见 `legacy.10` 节。本章仅承担规范性定义。

### 9.1 PreProc 主线义务

- 提取 `phi_t`、`Phi_c`、`omega_edge`、`drag_*`、`manning_n`、`soil_type`、`semantic_label`
- 每级强校验（提取 `phi_t` / 生成 `Phi_c` / 生成 `omega_edge` / 导出 `.stcf.nc`）
- 任一级失败立即停止；非法张量禁止进入 GPU
- 默认算法见 `legacy.10`；任何替换算法必须输出同一 STCF v5 字段集合

### 9.2 STCF v5 schema 强约束

- `.stcf.nc` 含 `schema_version = 5`
- DPM 一致性约束（`phi_t >= max(phi_xx, phi_yy)`、`Phi_c` 正定 + 条件数 + 特征值）必须 PreProc 时校验，不依赖运行时
- 详细字段定义见 §4.2

---

## 10. 验证矩阵与正确性基线

**主权说明**：本章为前部骨架第 10 章，**承担 GoldenSuite 物理不变式矩阵的权威定义**。`legacy.13` 仅保留算例细节展开。

### 10.1 GoldenTest / GoldenSuite 关系

- `GoldenTest` 是单项正确性基准（atomic test）
- `GoldenSuite` 是发布 / 合并门禁级集合（每次 CI / merge 必须通过）
- 每个 `GoldenTest ∈ GoldenSuite`
- 缺少任一强制项脚本、确定性参考路径或可复现实证记录，均视为 GoldenSuite 不完整；执行后果由配套稳定性协议定义
- GoldenSuite 按 release Phase 分级：Phase 1 release gate 为 G1–G10；Phase 2 扩展为 G1–G12（追加 D-Flow FM 河网稳态与双 1D 引擎共享单元仲裁两项）。Phase 3 GoldenTest 扩展登记于 §12 未来扩展能力

### 10.2 GoldenSuite 物理不变式矩阵（v5.3.3 主权）

| # | GoldenTest | 最小判据 | 证据路径要求 | 适用 release Phase |
|---|---|---|---|---|
| G1 | `hydrostatic_step` | `eta` 不漂移、速度不超过 `u_hydro_tol` | 固定顺序规约参考路径 | Phase 1+ |
| G2 | `phi_t_jump_hydrostatic` | `phi_t` 跳变下仍保持静水平衡 | 压力项与 `S_phi_t` 同模板证据 | Phase 1+ |
| G3 | `phi_c_edge_zero_velocity` | `Phi_c / phi_{e,n}` 跳变不生成虚假速度 | edge-local 投影记录 | Phase 1+ |
| G4 | `narrow_gap_blockage` | `phi_{e,n} -> 0` 时无非法穿透通量 | hard-block / soft-block 分类日志 | Phase 1+ |
| G5 | `dpm_drag_decay` | 拖曳只耗散动量，不制造质量 | 能量/动量衰减轨迹 | Phase 1+ |
| G6 | `phi_c_spd_reject` | 非正定或病态 `Phi_c` 不得进入 GPU / 正确性路径 | PreProc 拒绝证据 | Phase 1+ |
| G7 | `stcf_v4_to_v5_migration` | 字段迁移后语义与单位不漂移 | schema 映射报告 | Phase 1+ |
| G8 | `swmm_single_pipe_surcharge` | SWMM 溢流交换不突破 `Q_limit` | deficit 与 split 审计 | Phase 1+ |
| G9 | `cpu_gpu_deterministic_match` | CPU/GPU 正确性路径误差满足协议阈值 | double + 确定性 reduction 记录 | Phase 1+ |
| G10 | `snapshot_replay_mass_deficit` | rollback/replay 后 `mass_deficit_account` 一致（严格 0） | replay 审计日志 | Phase 1+ |
| G11 | `dflowfm_river_steady` | D-Flow FM 单引擎稳态通路无虚假流量；`Q_limit` 不被违反 | RiverExchangePoint 仲裁日志 + 守恒证据 | Phase 2+ |
| G12 | `dual_engine_shared_cell` | SWMM 与 D-Flow FM 共享 2D 单元时 `V_limit_k` 切分、`Q_limit` 仲裁、deficit 账与 replay 一致 | 共享单元仲裁审计 + replay 一致性 | Phase 2+ |

### 10.3 确定性正确性路径

- GoldenTest 必须提供一条固定顺序规约的参考路径
- CPU 参考求解器默认使用 `double`
- GPU 正确性对照路径在调试 / CI 模式下必须锁定：double 精度、固定顺序 reduction、禁用不确定原子加作为唯一判据
- 性能路径可保留原子加 / CUDA Graphs，但其结果必须回归对照确定性参考路径

GoldenTest / GoldenSuite 的准入判定与执行后果见配套稳定性协议。

---

## 11. 参数与默认值索引

**主权说明**：本章为前部骨架第 11 章，**承担所有数值默认参数的权威定义**。`legacy.16` 仅保留历史背景与适用说明。所有重复出现于稳定性协议中的数值参数，统一引用本章；machine-facing 名称、单位与退役别名同步以 `superpowers/specs/2026-04-22-symbols-and-terms-reference.md` 为准。

### 11.1 干湿与通量裁剪

| 参数 | 默认值 | 单位 | 说明 |
|---|---|---|---|
| `h_dry` | 1e-6 | m | 干单元判据 |
| `h_wet` | 1e-4 | m | 湿单元判据 |
| `h_dry_dense` | 5e-4 | m | 建筑密集区干阈值（必须 > h_wet） |
| `h_cut` | 1e-5 | m | 仅调试诊断，非主闭合切断 |
| `phi_edge_min` | 0.01 | 无量纲 | soft-block 边阈值 |
| `epsilon_omega` | 1e-4 | 无量纲 | hard-block 边判据 |
| `V_max_physical` | 10.0 | m/s | 耦合单元附近最大物理流速安全阀 |
| `phi_t_min_friction` | 0.1 | 无量纲 | 摩阻保护下界（v5.3.3 新增） |

### 11.2 张量与阻力退化

| 参数 | 默认值 | 单位 | 说明 |
|---|---|---|---|
| `epsilon_phi` | 1e-6 | 无量纲 | `Phi_c` 最小特征值下界 |
| `cond_max` | 1e4 | 无量纲 | `Phi_c` 条件数上界 |
| `epsilon_det` | 1e-10 | 无量纲 | 阻力矩阵奇异性判据 |
| `r_drag_max` | 100 | 无量纲 | 方向性拖曳比值上限 |

### 11.3 耦合与恢复

| 参数 | 默认值 | 单位 | 说明 |
|---|---|---|---|
| `V_limit` 系数 | 0.9 | 无量纲 | `V_limit = 0.9 × phi_t × h × A` |
| `drain_split_threshold` | 0.2 | 无量纲 | 20% 平滑规则；参考底为 `phi_t × h × A`（而非 `V_limit` 或 `V_limit_k`），见 §6.1 规则 4 |
| `N_writeoff_steps` | 3 | 大步 | deficit write-off 阈值 |
| `cooldown_steps` | 5 | 耦合步 | 熔断后冷却（指数退避上界 60） |
| `recovery_steps` | 10 | 耦合步 | 软启动恢复（线性回升 gamma） |
| `max_retry_count` | 3 | 次 | 长期隔离触发 |
| `C_rollback` | 0.8 | 无量纲 | rollback CFL 触发阈值 |
| `CFL_safety` | 0.8 | 无量纲 | 时间步安全系数（语义独立于 C_rollback） |
| `dt_couple_min` | 1e-3 | s | dt_couple 缩减下界；触底仍 rollback → `ABORT`，见 §8.3 |

### 11.4 守恒与验收

| 参数 | 默认值 | 单位 | 说明 |
|---|---|---|---|
| `M_ref` | 由算例 t=0 计算 | m³ | `sum(phi_t × h × A) over all cells with h >= h_wet` |
| `epsilon_deficit` | `max(1e-10, 1e-12 × M_ref)` | m³ | rollback / replay 性能路径容差；正确性路径要求严格 0 |
| `epsilon_mass` | `max(1e-10, 1e-12 × M_ref)` | m³ | 总质量守恒误差容差 |
| `u_hydro_tol` | 1e-12 | m/s | GoldenTest 静水速度容差 |
| `eta_tol` | 1e-12 | m | GoldenTest 水位容差 |

---

## 12. 未来能力与保留扩展

**主权说明**：本章为前部骨架第 12 章，承担未来扩展能力的边界声明；CUDA 实施细节见 `legacy.9`。

### 12.1 已登记的扩展路径

- AI 双阶校核（PINN + 贝叶斯）：第 5.B 节定义边界，详细见 `legacy.10` / `legacy.11`
- CUDA Backend：核心策略见 `legacy.9`；Phase 1 不必须落地
- Snapshot 分片 / 关键断面重构降级：仅作显存压力下二级降级，规范性见第 8 章
- 动态 `Phi_c`（破坏 / 滑坡 / 闸控开口）：当前主线锁定 `∂Phi_c/∂t = 0`；启用前必须升级主方程、edge-local 更新顺序、审计链与 GoldenSuite
- DPM-augmented HLLC（Cea-Vázquez-Cendón 路线）：当前不实施；触发条件见 §5.5.2

### 12.2 未来扩展的准入约束

任何未来扩展进入主线前必须满足：
- 新增 GoldenTest 覆盖该扩展工况
- 新增确定性参考路径
- 新增 PreProc 校验规则（如适用）
- 协议侧补充执行后果与 operator 矩阵条目

---

> **旧展开区说明**：以下旧编号 8~16 章为历史展开补充，不再作为竞争性权威来源。若与前部 0~8 收口骨架冲突，以前部为准。
> 大致映射：旧8→新6/7/8，旧9→12，旧10→9，旧11→2/3，旧12→2，旧13→10，旧14→8/10/11，旧15→3/7/8，旧16→11。
> 第二轮脱水规则：旧章节只允许保留历史背景、实现顺序示例与对前部骨架的回指；不得继续保留可被误读为权威接口、权威阈值或权威执行后果的完整正文。

## legacy.8 并列双引擎 1D-2D 联合耦合架构（历史展开，对应新骨架 §6 / §7 / §8）

> 说明：本章保留 v5.3.2 的历史性详细展开，作为第 6、7、8 章的补充材料读取；若与前部骨架冲突，以前部为准。本章不构成新的竞争性权威来源。

### 旧8.1 总体定位

SCAU-UFM 采用 **并列双引擎**：
- `SWMMAdapter`：城市排水管网 / 检查井 / 雨水口 / 封闭管渠
- `DFlowFMAdapter`：一维河网 / 水工建筑物 / RTC / 堤防与漫滩口门
- `Surface2DCore`：GPU 上的二维 DPM 地表求解器

三者由 `SimDriver` 统一调度，外层共享耦合时钟 `dt_couple`，但内部时间步独立：
- `dt_2d`
- `dt_swmm`
- `dt_dfm`

### 旧8.2 CouplingLib 三层结构

建议 `libs/coupling/` 划分为：
- `core/`：共享流量池、质量欠款账户、冲突仲裁、热熔断、黑匣子、rollback 协议
- `drainage/`：`SWMMAdapter`
- `river/`：`DFlowFMAdapter`

### 旧8.3 ISwmmEngine 接口与 SWMMAdapter（历史展开，接口权威定义见第 7 章）

本节不再保留 SWMM 侧接口签名全文。`ISwmmEngine`、`MockSwmmEngine`、`SWMMAdapter` 与共享 DTO 的权威边界统一见第 7.1 / 7.2 节及第 6.2 节。

历史实现顺序仅保留为：先由 `SWMMAdapter` 读取排水侧状态，再转换为共享 `ExchangeRequest`，经 `libs/coupling/core/` 仲裁后接收 `ExchangeDecision`，最后把裁定结果写回 SWMM 引擎边界。任何 SWMM 原生类型、头文件或二进制布局都不得越过 adapter 进入 core 公共 ABI。

### 旧8.4 共享流量池与优先级仲裁（历史展开，核心定义见第 6 章）

本节只补充并列双引擎场景下的历史仲裁展开，不重新定义共享流量池、`Q_limit` 与 deficit 清偿优先级；核心语义见第 6.1 节。

共享流量池的核心语义、`Q_swmm_req / Q_dfm_req` 申报入口、`Q_limit` 约束、比例缩放优先于硬切断，以及 deficit 清偿优先级，统一见第 6.1 节。（`Q_max_safe` 已退役为历史别名，权威 machine-facing 名称为 `Q_limit`，详见 §6.1 与符号表）此处仅保留历史实现顺序说明：先汇总意向，再评估 `Q_limit`，最后按既定优先级写回各自 `exchange buffer`。

### 旧8.5 D-Flow FM 接口与数据契约（历史展开，接口权威定义见第 7 章）

本节只补充 D-Flow FM 侧接口展开与河网交换元数据示例，不重新定义 `IDFlowFMEngine` / `DFlowFMAdapter` 的权威边界；以第 7.3 / 7.4 节为准。

本节不再保留 D-Flow FM / BMI 侧接口签名全文。`IDFlowFMEngine`、`DFlowFMAdapter`、BMI 变量桥接、共享 DTO 边界与适配器职责，统一见第 7.3 / 7.4 节及第 6.2 节。

历史实现顺序仅保留为：先由 `DFlowFMAdapter` 读取河网侧水位、流量与控制状态，再转换为共享 `ExchangeRequest` 或 `RiverExchangePoint` 元数据，经 `libs/coupling/core/` 仲裁后接收 `ExchangeDecision`，最后写回 D-Flow FM / BMI 边界。D-Flow FM 原生对象与 BMI bridge 类型只能停留在 `libs/coupling/river/` 或 `extern/` 边界内。

#### `RiverExchangePoint`
作为线源/边带交换契约，建议包含：
- `branch_id / link_id`
- `exchange_type`:
  - `bank_overtop`
  - `lateral_weir`
  - `gate_outlet`
  - `culvert_link`
- `crest_level`
- `exchange_width`
- `priority_weight`（adapter 上报给 core 的 machine-facing 仲裁字段；参与 §6.1 的 `V_limit` 切分与比例缩放，必须 > 0）
- `rtc_controlled`
- `neighbor_2d_edges[]`

### 旧8.6 热熔断、冷却与重试（历史展开，状态语义见前部第 8 章）

本节只补充 `FaultController` 的历史状态机展开，不重新定义热熔断、冷却与重试的权威状态语义；以前部第 8 章相关定义为准。

若 `DFlowFMAdapter` 或 `SWMMAdapter` 出现：
- 数值发散
- BMI/接口调用失败
- RTC 返回异常
- 非法水位/流量返回

则 `core/FaultController` 执行熔断流程（见下方状态机）。本节只定义 `FaultController` 的状态语义与恢复不变式；触发阈值、执行后果、黑匣子审计与 Phase 准入证据见配套稳定性协议相应章节。

#### FaultController 有限状态机（v5.3.2 补全）

```
状态定义：
  HEALTHY       — 引擎正常运行，可交换
  FAULTED       — 已熔断，交换口冻结为静止边界
  COOLDOWN      — 冷却期，不交换但监视 1D 引擎状态
  RECOVERING    — 软启动恢复中，gamma 从 0 线性回升
  ISOLATED      — 长期隔离，放弃重连，等待人工介入

状态转移：
  HEALTHY → FAULTED
    触发：上述 4 类故障中的任一
    动作：切断该引擎本周期交换；对应交换口退化为冻结边界；
          2D 主场继续运行；另一 1D 引擎继续运行；
          转储熔断前 5 个步长的黑匣子数据

  FAULTED → COOLDOWN
    触发：熔断动作执行完毕
    动作：开始计数 cooldown_steps（默认 5）

  COOLDOWN → RECOVERING
    触发：cooldown_steps 结束 且 1D 引擎 report_state().healthy == true
    动作：初始化 gamma = 0；开始 recovery_steps 计数

  COOLDOWN → ISOLATED
    触发：cooldown_steps 结束 但 1D 引擎仍 unhealthy，
          或连续 max_retry_count（默认 3）次 COOLDOWN→RECOVERING 失败
    动作：冻结该引擎交换口并输出 [ENGINE_ISOLATED] 警报；ISOLATED 是可由人工审计解除的运行态，不是永久不可逆终态

  RECOVERING → HEALTHY
    触发：gamma == 1.0 且本周期无 CFL 越限
    动作：恢复正常交换

  RECOVERING → FAULTED
    触发：恢复期间再次触发失稳
    动作：重新熔断；cooldown_steps *= 2（指数退避，上界 60）
```

```cpp
enum class FaultState { HEALTHY, FAULTED, COOLDOWN, RECOVERING, ISOLATED };

class FaultController {
public:
    FaultController(int cooldown_steps = 5, int recovery_steps = 10, int max_retry_count = 3);

    void on_fault(const std::string& engine_id);         // HEALTHY → FAULTED
    void tick();                                           // 每个 dt_couple 调用一次，推进状态机
    FaultState state(const std::string& engine_id) const;
    Real       gamma(const std::string& engine_id) const; // 恢复因子，RECOVERING 时 ∈ (0,1)
    bool       is_isolated(const std::string& engine_id) const;
};
```

### 旧8.7 Snapshot / Rollback / Replay（Phase 1 硬要求，历史展开；核心语义见前部第 8 章）

本节只补充 snapshot / rollback / replay 的历史实现展开，不重新定义覆盖范围、回滚一致性与 replay 行为；以前部第 8 章相关定义为准。

当 `core/` 的 CFL 反查判断当前裁定通量会导致局部失稳时：
- `SimDriver` 必须执行：
  **snapshot -> rollback -> reduce dt_couple -> replay**

**Snapshot 实现要求补充**：
- snapshot 覆盖范围见第 6.5 节，`exchange_buffer` / `mass_deficit_account` 语义见第 6.3 节，rollback / replay 触发判据见第 8.3 节；本节仅补充历史实现约束。
- `Surface2DCore` 每个 `dt_couple` 周期起始前，必须保留一份 2D 状态镜像
- 仅备份必要守恒量：`h`, `hu`, `hv`
- 显存增量目标：不超过总显存的 15%
- snapshot 完成时间点：必须早于任一 1D 引擎的 `request_exchange()`

```cpp
struct ExchangeBufferState {
    std::vector<int>  interface_ids;     // 耦合接口或节点 ID
    std::vector<Real> requested_q;       // 本周期各接口申报交换量 (m³/s)
    std::vector<Real> granted_q;         // 本周期仲裁后交换量 (m³/s)
    std::vector<Real> deficit_volume;    // 本周期新增 deficit (m³)
};

struct SwmmNodeStateSnapshot {
    std::vector<int>  node_ids;
    std::vector<Real> node_head;         // 节点水头 (m)
    std::vector<Real> lateral_inflow;    // 侧向入流 (m³/s)
};

struct CouplingSnapshot {
    StateSnapshot          surface_2d;
    ExchangeBufferState    exchange_buffer;
    MassDeficitAccount     deficit_account;
    SwmmNodeStateSnapshot  swmm_nodes;
    FaultControllerState   fault_state;
    RuntimeCounters        counters;
};
```

- `exchange_buffer`、`mass_deficit_account`、SWMM 节点状态、FaultController 状态与运行时计数器都属于 snapshot 的强制覆盖范围
- 任何 rollback 都必须同时恢复上述状态；禁止只恢复 2D 水深而不恢复耦合账本或 1D 节点状态

**显存压力预警与动态降级（最终增强）**：
- `Surface2DCore` 在每个 `dt_couple` 周期开始前检查 GPU 剩余显存
- 当显存占用 > 85%：
  - 触发 `count_mem_pressure_events += 1`
  - 优先保留 Level 1 资源：`h, hu, hv`, 地形, snapshot buffer
  - 保留 Level 2 资源：耦合交换 buffer, CFL 中间量
  - 暂停更新或释放 Level 3 资源：非核心 AI 特征场、细粒度诊断 heatmap buffer
  - 输出 `[MEM_PRESSURE_WARN]`

**Snapshot 分片/关键断面重构降级协议（v5.3.2 新增）**：
- 默认主路径仍为**全场最小守恒量 snapshot**，不得被“分片模式”替代为常态实现
- 若已完成 Level 3 降级后，显存压力仍不足以安全保留全场 snapshot buffer，则允许进入二级降级模式：
  - 仅保留关键区域 / 关键断面 / 耦合影响域的状态镜像
  - 其余区域在 rollback 后通过最近一次稳定时刻的基态 + 可重放源项 + 确定性 replay 进行重构
- 关键区域至少必须覆盖：
  - 所有 1D-2D 耦合单元及其局部传播邻域
  - 当前 `max_cell_cfl` 热点团簇
  - 当前步发生 exchange / split / deficit 偿还的单元集合
- 进入该模式时，必须同步保留并可回滚：
  - `mass_deficit_account`
  - exchange buffer / 仲裁结果
  - fault / cooldown / recovery 状态
- 该降级模式必须满足三条硬约束：
  1. 不得破坏总质量守恒审计
  2. 不得破坏 `mass_deficit_account` 的体积账一致性
  3. replay 路径必须保持确定性，且同一输入下可重复回放
- 若上述任一条件无法满足，则禁止启用分片模式，必须退回更小 `dt_couple` 或中止当前算例

**Rollback 触发判据**：
- 若缩放后的交换通量导致局部 CFL > 0.8（默认阈值）
- 向 `SimDriver` 发送 `SIG_ROLLBACK`
- 采用二分法或自适应缩减因子减小 `dt_couple`

**1D 引擎在 rollback / replay 后的唯一行为（v5.3.2 补全）**：
- `SWMMAdapter` 与 `DFlowFMAdapter` 都不得在 rollback 后保留“已推进但未被快照记录”的内部状态
- rollback 时，1D 引擎必须恢复到本 `dt_couple` 周期起点的快照状态
- replay 时，1D 引擎必须从该快照状态重新推进到当前时刻；禁止使用“快进但不重放耦合交换”的近似路径
- replay 期间的 exchange buffer 更新顺序必须保持确定性；若并行规约无法保证顺序，则调试/CI 正确性路径必须退回固定顺序规约
- 运行时计数器 `count_*` 在 rollback 后也必须恢复到快照值；replay 期间重新统计，禁止把失败路径与重放路径的计数混加

**软启动恢复（最终增强）**：
- 1D 引擎从熔断状态恢复后，交换通量不得一步跳回目标值
- 采用恢复因子 `gamma in [0,1]`：
  \[
  Q_{actual} = \gamma Q_{target}
  \]
- `gamma` 在 `recovery_steps` 个耦合步内**线性**回升（`gamma_n = n / recovery_steps`，`n = 0, 1, ..., recovery_steps`；默认 10，可在 5~20 范围内调整）。线性是唯一锁定路径；smoothstep 仅作可选诊断模式，禁止用于正确性路径（破坏确定性 replay）
- 若恢复期间再次触发失稳：
  - 自动延长 `cooldown_steps`
  - 重新进入熔断状态

### 旧8.8 黑匣子转储规范

黑匣子 `.crash` 数据必须包含：
- `exchange_state`
- 地形梯度
- RTC 状态
- 局部水力坡降
- 熔断前 5 个时间步的关键交换上下文

### 旧8.9 配置层扩展

```yaml
coupling:
  conflict_strategy: priority_based
  priorities:
    river_link: 1.0
    drainage_node: 0.8
  safe_depth_threshold:
    bank_overtop: 0.01
    lateral_weir: 0.005
    gate_outlet: 0.01
    culvert_link: 0.002
  enable_fault_isolation: true
  cooldown_steps: 5
  retry_policy: conservative
```

### 旧8.10 SWMM 嵌入式 1D-2D 双向耦合（历史展开，排水侧实现示例）

本节只补充 SWMM 嵌入式耦合的排水侧实现示例，不重新定义耦合主规则、交换账本或适配器契约；主规则见第 6 章，接口权威定义见第 7 章。

### 旧8.10.1 架构概述

管网模拟采用 **OWA SWMM 5.2.x** 引擎直接嵌入，通过其 C API 实现 1D 管网与 2D 地表流的双向耦合。

### 旧8.10.2 交换策略

- 主方案：**质量为主**
- 动量接口预留：`drain_momentum_model = none | vertical_jet_alpha | calibrated`
- v5.3 默认：`none`

#### `vertical_jet_alpha`（v5.2 新增）
- 非默认，仅用于实验平台/小尺度精细算例
- 不用于大尺度城市级默认主线
- 经验化卷吸模型：
\[
w_0 = \frac{|Q_{over}|}{A_{inlet}}
\]
\[
\mathbf{S}_{drain,mom}^{\alpha} = \beta_{jet} \cdot w_0 \cdot \frac{|Q_{over}|}{A_{cell}} \cdot \mathbf{d}_{spread}
\]
量纲：
- `w_0` 单位 `[m/s]`（卷吸竖向特征速度）
- `|Q_over|/A_cell` 单位 `[m/s]`
- `β_jet` 显式声明为**无量纲**经验系数
- `S_drain,mom^α` 单位 `[m²/s²]`，与动量方程右端一致

其中：
- `A_inlet`：井口有效面积（显式存于 CouplingNode）
- `beta_jet`：卷吸系数（可由 3x5m PIV 实验标定）
- `d_spread`：
  - 有街道主方向时，沿街道方向优先
  - 否则在星形域内各向同性扩散

数值约束：
- 仅在 `Q_over < 0`（管网向地表溢流）时激活
- 仅作用于耦合节点附近星形域
- 受局部 CFL、`V_max_physical`、`M_local` 三重约束

### 旧8.10.3 交换流态

- 自由堰流
- 淹没堰流（Villemonte 修正）
- 孔口流
- 管网溢流
- smoothstep 连续过渡

### 旧8.10.4 抽吸/注入硬限（历史展开，新主权见 §6.1）

本节只保留排水侧抽吸/注入硬限的历史背景与实现顺序示意；耦合主规则与硬门约束的统一定义见 §6.1。

历史背景：v5.3 之前主线只用几何体积 `hA`，v5.3 升级为物理可用储量 `phi_t h A` 并加 0.9 安全系数。

实现顺序示意（不再具有规范性）：
- 当前权威实现顺序见 §6.1 仲裁规则（包括 V_limit_k 切分、KKT 单步解算、非负回退）
- 本节不再保留独立公式或硬门定义

CUDA 数据类型对齐要求（仍然规范性，因为是实现层硬约束）：
- `Q_actual`, `phi_t`, `h`, `A`, `dt_sub` 一律使用 `Real` (`double`)
- `V_limit` 与 `Q_limit` 必须按 §6.1 的两步定义计算：先以 `Real` 计算 `V_limit`，再以 `Real` 除以 `dt_sub` 得到 `Q_limit`；中间量不得降级为 `float`
- 常数 `0.9` 必须显式写为 `0.9` 的 double 字面量（或 `Real(0.9)`）
- 目的：避免 GPU 上隐式类型降级导致的边界裁剪误差

### 旧8.10.5 质量欠款账户（历史展开，新主权见 §6.3）

本节只补充排水侧 deficit 体积账的历史展开与背景；`mass_deficit_account` 的权威语义、反馈路径机器化要求与 write-off 治理见 §6.3。

历史背景：v5.3.1 引入 deficit 体积账以避免极端工况下耦合质量"凭空消失"，v5.3.2 升级为可 rollback 单标量审计字段。

实现示例（仅作历史参考，不再具规范性）：

\[
V_{target} = Q_{target} \Delta t,\quad V_{applied} = Q_{applied} \Delta t,\quad V_{deficit,new} = V_{deficit,old} + (V_{target} - V_{applied})
\]

权威规则一概以 §6.3 为准；本节不再保留竞争性正文。

---

## legacy.9 CUDA 并行化策略（历史展开，对应新骨架 §12 未来能力）

### 9.1 GPU 内存布局

- STCF Cell-SoA：`phi_t[]`, `phi_xx[]`, `phi_xy[]`, `phi_yy[]`, `drag_theta[]`, `drag_a_parallel[]`, `drag_a_perp[]`, `drag_cd[]`, `manning_n[]`
- **Edge-based ghost data（最初引入于 v5.3.1，现纳入 v5.3.2 规范）**：为 Kernel 2 预打包左右单元的关键只读数据
  - `edge_phi_t_L[]`, `edge_phi_t_R[]`
  - `edge_phi_e_n[]`, `edge_omega[]`
  - `edge_zb_L[]`, `edge_zb_R[]`
  - 以及必要的张量投影辅助量
- SoilParamsLUT：float4 向量化读取
- `q_exch` 双缓冲 + pinned host memory + CUDA Events

### 9.2 Kernel 策略

- 默认：单次 launch + atomicAdd + CUDA Graphs
- 备选：边着色（旧 GPU）
- `__ldg()` / 只读缓存用于 `Phi_c` / STCF 只读字段
- Kernel 2 优先读取 **Edge-based ghost data**，避免 Thread-per-Edge 对 Cell-SoA 的跨步访存退化

---

## legacy.10 预处理链路（历史展开，对应新骨架 §9）

### 10.1 DPM 参数提取

排水管网、建筑轮廓、DSM/DEM、点云 → PreProc 提取：

1. `phi_t`：容积孔隙率
2. `Phi_c`：导流孔隙率张量（默认算法：Ray-casting + R-Tree）
3. `omega_edge`：界面连通性因子
4. `drag_theta`
5. `drag_a_parallel`
6. `drag_a_perp`
7. `drag_cd`
8. `manning_n`
9. SWMM `.inp`
10. `coupling_config.yaml`
11. `validate_dpm_consistency()` 校验

**PreProc 每级强校验（v5.2 新增）：**
- 提取 `phi_t` 后立即校验
- 生成 `Phi_c` 后立即校验
- 生成 `omega_edge` 后立即校验
- 导出 `.stcf.nc` 前总校验
- 任一级失败立即停止，不允许将非法张量传入 GPU

**PreProc 默认/可替换算法（v5.1 锁定）**：
- 默认：Ray-casting + R-Tree 空间索引
- 可替换：PCA 主方向提取、AI 语义增强映射
- 任何替换算法必须输出同一 STCF v5 字段集合，保持下游兼容

---

## legacy.11 子系统分解（历史展开，对应新骨架 §2 / §3）

本章作为新骨架 `2. Phase 1 最小可实现基线` / `3. 系统总架构与模块拓扑` 的历史性展开表，用于支撑模块落位与实现排序；其内容仅作展开说明，不重新定义前部章节已经锁定的模块边界。

| # | 子系统 | 范围 | 关键交付物 | 依赖 |
|---|--------|------|-----------|------|
| S1 | Core 基础设施 | 类型定义、配置、日志、错误处理、CMake 骨架 | `libs/core/` | 无 |
| S2 | MeshLib 网格库 | 三角/四边形混合网格生成或导入、局部加密、统一 cell/edge 拓扑、边列表、排序 | `libs/mesh/` | S1 |
| S3 | STCFLib / DPM 数据层 | STCF v5 schema、NetCDF IO、DPM 参数、Consistency 校验 | `libs/stcf/` | S1,S2 |
| S4 | Surface2DCore CPU 参考 | DPM-HLLC、Hydrostatic Reconstruction、Well-Balanced、**S_phi_t 补偿项离散**、干湿处理 | `libs/surface2d/backends/cpu/` | S1,S2,S3 |
| S5 | Surface2DCore 源项 | 降雨、Green-Ampt、Manning、**方向性 Drag 张量** | `libs/surface2d/source_terms/` | S1,S2,S3,S4 |
| S6 | CouplingLib | SWMM5 封装、D-Flow FM 适配、双向交换、零动量注入接口、**vertical_jet_alpha** | `libs/coupling/` | S1,S2,S3,S4 |
| S7 | Surface2DCore CUDA Backend | CFL kernel、Edge flux kernel、Cell update kernel、CUDA Graphs | `libs/surface2d/backends/cuda/` | S4,S5,S6 |
| S8 | PreProc | DPM 参数提取、STCF v5 生成、SWMM .inp 输出 | `python/scau_preproc/` | S2,S3 |
| S9 | AI 校核 | PINN + 贝叶斯优化 | `python/scau_ai/` | S3,S8 |
| S10 | SimDriver + 集成 | 主程序、配置、运行时更新 API、回归测试 | `apps/` | S1-S9 |

---

## legacy.12 实施路线图（历史展开，对应新骨架 §2）

本章是 `## 2. Phase 1 最小可实现基线` 的 Phase 1 内部里程碑（M1–M5）级实施排序，沿用历史 "Phase 1–5" 命名以保留追溯。下文 "### Phase N" 标题应读作 "M_N"，与前部骨架 §2 中的 release 级 Phase 1/2/3 不是同一概念。release 级 Phase 1/2/3 的 capability 与 roadmap 以前部骨架 §2 为准；若本章里程碑边界与前部骨架冲突，以前部为准。

### Phase 1
- 构建链验证（CMake + CUDA + SWMM + pybind11）
- MeshLib 原型（三角/四边形混合网格生成或导入、统一 cell/edge 拓扑）

### Phase 2
- STCF v5 / DPM 数据结构
- CPU 版 DPM-HLLC + S_phi_t 补偿项离散 + 静水平衡验证

### Phase 3
- 源项（Manning + Drag + Infiltration）
- SWMM 耦合（质量主导）

### Phase 4
- CUDA Backend：CFL + Edge + Cell 三核结构
- CUDA Graphs / atomicAdd benchmark
- 若 benchmark 证明 Kernel 3 寄存器压力过大，则允许拆分 `K3a/K3b`

### Phase 5
- PreProc 完整链路
- AI 双阶校核
- 城市真实算例验证

---

## legacy.13 验证算例矩阵（历史展开，权威定义已搬移到 §10）

本章是新骨架 `## 10. 验证矩阵与正确性基线` 的历史展开，仅保留测试语义、覆盖面与正确性基线的细化说明；本节不重复定义执行后果，相关处置口径以前部骨架与配套稳定性协议为准。

- 静水平衡（含 `phi_t` 梯度）
- **带孔隙率梯度的静水台阶 GoldenTest**
  - 判据：`max|eta(t)-eta(0)| <= 1e-12`
  - 判据：`max(max_i |u_i|, max_i |v_i|) <= u_hydro_tol`
  - 默认：`u_hydro_tol = 1e-12 m/s`
  - 上述判据必须在固定顺序规约的确定性参考路径上计算
  - 若任一超限：视为不满足该 GoldenTest 通过判据，并纳入相应审计/判定对象；具体分级与处置见配套稳定性协议相应章节
- `phi_t` 跳变静水
- `Phi_c` / `phi_{e,n}` 跳变零速稳态
- 极窄缝隙堵塞 (`phi_{e,n} -> 0`)
- DPM 拖曳阻力衰减
- `Phi_c` 正定性异常拦截测试
- STCF v4 -> v5 迁移测试
- SWMM 单管 / T 形 / 超载溢流
- 喷涌动量 Alpha（仅实验平台/小尺度）
- GPU vs CPU 一致性
- 真实城区（含真实管网）

**GoldenSuite 物理不变式矩阵**：权威定义已搬移到前部 §10.2；本节仅保留历史背景。

**v5.3.2 确定性正确性路径**：
- GoldenTest 必须提供一条固定顺序规约的参考路径
- CPU 参考求解器默认使用 `double`
- GPU 正确性对照路径在调试/CI 模式下必须锁定：
  - `double` 精度
  - 固定顺序 reduction 或等价确定性规约
  - 禁止用不确定原子加结果直接作为静水平衡唯一判据
- 性能路径可以保留原子加 / CUDA Graphs，但其结果必须回归对照确定性参考路径

GoldenTest / GoldenSuite 的准入判定与相关处置语义见配套稳定性协议相应章节。
本节只保留 GoldenTest / GoldenSuite 的语义地位与确定性正确性基线，不重复定义执行后果。

---

## legacy.14 运行时功能需求（历史展开，对应新骨架 §8 / §10 / §11）

本章作为新骨架 `8/10/11` 的历史展开补充，仅保留运行时功能语义、诊断项与输出要求；升级判定、处置后果与治理语义以新骨架及配套协议为准，本章不再重复定义。

- 检查点 + 热重启
- 降雨输入（CSV / NC）
- 输出控制
- 边界条件配置

### 14.1 在线诊断计数器（v5.3 强化）

运行时必须统计并输出以下计数器：
- `count_velocity_clamp`
- `count_negative_depth_fix`
- `count_flux_cutoff`
- `count_drain_split`
- `count_drag_matrix_degenerate`
- `count_ai_prior_conflict`
- `count_mem_pressure_events`
- `max_cell_cfl`（并输出空间分布, 用于定位微小孔隙率导致的虚假波速）

验收语义：
- `count_velocity_clamp / total_steps > 1%` 表征数值主离散存在风险，并应视为审计对象；具体分级与处置见稳定性协议
- `count_negative_depth_fix > 0` 表征可信性被破坏，并应视为异常事件；具体分级与处置见稳定性协议
- `count_drag_matrix_degenerate` 高频出现时，应回查 PreProc 的 `A_w` 与 `Phi_c`
- `count_ai_prior_conflict` 高频出现时，表明 AI 校核与底层几何参数存在冲突
- 支持导出 `.json` 报告，并在后处理中生成空间热力图

---

## legacy.15 软件工程约束（历史展开，对应新骨架 §3 / §7 / §8）

本章作为新骨架 `3/7/8` 的历史展开补充，仅保留模块边界、所有权、配置、可测试性与工程实现约束；治理升级、责任归属与流程后果以新骨架及配套协议为准，本章不再重复定义。

### 15.1 模块边界
- 所有二维地表 GPU kernels 归属 `libs/surface2d/backends/cuda/`，并复用 `Surface2DCore` 的状态、通量、源项、CFL 与诊断契约。
- `libs/surface2d/source_terms/` 与 `libs/surface2d/backends/cpu/` 提供 CPU 参考函数与确定性正确性路径；`libs/coupling/` 仅提供耦合语义、引擎适配与账本接口。

### 15.2 所有权模型
- SimDriver 拥有 host 状态
- cuda::Memory 拥有 device 状态
- `q_exch` 使用 A/B 双缓冲

### 15.3 同步原语
- pinned host memory
- CUDA Events
- 异步流水线 CPU(SWMM) / GPU(FVM)

### 15.4 错误处理
- 区分致命异常、一般异常与告警类诊断对象
- SwmmError
- NetCDF schema_version 检查

### 15.5 配置管理
- 单一入口：`simulation.yaml`
- 启动即全量校验

### 15.6 可测试性
- `ISwmmEngine` + `MockSwmmEngine`
- 便于 CouplingManager 单元测试

### 15.7 Schema 版本控制
- `.stcf.nc` 包含 `schema_version = 5`
- 提供迁移工具：`python/scau_preproc/stcf_upgrade_tool.py`
  - 旧字段 `phi_s` -> `phi_t`
  - 旧字段 `psi_tensor` -> `Phi_c` 映射
  - 缺失的 `omega_edge` / 旧标量字段 `a_w` / `drag_cd` 采用默认规则补齐
  - 迁移后：旧 `a_w` 自动映射为 `drag_a_parallel = drag_a_perp = a_w`（各向同性退化）
- 迁移工具必须输出变更日志；有关人工复核与证据留痕要求，见配套稳定性协议相应章节

### 15.8 检查点原子写入
- `tmp -> fsync -> rename`

### 15.9 构建链验证
- Phase 1 第一优先级
- Windows + Linux 双平台验证
- SWMM 集成失败不阻塞：由 `MockSwmmEngine` 保底推进主线

### 15.10 CI GPU 执行模型（最终锁定）
- 采用 **混合模式**：
  - 本地 GPU Runner：执行主线 GoldenTest、CUDA 回归、关键正确性测试
  - 云端 GPU 实例：执行大算例、长时段性能基准、偶发高负载验证
- 原则：
  - 主线正确性测试必须在本地 Runner 上可重复执行；具体阻断规则见稳定性协议
  - 云端结果用于性能评估与补充验证，不作为唯一正确性闸门

---

## legacy.16 精度控制规范（历史展开，权威定义已搬移到 §11）

本章作为新骨架 `11` 的历史展开补充，仅保留默认参数、单位与适用范围；CI 判定、运行时执行后果与升级处置以新骨架及配套协议为准，本章不再重复定义。

**默认参数表**：权威定义已搬移到前部 §11；本节仅保留历史适用说明。

具体参数请查 §11.1 ~ §11.4。

---

## 附录 A: GPU 内存预算

百万级网格估算约 `~700 MB`，单卡 A100/H100 充足，消费级 RTX 可运行但需混合精度优化。
