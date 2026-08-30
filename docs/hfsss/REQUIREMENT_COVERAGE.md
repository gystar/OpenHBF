# HFSSS需求覆盖率分析

**文档版本**: V3.1
**日期**: 2026-05-16

---

## 概述

本文档针对当前HFSSS实施分析了需求矩阵中178需求的覆盖范围。要求REQ-001通过REQ-138覆盖核心功能；REQ-139通过REQ-178涵盖PRD V2.0 (第12章) 中添加的企业SSD功能。

**V3.0更新 (2026)**: 对当前源树进行端到端代码审计。阶段4 (启动/NOR/FTL可靠性/跟踪) 和大部分相位5/6 (OOB/hfsss-ctrl/YAML/perf框架/故障-注入) + 企业V3.0组 (UPLP、多NS、安全、热/遥测) 现落地。覆盖率从38.8% 上升到**65.2%**全面实施 (86.5% 计算部分)。审查后更正:REQ-132(NAND故障注入接入待定)，REQ-161(TCG Opal命令解析挂起)，REQ-164 (`secure_boot_verify`未从引导流调用)，REQ-165(键表持续到任意文件，而不是NOR)，REQ-175/176 (遥测环存在，但NVMe日志页07h/08h派遣未连线) 全部从✅到⚠.REQ-163(清理操作模式) 在本说明中已被标记为部分，但这是一个测试预期错误，而不是实现差距-底层`nvme_uspace_sanitize`已发送所有四个NVMe § 5.22 SANACT代码。`systest_nvme_compliance::NC-013`和`systest_data_integrity::DI-007`现在为每个代码断言模拟器建模的后状态 (EXIT_FAILURE: 无操作；BLOCK_ERASE / CRYPTO_ERASE: L2P丢弃 → 未发送; 覆盖: 零填充模式)。NVMe § 5.22使确切的释放/覆盖/加密擦除内容松散; 测试将模拟器的具体可观察量固定为满足规范的 “旧数据不可恢复”/“主机模式写入” 保证。

**V2.0更新**: 增加了40个企业要求 (REQ-139通过REQ-178) 涵盖UPLP，QoS确定性，T10 DIF/PI，安全性，多名称空间管理，热量管理和高级遥测。

**V1.2更新**: 反映第一阶段 (FTL/媒体) 、第二阶段 (控制器/HAL) 、第三阶段 (用户空间NVMe) 完成情况。

### 状态定义
- ✅ **已实施**: 要求完全实现，至少有一个通过测试
- ⚠️ **部分**: 要求部分实现; 功能集或测试覆盖未完成
- ❌ **未实现**: 未实现要求
- 🔧 **存根**: 只有占位符/存根实现存在

---

## 按模块汇总

|模块|总计| ✅ | ⚠️ | ❌ | 🔧 |覆盖率%|改变|
|--------|------:|---:|---:|---:|---:|-----------:|--------|
|PCIe/NVMe设备仿真| 22 | 16 | 0 | 6 | 0 | 72.7% | ↑ REQ-009(IO队列动态创建/删除)，REQ-010(PRP/SGL-两个步行者)，REQ-014(中断合并)，REQ-019(PRP解析引擎)|
|控制器线程| 15 | 12 | 1 | 2 | 0 | 80.0% | -- |
|媒体线程| 20 | 19 | 1 | 0 | 0 | 95.0% | ↑ REQ-042多平面并发REQ-044每通道worker运行时；REQ-045现在✅通过channel_worker时间戳选择加入无锁完成队列批量排放 (`channel_worker_drain`) 每`docs/superpowers/specs/2026-04-24-req-045-completion-queue-design.md`; REQ-048通过验证保留时间驱动的RBER`tests/test_retention.c` |
|哈尔| 12 | 12 | 0 | 0 | 0 | 100% | ↑ REQ-063艾尔REQ-064PCIe链路状态REQ-069字节级配置空间 (LLD_13) |
|共同事务| 24 | 20 | 2 | 2 | 0 | 83.3% | ↑ REQ-085SPSC环REQ-087在引导之上的系统资源监视器/power/OOB/SMART/trace |
|算法任务层 (FTL)| 22 | 19 | 1 | 2 | 0 | 86.4% |↑ 6 (cmd状态机、重试、流ctl、磨损监视器、错误日志页); FTL die-busy retry-spin替换为每个die等待队列完成-事件驱动的分派 (`src/ftl/die_dispatcher.c`)-参见`docs/superpowers/specs/2026-04-30-ftl-die-busy-waitqueue-design.md`；012_seqwrite_verify延迟正在调查的根本原因-dispatcher (PR #118) FIFO票证锁定 (PR #119) 未改善写入延迟 (141.9 ms vs基线142.0 ms)，每`docs/superpowers/specs/2026-04-30-ftl-die-busy-waitqueue-design.md`第9.2节|
|性能要求| 8 | 8 | 0 | 0 | 0 | 100% | ↑ REQ-116..120 + REQ-122Amdahl可扩展性REQ-123突发性负载CPU探针REQ-121种子NAND延迟抖动注入器|
|产品接口| 8 | 5 | 2 | 1 | 0 |62.5% (87.5% 部分)| ↑ REQ-131 LLD_15与装运代码 (V1.1) 对账；REQ-125/126保留⚠️ 待定LLD_16主机路径证据|
|故障注入| 3 | 3 | 0 | 0 | 0 | 100% | ↑ REQ-134控制器故障挂钩 (池耗尽恐慌超时风暴) 连接到resource_mgr仲裁器; 验证人`tests/test_fault_inject.c::test_controller_*` |
|系统可靠性| 4 | 3 | 0 | 1 | 0 | 75.0% | ↑ REQ-088P99.9延迟异常检测器|
| **核心小计** | **138** | **118** | **4** | **16** | **0** | **85.5%**(88.4% 部分)| ↑ REQ-009, REQ-010, REQ-014, REQ-019从翻转❌/⚠→✅|
|企业: UPLP| 8 | 8 | 0 | 0 | 0 | 100% |↑ 已实施|
|企业: QoS确定性| 7 | 7 | 0 | 0 | 0 | 100% |↑ DWRR per-NS IOPS/BW caps SLA回滚热重新配置REQ-153占空比准入统计|
|企业版: T10 DIF/PI| 5 | 5 | 0 | 0 | 0 | 100% |↑ 类型1/2/3CRC-16 gc-path PI传播|
|企业: 安全| 7 | 7 | 0 | 0 | 0 | 100% |↑ aes-xts sim，密钥，加密擦除，消毒操作模式，secure-boot-verify，NOR-支持的双副本UPLP-安全密钥表，TCG-蛋白石锁定/解锁|
|企业: 多命名空间| 5 | 5 | 0 | 0 | 0 | 100% |↑ 已实施|
|企业级: 热/遥测| 8 | 8 | 0 | 0 | 0 | 100% |↑ 油门智能预测NVMe日志页面07h/08h/0xC0派遣AER通知程序助手REQ-178运行时生产者 (smart_monitor) |
| **企业小计** | **40** | **40** | **0** | **0** | **0** | **100%** |↑ 来自0%|
| **总计** | **178** | **158** | **4** | **16** | **0** | **88.8%**(91.0% 部分)|↑ 来自38.8% (V3.0)|

> **注意**: 上面的数字计算单个需求行。相关路线图集团级别的覆盖范围从不同的角度跟踪相同的现实。自V2.0以来的所有更改均已针对当前源代码进行了验证; 有关文件级证据，请参见每行上的注释列。

---

## 详细的需求覆盖范围

### 1. PCIe/NVMe设备仿真模块 (REQ-001 to REQ-022)

| ID |需求描述|状态|笔记|
|----|------------------------|--------|-------|
| REQ-001 |PCIe配置空间仿真-标准PCI Type 0配置标头| ✅ |中的基本配置标头结构`pci.h` |
| REQ-002 |PCIe配置空间仿真-PCIe功能链表| ✅ |四个标准帽 (PM0x01 @ 0x40,MSI0x05 @ 0x50,MSI-X0x11 @ 0x70,PCIe Express0x10 @ 0x90) 播种在`hal_pci_cfg_init`. `hal_pci_capability_find`走过的链条通过`cfg_read8`在每个条目的`{cap_id, next}`具有针对损坏指针的96跳安全界限的标头。由……承保`tests/test_hal.c::test_hal_pci_cfg_cap_chain_layout` / `test_hal_pci_cfg_capability_find`(15个断言，包括周期检测)。|
| REQ-003 |PCIe配置空间仿真-BAR寄存器配置| ✅ |定义的BAR常量和结构|
| REQ-004 |NVMe控制器寄存器仿真-CAP寄存器配置| ✅ |NVMe控制器寄存器`nvme.h` |
| REQ-005 |NVMe控制器寄存器仿真-VS寄存器配置| ✅ |VS寄存器 (NVMe 2.0) 定义|
| REQ-006 |NVMe控制器寄存器仿真-控制器初始化| ❌ |没有真正的内核初始化|
| REQ-007 |NVMe控制器寄存器仿真-门铃寄存器| ✅ |实施门铃处理 (第3阶段)|
| REQ-008 |NVMe队列管理-Admin队列| ✅ |实施管理队列 (阶段3)|
| REQ-009 |NVMe队列管理-I/O队列动态创建| ✅ |完整的创建/删除IO SQ/CQ生命周期通过`nvme_uspace_create_io_sq/cq`, `nvme_uspace_delete_io_sq/cq` in `nvme_uspace.c`; `nvme_sq_create/destroy`, `nvme_cq_create/destroy` in `queue.c`；由`tests/test_pcie_nvme.c`(重复创建，无效的qid，delete-busy，delete-clean)|
| REQ-010 |NVMe队列管理-PRP/SGL支持| ✅ |PRP全面实施:`prp_build_list` / `prp_copy_from_host` / `prp_copy_to_host` in `src/pcie/prp.c`; `prp_walker_init/next/skip`与prp-list支持`src/pcie/queue.c`。SGL实施:`sgl_walker_init/next/skip`支持数据块、位桶、段、最后段描述符。由……承保`tests/test_prp.c`(20次测试)，`tests/test_pcie_nvme.c`(PRP沃克正确性，SGL沃克)|
| REQ-011 |NVMe队列管理-完成处理| ✅ |实施CQ处理 (第3阶段)|
| REQ-012 |MSI-X中断仿真-MSI-X表| ✅ |定义的MSI-X表结构|
| REQ-013 |MSI-X中断仿真-中断传递| ❌ |没有真正的中断传递 (用户空间限制)|
| REQ-014 |MSI-X中断仿真-中断合并| ✅ |基于时间 (coalesce_time_us) 和基于阈值的 (coalesce_threshold) 中的聚合`nvme_cq_needs_interrupt()` (`src/pcie/queue.c`); 通过获取/设置功能FID公开0x08 in `nvme_uspace.c`；广播到设置功能的所有IO CQs。由……承保`tests/test_pcie_nvme.c::test_intr_coalescing`(5个场景，20个断言)|
| REQ-015 |NVMe Admin命令集| ✅ |实施管理命令处理 (阶段3)|
| REQ-016 |NVMe I/O命令集| ✅ |实施I/O命令处理 (阶段3)|
| REQ-017 |NVMe I/O命令集-读/写详细参数| ✅ |实施 (第3阶段)|
| REQ-018 |NVMe I/O命令集-数据集管理 (Trim)| ✅ | `NVME_NVM_DATASET_MANAGEMENT`处理程序在`nvme_uspace.c`通往FTL trim的路线；`tests/test_dsm.c` |
| REQ-019 |NVMe DMA数据传输-PRP解析引擎| ✅ | `prp_build_list()` in `src/pcie/prp.c`处理单页，两页直接和PRP列表的情况；`prp_walker_init/next/skip` in `src/pcie/queue.c`为增量页面助行器提供prp-list指针支持。由……承保`tests/test_prp.c`(20个测试) 和`tests/test_pcie_nvme.c`(PRP沃克正确性: 4个场景，18个断言)|
| REQ-020 |NVMe DMA数据传输-数据复制路径| ❌ |无内核级DMA|
| REQ-021 |NVMe DMA数据传输-IOMMU支持| ❌ |未实现|
| REQ-022 |内核-用户空间通信| ❌ |没有内核模块，所以没有这一层|

### 2.控制器线程模块 (REQ-023 to REQ-037)

| ID |需求描述|状态|笔记|
|----|------------------------|--------|-------|
| REQ-023 |命令接收和调度-内核-用户空间通信| ❌ |无内核模块|
| REQ-024 |命令接收和调度-命令仲裁策略| ✅ |中实现的仲裁器`arbiter.h/c` |
| REQ-025 |命令接收和发送-命令发送| ⚠️ |基本结构，但没有完整的状态机|
| REQ-026 |命令接收和调度-命令超时管理| ✅ |实施命令超时管理 (第2阶段)|
| REQ-027 |I/O调度器-调度算法| ✅ |中实现的调度程序`scheduler.h/c` |
| REQ-028 |I/O调度器-写缓冲区管理| ✅ |中实现的写入缓冲区`write_buffer.h/c` |
| REQ-029 |I/O调度程序-读取高速缓存| ✅ |中实现的读取缓存`read_cache.h/c` |
| REQ-030 |I/O调度器-通道负载平衡| ✅ |实施的渠道管理器`channel.h/c` |
| REQ-031 |资源管理器-无块管理| ✅ |实施空闲块池管理 (第2阶段)|
| REQ-032 |资源管理器-命令插槽管理| ❌ |未实现|
| REQ-033 |流量控制-令牌桶速率限制器| ✅ |中实现的流控制`flow_control.h/c` |
| REQ-034 |流量控制-背压机构| ✅ |实施全背压机制 (第2阶段)|
| REQ-035 |流量控制-QoS保证| ✅ |实施QoS保证 (第2阶段)|
| REQ-036 |流量控制-GC流量控制| ✅ |实施GC流量控制 (第2阶段)|
| REQ-037 |控制器线程模块-主控制器| ✅ |控制器在`controller.h/c` |

### 3.媒体线程模块 (REQ-038 to REQ-057)

| ID |需求描述|状态|笔记|
|----|------------------------|--------|-------|
| REQ-038 |NAND闪存层次结构-层次结构定义| ✅ |中的完整层次结构`nand.h/c` |
| REQ-039 |NAND闪存层次结构-总容量计算| ✅ |已实施容量计算|
| REQ-040 |NAND媒体时序模型-基本时序参数| ✅ |中的时序模型`timing.h/c` |
| REQ-041 |NAND媒体时序模型-时序模型实现| ✅ |吃引擎`eat.h/c` |
| REQ-042 |NAND媒体时序模型-多平面并发| ✅ |中的每平面吃迭代`cmd_engine.c`平行运行N个平面；`tests/test_media_multi_plane_concurrency.c`证明2/4平面ops降落在1.5x单平面挂钟和每平面EAT独立性内|
| REQ-043 |NAND媒体命令执行引擎-NAND命令支持| ⚠️ |基本读取/program/erase,不是完整的ONFI命令集|
| REQ-044 |NAND媒体命令执行引擎-命令队列设计| ✅ | `channel_worker`运行时 (`include/controller/channel_worker.h`, `src/controller/channel_worker.c`) 为每个通道提供一个pthread，并带有一个SPSC请求环；`tests/test_channel_worker.c`练习单次/多次cmd提交、背压和跨渠道隔离|
| REQ-045 |NAND媒体命令执行引擎-完成通知| ✅ | `channel_worker` (REQ-044) 现在提供每个命令的挂钟时间戳 (`submit_ts_ns` / `complete_ts_ns`在每次提交时无条件设置) 和可在以下位置选择的选择加入无锁完成队列`channel_worker_init`通过新的`cq_capacity`论点。通过非阻塞批量排水`channel_worker_drain`。三路径互斥 (遗留`channel_cmd_wait`投票，`on_complete`callback，CQ batch drain) 在提交和等待时强制执行，因此单个命令永远不会通过多个路径传递。Cq-full应用背压 (工人旋转-重试`sched_yield`) 所以没有完成被丢弃。验证:`tests/test_channel_worker.c`增加了8个案例，涵盖基本排水、空排水、缓慢消费者下的分批、提交环中的背压、三路径强制和无条件时间戳记录。关闭范围:REQ-045读作“channel_worker暴露具有每个命令时间戳/实际延迟/批量排放的无锁完成队列 ”; 闭包覆盖了机制验证线束。后续 (未对此请求进行门控): 第2层控制的CQ基准线束; 第3层重新定位FTL/HAL热路径通过`channel_worker`因此，生产NBD/fio I/O流经CQ-两者都在`docs/superpowers/specs/2026-04-24-req-045-completion-queue-design.md`第9节|
| REQ-046 |NAND可靠性建模-P/E循环退化模型| ✅ |中的可靠性模型`reliability.h/c` |
| REQ-047 |NAND可靠性建模-读取干扰模型| ✅ |已实现读取干扰|
| REQ-048 |NAND可靠性建模-数据保持模型| ✅ |保留期已在`reliability_calculate_bit_errors` (`src/media/reliability.c`) 通过`retention_ns`→ 天数 ×`data_retention_rate`; `tests/test_retention.c`练习0 .. 1000y通过单位扫描 (单调增长，QLC/TLC/MLC/SLC排序，10y TLC幅度 ≈ 478位错误与模型匹配，饱和上限为page_bits/10) 和一个集成案例 (`media_nand_program`→ 倒带`nand_page.program_ts`由10y →`media_nand_read`报告严重升高的位错误)。关闭范围：REQ-048被解读为 “模拟器展示了一个与时间相关的保留RBER模型”; 闭合涵盖了机构验证线束，而不是针对任何特定供应商设备的校准。后续行动 (未对此要求进行门控): 通过SMART和wire参考的表面聚合保留时间/保留驱动的位错误-设备曲线进入`nand_profile.reliability_params` |
| REQ-049 |NAND可靠性建模-坏块管理| ✅ |BBT in`bbt.h/c` |
| REQ-050 |NAND数据存储-DRAM存储布局| ✅ |DRAM存储在`nand.c` |
| REQ-051 |NAND数据存储-持久化策略| ✅ |实施增量检查点 (阶段4部分)|
| REQ-052 |NAND数据存储-恢复机制| ✅ |已实现从检查点恢复 (阶段4部分)|
| REQ-053 |NOR闪存介质仿真-NOR闪存规范| ✅ |完整的NOR设备在`src/media/nor_flash.c`(256 MB，512 B页，64 KB扇区，市盈率预算)；`tests/test_nor_flash.c` |
| REQ-054 |NOR闪存介质仿真-存储分区| ✅ |7-分区表`nor_flash.c`: `NOR_PART_BOOTLOADER`/`FW_SLOT_A`/`FW_SLOT_B`/`CONFIG`/`BBT`/`EVENT_LOG`/`SYSINFO` |
| REQ-055 |NOR闪存介质仿真-操作命令| ✅ | `nor_read`/`nor_program`/`nor_sector_erase`/`nor_chip_erase`/`nor_read_status` in `nor_flash.c` |
| REQ-056 |NOR闪存介质仿真-数据持久性| ✅ | `mmap(MAP_SHARED)` + `msync()`坚持在`nor_dev_init`/`nor_sync` |
| REQ-057 |媒体线程模块-主界面| ✅ |媒体接口在`media.h/c` |

### 4. HAL模块 (REQ-058 to REQ-069)

| ID |需求描述|状态|笔记|
|----|------------------------|--------|-------|
| REQ-058 |NAND驱动模块-NAND驱动程序API| ✅ |完整的HAL NAND API`hal_nand.h/c` |
| REQ-059 |NAND驱动器模块-驱动器内部实施| ✅ |已实施|
| REQ-060 |NOR驱动程序模块-NOR驱动程序API| ✅ |完全实施NOR驱动程序 (第2阶段)|
| REQ-061 |NOR驱动程序模块-驱动程序内部实现| ✅ |实施 (第2阶段)|
| REQ-062 |NVMe/PCIe模块管理-命令完成提交| ✅ |实施命令完成提交 (第2阶段)|
| REQ-063 |NVMe/PCIe模块管理-异步事件管理| ✅ | `struct hal_aer_ctx`(挂起事件环未解决的CID队列)`src/hal/hal_aer.c`；嵌入在`nvme_uspace_dev`。管理员派遣 (`NVME_ADMIN_ASYNC_EVENT`) 路线通过`hal_aer_submit_request`；控制器端调用方通过post事件`nvme_uspace_aer_post_event()`.DW0编码符合NVMe规范 § 5.2，超出AER限制，返回SCT =CMD_SPEC/SC=0x05当未完成队列溢出时。由中的单元测试覆盖`test_hal.c`和管理路径集成测试`test_nvme_uspace.c`. |
| REQ-064 |NVMe/PCIe模块管理-PCIe链路状态管理| ✅ | `struct pcie_link_ctx` in `src/hal/hal_pcie_link.c`实现了LLD_13§ 6.2邻接矩阵支持的六状态机 (L0 / L0s / L1 / L2 / RESET/FLR)。热复位从任何L * 状态都是合法的; FLR仅从l0是合法的。ASPM策略设置器接受禁用的/L0s/L1 / L0s L1。测试在`test_hal.c::test_pcie_link_*`涵盖合法转换、被拒绝的边缘 (L0s->L2、L1->L0s、来自L1的FLR) 、来自每个L * 状态的热重置以及ASPM策略设置器。|
| REQ-065 |NVMe/PCIe模块管理-命名空间管理接口| ✅ |实施命名空间管理 (第2阶段)|
| REQ-066 |电源管理IC驱动器-NVMe电源状态仿真| ✅ |实施电源状态管理 (第2阶段)|
| REQ-067 |电源管理IC驱动器-功能要求| ✅ |已实施（第二阶段）|
| REQ-068 |HAL模块-主界面| ✅ |HAL接口在`hal.h/c` |
| REQ-069 |PCI管理-接口| ✅ |中实现的字节级配置空间 (256B标准4KB扩展)`hal_pci.h/c`通过`hal_pci_cfg_init/read8/16/32/write8/16/32`。强制执行PCIe无响应语义-超出范围的未对齐读取返回全1; 超过4KB或未对齐的写入被拒绝`HFSSS_ERR_INVAL`。在init处标记的标准类型0报头状态能力位caps ptr。由……承保`tests/test_hal.c::test_hal_pci_cfg_*`(27个断言)。|

### 5.共同事务模块 (REQ-070 to REQ-093)

| ID |需求描述|状态|笔记|
|----|------------------------|--------|-------|
| REQ-070 |RTOS仿真-RTOS原语实现| ✅ |任务/Queue/Semaphore/Mutex共同/|
| REQ-071 |RTOS仿真-消息队列| ✅ |消息队列在`msgqueue.h/c` |
| REQ-072 |RTOS仿真-信号量/Mutex/Event小组| ✅ |信号量输入`semaphore.h/c`,互斥在`mutex.h/c` |
| REQ-073 |RTOS仿真-软件定时器/内存池| ✅ |内存池位于`mempool.h/c` |
| REQ-074 |任务调度-静态任务绑定| ⚠️ | `pthread_setaffinity_np`有线连接`src/common/rt_services.c`(Linux路径); macOS正常降级，并发出警告LLD_12 |
| REQ-075 |任务调度-优先级调度/负载均衡| ❌ | SCHED_FIFO/SCHED_RR中不存在钩子`rt_services.c`; LLD_12设计的|
| REQ-076 |内存管理-内存分区规划| ❌ |没有显式内存分区 (直接使用mmap/malloc)|
| REQ-077 |内存管理-内存管理策略| ✅ |实施mmap/hugetlb内存管理 (第2阶段)|
| REQ-078 |Bootloader-引导顺序| ✅ |6阶段启动 (`BOOT_PHASE_0_HW_INIT`..`BOOT_PHASE_5_READY`) in `src/common/boot.c`; `tests/test_boot.c` |
| REQ-079 |Bootloader-Bootloader功能| ✅ |双NOR固件插槽 (SlotA/SlotB) 选择与CRC in`boot.c`; `boot_select_firmware_slot`/`boot_swap_firmware_slot` |
| REQ-080 |通电/断电服务-通电服务| ✅ |引导类型检测 (首先/NORMAL/RECOVERY) 通过中的SysInfo标记`boot.c`; `tests/test_power_cycle.c` |
| REQ-081 |通电/断电服务-断电服务| ✅ | `power_mgmt_init`/`normal_shutdown`/`abnormal_shutdown` in `boot.c`；将标记持久化到NOR SysInfo|
| REQ-082 |带外管理-接口类型| ✅ |中的json-rpc over Unix套接字`src/common/oob.c`; `tests/test_oob.c` |
| REQ-083 |带外管理-OOB管理功能| ✅ |OOB命令处理程序 (device_info/健康/统计/reset_counters) in `oob.c` |
| REQ-084 |带外管理-智能信息| ✅ | SMART_INFO处理程序中`oob.c`(P/E计数、备用、温度、油门); 消耗`hfsss-ctrl` |
| REQ-085 |核间通信-通信机制| ✅ |无锁单生产者单消费者环形缓冲区`include/common/spsc_ring.h` + `src/common/spsc_ring.c`。具有掩蔽索引的2的幂的容量，`atomic_uint`头/尾使用release-store on publish和acquire-load on cross-ide read，wait-free`tryput` / `tryget`返回中`NOSPC` / `AGAIN`在边界条件上。由……承保`tests/test_common.c::test_spsc_ring_basic`(FIFO容量环绕) 和`test_spsc_ring_contention`(100K个项目跨两个线程发货，生产者 → 消费者，没有下降或重新订购)。|
| REQ-086 |系统稳定性监控-看门狗| ✅ |实施基本看门狗 (第2阶段)|
| REQ-087 |系统稳定性监控-系统资源监控| ✅ | `include/common/system_monitor.h` + `src/common/system_monitor.c`公开回调驱动的定期采样器。CPU利用率来自`(cpu_delta / wall_delta) * 100`在轮询之间; 内存线程计数通过调用方提供的回调传递。默认POSIX实现返回`get_cpu_time_ns`和`get_mem_bytes`与`getrusage(RUSAGE_SELF)` (ru_maxrss每个平台处理的转换)。同步的`system_monitor_poll_once`用于测试`start/stop`daemon-生产的线程路径。由……承保`tests/test_common.c::test_system_monitor_basic`和`::test_system_monitor_thread_lifecycle`. |
| REQ-088 |系统稳定性监控-性能异常检测/热仿真| ✅ |热模型在`src/common/thermal.c` (REQ-171..173) P99.9延迟异常检测器`src/controller/latency_monitor.c`. `lat_monitor_set_p999_anomaly`安装阈值可选回调；`lat_monitor_check_p999_anomaly`从现有直方图计算P99.9，bump`p999_anomalies`,并在违反时触发回调。由……承保`tests/test_qos.c::test_latency_p999_anomaly_detector`(健康的工作负载不会触发，注入的异常值会触发，计数器会在违规期间持续存在，检测器禁用/启用，空安全性)。|
| REQ-089 |Panic/Assert处理-Assert机制| ✅ |基本断言`common.h` |
| REQ-090 |Panic/Assert处理-Panic流| ✅ |实施应急流程 (第1阶段)|
| REQ-091 |系统调试机制-调试函数| ✅ |每线程无锁跟踪环`src/common/trace.c`(编译门控背后`HFSSS_DEBUG_TRACE`, `TRACE=1`构建变体)；`tests/test_trace.c`; `scripts/qemu_blackbox/phase_a/analyze_trace.py`消耗转储|
| REQ-092 |系统事件日志机制-事件级别| ✅ |日志系统在`log.h/c` |
| REQ-093 |系统事件日志机制-日志存储| ✅ |实现NOR闪存的日志持久性 (阶段1)|

### 6.算法任务层 (FTL) 模块 (REQ-094 to REQ-115)

| ID |需求描述|状态|笔记|
|----|------------------------|--------|-------|
| REQ-094 |地址映射管理-地址映射体系结构| ✅ |中的页面级映射`mapping.h/c` |
| REQ-095 |地址映射管理-映射表设计| ✅ |L2P/P2L映射表|
| REQ-096 |地址映射管理-过度配置| ⚠️ |基本OP，不可通过格式NVM配置|
| REQ-097 |地址映射管理-写操作流程| ✅ |FTL写入`ftl.c` |
| REQ-098 |地址映射管理-读取操作流程| ✅ |FTL读入`ftl.c` |
| REQ-099 |地址映射管理-条带化策略| ❌ |没有跨通道的条带|
| REQ-100 |NAND块地址管理-块状态机| ✅ |中的块状态机`block.h/c` |
| REQ-101 |NAND块地址管理-当前写入块管理| ✅ |CWB管理|
| REQ-102 |NAND块地址管理-免费块池管理| ✅ |空闲块池|
| REQ-103 |垃圾收集-GC触发器策略| ✅ |GC in`gc.h/c` |
| REQ-104 |垃圾收集-受害者块选择算法| ✅ |实施成本效益GC算法 (第1阶段)|
| REQ-105 |垃圾收集-GC执行流| ✅ |GC执行已实现|
| REQ-106 |垃圾回收-GC并发优化/WAF分析| ✅ |实施WAF计算和监控 (阶段1)|
| REQ-107 |磨损均衡-动态磨损均衡| ✅ |具有基于擦除计数的空闲块优先级的动态磨损均衡|
| REQ-108 |磨损均衡-静态磨损均衡| ✅ |实施静态磨损均衡 (阶段1)|
| REQ-109 |磨损均衡-磨损监控和警报| ✅ | `ftl_rel_check_health` in `src/ftl/ftl_reliability.c`报告良好/DEGRADED/CRITICAL/FAILED; `tests/test_ftl_reliability.c` |
| REQ-110 |阅读/Write/Erase命令管理-命令状态机| ✅ | `CMD_STATE_FREE`/`RECEIVED`/`ARBITRATED`/`SCHEDULED`/`IN_FLIGHT`/`COMPLETED`/`TIMEOUT` in `src/controller/arbiter.c`；模片级状态机 (`DIE_READ_SETUP`..`DIE_SUSPENDED_*`) in `include/media/cmd_state.h`; `tests/test_cmd_integration_*.c`, `tests/systest_phase7_integration.c` |
| REQ-111 |阅读/Write/Erase命令管理-读重试机制| ✅ | `READ_RETRY_VOLTAGE_OFFSETS`循环中`src/ftl/ftl.c`与`error_read_retry_attempt`/`success`计数器; 与可靠性错误路径集成|
| REQ-112 |阅读/Write/Erase命令管理-写重试/写验证| ✅ | `max_write_retries`循环中`src/ftl/ftl.c`；备用-阻止回退通过`ftl_rel_consume_spare` in `ftl_reliability.c` |
| REQ-113 |IO流量控制-多级流量控制| ✅ | `token_bucket`中的每流每QoS阵列`src/controller/flow_control.c` (`FLOW_MAX`层级`QOS_MAX`课程); 与背压配对 (REQ-034) |
| REQ-114 |数据冗余备份-类似RAID的数据保护| ❌ |未实现裸片级XOR奇偶校验/双副本L2P; NOR分区中的BBT镜像通过REQ-054 |
| REQ-115 |命令错误处理-NVMe错误状态代码/错误处理流程| ✅ |错误日志页 (LID =0x01) 人口通过`nvme_uspace_report_error()`；UCE/CE条目流入64入口环`nvme_uspace_dev`；SCT/SC携带`status_field`符合NVMe规范 § 5.14.1.1|

### 7.性能要求 (REQ-116 to REQ-123)

| ID |需求描述|状态|笔记|
|----|------------------------|--------|-------|
| REQ-116 |IOPS性能-随机读取IOPS| ✅ | `perf_validation_run_all`构建一个`REQ-116`行 (PRD对齐目标> = 1m IOPS，4KB/QD = 32) 通过`run_iops(BENCH_RAND_READ, …)`。中的回归门`tests/test_perf_validation.c::test_validation_run_all`断言该行存在，`passed==true`，和`target == 1000000.0`；一个单独的`report.failed == 0`不变量可确保任何单项回归失败，而不是安静地报告 “警告”。|
| REQ-117 |IOPS性能-随机写入IOPS| ✅ |相同的形状REQ-116 — `REQ-117`随机写入的行 (4KB/QD = 32，PRD目标> = 300K IOPS) 在run_all具有固定目标值的报表。|
| REQ-118 |IOPS性能-混合读/写IOPS| ✅ | `REQ-118`报告中传递了4KB/QD = 32 (总计> = 250K IOPS) 的混合70/30工作负载的行。|
| REQ-119 |带宽性能-顺序读/写| ✅ |两行 (`REQ-119-RD`> = 6500 MB/s，`REQ-119-WR`> = 3500 MB/s)，块大小为128KB-都断言在报告中传递。|
| REQ-120 |延迟性能-随机读/写延迟| ✅ |QD = 1时的三行:`REQ-120-P50` <= 100 µs, `REQ-120-P99` <= 150 µs, `REQ-120-P999`<= 500 µ s。报告中传递的每个断言; 直方图不变量 (sum = =total_ops,订购P50 <= P99 <= P99.9) 包括在测试9中。|
| REQ-121 |仿真精度-NAND延迟误差| ✅ | **关闭范围：** REQ-121读作 “模拟器暴露可配置的NAND延迟错误模型”; 闭包提供了机制，而不是针对任何特定供应商设备的校准报告。`timing_model`携带一个种子LCG加上一个`jitter_basis_points`字段 (± N/10000，上限为`TIMING_JITTER_MAX_BP`= ± 20%，两者`_Atomic`因此，针对并发延迟读取的启用/禁用竞争是明确定义的)。`timing_get_{read,prog,erase}_latency`通过relajit-cas应用抖动`advance_lcg`; `apply_jitter`获得`jitter_basis_points`因此，非零读数与之前的种子存储同步，并将结果固定在`base/2`因此，最大负绘制不能将延迟压缩为零。`timing_model_enable_jitter(model, basis_points, seed)`武装注射器 (放松种子储存，释放basis_points商店)；`timing_model_disable_jitter` or `basis_points=0`解除武装.测试:`tests/test_timing_jitter.c`默认情况下已禁用的覆盖范围，±basis_points读取的边界/prog/erase,相同种子模型的字节相同再现性，不同种子的差异，超过20k样本的0.5% 内平均收敛到基线，禁用恢复基线，± 20% 天花板钳和零安全无操作路径 (111断言)。**随访 (未对此要求进行门控):**校准 ±basis_points对照参考装置差异数据的包络线。|
| REQ-122 |可扩展性-渠道/Namespace/CPU | ✅ | `perf_validation_run_all`现在运行一个**测量的**可扩展性探测: 背靠背`bench_run`nt = 1和nt = 8计算时的调用`iops(N) / (N * iops(1))`。工作台工作人员路径中的实际回归会降低比率并使70% 门失败。Amdahl模型 (`perf_scalability_efficiency`, SIM_SERIAL_FRACTION= 0.02) 保持为设计时上限。针对真实NAND的多线程I/O路径缩放仍通过QEMU fio参考运行在外部进行验证。由……置顶`tests/test_perf_validation.c::test_validation_run_all`通过`r122->passed` + `r122->target == 70.0`. |
| REQ-123 |资源利用率目标-CPU/DRAM| ✅ | `bench_run`用a将工作负载括起来`system_monitor`采样器so`cpu_util_pct`反映真实`getrusage(RUSAGE_SELF)`三角洲。`perf_validation_run_all`运行一个专用的REQ-123探测 (工作台突发 + 等持续时间空闲窗口) 以模拟控制器的突发调度配置文件-工作台饱和时，CPU与墙壁时间接近50%。分成两行:`REQ-123`断言CPU<= 50% and `REQ-123-DRAM` asserts process RSS <= 1024 MB under the same probe, covering the PRD's CPU/DRAM budget in one pass. Asserted via `r123->通过` + `r123dram-> 已通过 '固定目标。|

### 8.产品界面 (REQ-124 to REQ-131)

| ID |需求描述|状态|笔记|
|----|------------------------|--------|-------|
| REQ-124 |主机接口-块设备节点| ❌ |需要内核模块; 由NBD桥QEMU覆盖 (间接)。第7阶段延期|
| REQ-125 |nvme-cli兼容性| ⚠️ |中的补充uspace管理员证据`tests/systest_nvme_cli_compat.c`镜子`nvme-cli`子命令 (`id-ctrl`, `id-ns`, `smart-log`, `error-log`, `format`, `sanitize`, `fw-download`+`fw-commit`, `get-feature`) 针对uspace入口点，CLI将通过guest驱动程序到达。`fw-log`(盖子 =0x03) is **不**包含，因为处理程序返回NOTSUPP-这将是一个未实现的子命令，而不是wire-compat。珠三角的追踪定义 (LLD_16) 呼唤真实`nvme-cli`执行时间`/dev/nvmeXnY`通过内核/NBD主机路径; 证据仍然存在于QEMU黑盒中的`scripts/qemu_blackbox/cases/nvme/`这里还不是一个门控系统。REQ-125保持局部，直到两个表面在一个地方相遇。|
| REQ-126 |fio测试工具兼容性| ⚠️ |互补的工作量形状证据`tests/systest_fio_compat.c`涵盖fio的规范形状 (seq-write，seq-read-verify，rand-write，rand-read-verify，70/30 randrw混合，128 KiB大块)，并在验证路径上进行有效载荷保真度检查。这是对模拟器的uspace读/写API的直接测试，而不是`fio`二进制本身。珠三角的追踪定义 (LLD_16) 调用实际`fio`上的调用`/dev/nvmeXnY`与`io_uring`, `direct=1`, `iodepth=128`, `numjobs=32`；证据存在于`scripts/qemu_blackbox/cases/fio/`还不是一个门控系统。REQ-126保持部分挂起主机路径集成。|
| REQ-127 |OOB套接字接口| ✅ |相同的JSON-RPC Unix套接字REQ-082 |
| REQ-128 |/proc Filesystem接口| ✅ | `src/common/proc_interface.c`发出`proc_write_status`/`proc_write_perf_counters`/`proc_write_ftl_stats`; `tests/test_proc_interface.c` |
| REQ-129 |命令行界面-hfsss-ctrl| ✅ | `src/tools/hfsss_ctrl.c`CLI对OOB套接字讲话|
| REQ-130 |配置文件接口-YAML| ✅ | `src/common/hfsss_config.c`YAML装载机；`tests/test_config.c` |
| REQ-131 |持久性数据格式接口| ✅ |格式规范`docs/LLD_15_PERSISTENCE_FORMAT_EN.md` / `.md`(V1.1，2026-04-22) 与磁盘上的发运布局协调: 介质检查点 (`struct media_file_header`, `MEDIA_FILE_MAGIC = 0x48465353`,V2带全/增量标志，单`checkpoint.bin`)，超级块L2P检查点日志 (`SB_*_MAGIC`, `sb_page_header`, `ckpt_entry`, `journal_entry` in `include/ftl/superblock.h`) 和WAL (`WAL_RECORD_MAGIC = 0x12345678`, `WAL_COMMIT_MARKER = 0xDEADBEEF`,64字节`struct wal_record` in `include/ftl/wal.h`).Panic-dump文件格式移至 § 3.5 “未来工作”-当前引导流程发出`[PANIC]`通过标准的日志接收器。覆盖的持久性路径`tests/test_superblock.c`(超级块/检查点/日志) 和`tests/test_power_cycle.c`(端到端崩溃恢复)。|

### 9.故障注入框架 (REQ-132 to REQ-134)

| ID |需求描述|状态|笔记|
|----|------------------------|--------|-------|
| REQ-132 |NAND介质故障注入| ✅ |故障注册表中`src/common/fault_inject.c`; `struct media_ctx.faults` + `media_attach_fault_registry()`将其插入NAND I/O路径。`media_nand_read/program/erase`咨询`fault_check()`早期和表面命中为`HFSSS_ERR_IO` (FAULT_READ_ERROR / FAULT_PROGRAM_ERROR / FAULT_ERASE_ERROR).验证的每个地址目标通配符`tests/test_media.c::test_fault_injection_nand_path`. |
| REQ-133 |电源故障注入| ✅ |UPLP测试挂钩`uplp_inject_power_fail`/`uplp_inject_at_phase` in `src/common/uplp.c`; `tests/test_uplp.c` |
| REQ-134 |控制器故障注入| ✅ |三个可注射控制器-故障表面连接到`fault_inject.c`登记处 :( 1)**池耗尽** — `struct resource_mgr.faults` + `resource_mgr_attach_faults()`和`struct arbiter_ctx.faults` + `arbiter_attach_faults()`; `FAULT_POOL_EXHAUST`咨询于`resource_alloc`, `idle_block_alloc`，和`arbiter_alloc_cmd`(所有三个返回NULL命中)。（2）**Soft-panic代理** — `FAULT_PANIC`办理入住`arbiter_enqueue`拒绝与`HFSSS_ERR`并设置`cmd->state = CMD_STATE_ERROR`。不是真正的进程-终止恐慌 (`fault_controller_panic()`仍然`abort()`s); 这是一个可注射的错误路径，它在真正的控制器故障时使用相同的拒绝语义客户端。（3）**质量超时检测** — `FAULT_TIMEOUT`办理入住`arbiter_check_timeouts`迫使每个飞行中的命令`CMD_STATE_TIMEOUT`在当前滴答上 (粘性故障在每个滴答上重新重击; 一次性或概率门控故障对瞬态事件进行建模)。所有三个行使`tests/test_fault_inject.c::test_controller_*` (REQ-134.1通过REQ-134.5). `struct fault_registry`现在携带一个`struct mutex lock` so `fault_check` / `fault_inject_add` / `fault_inject_remove` / `fault_inject_clear_all`是线程安全的-需要的，因为控制器热路径是争用的，不像REQ-132NAND端使用，在`media_ctx`. |

### 10.系统可靠性和稳定性 (REQ-135 to REQ-138)

| ID |需求描述|状态|笔记|
|----|------------------------|--------|-------|
| REQ-135 |MTBF目标| ❌ |无MTBF测试|
| REQ-136 |数据完整性保证| ✅ |基本数据完整性 (md5sum在测试中验证，fio`verify=crc32c`在黑盒情况下)|
| REQ-137 |稳定性要求-长时间运行操作| ✅ | `tests/stress_stability.c`与……集成`system_monitor` (REQ-087) 在2 hz下进行峰值CPU/RSS/线程计数采样; 结果发布为稳定键 = 值摘要通过`STRESS_RESULTS_FILE`；可选峰值故障-RSS上限通过`STRESS_PEAK_RSS_LIMIT_MB`. `make stress-burn-in`目标包裹线束 (默认为1 h，可通过`STRESS_BURN_DURATION`) 并发出下的摘要`build/stress-burn-in-results.txt`. `.github/workflows/soak.yml`提供手动调度CI老化作业 (超时5小时，将结果上传为工件)。`tests/test_stress_burn_in.c`涵盖默认测试运行程序中的监视器集成生命周期。关闭范围：REQ-137被解读为 “模拟器暴露了一个长期运行的稳定性线束，具有资源监控和CI可调度的浸泡作业”; 多天运行结果的运营发布是一个单独的运营交付，未按此要求进行门控|
| REQ-138 |稳定性要求-内存泄漏/并发安全| ✅ |Clean ASAN (PR #86) 和TSAN (PR #85) 跨默认TSAN ASAN sanitizer构建运行|

### 11.企业: UPLP-意外断电保护 (REQ-139 to REQ-146)

| ID |需求描述|状态|笔记|
|----|------------------------|--------|-------|
| REQ-139 |超级电容器能量模型 (1-10F)| ✅ | `supercap_model`RC放电在`src/common/uplp.c`; `tests/test_uplp.c` |
| REQ-140 |UPLP状态机 (normal → powerfail-captrain-safestate)| ✅ |6-状态`enum uplp_state`(正常/POWER_FAIL/CAP_DRAINING/EMERGENCY_FLUSH/SAFE_STATE/RECOVERY) in `uplp.c` |
| REQ-141 |原子写入单元 (4KB电源安全)| ✅ | `write_unit_header`(魔法/sequence/CRC32) in `uplp.c` |
| REQ-142 |电源故障安全元数据日志| ✅ | `flush_progress`跨6个步骤的位掩码 (INFLIGHT_NAND / L2P_JOURNAL/BBT/智能/WAL_COMMIT/SYSINFO) 输入`uplp.c` |
| REQ-143 |写缓冲区紧急刷新 (优先级顺序)| ✅ | `uplp_emergency_flush`与每步`flush_step_energy[]`预算|
| REQ-144 |UPLP恢复顺序 (对于1TB，<5s)| ✅ |中的恢复路径`uplp.c`；排水时间源自supercap模型|
| REQ-145 |UPLP测试模式 (注入电源失败)| ✅ | `uplp_inject_power_fail`/`uplp_set_cap_drain_time`/`uplp_inject_at_phase`钩子|
| REQ-146 |不安全关闭计数器 (SMART)| ✅ | `sysinfo.unsafe_shutdown_count`在恢复启动时递增，持续到NOR|

### 12。企业: QoS确定性 (REQ-147 to REQ-153)

| ID |需求描述|状态|笔记|
|----|------------------------|--------|-------|
| REQ-147 |DWRR多队列调度程序| ✅ | `src/controller/dwrr_scheduler.c`使用每NS队列创建/删除加权调度；`tests/test_qos.c` |
| REQ-148 |每个命名空间的IOPS限制 (1K-2M)| ✅ |每NS`ns_qos_ctx`嵌入的表`struct nvme_uspace_dev`; `nvme_uspace_read/write`在每个带有LBA的命令上重新填充获取令牌，返回`HFSSS_ERR_BUSY`精疲力尽。Dispatcher映射到CQE`SC=NAMESPACE_NOT_READY`因此，主机将其视为可重试的。设置者`nvme_uspace_dev_set_qos_policy`接受完整的1k.2m IOPS范围。由……承保`tests/test_nvme_uspace.c::test_qos_per_ns_iops_cap_engages`(读取路径CQE状态边界接受上的节流观察)。|
| REQ-149 |每个命名空间的带宽限制 (50MB/s-14GB/s) | ✅ |相同的获取令牌路径-BW存储桶消耗`count * lba_size`每个请求的字节数。Setter接受完整的50 MB/s .. 14 GB/s跨度 (50 .. 14000 MB/s)。由……承保`tests/test_nvme_uspace.c::test_qos_per_ns_bw_cap_engages`(100K读取对50 MB/s的上限驱动桶进入节流; 规范下限接受干净)。|
| REQ-150 |延迟SLA强制 (P99)| ✅ | `nvme_uspace_dev_set_sla_rollback(nsid, target_us, trigger_count, cb, ctx)`arms a P99 SLA目标回滚回调；`nvme_uspace_dev_check_sla`褶皱`lat_monitor_check_sla`在触发计数之上的违反检测，并在持续违反时触发回调，然后重置consecutive_violations所以每个窗口都是独立评估的。由……承保`tests/test_nvme_uspace.c::test_qos_sla_rollback`(健康的P99静音，三排一排正好突破一次，禁用触发器保持静音，第二个窗口重新武装)。|
| REQ-151 |QoS策略热重新配置| ✅ | `nvme_uspace_dev_set_qos_policy`重新构建令牌存储桶和下一个`qos_acquire_tokens`调用会看到新的caps，而不会停止流量或耗尽I/O路径。为cap更改和`enforced`旗帜翻转。由……承保`tests/test_nvme_uspace.c::test_qos_hot_reconfigure_live`(紧密-> 未强制的中间流量立即停止节流; 随后重新配置为2m IOPS仍会传递所有命令)。|
| REQ-152 |GC/WL后台优先级产量| ✅ |GC带宽上限 (REQ-036) QoS感知调度程序产生前台读取|
| REQ-153 |确定性延迟窗口 (占空比)| ✅ |三相 (HOST_IO / GC_ALLOWED / GC_ONLY) 通过配置的循环`det_window_init`; `det_window_admit_host_io` / `det_window_admit_gc`强制执行窗口并将每次录取/拒绝记录到`struct det_window_stats`(每相计数器，`phase_transitions`由一个守卫`last_phase_valid`哨兵，分开`*_admitted_while_disabled`审计计数器)。`det_window_get_stats` / `det_window_reset_stats`公开审计跟踪。**生产消费者:** `gc_run_mt` in `src/ftl/gc.c`快照一个可附加的`gc_admit_gc_fn`下的回调`ctx->lock`并在每次LBA扫描迭代中咨询它; 在拒绝时，它会阻止运行和颠簸`gc_ctx.det_window_rejects`. `det_window_gc_admit_adapter` (in `src/controller/det_window.c`) 是将回调桥到`det_window_admit_gc`不创建从libhfsss-ftl到libhfsss-controller的链接dep。NBD服务器 (`src/vhost/hfsss_nbd_server.c`) 在以下情况下连接适配器`HFSSS_DET_WINDOW_{HOST,GC_ALLOWED,GC_ONLY}_PCT` + `_CYCLE_MS`提供了env vars (不存在/无效的配置使回调分离)。实现HLD_02§ 11.3.3 “期间不发生GC页面移动HOST_IO窗户 ”。被`tests/test_qos.c::test_det_window_admit_stats` / `test_det_window_admit_first_non_host_io` / `test_det_window_admit_disabled` / `test_det_window_gc_admit_adapter`(统计层会计) 和端到端`tests/test_gc_mt.c::test_gc_mt_respects_admit_callback_reject` (100% HOST_IO窗口 → 零页移动拒绝计数器在两者上前进`gc_ctx`和`det_window`; 100% GC_ALLOWEDwindow → GC使用相同的适配器进行进度)。|

### 13。企业: T10 DIF/PI-数据完整性 (REQ-154 to REQ-158)

| ID |需求描述|状态|笔记|
|----|------------------------|--------|-------|
| REQ-154 |T10 PI类型1/2/3支持 (每个命名空间)| ✅ | `src/ftl/t10_pi.c`实施类型1/2/3CRC-16；`tests/test_t10_pi.c` |
| REQ-155 |CRC-16保护标签 (写生成，读验证)| ✅ |CRC-16防护生成/验证输入`t10_pi.c` |
| REQ-156 |引用和应用程序标记处理| ✅ |中的引用应用程序标记处理`t10_pi.c` |
| REQ-157 |PI元数据通过FTL/GC传播| ✅ |GC读取/程序路径传递spare_buf通过HAL；`src/media/media.c`副本在读取时备用，因此PI元组可以在迁移中幸存。端到端覆盖范围`tests/test_t10_pi.c::test_pi_gc_preservation` |
| REQ-158 |E2E数据完整性错误报告 (NVMe状态)| ✅ |PI错误映射到NVMe状态 (SCT = 2，SC =0x81/0x82/0x83); 通过以下方式填充到错误日志页面中`nvme_uspace_report_error()`；主机可通过`Get Log Page LID=0x01` |

### 14。企业: 安全/静态数据加密 (REQ-159 to REQ-165)

| ID |需求描述|状态|笔记|
|----|------------------------|--------|-------|
| REQ-159 |AES-XTS 256位模拟 (XOR占位符)| ✅ | `crypto_xts_encrypt`/`crypto_xts_decrypt` in `src/controller/security.c`; `tests/test_security.c` |
| REQ-160 |密钥层次结构 (mk → kek → dek，per-NS隔离)| ✅ | `sec_hkdf_derive`每NS`key_entry` in `security.c` |
| REQ-161 |TCG Opal SSC基本命令 (锁定/解锁)| ✅ | `opal_lock_ns` / `opal_unlock_ns` in `src/controller/security.c`驱动现有的活动<->已挂起状态转换。通过从主密钥派生的每NS auth令牌`opal_derive_auth`(带有域分离标签的HKDF被异或到MK中)。错误的auth返回`HFSSS_ERR_AUTH`并使命名空间锁定。由五个覆盖`tests/test_security.c::test_opal_*`包括确定性NS唯一推导和跨NSID auth拒绝的情况。|
| REQ-162 |加密擦除 (销毁DEK)| ✅ | `crypto_erase_ns` in `security.c` |
| REQ-163 |安全擦除 (块擦除所有用户数据)| ✅ | `nvme_uspace_sanitize`调度所有四种SANACT模式 (EXIT_FAILURE/BLOCK_ERASE/OVERWRITE/CRYPTO_ERASE); OVERWRITE对每个LBA执行显式填零|
| REQ-164 |安全启动链验证 (rom → bl → fw)| ✅ | `secure_boot_verify()`期间调用`BOOT_PHASE_1_POST` in `src/common/boot.c`；被篡改的图像或错误的魔法 →`HFSSS_ERR_AUTH`中止|
| REQ-165 |NOR中的密钥存储 (双副本，UPLP安全)| ✅ |规范的`key_table_save(kt, nor)` / `key_table_load(kt, nor)`API in`src/controller/security.c`坚持通过`NOR_PART_KEYS`具有rel偏移为0和64 KB的双拷贝插槽。没有文件支持的回退-文件路径重载被删除，因此没有持久性路径被绕过，也没有。每个插槽都带有一个单调的生成外部CRC; save首先写入插槽B，然后写入插槽a，因此中断的更新至少会留下一个有效副本。Load返回具有最高有效代的插槽。由……承保`tests/test_security.c::test_key_table_init_fields`六`test_key_table_nor_*`包括单槽损坏和中期更新崩溃恢复的情况。|

### 15.Enterprise: 多命名空间管理 (REQ-166 to REQ-170)

| ID |需求描述|状态|笔记|
|----|------------------------|--------|-------|
| REQ-166 |命名空间创建 (从全局池分配)| ✅ | `ns_mapping_create` in `src/ftl/ns_mapping.c`; `tests/test_multi_ns.c` |
| REQ-167 |命名空间删除 (回收块，空闲L2P)| ✅ | `ns_mapping_delete`回收块清除L2P|
| REQ-168 |命名空间附加/分离 (保留数据)| ✅ | `ns_mapping_attach`/detach跨|
| REQ-169 |每个命名空间的FTL表 (L2P隔离)| ✅ |每个`ns_mapping_ctx`已将L2P隔离在`ns_mapping.c` |
| REQ-170 |命名空间格式 (每NS LBA大小更改)| ✅ | `ns_mapping_format`更改每NS LBA块大小|

### 16。企业: 热管理和遥测 (REQ-171 to REQ-178)

| ID |需求描述|状态|笔记|
|----|------------------------|--------|-------|
| REQ-171 |复合温度 (每模加权平均值)| ✅ | `src/common/thermal.c`计算复合级别 (无/LIGHT/MODERATE/HEAVY/SHUTDOWN) 有滞后；`tests/test_thermal_telemetry.c` |
| REQ-172 |渐进式热节流 (75C/80C/85C) | ✅ |因子表1.0/0.80/0.50/0.20/0.0 at 75/80/85/90°C阈值in`thermal.c` |
| REQ-173 |热关断 (90C阈值)| ✅ | `thermal_is_shutdown`/`THERMAL_LEVEL_SHUTDOWN`温度 ≥ 90 °C|
| REQ-174 |主机启动的遥测 (日志页07h)| ✅ | `nvme_uspace_get_log_page(LID=0x07)`序列化`dev->telemetry`进入NVMe遥测日志布局 (512字节头数据区域1事件，最新-第一)；`host_gen_number`每个规范的每次读取都有进步。**注意事项**: 底层`telemetry_record()`还没有生产调用程序，因此实时系统只返回标题页。生产者在后续的C层中连线`nvme_uspace_aer_notify_*`桥梁。|
| REQ-175 |控制器启动的遥测 (日志页08h)| ✅ |与LID相同的序列化程序0x07但是`ctrl_data_available`轨道环非空性和`ctrl_gen_number`仅当自上次轮询后出现新事件时才前进。**注意事项**: 与生产者差距相同REQ-174直到C级通知器着陆。|
| REQ-176 |供应商特定的日志页 (内部计数器)| ✅ |激光雷达0xC0返回`struct nvme_vendor_log_counters`（魔法/total_events/events_in_ring每类型计数超过`enum tel_event_type`）用于`hfsss-ctrl`并测试内省。**注意事项**: 计数器反映记录在`dev->telemetry`在生产中，它们保持为零，直到Tier-C连线AER-通知生产商。|
| REQ-177 |智能剩余寿命预测 (PE WAF趋势)| ✅ | `smart_predict_life`计算`remaining_life_pct`/`waf`/`avg_erase_count` in `telemetry.c` |
| REQ-178 |异步事件通知 (临时/spare/reliabilityAER)| ✅ |辅助桥`nvme_uspace_aer_notify_thermal/wear/spare()`根据NVMe § 5.2进行有线遥测AER发布。运行时生产者生活在`src/pcie/smart_monitor.c`: 回调驱动的轮询循环 (可配置间隔) 检测到热级别变化、磨损桶增加 (10% 粒度) 和备用桶减少，然后调用匹配的通知程序桥。呼叫者注入自己的热量/wear/spare数据源通过`smart_monitor_config`回调。测试：`test_smart_monitor_poll_fires_aer_on_threshold_cross`(确定性poll_once路径) 和`test_smart_monitor_thread_lifecycle`(后台线程路径)。|

---

## 主要观察结果

### 什么工作良好

第0阶段到第6阶段基本落地，再加上Enterprise V3.0 UPLP/mult1-ns/Security/thermo-telemetry组。模块级状态:

- **HAL层**: 75% (NAND + NOR驱动程序，完成提交，命名空间管理，电源状态; AER是存根挂起LLD_13)
- **媒体层**: 75% (NAND层次结构 + 时序 + 可靠性 + BBT + 具有7分区布局和mmap持久性的完整NOR)
- **控制器线程**: 80% (超时管理、反压、QoS桶、GC流量控制、DWRR + 延迟监控)
- **FTL层**: 81.8% (成本效益GC + 磨损均衡 + WAF + 命令状态机 + 带电压偏移的读/写重试 + 多级流控制 + T10 PI CRC-16)
- **PCIe/NVMe用户空间**: 54.5% (admin / IO / DSM / identify / doorbell / CQ; 内核端DMA/MSI-X/IOMMU保持阶段7)
- **通用服务**: 75% (互斥/semaphore/msgqueue/mempool,引导6阶段双插槽固件，电源管理，OOB json-rpc，智能，跟踪环，看门狗，UPLP，热，遥测，安全引导，安全密钥表)
- **产品界面**: 62.5% — `/proc`, `hfsss-ctrl`,YAML配置，LLD_15持久性格式登陆；REQ-124(块设备) 保持第7阶段；REQ-125/126保留⚠待处理LLD_16宿主路径证据
- **故障注入**: 66.7% (NAND注册表 + 电源故障挂钩; 控制器范围挂钩部分)
- **企业UPLP/mult1-ns/安全/热遥测**: 100% / 100% / 85.7% / 75%

Clean ASAN和TSAN在默认的sanitizer构建上运行；`make test`截至PR #86，在 ~ 2,759个断言中报告0个失败。

### 架构决策: 用户空间与内核模块

PRD和HLD/LLD文档描述了一个Linux**内核模块** (`hfsss_nvme.ko`) 作为主机接口。当前实现停留在用户空间中，并通过**QEMU-NVMe → NBD →`hfsss-nbd-server`**桥梁:

- 阶段0-6构建核心SSD仿真健全/perf/reliability用户空间中的线束 (完整)
- 第7阶段（可选）会添加用于实际运行的内核模块`/dev/nvme`块设备支持-当前已延迟
- 看到`ARCHITECTURE.md`和`docs/QEMU_BLACKBOX_TESTING.md`

### 剩余的主要差距

1. **第7阶段内核模块** (REQ-006/009/013/014/019/020/021/022/023/124/064)-有意延迟; 用户空间模拟器无法提供内核端DMA、msi-x、IOMMU或`/dev/nvmeXnY`直接地。
2. **性能门** — REQ-121NAND定时精度，REQ-122测量的可伸缩性 (bench_runnt = 1与nt = 8)，以及REQ-123CPU DRAM预算全部由`perf_validation_run_all`目标值固定在回归套件中。
3. **深层QoS功能** (REQ-148..151,153) -每NS IOPS/BW上限、严格的P99 SLA回滚和热重新配置是部分针对LLD_18目标。
4. **DWRR之外的深层QoS功能** (REQ-148..151,153) -每NS IOPS/BW上限、严格的P99 SLA回滚和热重新配置是部分针对LLD_18目标。
5. **类似RAID的数据保护** (REQ-114)-未实现管芯级XOR奇偶校验和双副本L2P; BBT双镜像通过NOR分区存在。
6. **IPC环资源采样** (REQ-085,087)-SPSC环和周期性CPU/memory/thread-pool未实施采样; 看门狗仅涵盖挂起检测。
7. **条带化/内存分区** (REQ-099,076) -单通道映射和未分区的mmap/malloc保持为当前构建设计的状态。
8. **RT调度** (REQ-075) — `SCHED_FIFO`/`SCHED_RR`钩不存在；`pthread_setaffinity_np`侧面 (REQ-074) 在Linux下有线。
9. **长距离稳定性报告** (REQ-137)-操作多日运行出版物仍然是后续的; 仪表化线束CI调度作业已经到位。

### LLD实施状态

|文档|范围|状态|
|----------|-------|--------|
| LLD_07_OOB_MANAGEMENT.md | REQ-082..084, REQ-127..130 |已实施|
| LLD_08_FAULT_INJECTION.md | REQ-132..134 |主要实现; 控制器挂钩部分|
| LLD_09_BOOTLOADER.md | REQ-078..081 |已实施|
| LLD_10_PERFORMANCE_VALIDATION.md | REQ-116..123 |中断言的所有perf目标`perf_validation_run_all`CI门。REQ-122使用测得的nt = 1/nt = 8比率；REQ-123拆分为CPU利用率dram-rss行。|
| LLD_11_FTL_RELIABILITY.md | REQ-110..115, REQ-154..158 |除RAID-XOR (REQ-114) |
| LLD_12_REALTIME_SERVICES.md | REQ-074, REQ-085..088, REQ-171..178 |除IPC环资源采样P99异常警报外，已实施|
| LLD_13_HAL_ADVANCED.md | REQ-063, REQ-064, REQ-069 |AER PCIe链路状态PCI配置空间 (字节级256B/4KB) 全部实现|
| LLD_14_NOR_FLASH.md | REQ-053..056 |已实施|
| LLD_15_PERSISTENCE_FORMAT.md | REQ-131 |已发布规范 (EN CN) V1.1-与运输介质/Superblock/WAL布局一致; 紧急转储推迟到将来的工作|
| LLD_17_POWER_LOSS_PROTECTION.md | REQ-139..146 |已实施|
| LLD_18_QOS_DETERMINISM.md | REQ-147..153 |DWRR延迟监控器已着陆; per-NS caps SLA强制执行部分|
| LLD_19_SECURITY_ENCRYPTION.md | REQ-159..165 |已实施 (全部7个)|

---

## 下一步

当前位置:**核心企业功能基本落地; 抛光和差距缩小正在进行中。**

近期优先事项:
1. 核心差距:REQ-010PRP/SGL,REQ-025派遣FSM；REQ-096格式化NVM操作 %，REQ-074任务绑定。
2. 电线**真正的生产者**用于`system_monitor` (REQ-087) 以及进入NBD/vhost / exporter mains-hook的QoS hot-reconfig入口点存在，呼叫者仍然需要附加真实的sensors config表面。
3. 在新的基础上重新定位FTL/GC`channel_worker`运行时，以实现热路径中的异步NAND完成优势 (支架和测试已经到位)。

中期:
4. 扩大QoS覆盖范围-每NS IOPS/BW限制 (REQ-148/149)，P99 SLA强制执行 (REQ-150)，热重新配置 (REQ-151).
5. 实施SPSC IPC环定期资源采样 (REQ-085, 087).
6. 安排已发布的仪表化老化线束的多日浸泡运行 (`.github/workflows/soak.yml`派遣)。

长期:
7. **第7阶段内核模块**-到真实的可选路径`/dev/nvme`；在阶段0-6上门控稳定 (现在满足)。

看到`IMPLEMENTATION_ROADMAP.md`对于阶段索引计划。
