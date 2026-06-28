# SCAU-UFM superpowers 权威入口

本索引用于防止修文、复核、状态核销与实施计划被误读为当前规范。除本页列出的权威入口外，`superpowers/specs/` 文档均视为非规范性历史记录。

## 规范性入口

| 入口 | 权限边界 |
|---|---|
| `superpowers/specs/2026-04-11-scau-ufm-global-architecture-design.md` | 主 Spec；定义物理语义、接口契约、系统不变式、耦合核心语义与默认参数。 |
| `superpowers/specs/2026-04-14-scau-ufm-stability-reliability-protocol.md` | 稳定性协议；定义 gate、触发阈值、执行后果、证据要求与 operator 责任。 |
| `superpowers/specs/2026-04-22-symbols-and-terms-reference.md` | 符号与术语表；定义 machine-facing 名称、单位、退役别名与命名约束。 |
| `superpowers/specs/project-layout-design.md` | 目标目录结构设计；定义二维地表内核、CouplingLib（含 1D-1D 交换原语 `core/engine_interface/` 与三方步进驱动 `coupling/driver/`）、SWMM/D-Flow FM 适配、第三方治理（区分源码静态嵌入与 BMI 接口快照 + 运行时 DLL 两种嵌入模式）、validation 层、验证体系与 Phase 演进的目标工程落位；集成验证按三对两两双向 + 全耦合组织。 |

## 执行类规范（条件性权威）

下列 spec 在对应工程动作未完成时为条件性权威，约束当时的 spike / sandbox 退出标准与证据要求；动作完成且证据归档后转为历史记录。在归档前主仓库不得越过其退出标准进入对应阶段实施。

| 入口 | 权限边界 |
|---|---|
| `superpowers/specs/2026-05-08-third-party-spike-design.md` | SWMM 5.2.x 与 D-Flow FM / BMI 两路 1D 引擎可行性 spike；定义假设清单、退出标准、ABI 落差矩阵与失败回退路径。Phase 1 实施 `libs/coupling/drainage/`、Phase 2 实施 `libs/coupling/river/` 的入口前提。 |
| `superpowers/specs/2026-05-08-numerical-sandbox-design.md` | 三角/四边形混合非结构化网格上的 HLLC + DPM well-balanced 数值 sandbox；定义 sympy 派生证明、G1/G2/G3 在最小混合网格上的退出标准与失败回退路径。Phase 1 实施 `libs/surface2d/dpm/` / `riemann/` / `source_terms/` 的入口前提。 |

## 阶段性证据入口（非规范性）

下列文档用于定位近期实施阶段的边界分析、证据汇总与复核结论。它们不提升为规范性入口，不覆盖主 Spec、稳定性协议、符号表、目录设计或条件性权威 spike / sandbox 规范；若与上表冲突，以上表为准。

| 入口 | 证据边界 |
|---|---|
| `superpowers/specs/2026-05-29-m76-drainage-river-adapter-entry-boundary-analysis.md` | Drainage/River 适配层进入边界分析；确认真实 SWMM / D-Flow FM 集成仍受第三方 spike gate 约束，并限定下一步为 mock-only、third-party-free seam。 |
| `superpowers/specs/2026-05-29-m88-mock-adapter-boundary-hardening-rollup.md` | M77-M87 mock-only 适配层边界加固 rollup；汇总 third-party-free、DTO 使用、输入域校验与适配层不得持有核心运行时语义的证据。 |
| `superpowers/specs/2026-05-29-m93-mock-fixture-api-rollup.md` | M89-M92 mock fixture API rollup；汇总 SWMM / D-Flow FM mock fixture 与双 mock 场景证据，仍不构成真实第三方引擎集成证据。 |
| `superpowers/specs/2026-05-29-m94-engine-interface-core-runtime-member-guard-evidence.md` | M94 post-rollup evidence；记录 engine interface 禁止承载核心运行时成员名的直接编译期 guard，延续 m93 后续边界加固。 |
| `superpowers/specs/2026-05-29-m95-dflowfm-accept-lateral-discharge-target-evidence.md` | M95 post-rollup evidence；记录 D-Flow FM mock accept 边界只能写入 `lateral_discharge` 的目标校验，仍不构成真实 D-Flow FM 集成证据。 |
| `superpowers/specs/2026-05-30-m101-shared-cell-mock-coupling-rollup.md` | M96-M101 shared-cell / mock-coupling historical rollup；定位双 mock 共享单元、仲裁与元数据证据，不构成真实 1D 引擎集成证据；其中 aggregate shared deficit/replay 表述已由 M148 纠偏证据取代。 |
| `superpowers/specs/2026-05-30-m102-mock-coupling-scheduler-boundary-evidence.md` | M102 mock coupling scheduler historical evidence；记录 shared-cell mock scheduler 边界，不构成真实 scheduler 或真实 1D 引擎集成证据；其中 shared deficit/replay 语义同样由 M148 纠偏证据取代。 |
| `superpowers/specs/2026-06-01-m148-shared-cell-endpoint-deficit-replay-correction-evidence.md` | M148 shared-cell endpoint deficit/replay correction evidence；记录 endpoint-owned shared deficit ledger/replay、duplicate endpoint fail-closed validation、drain-split applied-volume 和 zero-storage replay guard 的当前纠偏证据；明确 supersede M96-M102 中的 aggregate shared deficit/replay 表述。 |
| `superpowers/specs/2026-05-30-m106-health-boundary-rollup.md` | Mock engine health boundary rollup；汇总 mock health 状态边界与第三方无关的健康检查证据。 |
| `superpowers/specs/2026-05-30-m108-health-to-diagnostic-rollup.md` | Health-to-diagnostic rollup；汇总 mock health 到 fault diagnostic 的非执行性诊断证据。 |
| `superpowers/specs/2026-05-30-m110-fault-controller-passive-diagnostic-to-policy-rollup.md` | Fault-controller passive diagnostic-to-policy rollup；汇总被动诊断到 policy 映射证据，仍不执行运行时控制。 |
| `superpowers/specs/2026-05-30-m113-mock-scheduler-passive-observation-rollup.md` | Mock scheduler passive observation rollup；汇总调度器被动观测证据，不构成真实 scheduler 执行证据。 |
| `superpowers/specs/2026-05-30-m116-passive-action-consumption-rollup.md` | Passive action consumption rollup；汇总 operator action 被动消费链路证据。 |
| `superpowers/specs/2026-05-30-m120-passive-state-classification-rollup.md` | Passive state classification rollup；汇总 fault-controller passive state 分类证据。 |
| `superpowers/specs/2026-05-30-m124-passive-transition-stage-rollup.md` | Passive transition stage rollup；汇总 passive transition 阶段证据。 |
| `superpowers/specs/2026-05-30-m128-passive-action-audit-stage-rollup.md` | Passive action audit stage rollup；汇总 action audit 阶段证据。 |
| `superpowers/specs/2026-05-30-m132-passive-action-outcome-stage-rollup.md` | Passive action outcome stage rollup；汇总 action outcome 阶段证据。 |
| `superpowers/specs/2026-05-30-m136-blocked-action-stage-rollup.md` | Blocked action stage rollup；汇总 blocked action 阶段证据。 |
| `superpowers/specs/2026-06-01-m141-scheduler-control-passive-chain-rollup.md` | Scheduler control passive chain rollup；汇总 scheduler control 被动链路证据。 |
| `superpowers/specs/2026-06-01-m145-scheduler-phase-evidence-stage-rollup.md` | Scheduler phase evidence stage rollup；汇总 scheduler phase evidence 阶段证据。 |
| `superpowers/specs/2026-06-01-m149-passive-scheduler-control-validation-stage-rollup.md` | Passive scheduler control validation stage rollup；汇总 scheduler control validation 阶段证据。 |
| `superpowers/specs/2026-06-01-m156-passive-scheduler-control-policy-validation-stage-rollup.md` | Passive scheduler control policy validation stage rollup；汇总 scheduler control policy validation 阶段证据。 |
| `superpowers/specs/2026-06-01-m159-phase-control-executable-boundary-stage-rollup.md` | Phase-control executable boundary stage rollup；汇总 executable boundary 阶段证据。 |
| `superpowers/specs/2026-06-01-m161-executable-boundary-negative-evidence-stage-rollup.md` | Executable boundary negative evidence stage rollup；汇总 executable boundary 负向证据。 |
| `superpowers/specs/2026-06-01-m163-mutating-control-evidence-stage-rollup.md` | Mutating control evidence stage rollup；汇总 mutation control 证据边界。 |
| `superpowers/specs/2026-06-01-m165-behavior-specific-evidence-dto-rollup.md` | Behavior-specific evidence DTO rollup；汇总 behavior-specific evidence DTO 阶段证据。 |
| `superpowers/specs/2026-06-02-m166-passive-behavior-specific-evidence-dto-completion-rollup.md` | Passive behavior-specific evidence DTO completion rollup；汇总 passive behavior-specific evidence DTO 完成证据。 |
| `superpowers/specs/2026-06-02-m173-evidence-consumption-to-runtime-evidence-no-mutation-rollup.md` | Evidence consumption to runtime evidence no-mutation rollup；汇总 evidence consumption 到 runtime evidence 的无 mutation 阶段证据。 |
| `superpowers/specs/2026-06-02-m177-dry-run-stage-rollup.md` | Dry-run stage rollup；汇总 dry-run 阶段证据，仍不等同于真实外部引擎或发布 gate 证据。 |
| `superpowers/specs/2026-06-03-m183-real-executor-fixture-readiness-rollup.md` | M178-M182 real executor fixture readiness rollup；汇总 real scheduler mutation executor 前置 DTO、fixture 与 postcondition helper 证据，仍不构成真实 executor 实现证据。 |
| `superpowers/specs/2026-06-08-m184-real-scheduler-mutation-executor-implementation-plan.md` | M184 real scheduler mutation executor implementation plan；规划只修改 in-memory scheduler state DTO 的真实 executor、fail-closed 顺序与测试矩阵，仍不构成真实 executor 实现证据。 |
| `superpowers/specs/2026-06-08-m185-real-scheduler-mutation-executor-implementation-evidence.md` | M185 real scheduler mutation executor implementation evidence；记录 in-memory scheduler mutation executor、fail-closed 测试与 side-effect-free 边界，仍不构成外部 runtime/adapter/release gate 证据。 |
| `superpowers/specs/2026-06-08-m186-real-executor-postcondition-consumption-evidence.md` | M186 real executor postcondition consumption evidence；记录 executor result 后置条件消费边界，确保缺少 verified postcondition evidence 时 mutation 不可视为完成。 |
| `superpowers/specs/2026-06-08-m187-runtime-evidence-writing-consumption-design.md` | M187 runtime evidence writing consumption design；规划 completed scheduler mutation 到 runtime evidence write intent 的消费边界，保持 executor/postcondition consumption 不直接外写或触发 release gate。 |
| `superpowers/specs/2026-06-08-m188-runtime-evidence-write-intent-implementation-evidence.md` | M188 runtime evidence write intent implementation evidence；记录 completed mutation 到 side-effect-free runtime write intent 的实现与 fail-closed 覆盖，仍不构成外部写入或 release gate 证据。 |
| `superpowers/specs/2026-06-08-m189-release-gate-policy-consumption-design.md` | M189 release-gate policy consumption design；规划 write-intent/runtime evidence 到 release-gate policy decision 的消费边界，仍不直接 emit `REVIEW_REQUIRED`、`BLOCK_MERGE`、`BLOCK_RELEASE` 或 `ABORT`。 |
| `superpowers/specs/2026-06-08-m190-release-gate-policy-decision-implementation-evidence.md` | M190 release-gate policy decision implementation evidence；记录 release-gate policy decision boundary 的实现与 fail-closed 覆盖，仍不 emit release-gate action、CI fail 或 release block。 |
| `superpowers/specs/2026-06-08-m191-real-scheduler-mutation-executor-chain-rollup.md` | M191 real scheduler mutation executor chain rollup；汇总 M184-M190 从 in-memory mutation 到 completion、write-intent、policy decision 的 side-effect-free 链路证据。 |
| `superpowers/specs/2026-06-08-m192-real-executor-test-maintainability-evidence.md` | M192 real executor test maintainability evidence；记录 executor chain 测试 helper 抽取与 completion/runtime-write/policy 窄目标拆分，保持行为不变。 |
| `superpowers/specs/2026-06-08-m193-external-runtime-evidence-writer-boundary-design.md` | M193 external runtime evidence writer boundary design；规划 prepared write-intent 后的独立外部 evidence writer 边界，保留 release-gate action 与上游 executor 链路分离。 |
| `superpowers/specs/2026-06-08-m194-external-runtime-evidence-writer-implementation-evidence.md` | M194 external runtime evidence writer implementation evidence；记录 writer dry-run/commit result boundary 的实现与 fail-closed 覆盖，仍不 emit release-gate action 或执行真实外部发布。 |
| `superpowers/specs/2026-06-08-m195-release-gate-action-intent-boundary-design.md` | M195 release-gate action intent boundary design；规划 committed runtime evidence writer result 与 policy decision 后的 action intent 边界，仍不 emit release-gate action。 |
| `superpowers/specs/2026-06-08-m196-release-gate-action-intent-implementation-evidence.md` | M196 release-gate action intent implementation evidence；记录 action intent DTO/function、fail-closed 与 intent mapping 覆盖，仍不 emit release-gate action 或执行真实外部发布。 |
| `superpowers/specs/2026-06-08-m197-release-gate-action-emission-boundary-design.md` | M197 release-gate action emission boundary design；规划 prepared action intent 到 emission result 的 dry-run/commit 结果边界，仍不集成真实 CI/release 发布。 |
| `superpowers/specs/2026-06-08-m198-release-gate-action-emission-implementation-evidence.md` | M198 release-gate action emission implementation evidence；记录 emission result DTO/function、fail-closed 与 dry-run/success/failure 覆盖，仍不执行真实 CI/release 发布。 |
| `superpowers/specs/2026-06-08-m199-release-gate-action-completion-boundary-design.md` | M199 release-gate action completion boundary design；规划 emission result 到 completion record 的完成/失败/review 记录边界，仍不执行真实 CI/release 发布。 |
| `superpowers/specs/2026-06-08-m200-release-gate-action-completion-implementation-evidence.md` | M200 release-gate action completion implementation evidence；记录 completion DTO/function 与 emitted/dry-run/failed/review mapping 覆盖，仍不执行真实 CI/release 发布。 |
| `superpowers/specs/2026-06-08-m201-release-gate-action-chain-rollup.md` | M201 release-gate action chain rollup；汇总 M195-M200 intent/emission/completion side-effect-free 链路证据，仍不构成真实 CI/release 发布。 |
| `superpowers/specs/2026-06-08-m202-external-release-gate-publisher-boundary-design.md` | M202 external release-gate publisher boundary design；规划 CouplingLib core 之外的真实 CI/release 发布边界与 fail-closed 前置条件，仍不实现外部发布。 |
| `superpowers/specs/2026-06-08-m203-mock-release-gate-publisher-boundary-design.md` | M203 mock release-gate publisher boundary design；规划 validation 层 mock-only publisher result 边界，消费 completion record 但不执行真实 CI/release 发布。 |
| `superpowers/specs/2026-06-08-m204-mock-release-gate-publisher-implementation-evidence.md` | M204 mock release-gate publisher implementation evidence；记录 validation 层 mock publisher DTO/function、fail-closed 与 dry-run/commit 覆盖，仍不执行真实 CI/release 发布。 |
| `superpowers/specs/2026-06-08-m205-mock-release-gate-publisher-chain-rollup.md` | M205 mock release-gate publisher chain rollup；汇总 M202-M204 external/mock publisher 边界、validation placement 与无真实 CI/release 发布证据。 |
| `superpowers/specs/2026-06-08-m206-serialized-publisher-payload-boundary-design.md` | M206 serialized publisher payload boundary design；规划 validation 层稳定 publisher payload/reference 边界，仍不实现真实序列化 sink 或外部发布。 |
| `superpowers/specs/2026-06-08-m207-mock-serialized-publisher-payload-boundary-design.md` | M207 mock serialized publisher payload boundary design；规划 validation 层 mock payload DTO/function 的 fail-closed 与 pass/no-action 语义。 |
| `superpowers/specs/2026-06-08-m208-mock-serialized-publisher-payload-implementation-evidence.md` | M208 mock serialized publisher payload implementation evidence；记录 validation 层 payload DTO/function、稳定 metadata 与 fail-closed 覆盖，仍不执行真实序列化或外部发布。 |
| `superpowers/specs/2026-06-08-m209-serialized-publisher-payload-chain-rollup.md` | M209 serialized publisher payload chain rollup；汇总 M206-M208 payload design/mock implementation 与稳定 metadata 证据，仍不执行真实序列化或外部发布。 |
| `superpowers/specs/2026-06-08-m210-mock-publisher-payload-consumption-boundary-design.md` | M210 mock publisher payload consumption boundary design；规划 validation mock publisher 消费 prepared payload record 而非 raw completion DTO。 |
| `superpowers/specs/2026-06-08-m211-mock-publisher-payload-consumption-implementation-evidence.md` | M211 mock publisher payload consumption implementation evidence；记录 payload-driven mock publish DTO/function 与 dry-run/commit 覆盖，仍不执行真实外部发布。 |
| `superpowers/specs/2026-06-08-m212-payload-driven-mock-publisher-chain-rollup.md` | M212 payload-driven mock publisher chain rollup；汇总 M210-M211 prepared payload record 驱动 mock publisher 的链路证据。 |
| `superpowers/specs/2026-06-08-m213-publisher-chain-aggregate-record-boundary-design.md` | M213 publisher chain aggregate record boundary design；规划 validation 层 payload preparation 与 payload-driven publish result 的一致性汇总记录。 |
| `superpowers/specs/2026-06-08-m214-publisher-chain-aggregate-record-implementation-evidence.md` | M214 publisher chain aggregate record implementation evidence；记录 validation 层 aggregate DTO/function 与 payload/result consistency fail-closed 覆盖。 |
| `superpowers/specs/2026-06-08-m215-publisher-chain-aggregate-rollup.md` | M215 publisher chain aggregate rollup；汇总 M213-M214 aggregate record 设计/实现证据与 fail-closed bug 修复。 |
| `superpowers/specs/2026-06-08-m216-validation-release-gate-publisher-stage-rollup.md` | M216 validation release-gate publisher stage rollup；汇总 M202-M215 从 external publisher design 到 mock payload-driven aggregate 的 validation 阶段证据。 |
| `superpowers/specs/2026-06-08-m217-validation-publisher-dependency-guard-design.md` | M217 validation publisher dependency guard design；规划 CouplingLib core 无 validation publisher 依赖、validation publisher 无真实 CI/release sink 依赖的 guard。 |
| `superpowers/specs/2026-06-08-m218-validation-publisher-dependency-guard-implementation-evidence.md` | M218 validation publisher dependency guard implementation evidence；记录 core 无 validation publisher 依赖、validation publisher 无真实 sink/adapter/runtime executor 依赖的 source guard。 |
| `superpowers/specs/2026-06-08-m219-validation-publisher-dependency-guard-rollup.md` | M219 validation publisher dependency guard rollup；汇总 M217-M218 source guard 对 core 独立性与 validation publisher mock-only 依赖边界的覆盖。 |
| `superpowers/specs/2026-06-08-m220-validation-release-gate-publisher-stage-completion-rollup.md` | M220 validation release-gate publisher stage completion rollup；合并 M216 阶段证据与 M219 dependency guard，关闭 mock-only publisher stage。 |
| `superpowers/specs/2026-06-08-m221-real-external-publisher-readiness-gap-analysis.md` | M221 real external publisher readiness gap analysis；列出真实 CI/release publisher 前置审批、策略、安全、非生产 sink 与测试缺口，确认 real publisher 仍 blocked。 |
| `superpowers/specs/2026-06-08-m222-non-production-real-publisher-sandbox-plan.md` | M222 non-production real publisher sandbox plan；定义真实 publisher 前必须具备的受控非生产 sink、审批、安全、测试与 guard 条件，仍不实现外部发布。 |
| `superpowers/specs/2026-06-08-m223-non-production-publisher-sandbox-sink-contract-design.md` | M223 non-production publisher sandbox sink contract design；定义受控非生产 sink 请求/响应、idempotency、fail-closed 与测试语义，仍不实现 sink。 |
| `superpowers/specs/2026-06-08-m224-mock-in-memory-sandbox-sink-boundary-design.md` | M224 mock in-memory sandbox sink boundary design；规划 validation 层 deterministic sink response mapper，仍不做网络/文件/CI/release 发布。 |
| `superpowers/specs/2026-06-08-m225-mock-in-memory-sandbox-sink-implementation-evidence.md` | M225 mock in-memory sandbox sink implementation evidence；记录 mock sink DTO/function 与 dry-run/commit/idempotency/fail-closed 覆盖。 |
| `superpowers/specs/2026-06-08-m226-non-production-sandbox-sink-rollup.md` | M226 non-production sandbox sink rollup；汇总 M222-M225 sandbox plan、sink contract 与 mock in-memory sink evidence，仍无网络/文件/CI/release 副作用。 |
| `superpowers/specs/2026-06-08-m227-mock-sandbox-publisher-adapter-boundary-design.md` | M227 mock sandbox publisher adapter boundary design；规划 prepared payload 到 mock sink request/response 的 validation adapter 边界，仍无网络/文件/CI/release 副作用。 |
| `superpowers/specs/2026-06-08-m228-mock-sandbox-publisher-adapter-boundary-design.md` | M228 mock sandbox publisher adapter boundary design；规划 validation adapter 的实现边界、pre-submit fail-closed 与 sink response mapping。 |
| `superpowers/specs/2026-06-08-m229-mock-sandbox-publisher-adapter-implementation-evidence.md` | M229 mock sandbox publisher adapter implementation evidence；记录 publish_mock_sandbox_payload DTO/function 与 mock sink adapter 覆盖，仍无真实副作用。 |
| `superpowers/specs/2026-06-09-m230-mock-sandbox-publisher-stage-record-boundary-design.md` | M230 mock sandbox publisher stage record boundary design；规划消费 mock sandbox publisher adapter result 并固化为稳定 stage record 的 validation 边界，仍无真实副作用。 |
| `superpowers/specs/2026-06-09-m230-mock-sandbox-publisher-stage-record-implementation-evidence.md` | M230 mock sandbox publisher stage record implementation evidence；记录 consume_mock_sandbox_publisher_adapter_result DTO/function 与 status mapping/metadata 覆盖，仍无真实副作用。 |
| `superpowers/specs/2026-06-09-m231-mock-sandbox-publisher-stage-aggregate-boundary-design.md` | M231 mock sandbox publisher stage aggregate boundary design；规划消费 adapter result 与 stage record 并固化为 rollup-ready aggregate record 的 validation 边界，仍无真实副作用。 |
| `superpowers/specs/2026-06-09-m231-mock-sandbox-publisher-stage-aggregate-implementation-evidence.md` | M231 mock sandbox publisher stage aggregate implementation evidence；记录 make_mock_sandbox_publisher_stage_aggregate DTO/function 与 adapter/stage consistency 覆盖，仍无真实副作用。 |
| `superpowers/specs/2026-06-09-m232-mock-sandbox-publisher-stage-completion-rollup.md` | M232 mock sandbox publisher stage completion rollup；汇总 M227-M231 mock sandbox publisher validation chain，关闭 mock-only publisher stage，仍不构成真实发布或 release readiness 证据。 |
| `superpowers/specs/2026-06-09-m233-mock-sandbox-publisher-final-chain-rollup-boundary-design.md` | M233 mock sandbox publisher final chain rollup boundary design；规划消费 stage aggregate 并固化为 final chain record 的 validation 边界，仍无真实副作用。 |
| `superpowers/specs/2026-06-09-m233-mock-sandbox-publisher-final-chain-rollup-implementation-evidence.md` | M233 mock sandbox publisher final chain rollup implementation evidence；记录 make_mock_sandbox_publisher_final_chain_record DTO/function 与 final chain consistency 覆盖，仍无真实副作用。 |
| `superpowers/specs/2026-06-09-m234-mock-sandbox-publisher-final-chain-completion-rollup.md` | M234 mock sandbox publisher final chain completion rollup；汇总 M227-M233 mock sandbox publisher validation chain，关闭 mock-only final chain，仍不构成真实发布或 production readiness 证据。 |
| `superpowers/specs/2026-06-09-m235-controlled-non-production-real-publisher-readiness-gap-analysis.md` | M235 controlled non-production real publisher readiness gap analysis；列出真实 publisher 前置审批、credential、sink policy、idempotency、audit 与 failure-handling 缺口，确认 real publisher 仍 blocked。 |
| `superpowers/specs/2026-06-09-m236-controlled-non-production-real-publisher-sandbox-contract-design.md` | M236 controlled non-production real publisher sandbox contract design；定义非生产 sandbox sink request/response、credential reference、operator approval、idempotency、audit 与 fail-closed 语义，仍不实现真实发布。 |
| `superpowers/specs/2026-06-09-m237-controlled-non-production-publisher-sandbox-contract-mock-implementation-evidence.md` | M237 controlled non-production publisher sandbox contract mock implementation evidence；记录 deterministic validation-layer contract mapper 与 fail-closed 覆盖，仍无真实 sink 提交或外部发布。 |
| `superpowers/specs/2026-06-09-m238-controlled-non-production-publisher-sandbox-contract-stage-rollup.md` | M238 controlled non-production publisher sandbox contract stage rollup；汇总 M235-M237 contract gap/design/mock evidence，关闭 validation-only contract stage，仍不构成真实发布或 production readiness 证据。 |
| `superpowers/specs/2026-06-12-m239-controlled-non-production-publisher-sandbox-contract-dependency-guard-design.md` | M239 controlled non-production publisher sandbox contract dependency guard design；定义 source/build/evidence 负向 guard，防止 mock contract 引入真实 network/filesystem/credential resolver/CI-release/engine 依赖。 |
| `superpowers/specs/2026-06-13-m241-surface2d-source-terms-and-geometry-cache-evidence.md` | M241 Surface2D 源项补全与几何缓存优化证据；记录 Manning 摩阻/降雨/入渗/耦合交换源项、GeometryCache 热路径优化、fail-closed 加固与 127/127 全量测试证据；不构成 GoldenSuite 门禁证据。 |
| `superpowers/specs/2026-06-13-m242-surface2d-cfl-wetting-structure-evidence.md` | M242 Surface2D CFL 与 Wetting/Drying 结构对齐证据；记录 `cfl/diagnostics`、`wetting_drying/limits` 模块抽取、step.cpp 边界收敛与 130/130 全量测试证据。 |
| `superpowers/specs/2026-06-13-m244-anisotropic-dpm-tensor-projection-evidence.md` | M244 Anisotropic DPM 张量投影模块证据；记录 `dpm/tensor_projection` 边张量算术平均、法向/切向投影、退化保守规则、各向异性度量与弱保证标记（主 Spec §5.3/§5.5.2），纯函数 fail-closed、零回归、132/132 全量测试；尚未接入 HLLC 热路径，不构成 GoldenSuite 门禁证据。 |
| `superpowers/specs/2026-06-13-m245-phi-c-tensor-migration-and-edge-conveyance-assembly-evidence.md` | M245 Phi_c 张量化与边导流装配证据；`CellDpmFields.Phi_c` 标量迁移为 `Tensor2Symmetric`，新增 `dpm/edge_conveyance` 从张量派生边 `phi_e_n`（算术平均投影 + omega 缩放 + 弱保证计数）；默认单位张量保持 `phi_e_n=1.0` 不漂移 golden，HLLC 仍消费边 `phi_e_n`，133/133 全量测试；记录 bug-014 phi_t 越界校验回归。不构成 GoldenSuite 门禁证据。 |
| `superpowers/specs/2026-06-13-m246-dpm-closure-laws-evidence.md` | M246 DPM closure_laws 参数一致性证据；新增 `dpm/closure_laws` 严格落地主 Spec `validate_dpm_consistency`（phi_t>=max 对角、phi_t<=1、正定、λ 下限/上限、条件数）；PreProc 期约束不接入热路径；记录放弃 `storage_exchange`（本 Spec 物理无此项）的范围决策，134/134 全量测试。不构成 GoldenSuite 门禁证据。 |
| `superpowers/specs/2026-06-13-m248-edge-classification-and-wb-pairing-gating-evidence.md` | M248 边界面分类与 WB 配对装配证据（原拟 M247，因并行会话占用改标）；新增 `dpm/edge_classification`（spec epsilon_omega=1e-4、phi_edge_min=0.01），统一 hard/soft/regular 判据，校正 hllc 临时阈值 1e-12→spec 值，并按 §5.6 在 hard-block 边门控 WB 配对；默认场零回归、135/135 全量测试。S_phi_t 接入动量留作后续任务。不构成 GoldenSuite 门禁证据。 |
| `superpowers/specs/2026-06-25-goldensuite-manifest-evidence.md` | GoldenSuite manifest + CI gate 证据；登记全 G1-G12 矩阵并激活 G1-G6 为 active CI gate，记录首轮 6/6 golden gate 与 manifest-completeness 通过；G7-G12 仍 pending。 |
| `docs/superpowers/specs/2026-06-27-g10-snapshot-replay-mass-deficit-evidence.md` | G10 Snapshot Replay Mass Deficit 证据；将 `snapshot_replay_mass_deficit` 从 manifest `pending` 提升为 `implemented`，以纯内存 CouplingLib core Golden 覆盖 aggregate/shared deficit、pending event fidelity、runtime counter replay 与 system mass audit 一致性；`ci_gate` 首轮保持 false。 |
| `superpowers/specs/2026-06-13-m249-well-balanced-wall-pressure-gap-evidence.md` | M249 静水 well-balancing 墙边界压力修复证据（已落地）；记录 lake-at-rest 伪加速 bug、根因（反射墙未施加 0.5gh² 压力）、落地修复、边诊断层测试迁移、启用 lake-at-rest 回归与 139/139 全量测试证据。bug-026 resolved。 |
| `superpowers/specs/2026-06-23-s-phi-t-momentum-coupling-evidence.md` | S_phi_t 接入动量（scope A）实施证据；HLLC 平流-only + `well_balanced` Audusse 单侧分裂配对（F_p/S_phi_t/平方差 S_topo）+ step 接线 + phi_t-缩放墙压；G4 平床 phi_t 跳变与 G5 变床静水均逐位 1e-12 well-balanced；含主 Spec §5.4 Audusse `reconstruct_hydrostatic_pair` 修复与 bed-step 夹具迁移。142/142 全量测试；动态 phi_t 跳变仍属 §5.5.2 验证空白区，不构成 GoldenSuite 门禁证据。 |
| `superpowers/specs/2026-06-13-m247-urban-runoff-generation-design.md` | M247 城市产流模块设计；规划 Surface2D 独占地表产流、Green-Ampt 首实现、CUDA-ready enum/SoA 分发、透水/不透水/屋面直排分链、roof direct-drain intent 经 CouplingLib 注入 SWMM lateral inflow、roof pending/overflow 守恒、fail-closed 与 Golden runoff urban block 候选。不构成实现或 GoldenSuite 门禁证据。 |
| `superpowers/specs/2026-06-12-m240-embedded-engine-extraction-and-tri-coupling-evidence.md` | M240 嵌入式引擎提取与三方两两双向耦合实施证据；记录 SWMM 5.2.4 solver 提取到 `extern/swmm5/`、BMI 契约快照、third_party 治理、真实 `SwmmEngine` 静态嵌入证据（真实算例 5/5）、`DFlowFMEngine` BMI 运行时加载 fail-closed 边界、core 1D-1D 接口交换原语、1D→2D 返流路径与 `libs/coupling/driver/` 三方耦合步进驱动；D-Flow FM 真实 kernel 步进证据仍 blocked（待外部构建 DLL）。 |

## 历史记录规则

- 根目录历史 Markdown、会话导出、早期方案评审稿与对话记录只作为非规范性背景材料，不得覆盖上表规范性入口。
- 若历史文档与规范性入口冲突，以本索引列出的规范性入口为准，并优先同步主 Spec、稳定性协议与符号表。
- 涉及 `Q_limit`、`V_limit`、`dt_sub_seconds`、`priority_weight`、`mass_deficit_account`、`epsilon_deficit`、`max_cell_cfl`、`C_rollback`、`CFL_safety` 的 machine-facing 定义，统一以符号表和主 Spec §11 为准。
