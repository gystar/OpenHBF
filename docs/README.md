# OpenHBX TASK1设计文档

OpenHBX是面向可配置高带宽存储产品的模拟框架。本仓库当前TASK1只交付`OCP_HBF_0_7`产品profile，即集成到Ramulator 2的OCP High Bandwidth Flash（HBF）模拟器；HBM、LPDDR和hybrid bonding精细模型不属于本次实现或验收范围。

HBF产品行为以`reference/OCP HBF Architecture Specification v0.7.0 FINAL.pdf`为唯一规范基线。Ramulator 2决定集成ABI；SimpleSSD-Standalone和HFSSS只作为PAL、Media、异步事件和测试组织参考。

## 阅读顺序

1. [`任务更正方案.md`](任务更正方案.md)：交付目标、规范优先级和协作门槛。
2. [`TASK1-整体设计方案.md`](TASK1-整体设计方案.md)：HBF-only范围、8模块架构、跨模块不变量和完成定义。
3. [`设计文档编写规范.md`](设计文档编写规范.md)：HLD/LLD结构、证据、接口和状态写法。
4. [`实验与测试执行规范.md`](实验与测试执行规范.md)：构建、执行、artifact、重放和状态更新门禁。
5. HLD：阅读模块需求、边界、架构、主流程和验收目标。
6. LLD：阅读文件、类型、API、状态机、算法和测试接缝。
7. [`TRACEABILITY_MATRIX.md`](TRACEABILITY_MATRIX.md)：核对规范要求到设计和Test ID的闭环。
8. [`OpenHBF-文件安排.md`](OpenHBF-文件安排.md)：核对生产与测试路径的唯一owner。
9. [`TASK1-代码实施顺序.md`](TASK1-代码实施顺序.md)：按S0至S9串行实现、验证和Agent交接。
10. [`OPENHBX_LOGGING_AND_OBSERVABILITY.md`](OPENHBX_LOGGING_AND_OBSERVABILITY.md)：日志、结构化事件、Ramulator调研与artifact边界。

所有HLD/LLD均在元数据中声明自身`docs/...`位置，并在正文列出计划中的公开头文件、生产实现、测试、配置和artifact根。路径表示设计落点，不等同于文件已实现。

## 状态图例

| 图标 | 状态 | 含义 |
|---|---|---|
| ⚪ | `PLANNED` | 只有设计 |
| 🟡 | `PARTIAL` | 部分生产路径完成 |
| 🔵 | `IMPLEMENTED` | 生产代码完成但证据不足 |
| 🟢 | `VERIFIED` | 已有合规自动验证证据 |
| 🔴 | `BLOCKED_SPEC` | 规范原文缺失导致阻断 |
| 🟣 | `EXTERNAL_DEPENDENCY` | 依赖外部规范或vendor profile |
| ⚫ | `NOT_APPLICABLE` | 明确不在适用范围 |

图标用于快速浏览，反引号中的状态文本才是权威值；状态升级必须遵循实验规范。

## 文档清单

| 序号 | HLD | LLD | 设计域 |
|---|---|---|---|
| 01 | `HLD_01_HBF_PRODUCT_CONFIGURATION.md` | `LLD_01_HBF_PRODUCT_CONFIGURATION.md` | HBF profile、schema、resolved config、capability与对象图装配 |
| 02 | `HLD_02_SYSTEM_INTEGRATION_EVENT_KERNEL.md` | `LLD_02_SYSTEM_INTEGRATION_EVENT_KERNEL.md` | Ramulator adapter、OpenHbxSystem、cycle/EventQueue、callback与drain |
| 03 | `HLD_03_OCP_HBF_HOST_PROTOCOL.md` | `LLD_03_OCP_HBF_HOST_PROTOCOL.md` | Host Channel、UCIe、AXI、packet、ordering与response |
| 04 | `HLD_04_HBF_ADDRESS_TOPOLOGY.md` | `LLD_04_HBF_ADDRESS_TOPOLOGY.md` | Channel ownership、A1/R1-R5、BlockSequence、replay与ZoneMap |
| 05 | `HLD_05_BASE_DIE_FLASH_CONTROLLER.md` | `LLD_05_BASE_DIE_FLASH_CONTROLLER.md` | DLU、scheduler、cache、ECC、Admin与lifecycle |
| 06 | `HLD_06_INTERCONNECT_FLASH_PAL.md` | `LLD_06_INTERCONNECT_FLASH_PAL.md` | package/TSV资源、Flash PAL、异步物理请求与repair |
| 07 | `HLD_07_NAND_MEDIA_RAS.md` | `LLD_07_NAND_MEDIA_RAS.md` | NAND topology、Read/Program/Erase、payload、BBT与可靠性 |
| 08 | `HLD_08_VERIFICATION_CONFORMANCE.md` | `LLD_08_VERIFICATION_CONFORMANCE.md` | unit、integration、system、conformance、fault与性能证据 |

HLD与LLD必须一一对应。旧的六模块HLD/LLD已被本清单取代，不再作为权威设计来源。

## 唯一主路径

```text
YAML / Ramulator / Trace / Synthetic
  -> 01 Configuration
  -> 02 OpenHbxSystem / EventKernel
  -> 03 OCP HBF Host Protocol
  -> 04 Address & Topology
  -> 05 Base Die Controller
  -> 06 Interconnect & Flash PAL
  -> 07 NAND Media & RAS
  -> terminal completion
```

08 Verification通过公开接口观察整条生产路径。任何driver、adapter或测试都不得建立第二套固定延迟、EventQueue、Controller、PAL或Media。

## 证据类别

- **OCP已确认**：可在PDF原文或仓库可检索基线中定位。
- **Ramulator已确认**：可在当前`thirdparty/ramulator2`源码中定位ABI或生命周期。
- **OpenHBX设计选择**：OCP未强制的queue、算法、synthetic profile或软件边界。
- **`SPEC_EXTRACT_REQUIRED`**：当前证据不足，正式合规前必须闭环。
- **`EXTERNAL_DEPENDENCY`**：依赖UCIe、AMBA或vendor/product profile。

设计文档存在只证明方案已描述。实现和验证状态必须依据[`实验与测试执行规范.md`](实验与测试执行规范.md)更新。

## 核心术语

Host Channel是同一物理NAND stack上的互斥容量ownership维度，不是复制Core Die/Die/Bank的物理父节点。物理层次固定为`Core Die -> Die -> Bank -> Block -> 4 KiB Page`，Host Channel通过显式mapping访问自己的Bank/Plane容量切片。详见[`HBF_CHANNEL_CORE_DIE_TOPOLOGY.md`](HBF_CHANNEL_CORE_DIE_TOPOLOGY.md)。

PAL表示OpenHBX内部的Physical Abstraction Layer软件边界，并非OCP HBF规范术语。它负责物理请求、互联资源和异步Media派发，不拥有NAND Page/Block/payload权威状态。
