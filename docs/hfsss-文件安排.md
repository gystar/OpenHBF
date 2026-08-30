# HFSSS `src` 文件功能说明

## 1. 文档范围

本文说明 `thirdparty/hfsss-simulator/src` 下全部 **106 个源码文件**的用途。说明以当前代码为准，尽量使用简单语言；“占位”“框架”表示接口已经存在，但功能还没有完整实现。

| 目录 | 文件数 | 主要职责 |
|------|-------:|----------|
| `common` | 21 | 日志、内存、线程同步、监控、掉电保护等公共服务 |
| `controller` | 15 | 请求调度、缓存、QoS、资源和通道管理 |
| `ftl` | 28 | 地址映射、写入分配、GC、恢复和磨损管理 |
| `hal` | 7 | FTL 与模拟硬件之间的统一接口 |
| `kernel` | 4 | Linux 内核与用户态模拟器的桥接框架 |
| `media` | 13 | NAND/NOR 数据、命令、时序和可靠性模型 |
| `pcie` | 10 | 用户态 PCIe/NVMe 控制器模型 |
| `perf` | 1 | 性能目标验证 |
| `tools` | 1 | 管理命令行工具 |
| `vhost` | 5 | NBD 和 vhost-user-blk 前端 |
| `src` 根目录 | 1 | 整个 SSD 模拟核心的统一入口 |

## 2. 整体数据流

一次普通读写大致经过下面这些层：

```text
QEMU / Linux / 测试程序
        |
        v
NBD、vhost-user-blk 或 PCIe/NVMe 前端
        |
        v
Controller：排队、缓存、QoS、资源控制
        |
        v
FTL：LBA 映射、写入分配、GC、恢复日志
        |
        v
HAL：统一硬件接口
        |
        v
Media：NAND 数据、命令状态、时序、坏块和可靠性
```

`common` 为各层提供日志、同步、监控、故障注入和掉电保护。不同运行方式不会一定经过图中的每一层，例如 `sssim.c` 可直接让调用者进入 FTL。

## 3. `src/common`：公共服务（21 个）

- **`common/boot.c`**：模拟 SSD 从上电到可用的启动过程，包括固件选择、安全校验、系统信息检查和异常关机恢复；也提供 CRC 和 WAL 恢复辅助功能。
- **`common/fault_inject.c`**：登记并触发测试故障，例如位翻转、介质老化、超时、掉电和控制器崩溃；支持地址匹配、概率触发和 JSON 导入导出。
- **`common/hfsss_config.c`**：生成、读取、保存和检查模拟器配置，并允许按名称读取或修改配置项。
- **`common/log.c`**：提供线程安全的分级日志；严重错误时打印位置和调用栈并终止程序。
- **`common/memory.c`**：申请和释放大块内存，并尽量使用 huge page 和锁页；也能在大块内存中快速切分小块。
- **`common/mempool.c`**：预先建立固定大小的内存块池，让频繁的小对象申请和归还更快。
- **`common/msgqueue.c`**：实现线程间有界消息队列，支持立即返回或等待超时。
- **`common/mutex.c`**：把 POSIX 递归互斥锁包装成项目统一接口；注意当前“超时加锁”实际仍是阻塞加锁。
- **`common/oob.c`**：通过 Unix socket 提供 JSON-RPC 带外管理，可查询或控制状态、SMART、性能、温度、GC、跟踪、日志、配置、QoS 和遥测。
- **`common/proc_interface.c`**：把状态、性能和 FTL 统计写成文本文件，提供类似 Linux `/proc` 的查看方式。
- **`common/reliability.c`**：提供命名空间限流、通道并发限制、带 CRC 的元数据双副本和数据完整性测试。
- **`common/rt_services.c`**：集中管理 CPU 亲和性、线程消息通道、低开销跟踪和 CPU/延迟异常监测。
- **`common/semaphore.c`**：实现计数信号量，用于限制并发资源或通知等待线程。
- **`common/spsc_ring.c`**：实现单生产者、单消费者无锁环形队列，容量必须是 2 的幂。
- **`common/system_monitor.c`**：采集进程 CPU、内存和线程数，超过阈值时发出告警。
- **`common/telemetry.c`**：保存最近的遥测事件，并根据擦除次数和写放大估算剩余寿命。
- **`common/thermal.c`**：根据温度计算降速等级、性能比例和是否需要关机，并统计各温度等级持续时间。
- **`common/ticket_lock.c`**：实现先到先得的轻量自旋锁，适合持锁时间很短的路径。
- **`common/trace.c`**：用每线程缓冲区记录高频 I/O 跟踪事件，结束时写入文件；也提供 CRC32C。
- **`common/uplp.c`**：模拟意外掉电保护，包括超级电容放电、紧急刷写、torn write 检测和指定阶段掉电注入。
- **`common/watchdog.c`**：后台检查任务是否及时“喂狗”，超时后调用上层处理函数。

## 4. `src/controller`：控制器核心（15 个）

- **`controller/arbiter.c`**：管理命令对象并按优先级仲裁，跟踪命令状态、执行超时和控制器故障注入。
- **`controller/channel.c`**：维护 NAND 通道状态，从较空闲的通道中选择请求目标，并用 LBA 帮助均匀分流。
- **`controller/channel_worker.c`**：为每个 NAND 通道运行工作线程，执行读、写、擦除并通知完成。
- **`controller/controller.c`**：控制器总装入口，创建、启动和清理仲裁器、调度器、缓存、通道、资源和流量控制模块；共享内存接口目前仅预留。
- **`controller/det_window.c`**：把时间分成仅主机 I/O、主机与 GC 共用、仅 GC 三种固定窗口，并统计允许和拒绝次数。
- **`controller/dwrr_scheduler.c`**：用 DWRR 在命名空间之间按权重公平调度，也可整体降速。
- **`controller/flow_control.c`**：使用令牌桶限制不同命令、QoS 和 GC 流量，并根据写缓冲和空闲块产生背压。
- **`controller/latency_monitor.c`**：记录延迟直方图，计算 P50、P95、P99、P99.9 并检查 SLA 是否超标。
- **`controller/qos_policy.c`**：保存每个命名空间的 IOPS、带宽、突发额度和延迟目标，并判断请求能否进入。
- **`controller/read_cache.c`**：实现 LRU 读缓存；写入覆盖相同 LBA 时可让旧缓存失效。
- **`controller/resource.c`**：管理固定对象池、空闲 NAND 块和 CPU 角色统计，并在资源不足时提示 GC。
- **`controller/scheduler.c`**：提供通用命令队列；FIFO 已实现，Greedy、Deadline 和 WRR 目前仍按 FIFO 工作，DWRR 通用出队路径也未完整接通。
- **`controller/security.c`**：管理命名空间密钥、锁定和加密擦除；当前“AES-XTS”只是流程模拟用的 XOR，占位而非真实加密。
- **`controller/shmem_if.c`**：通过 POSIX 共享内存在用户态模拟器和内核侧之间传递 NVMe 命令与完成项。
- **`controller/write_buffer.c`**：暂存主机写入并支持读命中；当前 flush 主要清除脏标记，没有直接把数据写入 NAND。

## 5. `src/ftl`：闪存转换层（28 个）

- **`ftl/backend_sched.c`**：每通道运行后端线程，按优先级调度 NAND 读写擦，并限制总并发和 GC 并发。
- **`ftl/block.c`**：管理物理块的空闲、打开、关闭、GC、坏块和保留状态，记录有效页和擦除次数，并选择 GC 牺牲块。
- **`ftl/data_superblock.c`**：把多个 NAND lane 的块组成并行写入组，负责分配、关闭、回收和选择 DSB 级 GC 目标。
- **`ftl/die_dispatcher.c`**：die 忙时按四级优先级排队等待，空闲后公平唤醒请求，并感知主机压力。
- **`ftl/die_dispatcher_internal.h`**：定义 die 调度器的内部队列、等待者、公平统计等结构，也供测试检查内部状态。
- **`ftl/ecc.c`**：ECC 占位实现；目前只复制数据并清空校验区，不会真正计算或纠错。
- **`ftl/error.c`**：累计读重试、重试成功、写后校验和校验失败等错误统计。
- **`ftl/ftl.c`**：FTL 总入口，实现初始化、读、写、TRIM、FLUSH、检查点和统计，并串联映射、分配、GC、日志和 HAL。
- **`ftl/ftl_backend_exec.c`**：把 FTL 的同步 NAND 操作包装成后端任务，调度后调用 HAL 并返回结果。
- **`ftl/ftl_debug.c`**：生成多线程 FTL 的只读调试快照，包括线程、流、映射日志和 TRIM 日志状态。
- **`ftl/ftl_map.c`**：实现按页懒分配、支持并发更新的运行时 L2P/P2L 表，并为 GC 提供条件替换以避免覆盖新写入。
- **`ftl/ftl_map_log.c`**：用内存环形日志记录 LBA 映射变化，满时覆盖旧记录并统计丢失数。
- **`ftl/ftl_reliability.c`**：统计块磨损和备用块，给出 GOOD、DEGRADED、CRITICAL、FAILED 健康状态。
- **`ftl/ftl_stream.c`**：按覆盖、连续写等特征把写入分到默认、热数据、顺序和 GC 流。
- **`ftl/ftl_trim_log.c`**：记录并自动合并相邻或重叠的 TRIM LBA 范围。
- **`ftl/ftl_worker.c`**：运行读、修改、GC 和磨损检查线程；读可并行，写/TRIM/FLUSH 由单修改线程处理，大 TRIM 会切片。
- **`ftl/gc.c`**：搬走牺牲块中的有效页、条件更新映射、擦除旧块并回收空间，是垃圾回收的核心实现。
- **`ftl/gc_thread.c`**：后台等待空间不足信号并定期检查空闲块，达到阈值后启动 GC。
- **`ftl/io_queue.c`**：实现 FTL 工作线程使用的单生产者、单消费者无锁请求/完成队列。
- **`ftl/mapping.c`**：传统全局 L2P/P2L 表，供旧单线程路径、恢复和兼容代码使用。
- **`ftl/ns_gc.c`**：按各命名空间的空间压力分配有限 GC 预算，并计算各命名空间写放大。
- **`ftl/ns_mapping.c`**：创建、删除、查询和格式化命名空间，分配连续 LBA 范围；实际页映射仍交给映射模块。
- **`ftl/superblock.c`**：在保留 NAND 块中保存检查点、块/DSB 状态和映射/TRIM 日志，启动时回放并恢复 FTL。
- **`ftl/t10_pi.c`**：生成和检查 T10 PI 的 CRC-16 guard tag 与 reference tag。
- **`ftl/taa.c`**：把 LBA 映射表分片并分别加锁，让不同地址范围可以并行查询和更新。
- **`ftl/wal.c`**：实现预写日志，记录、提交、回放和截断写入/TRIM/映射操作，也可保存到文件。
- **`ftl/wear_level.c`**：计算块磨损差异并判断是否需要静态均衡；实际数据搬移目前仍是占位。
- **`ftl/wl_thread.c`**：后台定期扫描擦除次数并记录静态均衡需求，目前只统计而不搬移数据。

### 三套映射结构的区别

- `mapping.c`：简单的传统全局表，主要服务旧路径和恢复。
- `taa.c`：按 LBA 范围分片，主要服务多线程 FTL 路径。
- `ftl_map.c`：按页懒分配并使用原子版本，强调运行时并发和 GC 条件更新。

## 6. `src/hal`：硬件抽象层（7 个）

- **`hal/hal.c`**：HAL 总入口，把 NAND、NOR、PCI 和功耗设备挂到统一上下文，并向 FTL 提供同步操作接口。
- **`hal/hal_aer.c`**：分别排队等待中的 NVMe AER 请求和已发生事件，双方都有时按顺序配对完成。
- **`hal/hal_nand.c`**：保存 NAND 几何参数并检查命令，再把读、写、擦、坏块操作转给 `media`。
- **`hal/hal_nor.c`**：在内存中模拟 NOR 的读、只能 1 变 0 的写、擦除和文件持久化。
- **`hal/hal_pci.c`**：模拟 PCI 配置空间和 NVMe 命名空间管理；这是用户态后端，不是 Linux 驱动。
- **`hal/hal_pcie_link.c`**：模拟 PCIe 链路状态、ASPM、热复位和 FLR，并统计状态停留和非法切换。
- **`hal/hal_power.c`**：模拟 NVMe PS0--PS4 功耗状态、切换延迟和各状态停留时间。

## 7. `src/kernel`：Linux 内核桥接框架（4 个）

- **`kernel/hfsss_nvme_kmod.c`**：注册 HFSSS 虚拟 PCI 驱动并负责 probe/remove；当前仍是框架，尚未完整接入 Linux blk-mq。
- **`kernel/hfsss_nvme_pci.c`**：启用 PCI 设备、映射 BAR0，维护 NVMe 寄存器影子和 CC.EN/复位带来的就绪变化。
- **`kernel/hfsss_nvme_queue.c`**：管理 Admin/I/O 队列和 MSI-X；真正的中断完成与 blk-mq 路径目前仍是桩。
- **`kernel/hfsss_nvme_shmem.c`**：实现内核与用户态之间的共享内存双环；用户进程 mmap 字符设备和真实请求完成尚未接通。

## 8. `src/media`：介质模型（13 个）

- **`media/bbt.c`**：管理坏块表和每块擦除次数，并支持随介质镜像保存和恢复。
- **`media/cmd_engine.c`**：NAND 命令执行核心，处理读、编程、擦除、复位、状态、身份、多平面、缓存和暂停/恢复命令，并结合状态机和时序推进各阶段。
- **`media/cmd_legality.c`**：定义每种 die 状态允许哪些命令，以及命令接受、完成或中止后的下一状态。
- **`media/cmd_stage_timing.c`**：把命令总耗时拆成 setup、array busy 和 data transfer 阶段，特别处理 cache read/program 的重叠时间。
- **`media/cmd_state.c`**：保存和更新每个 NAND die 当前命令、目标、阶段、完成时间和中止状态。
- **`media/eat.c`**：记录通道、chip、die、plane 何时可再次访问，用资源的“最早可用时间”模拟并行冲突。
- **`media/media.c`**：介质层总实现，完成 NAND 读写擦、故障和位错误注入、多平面/缓存操作、统计，以及全量/增量镜像和检查点持久化。
- **`media/nand.c`**：建立和释放 channel/chip/die/plane/block/page 的 NAND 层级数据结构，并检查物理地址是否合法。
- **`media/nand_identity.c`**：根据配置生成 NAND ID、ONFI 参数页、CRC 和普通/增强状态返回值。
- **`media/nand_profile.c`**：定义 SLC/MLC/TLC/QLC 的 ONFI 与 JEDEC Toggle profile，包括时序、支持命令和多平面限制。
- **`media/nor_flash.c`**：实现文件映射式 NOR 设备、固定分区、读、编程、扇区/整片擦除、状态和持久化同步。
- **`media/reliability.c`**：根据 NAND 类型、P/E 次数、读取次数和保存时间估算位错误与坏块风险。
- **`media/timing.c`**：计算读、编程、擦除、暂停/恢复和 cache busy 延迟，并可加入可重复的随机抖动。

## 9. `src/pcie`：用户态 PCIe/NVMe（10 个）

- **`pcie/dma.c`**：根据 PRP 建立分段并在主机地址与连续缓冲区之间模拟 DMA 搬运。
- **`pcie/msix.c`**：模拟 MSI-X 向量表和 Pending Bit Array；屏蔽期间先记账，取消屏蔽后再触发。
- **`pcie/nvme.c`**：维护 NVMe 控制器寄存器和状态，生成 Identify 数据，并校验 Admin/I/O 命令及完成状态。
- **`pcie/nvme_uspace.c`**：用户态 NVMe 的核心编排层，把 Admin/I/O SQE 转成 `sssim`/FTL 操作，并实现 Features、日志页、AER、SMART、格式化、Sanitize、固件和 QoS 等功能。
- **`pcie/pci.c`**：提供 PCI 配置空间、BAR 和 MSI-X 外壳，把 BAR0 转给 NVMe 寄存器、BAR4 转给 MSI-X。
- **`pcie/pcie_nvme.c`**：把 PCI、NVMe、队列、DMA、MSI-X 和共享内存装配成一个完整用户态设备对象。
- **`pcie/prp.c`**：解析 PRP1、PRP2 和 PRP list，检查对齐和长度，并遍历分散的主机内存。
- **`pcie/queue.c`**：管理用户态 NVMe SQ/CQ、门铃、命令获取、完成投递、phase bit 和中断合并计数。
- **`pcie/shmem.c`**：实现 POSIX 共享内存命令环，并在内部槽格式与 NVMe SQE 之间转换。
- **`pcie/smart_monitor.c`**：后台监视温度、寿命和备用空间，只在跨过重要阈值时产生 AER，避免重复告警。

## 10. `src/vhost`：QEMU/外部主机前端（5 个）

- **`vhost/hfsss_img_export.c`**：创建指定大小的稀疏 raw 文件并打印 QEMU 示例；当前不会导出真实 FTL/NAND 内容，也不是实时后端。
- **`vhost/hfsss_nbd_server.c`**：NBD 服务主程序，把网络读、写、FLUSH 和 TRIM 转成 NVMe/FTL 操作，支持同步、多线程和异步模式。
- **`vhost/hfsss_vhost_main.c`**：配置并启动用户态 NVMe 和 vhost-user-blk Unix socket，管理服务进程生命周期。
- **`vhost/nbd_async.c`**：用固定槽跟踪异步 NBD 请求，拆分并投递 FTL I/O，再汇总乱序完成并回复客户端。
- **`vhost/vhost_user_blk.c`**：实现 vhost-user/virtio-blk 协议、共享内存和 vring，把 guest 请求转给用户态 NVMe 后端并通知完成。

## 11. 其他文件（3 个）

- **`perf/perf_validation.c`**：用内存工作负载和简化时序模型验证 IOPS、吞吐、延迟、NAND 时序和多线程扩展目标，并输出文本或 JSON 报告；它不执行真实 NAND/FTL I/O。
- **`tools/hfsss_ctrl.c`**：连接 OOB Unix socket 的命令行工具，可查看状态、SMART、性能、日志和配置，并触发 GC、跟踪开关或性能计数重置。
- **`sssim.c`**：整个 SSD 核心的统一 C API，设置默认几何，按 Media -> NOR/NAND HAL -> HAL -> FTL 顺序初始化，提供读、写、TRIM、FLUSH、统计和安全关机持久化。

## 12. 当前实现中需要特别注意的占位功能

- `ftl/ecc.c` 还没有真正的 ECC 编解码和纠错。
- `controller/security.c` 还没有真正的 AES-XTS 加密。
- `controller/write_buffer.c` 的 flush 没有直接落到 NAND。
- `ftl/wear_level.c` 和 `ftl/wl_thread.c` 能判断需要均衡，但还不搬移数据。
- `controller/scheduler.c` 中除 FIFO 外的多种通用策略没有完全实现或接通。
- `src/kernel` 是 Linux 驱动桥接框架，尚不是完整可用的 blk-mq NVMe 驱动。
- `vhost/hfsss_img_export.c` 只创建空稀疏 raw 文件，不会导出模拟介质数据。
- `perf/perf_validation.c` 使用独立的简化模型，结果用于目标验证，不等同于端到端实测。
