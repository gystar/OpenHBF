# OpenHBX TASK1代码实施顺序与Agent交接规范

**文档版本**：V1.0  
**文档位置**：`docs/TASK1-代码实施顺序.md`  
**适用范围**：`OCP_HBF_0_7`生产代码、配置、CMake和测试  
**实施状态**：🟢 `VERIFIED`（S0-S9分阶段门禁完成；完整OCP仍受外部规范阻断）

## 1. 目标与执行原则

本文把8模块设计转换为可串行实施的代码阶段。Agent一次只执行一个已解锁阶段；当前阶段通过规定门禁并留下交接记录后，下一阶段才能开始。禁止多个Agent同时定义公共类型、EventQueue、地址语义或跨模块端口。

正式验证统一遵循[`实验与测试执行规范.md`](实验与测试执行规范.md)：默认在Conda环境中通过CMake/CTest执行，Docker可用于固定工具链复现；日常构建进入`build/tests/`，隔离变体进入`build/tests-<profile>/`，证据进入`build/artifacts/<suite>`。

实施遵循以下不变量：

- 新生产代码按`config/system/integration/hbf/address/controller/interconnect/pal/media/nand/ras`分类，不继续扩大旧`src/ftl/`和扁平`src/media/`。
- 跨模块只依赖`include/openhbx/`公开值类型和窄端口；私有头保留在对应`src/<domain>/`。
- 先冻结值类型、所有权和错误语义，再实现状态机；禁止由下游重新定义token、cycle、payload或status。
- 每阶段先完成E1，再完成必要E2；fake只能替代尚未实现的相邻公开端口。
- 旧实现只可逐符号迁移和验证，不可通过兼容wrapper把旧FTL/fixed-latency pipeline重新包装成新生产路径。

## 2. 串行依赖

```text
S0 Baseline and skeleton
 -> S1 Common contracts and EventKernel
 -> S2 Configuration and composition shell
 -> S3 NAND Media and RAS
 -> S4 Address and topology
 -> S5 Interconnect and Flash PAL
 -> S6 Base Die controller
 -> S7 OCP HBF Host protocol
 -> S8 OpenHbxSystem, drivers and Ramulator adapter
 -> S9 System/conformance evidence
```

04地址是05的规则协作者，不是`05 -> 06 -> 07`数据转发节点。生产请求路径最终为`02 -> 03 -> 05 -> 06 -> 07`，05在Program/Admin路径旁路查询或预约04。

### 2.1 实际阶段状态

| 阶段 | 状态/最高证据 | 权威artifact |
|---|---|---|
| S0 | 🟢 `VERIFIED`（E0） | `build/artifacts/stages/s0/EXP-S0-BUILD-CONTRACT/20260828T145000Z/` |
| S1 | 🟢 `VERIFIED`（E1） | `build/artifacts/stages/s1/EXP-S1-EVENT-KERNEL/20260828T145833Z/` |
| S2 | 🟢 `VERIFIED`（E1/E2） | `build/artifacts/stages/s2/EXP-S2-HBF-CONFIG/20260828T150916Z/` |
| S3 | 🟢 `VERIFIED`（E1/E2） | `build/artifacts/stages/s3/EXP-S3-NAND-MEDIA/20260828T160000Z/`；旧`153000Z`废止 |
| S4 | 🟢 `VERIFIED`（E1/E2） | `build/artifacts/stages/s4/EXP-S4-HBF-ADDRESS/20260828T153231Z/` |
| S5 | 🟢 `VERIFIED`（E1/E2） | `build/artifacts/stages/s5/EXP-S5-INTERCONNECT-PAL/20260830T074707Z/` |
| S6 | 🟢 `VERIFIED`（E1/E2） | `build/artifacts/stages/s6/EXP-S6-BASE-DIE-CONTROLLER/20260830T081326Z/` |
| S7 | 🟢 `VERIFIED`（E1/E2 transaction-level） | `build/artifacts/stages/s7/EXP-S7-OCP-HBF-HOST/20260830T084719Z/` |
| S8 | 🟢 `VERIFIED`（E3） | `build/artifacts/stages/s8-current/EXP-S8-OPENHBX-SYSTEM/20260830T101705Z/`；旧`build-s8`证据废止 |
| S9 | 🟢 `VERIFIED`（E2、E3及E4候选字段证据） | `build/artifacts/stages/s9/EXP-S9-CONFORMANCE/20260830T103513Z/` |

这里的S9阶段完成只表示已提取transaction-level字段和发布审计通过，不表示Format 6、AXI wire、vendor数值、hybrid bonding、Ramulator payload或完整OCP一致性已闭环。

S6和S7表中的artifact是其阶段结束时生成的历史组件E1/E2 binary证据；后续S8修复了跨Die geometry与FinalDelivery reset rebase等集成契约，因此不能用S6/S7旧binary代表当前最终源码。最终集成语义由`build/artifacts/stages/s8-current/.../20260830T101705Z/`和本表S9 artifact覆盖，组件历史证据仍只证明其当时明确列出的局部oracle。

## 3. 阶段定义

### S0 基线、目录与构建骨架

**输入**：全部HLD/LLD、当前旧六模块代码和CMake。  
**修改范围**：顶层及各模块`CMakeLists.txt`、新目录、测试manifest；不实现业务状态机。

交付：

- 建立`src/config`、`src/system`、`src/hbf/{host,address,controller}`、`src/interconnect`、`src/pal`、`src/media/nand`和`src/ras`；
- 建立对应`include/openhbx/...`和`tests/unit|component|system|conformance/...`；
- 每个生产target显式列source，统一链接warning target；
- 旧target继续可构建但标为legacy，不进入新`OpenHBX`生产聚合target；
- 增加只检查公开头和target链接关系的public-api测试。

门禁：Docker内configure、build和`ctest -L openhbx_public_api`通过；源码树无生成物。

### S1 公共契约与EventKernel

**Owner**：02。  
**修改范围**：`include/openhbx/common/`、`include/openhbx/system/`、`src/common/`、`src/system/`、`tests/unit/system/`。

交付：strong `Cycle/Token/Generation/TxnId`、typed `Accepted/Busy/Rejected`、payload handle、checked arithmetic、唯一EventQueue、completion registry、generation reset和drain/snapshot基础。不得依赖Host、Controller或Media内部类型。

门禁：`SYS-T01/02/03/04/06/07`的E1测试通过；同cycle排序、stale event、exactly-once和`accepted=terminal+outstanding`具备自动oracle。

### S2 配置与装配外壳

**Owner**：01。  
**修改范围**：`include/openhbx/config/`、`src/config/`、`configs/products/`、`tests/unit/config/`、`tests/component/config/`。

交付：严格schema、`OCP_HBF_0_7` profile、来源标记、checked派生、capability descriptor、resolved config/hash和事务式composer外壳。Composer此阶段可接收显式测试builder，但不得内建fake运行路径。

门禁：`CFG-T01`至`CFG-T12`中不依赖未实现组件的E1/E2测试通过；HBM/LPDDR、未知字段、非法单位和无来源vendor参数启动失败。

### S3 NAND Media与RAS

**Owner**：07。  
**修改范围**：`include/openhbx/media/`、`include/openhbx/media/nand/`、`src/media/nand/`、`src/ras/`、`tests/unit/media/nand/`、`tests/unit/ras/`、`tests/component/media/`。

交付：NAND topology、Page/Block状态、Sparse/TimingOnly payload、Read/Program/Erase stage、timing profile、Bank/Die EAT、terminal commit、BBT、raw reliability和environment。Plane/Multi-Plane只在capability启用时存在。

门禁：`VER-MEDIA-001`至`007`达到规定E1/E2；Program/Erase失败零提交、Read snapshot、同Bank串行/跨Bank并行、bad block和reset oracle通过。

### S4 HBF地址与拓扑

**Owner**：04。  
**修改范围**：`include/openhbx/hbf/address/`、`src/hbf/address/`、`tests/unit/hbf/address/`、`tests/component/hbf/address/`。

交付：strong address units、A1/R1-R5 checked mapping、profile-driven Channel ownership、reverse mapping、BlockSequence reservation/replay和ZoneMap。`bank % channel_count`只能位于synthetic fixture/profile。

门禁：`ADDR-T01`至`ADDR-T09`达到E1/E2；非对称geometry、overflow、wrong owner、Page顺序、replay和Zone remap无payload copy通过。

### S5 互联与Flash PAL

**Owner**：06。  
**修改范围**：`include/openhbx/interconnect/`、`include/openhbx/pal/`、`src/interconnect/`、`src/pal/`、`tests/unit/interconnect/`、`tests/component/pal/`。

交付：route/profile、atomic EAT reservation、TSV active/spare/repair epoch、PAL FSM、Media Busy重试、forward/media/return分段和typed completion。PAL只能通过07公开端口访问Media。

门禁：`VER-PAL-001`至`006`的E1/E2通过；route共享串行、独立并行、Busy零副作用、fault/spare和reset资源回收通过。

### S6 Base Die Flash Controller

**Owner**：05。  
**修改范围**：`include/openhbx/hbf/controller/`、`src/hbf/controller/`、`tests/unit/hbf/controller/`、`tests/component/hbf/controller/`。

交付：DLU accumulator、overlap/limit/timeout、read forwarding、Regular/Batch scheduler、Bank cache、ECC/status mapping、Admin/Scratchpad/lifecycle，以及04 reservation与06 PAL端口接线。

门禁：`CTRL-T01`至`CTRL-T11`达到E1/E2；64-sector排列、0x2/0x4/0x5/0x7/0x8/0x9/0xA、data-valid、双cache、PAL Busy和Program terminal顺序通过。

### S7 OCP HBF Host协议

**Owner**：03。  
**修改范围**：`include/openhbx/hbf/host/`、`src/hbf/host/`、`tests/unit/hbf/host/`、`tests/component/hbf/host/`。

交付：Channel gate、transaction-level UCIe/AXI、packet/register/sideband表、Host validator、ordering scoreboard和response codec/router。逐bit Format 6保持`BLOCKED_SPEC`，不得阻塞transaction-level主路径。

门禁：`HOST-T01`至`HOST-T10`达到E1/E2；Channel隔离、64 B/4 KiB、reserved packet、same-ID、同地址hazard、non-posted write、完整status和reset gate通过。

### S8 系统装配、Driver与Ramulator

**Owner**：02与01的公开装配边界。  
**修改范围**：`src/system/`、`src/integration/`、`apps/`、`configs/products/`、`tests/system/`。

交付：唯一`OpenHbxSystem`对象图、RequestBridge/callback、trace与synthetic driver、Ramulator `IMemorySystem` adapter、Stats bridge、reset/drain和resolved-config artifact。禁止第二套固定延迟路径。

门禁：Docker内真实01至07生产组件E3通过；Ramulator adapter启用和禁用两种build均通过；write-read payload、callback exactly-once、reset phase sweep和三次determinism digest通过。

### S9 Conformance与发布证据

**Owner**：08。  
**修改范围**：`tests/verification/`、`tests/conformance/`、artifact schema和状态文档；不修改生产语义以迎合测试。

交付：OCP可提取vector、负向status矩阵、resource ceiling、fault/reset、manifest/replay和未关闭spec-gap清单。

门禁：适用E4通过；外部UCIe/AMBA/vendor缺口仍保持`BLOCKED_SPEC`或`EXTERNAL_DEPENDENCY`；只按实际证据更新状态。

## 4. Agent串行交接

每个Agent开始前必须读取`docs/README.md`、实验规范、本阶段HLD/LLD及上一阶段交接记录。完成时在`build/artifacts/<suite>/<experiment-id>/<run-id>/`保存证据，并向下一Agent提供：

| 字段 | 要求 |
|---|---|
| Stage | `S0`至`S9` |
| Revision/dirty baseline | 开始与结束状态，不回退用户改动 |
| Files changed | 仅本阶段owner路径 |
| Public contracts frozen | 新增或改变的类型/API |
| Tests | Docker命令、CTest label、通过/失败数量 |
| Evidence level | E0至E4及允许声明范围 |
| Artifacts | 仓库相对artifact路径 |
| Open gaps | 精确阻断条件和下一owner |

下一Agent发现上阶段门禁失败时，只能修复上阶段或退回，不得在失败基础上继续堆叠下游实现。

## 5. 当前迁移判定

当前`include/openhbf/{ftl,controller,media,integration}`与`src/{ftl,controller,media,integration}`属于旧六模块实现。可迁移候选包括checked math、EventQueue、DLU accumulator、mapping公式、Page/Block状态和部分EAT；但迁移后必须进入新owner目录、使用新公开契约并通过对应阶段测试。旧测试和`tests/build-test/`不能作为新架构正式证据。

S0开始前所有新模块状态保持⚪ `PLANNED`。任何阶段只完成文件骨架或编译时，最高只能标记🟡 `PARTIAL`，不能标记🟢 `VERIFIED`。
