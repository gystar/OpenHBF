# HFSSS实施路线图

**版本**: V3.0
**日期**: 2026-04-19
**基于**：PRD V2.0，需求矩阵（178项需求——138项核心需求、40项企业级需求）`REQUIREMENT_COVERAGE.md` V3.0

---

## 概述

该路线图跟踪8个阶段中全部178项需求的实施情况（其中138项为核心需求）REQ-001..13840家企业来自REQ-139..178).
所有138项核心要求均被归类为**P0**在需求矩阵中。
本文档增加了一项实用性**等级分类**（严重/高/中等/可选）
基于依赖深度、用户可见的影响以及目标版本。

|层级|标准|计数|
|------|----------|-------|
| **关键** |核心正确性，V2.0版本必须交付| ~48 |
| **高** |主要功能，2.0版本质量| ~35 |
| **中等** |验证与工具链，V2.0优化| ~28 |
| **可选** |内核模块，V2.5 enterprise功能| ~23 |

---

## 需求覆盖率快照 (截至2026-04-19)

|阶段|范围|覆盖范围 (核心✅）|覆盖范围 (包括。⚠）|状态|
|-------|-------|-------------------:|--------------------:|--------|
|第0阶段-基础|NAND层次结构、时序、EAT、BBT、RTOS原语、基本FTL的REQ组/Controller/HAL | 46/138 | — |✅完成|
|第1阶段-核心FTL|成本效益GC、磨损均衡、WAF、增量检查点WAL、panic/assert| 60/138 | — |✅完成|
|阶段2-HAL和控制器|超时管理、背压、QoS桶、GC流量ctl、NOR驱动程序、命名空间管理、PM状态| 74/138 | — |✅完成|
|阶段3-用户空间NVMe|管理/I/O/DSM指令处理、门铃、CQ、识别、DSM TRIM| 87/138 | — |✅完成|
|阶段4-启动/NOR/FTL可靠性/跟踪|6阶段启动、双NOR插槽、电源管理、完整NOR分区、读/写重试、cmd状态机、流量控制、跟踪环| ~101/138 | — |✅完成|
|第5阶段-OOB和工具|JSON-RPC Unix套接字，/proc接口，`hfsss-ctrl`CLI，YAML配置，延迟监视器| ~114/138 | — |✅完成|
|第6阶段-Perf & Fault Inj.| `perf_validation`基准线束，`fault_inject`注册表，DWRR，UPLP，热节流，遥测，安全，多NS| **91/138核心25/40企业** | 116/178 (✅) / 154/178 (✅+⚠️) |✅完成 (perf target强制执行、NAND故障注入接入、安全启动接入、NVMe日志页面07h/08h、由 NOR 支持的密钥表全部保留⚠）|
|第7阶段-内核模块| `/dev/nvme`原始nvme上的块设备、实际MSI-X/DMA/IOMMU、nvme-cli/fio|0/12 (故意)| — |🔲可选 (延迟)|

> **注意**: Core将138要求从REQ-001..REQ-138；谐音在中可见`REQUIREMENT_COVERAGE.md`。第6阶段还涵盖了40个Enterprise V3.0 REQs (UPLP / QoS / T10 PI/安全性/multi-ns/热遥测)，其中大多数已实现或部分实现。阶段7是唯一剩下的范围，并且根据用户空间优先的体系结构决策是显式可选。

---

## 版本目标参考

|版本|承诺|范围|状态|
|---------|-----------|-------|--------|
| V1.0 |Alpha核心模拟|基础核心FTL HAL控制器用户空间NVMe (REQ-001..115核心子集)|✅完成|
| V1.5 |Beta - full FTL持久性启动/电源|L2P检查点WAL UPLP双插槽固件恢复|✅完成|
| V2.0 |Release-OOB，reliability，fault-inj，perf线束|JSON-RPC`hfsss-ctrl`YAML故障-注入`perf_validation`热/遥测|✅完成 (目标强制执行⚠）|
| V3.0 |企业|UPLP/多NS/安全/热/遥测/DWRR QoS (REQ-139..178) |✅大部分完成 (25/40✅,15/40⚠）|
|V2.5 (可选)|内核模块| `hfsss_nvme.ko`，真实`/dev/nvme`,nvme-在原始NVMe的cli/fio|🔲延期 (第7阶段)|

---

## 依赖关系图

```
Foundation (P0) ─────────────────────────────────────────────┐
  NAND Hierarchy  Timing Model  Memory Mgmt  RTOS Primitives  │
       │               │              │             │           │
       └───────────────┴──────────────┴─────────────┘           │
                             │                                   │
              ┌──────────────┴──────────────┐                   │
        FTL Layer (P1)               HAL Layer (P2)             │
  Mapping / GC / WL / ECC       NAND/NOR Drivers               │
              │                         │                       │
              └─────────┬───────────────┘                       │
                        │                                       │
              Controller Thread (P2)                           │
          Arbiter / Scheduler / FlowCtrl                       │
                        │                                       │
              User-Space NVMe (P3)                             │
          Admin/IO Cmds / CQ / Doorbell                        │
                        │                                       │
         ┌──────────────┴──────────────────┐                   │
   Boot & Reliability (P4)       OOB & Tools (P5)              │
Bootloader / Power / Retry    Socket / CLI / Config            │
         │                              │                       │
         └──────────────┬───────────────┘                       │
                        │                                       │
         Perf & Fault Injection (P6)                           │
    Benchmarks / FaultInj / Stability                          │
                        │                                       │
         ┌──────────────┘                                       │
   Kernel Module (P7, Optional)                                │
  Real /dev/nvme / nvme-cli / fio  ◄──────────────────────────┘
```

---

## 已完成的阶段

### 阶段0: 基础✅

**要求**: REQ-038, REQ-039, REQ-040, REQ-041, REQ-050, REQ-057, REQ-058, REQ-059, REQ-068, REQ-070, REQ-071, REQ-072, REQ-073, REQ-076, REQ-077, REQ-089, REQ-092, REQ-094, REQ-095, REQ-097, REQ-098, REQ-100, REQ-101, REQ-102, REQ-103, REQ-105, REQ-136, REQ-138*( 18个结构/接口要求) *

**交付成果**:
- 常见服务: 日志、mempool、msgqueue、信号量、互斥量
- 媒体层: NAND层次结构，时序模型，EAT引擎，BBT，可靠性模型
- HAL: NAND驱动程序 (同步)，NOR/PCI/power存根
- FTL: L2P/P2L映射，块状态机，贪婪GC，基本磨损均衡
- 控制器: 仲裁器、调度器、写缓冲区、读缓存、通道、令牌桶流量控制
- PCIe/NVMe: 寄存器结构 (存根)
- **362测试，0次失败**

**覆盖范围**: 46/134 (34.3%)

---

### 阶段1: 核心FTL和媒体✅

**增加了要求**: REQ-051, REQ-052, REQ-090, REQ-093, REQ-104, REQ-106, REQ-107, REQ-108* (约14行) *

**交付成果**:
- 成本效益GC受害者选择算法 (REQ-104)
- 具有周期性块混洗的静态磨损均衡 (REQ-108)
- WAF (写入放大系数) 计算和监控 (REQ-106)
- 具有基于擦除计数的优先级的动态磨损均衡 (REQ-107)
- 增量检查点WAL持久性到主机文件系统 (REQ-051)
- 使用WAL replay从检查点恢复 (REQ-052)
- 日志持久性到NOR Flash (REQ-093)
- 全面的断言/Panic处理与转储文件 (REQ-090)

**覆盖范围**: 60/134 (44.8%)

---

### 阶段2: HAL和控制器✅

**已添加要求**: REQ-025, REQ-026, REQ-028, REQ-029, REQ-032, REQ-033, REQ-034, REQ-035, REQ-060, REQ-061, REQ-062, REQ-065, REQ-066, REQ-067, REQ-086*（约14行请求）*

**交付成果**:
- 使用每个命令计时器的命令超时管理 (REQ-025)
- 截止时间I/O调度程序 (REQ-026)
- 完全背压机制 (写缓冲区> 90% → 停止新的写入) (REQ-033)
- 紧急的QoS延迟保证/High/Medium/Low优先级 (REQ-034)
- GC流量控制 (到GC的最大30% 带宽) (REQ-035)
- NOR驱动程序完全实现 (REQ-060, REQ-061)
- 通过HAL提交NVMe命令完成 (REQ-062)
- 命名空间管理 (动态NSID分配) (REQ-065)
- NVMe电源状态转换PS0-PS4 (REQ-066, REQ-067)
- 使用mmap hugetlb进行内存管理 (REQ-032)
- 基本看门狗 (每任务馈送，挂起检测) (REQ-086)

**覆盖范围**: 74/134 (55.2%)

---

### 阶段3: 用户空间NVMe接口✅

**已添加要求**: REQ-007, REQ-008, REQ-011, REQ-015, REQ-016, REQ-017* (约13行) *

**交付成果**:
- NVMe门铃寄存器处理 (REQ-007)
- 管理队列 (QID = 0) 创建和管理 (REQ-008)
- 完成队列 (CQ) 处理和CQE构造 (REQ-011)
- 管理命令集: 识别、创建/删除SQ/CQ、获取日志页面、设置/获取功能 (REQ-015)
- I/O命令集: 读、写、刷新 (REQ-016, REQ-017)
- 具有直接函数调用集成的用户空间NVMe库
- **431测试，0次失败**

**体系结构注释**: 阶段0-3实现了仅用户空间的库。主机Linux NVMe驱动程序无法直接检测到此设备。第7阶段 (可选) 为real添加了内核模块`/dev/nvme`支持。

**覆盖范围**: 87/134 (64.9%)

---

## 第4-7阶段范围和状态

---

### 阶段4: 启动、电源和核心可靠性

**目标**: 完成固件生命周期 (启动 → 运行 → 关机 → 恢复) 并缩小关键可靠性差距
**状态**：✅完成-所有四个轨道都已着陆 (启动/电源、完全NOR、FTL可靠性、公共服务跟踪)
**设计参考**: LLD_09_BOOTLOADER.md, LLD_06_APPLICATION.md

#### 跟踪A-引导加载程序和电源服务 (关键，V1.5)
* 可以与轨道B平行运行 *

|任务| REQ-ID |层级|描述|
|------|--------|------|-------------|
|6阶段引导顺序| REQ-078 |关键|阶段0-5: HW init → POST → 元数据加载 → ctrl init → NVMe init → 就绪; 模拟3-8s延迟|
|双固件插槽 (NOR插槽A/B)| REQ-079 |关键|选择具有有效CRC的插槽; 固件更新时的原子插槽交换|
|引导类型检测| REQ-080 |关键|从SysInfo分区检测首次启动/正常/异常|
|正常掉电服务| REQ-081 |关键|停止I/O → 刷新写缓冲区 → 更新L2P检查点 → 设置CSTS.SHST =0x02 |
|异常掉电 (SIGTERM/WAL)| REQ-081 |关键|atexit处理程序 → 写入崩溃标记 → 刷新WAL|
|异常重启时的WAL恢复| REQ-080 |关键|重播WAL条目; 异常关闭后验证数据完整性|
|智能电源周期跟踪| REQ-080 |高|增量power_cycles和unsafe_shutdowns在每个引导|

#### 跟踪B-完全实施NOR闪存 (高，V1.5)
* 可以与轨道A并行运行 *

|任务| REQ-ID |层级|描述|
|------|--------|------|-------------|
|NOR命令集| REQ-053, REQ-055 |高|对NOR映像文件进行读取、编程、擦除操作|
|NOR分区布局| REQ-054 |高|引导加载程序 (4MB) / SlotA(64MB) / SlotB(64MB) / Config(8MB) / BBT(8MB) / Log(16MB) / SysInfo(4MB)|
|NOR数据持久性| REQ-056 |高|将BBT、P/E计数、固件元数据保留到磁盘上的NOR映像|

#### 跟踪c-ftl可靠性 (关键，V1.5-V2.0)
* 取决于轨道A (断电确保WAL可用于重试上下文) *

|任务| REQ-ID |层级|描述|
|------|--------|------|-------------|
|命令状态机| REQ-110 |关键|已接收 → 解析 →L2P_LOOKUP → NAND_QUEUED→ 执行 →ECC_CHECK→ 完成|
|带电压偏移的读取重试| REQ-111 |关键|调整Vread偏置; 最多15次重试; 重试前的软判决LDPC|
|写入重试写入验证| REQ-112 |关键|重试失败的程序; 在重复失败时分配备份块|
|磨损监测和智能警报| REQ-109 |高|SMART Available Spare low-当最大P/E> 80% 时发出警告PE_CYCLE_LIMIT |
|NVMe错误代码 (完整)| REQ-115 |高|DNR位、错误日志页填充、智能媒体错误计数器|

#### Track D-通用服务 (高，V2.0)
* 可以与轨道a-c并行运行 *

|任务| REQ-ID |层级|描述|
|------|--------|------|-------------|
|系统资源监控| REQ-087 |高|每个线程的CPU使用率、内存分区使用率、NAND通道队列深度|
|性能异常检测| REQ-088 |高|P99.9延迟警报; 温度模型T =T_ambientIOPS×c_i + BW×c_b |
|调试跟踪环形缓冲区| REQ-091 |高|100k条目无锁命令跟踪; JSON行导出; 每通道NAND跟踪|

**第四阶段退出标准**:
- [x] `hfsss_init()`执行所有6个启动阶段，每个阶段记录时间 (`src/common/boot.c`)
- [x] 正常关闭: 写入L2P检查点，CSTS.SHST =0x02试验中确认 (`tests/test_power_cycle.c`)
- [x] 异常关机: WAL文件写入，设置了崩溃标记; 下次引导正确重播WAL
- [x] 读取重试: 注入的ECC错误 → 最多15次重试 (`READ_RETRY_VOLTAGE_OFFSETS`循环中`src/ftl/ftl.c`)
- [x] 智能可用备件低于阈值 →critical_warning位设置 (`ftl_rel_check_health`)
- [x] 所有新测试都通过 (从PR #86开始，在0次失败时回归)

**剩余随访**: REQ-115错误日志页填充和REQ-087保持定期资源采样⚠️/❌；见`REQUIREMENT_COVERAGE.md`差距列表。

---

### 阶段5: OOB管理和工具

**目标**: 公开完整的可观察性和控制接口; 完成产品工具链
**状态**：✅Complete-json-rpc套接字，`/proc`接口，`hfsss-ctrl`CLI，YAML配置，延迟监控全部落地
**设计参考**: LLD_07_OOB_MANAGEMENT.md

#### Track A-OOB后端 (Critical，V2.0)
* 轨道B和C的先决条件 *

|任务| REQ-ID |层级|描述|
|------|--------|------|-------------|
|Unix域套接字JSON-RPC 2.0服务器| REQ-082 |关键|在……上收听`/var/run/hfsss/hfsss.sock`；最多16个客户端; 基于epoll|
|OOB管理功能| REQ-083 |关键|status.get / smart.get / perf.get / channel.get / die.get / config.set / gc.trigger / snapshot.save / log.get|
|智能/运行状况日志页面 (0x02) | REQ-084 |关键|所有16个字段; 温度模型集成; 警告/暴击阈值触发|
|潜伏期直方图 (P50/P99/P99.9) | REQ-088 |高|指数存储桶; 按命令类型细分|

#### 跟踪B-产品接口 (高，V2.0)
* 依赖于跟踪A (json-rpc后端) *

|任务| REQ-ID |层级|描述|
|------|--------|------|-------------|
| /proc/hfsss/filesystem接口| REQ-128 |高|状态/配置/perf_counters / channel_stats / ftl_stats / latency_hist/version (仅限Linux)|
|hfsss-ctrl CLI工具| REQ-129 |高|Json-rpc套接字上的瘦包装器; 中列出的所有命令LLD_07 §8 |
|YAML配置文件| REQ-130 |高|解析`/etc/hfsss/hfsss.yaml`；验证所有部分; 默认值|
|持久性格式文档| REQ-131 |中等|NAND数据文件头规范; L2P检查点格式; WAL记录格式|

#### 跟踪C-调试和IPC (中型，V2.0)
*可与B赛道并行运行*

|任务| REQ-ID |层级|描述|
|------|--------|------|-------------|
|通过OOB调试跟踪导出| REQ-091 |高| `trace.enable` / `trace.dump`通过OOB的JSON线路 (来自阶段4轨道D的后端)|
|核间通信 (IPC)| REQ-085 |中等|固件线程之间的SPSC环形缓冲区; eventfd通知; 共享内存大负载|
|信道条带化 (循环)| REQ-099 |中等|LPN → 通过循环或散列进行的信道分配; 可通过`config.set` |
|数据集管理/Trim| REQ-018 |中等|NVMe Deallocate命令 → FTL使LPN范围失效|

**第五阶段退出标准**:
- [ ] 'echo '{"jsonrpc":"2.0"，"method":"status.get"，"params" :{}, "id":1}'| nc -U /var/run/hfsss/hfsss.sock'返回有效的JSON
- [x] `hfsss-ctrl smart`显示智能字段，包括温度 (通过OOB插座)
- [x] 跟踪转储通过`HFSSS_TRACE_DUMP`env变量 (TRACE = 1 build); JSON/analyze路径`scripts/qemu_blackbox/phase_a/analyze_trace.py`
- [x] YAML配置文件在启动时加载 (`src/common/hfsss_config.c`)
- [x] 测试计数过去的600 (回归 ~ 2,759 PR #86的断言)

**残留随访**: REQ-126(直接`/dev/nvme`对于fio) 需要第7阶段内核模块; 目前通过QEMU → NBD间接满足。

---

### 阶段6: 性能验证和故障注入

**目标**: 验证所有性能目标; 实现故障注入框架; 实现生产稳定性
**状态**：✅完整-线束框架已落地; 仍有严格的目标执行 (见下文)
**设计参考**: LLD_08_FAULT_INJECTION.md, LLD_10_PERFORMANCE_VALIDATION.md

#### 跟踪A-性能验证 (关键，V2.0)
* 可以与轨道B和C并行运行 *

|任务| REQ-ID |层级|描述|
|------|--------|------|-------------|
|内置基准引擎| REQ-116, REQ-117 |关键|顺序的/random/mixed/Zipfianload generator; QD 1-128; 收集延迟直方图|
|随机读取IOPS目标 (600K-1M)| REQ-116 |关键|QD验证 = 32，4KB; 报告通过/失败与目标|
|随机写入IOPS目标 (150K-300K)| REQ-117 |关键|在QD = 32, 4KB时验证|
|混合R/W IOPS目标 (250K，70/30)| REQ-118 |关键|在QD = 32时验证|
|顺序带宽 (6.5 GB/s读取，3.5 GB/s写入)| REQ-119 |关键|以128KB的块大小进行验证|
|延迟目标 (P50 ≤ 100 µ s，P99 ≤ 150 µ s，P99.9 ≤ 500 µ s)| REQ-120 |关键|在QD = 1时验证|
|NAND定时精度 (<5% 误差与参考)| REQ-121 |高|tR / tPROG/tERS与ONFI参考; N = 每次操作1000个样本|
|可扩展性 (16个线程的并行效率 ≥ 70%)| REQ-122 |高|Amdahl系列分数 ≤ 6%|
|CPU ≤ 50%，DRAM在配置预算内| REQ-123 |中等|峰值负载下的资源利用率分析|

#### 轨迹B-故障注入框架 (高，V2.5)
* 取决于OOB`fault.inject`第5阶段的接口 *

|任务| REQ-ID |层级|描述|
|------|--------|------|-------------|
|NAND故障注册表 (O(log N) 热路径)| REQ-132 |高| `fault_registry`与排序索引；`type_present`用于无锁快速退出的位掩码|
|坏块/读取/编程/擦除错误注入| REQ-132 |高|Per (ch、chip、die、plane、block、page) 定位; 支持通配符|
|位翻转注入| REQ-132 |高|XOR掩码应用于媒体层中的页面缓冲区|
|读取扰动风暴模拟| REQ-132 |高|阈值读取计数后每个块的概率放大器|
|数据保留老化加速| REQ-132 |中等|保留衰减模型上的老化因子乘数|
|电源故障注入 (空闲/mid-write/mid-gc/mid-checkpoint)| REQ-133 |高| `_exit(1)`写入崩溃标记后; 验证WAL恢复|
|控制器故障注入 (恐慌/池耗尽/超时风暴)| REQ-134 |中等|触发名为mempool的现有紧急气流或排气|
|一次性故障模式与粘性故障模式| REQ-132 |中等| `FAULT_PERSIST_ONE_SHOT`首次点击后自动清除|
|活动故障注册表列表/通过OOB清除| REQ-132 |中等| `fault.list`和`fault.clear`JSON-RPC方法|

#### Track C-系统可靠性 (高，V2.0)
* 可以与轨道A和B并行运行 *

|任务| REQ-ID |层级|描述|
|------|--------|------|-------------|
|多级流量控制| REQ-113 |高|每个命名空间令牌桶NAND通道级队列深度限制|
|类似RAID的元数据冗余| REQ-114 |中等|双拷贝L2P (DRAM文件); BBT双镜像in NOR; 启动时的一致性检查|
|CPU线程关联SCHED_FIFO | REQ-074, REQ-075 |中等| `pthread_setaffinity_np`绑定; NVMe调度线程的实时优先级|
|异步NAND完成通知| REQ-045 |中等|无锁完成队列; 批量完成处理|
|数据完整性验证| REQ-136 |关键|端到端md5sum: 写入n × 4KB → 回读 → 验证匹配; 要求100% 通过率|
|72小时稳定性试验| REQ-137 |高|连续50%-loadR/W; 没有崩溃，没有数据损坏，没有内存泄漏|
|ThreadSanitizer清洁运行| REQ-138 |高| `-fsanitize=thread`；零报告数据竞赛|
|AddressSanitizer清洁运行| REQ-135 |高| `-fsanitize=address`；零堆错误; MTBF代理测试|
| `perf_validation_run_all`报告|所有性能要求|关键|结构化JSON文本报告; 如果任何关键要求失败，则非零退出|

**第6阶段退出标准**:
- [ ] `perf_validation_run_all`退出0-存在线束，严格的通过/失败阈值REQ-116..120尚未断言
- [x] `fault_inject`具有NAND电源故障挂钩的注册表 (`src/common/fault_inject.c`, `tests/test_fault_inject.c`)
- [x] 电源故障注入 → WAL回放验证 (`src/common/uplp.c` + `tests/test_uplp.c`)
- [] 已发布72小时稳定运行-`tests/stress_stability.c`线束存在; 长格式运行未发布
- [x] ThreadSanitizer clean (PR #85)
- [x] AddressSanitizer clean (PR #86)

**残留随访**: perf目标执行 (`perf_validation_run_all`通过/失败接线)，长途稳定性发布，REQ-114管芯级XOR奇偶校验，REQ-075 SCHED_FIFO, REQ-045异步完成通知，REQ-087/088定期资源P99.9异常警报。

---

### 第7阶段: 内核模块 (可选)

**目标**: 呈现一个真实的`/dev/nvme`块设备到主机linux内核，启用nvme-cli和fio
**状态**：🔲可选-阶段0-6上的门稳定性
**预计时长**: 4-6周
**先决条件**: 阶段0-6完整稳定; Linux内核 ≥ 5.15构建环境

#### 跟踪A内核PCIe/NVMe驱动程序

|任务| REQ-ID |层级|描述|
|------|--------|------|-------------|
|Linux PCI端点函数驱动程序框架| REQ-022 |可选| `hfsss_nvme.ko`；PCI探头/拆卸挂钩|
|PCI配置空间仿真| REQ-001, REQ-002 |可选|完整的0型标头; PCIe功能链 (PM、msi-x、PCIe)|
|BAR0 MMIO映射| REQ-003 |可选|将NVMe寄存器空间映射到内核地址空间|
|NVMe CAP/VS/CC/CSTS寄存器| REQ-004, REQ-005, REQ-006 |可选|具有读/写处理程序的真实MMIO; CSTS.RDY转换|
|I/O队列动态创建| REQ-009 |可选|通过内核上下文中的管理命令创建/删除SQ/CQ|
|MSI-X表中断传递| REQ-012, REQ-013 |可选| `pci_alloc_irq_vectors`；每CQ MSI-X向量|
|中断合并| REQ-014 |可选|引发中断前的聚合完成|

#### 跟踪B-DMA和数据路径

|任务| REQ-ID |层级|描述|
|------|--------|------|-------------|
|PRP列表解析引擎| REQ-019 |可选|内核中的PRP1/PRP2/PRP列表遍历|
|通过DMA复制数据| REQ-020 |可选| `dma_map_page` / `dma_unmap_page`对于主机↔模拟器传输|
|IOMMU支持| REQ-021 |可选| `iommu_domain_alloc`；DMA重映射|
|内核-用户空间环形缓冲区| REQ-022 |可选|之间的mmap共享内存`hfsss_nvme.ko`和用户空间模拟器|

#### 跟踪c-host工具集成

|任务| REQ-ID |层级|描述|
|------|--------|------|-------------|
| `/dev/nvme0n1`块设备| REQ-124 |可选|可见到`lsblk`, `fdisk`, `mkfs` |
|NVMe Trim (数据集管理)| REQ-018 |可选|内核释放 → 用户空间FTL修剪|
|nvme-cli完全兼容| REQ-125 |可选| `nvme id-ctrl`, `nvme smart-log`, `nvme format`，等等|
|fio与io_uring/直接I/O| REQ-126 |可选| `ioengine=io_uring`, `direct=1`, `iodepth=128`, `numjobs=32` |

**第7阶段退出标准**:
- [ ] 'lspci|grep-invme' 显示HFSSS设备
- [ ] `nvme list`列表`/dev/nvme0n1`
- [ ] `nvme smart-log /dev/nvme0`返回有效的智能数据
- [ ] `fio --ioengine=io_uring --direct=1 --rw=randread --bs=4k --iodepth=32 --filename=/dev/nvme0n1`无错误完成

**覆盖目标**: 134/134 (100%)

---

## 并行化指南

### 第4阶段平行轨道

```
Week 1-2:
  ├── Track A: Bootloader (REQ-078, REQ-079)
  ├── Track B: NOR Flash full (REQ-053–056)
  └── Track D: Debug trace ring (REQ-091) ← independent

Week 2-3:
  ├── Track A: Power services (REQ-080, REQ-081)  ← needs WAL from Phase 1
  ├── Track C: Read Retry (REQ-111)  ← needs command state machine (REQ-110) first
  └── Track D: Resource monitoring (REQ-087, REQ-088)

Week 3-4:
  ├── Track C: Write Retry (REQ-112)
  ├── Track C: Wear monitoring (REQ-109)
  └── Integration + new tests
```

### 第5阶段平行轨道

```
Week 1:
  └── Track A: OOB socket server + status/smart/perf handlers (REQ-082, REQ-083, REQ-084)

Week 2:
  ├── Track B: /proc interface (REQ-128)  ← needs Track A
  ├── Track B: YAML config (REQ-130)  ← independent
  └── Track C: IPC Ring Buffer (REQ-085)  ← independent

Week 3:
  ├── Track B: hfsss-ctrl CLI (REQ-129)  ← needs OOB socket
  ├── Track C: Channel striping (REQ-099)  ← independent
  └── Track C: Trim/Deallocate (REQ-018)  ← independent

Week 4:
  └── Integration + persistence format doc (REQ-131) + new tests
```

### 第6阶段平行轨道

```
Weeks 1-3:
  ├── Track A: Benchmark engine + IOPS/BW targets (REQ-116–119)
  ├── Track B: Fault registry + NAND fault injection (REQ-132)
  └── Track C: Multi-level flow control (REQ-113)

Weeks 3-5:
  ├── Track A: Latency targets + accuracy + scalability (REQ-120–122)
  ├── Track B: Power + controller fault injection (REQ-133, REQ-134)
  └── Track C: TSan/ASan runs + 72h stability setup (REQ-137, REQ-138)

Weeks 5-6:
  ├── Track A: Resource utilization profiling (REQ-123) + report generation
  ├── Track B: OOB fault.list / fault.clear integration
  └── Track C: MTBF proxy test + final data integrity sweep
```

---

## 未解决项目 (延期或需要澄清)

| REQ-ID |描述|当前状态|决定|
|--------|-------------|---------------|----------|
| REQ-006 |NVMe控制器初始化 (真实MMIO)| ❌ |推迟到第7阶段 (仅内核)|
| REQ-009 |I/O队列动态创建 (内核)| ❌ |推迟到第7阶段|
| REQ-010 |PRP/SGL支持|⚠️ 部分|完全PRP解析推迟到第7阶段; 用户空间使用缓冲区指针|
| REQ-013 |Msi-x中断传递| ❌ |用户空间限制; 推迟到第7阶段|
| REQ-014 |中断合并| ❌ |推迟到第7阶段|
| REQ-021 |IOMMU支持| ❌ |已推迟至第7阶段|
| REQ-022 |内核-用户空间环形缓冲区| ❌ |已推迟至第7阶段|
| REQ-024 |命令分派 (全状态机)|⚠部分|命令状态机 (REQ-110) 在第4阶段填补了这一空白|
| REQ-031 |指令插槽管理 (65535飞行限制)| ❌ |可以在第5或第6阶段添加; 在内核集成之前风险较低|
| REQ-042 |多平面并发 (完整)|⚠部分|每平面吃轨道; 第6阶段完全兼容ZNS的多平面|
| REQ-043 |完整的ONFI命令集|⚠部分|高速缓存读取、挂起/恢复、重置、读取ID-第6阶段磁道C|
| REQ-044 |每通道命令队列线程数|⚠部分|全穿线推迟到第6阶段C轨道|
| REQ-048 |数据保留时间加速|⚠部分|1000:1时间比-相位6伴随故障注入|
| REQ-063 |异步事件管理 (异步事件)| ❌ |需要NVMe Admin命令 → 控制器唤醒路径; 第6阶段|
| REQ-064 |PCIe链路状态 (L0/L1/L2,热复位，FLR)| ❌ |已推迟至第7阶段（仅内核）|
| REQ-096 |通过格式NVM可配置OP|⚠部分|格式化NVM命令尚未连接到FTL OP比率; 阶段5|
| REQ-114 |跨芯片XOR奇偶校验 (RAID-5)| ❌ |中优先级; 第6阶段磁道C; V2.0中仅元数据冗余|

---

## 风险登记册

|风险|概率|影响|缓解|
|------|-------------|--------|------------|
|未达到绩效目标 (REQ-116–120) |中等|高|在基准测试之前在阶段6中添加多线程; 通过分析识别瓶颈|
|WAL恢复正确性|低|关键|基于属性的测试: 随机崩溃点，恢复时验证不变量|
|内核模块复杂性 (第7阶段)|高|中等|阶段0-6在没有内核的情况下完成; 阶段7是可选|
|ThreadSanitizer数据竞赛|中等|高|从第4阶段开始在CI中运行TSan; 逐步修复比赛|
|测试覆盖率漂移 (REQ-ID→ 测试映射)|中等|中等|每个阶段退出标准列出明确的测试名称; 在CI中强制执行|
|NOR闪存分区布局更改|低|中等|在单个标头中定义的所有分区偏移; 单个更改点|

---

## 要求 → 相位交叉参考

| REQ-ID |阶段| REQ-ID |阶段| REQ-ID |阶段|
|--------|-------|--------|-------|--------|-------|
| REQ-001–005 | 0 ✅ | REQ-046–049 | 0 ✅ | REQ-091 | 4 |
| REQ-006 | 7 | REQ-050 | 0 ✅ | REQ-092–093 | 0/1 ✅ |
| REQ-007–008 | 3 ✅ | REQ-051–052 | 1 ✅ | REQ-094–098 | 0 ✅ |
| REQ-009 | 7 | REQ-053–056 | 4 | REQ-099 | 5 |
| REQ-010 |7 (部分)| REQ-057–059 | 0 ✅ | REQ-100–103 | 0 ✅ |
| REQ-011 | 3 ✅ | REQ-060–062 | 2 ✅ | REQ-104–108 | 1 ✅ |
| REQ-012 | 0 ✅ | REQ-063 | 6 | REQ-109–112 | 4 |
| REQ-013–014 | 7 | REQ-064 | 7 | REQ-113 | 6 |
| REQ-015–017 | 3 ✅ | REQ-065–067 | 2 ✅ | REQ-114 | 6 |
| REQ-018 | 5 | REQ-068–069 | 0 ✅ | REQ-115 | 4 |
| REQ-019–022 | 7 | REQ-070–073 | 0 ✅ | REQ-116–123 | 6 |
| REQ-023–030 | 0–2 ✅ | REQ-074–075 | 6 | REQ-124–126 | 7 |
| REQ-031 | 6 | REQ-076–077 | 0/2 ✅ | REQ-127–131 | 5 |
| REQ-032–036 | 2 ✅ | REQ-078–081 | 4 | REQ-132–134 | 6 |
| REQ-037 | 0 ✅ | REQ-082–084 | 5 | REQ-135–138 | 6 |
| REQ-038–045 |0 (部分)| REQ-085 | 5 | | |
| | | REQ-086–090 | 2/1 ✅ | | |
