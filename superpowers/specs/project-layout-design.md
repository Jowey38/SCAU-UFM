# SCAU-UFM 项目目录结构设计

> **For agentic workers:** 本文档是 SCAU-UFM 规范性入口之一，定义“二维 Anisotropic DPM HLLC 地表模型 + 一维 SWMM 管网模型 + 一维 D-Flow FM 河网模型”全耦合架构下的目标项目目录结构、目录职责边界、Phase 演进落位、验证证据落位与第三方治理落位。

**主权边界：** 本文档只定义 target-state 工程目录结构与目录职责。物理语义、接口契约、系统不变式和耦合核心语义仍以主 Spec 为准；gate、触发阈值、执行后果和证据要求仍以稳定性协议为准；machine-facing 名称、单位和退役别名仍以符号表为准。若历史 review/status/plan/design 文档与本文冲突，以本文为准。若主 Spec 中旧 `libs/solver/`、`libs/source/`、`libs/cuda/` 目录表达尚未同步，以本文的 `libs/surface2d/` 目标布局为准。

**目标：** 为 SCAU-UFM 固化一套能够真实反映二维地表数值内核、SWMM + D-Flow FM 并列嵌入、CouplingLib 分层、第三方版本治理、验证证据链与后续 Phase 演进边界的项目目录结构。

**架构：** 目录结构必须服务于架构边界，而不是单纯整理文件夹。本设计将“二维地表主模型”“1D-2D 耦合协议”“SWMM 适配”“D-Flow FM 适配”“第三方源码”“第三方版本治理”“构建接入”“验证体系”八类职责彻底分开，同时确保 Anisotropic DPM、HLLC、D-Flow FM 在架构图、目录树、耦合章节和测试体系中都有明确落位。

**技术栈：** CMake / vcpkg、C++ 模型核心、CUDA 性能后端、Python 工具链、嵌入式 SWMM、嵌入式 D-Flow FM / BMI bridge、Markdown 规格书体系。

---

## 1. 设计目标

本目录结构设计要同时解决以下问题：

1. 当前 `SCAU-UFM 全局架构设计规格书` 的 `## 3. 项目目录结构` 没有明确体现 `D-Flow FM` 的嵌入落位。
2. 当前目录结构没有给第三方版本锁定、补丁治理、兼容性治理留出明确位置。
3. `CouplingLib` 的核心协议、SWMM 适配器、D-Flow FM 适配器需要明确分层，避免目录结构继续模糊职责。
4. 二维地表模型本体需要从泛化 `solver` 中显式独立，体现 `Anisotropic DPM`、`HLLC`、三角/四边形混合非结构化网格、源项、润湿干涸、时间推进、CFL、CPU/CUDA 后端等研发核心。
5. 目录树必须与第 2 章架构图、第 8 章双引擎耦合章节、数值方法章节和测试验证章节形成闭环，而不是各说各话。
6. 目录设计必须能区分：
   - Phase 1 必须真实落地的模块
   - Phase 1 先接口占位、后续逐步接通的模块
   - 更远期扩展能力的自然生长位置

---

## 2. 方案选择

本轮推荐采用：**分层嵌入式 + 数值内核显式化目录结构**。

理由如下：

- 保留当前已经成熟的 `CouplingLib core/drainage/river` 架构语言。
- 明确将 `SWMM` 与 `D-Flow FM` 作为并列 1D 嵌入式引擎对待。
- 将二维地表模型提升为 `libs/surface2d/` 一级核心库，避免 `solver` 语义过泛。
- 将非结构化网格能力明确落位到 `libs/mesh/`，支持三角网与四边形混合单元、统一边拓扑和单元几何量。
- 在目录层显式表达 `Anisotropic DPM`、`HLLC`、源项、润湿干涸、时间推进与 CPU/CUDA 后端。
- 将第三方源码、第三方版本治理、第三方补丁治理、第三方构建接入分层拆开。
- 既能保持当前 Spec 的连续性，又能提升数值模型、工程实现和评审表达能力。

不推荐采用：

- 只做最小增量补丁式目录修补：表达力不足，二维地表数值内核仍不清晰。
- 完全按“二维地表 / SWMM / D-Flow FM / 耦合器 / 数据工具”重构顶层目录：会破坏现有 `CouplingLib` 叙事，也容易把第三方适配逻辑和我方核心逻辑混在一起。

---

## 3. 推荐的最终目录树

### 3.1 顶层目录树

```text
SCAU-UFM/
├── CMakeLists.txt
├── vcpkg.json
├── README.md
│
├── cmake/
│   ├── toolchains/
│   ├── modules/
│   └── third_party/
│       ├── swmm.cmake
│       ├── dflowfm.cmake
│       └── bmi_bridge.cmake
│
├── libs/
│   ├── core/
│   │   ├── include/core/
│   │   └── src/
│   │
│   ├── mesh/
│   │   ├── include/mesh/
│   │   │   ├── node.hpp
│   │   │   ├── cell.hpp
│   │   │   ├── edge.hpp
│   │   │   ├── mixed_topology.hpp
│   │   │   ├── geometry.hpp
│   │   │   ├── quality.hpp
│   │   │   └── partition.hpp
│   │   └── src/
│   │
│   ├── stcf/
│   │   ├── include/stcf/
│   │   │   ├── schema.hpp
│   │   │   ├── io_netcdf.hpp
│   │   │   ├── dpm_consistency.hpp
│   │   │   ├── tensor.hpp
│   │   │   ├── chebyshev.hpp
│   │   │   ├── soil_params.hpp
│   │   │   ├── landcover.hpp
│   │   │   └── roughness.hpp
│   │   └── src/
│   │
│   ├── surface2d/
│   │   ├── include/surface2d/
│   │   │   ├── state/
│   │   │   ├── geometry/
│   │   │   ├── boundary/
│   │   │   ├── dpm/
│   │   │   │   ├── anisotropic_porosity.hpp
│   │   │   │   ├── storage_exchange.hpp
│   │   │   │   ├── tensor_projection.hpp
│   │   │   │   └── closure_laws.hpp
│   │   │   ├── riemann/
│   │   │   │   ├── hllc.hpp
│   │   │   │   ├── wave_speeds.hpp
│   │   │   │   └── positivity_preserving.hpp
│   │   │   ├── flux/
│   │   │   ├── source_terms/
│   │   │   │   ├── bed_slope.hpp
│   │   │   │   ├── friction.hpp
│   │   │   │   ├── rainfall.hpp
│   │   │   │   ├── infiltration.hpp
│   │   │   │   └── drainage_exchange.hpp
│   │   │   ├── wetting_drying/
│   │   │   ├── time_integration/
│   │   │   ├── cfl/
│   │   │   └── diagnostics/
│   │   ├── src/
│   │   └── backends/
│   │       ├── cpu/
│   │       └── cuda/
│   │
│   ├── coupling/
│   │   ├── core/
│   │   │   ├── exchange/
│   │   │   ├── arbitration/
│   │   │   ├── conservation/
│   │   │   ├── scheduler/
│   │   │   ├── mapping/
│   │   │   ├── deficit/
│   │   │   ├── snapshot/
│   │   │   ├── rollback/
│   │   │   └── fault/
│   │   │
│   │   ├── drainage/
│   │   │   ├── engine_interface/
│   │   │   ├── swmm_adapter/
│   │   │   ├── inlet_mapping/
│   │   │   ├── manhole_exchange/
│   │   │   └── mock/
│   │   │
│   │   └── river/
│   │       ├── engine_interface/
│   │       ├── bmi_interface/
│   │       ├── dflowfm_adapter/
│   │       ├── river_exchange/
│   │       ├── boundary_mapping/
│   │       ├── hydraulic_structures/
│   │       └── mock/
│   │
│   └── io/
│       ├── config/
│       ├── results/
│       ├── checkpoints/
│       └── telemetry/
│
├── extern/
│   ├── swmm5/
│   ├── dflowfm/
│   └── dflowfm-bmi/
│
├── third_party/
│   ├── manifest/
│   │   ├── swmm5.version
│   │   ├── dflowfm.version
│   │   └── dflowfm-bmi.version
│   ├── patches/
│   │   ├── swmm5/
│   │   ├── dflowfm/
│   │   └── dflowfm-bmi/
│   ├── licenses/
│   │   ├── SWMM_LICENSE.txt
│   │   ├── DFLOWFM_LICENSE.txt
│   │   └── DFLOWFM_BMI_LICENSE.txt
│   └── compatibility/
│       ├── matrix.md
│       ├── upgrade-policy.md
│       └── abi-boundary-policy.md
│
├── apps/
│   ├── sim_driver/
│   ├── preproc_cli/
│   ├── postproc_cli/
│   ├── diagnostics/
│   └── benchmark/
│
├── python/
│   ├── scau_preproc/
│   ├── scau_postproc/
│   ├── scau_validation/
│   └── scau_ai/
│
├── configs/
│   ├── runtime/
│   ├── surface2d/
│   ├── coupling/
│   ├── swmm/
│   ├── dflowfm/
│   └── third_party/
│
├── tests/
│   ├── unit/
│   │   ├── surface2d/
│   │   ├── coupling/
│   │   └── io/
│   ├── numerical/
│   │   ├── mixed_mesh_geometry/
│   │   ├── mesh_quality/
│   │   ├── riemann_hllc/
│   │   ├── anisotropic_dpm/
│   │   ├── wetting_drying/
│   │   ├── source_terms/
│   │   └── conservation/
│   ├── integration/
│   │   ├── surface_swmm/
│   │   ├── surface_dflowfm/
│   │   └── full_coupled/
│   ├── golden/
│   │   ├── suite_manifest/
│   │   ├── reference/
│   │   └── evidence/
│   ├── fault_replay/
│   ├── regression/
│   ├── compatibility/
│   └── performance/
│
├── examples/
│   ├── minimal_surface2d/
│   ├── swmm_coupling/
│   ├── dflowfm_coupling/
│   └── full_coupled_city/
│
├── benchmarks/
│   ├── surface2d/
│   ├── coupling/
│   └── gpu/
│
└── docs/
    ├── architecture/
    ├── specs/
    ├── plans/
    ├── protocols/
    ├── numerical_methods/
    └── third_party/
```

---

## 4. 关键目录的唯一职责

### 4.1 `libs/mesh/`

**唯一职责：** 承载二维地表模型所需的三角/四边形混合非结构化网格拓扑、几何量和网格质量约束。

应包含：

- `node`、`cell`、`edge` 的统一拓扑表达。
- 三角形与四边形混合单元类型、节点顺序、边顺序和邻接关系。
- 单元面积、中心点、边长、边法向、局部坐标和边界标记。
- 混合网格导入、局部加密后的拓扑重建、分区边界与 halo 元数据。
- 网格质量检查，包括负面积、重复边、非流形邻接、退化四边形和法向一致性。

不应包含：

- Anisotropic DPM 的物理闭合。
- HLLC 通量、源项离散或时间推进逻辑。
- SWMM、D-Flow FM 或 CouplingLib 的交换决策。
- STCF 字段的 NetCDF I/O 本体。

### 4.2 `libs/surface2d/`

**唯一职责：** 承载二维地表主模型的数值状态、通量、源项、时间推进和计算后端。

应包含：

- 二维浅水 / DPM 状态变量定义。
- 消费 `libs/mesh/` 提供的三角/四边形混合网格几何、边拓扑、边界条件与单元邻接访问。
- Anisotropic DPM 的孔隙率张量、储水交换、张量投影和闭合关系。
- HLLC 黎曼求解器、波速估计、正性保持和通量计算。
- 床面坡降、摩阻、降雨、入渗、管网交换等源项。
- 润湿干涸、CFL 控制、时间推进和诊断指标。
- CPU 与 CUDA 后端实现。

不应包含：

- 网格生成器、网格导入器或混合单元拓扑重建逻辑。
- SWMM 原生 API。
- D-Flow FM / BMI 原生 API。
- 第三方引擎生命周期管理。
- 耦合仲裁策略和共享流量池。

### 4.3 `libs/surface2d/dpm/`

**唯一职责：** 表达各向异性双孔隙率模型的物理闭合与数值投影。

应包含：

- `anisotropic_porosity`：各向异性孔隙率张量与方向性可通行性。
- `storage_exchange`：主孔隙与次孔隙之间的储水交换。
- `tensor_projection`：张量参数向单元边、通量方向和局部坐标的投影。
- `closure_laws`：DPM 闭合关系与参数约束。

不应包含：

- HLLC 具体波速公式。
- SWMM / D-Flow FM 交换点映射。
- 后端专用 CUDA kernel。

### 4.4 `libs/surface2d/riemann/`

**唯一职责：** 承载二维地表模型的 HLLC 黎曼求解器及其数值稳定性约束。

应包含：

- `hllc`：HLLC 通量求解主逻辑。
- `wave_speeds`：左右波速、接触波速和方向投影波速估计。
- `positivity_preserving`：水深非负、干湿界面稳定和极小水深处理。

不应包含：

- 降雨、入渗、摩阻等源项。
- 耦合器的流量仲裁。
- 第三方 1D 引擎适配逻辑。

### 4.5 `libs/surface2d/source_terms/`

**唯一职责：** 承载二维地表控制方程的源项离散。

应包含：

- `bed_slope`：床面坡降源项。
- `friction`：Manning / Chezy 等摩阻源项。
- `rainfall`：降雨输入源项。
- `infiltration`：入渗损失源项。
- `drainage_exchange`：来自耦合层决策后的井口、雨水口、河网边界交换源项。

不应包含：

- 直接调用 SWMM 或 D-Flow FM。
- 决定哪个 1D 引擎优先取水。
- 第三方引擎错误处理。

### 4.6 `libs/surface2d/backends/`

**唯一职责：** 承载二维地表模型的计算后端实现。

应包含：

- `cpu/`：参考实现、单元测试友好的标量或多线程实现。
- `cuda/`：GPU kernel、device 数据布局、host-device 调度和性能优化。

边界约束：

- CPU 与 CUDA 后端共享 `include/surface2d/` 下的 C++ 公共数值语义。
- 后端目录不得定义新的物理模型语义。
- CUDA 后端不得绕过 `coupling/core/` 直接访问 SWMM 或 D-Flow FM。

### 4.7 `libs/coupling/core/`

**唯一职责：** 定义并执行 1D-2D 全耦合的公共语义。

应包含：

- `ExchangeRequest`
- `ExchangeDecision`
- `EngineReport`
- 共享流量池
- 优先级仲裁
- 全耦合时间调度
- 1D-2D 空间映射公共契约
- 质量守恒检查
- `mass_deficit_account`
- `exchange_buffer`
- snapshot / rollback / replay 的公共契约
- `FaultController` 的耦合侧状态管理

不应包含：

- SWMM 专有 API。
- D-Flow FM / BMI 专有 API。
- 任一引擎的内部数据结构。
- 二维 HLLC 通量求解细节。

ABI / headers firewall：

- 公共头文件不得包含 `extern/swmm5/`、`extern/dflowfm/`、`extern/dflowfm-bmi/` 下的第三方头。
- 公共 DTO 不得携带第三方句柄、结构体、枚举或依赖第三方内存布局的字段。
- 第三方错误码、异常与生命周期必须在 `drainage/` 或 `river/` 内转换为 core 可识别的项目内状态与 DTO。

### 4.8 `libs/coupling/drainage/`

**唯一职责：** 承接城市排水 1D 引擎（SWMM）接入。

应包含：

- `ISwmmEngine`
- `SWMMAdapter`
- `MockSwmmEngine`
- 雨水口、检查井、节点到地表单元的映射逻辑
- 管网与地表之间的交换流量执行逻辑

不应包含：

- 共享流量池。
- 通用 deficit 账本。
- D-Flow FM 相关逻辑。
- 将 SWMM 原生类型暴露给 `libs/coupling/core/` 或 `libs/surface2d/`。

### 4.9 `libs/coupling/river/`

**唯一职责：** 承接河网 / 水工建筑物 1D 引擎（D-Flow FM）接入。

应包含：

- `IDFlowFMEngine`
- `DFlowFMAdapter`
- `MockDFlowFMEngine`
- `RiverExchangePoint`
- `bmi_interface/` 下的我方 BMI 抽象适配层
- 河网断面、堤防、闸门、涵洞、泵站、堰等交换映射

不应包含：

- SWMM 节点逻辑。
- 通用仲裁规则。
- 2D 核心数值更新。
- 将 D-Flow FM / BMI 原生类型暴露给 `libs/coupling/core/` 或 `libs/surface2d/`。
- 将 `extern/dflowfm-bmi/` 第三方源码复制进我方适配层。

### 4.10 `libs/stcf/`

**唯一职责：** 承载空间-时间耦合场（STCF）的数据结构、参数一致性和 I/O。

应包含：

- STCF schema。
- NetCDF / 栅格 / 矢量数据读写接口。
- DPM 参数一致性检查。
- 各向异性张量、Chebyshev 参数、土壤参数、下垫面和糙率表达。

不应包含：

- HLLC 通量求解。
- 1D 引擎适配。
- 仿真调度主循环。

### 4.11 `libs/io/`

**唯一职责：** 承载项目内配置、结果、检查点和遥测 I/O 的公共实现。

应包含：

- 配置解析。
- 结果输出。
- checkpoint 读写。
- 诊断和 telemetry 输出。

不应包含：

- 数值格式本体。
- SWMM 或 D-Flow FM vendor 源码。
- 第三方版本治理文件。

### 4.12 `extern/swmm5/`

**唯一职责：** 存放嵌入式 SWMM 第三方本体。

### 4.13 `extern/dflowfm/`

**唯一职责：** 存放嵌入式 D-Flow FM 第三方本体。

### 4.14 `extern/dflowfm-bmi/`

**唯一职责：** 存放 Deltares / 上游来源的 D-Flow FM BMI bridge 第三方 vendor 快照。

边界约束：

- `extern/dflowfm-bmi/` 不是我方 BMI 抽象层，不承载 `IDFlowFMEngine`、`DFlowFMAdapter` 或项目内 DTO。
- 我方 BMI 抽象与适配代码只能落在 `libs/coupling/river/bmi_interface/` 与 `libs/coupling/river/dflowfm_adapter/`。
- 两者通过 `cmake/third_party/bmi_bridge.cmake` 接入；禁止互相复制源码。

### 4.15 `third_party/manifest/`

**唯一职责：** 记录第三方版本锁定事实。

### 4.16 `third_party/patches/`

**唯一职责：** 记录我们对第三方做过的修改。

### 4.17 `third_party/compatibility/`

**唯一职责：** 记录第三方版本兼容性结论、升级策略和 ABI 边界政策。

### 4.18 `cmake/third_party/`

**唯一职责：** 定义第三方如何被构建系统接入。

### 4.19 `tests/numerical/`

**唯一职责：** 验证二维地表数值格式与物理源项的正确性。

应包含：

- 混合网格几何量、边法向、单元面积、邻接关系和边遍历一致性验证。
- HLLC 黎曼问题。
- Anisotropic DPM 参数投影与储水交换验证。
- 润湿干涸稳定性验证。
- 源项平衡验证。
- 质量守恒验证。

### 4.20 `tests/golden/`

**唯一职责：** 保存 GoldenSuite / GoldenTest 的门禁级正确性基准、参考结果与可复现实证。

子目录职责：

- `tests/golden/suite_manifest/`：记录 GoldenSuite 组成、GoldenTest ID、适用 Phase、依赖参考数据与 CI gate 绑定关系。manifest 最小字段集为 `{test_id, name, test_path, applicable_phase, reference_path, ci_gate}`；`test_id` 必须与主 Spec §10.2 中的 `G_n` 一致；`test_path` 必须形如 `tests/golden/<test_name>/`，与 `G_n` 同名约束（如 `G1` → `tests/golden/hydrostatic_step/`）。
- `tests/golden/reference/`：保存可复现的参考结果、输入快照、容差声明和基准版本标识。
- `tests/golden/evidence/`：保存每次 gate 执行的结果、日志、差异摘要、质量守恒证据和发布/合并判定材料。

### 4.21 `tests/fault_replay/`

**唯一职责：** 验证 fault handling、snapshot、rollback、replay 与 `mass_deficit_account` 的一致性证据链。

应覆盖：

- deficit 账滚动、清偿与核销。
- `epsilon_deficit` 在正确性路径与性能路径中的约束差异。
- rollback 后从 snapshot replay 的确定性一致性。
- `ABORT`、`REVIEW_REQUIRED`、`BLOCK_MERGE`、`BLOCK_RELEASE` 的故障证据落位。

### 4.22 `tests/compatibility/`

**唯一职责：** 验证第三方升级后系统边界没有被破坏。

### 4.23 `configs/third_party/`

**唯一职责：** 表达第三方启用方式与路径配置。

---

## 5. 第三方嵌入与版本锁定原则

### 5.1 嵌入落位原则

嵌入式第三方必须有明确目录落位：

- `SWMM` → `extern/swmm5/`
- `D-Flow FM` → `extern/dflowfm/`
- `D-Flow FM BMI bridge` → `extern/dflowfm-bmi/`

### 5.2 版本声明原则

每个第三方组件必须有独立版本声明文件：

- `third_party/manifest/swmm5.version`
- `third_party/manifest/dflowfm.version`
- `third_party/manifest/dflowfm-bmi.version`

### 5.3 版本声明最小字段

每个 `.version` 文件至少应包含：

- 版本号。
- 来源（官方包 / fork / vendorized snapshot）。
- 获取方式。
- 校验方式（checksum / release tag）。
- 是否应用 patch。
- 与当前 `SimDriver / Adapter / CMake` 的兼容性备注。

### 5.4 升级治理原则

任一第三方版本变化，都必须补交：

- GoldenSuite 回归结果。
- adapter 接口兼容性验证。
- snapshot / rollback / replay 一致性验证。
- HLLC / DPM 数值回归结果。
- 全耦合质量守恒验证结果。
- 兼容矩阵更新。

---

## 6. Phase 1 的目录落位要求

**主权边界：** 本章是 主 Spec §2.1 中 Phase 1 必交付 capability 在工程目录上的反映。Phase 1/2/3 的 capability 与 release roadmap 以 主 Spec §2 为准；本章只承担 capability → 目录路径的落位映射。若本章目录列表与 主 Spec §2 的 capability 表存在冲突，以 主 Spec §2 为准。

### 6.1 Phase 1 必须有最小实现的目录

反映 主 Spec §2.1 中 Phase 1 必交付 capability：

- `libs/core/`
- `libs/mesh/`
- `libs/stcf/`
- `libs/surface2d/state/`
- `libs/surface2d/dpm/`
- `libs/surface2d/riemann/`
- `libs/surface2d/source_terms/`
- `libs/surface2d/time_integration/`
- `libs/surface2d/backends/cpu/`
- `libs/coupling/core/`
- `libs/coupling/drainage/`
- `libs/io/`
- `apps/sim_driver/`
- `configs/runtime/`
- `configs/surface2d/`
- `configs/coupling/`
- `tests/numerical/`
- `tests/golden/`
- `tests/golden/suite_manifest/`
- `tests/golden/reference/`
- `tests/golden/evidence/`

### 6.2 Phase 1 必须显式存在、但可先最小占位的目录

承接 主 Spec §2.2 Phase 2 capability 的目录骨架，Phase 1 必须显式占位、但允许内容为空：

- `libs/surface2d/backends/cuda/`
- `libs/coupling/river/`
- `extern/dflowfm/`
- `extern/dflowfm-bmi/`
- `cmake/third_party/`
- `third_party/manifest/`
- `tests/fault_replay/`
- `tests/compatibility/`
- `tests/performance/`

### 6.3 Phase 1 可暂时保留未来扩展语义的目录

承接 主 Spec §2.2 Phase 3 capability 的目录骨架，Phase 1 不要求占位，但目录命名一旦使用就必须遵循本设计：

- `python/scau_ai/`
- `apps/diagnostics/`
- `apps/benchmark/`
- `examples/dflowfm_coupling/`
- `benchmarks/gpu/`

关键约束：

**即使 Phase 1 尚未完成 D-Flow FM 的完整联调，目录结构中也必须提前体现其并列 1D 引擎地位。即使 Phase 1 尚未完成 CUDA 主执行路径，目录结构中也必须体现 CPU/CUDA 后端并列演进边界。**

---

## 7. 与主 Spec 其他章节的联动要求

### 7.1 与第 2 章架构图联动

目录结构必须回映到系统总架构图中的一级模块：

- `MeshLib` → `libs/mesh/`
- `Surface2DCore` → `libs/surface2d/`
- `Anisotropic DPM` → `libs/surface2d/dpm/` 与 `libs/stcf/`
- `HLLC Riemann Solver` → `libs/surface2d/riemann/`
- `CPU Backend` → `libs/surface2d/backends/cpu/`
- `CUDA Backend` → `libs/surface2d/backends/cuda/`
- `CouplingLib` → `libs/coupling/core/`, `libs/coupling/drainage/`, `libs/coupling/river/`
- `STCFLib` → `libs/stcf/` 与 `python/scau_preproc/`
- 第三方引擎 → `extern/swmm5/`, `extern/dflowfm/`, `extern/dflowfm-bmi/`

### 7.2 与第 8 章双引擎耦合联动

第 8 章必须显式回指以下目录：

- `libs/surface2d/source_terms/drainage_exchange.hpp`
- `libs/coupling/core/`
- `libs/coupling/core/scheduler/`
- `libs/coupling/core/conservation/`
- `libs/coupling/core/mapping/`
- `libs/coupling/drainage/`
- `libs/coupling/river/`
- `extern/swmm5/`
- `extern/dflowfm/`
- `extern/dflowfm-bmi/`
- `third_party/manifest/`

### 7.3 与数值方法章节联动

数值方法章节必须显式回指以下目录：

- `libs/surface2d/dpm/`
- `libs/surface2d/riemann/`
- `libs/surface2d/source_terms/`
- `libs/surface2d/wetting_drying/`
- `libs/surface2d/time_integration/`
- `libs/surface2d/cfl/`
- `tests/numerical/`

### 7.4 最小三向映射表要求

主 Spec 中应增加一张映射表：

| 架构概念 | 目录落位 | 说明 |
|---|---|---|
| `MeshLib` | `libs/mesh/` | 三角/四边形混合非结构化网格拓扑、几何量与质量检查 |
| `Surface2DCore` | `libs/surface2d/` | 二维地表主模型 |
| `Anisotropic DPM` | `libs/surface2d/dpm/` | 各向异性双孔隙率模型闭合与投影 |
| `HLLC Riemann Solver` | `libs/surface2d/riemann/` | HLLC 通量、波速估计和正性保持 |
| `Source Terms` | `libs/surface2d/source_terms/` | 坡降、摩阻、降雨、入渗和耦合交换源项 |
| `CPU Backend` | `libs/surface2d/backends/cpu/` | CPU 参考或生产后端 |
| `CUDA Backend` | `libs/surface2d/backends/cuda/` | GPU 加速后端 |
| `CouplingLib core` | `libs/coupling/core/` | 共享流量池、仲裁、调度、映射、守恒、回滚契约 |
| `SWMMAdapter` | `libs/coupling/drainage/` | 城市排水 1D 引擎适配 |
| `DFlowFMAdapter` | `libs/coupling/river/` | 河网 / 水工建筑物 1D 引擎适配 |
| `SWMM vendor` | `extern/swmm5/` | 第三方本体 |
| `D-Flow FM vendor` | `extern/dflowfm/` | 第三方本体 |
| `BMI bridge` | `extern/dflowfm-bmi/` | 第三方桥接层 |
| 第三方版本治理 | `third_party/manifest/` | 版本锁定与来源管理 |
| 第三方补丁治理 | `third_party/patches/` | 外部代码 patch 管理 |
| 第三方构建接入 | `cmake/third_party/` | 第三方构建发现与集成 |
| 数值验证 | `tests/numerical/` | HLLC、DPM、源项、守恒等数值验证 |
| 耦合集成验证 | `tests/integration/` | Surface-SWMM、Surface-DFlowFM、Full Coupled 验证 |

### 7.5 强制联动原则

主 Spec 应增加以下原则性约束：

> 凡在系统总架构图中出现的一级模块，都必须能在项目目录结构中找到明确落位；凡在项目目录结构中出现的关键数值、耦合与第三方目录，也必须能在系统总架构、数值方法章节或耦合章节中找到角色定义。

### 7.6 Machine-facing 命名联动原则

目录职责不得引入与符号表冲突的机器可读字段、schema 名称、日志字段或 CI 字段。

强制约束：

- `Q_limit` 是耦合子步流量硬门上界的唯一 machine-facing 名称；不得在 audit、CI、log、schema 或机器可读接口中使用 `Q_max_safe`。
- 体积-流量换算必须保留两步表达：`V_limit = 0.9 * phi_t * h * A`，`Q_limit = V_limit / dt_sub`。
- `phi_e_n` 与 `omega_edge` 必须分字段记录；前者归属边法向有效导流标量，后者归属边连通性/开闭权重。
- `max_cell_cfl` 是 raw 物理 CFL 诊断量，不得乘 `CFL_safety`；rollback 判定必须独立比较 `max_cell_cfl` 与 `C_rollback`。
- `mass_deficit_account`、`epsilon_deficit`、`snapshot`、`rollback` 与 `replay` 的目录落位归属 `libs/coupling/core/`，其正确性与故障证据归属 `tests/fault_replay/`。

---

## 8. 测试与验证目录原则

### 8.1 `tests/unit/`

用于验证单个库、单个接口或单个纯函数级模块，不依赖真实第三方 1D 引擎。

### 8.2 `tests/numerical/`

用于验证二维地表数值模型的正确性和稳定性，是混合网格几何一致性、HLLC、Anisotropic DPM、源项平衡、润湿干涸和质量守恒的主要验证位置。

### 8.3 `tests/integration/`

用于验证跨模块集成：

- `surface_swmm/`：二维地表与 SWMM 的耦合。
- `surface_dflowfm/`：二维地表与 D-Flow FM 的耦合。
- `full_coupled/`：二维地表、SWMM、D-Flow FM 三者全耦合。

### 8.4 `tests/golden/`

用于保存 GoldenSuite / GoldenTest 的黄金基准结果，覆盖数值内核、耦合流程和典型城市洪涝场景。

- `suite_manifest/` 固化 GoldenSuite 成员、GoldenTest 元数据、CI gate 绑定关系和适用 Phase。
- `reference/` 固化参考输入、参考输出、容差、生成版本和可复现基线。
- `evidence/` 固化 gate 执行结果、差异摘要、日志、守恒证据和发布/合并判定材料。

### 8.5 `tests/fault_replay/`

用于验证 fault handling、snapshot、rollback、replay 与 `mass_deficit_account` 的一致性，是稳定性协议运行证据的固定落位。

### 8.6 `tests/compatibility/`

用于验证第三方版本升级、patch 变化和构建接入变化不会破坏 ABI 边界、适配器契约或回归算例。

### 8.7 `tests/performance/`

用于验证 CPU/CUDA 后端性能、内存占用、吞吐率和大规模算例可扩展性。

---

## 9. 规范性结论

本目录结构是 SCAU-UFM target-state 工程落位的规范入口，补齐并固化以下边界：

1. **二维地表主模型的明确工程落位**
   - `libs/mesh/` 明确承载三角/四边形混合非结构化网格拓扑、几何量和质量约束。
   - `libs/surface2d/` 明确承载 Anisotropic DPM、HLLC、源项、润湿干涸、时间推进、CFL 诊断和 CPU/CUDA 后端。

2. **D-Flow FM 的明确工程落位**
   - D-Flow FM 在 `libs/coupling/river/`、`extern/dflowfm/`、`extern/dflowfm-bmi/`、`cmake/third_party/` 和 `tests/integration/surface_dflowfm/` 中形成闭环，并与 SWMM 保持并列 1D 引擎地位。

3. **第三方版本锁定与兼容性治理的明确工程落位**
   - 第三方源码、版本声明、补丁、许可证、兼容性矩阵、ABI 边界政策、构建接入和兼容性测试必须分层管理。

4. **验证证据链的明确工程落位**
   - `tests/numerical/`、`tests/golden/`、`tests/fault_replay/`、`tests/integration/`、`tests/compatibility/` 与 `tests/performance/` 分别承载数值正确性、GoldenSuite 门禁、故障回放、全耦合集成、第三方兼容性和性能扩展证据。

实现团队必须能够从目录结构直接确认：

- SCAU-UFM 是以三角/四边形混合非结构化网格上的二维 Anisotropic DPM HLLC 地表模型为主模型的全耦合城市洪涝系统。
- `SWMM + D-Flow FM` 是并列嵌入的双 1D 引擎。
- `CouplingLib core` 是耦合协议、调度、映射、守恒、deficit 账、snapshot、rollback 和 replay 的唯一归属层。
- 第三方源码、第三方治理、我方适配器、构建接入和验证体系必须保持分层管理，不得互相承载职责。
