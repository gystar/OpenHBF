# HFSSS 文档规范

**文档版本**：V1.0
**日期**：2026-03-23

---

## 1. 语言

- 所有文档必须使用英文编写。
- 技术术语遵循 NVMe、JEDEC、OCP 和 TCG 标准术语。
- 中文源文档（文件名不带 `_EN` 后缀）仅作为参考保留，不属于权威版本。带 `_EN` 后缀的版本为权威版本。
- 任何 `_EN` 文件或计划纳入英文文档集的文件（例如 `TRACEABILITY_MATRIX.md`、`REQUIREMENT_COVERAGE.md`）均不得出现中文字符（Unicode 范围 U+4E00--U+9FFF）。

---

## 2. 文档结构

所有 HLD、LLD 和 TEST 文档必须按顺序包含以下章节：

### 2.1 HLD 文档模板

1. **标题和元数据**（文档名称、模块名称）
2. **修订历史**表（版本、日期、作者、说明）
3. **目录**（带编号的章节链接）
4. **概述/范围**（模块功能及其边界）
5. **需求可追溯性**表（REQ-ID、描述、优先级、状态）
6. **架构/设计**章节（图示、数据流、接口）
7. **架构决策记录（ADR）**（按 ADR-001、ADR-002 等编号）
8. **参考资料**（标准、相关文档）

### 2.2 LLD 文档模板

1. **标题和元数据**
2. **修订历史**表
3. **目录**
4. **概述/范围**（目的、引用的 HLD、涵盖的需求）
5. **数据结构**（C 结构体定义、常量、枚举）
6. **API/函数接口**规范
7. **内部逻辑和流程**（状态机、算法、时序图）
8. **需求可追溯性**表
9. **架构决策记录（ADR）**
10. **参考资料**

### 2.3 TEST 文档模板

1. **标题和元数据**
2. **修订历史**表
3. **目录**
4. **概述/范围**（测试策略、引用的 LLD）
5. **测试用例表**（测试 ID、描述、步骤、预期结果、优先级）
6. **需求覆盖率**映射（Test ID 到 REQ-ID）
7. **参考资料**

---

## 3. 命名约定

### 3.1 文件命名

| 文档类型 | 命名模式 | 示例 |
|----------|----------|------|
| 高层设计 | `HLD_NN_MODULE_NAME_EN.md` | `HLD_01_PCIE_NVMe_EMULATION_EN.md` |
| 低层设计 | `LLD_NN_MODULE_NAME_EN.md` | `LLD_17_POWER_LOSS_PROTECTION_EN.md` |
| 测试规范 | `TEST_LLD_NN_MODULE_NAME_EN.md` | `TEST_LLD_01_PCIE_NVMe_EMULATION_EN.md` |
| 系统测试计划 | `SYSTEM_TEST_PLAN_EN.md` | |
| 需求矩阵 | `REQUIREMENTS_MATRIX_EN.csv` | |
| PRD | `SSD_Simulator_PRD_EN.md` | |

- `NN` 是两位、以零补齐的序号（01--19）。
- 模块名称使用 UPPER_SNAKE_CASE。
- 英文文档的文件扩展名前必须带有 `_EN` 后缀。

### 3.2 REQ-ID 格式

- 格式：`REQ-NNN`（三位，以零补齐）。例如：`REQ-001`、`REQ-042`、`REQ-178`。
- 企业级需求 ID 接续核心需求编号（从 REQ-139 开始）。

### 3.3 测试用例 ID 格式

| 测试文档 | 前缀 | 示例 |
|----------|------|------|
| TEST_LLD_01 | `UT_PCIE_*`、`UT_NVME_*`、`UT_QUEUE_*` | `UT_PCIE_001` |
| TEST_LLD_02 | `UT_ARB_*`、`UT_SCHED_*`、`UT_WB_*` | `UT_ARB_010` |
| TEST_LLD_03 | `UT_NAND_*`、`UT_TIMING_*` | `UT_NAND_001` |
| TEST_LLD_04 | `UT_HAL_*` | `UT_HAL_NAND_001` |
| TEST_LLD_05 | `UT_RTOS_*`、`UT_LOG_*` | `UT_RTOS_001` |
| TEST_LLD_06 | `UT_FTL_*`、`UT_GC_*`、`UT_WL_*` | `UT_FTL_001` |
| 系统测试 | `ST-NNN` | `ST-001` |
| 故障注入 | `FI-NNN` | `FI-001` |

---

## 4. 术语表

以下术语必须在所有文档中一致使用。不得使用表中列出的替代表述。

| 规范术语 | 定义 | 禁止使用 |
|----------|------|----------|
| **UPLP** | Unexpected Power Loss Protection（意外掉电保护）——企业级 SSD 在电源故障期间保障数据安全的功能 | “UPL”；不带 UPLP 缩写的 “power loss protection” |
| **T10 DIF/PI** | T10 Data Integrity Field / Protection Information（T10 数据完整性字段/保护信息）——端到端数据完整性标准 | 单独使用 “DIF”（无 T10 上下文）；单独使用 “PI”（无 DIF 上下文） |
| **DWRR** | Deficit Weighted Round Robin（亏损加权轮询）——用于多队列带宽分配的公平调度算法 | 指亏损加权变体时使用 “DRR” 或 “WRR” |
| **WRR** | Weighted Round Robin（加权轮询）——NVMe 命令仲裁方法（与 DWRR 不同） | 可在 NVMe 仲裁语境中使用；不得与 DWRR 混淆 |
| **AES-XTS** | AES in XEX-based Tweaked-codebook mode with ciphertext Stealing——用于静态数据的加密模式 | 指该加密模式时使用 “AES-CBC” 或单独使用 “AES” |
| **TCG Opal** | Trusted Computing Group Opal Security Subsystem Class——自加密驱动器协议 | 不带 “TCG” 前缀而单独使用 “Opal” |
| **supercapacitor** | 用于 UPLP 的储能组件（单个单词、小写） | “super-capacitor”、“super capacitor”、“Super Capacitor” |
| **namespace** | NVMe 逻辑地址空间（单个单词、小写） | “name space”、“name-space” |
| **FTL** | Flash Translation Layer（闪存转换层） | |
| **GC** | Garbage Collection（垃圾回收） | |
| **WL** | Wear Leveling（磨损均衡） | |
| **WAF** | Write Amplification Factor（写放大系数） | |
| **EAT** | Effective Access Time（有效访问时间）——时序仿真引擎 | |
| **BBT** | Bad Block Table（坏块表） | |
| **L2P / P2L** | Logical-to-Physical / Physical-to-Logical（逻辑到物理/物理到逻辑）映射 | |
| **OOB** | 根据上下文，指 Out-of-Band（带外管理接口）或 Out-of-Band（NAND 备用区） | |
| **HAL** | Hardware Abstraction Layer（硬件抽象层） | |
| **RTOS** | Real-Time Operating System（实时操作系统） | |
| **WAL** | Write-Ahead Log（预写日志） | |
| **CWB** | Current Write Block（当前写入块） | |
| **SQ / CQ** | Submission Queue / Completion Queue（NVMe 提交队列/完成队列） | |
| **AER** | Asynchronous Event Request（NVMe 异步事件请求） | |
| **SPSC** | Single-Producer Single-Consumer（单生产者单消费者环形缓冲区） | |

---

## 5. 交叉引用格式

### 5.1 文档引用

引用其他文档时，应使用完整文件名：

- 正确：“请参见 `LLD_17_POWER_LOSS_PROTECTION_EN.md` 第 4 节。”
- 错误：“请参见 LLD_17”或“请参见掉电文档。”

### 5.2 需求引用

始终使用完整的 `REQ-NNN` 格式：

- 正确：“此功能实现了 REQ-139 至 REQ-146。”
- 错误：“此功能实现了需求 139-146。”

### 5.3 章节引用

引用同一文档中的章节时，应使用章节编号和标题：

- 正确：“请参见第 3.2 节（数据结构）。”
- 错误：“请参见上文。”

### 5.4 测试用例引用

使用带前缀的完整测试用例 ID：

- 正确：“由 UT_FTL_001 验证。”
- 错误：“由测试 1 验证。”

---

## 6. 版本控制

### 6.1 修订历史表格式

每份文档都必须在标题之后紧接着包含修订历史表：

```markdown
## 修订历史

| 版本 | 日期       | 作者  | 说明 |
|------|------------|-------|------|
| V1.0 | 2026-03-08 | HFSSS | 首次发布 |
| V1.1 | 2026-03-23 | HFSSS | 新增企业级功能 |
```

### 6.2 版本编号

- **主版本**（V1.0、V2.0）：发生重大结构变更或新增重要章节。
- **次版本**（V1.1、V1.2）：内容增补、修正或澄清。
- 修订历史中的版本号必须与文档声明的版本一致。

### 6.3 PRD 版本对应关系

- 核心需求（REQ-001 至 REQ-138）：PRD V1.0。
- 企业级需求（REQ-139 至 REQ-178）：PRD V2.0。
- 实施里程碑遵循路线图版本（V1.0 至 V3.0）。

---

## 7. 格式指南

- 使用 ATX 风格的 Markdown 标题（`#`、`##`、`###`）。
- 代码块使用三个反引号，并指定语言标识符（例如 `c`、`markdown`）。
- 表格使用 GitHub Flavored Markdown 的竖线语法。
- 图表使用围栏代码块中的 ASCII 图。
- 行长度不作硬性限制，但表格行应保持易读。
- 章节之间空一行；不得有行尾空白。
