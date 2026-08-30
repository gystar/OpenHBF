# 单一 FTL 读写分离与 DSB 集成实施计划

> **对于智能体工作者:**所需的子技能: 使用`superpowers:subagent-driven-development` (推荐) 或`superpowers:executing-plans`逐任务实施此计划。步骤使用复选框 (`- [ ]`) 用于跟踪的语法。

**目标：**将当前的多工作者FTL数据路径重构为一个权威的FTL核心，具有分离的写入/修剪变更处理、可扩展的读取工作线程、DSB支持的多流写入放置以及显式映射/修剪日志。

**架构：**保持一个单一的`struct ftl_ctx`作为映射、DSB、面向GC的元数据和后端调度的所有者。替换`LBA % FTL_NUM_WORKERS`使用操作进行路由-角色路由: 写入/TRIM/metadata突变转到一个变更工作线程，读取转到一个或多个读取工作线程，并且NAND操作通过DSB/后端调度器发出。运行时映射移动到具有轻量加锁读取和单写入器更新的新FTL映射层后面; 持久检查点/恢复在此计划中保持不变。

**技术栈：**C11，pthreads，atomics，现有`io_ring`,现有FTL/DSB/backend调度器模块、现有单元测试工具和Makefile。

**参考：** `docs/superpowers/specs/2026-05-22-data-superblock-design.md`, PR #127 (`codex/data-superblock-phase1`).

**前提条件：**仅在以下情况下执行此计划`codex/data-superblock-phase1`或已包含以下内容的稍后分支:
- `include/ftl/data_superblock.h`与`struct ftl_dsb_mgr`.
- `include/ftl/backend_sched.h`和`backend_sched_init`.
- `build/bin/test_data_superblock`, `build/bin/test_backend_sched`，和`build/bin/test_mt_ftl`构建目标。

直接从运行此计划中的命令块`master`在出现这些DSB/后端调度程序先决条件之前，预计会失败。

**范围边界:**
- 仅修改FTL层文件。
- DSB内部策略未更改: 无DSB坏块替换设计、无DSB成员布局重写、无DSB停用机制更改。
- 功率损耗/checkpoint/recovery路径未更改: 现有`sb_checkpoint_*`, `sb_recover_*`,正常断电重启行为保持原样。
- 添加了调试跟踪/事件，但此处未实现可视化、仪表板和离线分析工具。

---

## 实施收尾

**截至2026-05-23的状态:**核心重构通过PR #129落地
(`[codex] Refactor FTL worker routing and runtime maps`) 并合并为
`master`作为合并提交`f2ee50ce19ed9f16a6292e0dc2170a86d61eff22`.

下面未选中的任务框将保留为原始实现
计划。将此收尾部分视为权威的合并后状态。

### PR #129 已实现内容

- `ftl_mt_submit()`不再使用`LBA % worker_count`路由的所有权。
- 写入，修剪和刷新到一个变更工作线程的路线; 读取路线循环
  跨已配置的读取工作线程。
- 工作线程空闲等待现在使用`pthread_cond_wait()`使用提交端唤醒，
  关闭FTL 工作线程的原始空闲忙等待问题。
- MT主机写入通过流拥有的DSB打开指针进行分配，并使用
  用于NAND编程/读取操作的后端调度器包装器。
- 运行时`ftl_map`提供L2P查找/update/trim,P2L反向查找，P2L
  碰撞处理，以及`ftl_map_update_if_equal()`对于GC remap安全。
- `ftl_map_log`中的记录写入、修剪、GC重新映射和GC跳过事件
  内存环形缓冲区。
- `ftl_trim_log`合并相邻/重叠的范围，并且工作进程
  具有队列写入抢占的4096页切片中的大型TRIM请求
  切片。
- 数据流放置支持默认、顺序、热覆盖和GC流，
  带有初始主机开放流上限。
- DSB内部和检查点/恢复内部不是故意的
  被这个重构改变了。

### 验证证据

PR #129最终修复提交`540b4f293272a6e11a549fed1eaf4fe8fd81d2fd`
在与合并之前已在本地验证:

```bash
make -j8 build/bin/test_ftl_map build/bin/test_ftl_map_log \
  build/bin/test_ftl_trim_log build/bin/test_ftl_stream \
  build/bin/test_ftl_backend_exec build/bin/test_mt_ftl \
  build/bin/test_gc_mt

build/bin/test_ftl_map
build/bin/test_ftl_map_log
build/bin/test_ftl_trim_log
build/bin/test_ftl_stream
build/bin/test_ftl_backend_exec
build/bin/test_mt_ftl
build/bin/test_gc_mt
git diff --check
```

观察结果:

- `test_ftl_map`: 27/27通过。
- `test_ftl_map_log`: 17/17通过。
- `test_ftl_trim_log`: 13/13通过。
- `test_ftl_stream`: 7/7通过。
- `test_ftl_backend_exec`: 8/8通过。
- `test_mt_ftl`: 109/109通过。
- `test_gc_mt`: 38/38通过。
- `git diff --check`: 无输出。

合并后的关闭验证，在分支上运行
`codex/ftl-refactor-closeout`基于`master`
`f2ee50ce19ed9f16a6292e0dc2170a86d61eff22`:

- `make test`: 已退出0。
- `rg -n"lba %FTL_NUM_WORKERS|req->lba %|worker_id= req->lba “包括/ftl src/ftl测试-S`:
  没有火柴.
- `make coverage-clean && make coverage-ut && bash scripts/coverage/ratchet_check.sh`:
  已退出0。
- 覆盖范围摘要: 行84.5% (15956/18893)，功能95.1%
  (1137/1195)，67.0% 分行 (6929/10345)。
- 棘轮结果: 传球反对`.coverage-baseline.json`地板
  (行72.1%，函数82.3%，分支52.3%)。
- HTML覆盖率报告:
  `build-cov/coverage/ut/index.html`.
- Coverage runner警告: 71个工作负载通过，10个工作负载返回
  在覆盖率模式下非零，同时仍提供覆盖率数据:
  `test_perf_validation`, `systest_data_integrity`,
  `systest_error_boundary`, `systest_nvme_compliance`,
  `systest_persistence`, `systest_wear_gc`, `stress_admin_mix`,
  `stress_mixed`, `stress_mixed_trim`，和`stress_rw`.
- GitHub问题 #5在确认FTL工作空闲后被评论并关闭
  路径现在等待`pthread_cond_wait()`和提交侧唤醒。

### 剩余随访

- 调查覆盖模式警告工作负载，如果项目需要
  覆盖运行程序本身报告0个警告/失败工作负载; 常规
  `make test`运行已退出0。
- 展开`ftl_debug`从当前精简快照到完整计数器集
  本文档中描述: 路由、映射查找/更新、修剪合并、
  流选择、DSB储备压力、后端提交压力和长尾
  后端等待。
- 替换剩余的固定大小的worker-array兼容性 (`FTL_NUM_WORKERS`)
  如果/当模拟器需要避免时，使用显式配置的worker计数
  正在启动未使用的读取工作线程。
- 移动GC remap通过变更工作线程的障碍在未来的清理，如果我们想
  删除之间的长期双写形状`runtime_map`还有TAA.
- 在中实施单独的FTL转储/重建计划
  `docs/superpowers/plans/2026-05-23-ftl-dump-rebuild.md`.

---

## 此计划修复的当前问题

当前异步FTL路径具有单个共享`ftl_ctx`,但它仍然通过路由前端工作`req->lba % FTL_NUM_WORKERS` in `src/ftl/ftl_worker.c`。这产生了三个问题:

1. 逻辑地址散列路由与后端通道/管芯能力无关。
2. 读、写和TRIMs在同一个worker形状中竞争，尽管它们有不同的一致性需求。
3. 多个写入工作者可以同时改变映射和DSB打开游标，从而强制锁定应该有一个突变所有者的结构。

目标设计保留单个FTL并删除基于LBA的所有权。写入和TRIMs通过突变worker序列化，使L2P/P2L/DSB/journal更新确定性。通过读取工作程序和锁光映射查找进行读取缩放。后端并行性由通道工作程序和DSB放置提供，而不是通过拆分FTL所有权。

---

## 目标体系结构

```text
NVMe/NBD/controller submitter
        |
        v
ftl_mt_submit()
        |
        +----------------------+
        | operation-role route |
        +----------+-----------+
                   |
        +----------+------------------------------+
        |                                         |
        v                                         v
write_trim_worker                         read_worker[N]
single metadata mutation owner            lock-light map lookup
        |                                         |
        +-------------------+---------------------+
                            |
                            v
                    ftl_backend_sched
                    per-channel workers
                            |
                            v
                    HAL / NAND simulator
```

### FTL核心所有权

`struct ftl_ctx`仍然是唯一的FTL实例。它拥有:

- 运行时L2P/P2L映射。
- DSB管理器并打开DSB指针。
- 流管理器和每个流打开DSB状态。
- 突变日志和修剪范围日志。
- 现有GC、WL、ECC、error、stats和后端调度程序引用。

### 前端工人

更换固定`FTL_NUM_WORKERS`具有角色的语义:

```c
enum ftl_worker_role {
    FTL_WORKER_WRITE_TRIM = 0,
    FTL_WORKER_READ = 1,
};

struct ftl_frontend_worker {
    u32 worker_id;
    enum ftl_worker_role role;
    pthread_t thread;
    struct io_ring request_ring;
    struct io_ring completion_ring;
    pthread_mutex_t request_lock;
    pthread_cond_t request_cond;
    bool request_sync_initialized;
    struct ftl_ctx *ftl;
    atomic_bool running;
    u64 ops_completed;
    u64 ops_failed;
};
```

默认工作人员形状:

```text
write_trim_workers = 1
read_workers       = min(4, max(1, channel_count / 4)) initially
```

读取工作人员计数必须可通过`struct ftl_mt_config`,具有从几何图形派生的默认值。

### 路由规则

```text
READ       -> read worker round-robin or queue-depth-aware least-loaded
WRITE      -> write/TRIM worker
TRIM       -> write/TRIM worker
FLUSH      -> write/TRIM worker
STOP       -> all workers
```

没有`lba % worker_count`所有权。LBA仍然可以用作read-worker选择的cache-affinity提示，但不能用作正确性边界。

### 后端DSB集成

主机写入和GC重定位写入通过DSB分配:

```c
dsb_reserve_write(&ctx->dsb_mgr, &stream->open_dsb, &target);
dsb_commit_write(&ctx->dsb_mgr, &stream->open_dsb, &target);
```

FTL层可以保持多个`open_dsb`指针，每个流一个。它不会改变DSB选择成员、替换坏块、退出DSB或在内部安排通道的方式。

---

## 测绘设计

### L2P运行时表

创建新的运行时地图层:

- `include/ftl/ftl_map.h`
- `src/ftl/ftl_map.c`
- `tests/test_ftl_map.c`

地图层隐藏了现有的`mapping_ctx`和`taa_ctx`从新的worker架构中分离出来。

```c
enum ftl_map_entry_state {
    FTL_MAP_EMPTY = 0,
    FTL_MAP_PRESENT = 1,
    FTL_MAP_TRIMMED = 2,
};

struct ftl_map_entry {
    _Atomic u64 raw;
};

struct ftl_map_page {
    _Atomic u32 live_count;
    struct ftl_map_entry entries[512];
};

struct ftl_map_ctx {
    u64 total_lbas;
    u32 entries_per_page;
    u64 page_count;
    _Atomic(struct ftl_map_page *) *pages;
    struct ftl_p2l_index p2l;
    bool initialized;
};
```

条目编码:

```text
bits  0..55: ppn.raw low bits
bits 56..57: state
bits 58..63: small generation/version counter
```

这使查找保持到一个指针加载和一个条目加载。当前的`union ppn`布局未更改。

### L2P更新规则

只有写/修剪工作线程才会改变主机L2P条目。GC重映射完成也作为突变消息提交给写/修剪工作器。这使得L2P/P2L更新单写入器，并消除了对主机写入的全局映射锁定的需要。

读取使用:

```c
int ftl_map_lookup(struct ftl_map_ctx *map, u64 lba, union ppn *ppn_out);
```

写入/修剪使用:

```c
int ftl_map_update(struct ftl_map_ctx *map, u64 lba,
                   union ppn new_ppn, union ppn *old_ppn_out);
int ftl_map_trim(struct ftl_map_ctx *map, u64 lba,
                 union ppn *old_ppn_out);
int ftl_map_update_if_equal(struct ftl_map_ctx *map, u64 lba,
                            union ppn expected_old,
                            union ppn new_ppn,
                            bool *updated_out);
```

`ftl_map_update_if_equal()`由GC重定位使用，以避免覆盖赢得比赛的主机写入。

### P2L运行时索引

创建独立于DSB内部的FTL拥有的反向映射:

```c
struct ftl_p2l_entry {
    u64 ppn_raw;
    u64 lba;
    u32 generation;
    bool valid;
};

struct ftl_p2l_index {
    struct ftl_p2l_entry *entries;
    u64 capacity;
    u64 used;
};
```

对墓碑使用开放式寻址。必须测试冲突处理，因为当两个ppn折叠到同一索引时，现有的传统P2L散列可能会丢失条目。

P2L索引仅由突变worker更新:

- 写更新: 删除旧的PPN，插入新的PPN。
- 修剪: 拆卸旧PPN。
- GC remap-if-equal成功: 删除受害者PPN，插入重新定位的PPN。
- GC remap-if-equal skip: 删除重新定位的PPN并将刚刚写入的重新定位页面标记为无效。

本计划不坚持新的P2L指数。电流功率路径保持不变。

---

## 映射日志设计

创建：

- `include/ftl/ftl_map_log.h`
- `src/ftl/ftl_map_log.c`
- `tests/test_ftl_map_log.c`

突变日志是存储器中的FTL数据路径日志。它不是此计划中的断电恢复日志。

```c
enum ftl_map_log_op {
    FTL_MAP_LOG_WRITE = 1,
    FTL_MAP_LOG_TRIM = 2,
    FTL_MAP_LOG_GC_REMAP = 3,
    FTL_MAP_LOG_GC_SKIP = 4,
};

struct ftl_map_log_record {
    u64 seq;
    enum ftl_map_log_op op;
    u64 lba;
    u32 count;
    union ppn old_ppn;
    union ppn new_ppn;
    u32 stream_id;
    u64 dsb_id;
    int status;
};
```

属性：

- 单一生产者: 写/修剪变更工作线程。
- 多读取器通过锁保护的复制API调试快照。
- 带有丢失记录计数器的固定大小环形缓冲区。
- 每个映射突变必须正好发出一个日志记录。
- 现有的持久性日志调用保留在其现有路径中，此处不再重新设计。

---

## 修剪原木设计

创建：

- `include/ftl/ftl_trim_log.h`
- `src/ftl/ftl_trim_log.c`
- `tests/test_ftl_trim_log.c`

TRIM日志存储逻辑范围并支持合并:

```c
struct ftl_trim_range {
    u64 start_lba;
    u64 end_lba_exclusive;
    u64 seq;
};

struct ftl_trim_log {
    struct ftl_trim_range *ranges;
    u32 capacity;
    u32 count;
    u64 next_seq;
};
```

初始行为:

1. TRIM请求被路由到写/TRIM工作者。
2. worker将范围追加或合并到`ftl_trim_log`.
3. worker清除有界块中的L2P/P2L条目。
4. 仅在所有受影响的LBAs逻辑上取消映射后才返回完成。
5. 通过继续调用来保留现有范围-日志行为
   `sb_journal_append_range(&ctx->sb, JRNL_OP_TRIM_RANGE, start_lba, 0, page_count, 0)`
   从变更工作线程那里。

大范围配平配合加工:

```text
max_trim_pages_per_slice = 4096
```

在每个切片之后，工作线程可以在继续TRIM之前为排队的写入提供服务。这使TRIM在保持完成正确性的同时避免产生长的写尾延迟。

读取工作程序不会在初始实现中查阅TRIM日志，因为完成等待L2P清除。后来的lazy-TRIM设计可以在读取完成之前添加间隔查找; 这是故意超出此计划的。

---

## 多流设计

创建：

- `include/ftl/ftl_stream.h`
- `src/ftl/ftl_stream.c`
- `tests/test_ftl_stream.c`

流是FTL放置课程。它们让FTL为不同的写入温度和目的保持单独的开放dsb。

```c
enum ftl_stream_class {
    FTL_STREAM_DEFAULT = 0,
    FTL_STREAM_SEQ = 1,
    FTL_STREAM_HOT = 2,
    FTL_STREAM_COLD = 3,
    FTL_STREAM_GC = 4,
    FTL_STREAM_COUNT = 5,
};

struct ftl_stream {
    enum ftl_stream_class id;
    struct ftl_data_superblock *open_dsb;
    u64 host_write_pages;
    u64 last_lba;
    u32 sequential_run;
};

struct ftl_stream_mgr {
    struct ftl_stream streams[FTL_STREAM_COUNT];
    u32 max_open_host_streams;
    bool initialized;
};
```

初始流选择:

```text
GC relocation             -> FTL_STREAM_GC
host sequential run >= 32 -> FTL_STREAM_SEQ
hot overwrite detected    -> FTL_STREAM_HOT
default random write      -> FTL_STREAM_DEFAULT
```

冷流分类需要主机提示或更长的历史记录，默认情况下不启用。枚举和计数器存在，因此以后的控制器/NVMe指令可以馈送它，而无需另一个FTL形状更改。

---

## 多开放DSB设计

当前的FTL有一个`host_open_dsb`还有一个`gc_open_dsb`。将主机打开状态替换为每个流的打开dsb:

```c
struct ftl_ctx {
    struct ftl_config config;
    struct ftl_map_ctx runtime_map;
    struct ftl_map_log map_log;
    struct ftl_trim_log trim_log;
    struct ftl_stream_mgr stream_mgr;
    struct ftl_data_superblock *gc_open_dsb;
};
```

写入位置:

```c
struct ftl_stream *stream = ftl_stream_select(&ctx->stream_mgr, req);
ret = dsb_reserve_write(&ctx->dsb_mgr, &stream->open_dsb, &target);
```

限制：

```text
max_open_host_streams = 3 initially
GC stream is separate and not counted against host streams
```

如果有太多主机流需要打开dsb，则使用关闭最近写入最少的打开流`dsb_close()`。DSB关闭/free/retire内部仍然由DSB代码拥有。

---

## 调试/跟踪设计

创建：

- `include/ftl/ftl_debug.h`
- `src/ftl/ftl_debug.c`
- `tests/test_ftl_debug.c`

添加结构化计数器和跟踪事件，而不是可视化。

计数器:

```c
struct ftl_debug_counters {
    u64 submit_read;
    u64 submit_write;
    u64 submit_trim;
    u64 route_read_worker[16];
    u64 route_write_trim;
    u64 map_lookup_hit;
    u64 map_lookup_miss;
    u64 map_update;
    u64 map_trim;
    u64 p2l_collision;
    u64 trim_ranges_coalesced;
    u64 stream_select[FTL_STREAM_COUNT];
    u64 dsb_reserve_busy;
    u64 backend_submit_busy;
    u64 long_tail_backend_wait;
};
```

每个新的子系统都公开了一个快照功能，该功能可以复制计数器而无需重置它们:

```c
void ftl_debug_snapshot(struct ftl_ctx *ctx,
                        struct ftl_debug_counters *out);
```

此计划中没有实现仪表板、图表、JSON导出器或可视化管道。

---

## 文件结构

创建：

- `include/ftl/ftl_map.h`和`src/ftl/ftl_map.c`: 运行时L2P/P2L映射API。
- `include/ftl/ftl_map_log.h`和`src/ftl/ftl_map_log.c`: 内存中突变日志。
- `include/ftl/ftl_trim_log.h`和`src/ftl/ftl_trim_log.c`: 修剪范围合并和切片。
- `include/ftl/ftl_stream.h`和`src/ftl/ftl_stream.c`: 流分类和每流开放DSB所有权。
- `include/ftl/ftl_debug.h`和`src/ftl/ftl_debug.c`: 调试计数器和快照。
- `tests/test_ftl_map.c`
- `tests/test_ftl_map_log.c`
- `tests/test_ftl_trim_log.c`
- `tests/test_ftl_stream.c`
- `tests/test_ftl_worker_roles.c`

修改：

- `include/ftl/ftl.h`: 添加地图/log/trim/stream/debug字段到`struct ftl_ctx`.
- `include/ftl/ftl_worker.h`: 将固定的worker数组语义替换为基于角色的前端worker。
- `src/ftl/ftl.c`: 通过新地图路由写入/修剪页面操作/stream/logApi，同时保留同步公共api。
- `src/ftl/ftl_worker.c`: 将LBA哈希路由替换为操作角色路由。
- `src/ftl/gc.c`: 通过突变路径提交GC映射更新或`ftl_map_update_if_equal()`.
- `Makefile`: 添加新的FTL模块和测试。

不要修改:

- `src/ftl/data_superblock.c`DSB内部构件。
- `src/ftl/superblock.c`检查点/恢复内部。
- 媒体/HAL/controller层，除非编译依赖项需要仅包含调整。

---

## 阶段0: 基线和护栏

### 任务1: 建立DSB-分支机构基线

**文件：**
- 没有源更改。

- [ ] **步骤1: 确认分支包含DSB接口**

运行：

```bash
rg -n "struct ftl_dsb_mgr|backend_sched_init|host_open_dsb|lba % FTL_NUM_WORKERS" include/ftl src/ftl
```

期望的：

```text
include/ftl/data_superblock.h contains struct ftl_dsb_mgr
include/ftl/backend_sched.h contains backend_sched_init
include/ftl/ftl.h contains host_open_dsb
src/ftl/ftl_worker.c contains lba % FTL_NUM_WORKERS
```

- [ ] **步骤2: 运行当前目标测试**

运行：

```bash
make build/bin/test_data_superblock
build/bin/test_data_superblock
make build/bin/test_backend_sched
build/bin/test_backend_sched
make build/bin/test_mt_ftl
build/bin/test_mt_ftl
```

预期: 每个二进制退出0。

- [ ] **步骤3: 将当前工作人员路由记录为回归目标**

在PR描述或实施日志中创建一个小注释:

```text
Before refactor, ftl_mt_submit routes worker_id = req->lba % FTL_NUM_WORKERS.
After Phase 1, WRITE/TRIM/FLUSH must route to the mutation worker and READ must route to read workers.
```

- [ ] **步骤4: 仅在添加了基线注释文件时提交**

如果没有更改文件，则不需要提交。

---

## 阶段1: 用基于角色的前端工作人员替换lba-hash工作人员

### 任务2: 首先添加worker角色测试

**文件：**
- 创建：`tests/test_ftl_worker_roles.c`
- 修改：`Makefile`

- [ ] **步骤1: 写入失败的路由测试**

创建`tests/test_ftl_worker_roles.c`:

```c
#include "ftl/ftl_worker.h"
#include "ftl/io_queue.h"
#include "common/common.h"
#include <assert.h>
#include <string.h>

static void test_write_trim_do_not_hash_by_lba(void)
{
    struct ftl_mt_ctx ctx;
    struct io_request w1;
    struct io_request w2;

    memset(&ctx, 0, sizeof(ctx));
    memset(&w1, 0, sizeof(w1));
    memset(&w2, 0, sizeof(w2));

    w1.opcode = IO_OP_WRITE;
    w1.lba = 0;
    w2.opcode = IO_OP_WRITE;
    w2.lba = 7;

    assert(ftl_mt_select_worker_for_test(&ctx, &w1) ==
           ftl_mt_select_worker_for_test(&ctx, &w2));

    w1.opcode = IO_OP_TRIM;
    w2.opcode = IO_OP_TRIM;
    w1.lba = 1024;
    w2.lba = 2049;
    assert(ftl_mt_select_worker_for_test(&ctx, &w1) ==
           ftl_mt_select_worker_for_test(&ctx, &w2));
}

int main(void)
{
    test_write_trim_do_not_hash_by_lba();
    return 0;
}
```

在下添加仅测试选择器原型`#ifdef HFSSS_TESTING`:

```c
u32 ftl_mt_select_worker_for_test(struct ftl_mt_ctx *ctx,
                                  const struct io_request *req);
```

- [ ] **步骤2: 运行并验证失败**

运行：

```bash
make build/bin/test_ftl_worker_roles
```

预期: 编译/链接失败，因为`ftl_mt_select_worker_for_test()`不存在。

### 任务3: 实现基于角色的worker元数据

**文件：**
- 修改：`include/ftl/ftl_worker.h`
- 修改：`src/ftl/ftl_worker.c`

- [ ] **步骤1: 替换标头中的固定工作人员形状**

改变`include/ftl/ftl_worker.h`要添加:

```c
#define FTL_MAX_READ_WORKERS 16

enum ftl_worker_role {
    FTL_WORKER_WRITE_TRIM = 0,
    FTL_WORKER_READ = 1,
};

struct ftl_mt_config {
    u32 read_worker_count;
};
```

改变`struct ftl_worker`包括:

```c
enum ftl_worker_role role;
```

改变`struct ftl_mt_ctx`包括:

```c
struct ftl_mt_config mt_config;
struct ftl_worker write_trim_worker;
struct ftl_worker read_workers[FTL_MAX_READ_WORKERS];
u32 read_worker_count;
_Atomic u32 next_read_worker;
```

保留现有的`workers[FTL_NUM_WORKERS]`仅在需要在第一个修补程序中保留编译时才临时使用。在此阶段结束时将其删除。

- [ ] **步骤2: 实现默认配置帮助程序**

In `src/ftl/ftl_worker.c`,添加:

```c
static u32 ftl_default_read_worker_count(const struct ftl_config *cfg)
{
    u32 n;
    if (!cfg || cfg->channel_count == 0) {
        return 1;
    }
    n = cfg->channel_count / 4;
    if (n == 0) {
        n = 1;
    }
    if (n > FTL_MAX_READ_WORKERS) {
        n = FTL_MAX_READ_WORKERS;
    }
    return n;
}
```

- [ ] **步骤3: 实现worker选择器**

In `src/ftl/ftl_worker.c`,添加:

```c
static struct ftl_worker *ftl_mt_select_worker(struct ftl_mt_ctx *ctx,
                                               const struct io_request *req)
{
    u32 idx;

    if (!ctx || !req) {
        return NULL;
    }

    switch (req->opcode) {
    case IO_OP_WRITE:
    case IO_OP_TRIM:
    case IO_OP_FLUSH:
        return &ctx->write_trim_worker;
    case IO_OP_READ:
        if (ctx->read_worker_count == 0) {
            return NULL;
        }
        idx = atomic_fetch_add(&ctx->next_read_worker, 1);
        return &ctx->read_workers[idx % ctx->read_worker_count];
    default:
        return &ctx->write_trim_worker;
    }
}

#ifdef HFSSS_TESTING
u32 ftl_mt_select_worker_for_test(struct ftl_mt_ctx *ctx,
                                  const struct io_request *req)
{
    struct ftl_worker *w = ftl_mt_select_worker(ctx, req);
    return w ? w->worker_id : UINT32_MAX;
}
#endif
```

- [ ] **步骤4: 运行worker-role测试**

运行：

```bash
make build/bin/test_ftl_worker_roles
build/bin/test_ftl_worker_roles
```

预期: 测试通过。

### 任务4: 将生命周期转换为角色工作者

**文件：**
- 修改：`src/ftl/ftl_worker.c`
- 修改：`tests/test_mt_ftl.c`

- [ ] **步骤1: 初始化write/TRIM worker**

添加帮助者:

```c
static int ftl_worker_init(struct ftl_worker *w, u32 worker_id,
                           enum ftl_worker_role role,
                           struct ftl_ctx *ftl)
{
    int ret;

    memset(w, 0, sizeof(*w));
    w->worker_id = worker_id;
    w->role = role;
    w->ftl = ftl;
    w->running = false;

    ret = io_ring_init(&w->request_ring, sizeof(struct io_request),
                       IO_RING_DEFAULT_CAPACITY);
    if (ret != HFSSS_OK) {
        return ret;
    }
    ret = io_ring_init(&w->completion_ring, sizeof(struct io_completion),
                       IO_RING_DEFAULT_CAPACITY);
    if (ret != HFSSS_OK) {
        io_ring_cleanup(&w->request_ring);
        return ret;
    }
    if (pthread_mutex_init(&w->request_lock, NULL) != 0) {
        io_ring_cleanup(&w->completion_ring);
        io_ring_cleanup(&w->request_ring);
        return HFSSS_ERR_NOMEM;
    }
    if (pthread_cond_init(&w->request_cond, NULL) != 0) {
        pthread_mutex_destroy(&w->request_lock);
        io_ring_cleanup(&w->completion_ring);
        io_ring_cleanup(&w->request_ring);
        return HFSSS_ERR_NOMEM;
    }
    w->request_sync_initialized = true;
    return HFSSS_OK;
}
```

- [ ] **步骤2: 初始化读取工作线程**

In `ftl_mt_init()`:

```c
ctx->read_worker_count = ftl_default_read_worker_count(config);
atomic_store(&ctx->next_read_worker, 0);
ret = ftl_worker_init(&ctx->write_trim_worker, 0,
                      FTL_WORKER_WRITE_TRIM, &ctx->ftl);
```

然后循环:

```c
for (u32 i = 0; i < ctx->read_worker_count; i++) {
    ret = ftl_worker_init(&ctx->read_workers[i], i + 1,
                          FTL_WORKER_READ, &ctx->ftl);
}
```

- [ ] **步骤3: 更新开始/stop/cleanup循环**

替换环路`FTL_NUM_WORKERS`与帮助者通话结束:

```text
write_trim_worker
read_workers[0..read_worker_count)
```

- [ ] **步骤4: 更新提交**

替换`req->lba % FTL_NUM_WORKERS`带有：

```c
struct ftl_worker *w = ftl_mt_select_worker(ctx, req);
```

推送至`w->request_ring`和信号`w->request_cond`.

- [ ] **步骤5: 更新完成轮询**

轮询write/TRIM worker和所有read worker:

```c
if (io_ring_pop(&ctx->write_trim_worker.completion_ring, out)) {
    return true;
}
for (u32 i = 0; i < ctx->read_worker_count; i++) {
    if (io_ring_pop(&ctx->read_workers[i].completion_ring, out)) {
        return true;
    }
}
return false;
```

- [ ] **步骤6: 运行测试**

运行：

```bash
build/bin/test_ftl_worker_roles
build/bin/test_mt_ftl
make test
```

预期: 全部通过。

- [ ] **第七步: 提交**

```bash
git add include/ftl/ftl_worker.h src/ftl/ftl_worker.c tests/test_ftl_worker_roles.c Makefile tests/test_mt_ftl.c
git commit -m "refactor(ftl): route frontend IO by operation role"
```

---

## 阶段2: 运行时L2P/P2L映射层

### 任务5: 添加`ftl_map`单元测试

**文件：**
- 创建：`tests/test_ftl_map.c`
- 修改：`Makefile`

- [ ] **步骤1: 为查找编写失败的测试/update/trim**

创建断言的测试:

```c
ftl_map_init(&map, 4096) == HFSSS_OK;
ftl_map_lookup(&map, 10, &ppn) == HFSSS_ERR_NOENT;
ftl_map_update(&map, 10, ppn_a, &old) == HFSSS_OK;
ftl_map_lookup(&map, 10, &got) == HFSSS_OK && got.raw == ppn_a.raw;
ftl_map_update(&map, 10, ppn_b, &old) == HFSSS_OK && old.raw == ppn_a.raw;
ftl_map_trim(&map, 10, &old) == HFSSS_OK && old.raw == ppn_b.raw;
ftl_map_lookup(&map, 10, &got) == HFSSS_ERR_NOENT;
```

- [ ] **步骤2: 写入失败的P2L碰撞测试**

在新的P2L索引容量下创建两个散列到同一存储桶的ppn，并在插入后断言两个反向查找都成功。

- [ ] **步骤3: 运行并验证编译失败**

运行：

```bash
make build/bin/test_ftl_map
```

预期: 缺失`ftl_map_*`符号。

### 任务6: 实施`ftl_map`

**文件：**
- 创建：`include/ftl/ftl_map.h`
- 创建：`src/ftl/ftl_map.c`
- 修改：`Makefile`

- [ ] **步骤1: 添加公共标头**

使用映射设计部分的结构和api:

```c
int ftl_map_init(struct ftl_map_ctx *map, u64 total_lbas);
void ftl_map_cleanup(struct ftl_map_ctx *map);
int ftl_map_lookup(struct ftl_map_ctx *map, u64 lba, union ppn *ppn_out);
int ftl_map_update(struct ftl_map_ctx *map, u64 lba,
                   union ppn new_ppn, union ppn *old_ppn_out);
int ftl_map_trim(struct ftl_map_ctx *map, u64 lba,
                 union ppn *old_ppn_out);
int ftl_map_update_if_equal(struct ftl_map_ctx *map, u64 lba,
                            union ppn expected_old,
                            union ppn new_ppn,
                            bool *updated_out);
```

- [ ] **步骤2: 实现地图页面分配**

`ftl_map_update()`分配目标`ftl_map_page`第一次写。`ftl_map_lookup()`从不分配。

- [ ] **步骤3: 实现原子查找**

读取路径:

```c
struct ftl_map_page *page =
    atomic_load_explicit(&map->pages[page_id], memory_order_acquire);
if (!page) {
    return HFSSS_ERR_NOENT;
}
raw = atomic_load_explicit(&page->entries[offset].raw, memory_order_acquire);
```

返回`HFSSS_ERR_NOENT`除非国家`FTL_MAP_PRESENT`.

- [ ] **步骤4: 实施单写入器更新**

使用发布存储进行条目发布。因为突变工作线程是唯一的主机写入器，所以正常更新不需要比较交换。

- [ ] **步骤5: 实现P2L开放寻址**

使用带有墓碑的线性探测:

```text
EMPTY: stop search
TOMBSTONE: reusable insert slot
VALID && ppn_raw matches: found
VALID && ppn_raw differs: continue
```

- [ ] **步骤6：运行测试**

运行：

```bash
build/bin/test_ftl_map
```

预期: 通过。

- [ ] **步骤7：提交**

```bash
git add include/ftl/ftl_map.h src/ftl/ftl_map.c tests/test_ftl_map.c Makefile
git commit -m "feat(ftl): add runtime L2P and P2L map layer"
```

### 任务7: 导线`ftl_map`在不改变电源路径的情况下进入FTL

**文件：**
- 修改：`include/ftl/ftl.h`
- 修改：`src/ftl/ftl.c`
- 修改：`src/ftl/gc.c`
- 修改：`tests/test_ftl.c`
- 修改：`tests/test_gc_mt.c`

- [ ] **步骤1: 添加运行时映射字段**

In `struct ftl_ctx`:

```c
struct ftl_map_ctx runtime_map;
```

- [ ] **步骤2: 初始化和清理运行时映射**

In `ftl_init()`几何图形已知后:

```c
ret = ftl_map_init(&ctx->runtime_map, config->total_lbas);
```

In `ftl_cleanup()`:

```c
ftl_map_cleanup(&ctx->runtime_map);
```

- [ ] **步骤3: 保留同步和电源路径的传统映射**

请勿移除`struct mapping_ctx mapping`。不要更改`sb_checkpoint_write_ex()` or `sb_recover_ex()`在这个阶段调用。

- [ ] **步骤4: 镜像主机写入/修剪突变**

主机写入当前调用的位置`mapping_update()` or `taa_update()`，电话`ftl_map_update()`第一，保持现有的遗留映射更新第二。这提供了运行时映射覆盖而不改变持久性行为。

- [ ] **步骤5: 读取路径在MT模式下使用运行时映射**

改变`ftl_read_page_mt_ex()`要调用:

```c
ret = ftl_map_lookup(&ctx->runtime_map, lba, &ppn);
```

保持同步`ftl_read()`在现有的`mapping_ctx`直到公共同步路径在稍后的阶段被转换。

- [ ] **步骤6: GC remap使用条件更新**

在GC重定位完成路径中:

```c
ret = ftl_map_update_if_equal(&ctx->runtime_map, lba,
                              src_ppn, dst_ppn, &updated);
```

然后将成功镜像到现有的`taa_update_if_equal()` or `mapping_update()`正如当前代码所期望的那样。

- [ ] **步骤7: 运行测试**

运行：

```bash
build/bin/test_ftl_map
build/bin/test_ftl
build/bin/test_gc_mt
make test
```

预期：全部通过。

- [ ] **步骤8: 提交**

```bash
git add include/ftl/ftl.h src/ftl/ftl.c src/ftl/gc.c tests/test_ftl.c tests/test_gc_mt.c
git commit -m "feat(ftl): wire runtime map into MT data path"
```

---

## 阶段3: 映射和修剪日志

### 任务8: 实现突变映射日志

**文件：**
- 创建：`include/ftl/ftl_map_log.h`
- 创建：`src/ftl/ftl_map_log.c`
- 创建：`tests/test_ftl_map_log.c`
- 修改：`Makefile`

- [ ] **步骤1: 编写失败的环测试**

测试：

```c
ftl_map_log_init(&log, 4) == HFSSS_OK;
append records with seq 1..5;
snapshot returns the newest 4 records;
lost_count == 1;
records are ordered by seq ascending in snapshot output;
```

- [ ] **步骤2: 实现固定大小的环**

对快照和追加使用一个锁。生产者是单线程的，但锁保持调试读者简单和安全。

- [ ] **步骤3: 从写入发出日志记录/TRIM/GCmap突变**

In `src/ftl/ftl.c`,每个成功的运行时映射突变后，追加:

```c
FTL_MAP_LOG_WRITE
FTL_MAP_LOG_TRIM
FTL_MAP_LOG_GC_REMAP
FTL_MAP_LOG_GC_SKIP
```

- [ ] **步骤4: 运行测试**

运行：

```bash
build/bin/test_ftl_map_log
build/bin/test_ftl
build/bin/test_gc_mt
```

预期：通过。

- [ ] **步骤5: 提交**

```bash
git add include/ftl/ftl_map_log.h src/ftl/ftl_map_log.c tests/test_ftl_map_log.c Makefile src/ftl/ftl.c src/ftl/gc.c
git commit -m "feat(ftl): add in-memory mapping mutation log"
```

### 任务9: 实现修剪范围日志和切片处理

**文件：**
- 创建：`include/ftl/ftl_trim_log.h`
- 创建：`src/ftl/ftl_trim_log.c`
- 创建：`tests/test_ftl_trim_log.c`
- 修改：`src/ftl/ftl_worker.c`
- 修改：`src/ftl/ftl.c`
- 修改：`Makefile`

- [ ] **步骤1: 编写失败的合并测试**

测试用例:

```text
append [100, 120) then [120, 140) -> one range [100, 140)
append [200, 220) then [210, 230) -> one range [200, 230)
append [300, 320) then [330, 340) -> two ranges
```

- [ ] **步骤2: 实施`ftl_trim_log_append()`**

维护排序范围`start_lba`。合并重叠和相邻的范围。

- [ ] **步骤3: 添加切片修剪工人循环**

当TRIM请求超过4096页时:

```c
while (remaining > 0) {
    slice = min(remaining, 4096);
    clear slice through ftl_map_trim();
    mirror existing mapping/TAA trim behavior;
    remaining -= slice;
    service one queued WRITE if present before continuing;
}
```

这可以防止长时间修剪垄断变更工作线程。

- [ ] **步骤4: 保留完成正确性**

只有在所有切片都在逻辑上取消映射后，才会推送TRIM请求的完成。

- [ ] **步骤5: 运行测试**

运行：

```bash
build/bin/test_ftl_trim_log
build/bin/test_dsm
build/bin/test_mt_ftl
make test
```

预期：通过。

- [ ] **步骤6: 提交**

```bash
git add include/ftl/ftl_trim_log.h src/ftl/ftl_trim_log.c tests/test_ftl_trim_log.c Makefile src/ftl/ftl_worker.c src/ftl/ftl.c
git commit -m "feat(ftl): add coalesced TRIM log and sliced trim processing"
```

---

## 阶段4: 多流多开放DSB

### 任务10: 添加流管理器

**文件：**
- 创建：`include/ftl/ftl_stream.h`
- 创建：`src/ftl/ftl_stream.c`
- 创建：`tests/test_ftl_stream.c`
- 修改：`Makefile`

- [ ] **步骤1: 写入失败的流选择测试**

测试：

```c
ftl_stream_select_write(&mgr, 100, false) == FTL_STREAM_DEFAULT;
after 32 sequential LBAs, stream == FTL_STREAM_SEQ;
overwrite of recently written LBA maps to FTL_STREAM_HOT;
GC requests map to FTL_STREAM_GC;
```

- [ ] **步骤2: 实现流管理器**

使用来自多流设计的结构。保持历史记录较小:

```text
last_lba
sequential_run
small overwrite bloom/hash table with 4096 slots
```

- [ ] **步骤3: 运行流测试**

运行：

```bash
build/bin/test_ftl_stream
```

预期：通过。

- [ ] **步骤4: 提交**

```bash
git add include/ftl/ftl_stream.h src/ftl/ftl_stream.c tests/test_ftl_stream.c Makefile
git commit -m "feat(ftl): add stream classification manager"
```

### 任务11: 将单个主机open DSB替换为按流open DSB

**文件：**
- 修改：`include/ftl/ftl.h`
- 修改：`src/ftl/ftl.c`
- 修改：`tests/test_data_superblock.c`
- 修改：`tests/test_ftl.c`

- [ ] **步骤1: 将流管理器添加到`ftl_ctx`**

```c
struct ftl_stream_mgr stream_mgr;
```

保持`gc_open_dsb`作为一个单独的领域。

- [ ] **步骤2: 替换主机写DSB指针用法**

从以下位置更改主机写入位置:

```c
dsb_reserve_write(&ctx->dsb_mgr, &ctx->host_open_dsb, &target);
```

to:

```c
struct ftl_stream *stream =
    ftl_stream_select_host_write(&ctx->stream_mgr, lba, had_old);
dsb_reserve_write(&ctx->dsb_mgr, &stream->open_dsb, &target);
```

- [ ] **步骤3: 强制执行开放主机流限制**

如果选定的流需要打开的DSB和`open_host_stream_count >= max_open_host_streams`,关闭最近最少使用的主机流DSB:

```c
dsb_close(&ctx->dsb_mgr, victim_stream->open_dsb);
victim_stream->open_dsb = NULL;
```

- [ ] **步骤4: 保持GC流分离**

GC重定位仍然使用:

```c
dsb_reserve_write(&ctx->dsb_mgr, &ctx->gc_open_dsb, &target);
```

不要通过主机流限制路由GC。

- [ ] **步骤5: 添加测试**

添加断言:

```text
sequential writes reuse FTL_STREAM_SEQ open DSB
random writes use FTL_STREAM_DEFAULT open DSB
hot overwrite uses FTL_STREAM_HOT open DSB
open host stream count never exceeds max_open_host_streams
GC open DSB is not counted as host open stream
```

- [ ] **步骤6：运行测试**

运行：

```bash
build/bin/test_ftl_stream
build/bin/test_data_superblock
build/bin/test_ftl
build/bin/test_gc_mt
make test
```

预期：通过。

- [ ] **步骤7：提交**

```bash
git add include/ftl/ftl.h src/ftl/ftl.c tests/test_data_superblock.c tests/test_ftl.c tests/test_gc_mt.c
git commit -m "feat(ftl): place host writes through per-stream open DSBs"
```

---

## 阶段5: 后端调度程序执行集成

### 任务12: 添加FTL后端执行包装器

**文件：**
- 创建：`include/ftl/ftl_backend_exec.h`
- 创建：`src/ftl/ftl_backend_exec.c`
- 创建：`tests/test_ftl_backend_exec.c`
- 修改：`Makefile`

- [ ] **步骤1: 编写失败的后端执行测试**

测试：

```text
submit read op with PPN channel 0 -> completion status OK
submit program op with PPN channel 1 -> completion status OK
submit op to invalid channel -> HFSSS_ERR_INVAL
backend group waits until all submitted ops complete
```

- [ ] **步骤2: 实现包装API**

```c
int ftl_backend_submit_read(struct ftl_ctx *ctx, union ppn ppn,
                            void *data, void *spare,
                            enum ftl_backend_priority prio);
int ftl_backend_submit_program(struct ftl_ctx *ctx, union ppn ppn,
                               const void *data, void *spare,
                               enum ftl_backend_priority prio);
int ftl_backend_submit_erase(struct ftl_ctx *ctx, union ppn ppn,
                             enum ftl_backend_priority prio);
```

后端计划程序工作线程调用完成回调，该回调调用通道工作线程内的现有HAL同步操作。

- [ ] **步骤3: 路由MT通过后端包装器读取**

In `ftl_read_page_mt_ex()`,直接替换`hal_nand_read_sync()`与`ftl_backend_submit_read()`并等待操作组。

- [ ] **步骤4: 通过后端包装器路由MT主机写入**

In `ftl_write_page_mt_ex()`,替换直接`hal_nand_program_sync()`与`ftl_backend_submit_program()`.

- [ ] **步骤5: 最初保持同步公共api不变**

`ftl_read()`和`ftl_write()`可能会继续其现有的同步HAL调用，直到稍后的阶段。这限制了爆炸半径并保持了传统测试的稳定。

- [ ] **步骤6：运行测试**

运行：

```bash
build/bin/test_backend_sched
build/bin/test_ftl_backend_exec
build/bin/test_mt_ftl
build/bin/test_data_superblock
make test
```

预期：通过。

- [ ] **步骤7：提交**

```bash
git add include/ftl/ftl_backend_exec.h src/ftl/ftl_backend_exec.c tests/test_ftl_backend_exec.c Makefile src/ftl/ftl.c
git commit -m "feat(ftl): execute MT NAND IO through backend scheduler"
```

---

## 阶段6: 调试计数器和跟踪挂钩

### 任务13: 添加FTL调试计数器

**文件：**
- 创建：`include/ftl/ftl_debug.h`
- 创建：`src/ftl/ftl_debug.c`
- 创建：`tests/test_ftl_debug.c`
- 修改：`include/ftl/ftl.h`
- 修改：`src/ftl/ftl_worker.c`
- 修改：`src/ftl/ftl.c`
- 修改：`src/ftl/ftl_map.c`
- 修改：`src/ftl/ftl_trim_log.c`
- 修改：`src/ftl/ftl_stream.c`
- 修改：`Makefile`

- [ ] **步骤1: 写入失败的快照测试**

测试：

```c
struct ftl_debug_counters before;
struct ftl_debug_counters after;
ftl_debug_snapshot(&ctx.ftl, &before);
submit one write and one read;
ftl_debug_snapshot(&ctx.ftl, &after);
assert(after.submit_write == before.submit_write + 1);
assert(after.submit_read == before.submit_read + 1);
```

- [ ] **步骤2: 实现计数器**

使用`_Atomic u64`里面`struct ftl_debug_state`。快照复制具有宽松负载的每个计数器。

- [ ] **步骤3: 添加计数器增量**

在以下位置递增计数器:

```text
ftl_mt_submit route decision
map lookup hit/miss
map update/trim
P2L collision
trim coalesce
stream select
DSB reserve busy
backend submit busy
backend wait above threshold
```

- [ ] **步骤4: 为长尾调查添加跟踪消息**

使用现有的跟踪/日志基础结构。消息必须包括:

```text
op, lba, stream_id, dsb_id, ppn, queue_depth, wait_ns, status
```

- [ ] **步骤5：运行测试**

运行：

```bash
build/bin/test_ftl_debug
build/bin/test_ftl_worker_roles
build/bin/test_mt_ftl
make test
```

预期：通过。

- [ ] **步骤6：提交**

```bash
git add include/ftl/ftl_debug.h src/ftl/ftl_debug.c tests/test_ftl_debug.c Makefile include/ftl/ftl.h src/ftl/ftl_worker.c src/ftl/ftl.c src/ftl/ftl_map.c src/ftl/ftl_trim_log.c src/ftl/ftl_stream.c
git commit -m "feat(ftl): add debug counters for routing mapping trim and DSB placement"
```

---

## 阶段7: 清理、兼容性和覆盖

### 任务14: 删除旧的lba-hash worker残余

**文件：**
- 修改：`include/ftl/ftl_worker.h`
- 修改：`src/ftl/ftl_worker.c`
- 修改：`tests/test_ftl_worker_roles.c`

- [ ] **步骤1: 删除`FTL_NUM_WORKERS`正确性语义**

如果宏保持兼容性，请重命名其含义:

```c
#define FTL_DEFAULT_READ_WORKERS 4
```

没有代码可以路由`req->lba % FTL_NUM_WORKERS`.

- [ ] **步骤2: 将基于grep的回归测试命令添加到计划证据**

运行：

```bash
rg -n "lba % FTL_NUM_WORKERS|req->lba %|worker_id = req->lba" include/ftl src/ftl tests
```

预期: 除文档中的历史注释外，没有匹配项。

- [ ] **步骤3: 运行测试**

运行：

```bash
build/bin/test_ftl_worker_roles
build/bin/test_mt_ftl
make test
```

预期：通过。

- [ ] **步骤4：提交**

```bash
git add include/ftl/ftl_worker.h src/ftl/ftl_worker.c tests/test_ftl_worker_roles.c
git commit -m "refactor(ftl): remove LBA-hash worker ownership remnants"
```

### 任务15: 覆盖和最终验证

**文件：**
- 除非测试暴露间隙，否则不需要更改源。

- [ ] **步骤1: 运行聚焦FTL套件**

运行：

```bash
build/bin/test_ftl_map
build/bin/test_ftl_map_log
build/bin/test_ftl_trim_log
build/bin/test_ftl_stream
build/bin/test_ftl_worker_roles
build/bin/test_ftl_backend_exec
build/bin/test_ftl_debug
build/bin/test_data_superblock
build/bin/test_backend_sched
build/bin/test_mt_ftl
build/bin/test_gc_mt
```

预期: 全部退出0。

- [ ] **步骤2: 运行完整套件**

运行：

```bash
make test
```

应为: 退出0。

- [ ] **步骤3: 运行覆盖率**

运行：

```bash
make coverage-clean
make coverage-ut
bash scripts/coverage/ratchet_check.sh
```

期望的：

```text
make coverage-ut exits 0
ratchet check passes
FTL-related new modules have targeted unit coverage
overall line coverage stays at or above the existing 85% target
```

- [ ] **步骤4: 运行静态空白检查**

运行：

```bash
git diff --check
```

预期: 无输出，退出0。

- [ ] **步骤5: 如果在清理后添加了测试，则最终提交**

```bash
git add tests Makefile
git commit -m "test(ftl): cover single-core mutation and read-worker split"
```

---

## 风险及缓解措施

### 风险: 运行时映射与旧映射不同

缓解措施: 在此计划期间，将更新镜像到两个`ftl_map_ctx`和现有的`mapping_ctx`/`taa_ctx`。添加在写入、覆盖、修剪和GC重定位后比较查找结果的测试。

### 风险: 单个写/修剪工人成为瓶颈

缓解: worker仅拥有元数据突变。NAND程序/erase/read工作被推送到后端渠道工作人员。大修剪被切片以防止变更工作线程垄断。

### 风险: Read在覆盖期间看到过时的映射

缓解: 仅在程序成功后发布新的映射条目。旧PPN保持有效，直到变更工作线程完成映射更新，然后旧PPN无效。“读取” 会看到旧的有效数据或新的有效数据。

### 风险: GC重新映射竞争主机覆盖

缓解措施: GC使用`ftl_map_update_if_equal()`预期的受害者PPN。如果主机覆写已经改变了LBA，则GC重定位被丢弃，并且重定位的页被标记为无效。

### 风险: 多流开放太多dsb

缓解措施:`max_open_host_streams`默认为3。LRU主机流DSB在达到限制时关闭。GC DSB保持独立。

---

## 完成条件

重构在以下情况下完成:

1. `ftl_mt_submit()`不再路由`LBA % worker_count`.
2. 书写/TRIM/FLUSH由突变工作者处理。
3. 读取由可配置的读取工作线程处理。
4. MT主机通过每流开放dsb写入保留/提交。
5. 单元测试涵盖了运行时L2P/P2L更新。
6. 映射突变日志记录write、trim、GC remap和GC skip事件。
7. 修剪范围日志合并相邻/重叠的范围，并处理切片中的大范围。
8. 调试计数器公开路由、映射、TRIM、流、DSB和后端等待事件。
9. 无DSB内部坏块/layout/retire逻辑被修改了。
10. 无检查点/recovery/power-cycle逻辑被修改了。
11. `make test`通过了。
12. `make coverage-ut`传球和棘轮保持绿色。
