# 切换-FTL die-busy等待-队列票证-锁定跟进

**日期：** 2026-05-08
**handoff的作者:**上一届会议 (克劳德代码)
**用于：**下一个代理 (Codex) 拿起工作
**项目：** `/Users/zifengyang/Desktop/hfsss-simulator`

这份文件是独立的: 阅读它，你可以准确地拿起
上次会话停止的位置。这项工作是在一个
打开PR (#118，处于查看-响应状态) 和排队跟进
(票证锁定，准备开始一次 #118合并)。

---

## 1.状态摘要

### 1.1做了什么

**PR #118** — `perf(ftl): per-die wait-queue + completion-driven dispatch`

网址：https://github.com/AlvinYangZF/hfsss-simulator/pull/118
分支：`perf/ftl-die-busy-waitqueue`
负责人提交:`867651a`(修复 (ftl): 地址PR #118评论-线程安全注入内存顺序)

完成10个实施任务 (所有已提交和已推送):

|任务|提交|它交付了什么|
|---|---|---|
| T1 | `eaf9967` | cmd_enginedie就绪通知程序挂钩 (锚点A和B; 而不是C)|
| T2 | `f069758` | `die_dispatcher`数据结构L1单元测试 (25例)|
| T3 | `a3e0f80` | `die_dispatcher_wait` + `on_die_ready`L2 mt试验 (11例)|
| T4 | `567578e` |L3实数-cmd_engine整合测试 (22例)|
| T5 | `bf62ef6` | ftl_mt将retry-spin替换为dispatcher_wait + `_ex`变体|
| T6 | `648c1bd` | GC `gc_trigger_t`线程每触发器优先级|
| T7 | `fc78c31` |L4优先级WFQ集成测试 (5个统计案例)|
| T8 | `ee24847` |封闭故障注入旋钮 (`HFSSS_DIE_DISP_FORCE_BUSY`, `HFSSS_DIE_DISP_NOTIFIER_DELAY_NS`) |
| T9 | `8fac7d7` |CLAUDE.md诊断REQUIREMENT_COVERAGE更新|
| T10 | `ffc7f1a` + `eaa5a1a` |重试预算调整为超过锁饥饿; 后impl基线存档; 规范KPI对账|
|复查修复| `867651a` |线程安全的RNG，获取/发布顺序，GC预算原理|

**规格计划 (在PR #118中提交):**
- 调度员规格:`docs/superpowers/specs/2026-04-30-ftl-die-busy-waitqueue-design.md`
- 调度员计划:`docs/superpowers/plans/2026-04-30-ftl-die-busy-waitqueue.md`
- 票-锁规格:`docs/superpowers/specs/2026-05-08-cmd-engine-ticket-lock-design.md`
- 票锁定计划:`docs/superpowers/plans/2026-05-08-cmd-engine-ticket-lock.md`

**已存档的基线:**
- `docs/perf-baselines/2026-04-29-master-0eb0d83/`-Pre-impl (主基线)
- `docs/perf-baselines/2026-04-30-perf-ftl-die-busy-waitqueue/`-实施后

**添加测试 (所有绿色开启`make test`):**
- `tests/test_cmd_engine_notifier.c`(13例)
- `tests/test_die_dispatcher_unit.c`(25例)
- `tests/test_die_dispatcher_mt.c`(11例)
- `tests/test_die_dispatcher_engine.c`(22例)
- `tests/test_die_dispatcher_faults.c`(6例)
- `tests/cmd_engine_mock.{h,c}`(L2的模拟)
- L4例扩展`test_ftl_mt.c`和`test_gc_mt.c`

### 1.2什么是不送的

PR #118 **不符合原始性能KPI**。诚实的数字:

|指标|基线 (主)|原始目标|已实现|
|---|---|---|---|
|012 W平均lat| 142.0 ms | ≤ 10 ms | 141.8 ms ❌ |
|012瓦p99 lat| 152.0 ms | ≤ 20 ms |143.6 ms (轻微)|
|012 W IOPS| 112 | ≥ 1500 | 112 ❌ |
|预签入|(主修复路径可能会在压力下失败)|8/8通行证|8/8通行证✅|
|其他情况 (010/011/013/014) | — |在 ± 10% 内|在 ± 2% 内✅|

**为什么**: cmd_engine's `die_lock`和`channel_lock`是
`pthread_mutex_t`-不公平.调度员的叫醒命令是
被锁定的人与新来的人竞争，所以一个示意的服务员
反复输给新来的FTL 工作线程并重新排队。这个
总等待时间收敛到与前一个大致相同的值
nanosleep重试-旋转。

这在PR #118的规范9.2节中有三个记录
后续路径。

---

## 2.立即下一步-完成PR #118

### 2.1等待CI

提交后`867651a`被推送，GitHub操作重新触发:
- 在macos上构建-最新
- 在ubuntu上构建-最新
- 内核模块构建 (Linux)

当写这个交接时，所有三个都是`pending`。可能的
持续时间为30-60 s，每个基于先前的运行。

**Do**: `gh pr checks 118`直到所有三个`pass` or `fail`.

```sh
until gh pr checks 118 --json bucket --jq 'all(.bucket != "pending")' 2>/dev/null | grep -q true; do
  sleep 30
done
gh pr checks 118
```

### 2.2 CI是否为绿色

移交给人工审阅者进行合并。做**不**合并PR
你自己，除非人类明确地这么说-这是一个不平凡的
人类最后一次想要关注的基础设施公关。

### 如果CI失败，则2.3

通过读取故障日志`gh run view <run-id> --log-failed`。大多数
可能的候选人:
- 来自审查的编译错误-修复提交 (已在本地验证
  但CI是在Linux上，它有微妙的不同的标题)。
- 种族敏感的测试片-重新运行; 如果它持续2/3运行，它的
  真实的。

不要推动投机性修复。先诊断，然后打补丁。

### 2.4开放评论评论

审稿人 (AlvinYangZF) 将三个重要项目推迟到
票证锁定跟进:
- 重要3:`dispatch_list_del`重载 “不在列表中” 与
  通过自循环 “已经出列”。fix提案是一个明确的
  状态枚举 (`WAITING / DEQUEUED / TIMED_OUT`) on `struct die_waiter`.
- NIT 2:`gc_hal_*`重试帮助程序重复。
- NIT 3:`die_priority_slot(DIE_PRIO_GC_CRITICAL)`返回-1。

在锁票工作期间决定这些是否自然折叠
你在那里所做的改变。无阻止合并。

---

## 3.后续工作-票-锁PR

规格：`docs/superpowers/specs/2026-05-08-cmd-engine-ticket-lock-design.md`
计划：`docs/superpowers/plans/2026-05-08-cmd-engine-ticket-lock.md`

该计划有四个咬大小的任务 (TL1-TL4)，并带有明确的代码
样品和TDD步骤。在开始之前，端到端地阅读计划文件。

### 3.1先决条件

- PR #118已合并。
- `git checkout master && git pull origin master`.
- `git checkout -b perf/cmd-engine-ticket-lock`.
- 调度程序基础结构 (通知程序挂钩、每模等待队列、
  GC触发线程) 在 #118合并后在主服务器上活动。

### 3.2任务摘要 (计划中的全部内容)

**TL1-票证锁基本单元测试**
- 文件：`include/common/ticket_lock.h`, `src/common/ticket_lock.c`,
  `tests/test_ticket_lock.c`,Makefile。
- 原子票服务柜台; 自旋收益收购。
- 单元测试: 单线程，争用下的FIFO顺序，
  64螺纹 × 10000应力，try_lock语义学。
- ~ 50 LOC原语 ~ 150 LOC测试。

**TL2-更换`die_lock`带检票锁**
- 文件：`include/media/nand.h`(结构nand_die), `src/media/nand.c`
  (init/cleanup),`src/media/cmd_engine.c`(每个触及的站点
  `die->die_lock`).
- 运行回归套件 (`test_media` IS-04/IS-06/SUSPENDED,
  `test_cmd_engine_notifier`, `test_die_dispatcher_*`,
  `test_reset_abort_race`).
- 运行`make pre-checkin`并捕获012个数字-期望a
  显著下降 (平均从 ~ 142毫秒到20-40毫秒范围)。

**TL3-更换`channel_lock`带票务锁定功能**
- 相同的待遇`struct nand_channel`.
- 在此任务之后`make pre-checkin`应显示012平均值 ≤ 10 ms，
  p99 ≤ 20 ms，IOPS ≥ 1500。这是KPI的原始规范
  要求的。
- 在以下位置存档新基线
  `docs/perf-baselines/<date>-cmd-engine-ticket-lock/`.

**TL4-规范对账开放公关**
- 更新PR #118的规范部分9.2，将选项1标记为已合并
  实现的KPI数字。
- 更新`docs/REQUIREMENT_COVERAGE.md`超光速小计。
- 推送分支，打开PR标题
  `perf(cmd_engine): FIFO-fair ticket lock — close 012 latency gap from dispatcher PR`.
- 主体: 参考PR #118; 显示基线 → 模后锁定 →
  post-既锁定数字级数; 解释为什么这是
  缩小差距的最小后续行动 (根据规范9.2)。

已在本地任务列表中创建的任务id: #227-#230 (每个
TL任务与适当的`blockedBy`链条）。如果您使用不同的任务
系统，忽略这些id。

---

## 4.不在已提交文档中的关键上下文

### 4.1 2048 × 10毫秒重试预算的原因

最初的规范要求`8 × 50 ms = 400 ms`。在fio-012下
(`bs=128k iodepth=16`有8名FTL工人从SQ喂养)
预算耗尽: 观察到的最大重试次数为63。颠簸到
2048 × 10 ms = ~ 20 s是 “最坏情况下的锁饥饿”
天花板，远低于NVMe 30 s超时。车票锁定后
土地，预算可能会削减; 这是TL3的子决定
值得留给谁拿起工作。

### 4.2为什么GC有不同的 (更严格的) 预算

GC: 8 × 50 ms = 400。
主机IO: 2048 × 10 ms = 20 s。

GC从NOSPC下的主机写入路径内联运行。停滞的GC
吸收等待空闲空间的每个主机写入。400 ms天花板
故意限制GC的阻挡窗口，以便卡住的GC通过表面
繁忙 → 主机重试，而不是多秒的NVMe停顿。之后
ticket-lock lands GC争用急剧下降; 更紧的
预算可以保持。

### 4.3为什么`dispatch_list_del`通过自循环重载 “不在列表中”

`die_waiter_init`电话`dispatch_list_init(&w.list)`这使得
列表节点自循环。`dispatch_list_del`弹出后也离开
节点自循环。所以`dispatch_list_empty(&w.list)`返回true
对于新节点和弹出节点。这个`die_dispatcher_wait`
超时路径使用此来决定 “服务员是否仍在排队，或者
通知人已经拿走了？"没有额外的状态字段。

如果票证锁定后续更改`dispatch_list_del`语义学
(例如，重用列表指针的东西)，等待路径
必须返工自拆卸逻辑。审稿人建议
显式状态枚举-考虑该下拉是否值得〜10
TL2/tl3期间的LOC。

### 4.4调度员的perf开销大致为零

PR #118在前面添加了dispatchercmd_engine。我们确认
014_应力，010_randwrite，011_randrw和013_trim都保持在
± 主基线的2%。调度员没有放慢速度
即使在工作负载下，它也无济于事。这意味着
票证锁定修复可以独立且干净地着陆-没有回滚风险
从调度员本身。

### 4.5子代理调度对于类似形状的任务运行良好

任务T1、T2、T3、T4、T6、T7、T8被分派为`general-purpose`
使用严格任务提示模板的子代理。任务T5和T10是
由主会话在线完成，因为 (a) T5需要以前的
会话的分支/文件状态的置信度和 (b) T10是
和解步骤。票证锁定计划 (TL1-TL4) 的大小为
子代理调度-特别是TL1是独立的。

### 4.6构建系统怪癖

T10 a期间一次`make`调用在一个
页眉编辑。通过 “rm” 强制重建-f构建/lib/libhfsss-ftl.a
构建/bin/hfsss-nbd-server` then `让一切都解决它。值得了解
如果您看到 “0字节更改，但测试仍然以相同的方式失败”。

### 4.7 macOSpthread_mutex具体不公平

特别是针对macOS记录了公平性问题，但是
PTHREAD_MUTEX_DEFAULT在Linux上，glibc也是非FIFO。门票
锁定修复在两个平台上都是正确的。CI运行在
ubuntu-最新的macos-最新的内核-模块-Linux-所有三个应该
同样受益。

---

## 5.留给下一个代理人的决定

1. **TL2应该独立于TL3着陆吗？**该计划有他们作为
   顺序任务，但技术上单独的TL2 (只是`die_lock`)
   应该会产生大部分的延迟胜利，因为`die_lock`是
   大多数人争辩。值得在TL2之后测量并决定是否
   如果单独运送TL2`channel_lock`争执原来是
   可以忽略不计的。

2. **重要的3个审查项目 (国家枚举) 是否应该落在
   检票锁PR？**~ 10 LOC，使超时路径自删除
   逻辑更清晰。温和的偏好是。

3. **GC重试预算是否应更改？**票证锁定后
   争用风暴消失，所以GC的400 ms天花板有更多
   边距。别管它，除非分析显示GC正在击中
   天花板。

4. **规格段9.2标记**: 当票证锁PR打开时，
   它应该内联修改PR #118的规范文档，还是添加新的规范
   文档？在线更新更容易发现; 新文档更容易发现
   历史上干净。采用在线方法与
   此回购中的现有模式 (`docs/superpowers/specs/`节目
   每个功能的单文档，在同一文件中进行协调)。

---

## 6.下一个代理的快速定位

**回购布局:**
- `src/ftl/die_dispatcher.c`(内部标头)-新调度程序
- `src/ftl/ftl.c`-主机-IO重试循环`_ex`变体
- `src/ftl/gc.c`-GC体与`gc_hal_*_with_wait`包装物
- `src/media/cmd_engine.c`-锚A和B通知站点
- `include/ftl/die_dispatcher.h`-公共API
- `include/media/nand.h` — `die_ready_notifier`字段打开`nand_device`

**构建/测试:**
- `make all`-完整的构建
- `make test`-全单元/系统套件 (~ 30 s)
- `make pre-checkin`-QEMU blackbox 9-案例捆绑包 (〜15分钟)
- 强制重建:`rm -rf build && make all`

**CI:**在PR打开的情况下推送到任何分支的GitHub操作。macOS
ubuntu内核模块构建 (CI中没有测试运行; 测试是本地的)。

**项目规则** (`CLAUDE.md`):
- 英语-仅在代码中。
- 提交、代码中没有 “固定/步骤/阶段/周” 进度条款
  评论或公关机构。
- 任何地方都没有AI工具名称。
- 避免使用行号锚定文档 (“请参阅行432”)-使用概念
  描述。
- 测试驱动: 在每个任务中，测试都先于实现。

**上一个会话使用的技能:**
`superpowers:subagent-driven-development`-派遣一个子代理
具有自包含提示的任务。计划文件的任务部分是
已成型，可直接复制到子代理提示中。

---

## 7.切换时的最终状态

- `git status`清洁 (相对于push at`867651a`)
- 本地分支机构:`perf/ftl-die-busy-waitqueue`(提前16次提交
  主)
- 推到`origin/perf/ftl-die-busy-waitqueue`
- PR #118状态: 审核-已推送修复，CI重新运行
- 没有卡住的后台任务 (在会话中验证)
- `/tmp/pre-checkin.log`包含最近的运行输出
  (8/8通过)

祝你好运。
