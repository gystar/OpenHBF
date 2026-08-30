# OpenHBF 模拟器简明设计说明

## 1. 这个模拟器要做什么

OpenHBF用于模拟OCP HBF设备从Host请求到NAND介质完成的完整过程。它关注的是请求经过多少排队、传输和介质阶段，需要多少模拟时间，不同资源能否并行，以及错误、重试和退役怎样影响结果。

它不是传统NVMe SSD模拟器。正式模型不包含PCIe/NVMe队列、Trim、传统垃圾回收或任意页级地址映射。

本项目区分三类信息：

- **OCP明确规定**：例如最多16个独立Channel、每Channel独立x64 UCIe链路、Host以64 B访问、NAND侧以4 KiB DLU读写、同一Bank的sense严格有序。
- **OpenHBF的实现选择**：例如使用一个统一事件队列、把NAND操作拆为多个阶段、以Bank阵列作为主要介质并行资源。
- **合成参数**：例如SLC/TLC的读取、编程和擦除时间。OCP没有提供这些数值，当前值只是可重复的模拟默认值，不是OCP或厂商保证值。

## 2. 总体数据路径

一条正常请求按以下方向流动：

```text
Ramulator、Trace或直接测试输入
          ↓
02 系统集成与统一时间
          ↓
01 Host接口与UCIe/AXI规则
          ↓
03 Base Die控制器
          ↓
04 规则地址与顺序管理
          ↓
05 NAND Media
          ↓
沿原路径返回完成结果
```

所有模块使用同一个模拟时钟和事件队列。请求被接受不代表立即完成；只有未来的完成事件到达后，数据和状态才允许正式改变。

## 3. 六个模块及当前完成度

状态含义：`未完成`表示主要生产路径尚未落地；`部分完成`表示已有可运行骨架，但仍缺关键能力；`基本完成`表示核心路径可运行，但仍不能视为完整、精确或发布就绪。

### 01 Host Interface — 未完成

负责每个Host Channel的64 B请求检查、UCIe收发时间、虚拟AXI队列、同一AXI ID的返回顺序，以及最终响应排序。

目标是最多支持16个相互隔离的Channel，每个Channel具有独立链路和独立存储池。当前还没有形成完整的独立Host模块实现，因此不能声称UCIe信用、链路初始化和全部AXI顺序规则已经得到模拟。

### 02 Ramulator与系统集成 — 核心基本完成，系统接线未完成

负责连接Ramulator、配置、统一模拟时间、统一事件队列、请求生命周期和最终回调。不同输入方式应当进入同一条设备路径，不能为Ramulator单独建立一套固定延迟模型。

当前规划的12个Integration/System生产文件均已落地。已经完成统一系统时钟和事件队列、Host请求格式检查、Accepted/Retry准入、一次性完成与回调保护、Reset generation隔离、DirectRunner，以及不重复实现设备逻辑的Ramulator薄适配器。Integration core已有Docker严格构建和两个核心CTest证据，Ramulator-enabled模式也已经通过适配器编译检查。

尚未完成的是把真实01、03、04、05组成最终production对象图，完整解析YAML和Ramulator配置，链接并运行Ramulator Factory对象，以及apps和完整端到端测试。因此02自身的核心机制已经基本完成，但还不能宣称Ramulator请求已经通过完整OCP设备流水线。

### 03 Base Die Controller — 部分完成

负责把64个64 B写入片段收集为完整4 KiB DLU，并计划负责ECC、TSV、Bank请求队列、每Bank至少两个4 KiB读缓存、Regular/Batch Read、Scratchpad、寄存器、Admin和生命周期管理。

当前已完成基础DLU聚合、部分事务门面和控制面。ECC、TSV、Bank调度、Bank缓存、Batch Read、完整读路径和统计仍未完成。因此现在还不能模拟完整的cache hit带宽或ECC流水线瓶颈。

### 04 HBF地址与顺序管理 — 部分完成

文档中仍把本模块简称为FTL，但它不是传统SSD FTL。它使用OCP给出的规则公式直接计算物理位置，并维护顺序Program、Block Sequence、Host Replay、Zone Remap和容量退役视图。

它不保存巨大的页级L2P/P2L表，也不选择GC victim，更不会后台复制有效数据。当前已有规则映射、顺序管理和Zone相关骨架，但缺少与03、05组成的完整异步链路及正式系统测试。

### 05 NAND Media — 基本完成，但仍有重要缺口

负责NAND层次、4 KiB数据、Page/Block状态、Read/Program/Erase时序、资源最早可用时间、可靠性、坏块表、退役、温度观察和介质统计。

当前核心异步路径已经可以运行，并能区分Read、Program和Erase阶段，也能模拟顺序Program、坏块、磨损计数、读干扰和数据保持风险。但仍有三个重要问题：数据与页面状态的最终提交还不是严格原子事务；恢复镜像没有保存完整Page数据；统计只累计字节和时间，尚未完成窗口带宽和分层利用率。因此这里使用“基本完成”，而不是“完成”。

### 06 Verification — 未完成

负责端到端数据校验、事件顺序、故障矩阵、Reset竞争、确定性重放、长时间压力测试、配置一致性和理论带宽上限检查。

当前只有若干单元和集成脚手架，尚未建立文档规划的完整系统验证套件。Debug下Media测试可以通过，但Release测试目前因使用会被关闭的普通断言而无法可靠构建和验证。

## 4. 层次结构和并行粒度

### 4.1 正式可见层次

当前正式模型使用：

```text
HBF Stack
├── Host Channel 0：独立链路和专属存储切片
├── Host Channel 1：独立链路和专属存储切片
└── ...最多16个Channel

每个Channel的存储切片
└── Core Die → Die → Bank → Block → 4 KiB Page
```

Channel是独立命令入口和资源所有者，但不应简单理解为复制一整套物理容量。每个Channel只能访问分配给自己的存储切片，不能跨Channel访问。

### 4.2 当前是Bank级并行

当前实现的主要并行单位是**Bank**(由于OCP的文档中采用Bank, 我也就遵循Bank的用法，实际上我认为和平常所说的plane含义相同)，也不是Multi-Plane命令：

- 不同Channel拥有独立链路和独立存储池，可以并行。
- 不同Bank的NAND阵列操作可以并行。
- 同一Bank的NAND sense必须严格按序。
- 数据进出还会竞争Channel内部共享介质路径。

## 5. 请求怎样执行

### Read

Host可以请求64 B或一个DLU内的多个64 B片段。03先检查未完成写入和Bank缓存；未命中时，04计算目标物理Page，05执行NAND sense和数据传出。之后03计划执行ECC并把Host需要的部分返回。

### Program

Host的64 B写入先在03聚合。只有完整4 KiB到齐，才能向NAND发出Program。04要求同一Block内按Page 0、Page 1、Page 2的顺序写入。05依次模拟数据传入、阵列编程和校验，最终成功后才提交数据。

### Erase

Erase以Block为单位。成功后该Block中的Page回到擦除状态，同时增加一次P/E计数。Reset和Status不是额外的NAND数据命令：Reset属于生命周期控制，Host可见状态由03的寄存器和Admin路径提供。

## 6. 当前默认规模与性能参数

### 6.1 OCP规定的接口上限

| 项目 | OCP基线 |
|---|---:|
| Host Channel | 每个Stack最多16个，彼此独立 |
| 每Channel虚拟AXI接口 | 1、2或4个 |
| UCIe mainband | 每Channel x64、全双工 |
| Speed Grade 1 | 8 GT/s，模块raw 64 GB/s，系统参考48 GB/s |
| Speed Grade 2 | 16 GT/s，模块raw 128 GB/s，系统参考94 GB/s |
| Speed Grade 3 | 32 GT/s，模块raw 256 GB/s，系统参考188 GB/s |
| 最高系统有效带宽参考 | 约3.072 TB/s |
| Host基本访问粒度 | 64 B |
| NAND侧DLU/Page契约 | 4 KiB |
| 每Bank读缓存 | 至少两个4 KiB slot，由03管理 |

这些带宽是接口或系统目标，不等于当前程序已经能达到的实测吞吐。最终吞吐应取Host链路、ECC、TSV、Bank阵列、共享介质路径和队列等所有瓶颈中的最小值。

### 6.2 当前示例配置

| 项目 | Ramulator示例 | 小型测试配置 |
|---|---:|---:|
| Channel | 1 | 1 |
| 每Channel AXI | 1 | 1 |
| Core Die | 1 | 1 |
| 每Core Die的Die | 1 | 1 |
| 每Die的Bank | 2 | 2 |
| 每Bank的Block | 1024 | 4 |
| 每Block的Page | 256 | 8 |
| Media最大并发命令 | 256 | 8 |
| payload模式 | 只计时 | 只计时 |

因此，当前默认运行并不是16 Channel满配置，也不能直接用3.072 TB/s作为当前模拟结果。

### 6.3 当前NAND合成时间

下表数值来自OpenHBF当前内置合成模型，单位为微秒。

| 类型 | Page Read sense | Program array | Block Erase array |
|---|---:|---:|---:|
| SLC | 25 | 200 | 1500 |
| MLC | 50 | 600 | 2000 |
| TLC LSB | 60 | 700 | 2500 |
| TLC CSB | 85 | 1100 | 2500 |
| TLC MSB | 110 | 1500 | 2500 |
| QLC合成类 | 140 | 2200 | 3000 |

除此之外，Read数据传出和Program数据传入当前各为4微秒；Program verify依类型约为20至120微秒，Erase verify约为100至180微秒。模拟器把这些时间向上取整为整数cycle。

当前TLC默认用Page序号按LSB、CSB、MSB循环分类。这只是OpenHBF的合成布局。真实器件的wordline和page mapping必须由厂商资料替换。

### 6.4 能否给出当前Media理论带宽

只能在给定拓扑、读写比例和资源占用规则后计算。例如，单个SLC Bank仅按25微秒sense和4 KiB计算，理想连续读上限约为164 MB/s；单个TLC LSB Bank约为68 MB/s，TLC MSB约为37 MB/s。多个独立Bank理论上可以近似线性叠加，直到共享数据路径、ECC、TSV或Host链路先饱和。

这些只是阵列sense上限估算，没有计入数据传出、排队、cache、ECC和协议开销，不能当作完整系统带宽。当前统计模块也尚未完成自动的资源上限和滑动窗口带宽报告。

## 7. 常见问题

### 问：模拟器包括ECC吗？

**设计包括，实现未完成。** ECC属于03 Base Die，而不是NAND Media。目标是模拟编码、解码、CECC、UECC和Read Retry的时间及资源竞争。05只产生原始错误结果，不能自己宣称已经完成ECC纠错。

### 问：包括磨损均衡吗？

**不包括传统后台磨损均衡。** 05会记录P/E次数并根据合成模型增加故障概率，也能标记坏块和退役容量；但它不会为了均衡磨损而主动搬移有效数据。若未来加入厂商维护策略，必须证明符合OCP禁止active-data transfer的边界。

### 问：包括GC吗？

**不包括。** OCP HBF v0.7.0明确说明HBF不支持传统GC和active data transfer。模拟器不设计victim选择、有效页复制、WAF steady state或后台回收队列。

### 问：包括L2P/P2L映射吗？

**不包括传统页级映射表。** 04根据A1和R1至R5公式直接计算物理位置，只保存Block顺序、Zone映射和退役等有限状态。这样更符合HBF规则地址设计，也避免误做成传统SSD FTL。

### 问：没有GC和L2P，坏块或Program失败怎么办？

通过顺序Program、Page-0自动Erase、Host Replay、Zone Remap、Reduced Capacity和retirement处理。Zone Remap只改变映射，不在设备内部复制现有Page；需要恢复的数据由Host按规范重放。

### 问：模拟器支持SLC、MLC、TLC和QLC吗？

**05的合成时序和可靠性模型支持选择这些类型。** 但OCP没有规定这些NAND参数。当前TLC有LSB、CSB、MSB差异，QLC仍是一个聚合合成类，不能称为完整厂商级QLC模型。

### 问：支持Multi-Plane吗？

**正式OCP路径不支持。** 当前是Bank级并行。Plane和Multi-Plane只能作为未来vendor profile扩展，必须有独立规范或datasheet依据。

### 问：每个Channel是不是独立命令发起方？

**从Host接口逻辑上是。** 每个Channel有独立UCIe链路、队列、地址空间和存储池，不能访问其他Channel的池。Reset、电源和部分管理信号可以共享，但数据路径不能因此跨Channel调度。

### 问：现在能模拟真实数据，还是只算时间？

05同时具有保存4 KiB数据和只保存时序信息的两种模式。不过当前两个示例配置都选择只计时模式；而且完整payload的掉电保存与恢复仍未完成。因此端到端数据正确性还不能视为已经系统验证。

### 问：现在能报告cache hit bandwidth和Media bandwidth吗？

**设计上可以分开报告，实现尚不完整。** Cache hit/miss与hit bandwidth唯一归03统计；真实NAND流量和Media bandwidth归05统计；02负责最终汇总。当前03缓存统计未完成，05也只有累计字节和stage时间，尚缺正式窗口带宽。

### 问：当前最需要先修正什么？

优先级应为：先修复05的数据与状态原子提交和完整payload恢复；完成03的ECC、TSV、Bank调度与缓存；完成01 Host接口和04全链异步接入；再由02把真实01至05装配成production对象图并完成Ramulator运行链接；最后建立06的系统级数据、并发、Reset、故障和带宽上限验证。在这些工作完成前，项目应称为“02核心和Media基本可运行”，不应称为完整OCP HBF模拟器。

## 8. 一句话总结

OpenHBF的目标是模拟“独立Host Channel + Base Die控制 + 规则地址管理 + Bank级NAND并行”的HBF系统。它有意不复制传统SSD的GC和页级FTL；当前02系统核心与05 Media核心完成度最高，但真实Host到Media的生产接线、Base Die完整流水线和系统验证仍是主要缺口。
