# OpenHBX TASK1：OCP HBF 产品实现总体设计方案

**文档版本**：V3.0

**修订日期**：2026-08-28

**项目名称**：OpenHBX

**TASK1产品profile**：`OCP_HBF_0_7`

**规范基线**：*OCP HBF Architecture Specification v0.7.0 FINAL*

**集成基线**：当前仓库`thirdparty/ramulator2`
**实现状态**：设计重构阶段，新增功能均为`PLANNED`

## 修订历史

| 版本 | 日期 | 修订说明 |
|---|---|---|
| V1.0 | 2026-08-23之前 | 定义OpenHBF单一HBF模拟器 |
| V2.0 | 2026-08-23 | 提出OpenHBX多产品可组合长期框架 |
| V3.0 | 2026-08-28 | 收敛TASK1：只实现OCP HBF v0.7.0；按8个功能域重建HLD/LLD，其他产品仅保留未来扩展边界 |

## 1. 任务定义

TASK1交付一个集成到Ramulator 2的、配置驱动、确定性、异步的OCP HBF模拟器。用户提供一个YAML配置文件，选择`OCP_HBF_0_7`产品profile及允许覆盖的产品参数，OpenHBX完成配置校验、对象图装配、请求执行、状态统计和结果输出。

本任务的“完成HBF”指：在OCP HBF v0.7.0及已声明外部依赖的边界内，形成从Ramulator/trace请求到Host协议、Base Die、地址管理、互联/PAL和NAND介质的唯一生产路径，并通过可重复测试验证可定位的协议行为。

### 1.1 本次必须交付

1. `OCP_HBF_0_7`配置profile、schema、派生参数和跨层能力校验；
2. Ramulator 2 `IMemorySystem`集成、统一`OpenHbxSystem`、cycle和EventQueue；
3. HBF Host Channel、UCIe Streaming抽象、虚拟AXI、packet/status和ordering；
4. Host local address、Channel ownership、A1/R1-R5和HBF有限地址状态；
5. Base Die DLU聚合、调度、ECC、Bank cache、Admin和lifecycle；
6. package/vertical interconnect、TSV资源和Flash PAL异步派发；
7. NAND组织、Read/Program/Erase、时序/EAT、payload、BBT和可靠性；
8. unit、component、system和可达范围内的OCP conformance验证。

### 1.2 本次不要求交付

- HBM2、HBM3、HBM4、LPDDR5、LPDDR6的Host、Controller或DRAM Media；
- Ramulator DRAMSpec适配和DRAM ACT/PRE/RD/WR/REF状态机；
- CXL、PCIe/NVMe、传统SSD Namespace或Block Device前端；
- hybrid bonding的物理精度模型和产品默认参数；
- 传统任意页级L2P/P2L、Trim、victim GC、wear-level relocation或active-data relocation；
- 因缺少UCIe v3.0、AMBA AXI或vendor profile而无法确定的bit-accurate行为。

上述内容可以在接口中留出扩展点，但不得写入TASK1完成条件，也不得以stub存在声称支持。

## 2. 依据、事实分类与合规边界

### 2.1 依据优先级

| 优先级 | 依据 | 用途 |
|---|---|---|
| P0 | `reference/OCP HBF Architecture Specification v0.7.0 FINAL.pdf` | HBF产品行为唯一规范基线 |
| P0辅助 | `reference/OCP_Specification.md`及`_EN.md` | 可检索定位，不替代PDF |
| P1 | 当前`thirdparty/ramulator2`源码 | `IMemorySystem`、Request、Factory、Config和Stats ABI |
| P2 | SimpleSSD、HFSSS | PAL、Media、EAT、事件与测试实现参考 |
| P3 | OpenHBX设计选择 | queue、数据容器、synthetic NAND参数和软件模块边界 |

每项重要需求在HLD/LLD中标记为规范事实、源码事实、OpenHBX设计选择、外部依赖或计划。无法从现有依据确认的内容使用`SPEC_EXTRACT_REQUIRED`、`BLOCKED_SPEC`或`EXTERNAL_DEPENDENCY`，不能由常识补齐。

### 2.2 外部依赖

OCP HBF PDF引用但未完整定义的UCIe v3.0 Format 6逐bit布局、credit/FDI/RDI、完整sideband framing，以及AMBA AXI基础协议属于外部依赖。TASK1可以建立transaction-level抽象、字段容器和测试接缝，但在外部规范证据缺失时不能声称bit-accurate conformance。

产品/vendor-specific的R1/R2/R3/R5几何、NAND`tR/tPROG/tERS`、可靠性、thermal和真实封装参数由profile提供。仓库内置值必须标记为OpenHBX synthetic defaults，不得写成OCP保证值。

### 2.3 建模粒度结论

TASK1采用系统与事务级HBF模型，目标是正确复现OCP可观察行为、Flash状态变化、资源竞争和simulation-cycle时序，不复现晶体管、电气波形或完整UCIe PHY。该粒度与HFSSS的模块化方法对齐：Host协议、控制器、PAL、NAND命令引擎、时序/EAT、可靠性和BBT均有独立owner、状态机及测试接缝，不使用一个固定延迟函数替代整条Flash路径。

以下最低行为不得进一步裁剪，否则属于过度简化：

- Host必须保留Channel隔离、64 B/4 KiB边界、packet/status、ordering、ready/reset和non-posted completion；UCIe/AXI采用transaction-level抽象，但不得绕过这些语义。
- Controller必须保留DLU聚合、overlap/timeout、read forwarding、同Bank read顺序、每Bank cache credit、ECC/retry、Page顺序和replay协作。
- PAL必须分别建模命令/数据传输、共享route资源、backpressure、TSV故障/备用路径和异步Media completion，不能合并成常量延迟。
- Media必须分别建模Read/Program/Erase、命令合法性、stage timing、Bank/Die EAT、terminal-time commit、payload、坏块和raw reliability，不能只维护计数器或只返回延迟。

NAND cell mode、Plane、多平面命令、Die内部总线、精确`tR/tPROG/tBERS`、热传导和电气链路只有在vendor profile提供来源时启用。它们不是OCP HBF主机可见合规的统一前提，也不应通过无来源默认值伪造物理精度。

## 3. HBF核心不变量

1. 一个HBF stack最多16个独立Host Channel；Channel只能访问自己的容量切片，不复制物理Core Die/Die/Bank容量。
2. 每Channel具有独立x64 full-duplex UCIe mainband抽象，并可暴露1、2或4个虚拟AXI Interface；具体能力由profile和规范表约束。
3. Host Flash transaction以64 B为基本粒度，不跨4 KiB DLU边界。
4. Base Die是pending DLU sector聚合的唯一owner；完整64个sector后才允许向Core Die发出4 KiB Program。
5. Flash Write为non-posted；成功response只能在NAND Program terminal completion之后产生。
6. 相同AXI ID response保序；不同ID可乱序；相同64 B地址的mixed read/write hazard跨ID仍必须保序。
7. Read Status `0x5`表示CECC且数据有效；`0x6`要求携带Read Retry信息；`0xA`表示pending DLU目标sector尚未到达。
8. A1以64 B为单位，4 KiB Page时R4固定为64；R1至R5计算使用checked arithmetic和显式单位。
9. 同一Block按Page顺序Program；Page-0触发内部auto-erase；Program Fail进入Host replay路径。
10. Zone Remap只改变mapping，不复制payload；正式profile不存在传统GC和active-data relocation。
11. issue只建立token、reservation和future event；Page、Block和payload权威状态在terminal completion提交。
12. Accepted请求精确一次terminal；Busy/Backpressured/Rejected零副作用；reset后的旧generation事件不能提交新状态。
13. 系统只有一个simulation time和EventQueue；任何模块不得使用私有tick或host wall-clock改变结果。
14. 物理层次为`Core Die -> Die -> Bank -> Block -> 4 KiB Page`；Channel是ownership维度，不是物理父节点。

## 4. 八模块总体划分

| 编号 | HLD/LLD设计域 | 回答的问题 | TASK1范围 |
|---|---|---|---|
| 01 | HBF产品配置与装配 | 用户选择什么产品，组件是否合法 | 只注册`OCP_HBF_0_7` |
| 02 | 系统集成与事件内核 | 请求怎样接入Ramulator并确定性推进 | `IMemorySystem`、对象图、EventQueue |
| 03 | OCP HBF Host协议 | Host怎样发送transaction并获得response | UCIe/AXI transaction-level HBF协议 |
| 04 | HBF地址与拓扑 | Host地址映射到哪里 | ownership、A1/R1-R5、有限mapping状态 |
| 05 | Base Die与Flash控制器 | 请求何时转成Flash工作并完成 | DLU、scheduler、cache、ECC、Admin |
| 06 | 互联、封装与Flash PAL | 命令和数据怎样到达目标介质资源 | package/TSV资源、FlashPhysicalRequest |
| 07 | NAND介质与HBF RAS | NAND命令怎样改变物理状态 | topology、stage、payload、BBT、reliability |
| 08 | 验证与一致性 | 怎样证明上述实现满足契约 | requirement、oracle、artifact、E1-E4 |

本次建立以下一一对应的文档：

| 编号 | HLD | LLD |
|---|---|---|
| 01 | `HLD_01_HBF_PRODUCT_CONFIGURATION.md` | `LLD_01_HBF_PRODUCT_CONFIGURATION.md` |
| 02 | `HLD_02_SYSTEM_INTEGRATION_EVENT_KERNEL.md` | `LLD_02_SYSTEM_INTEGRATION_EVENT_KERNEL.md` |
| 03 | `HLD_03_OCP_HBF_HOST_PROTOCOL.md` | `LLD_03_OCP_HBF_HOST_PROTOCOL.md` |
| 04 | `HLD_04_HBF_ADDRESS_TOPOLOGY.md` | `LLD_04_HBF_ADDRESS_TOPOLOGY.md` |
| 05 | `HLD_05_BASE_DIE_FLASH_CONTROLLER.md` | `LLD_05_BASE_DIE_FLASH_CONTROLLER.md` |
| 06 | `HLD_06_INTERCONNECT_FLASH_PAL.md` | `LLD_06_INTERCONNECT_FLASH_PAL.md` |
| 07 | `HLD_07_NAND_MEDIA_RAS.md` | `LLD_07_NAND_MEDIA_RAS.md` |
| 08 | `HLD_08_VERIFICATION_CONFORMANCE.md` | `LLD_08_VERIFICATION_CONFORMANCE.md` |

## 5. 唯一总体架构

```text
YAML / Ramulator / Trace / Synthetic Driver
                     |
                     v
       01 HBF Product Configuration
       schema / profile / resolved config
                     |
                     v
  02 OpenHbxSystem + Deterministic EventKernel
      RequestBridge / cycle / event / callback
                     |
                     v
          03 OCP HBF Host Protocol
     Host Channel / UCIe / AXI / ordering
                     |
                     v
          04 HBF Address & Topology
 ownership / A1-R5 / BlockSequence / ZoneMap
                     |
                     v
       05 Base Die & Flash Controller
 DLU / scheduler / cache / ECC / Admin / lifecycle
                     |
                     v
       06 Interconnect & Flash PAL
 package / TSV / route / resource EAT / dispatch
                     |
                     v
          07 NAND Media & HBF RAS
 CoreDie / Die / Bank / Block / Page / payload
                     |
                     v
       terminal completion and reverse events

  08 Verification observes every public boundary
```

所有入口最终调用`OpenHbxSystem::try_submit()`。Ramulator adapter只复制并转换`Request`，不得建立固定延迟、第二套EventQueue、第二套Host/Controller/PAL/Media路径。反向completion沿窄port和event返回，只有02 RequestBridge调用原Ramulator callback。

## 6. 模块职责与边界

### 6.1 01 HBF产品配置与装配

负责读取YAML、合并`OCP_HBF_0_7`默认profile与用户override、验证schema和跨层能力、计算派生值、输出immutable resolved config，并由单一composer创建02至07对象图。

TASK1不需要通用动态插件市场。可以采用typed factory/registry为未来产品留接口，但运行时只允许HBF组件组合。配置未知字段、非法单位、Host/Controller/Media能力不匹配或正式profile启用GC时启动失败。

### 6.2 02 系统集成与事件内核

负责Ramulator 2 `IMemorySystem`适配、Request副本和callback lifetime、唯一`OpenHbxSystem`、全局cycle/EventQueue、token/generation、event phase、reset编排、drain和系统Stats聚合。

02不拥有Host ordering、DLU、地址mapping、TSV或NAND状态。`send(false)`及`try_submit(Backpressured)`零副作用；Accepted后callback精确一次且先设置`depart`。

### 6.3 03 OCP HBF Host协议

负责最多16个Host Channel、UCIe Streaming transaction-level链路、虚拟AXI Interface、USER packet type、请求合法性、same-ID response order、mixed-address hazard、sideband/register访问入口和最终response gate。

03不拥有DLU聚合、A1/R1-R5、系统clock、Ramulator callback或NAND状态。外部UCIe/AMBA缺失部分以capability和`BLOCKED_SPEC`隔离。

### 6.4 04 HBF地址与拓扑

负责Channel local space、Channel-to-capacity ownership、结构化HBF地址、A1/R1-R5 checked mapping、Block expected-page/Page-0规则、Host replay地址序列、ZoneMap、Reduced Capacity和retirement view。

04不是传统SSD FTL：不实现任意L2P/P2L、allocator、Trim、GC、WL relocation或active refresh copy。04不拥有Page payload、DLU accumulator、Media EAT或Host callback。

### 6.5 05 Base Die与Flash控制器

负责唯一`DluAccumulator`、64-sector mask/timeout/overlap/forward、Flash request scheduler、每Bank至少两个4 KiB cache slot、ECC encode/decode、Read Retry协调、Scratchpad、Register/Admin和lifecycle。

05根据04的结构化目标向06提交异步物理请求，但不直接修改NAND状态。TSV lane、mapping、spare和repair FSM归06；NAND Page/Block、raw reliability和BBT归07。

### 6.6 06 互联、封装与Flash PAL

PAL（Physical Abstraction Layer）是OpenHBX/SimpleSSD参考的软件边界，不是OCP HBF规范术语。06负责把05的`FlashPhysicalRequest`转换为package/vertical transfer与07介质命令，预约TSV/route/resource EAT，处理backpressure、token correlation、transfer completion、TSV fault/spare/repair和typed physical completion。

TASK1基线只要求可配置的TSV transaction-level profile。`TSV + hybrid bonding`仅保留为未来/vendor profile接口，没有参数来源时为`EXTERNAL_DEPENDENCY`，不属于HBF协议完成门槛。

06不拥有Page/Block/payload权威状态，不决定Host replay或Zone Remap，也不实现NAND内部Program/Erase状态机。

### 6.7 07 NAND介质与HBF RAS

负责物理层次、Page/Block状态、持久payload、Read/Program/Erase阶段、NAND timing、Bank/Die/array EAT、completion-time commit、raw error、BBT、PEC、read counter、refresh notice、Die状态和thermal观测。

RAS表示Reliability、Availability、Serviceability。07拥有介质故障状态；05拥有Host可见策略；06拥有TSV故障状态。三者通过typed completion和snapshot协作，不能复制状态机。

### 6.8 08 验证与一致性

负责需求到设计、实现、Test ID和artifact的闭环；提供配置、Host协议、mapping、DLU、PAL、Media、reset/fault、determinism、driver differential和性能ceiling测试。验证代码只观察公开接口，fake只替代外部sink或受控fault source。

08不得用简化fake pipeline证明完整HBF系统合规，也不得把外部规范缺口标记为已关闭。

## 7. 跨模块公共契约

### 7.1 结果类型

所有有界异步端口使用三态结果：

```text
Accepted(token) : 所有权按接口契约转移，未来精确一次completion
Busy            : 调用方保留请求，callee零副作用，可在ready后重试
Rejected(status): callee零future event，返回不可重试或typed错误
```

不得用`bool`跨越内部模块边界而丢失Busy与Rejected差异。Ramulator `send()`因ABI限制仍返回`bool`，由02把typed结果映射为`true/false`。

### 7.2 稳定标识与所有权

跨模块对象使用`TxnId`、`Token`、`Generation`、typed address和`PayloadHandle`。EventQueue中不保存指向可回收事务的裸pointer。只有Accepted时才能转移payload所有权；Busy/Rejected后由调用者继续持有。

### 7.3 提交点

```text
admission -> reserve -> issue -> transfer -> media stage
          -> terminal media completion -> commit -> response
```

NAND Program成功时才原子提交Page payload和状态；Erase verify成功时才清除Block payload并增加PEC；Read issue不返回未来snapshot。04的expected-page推进与07 Program成功completion一致提交或通过明确两阶段协议协调。

### 7.4 Reset与generation

Reset提升受影响scope的generation。旧event可以被取消或由orphan sink吸收，但不能修改新generation状态；每个已Accepted Host transaction最终仍获得一次明确terminal结果。reset域和精确外部信号时序若规范证据不足，保持`BLOCKED_SPEC`。

## 8. HBF主要流程

### 8.1 Read

```text
Ramulator/Driver submit
 -> 02 RequestBridge admission
 -> 03 Host/UCIe/AXI validation and ordering
 -> 04 address + ownership mapping
 -> 05 pending-DLU forwarding or Bank cache lookup
 -> miss: 06 PAL route/TSV transfer and Media issue
 -> 07 NAND sense/reliability/payload snapshot
 -> 06 return transfer
 -> 05 ECC/retry/cache fill/sector selection
 -> 03 response ordering and UCIe TX
 -> 02 depart + exactly-once callback
```

pending DLU中目标sector已收到时05直接forward；DLU存在但sector未收到时返回Read Status `0xA`，不得读取旧NAND payload。

### 8.2 Write

```text
64 B Host write
 -> 03 Host admission/order
 -> 04 ownership/address validation
 -> 05 PendingDlu accumulate
 -> incomplete: retain waiter until ready/timeout
 -> complete 4 KiB: ECC + sequence check
 -> 06 data transfer + PAL issue
 -> 07 NAND Program/verify
 -> success: Page payload/state commit
 -> 04 expected-page commit
 -> 05/03 response
 -> 02 callback
```

duplicate sector、最大pending DLU和timeout分别映射规范typed status。Program Fail不推进expected-page，Block进入replay状态，由Host按04生成的序列重放。

### 8.3 Page-0、Erase与Replay

Page-0 Program触发内部auto-erase序列。07只在Erase verify成功后提交Block清空；随后才允许Page-0 Program。Program Fail后04锁定正常推进并输出replay范围；Zone Remap只改变mapping，不能要求07复制旧payload。

### 8.4 Admin、Register与Lifecycle

03的ControlPlane处理规范表9 Admin opcode和Tables 15/16 register schema，长操作使用异步event。涉及Zone/retirement的命令委托04，涉及TSV诊断的命令委托06，涉及BBT/temperature的查询读取07 snapshot。READY只有在必要Host、Controller、PAL和Media readiness全部成立后置位。

## 9. 配置基线

```yaml
OpenHBX:
  schema_version: 1
  product: OCP_HBF_0_7

  system:
    impl: OpenHbxSystem
    clock_ratio: 1
    event_capacity: 65536

  host:
    impl: HbfUcieAxi
    host_channels: 16
    axi_interfaces_per_channel: 4
    transaction_bytes: 64

  address:
    impl: HbfR1ToR5
    dlu_bytes: 4096
    r1: SPEC_EXTRACT_REQUIRED
    r2: SPEC_EXTRACT_REQUIRED
    r3: SPEC_EXTRACT_REQUIRED
    r4: 64
    r5: SPEC_EXTRACT_REQUIRED

  controller:
    impl: HbfBaseDie
    pending_dlu_per_channel: 128
    bank_cache_slots: 2

  interconnect:
    impl: HbfFlashPal
    vertical_link: TsvSyntheticBaseline

  media:
    impl: NandFlash
    profile: OpenHbxTlcSynthetic
    payload_mode: SparsePayload

  formal_profile:
    garbage_collection: false
    active_relocation: false
    trim: false
```

示例中的容量、queue和synthetic profile值是OpenHBX设计选择。正式产品运行必须为R1/R2/R3/R5提供有来源值；不能把`SPEC_EXTRACT_REQUIRED`字符串作为运行时数值接受。

## 10. 配置与能力校验

启动前至少检查：

1. `product`严格为TASK1支持的`OCP_HBF_0_7`；
2. Host、Mapper、Controller、PAL和Media均声明HBF/Flash兼容能力；
3. Host Channel为1至16，AXI数量与speed grade/capability一致；
4. transaction为64 B，DLU为4096 B且R4为64；
5. ownership mapping覆盖每个Bank一次且不存在跨Channel访问；
6. R1至R5、容量和地址乘法无溢出；
7. queue、credit、event和payload容量为正且下一阶段可预留；
8. PAL route endpoint与CoreDie/Die topology一致；
9. Media支持Read/Program/Erase及所需4 KiB payload；
10. 正式profile未启用GC、Trim、任意L2P或active relocation；
11. standard、vendor、synthetic和外部依赖参数均携带来源标签；
12. payload mode满足选定验证oracle。

错误报告包含配置路径、实际值、期望约束和关联requirement，尽量一次报告全部独立错误。成功解析后保存`resolved-config.yaml`和capability manifest。

## 11. 时间、资源与性能

02使用整数cycle和稳定`phase,sequence`排序。跨组件duration显式标注clock domain并换算到系统cycle。Host link、TSV、Bank array、Die path、ECC engine、cache slot、queue和event reservation均为有界资源。

性能结论必须拆分：

- Host理论上限：由Host Channel数、UCIe速率、lane宽度、duplex和协议效率推导；
- PAL/TSV上限：由有效bits/cycle、route共享组和serialization推导；
- NAND上限：由Bank并行度、Read/Program/Erase时序和command mix推导；
- 系统上限：上述瓶颈的最小值，并说明cache hit路径是否绕过Media。

OCP未规定的NAND和封装参数只用于synthetic实验。不得用host wall-clock替代simulation cycle，也不得重复相加Host bytes、cache-served bytes和Media bytes。

## 12. 错误、RAS与可观测性

错误在最接近来源的模块产生，并由唯一映射点转换：

| 来源 | owner | 示例 | 上层处理 |
|---|---|---|---|
| Host协议 | 03 | reserved packet、alignment、ordering violation | Host typed response |
| 地址/顺序 | 04 | out-of-range、wrong owner、replay required | HBF status/Admin状态 |
| Controller | 05 | overlap、MPDLU、timeout、ECC result | Read/Write status |
| Interconnect/PAL | 06 | Busy、route offline、TSV degraded | retry、offline或Admin告警 |
| NAND Media | 07 | CECC/UECC、Program/Erase Fail、bad block | 05/04策略处理 |

每个event/trace至少包含cycle、sequence、token、generation、module、operation、stage、typed address、result和resource ID。snapshot不得暴露可写内部引用。日志溢出使用drop-and-count，启用日志不能改变事件顺序或RNG。

## 13. 验证策略

### 13.1 证据层级

- E1：单个Mapper、Accumulator、TSV resource或NAND state组件；
- E2：Host/Controller、Controller/PAL、PAL/Media等真实组件边界；
- E3：真实`OpenHbxSystem`从driver到terminal response；
- E4：OCP vector、Ramulator ABI差分和理论resource ceiling。

Fake最多替代外部sink、可控clock-independent input或fault source。Fake Host、Controller、PAL或Media组成的pipeline不能支持E3/E4结论。

### 13.2 必须覆盖的系统行为

1. `send(false)`和内部Busy零副作用；
2. Accepted精确一次terminal/callback并最终drain；
3. 64 B边界、burst不跨DLU、same-ID与mixed hazard；
4. 64-sector任意代表顺序、overlap、MPDLU、timeout和forward；
5. A1/R1-R5非对称geometry、ownership、Page-0和replay序列；
6. 每Banksense顺序、跨Bank并行、双cache slot和Batch边界；
7. PAL/TSV serialization、fault、repair epoch和backpressure；
8. 4 KiB逐bytepayload、Program/Erase completion-time commit；
9. CECC data-valid、UECC data-invalid、retry和Program Fail；
10. Zone Remap无Media copy，正式binary无GC/active relocation；
11. reset phase sweep、stale event、determinism和资源守恒；
12. trace、synthetic和Ramulator driver可观察结果一致。

实验执行、证据保存和状态更新统一遵循
[`实验与测试执行规范.md`](实验与测试执行规范.md)。

## 14. 实施顺序

### Phase A：冻结文档与公共契约

- 完成8组HLD/LLD、需求ID、接口、owner和Test ID；
- 更新README、文件索引和TRACEABILITY_MATRIX；
- 关闭文档内部冲突，外部规范缺口进入阻断队列。

### Phase B：配置、内核与介质基础

- 实现01 schema/profile/composer；
- 实现02 EventKernel、token/generation和RequestBridge；
- 实现07 NAND topology、payload和异步Read/Program/Erase。

### Phase C：地址、PAL与控制器

- 实现04 checked mapping、sequence/replay/ZoneMap；
- 实现06 PAL、TSV资源和Media adapter；
- 实现05 DLU、scheduler、cache、ECC和ControlPlane。

### Phase D：Host与全路径集成

- 实现03 Host Channel、UCIe/AXI transaction模型、ordering和response；
- 装配唯一生产pipeline；
- 接通Ramulator、trace和synthetic driver。

### Phase E：一致性与性能证据

- 完成08的E1至E4测试；
- Docker内执行CMake/CTest、sanitizer、determinism和resource ceiling实验；
- 只依据实际artifact更新状态。

## 15. TASK1完成定义

只有同时满足以下条件，才能声明TASK1在其已声明范围内完成：

1. 8组HLD/LLD及追踪矩阵一致，所有生产文件和测试有唯一owner；
2. 用户可通过一个YAML选择`OCP_HBF_0_7`并生成可审计resolved config；
3. 所有入口经过唯一`OpenHbxSystem`、Host、Address、Controller、PAL和NAND路径；
4. 第3章HBF核心不变量全部具备自动oracle和相应证据；
5. 正式profile不存在HBM/LPDDR、传统GC、Trim、任意L2P或active relocation路径；
6. completion-time commit、reset generation、exactly-once及资源守恒通过E3；
7. 可定位的OCP packet/status/register/mapping行为通过E4 vector；
8. Ramulator adapter ABI、backpressure和callback通过差分/集成测试；
9. 所有正式验证在Docker中通过CMake/CTest执行并保存合规artifact；
10. 外部UCIe/AMBA/vendor缺口仍明确列出，发布声明不超过证据覆盖范围。

文档生成、代码编译、单个fake测试或synthetic参数实验均不能单独构成TASK1完成证据。

## 16. 后续扩展边界

OpenHBX长期可以增加HBM、LPDDR和其他介质，但必须新增独立产品profile、Host、Mapper、Controller和Media能力，不能复用HBF Flash状态机冒充DRAM。02 EventKernel、01配置框架和部分06互联基础设施可以复用；HBF专用DLU、Program/Erase、replay和BBT保持产品隔离。

hybrid bonding、先进interposer和更精细thermal模型作为后续profile演进，不影响TASK1的OCP HBF协议主路径与验收。
