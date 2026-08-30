# HFSSS架构概述

**项目** ：高保真全栈SSD模拟器(HFSSS)
版本1.0

---

## 概述

HFSSS是用C编写的高保真SSD模拟器，它模拟从主机接口到NAND闪存介质的完整SSD堆栈。虽然原始设计文档描述了Linux内核模块架构，但当前的实现是一个纯粹的用户空间库，它提供了：

- 具有定时模型的NAND闪存介质模拟
- 硬件抽象层
- 带垃圾收集功能的闪存转换层(FTL)
- 通用服务（日志记录、内存池、消息队列等）
- 顶层 SSD 模拟器接口

---

## 主机接口后端

虽然核心模拟器是一个独立的库，但它通过两个主机接口后端将其功能暴露给外部世界（例如QEMU ）。这些后端充当服务器，将标准块协议转换为对模拟器内部`nvme_uspace` API的调用。

```
┌──────────────────┐   (Unix Socket)   ┌───────────────────┐
│  QEMU via        ├───────────────────►  hfsss-vhost-blk  │
│ vhost-user-blk-pci │                   │ (vhost_user_blk.c)│
└──────────────────┘                   └────────┬──────────┘
                                                  │
┌──────────────────┐   (TCP Socket)    ┌───────────────────┐
│  QEMU via NBD    ├───────────────────►  hfsss-nbd-server │
└──────────────────┘                   │ (hfsss_nbd_server.c)│
                                         └────────┬──────────┘
                                                  │
                               ┌──────────────────▼──────────────────┐
                               │      nvme_uspace_dev Interface      │
                               │ (read, write, flush, trim)          │
                               └──────────────────┬──────────────────┘
                                                  │
                               ┌──────────────────▼──────────────────┐
                               │         Core SSD Simulator          │
                               │ (FTL, HAL, Media)                   │
                               └─────────────────────────────────────┘
```

---

## 实际实现的架构

```
┌─────────────────────────────────────────────────────────────────┐
│                     User Application                              │
│  (tests, custom programs using sssim.h)                         │
└────────────────────────┬────────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────────┐
│                   Top-Level Interface (sssim.h)                  │
│  sssim_init(), sssim_read(), sssim_write(), sssim_trim()       │
└────────────────────────┬────────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────────┐
│                   FTL Layer (ftl.h)                              │
│  - Address Mapping (L2P/P2L tables)                             │
│  - Block Management                                               │
│  - Garbage Collection (Greedy algorithm)                         │
│  - Wear Leveling                                                  │
└────────────────────────┬────────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────────┐
│                   HAL Layer (hal.h)                              │
│  - NAND Driver API                                                │
│  - NOR Driver (stub)                                              │
│  - PCI Management (stub)                                          │
│  - Power Management (stub)                                        │
└────────────────────────┬────────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────────┐
│              Media Layer (media.h)                                │
│  - NAND Hierarchy (Channel → Chip → Die → Plane → Block → Page)│
│  - Timing Model (tR, tPROG, tERS)                                │
│  - EAT (Earliest Available Time) Engine                          │
│  - Bad Block Table (BBT)                                          │
│  - Reliability Model (PE cycles, read disturb)                   │
└──────────────────────────────────────────────────────────────────┘
                         │
┌────────────────────────▼────────────────────────────────────────┐
│              Common Services (common/)                            │
│  - Log System (log.h)                                             │
│  - Memory Pool (mempool.h)                                        │
│  - Message Queue (msgqueue.h)                                     │
│  - Semaphore (semaphore.h)                                        │
│  - Mutex (mutex.h)                                                │
└──────────────────────────────────────────────────────────────────┘
```

---

## 显示模块说明

### 1.顶层界面（ `sssim.h` ）
为应用程序提供与模拟SSD交互的简单界面：
- `sssim_init()` -初始化SSD模拟器
- `sssim_read()` -从SSD读取LBA
- `sssim_write()` -将LBA写入SSD
- `sssim_trim()` -丢弃LBA （ TRIM命令）
- `sssim_flush()` -刷新所有挂起的写入
- `sssim_get_stats()` -获取FTL统计信息

### 2. FTL层(`ftl/`)
实现Flash转换层：
- **映射** ：页面级L2P （逻辑到物理）映射
- **区块管理** ：区块状态跟踪、当前写入区块管理
- **垃圾回收** ：贪婪的受害者选择，有效的页面迁移
- **磨损调平** ：基本磨损调平支持
- ** ECC/错误处理** ：存根实现

### 3. HAL层(`hal/`)
硬件抽象层
- ** NAND驱动程序** ：同步NAND读取/program/erase操作
- ** NOR驱动程序** ：存根实现
- ** PCI管理** ：存根实施
- **电源管理** ：存根实现

### 4.媒体层(`media/`)
NAND闪存介质模拟：
- ** NAND层次结构** ：模型通道、芯片、模具、平面、块、页面
- **定时模型** ：具有LSB/CSB/MSB延迟差异的TLC/MLC/SLC定时
- ** EAT引擎** ：跟踪每个NAND组件的最早可用时间
- **坏块表** ：初始坏块，动态坏块标记
- **可靠性模型** ： PE循环跟踪、读取干扰模拟

### 5.通用服务(`common/`)
所有其他层使用的实用程序模块：
- **日志系统** ：多级日志记录(DEBUG/INFO/WARN/ERROR)
- **内存池** ：固定大小的块分配器，以提高性能
- **消息队列** ：用于组件间通信的线程安全队列
- **信号量** ：计数信号量实现
- **互斥体** ：递归互斥体实现

### 6. Vhost-user后端(`src/vhost/hfsss_vhost_main.c`)
通过Unix套接字将模拟器暴露为`vhost-user-blk`设备，旨在与QEMU进行高性能集成。
- **协议** ：通过链接外部系统库（来自QEMU或DPDK开发包）来实现。
- **架构** ：作为单独的后端流程运行。QEMU使用标准`vhost-user-blk-pci`设备作为客户端进行连接。
- **性能** ：支持QEMU客户机和模拟器之间的零拷贝数据传输，提供高吞吐量和低延迟。

### 7.下一工作日后端(`src/vhost/hfsss_nbd_server.c`)
通过TCP套接字将模拟器暴露为标准网络块设备(NBD)。
- **协议** ： NBD “新式”握手和命令协议的独立、从头开始实施。
- **架构** ：作为一次处理一个客户端的单线程迭代服务器运行。
- **功能** ：支持读取、写入、刷新和修剪操作。包括通过读-修改-写过程处理未对齐I/O请求的逻辑。可以利用FTL的多线程I/O提交路径。

---

## 数据流示例

### 写入操作
1. 应用调用`sssim_write(ctx, lba, count, data)`
2. SSSIM层验证参数并调用`ftl_write()`
3. FTL层：
   - 分配物理页面
   - 更新L2P映射
   - 呼叫`hal_nand_program_sync()`
4. HAL层调用`media_nand_program()`
5. 媒体层：
   - 计算EAT （时间）
   - 将数据存储在DRAM缓冲区中
   - 更新统计信息

### 读取操作
1. 应用调用`sssim_read(ctx, lba, count, data)`
2. SSSIM层调用`ftl_read()`
3. FTL层：
   - 查找L2P映射
   - 呼叫`hal_nand_read_sync()`
4. HAL层调用`media_nand_read()`
5. 媒体层从DRAM缓冲区检索数据

---

## Configuration

默认SSD配置（来自`sssim_config_default()` ） ：
- LBA总数： 262,144 （ 1GB ， 4KB LBA大小）
- LBA大小： 4096字节
- NAND页面大小： 4096字节
- 每个区块的页面数： 256
- 每平面格挡数： 1024
- 每模平面数： 2
- 每个芯片的模具数： 2
- 每个通道的筹码数： 2
- 频道计数： 2
- 超额调配： 10%

---

## 构建和测试

```bash
# Build everything
make all

# Run all tests
make test

# Clean build artifacts
make clean
```

测试输出显示超过431个测试用例全部通过（截至第3阶段完成）。

---

## 架构决策：用户空间与内核模块

### 决定

该项目使用**分阶段架构** ，从纯粹的用户空间库开始，可选择成长为内核模块：

- ** Phases 0–6 （当前路径） ** ：用户空间库。模拟完整的SSD内部堆栈（ FTL、HAL、介质、控制器） ，无需任何内核更改。可通过`sssim.h`从测试程序和基准访问。
- **第7阶段（可选，未来） ** ： Linux内核模块（ `hfsss_nvme.ko` ） ，为主机操作系统提供真正的`/dev/nvme0n1`块设备，实现nvme-cli和fio集成。

### 理由

|关 注|用户空间选择|
|---------|------------------|
|开发速度|无内核构建周期；在几秒钟内使用`make test`进行迭代|
|便捷性|在没有内核的macOS （开发）和Linux （ CI ）上运行|
|安全|Bug无法使主机操作系统惊慌失措|
|范围|研究用例（ FTL算法， GC ， WL ，故障注入）在没有真正的块设备的情况下完全满足|

### 比较表格

|構面|PRD/HLD/LLD规格|当前实施（第0–6阶段）|第7阶段（可选）|
|--------|----------------------|--------------------------------------|---------------------|
|主机接口|Linux内核NVMe驱动程序|`sssim.h`用户空间API|Real `/dev/nvme0n1`|
|PCIe/NVMe仿真|通过内核提供完整配置空间、BAR、MSI-X|用户空间NVMe命令结构|全内核PCI驱动程序|
|命令接收|来自内核的共享内存环缓冲区|直接函数调用API|通过mmap的环形缓冲区|
|DMA|`dma_map_page`/IOMMU|缓冲区指针传递|真正的内核DMA|
|中断派送|`apic->send_IPI`|同步返回值|Real MSI-X|
|螺纹型号|超过26个线程， CPU固定， SCHED_FIFO|可配置线程，无关联|全多核仿真|
|兼容性|nvme-cli、fio、内核NVMe驱动程序|只有程序|完全 WPML 兼容性|

### 对开放需求的影响

要求REQ-120至REQ-122 （ block device、nvme-cli、fio ）在第7阶段❌之前保持不变。所有其他要求都可以在用户空间架构中解决。

### 第7阶段范围（用于规划目的）

- 实现`hfsss_nvme.ko`作为Linux PCI端点功能驱动程序
- 将BAR0 MMIO映射到模拟的NVMe寄存器
- 通过`pci_irq_send_affinity_hint`传送MSI-X中断
- 通过共享内存将内核空间命令接收桥接到用户空间模拟器
- 预计工作量： 4–6周；以1–6期稳定性为准

---

## 参考文献

- PRD原文： `SSD_Simulator_PRD.md`
- 原始中文HLD ： `docs/HLD_*.md`
- 原始中国LLD ： `docs/LLD_*.md`
- 测试设计： `docs/TEST_*.md`
