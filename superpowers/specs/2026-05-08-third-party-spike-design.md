# SCAU-UFM 第三方引擎 spike 设计

**日期**: 2026-05-08
**性质**: 工程类规范（条件性权威）
**适用范围**: SWMM 5.2.x 与 D-Flow FM / BMI bridge 两路 1D 引擎在主仓库正式接入之前的可行性 spike
**配套主 Spec**: `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md`（§7 引擎适配器契约）
**配套稳定性协议**: `superpowers/specs/2026-04-14-scau-ufm-stability-reliability-protocol.md`

---

## 0. 文档定位

本 spec 是 SCAU-UFM Phase 1 工程实施的入口前提之一。在 SWMM 与 D-Flow FM 两路 spike 退出标准未达成前，主仓库 `libs/coupling/drainage/` 与 `libs/coupling/river/` 不得进入正式实现；相关接口契约（主 Spec §7.1 / §7.2 / §7.3 / §7.4）在 spike 结论支持下可能需要回写调整。

本 spec 在两路 spike 退出标准达成且证据归档后转为历史记录，不再作为权威。

---

## 1. 目的与非目标

### 1.1 目的

- 验证主 Spec §7 中 `ISwmmEngine` 与 `IDFlowFMEngine` 接口的全部假设是否在真实第三方实现上成立。
- 暴露任何 ABI / 生命周期 / 错误处理 / 版本稳定性方面的 gap，并产出"接口落差矩阵"作为主 Spec §7 回写依据。
- 把"假设第三方支持 X"变成"已验证第三方支持 X 或不支持 X 但有缓解策略"。

### 1.2 非目标

- 不实现耦合仲裁、`Q_limit`、deficit 账、snapshot/replay 或任何 §6 / §8 主语义。spike 范围严格限于第三方引擎的可被外部驱动能力。
- 不集成进 `libs/coupling/`。spike 落位在 `spikes/`，构建独立、依赖独立。
- 不验证物理结果正确性。物理验证由 sandbox B 与 GoldenSuite 承担。
- 不做性能 benchmark；性能问题在 release Phase 2 CUDA backend 阶段验证。

---

## 2. 仓库落位

```text
spikes/
├── swmm/
│   ├── CMakeLists.txt
│   ├── host/
│   │   └── swmm_spike_host.cpp
│   ├── cases/
│   │   ├── single_pipe.inp
│   │   └── manhole_overflow.inp
│   └── evidence/
│       ├── abi_gotchas.md
│       ├── interface_gap_matrix.md
│       └── spike_report.md
└── dflowfm/
    ├── CMakeLists.txt
    ├── host/
    │   └── dflowfm_spike_host.cpp
    ├── cases/
    │   ├── single_reach.mdu
    │   └── reach_with_weir.mdu
    └── evidence/
        ├── abi_gotchas.md
        ├── interface_gap_matrix.md
        └── spike_report.md
```

约束：

- `spikes/` 不得与 `libs/`、`extern/`、`third_party/`、`apps/` 共享 CMake target；spike 自带独立构建系统，避免污染主仓库。
- `spikes/<engine>/host/` 中代码可直接 include 第三方头文件，**不**走主仓库 §7 接口；其唯一目的是探测第三方实际能力。
- `spikes/<engine>/evidence/` 中文档进入 git；二进制构建产物不进 git。

---

## 3. 必须验证的假设清单（must-pass）

主 Spec §7 假定 1D 引擎具备以下能力。每条假设在两个 spike 中都必须给出可重放证据。

### 3.1 生命周期

| 假设 | SWMM | D-Flow FM | 验证方法 |
|---|---|---|---|
| 可从配置文件 init 出运行实例 | 是 | 是 | host 调用 init API；进程退出码 0 |
| 可在不退出进程的前提下重置或销毁实例 | 是 | 是 | host 调用 finalize API 后再 init 一次，无内存崩溃 |
| 错误时返回明确错误码或异常 | 是 | 是 | 故意提供错误配置，捕获错误并打印 |

### 3.2 时间推进

| 假设 | 验证方法 |
|---|---|
| 可以 step-by-step 推进，外部控制 dt | host 显式调用 step(dt) 并打印每步内部时间 |
| step 之间内部状态保持稳定，可读 | step 之间读取节点水位 / 管段流量并打印 |
| 引擎不擅自加大 dt 或多步合并 | 验证内部时间增量与外部传入 dt 一致 |

### 3.3 状态读写

| 假设 | SWMM 字段示例 | D-Flow FM 字段示例 |
|---|---|---|
| 可在任意 step 之间读出耦合所需状态 | 节点水位、节点 inflow、管段 surcharge | 河道断面水位、节点流量、堰流量 |
| 可在 step 之前写入耦合输入 | 节点 inflow / outflow | 节点 boundary discharge / stage |
| 写入后下一个 step 真正反映写入值 | 写后读、step 后再读，对照 | 同 |

### 3.4 hot-start / 状态保存

| 假设 | 验证方法 |
|---|---|
| 可在任意 step 之间触发 state 保存 | host 调用 save_state，落地为文件或内存对象 |
| 可从保存点重新启动且与原始路径推进结果一致 | 保存 → 继续 N 步 → 与从保存点重新启动 N 步对比 |
| hot-start 文件格式可在版本间稳定读取（同 minor 版本） | 用 5.2.x 不同补丁版本测试 |

### 3.5 错误与异常

| 假设 | 验证方法 |
|---|---|
| 引擎不在不可恢复错误时抛出 longjmp / abort 强制结束宿主进程 | 故意诱发数值不收敛，确认仍可被 host 捕获 |
| 错误码或异常含足够信息可分类（fatal / recoverable / warn） | 多种错误情景，记录返回值 |
| 引擎不持有静态全局状态导致第二个实例不可创建（同进程内） | 同进程内创建 2 个实例，分别推进，状态独立 |

### 3.6 版本稳定性

| 假设 | 验证方法 |
|---|---|
| 在锁定的 minor 版本范围内 API / ABI 稳定 | 对至少 2 个 patch 版本重跑同一 host，行为一致 |
| 升级到下一个 minor 版本后接口或语义有变更被记录 | 升级跑一次，若出现行为差异，写入 abi_gotchas.md |

---

## 4. 必须记录的 gap 清单（must-document）

即使下列条目通过，也必须显式记录：

- 每个 API 的线程安全性结论（main thread only / thread-safe / unspecified）。
- 每个 API 的内存所有权（caller-owned / callee-owned / refcount）。
- 每个 state field 的单位（明确写出，而不是默认 SI）。
- 每个 state field 的有效定义域（含负值含义、特殊值如 NaN / -999 / DBL_MAX）。
- 引擎对操作系统资源的占用（线程、文件句柄、临时目录）。
- 引擎对外部依赖（OpenMP、MKL、netCDF 等）的硬性要求与版本。

未在 spike 报告中显式回答上述 6 项的，不视为通过退出标准。

---

## 5. SWMM 5.2.x 子 spike

### 5.1 测试用例

- `cases/single_pipe.inp`：上游恒定入流、单管、下游自由出流。最小 sanity case，验证 init/step/state 基本能力。
- `cases/manhole_overflow.inp`：包含一个会发生 surcharge 的检查井。验证 §3.3 中 `swmm_single_pipe_surcharge` 类似场景下的状态读取，对应 G8 GoldenTest 的物理本底。

### 5.2 host 程序结构

```cpp
// spikes/swmm/host/swmm_spike_host.cpp 骨架
int main(int argc, char** argv) {
    auto handle = swmm_open(input, report, output);
    swmm_start(0);

    for (int step = 0; step < N_STEPS; ++step) {
        // §3.3 写入测试：耦合下注入 inflow
        swmm_setNodeInflow(node_id, q_inject);
        double dt;
        swmm_step(&dt);
        // §3.3 读取测试
        double head = swmm_getNodeValue(node_id, NODE_HEAD);
        double overflow = swmm_getNodeValue(node_id, NODE_OVERFLOW);
        log(step, head, overflow);

        // §3.4 hot-start：每 K 步保存一次
        if (step % K == 0) swmm_saveHotStart(path);
    }

    swmm_end();
    swmm_close();
    return 0;
}
```

实际 API 名称以 SWMM 5.2.x 头文件为准；spike 的目的之一就是把上述伪代码"对齐到真实 API"并把每个对齐过程的 gap 写入 `abi_gotchas.md`。

### 5.3 关注点（30 年经验提示）

- SWMM 5.2.x 历史上对 hot-start 的支持稳定但**对子时间步原地恢复支持薄弱**——很多版本只能从开头重跑。这是 §7.1 与 §6.5 snapshot 范围设计的核心风险。spike 必须确认能否真正"任意 step 之间保存 → 任意 step 之间读取并继续"。
- `swmm_setNodeInflow` 类外部 API 在 5.2 之前每个版本都微调过签名/语义，spike 必须锁定具体可用 API 的版本号写入 `interface_gap_matrix.md`。

---

## 6. D-Flow FM 子 spike

### 6.1 测试用例

- `cases/single_reach.mdu`：单条河段、上游恒定流量、下游恒定水位。最小 sanity case。
- `cases/reach_with_weir.mdu`：含一个固定堰。验证 §3.3 中读取河道断面状态、堰流量与堰位置的能力，对应 G11 `dflowfm_river_steady` 的物理本底。

### 6.2 host 程序结构

```cpp
// spikes/dflowfm/host/dflowfm_spike_host.cpp 骨架（基于 BMI）
int main(int argc, char** argv) {
    bmi::Bmi* model = create_dflowfm_bmi();
    model->initialize(config);

    for (int step = 0; step < N_STEPS; ++step) {
        // §3.3 写入测试：boundary discharge
        model->set_value("boundary_discharge", &q_inject);
        model->update_until(target_time);
        // §3.3 读取测试
        std::vector<double> stages;
        model->get_value("stage_at_section", stages.data());
        double weir_q = get_structure_state("weir_id", "discharge");
        log(step, stages, weir_q);

        // §3.4 hot-start
        if (step % K == 0) model->save_state(path);
    }

    model->finalize();
    return 0;
}
```

实际 BMI 接口签名以 D-Flow FM 当前发行版的 BMI 头为准；spike 同样负责"对齐到真实 API"并记录 gap。

### 6.3 关注点（30 年经验提示）

- D-Flow FM 的 BMI 实现历史上对**外部时间步精确控制**支持不一致：早期版本 update_until 内部可能多步，且不暴露内部 dt 细节，这直接威胁 `dt_couple` 的耦合一致性。spike 必须验证这一点。
- D-Flow FM 的水工建筑物（weir / gate / pump）状态读写在 BMI 层不一定全开放，部分需要走 RTC 通道。`RiverExchangePoint.priority_weight` 与 RTC 状态在 §7.5 假定可读，spike 必须给出确切的可读字段清单。
- D-Flow FM 通常运行在自身工作目录中，对当前工作目录、临时文件路径、环境变量有隐含依赖。spike 必须显式记录这些前置条件。

---

## 7. 退出标准

两个子 spike 各自独立判定。下列条件全部满足才视为退出：

1. §3 表中所有 must-pass 假设均有可重放证据写入 `evidence/spike_report.md`。
2. §4 中 6 项 must-document gap 均显式回答。
3. `evidence/interface_gap_matrix.md` 给出主 Spec §7 接口逐条对应的真实第三方 API 落差状态：`MATCH` / `MATCH_WITH_NOTE` / `GAP_MITIGABLE` / `GAP_BLOCKING`。
4. 任一 `GAP_BLOCKING` 必须给出具体的接口设计调整提案，并指明影响主 Spec §7 的哪些段落。
5. host 二进制可在干净环境下从 `cases/` 起跑通至少 100 个 step，并产出可比对日志。
6. `evidence/abi_gotchas.md` 至少覆盖：线程安全、内存所有权、状态字段单位、定义域、OS 资源、外部依赖。

---

## 8. 失败回退路径

- 若 `GAP_BLOCKING` 在 SWMM 子 spike 中出现：暂停主仓库 `libs/coupling/drainage/` 实施；按提案修订主 Spec §7.1 / §7.2，并触发 brainstorming 二轮。
- 若 `GAP_BLOCKING` 在 D-Flow FM 子 spike 中出现：暂停主仓库 `libs/coupling/river/` 实施；按提案修订主 Spec §7.3 / §7.4 / §7.5，并触发 brainstorming 二轮。
- 若两者均出现 `GAP_BLOCKING` 且属于"step-by-step 推进不可控"类阻断：考虑把 D-Flow FM 推迟到 release Phase 2 之外的扩展能力，并降级 G11 / G12 的适用 Phase。该决策需要更新主 Spec §10.1 与 §10.2 的 release Phase 列。

---

## 9. 证据归档

退出标准达成后：

- spike 报告（`spike_report.md`）合并入 `superpowers/specs/2026-05-08-third-party-spike-design-evidence.md`，作为 §7 接口最终封板的依据。
- `interface_gap_matrix.md` 在 `MATCH_WITH_NOTE` / `GAP_MITIGABLE` 项中的 NOTE 必须显式回写到主 Spec §7 对应小节，避免 spike 结论与主 Spec 长期分离。
- 本 spec 文件在归档后保留为历史记录，不作为权威。

---

## 10. 与 Phase 路线图的关系

本 spike 是 Phase 1 实施的入口前提：

- Phase 1 capability 中 SWMM 适配相关条目（`libs/coupling/drainage/`、MockSwmmEngine 通路）依赖 SWMM 子 spike 退出。
- Phase 2 capability 中 D-Flow FM 河网耦合依赖 D-Flow FM 子 spike 退出。
- Phase 1 release gate 不要求 D-Flow FM 子 spike 完成；但若 SWMM 子 spike 不通过，Phase 1 release gate 同样不能通过。
