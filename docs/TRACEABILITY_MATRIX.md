# OpenHBX TASK1需求可追溯矩阵

**版本**：V3.0

**产品profile**：`OCP_HBF_0_7`

**规范**：*OCP HBF Architecture Specification v0.7.0 FINAL*

**状态基线**：截至2026-08-30，S0-S7已形成E0至E2组件证据，重做后的S8形成真实01至07 pipeline E3证据；S9 Docker与宿主重放均为3/3，形成E3及`E4-extracted-field`候选证据。新增Docker Flash read性能测试形成单endpoint synthetic `E4-model-resource`候选证据，但没有完整E4 conformance或多资源饱和性能证据。逐项状态不得超过下方证据审计范围。

## 1. 状态定义

| 证据状态 | 含义 |
|---|---|
| `PDF_CONFIRMED` | 可在HBF PDF或仓库可检索基线中定位 |
| `RAMULATOR_CONFIRMED` | 可在当前Ramulator源码中定位 |
| `DESIGN_CHOICE` | OCP未规定的OpenHBX软件或synthetic模型选择 |
| `SPEC_EXTRACT_REQUIRED` | 仍需从规范原文提取精确字段/行为 |
| `EXTERNAL_DEPENDENCY` | 依赖UCIe、AMBA或vendor/product profile |

## 2. 配置与装配

| Requirement ID | 要求 | 来源 | HLD/LLD | Test ID | 设计状态 |
|---|---|---|---|---|---|
| CFG-001 | 只接受`OCP_HBF_0_7`正式profile | TASK1 V3 | 01 | CFG-T01 | `PLANNED` |
| CFG-002 | HBM/LPDDR选择在启动阶段明确拒绝 | TASK1裁剪 | 01 | CFG-T02 | `PLANNED` |
| CFG-003 | unknown/missing/type/range错误结构化报告 | DESIGN_CHOICE | 01 | CFG-T03 | `PLANNED` |
| CFG-004 | profile、override、derived value按冻结顺序合并 | DESIGN_CHOICE | 01 | CFG-T04 | `PLANNED` |
| CFG-005 | 容量、地址和时序派生使用checked arithmetic | DESIGN_CHOICE | 01 | CFG-T05 | `PLANNED` |
| CFG-006 | Host/Address/Controller/PAL/Media能力闭环 | DESIGN_CHOICE | 01 | CFG-T06 | `PLANNED` |
| CFG-007 | 正式HBF profile拒绝GC/Trim/L2P/active relocation | OCP §11.4 | 01 | CFG-T07 | `PLANNED` |
| CFG-008 | resolved config与capability manifest可审计 | 实验规范 | 01/08 | CFG-T08 | `PLANNED` |
| CFG-009 | standard/vendor/synthetic参数来源可区分 | DESIGN_CHOICE | 01 | CFG-T09 | `PLANNED` |
| CFG-010 | composer构建唯一02至07对象图 | TASK1 V3 | 01/02 | CFG-T10 | `PLANNED` |
| CFG-011 | 配置失败不产生部分对象图或运行事件 | DESIGN_CHOICE | 01 | CFG-T11 | `PLANNED` |
| CFG-012 | 外部阻断字段不能伪装为运行时数值 | 文档规范 | 01/08 | CFG-T12 | `PLANNED` |

## 3. Ramulator与系统内核

| Requirement ID | 要求 | 来源 | HLD/LLD | Test ID | 设计状态 |
|---|---|---|---|---|---|
| RAM-001 | 注册为Ramulator memory system并可由Factory构造 | RAMULATOR_CONFIRMED | 02 | RAM-T01 | `PLANNED` |
| RAM-002 | 精确实现当前`send/tick/get_clock_ratio/get_tCK/get_tx_bytes` | RAMULATOR_CONFIRMED | 02 | RAM-T02 | `PLANNED` |
| RAM-003 | `send(false)`零副作用且Request可重试 | RAMULATOR_CONFIRMED/设计契约 | 02 | RAM-T03 | `PLANNED` |
| RAM-004 | Accepted后保存Request所需值和callback，不依赖调用栈对象 | RAMULATOR_CONFIRMED | 02 | RAM-T04 | `PLANNED` |
| RAM-005 | terminal时先设置depart再调用原callback一次 | RAMULATOR_CONFIRMED | 02 | RAM-T05 | `PLANNED` |
| RAM-006 | 保持`Read=0`、`Write=1` ABI，扩展命令不污染枚举 | RAMULATOR_CONFIRMED | 02 | RAM-T06 | `PLANNED` |
| RAM-007 | ConfigNode与01 resolved config语义一致 | RAMULATOR_CONFIRMED | 01/02 | RAM-T07 | `PLANNED` |
| RAM-008 | Stats经Ramulator树导出且不重复计数 | RAMULATOR_CONFIRMED/DESIGN_CHOICE | 02 | RAM-T08 | `PLANNED` |
| SYS-001 | 唯一`OpenHbxSystem`拥有cycle/EventQueue/generation | DESIGN_CHOICE | 02 | SYS-T01 | `PLANNED` |
| SYS-002 | 同cycle事件按固定phase和sequence确定排序 | DESIGN_CHOICE | 02 | SYS-T02 | `PLANNED` |
| SYS-003 | Accepted请求精确一次terminal并满足守恒 | 实验规范 | 02/08 | SYS-T03 | `PLANNED` |
| SYS-004 | reset提升generation，旧事件只回收不提交 | DESIGN_CHOICE | 02 | SYS-T04 | `PLANNED` |
| SYS-005 | drain使用simulation状态并给出typed超限诊断 | 实验规范 | 02/08 | SYS-T05 | `PLANNED` |
| SYS-006 | clock-domain换算使用整数checked arithmetic且时间不倒退 | DESIGN_CHOICE | 02 | SYS-T06 | `PLANNED` |
| SYS-007 | Busy/Rejected不创建future event或completion obligation | DESIGN_CHOICE | 02 | SYS-T07 | `PLANNED` |
| SYS-008 | snapshot定位非idle owner且读取不推进cycle | 实验规范 | 02/08 | SYS-T08 | `PLANNED` |

## 4. OCP HBF Host协议

| Requirement ID | OCP要求 | 证据 | HLD/LLD | Test ID | 设计状态 |
|---|---|---|---|---|---|
| HOST-001 | 配置1至16个独立Channel，跨Channel请求拒绝且零副作用 | PDF_CONFIRMED，Product Description | 03/04 | HOST-T01 | `PLANNED` |
| HOST-002 | Flash Read/Write为64 B基本transaction且burst不跨4 KiB DLU | PDF_CONFIRMED，§5.3/§5.4 | 03 | HOST-T02 | `PLANNED` |
| HOST-003 | Flash、Scratchpad、CSR/Admin packet type表驱动且reserved拒绝 | PDF_CONFIRMED，Table 8/9 | 03 | HOST-T03 | `PLANNED` |
| HOST-004 | same AXI ID保序，不同ID可乱序但同地址hazard保序 | PDF_CONFIRMED，Table 8 Note 1/§5.4.3 | 03 | HOST-T04 | `PLANNED` |
| HOST-005 | Write non-posted，仅Controller terminal后产生一次response | PDF_CONFIRMED，§5.4 | 03/05 | HOST-T05 | `PLANNED` |
| HOST-006 | Read/Write status、data-valid与ErrorInfo按Tables 12/13编码 | PDF_CONFIRMED，Tables 12/13 | 03/05 | HOST-T06 | `PLANNED` |
| HOST-007 | Always-On/MMIO寄存器按access、reset domain和owner校验 | PDF_CONFIRMED，Tables 15/16 | 03/05 | HOST-T07 | `PLANNED` |
| HOST-008 | Link/Channel ready与`BUCC.EN`联合控制admission | PDF部分确认；外部UCIe依赖 | 03/05 | HOST-T08 | `PLANNED` |
| HOST-009 | Sideband opcode表驱动并拒绝reserved value | PDF_CONFIRMED，Table 14 | 03 | HOST-T09 | `PLANNED` |
| HOST-010 | Busy/Rejected不分配token、不转移payload、不产生event | DESIGN_CHOICE | 03 | HOST-T10 | `PLANNED` |
| HOST-011 | Format 6/AXI wire-level在外部规范提取前不宣称bit-accurate | EXTERNAL_DEPENDENCY | 03/08 | HOST-T11 | `BLOCKED_SPEC` |

## 5. 地址与拓扑

| Requirement ID | 要求 | 来源 | HLD/LLD | Test ID | 设计状态 |
|---|---|---|---|---|---|
| ADDR-001 | A1按64 B单位解析且DLU内sector为0至63 | PDF_CONFIRMED，§5.7 | 04 | ADDR-T01 | `PLANNED` |
| ADDR-002 | 严格实现L1/B1/P1/Bank_Num/L2及R1-R5公式 | PDF_CONFIRMED，§5.7 | 04 | ADDR-T02 | `PLANNED` |
| ADDR-003 | Channel只能访问其owner Bank容量切片 | OCP产品组织/拓扑解释 | 04/07 | ADDR-T03 | `PLANNED` |
| ADDR-004 | geometry、容量乘法和反向映射使用checked arithmetic | DESIGN_CHOICE | 04 | ADDR-T04 | `PLANNED` |
| ADDR-005 | 同Block按Page顺序reserve/complete，Page-0形成auto-erase意图 | PDF_CONFIRMED，§5.7/§11.4 | 04/05/07 | ADDR-T05 | `PLANNED` |
| ADDR-006 | Program Fail进入replay gate，地址序列为`L2+n*R5` | PDF_CONFIRMED，§5.7 | 04/05 | ADDR-T06 | `PLANNED` |
| ADDR-007 | Zone Remap只改变mapping view并要求Host重写 | PDF_CONFIRMED，Admin opcode 0x08 | 04/08 | ADDR-T07 | `PLANNED` |
| ADDR-008 | retirement/reduced capacity使地址不可用且不改变物理层次 | PDF_CONFIRMED/设计边界 | 04/07 | ADDR-T08 | `PLANNED` |
| ADDR-009 | 正式profile拒绝L2P、GC和active relocation配置 | OCP §11.4/TASK1 | 01/04 | ADDR-T09 | `PLANNED` |

R1/R2/R3/R5具体产品几何仍依赖product/vendor profile；公式行为可验证不代表任意默认值符合真实产品。

## 6. Base Die控制器

| Requirement ID | 要求 | 来源 | HLD/LLD | Test ID | 设计状态 |
|---|---|---|---|---|---|
| CTRL-001 | 64个64 B sector聚合为4 KiB DLU且首sector启动timeout | PDF_CONFIRMED，§5.4.1 | 05 | CTRL-T01 | `PLANNED` |
| CTRL-002 | duplicate、pending上限和timeout映射0x2/0x4/0x5 | PDF_CONFIRMED，Table 13 | 05 | CTRL-T02 | `PLANNED` |
| CTRL-003 | 仅完整DLU且04顺序许可后向PAL发Program | PDF_CONFIRMED，§5.4/§5.7 | 04/05/06 | CTRL-T03 | `PLANNED` |
| CTRL-004 | Write仅PAL/Media成功commit后terminal response | PDF_CONFIRMED，non-posted write | 05/06/07 | CTRL-T04 | `PLANNED` |
| CTRL-005 | accumulator forwarding及pending缺sector状态0xA | PDF_CONFIRMED，§5.4.3 | 05 | CTRL-T05 | `PLANNED` |
| CTRL-006 | 同Bank sense严格有序，Regular结束当前Batch | PDF_CONFIRMED，Read行为 | 05 | CTRL-T06 | `PLANNED` |
| CTRL-007 | 每Bank至少两个4 KiB cache buffer | PDF_CONFIRMED，BUCCAP/Read行为 | 05 | CTRL-T07 | `PLANNED` |
| CTRL-008 | ECC区分OK/CECC有效/UECC无效或Retry | PDF_CONFIRMED，Table 12 | 05/07 | CTRL-T08 | `PLANNED` |
| CTRL-009 | Admin、Scratchpad和lifecycle/reset有界执行 | PDF部分确认 | 05 | CTRL-T09 | `PLANNED`/`BLOCKED_SPEC` |
| CTRL-010 | 所有介质操作只经06 PAL，05不拥有TSV | DESIGN_CHOICE | 05/06 | CTRL-T10 | `PLANNED` |
| CTRL-011 | vendor timeout/cache/latency参数有来源或synthetic标签 | EXTERNAL_DEPENDENCY | 01/05 | CTRL-T11 | `BLOCKED_SPEC` |

## 7. 互联与Flash PAL

| Requirement ID | 要求 | 来源 | HLD/LLD | Test ID | 设计状态 |
|---|---|---|---|---|---|
| PAL-001 | 只接收canonical FlashPhysicalRequest | DESIGN_CHOICE | 06 | VER-PAL-001 | `PLANNED` |
| PAL-002 | forward/media/return异步FSM精确一次完成 | DESIGN_CHOICE | 06 | VER-PAL-002 | `PLANNED` |
| PAL-003 | 共享route资源按EAT串行，独立资源并行 | DESIGN_CHOICE | 06 | VER-PAL-003 | `PLANNED` |
| PAL-004 | TSV fault、spare、mapping和repair epoch唯一归06 | OCP Base Die TSV能力+DESIGN_CHOICE | 06 | VER-PAL-004 | `PLANNED` |
| PAL-005 | reset/stale event不提交且credit可回收 | DESIGN_CHOICE | 06 | VER-PAL-005 | `PLANNED` |
| PAL-006 | 带宽ceiling由lane/rate/efficiency推导 | DESIGN_CHOICE | 06/08 | VER-PAL-006 | `PARTIAL`（single-endpoint synthetic） |
| PAL-007 | hybrid bonding缺vendor数据时不得成为正式profile | EXTERNAL_DEPENDENCY | 06 | VER-CFG-006 | `BLOCKED_SPEC` |

PAL是OpenHBX软件边界，不是OCP HBF规范术语。

## 8. NAND介质与RAS

| Requirement ID | 要求 | 来源 | HLD/LLD | Test ID | 设计状态 |
|---|---|---|---|---|---|
| MEDIA-001 | 建立CoreDie/Die/Bank/Block/Page及4 KiB payload | OCP组织+DESIGN_CHOICE | 07 | VER-MEDIA-001 | `PLANNED` |
| MEDIA-002 | Read/Program/Erase分stage异步执行 | DESIGN_CHOICE，HFSSS参考 | 07 | VER-MEDIA-002 | `PLANNED` |
| MEDIA-003 | issue不提交未来状态，terminal completion原子commit | 系统不变量 | 07 | VER-MEDIA-003 | `PLANNED` |
| MEDIA-004 | NAND array/Bank/Die EAT与PAL EAT分离 | DESIGN_CHOICE | 06/07 | VER-MEDIA-004 | `PLANNED` |
| MEDIA-005 | Read返回immutable 4 KiB snapshot | DESIGN_CHOICE | 07 | VER-MEDIA-005 | `PLANNED` |
| MEDIA-006 | raw outcome支持CECC候选/UECC/retry/refresh notice | OCP reliability/status | 07/05 | VER-MEDIA-006 | `PLANNED` |
| MEDIA-007 | BBT、bad block、PEC和retirement状态唯一归07 | OCP reliability | 07 | VER-MEDIA-007 | `PLANNED` |
| MEDIA-008 | reset/fault/thermal观测确定且可重放 | OCP thermal+DESIGN_CHOICE | 07/08 | VER-MEDIA-008 | `PLANNED` |

NAND timing、cell mode和可靠性数字若非vendor数据，必须标记为synthetic，不能作为OCP参数合规结论。

## 9. 验证要求

| Requirement ID | 验证目标 | 证据级别 | HLD/LLD | 设计状态 |
|---|---|---|---|---|
| VER-001 | 配置、profile与非法跨字段组合 | E1/E2 | 08 | `VERIFIED`（S2） |
| VER-002 | OCP Host 64 B/DLU边界、ordering、status与register vector | E1-E4 | 08 | `PARTIAL`/`BLOCKED_SPEC` |
| VER-003 | 真实01至07全链路mapping、DLU、PAL/Media payload与terminal因果 | E3 | 08 | `VERIFIED`（S8 transaction-level） |
| VER-004 | fault/reset/stale/duplicate completion和drain | E3 | 08 | `VERIFIED`（S8已覆盖范围） |
| VER-005 | deterministic digest和resource守恒 | E3 | 08 | `VERIFIED`（S8） |
| VER-006 | Host/PAL/NAND理论ceiling | E4 | 08 | `PARTIAL`（Fabric局部ceiling候选；其余资源未覆盖） |
| VER-007 | E4 vector、规范定位与artifact可审计 | E4 | 08 | `VERIFIED`（已提取字段范围） |
| VER-008 | 未提取OCP字段保持阻断，不以通用常识补齐 | E4 audit | 08 | `BLOCKED_SPEC` |

## 10. 阻断队列

| Gap ID | 缺口 | 影响需求 | 关闭条件 |
|---|---|---|---|
| GAP-001 | UCIe v3.0 Format 6逐bit header/CRC | HOST-011/VER-003 | 外部规范定位、codec和golden vector |
| GAP-002 | UCIe credit、FDI/RDI与完整Link FSM | HOST-008/011 | 外部规范状态机与恢复vector |
| GAP-003 | 完整AMBA AXI基础/边界行为 | HOST-004/011 | AMBA原文与ordering oracle |
| GAP-004 | RESETBD_n/RESETSB_n精确in-flight语义 | HOST-008/SYS-004 | PDF/外部规范timeline与phase sweep |
| GAP-005 | R1/R2/R3/R5和真实stack geometry | ADDR-004 | 有来源product/vendor profile |
| GAP-006 | Speed Grade完整表与产品带宽参数 | HOST-002/003/PAL-006 | 表提取与公式review |
| GAP-007 | Scratchpad具体容量/latency及部分控制字段 | CTRL-009 | product profile与边界vector |
| GAP-008 | IEEE1500/DA完整instruction/data语义 | HOST-008/VER-003 | 规范与golden vector |
| GAP-009 | hybrid bonding物理参数 | PAL-007 | vendor pitch/lane/BER/latency/energy/thermal数据 |
| GAP-010 | factory/runtime坏块未与Zone/容量视图及Host replay形成闭环 | ADDR-008/MEDIA-005/VER-003/004 | 启动期manifest规避、运行期BBT提交与原子retirement、无spare容量降级及E3故障测试全部完成 |

## 11. 发布声明规则

`DESIGNED`、`PLANNED`和文件存在均不代表实现或验证完成。只有符合[`实验与测试执行规范.md`](实验与测试执行规范.md)的E3/E4 artifact才能支持端到端或conformance声明。

发布报告必须分别统计PDF confirmed、Ramulator confirmed、design choice、external dependency和blocked项。只要HOST-011或VER-008相关缺口未关闭，就只能声明“已验证的OCP HBF字段/行为”，不能声明完整bit-accurate OCP/UCIe/AXI conformance。

当前S9证据为`build/artifacts/stages/s9/EXP-S9-CONFORMANCE/20260830T103513Z/`（2026-08-30，Docker Debug，`openhbx_s9` 3/3）。真实pipeline vector支持64 B、DLU边界、non-posted terminal及payload字段证据；同步多child/Busy oracle为E2。Format 6、AXI wire、Table 14 numeric framing、vendor/hybrid参数、Ramulator functional payload以及饱和resource ceiling均未由该证据关闭。

`tests/performance/test_flash_read_bandwidth.cpp`的Docker结果补充了`VER-006`局部证据：single-endpoint synthetic真实pipeline在1次warmup后完成15次4 KiB read，即61440 B/8146 simulation cycles，`7.542352 B/cycle`不超过Fabric公式ceiling `7.699248 B/cycle`，且system stats验证accepted/completed bytes、failure和outstanding守恒。配置显式采用OpenHBX/Ramulator adapter既有synthetic baseline `tCK=1000 ps`，对应模型值`7.542352 GB/s`。此结果为`E4-model-resource`候选；该`tCK`不是vendor/OCP保证，且未覆盖多endpoint和Host/Bank/Media独立瓶颈，故`VER-006`仍为`PARTIAL`。

当前`tests/performance/test_hbf_max_read_bandwidth.cpp`形成16 Channel饱和候选证据：16 Core Die × 16 Die/Core × 16 Bank/Die共4096个物理Bank，均匀映射为每Channel 256个owned Bank；每Channel 4个虚拟AXI共享物理Channel resource，Forward/Return独立计时，每条route携带完整64-lane集合，repair/spare按Channel隔离。4096个4 KiB read在8884 ns完成16777216 B，得到`1888.475462 GB/s`，为4096 GB/s raw ceiling的`46.1054%`，相对OCP 3072 GB/s用户目标为`61.4738%`。测试每2000 simulation cycles自动报告区间完成字节和GB/s；周期文本日志用于诊断，最终守恒和性能oracle来自结构化stats，故`VER-006`保持`PARTIAL`。
