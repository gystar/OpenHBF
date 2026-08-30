# OCP HBF Architecture Specification v0.7.0 中文设计基线

## 1. 文档范围与使用规则

本文依据 `OCP HBF Architecture Specification v0.7.0 FINAL.pdf` 整理，覆盖 Product Description（PDF 15--24 页）、UCIe Host Interface Architecture（25--77 页）、Reset/Power（79--85 页）、Reliability/Thermal（106--109 页）和 Software User Guide（112--123 页）。本文用于 OpenHBF 的 HLD、LLD、需求矩阵和测试设计，不替代正式 PDF，也不扩写规范未定义的产品参数。

以下内容属于外部依赖而非 HBF PDF 内部缺口：UCIe v3.0 Format 6 的逐 bit header/CRC 布局、完整 sideband framing/credit/FDI/RDI 状态机，以及 AMBA AXI 的基础 valid/ready 语义。Feature、BIST、vendor error、timer 等标为 product/vendor-specific 的数值必须由产品 profile 提供。

## 2. 产品架构与性能

HBF 是与 xPU 紧密耦合的非一致性、高容量 NAND flash。一个堆栈最多有 16 个独立 Host Channel；每个 Channel 通过自己的 UCIe link 访问专属 Core Die 资源，不能跨 Channel 访问。Base Die 负责 UCIe、命令调度、ECC、错误处理、NAND 初始化、地址映射、read counter、TSV repair 和管理接口；Core Die 是 NAND Die 堆栈。Device Logical Unit（DLU）为 4 KiB，是 Base Die 向 Core Die 发起 read/program 的最小逻辑单位。

每个 UCIe Channel 使用 x64 full-duplex mainband 和 Streaming Protocol，选择 Format 6 Latency-Optimized 256B Flit with Optional Bytes；CRC 按每个 128 B segment 处理。每 Channel 可暴露 1、2 或 4 个虚拟 AXI Interface。

| Speed Grade | Line rate | Raw bandwidth/module | System bandwidth/module | 最大 module 数 | 最大 stack height |
|---|---:|---:|---:|---:|---:|
| 1 | 8 GT/s | 64 GB/s | 48 GB/s | 8 | 8 |
| 2 | 16 GT/s | 128 GB/s | 94 GB/s | 16 | 16 |
| 3 | 32 GT/s | 256 GB/s | 188 GB/s | 16 | 16 |

最高配置的系统有效带宽约为 3.072 TB/s。通道独立计时和运行，容量在该通道的 AXI Interface 间划分。Scratchpad 是可选、易失、高优先级存储，Host 负责内容及 validity；其访问粒度为 64 B，power cycle 后内容不保证保持。

## 3. Link、AXI 与 packet type

Link initialization 分为四个阶段，通道分别完成初始化和 ready 判定。Sideband packet header 包含 5-bit opcode、3-bit source ID 和 3-bit destination ID。精确物理训练、flit bit layout 和 credit 行为引用 UCIe v3.0。

`ARUSER/AWUSER[1:0]` 定义 command packet type：`00b` Flash IO、`01b` Scratchpad IO、`10b` CSR/Admin、`11b` Reserved。`RUSER/BUSER[1:0]` 返回 response packet type，`[5:2]` 返回 Command Status，更高位承载 ErrorInfo。相同 AXI ID 的 transaction 必须保持顺序；不同 ID 可以乱序，但相同 64 B 地址上的 read/write hazard 仍必须保序。

规范表 9 的 Admin opcode 为：

| Opcode | Command |
|---:|---|
| `0x01` | Set Feature |
| `0x02` | Get Feature |
| `0x03` | Secure Erase |
| `0x04` | Get Log Page |
| `0x07` | BIST |
| `0x08` | Zone Remapping |
| `0x09` | Read UCIe Errors |
| `0x0A` | Reduced Capacity |
| `0x20` | Register Read/Write，方向由 AR/AW 区分 |

Zone Remapping 只交换或修改 mapping，不搬移 user data；操作完成后由 Host 重写所需数据。HBF 不提供传统 SSD 式 background GC 或 active-data relocation 语义。

## 4. Flash read、write 与 mixed workload

Host Flash read 的粒度为 64 B。非 burst 方式可用 64 个 64 B command 读取一个 4 KiB DLU；支持 burst 的产品可使用 length 0--63，访问不得跨越 DLU 边界。同一 Bank 的 NAND sense 必须严格有序，每 Bank 至少提供两个 4 KiB cache buffer。Regular Read 与 Batch Read 必须分别建模；Batch 后出现 Regular Read 表示当前 batch 结束。Scratchpad read 忽略 batch hint。

Host Flash write 也可以由 64 B transaction 构成，不要求单个 command 一次提交完整 4 KiB。Base Die 以 DLU 地址为 key 保存 64 个 sector 的 received mask、数据、关联 command 和 deadline；第一个 sector 到达即启动 accumulation timeout。只有收齐完整 4 KiB 后才能向 Core Die program。write 为 non-posted，每个 command 只产生一个 response，且只有数据实际写入 Core Die 后才能成功完成。

同一 pending DLU 的重复 sector 返回 Overlap Address；pending DLU 达到产品上限时拒绝新累积；超时未收齐时完成该 DLU 的关联 command 并报告 timeout。同一 NAND block 内 page 必须依序 program，page-0 write 触发内部 auto-erase。program failure 后 block 进入 replay 状态，Host 从 page 0 重放至失败页后才能继续。

Mixed workload 在 64 B 地址粒度维护 hazard，即使 AXI ID 不同也不能返回旧数据。read 命中 accumulator 中已收到的 sector 时直接 forwarding；目标 DLU 已 pending、但目标 sector 尚未收到时，不读旧 NAND 数据，而返回 Read Status `0xA`。

## 5. Command Status

规范表 12 定义 Read Status：

| Status | 含义 |
|---:|---|
| `0x1` | Invalid Address |
| `0x2` | Temporarily Restricted |
| `0x3` | Invalid User Field |
| `0x4` | UECC / Block Refresh Required |
| `0x5` | CECC / Data Valid / Refresh Recommended |
| `0x6` | UECC / Read Retry Required |
| `0x7` | Read Erased Page |
| `0x8` | Capacity Unusable |
| `0x9` | Die Temporarily Blocked |
| `0xA` | Read from Pending Write |
| `0xD`--`0xF` | Vendor-specific |

规范表 13 定义 Write Status：

| Status | 含义 |
|---:|---|
| `0x1` | Invalid Address |
| `0x2` | Overlap Address |
| `0x3` | Invalid User Field |
| `0x4` | Maximum Pending DLU |
| `0x5` | DLU Accumulation Timeout |
| `0x6` | Write Order Violation |
| `0x7` | Program Fail / Replay Block |
| `0x8` | Capacity Unusable |
| `0x9` | Die Temporarily Blocked |
| `0xD`--`0xF` | Vendor-specific |

`0x5` Read Status 的数据仍有效；`0x6` 需要按 ErrorInfo 指示的 retry stage 重试。Die recovery 期间对目标 Die 的新访问立即返回 temporary blocked，而不能无限排队。

## 6. Page-0 地址计算与 replay

第 5.7 节中 `A1` 是以 64 B 为单位的 UCIe local address，`R4` 是每 page 包含的 64 B 数；4 KiB page 时 `R4=64`。因此不应再把公式中的 `>>6` 误解为 byte address 的右移。

```text
L1       = QUOTIENT(A1, R4)
B1       = QUOTIENT(L1, R1 * R2 * R3)
P1       = REMAINDER(L1, R1 * R2 * R3)
Bank_Num = REMAINDER(REMAINDER(L1, R1 * R2 * R3), R1 * R2)
L2       = B1 * R1 * R2 * R3 + Bank_Num
```

发生 block replay 或 refresh 时，Host 按 `L2, L2 + R5, ..., L2 + (R3 - 1) * R5` 生成该 block 的 page 地址序列。实现变量必须携带 `byte`、`64b_unit` 或 `dlu` 单位，禁止混用。

## 7. Sideband opcode

表 14 定义 32-bit/64-bit Memory Read/Write、DMS、Configuration Read/Write、Completion、Message、Vendor-specific 和 Management Port 等 opcode。OpenHBF 必须使用表驱动 enum 和 reserved-value 校验；完整 frame、credit 和链路层交互仍引用 UCIe v3.0，不能根据 opcode 表臆造 bit-accurate wire model。

## 8. Always-On 与 MMIO register

表 15 为 Always-On register，表 16 为 MMIO register。关键 MMIO offset 如下：

| Offset | Register | 关键语义 |
|---:|---|---|
| `0x0000` | BUCCAP | NCDU 支持 1/2/4/8/16 Core Die；NCBB 默认每 Bank 两个 cache buffer；MOCS 最大支持 16384 outstanding command |
| `0x0008` | VS | Version |
| `0x000C` | BUCC | `EN` 控制 Channel 是否接收 IO |
| `0x0010` | BUCSTS | `RDY` 表示 Channel ready |
| `0x0014` | BUCR | 写 `0x484246` 触发 reset |
| `0x0018` | TMON | `CES` 为 sticky，HBF_RESET 不清除 |
| `0x001C` | SMEMSA | Scratchpad start，64 B aligned |
| `0x0024` | SMEMSZ | per-AXI Scratchpad size，单位 64 B |
| `0x0034` | FIOSA | Flash IO start，4 KiB aligned |
| `0x003C` | FIOSZ | per-AXI Flash IO size，单位 4 KiB |
| `0x004C` | SMSZ | Supported memory size |
| `0x0054` | VSTSZ | Vendor-specific table size |
| `0x005C` | ESZ | Error structure size |
| `0x0064` | ENC | Encryption capability/control |
| `0x0100` | TMR | Timer register |
| `0x0140` | LOCKOUT | Lockout control/status |
| `0x0144` | MAXPEC | Maximum program/erase count |
| `0x0148` | AVGPEC | Average program/erase count |
| `0x014C` | REDCAP | Reduced capacity information |
| `0x0150` | TTTEMP | 温度为 `60 C + value * 0.5 C` |

寄存器模型必须实现 access attribute、reset domain、per-Channel/per-AXI ownership 和 reserved bit 检查。未由规范固定的字段宽度或产品能力值属于 product profile。

## 9. Reset、power、reliability 与 thermal

`RESETSB_n` 和 `RESETBD_n` 为低有效 reset；各 reset domain、link initialization、`READY`/`BUCSTS.RDY` 和 `BUCC.EN` 必须共同约束 IO admission。软件 reset 由 `BUCR` magic value 触发。`TMON.CES` 的 sticky 属性跨 HBF_RESET 保持；精确 in-flight transaction 处置若规范没有给出完整时序，应作为显式 reset policy，不得静默丢失 completion。

可靠性模型至少覆盖 CECC、UECC、read retry、block refresh、program failure、replay、reduced-capacity bitmap，以及 Block/Bank/Die/Channel retirement。温度模型提供 threshold、告警和 `CATTRIP` 路径；`TTTEMP` 编码按上式换算。电气、封装和真实热传导可作为 profile 或外部模型，不应被 transaction-level 仿真宣称为物理合规。

## 10. OpenHBF 落实清单

1. Host ingress 以 64 B 为 Flash read/write 基本 transaction，burst 只改变 command 数量。
2. Base Die 实现 bounded `PendingDlu`、sector mask、timeout、overlap、forwarding 和 `0xA` hazard response。
3. 完整 4 KiB 收齐并通过 block-order 检查后，才向 Core Die program；成功 completion 位于 Core Die 写入之后。
4. 同 ID ordering、跨 ID 可乱序和相同地址 mixed ordering 分开建模。
5. Regular/Batch Read、每 Bank 双 cache buffer、Scratchpad 高优先级旁路分别建模。
6. 表 9、12、13、14、15、16 使用 typed enum/register descriptor，并用 reserved-value test 覆盖。
7. Page-0/replay 公式统一使用明确单位，Zone Remapping 后由 Host 重写数据。
8. 仅把外部 UCIe/AMBA 和 product/vendor-specific 内容保留为待配置项；已从本 PDF 提取的 packet、status、opcode、register 和地址单位不得继续标记为未知。
