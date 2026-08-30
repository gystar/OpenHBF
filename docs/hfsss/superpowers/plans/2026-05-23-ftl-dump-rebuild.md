# FTL转储和重建实施计划

> **对于代理型员工：**所需子技能：使用superpowers:subagent-driven-development（推荐）或superpowers:executing-plans，逐任务执行本计划。步骤使用复选框（`- [ ]`) 用于跟踪的语法。

**目标：**为L2P表，多级映射页面，突变日志/日志和修剪位图构建独立的FTL转储/重建机制，以便工程师可以调试，验证和离线重建映射状态，而无需使P2L成为持久的事实来源。

**架构：**添加一个独立的FTL转储模块，该模块导出版本化的二进制部分以及人类可读的摘要。转储输入是运行时映射状态、有序突变记录、持久性日志记录和修剪位图/范围状态。重建加载基本L2P/多级映射快照，重播有序写入/GC/TRIM记录，按顺序规则重新应用修剪状态，然后将P2L重建为派生的运行时索引。

**技术栈：**C11，现有FTL映射/superblock/journal类型、CRC32帮助程序、快照读取器的pthread锁、现有Makefile工具中的单元测试。可选CLI工具仅限于本地调试使用，并且不是引导路径的一部分。

**参考：**这个计划是故意分开的`docs/superpowers/plans/2026-05-23-single-ftl-read-write-split-dsb.md`.

**使用单FTL拆分计划订购:**如果此转储计划在`2026-05-23-single-ftl-read-write-split-dsb.md`,每个流打开DSB指针是运行时放置提示，而不是权威重建元数据。版本1实时转储排除那些打开的DSB指针，除非将来`FTL_DUMP_INCLUDE_RUNTIME_DEBUG`已添加标志。Rebuild从L2P、有序映射/日志记录和修剪状态导出P2L和活动计数; 它一定不需要打开的DSB指针。

**范围边界：**
- 此计划不会修改正常的通电恢复、检查点选择或断电路径。
- 此计划不会使P2L持久权威元数据。
- 此计划不会更改DSB内部布局、坏块替换或停用策略。
- 此计划不添加可视化仪表板。它只发出转储文件、摘要文件和结构化重建报告，以供将来的可视化工作使用。

---

## 设计职位

### P2L为什么不持久化

P2L对于GC加速和调试检查很有用，但L2P仍然是权威映射:

```text
authoritative: L2P + ordered mutation records + TRIM state
derived:       P2L, block live counts, DSB live counts
```

将P2L保留为恢复源会产生双重权限问题:

```text
L2P says LBA X -> PPN A
P2L says PPN B -> LBA X
journal says X was trimmed after B
```

重建路径通过从重建的L2P重建P2L并通过仅将P2L转储不匹配报告为诊断来避免这种情况。

### 这个转储/重建机制是什么

用例：

1. 长尾延迟、GC异常或映射不一致后的离线诊断。
2. 来自运行模拟器的映射状态的确定性再现。
3. 验证运行时L2P、映射日志、修剪位图和P2L-derived索引是否一致。
4. 在不更改正常启动恢复的情况下进行紧急取证重建实验。

非使用案例:

1. 正常通电恢复。
2. UPLP紧急元数据刷新。
3. 替换`sb_checkpoint_*` or `sb_recover_*`.
4. 使P2L或调试转储成为持久化SSD元数据协定的一部分。

---

## 转储格式

使用以下部分创建版本化的二进制转储文件:

```text
FTLD header
manifest section
L2P flat section
multi-level map directory section
multi-level map page section(s)
map mutation log section
persistent journal mirror section
TRIM bitmap section
TRIM range log section
optional P2L debug section
summary section
```

每个部分都有:

```c
struct ftl_dump_section_header {
    u32 type;
    u32 version;
    u64 offset;
    u64 length;
    u64 record_count;
    u64 base_sequence;
    u64 end_sequence;
    u32 crc32;
    u32 flags;
};
```

文件开头:

```c
#define FTL_DUMP_MAGIC 0x44505446u /* little-endian bytes: "FTPD" */
#define FTL_DUMP_VERSION 1u

struct ftl_dump_file_header {
    u32 magic;
    u32 version;
    u32 header_size;
    u32 section_count;
    u64 created_ns;
    u64 total_lbas;
    u64 page_size;
    u64 snapshot_sequence;
    u32 flags;
    u32 header_crc32;
};
```

### CRC范围

所有转储整数都是序列化的little-en。在填充所有非CRC字段之后计算CRC字段。

- `ftl_dump_file_header.header_crc32`完全覆盖`header_size`文件头的字节数`header_crc32`为计算设置为零。
- `ftl_dump_section_header.crc32`仅涵盖部分有效负载字节，而不涵盖部分标头。
- 读取器首先验证文件头，然后是每个节头的边界，然后是有效载荷CRC。

截面类型:

```c
enum ftl_dump_section_type {
    FTL_DUMP_SECTION_MANIFEST = 1,
    FTL_DUMP_SECTION_L2P_FLAT = 2,
    FTL_DUMP_SECTION_MAP_DIRECTORY = 3,
    FTL_DUMP_SECTION_MAP_PAGE = 4,
    FTL_DUMP_SECTION_MAP_LOG = 5,
    FTL_DUMP_SECTION_SB_JOURNAL = 6,
    FTL_DUMP_SECTION_TRIM_BITMAP = 7,
    FTL_DUMP_SECTION_TRIM_RANGE_LOG = 8,
    FTL_DUMP_SECTION_P2L_DEBUG = 9,
    FTL_DUMP_SECTION_SUMMARY = 10,
};
```

转储编写器可能会忽略当前分支中不存在的部分。例如，在新的运行时之前`ftl_map_ctx`土地，`MAP_DIRECTORY`和`MAP_PAGE`可以缺席，而`L2P_FLAT`存在。

---

## 重建语义

重建顺序:

```text
1. Validate header and section CRCs.
2. Load base L2P state from L2P_FLAT or MAP_DIRECTORY + MAP_PAGE sections.
3. Apply ordered map mutation log records newer than base_sequence.
4. Apply persistent journal mirror records newer than base_sequence if present.
5. Apply TRIM bitmap when its epoch equals the base snapshot epoch.
6. Apply TRIM range log records newer than base_sequence.
7. Rebuild P2L from the final L2P image.
8. Compare optional P2L debug dump against rebuilt P2L and report mismatches.
9. Emit a rebuild report with counts, skipped records, conflicts, and CRC status.
```

重要的顺序规则:

```text
TRIM bitmap is a snapshot of unmapped state at bitmap_epoch.
TRIM range log is ordered mutation history after bitmap_epoch.
If a WRITE record has a higher sequence than a TRIM bit/range, WRITE wins.
If a TRIM record has a higher sequence than a WRITE record, TRIM wins.
```

纯修剪位图不足以解决修剪后写入排序。因此，每个位图转储包括:

```c
struct ftl_trim_bitmap_header {
    u64 base_lba;
    u64 lba_count;
    u64 bitmap_epoch;
    u64 bit_count;
    u32 word_bits;
    u32 flags;
};
```

和有序的修剪范围记录在它们存在时发出。

---

## 文件结构

创建：

- `include/ftl/ftl_dump.h`: 公共转储/重建API，文件格式结构，标志
- `src/ftl/ftl_dump.c`: 节写入器/读取器，CRC验证，重建引擎。
- `tests/test_ftl_dump_format.c`: 二进制格式，CRC，段解析测试
- `tests/test_ftl_dump_rebuild.c`: 在合成映射数据上重建语义测试。
- `tests/test_ftl_dump_trim.c`: 修剪位图/范围排序测试。
- `tools/ftl_dump_tool.c`: 用于转储摘要和脱机重建验证的可选本地CLI。

修改：

- `Makefile`: 添加`libhfsss-ftl`源、新测试和可选工具目标。
- `include/ftl/ftl.h`: 仅当从实时直接快照时才包括转储API`ftl_ctx`是有线的。
- `src/ftl/ftl.c`: 在核心API存在后添加非侵入式快照挂钩。
- `src/ftl/superblock.c`注意: 如果无法在不公开内部的情况下复制现有日记帐数据，则仅添加只读日记帐导出帮助程序。

请勿修改：

- 正常呼叫`sb_checkpoint_write_ex()`, `sb_checkpoint_read_ex()`, or `sb_recover_ex()`.
- DSB内部结构。
- 功率损失或清洁功率循环测试，除非断言它们保持不变。

---

## 阶段1: 转储文件格式和部分IO

### 任务1: 添加转储格式测试

**文件：**
- 创建：`tests/test_ftl_dump_format.c`
- 修改：`Makefile`

- [ ] **步骤1: 写入失败的标头验证测试**

创建`tests/test_ftl_dump_format.c`:

```c
#include "ftl/ftl_dump.h"
#include "common/common.h"
#include <assert.h>
#include <string.h>

static void test_header_init_sets_magic_and_version(void)
{
    struct ftl_dump_file_header hdr;
    memset(&hdr, 0, sizeof(hdr));

    assert(ftl_dump_header_init(&hdr, 1024, 4096, 123) == HFSSS_OK);
    assert(hdr.magic == FTL_DUMP_MAGIC);
    assert(hdr.version == FTL_DUMP_VERSION);
    assert(hdr.total_lbas == 1024);
    assert(hdr.page_size == 4096);
    assert(hdr.snapshot_sequence == 123);
}

int main(void)
{
    test_header_init_sets_magic_and_version();
    return 0;
}
```

- [ ] **步骤2: 运行测试以验证失败**

运行：

```bash
make build/bin/test_ftl_dump_format
```

预期: 编译失败，因为`ftl/ftl_dump.h`不存在。

### 任务2: 实现公共转储格式头

**文件：**
- 创建：`include/ftl/ftl_dump.h`
- 创建：`src/ftl/ftl_dump.c`
- 修改：`Makefile`

- [ ] **步骤1: 添加具有稳定结构的标头**

实现从转储格式部分加上结构:

```c
int ftl_dump_header_init(struct ftl_dump_file_header *hdr,
                         u64 total_lbas, u64 page_size,
                         u64 snapshot_sequence);

int ftl_dump_section_crc(const void *data, u64 len, u32 *crc_out);
```

- [ ] **步骤2: 添加最小实现**

In `src/ftl/ftl_dump.c`:

```c
#include "ftl/ftl_dump.h"
#include "common/crc32.h"
#include <string.h>

int ftl_dump_header_init(struct ftl_dump_file_header *hdr,
                         u64 total_lbas, u64 page_size,
                         u64 snapshot_sequence)
{
    if (!hdr || total_lbas == 0 || page_size == 0) {
        return HFSSS_ERR_INVAL;
    }
    memset(hdr, 0, sizeof(*hdr));
    hdr->magic = FTL_DUMP_MAGIC;
    hdr->version = FTL_DUMP_VERSION;
    hdr->header_size = sizeof(*hdr);
    hdr->total_lbas = total_lbas;
    hdr->page_size = page_size;
    hdr->snapshot_sequence = snapshot_sequence;
    return HFSSS_OK;
}
```

如果项目CRC帮助程序公开为`hfsss_crc32`,包括正确的标题和实现:

```c
int ftl_dump_section_crc(const void *data, u64 len, u32 *crc_out)
{
    if (!data || !crc_out) {
        return HFSSS_ERR_INVAL;
    }
    *crc_out = hfsss_crc32(data, len);
    return HFSSS_OK;
}
```

- [ ] **步骤3: 运行格式测试**

运行：

```bash
build/bin/test_ftl_dump_format
```

预期：通过。

- [ ] **步骤4：提交**

```bash
git add include/ftl/ftl_dump.h src/ftl/ftl_dump.c tests/test_ftl_dump_format.c Makefile
git commit -m "feat(ftl): add dump file format primitives"
```

### 任务3: 添加分节编写器/阅读器

**文件：**
- 修改：`include/ftl/ftl_dump.h`
- 修改：`src/ftl/ftl_dump.c`
- 修改：`tests/test_ftl_dump_format.c`

- [ ] **步骤1: 添加失败部分往返测试**

附录：

```c
static void test_section_round_trip_validates_crc(void)
{
    struct ftl_dump_section_header sh;
    const char payload[] = "mapping-section";
    u8 out[256];
    u64 written = 0;

    assert(ftl_dump_write_section_to_mem(out, sizeof(out),
           FTL_DUMP_SECTION_SUMMARY, payload, sizeof(payload),
           10, 20, &sh, &written) == HFSSS_OK);
    assert(sh.type == FTL_DUMP_SECTION_SUMMARY);
    assert(sh.record_count == 1);
    assert(sh.base_sequence == 10);
    assert(sh.end_sequence == 20);
    assert(written == sizeof(sh) + sizeof(payload));
    assert(ftl_dump_validate_section(out, written, &sh) == HFSSS_OK);

    out[sizeof(sh)] ^= 0x1;
    assert(ftl_dump_validate_section(out, written, &sh) != HFSSS_OK);
}
```

- [ ] **步骤2: 实现内存写入器/验证器**

添加api:

```c
int ftl_dump_write_section_to_mem(u8 *dst, u64 dst_len,
                                  enum ftl_dump_section_type type,
                                  const void *payload, u64 payload_len,
                                  u64 base_sequence, u64 end_sequence,
                                  struct ftl_dump_section_header *out_hdr,
                                  u64 *written_out);

int ftl_dump_validate_section(const u8 *src, u64 src_len,
                              struct ftl_dump_section_header *hdr_out);
```

- [ ] **步骤3：运行测试**

运行：

```bash
build/bin/test_ftl_dump_format
```

预期：通过。

- [ ] **步骤4：提交**

```bash
git add include/ftl/ftl_dump.h src/ftl/ftl_dump.c tests/test_ftl_dump_format.c
git commit -m "feat(ftl): add dump section round-trip validation"
```

---

## 阶段2: L2P和多级映射转储

### 任务4: 添加平面L2P转储测试

**文件：**
- 创建：`tests/test_ftl_dump_rebuild.c`
- 修改：`Makefile`

- [ ] **步骤1: 写入失败的平面L2P转储测试**

创建合成条目:

```c
static void test_flat_l2p_dump_round_trip(void)
{
    struct ftl_dump_l2p_record records[2];
    struct ftl_dump_rebuild_ctx rebuild;
    union ppn ppn;

    records[0].lba = 5;
    records[0].ppn_raw = 0x100;
    records[0].sequence = 10;
    records[1].lba = 7;
    records[1].ppn_raw = 0x200;
    records[1].sequence = 11;

    assert(ftl_dump_rebuild_init(&rebuild, 128) == HFSSS_OK);
    assert(ftl_dump_rebuild_apply_l2p_records(&rebuild, records, 2) == HFSSS_OK);
    assert(ftl_dump_rebuild_lookup(&rebuild, 5, &ppn) == HFSSS_OK);
    assert(ppn.raw == 0x100);
    assert(ftl_dump_rebuild_lookup(&rebuild, 7, &ppn) == HFSSS_OK);
    assert(ppn.raw == 0x200);
    ftl_dump_rebuild_cleanup(&rebuild);
}
```

- [ ] **步骤2：运行并验证失败情况**

运行：

```bash
make build/bin/test_ftl_dump_rebuild
```

预期: 缺少重建符号。

### 任务5: 实现平面L2P转储/重建记录

**文件：**
- 修改：`include/ftl/ftl_dump.h`
- 修改：`src/ftl/ftl_dump.c`
- 修改：`tests/test_ftl_dump_rebuild.c`

- [ ] **步骤1: 添加记录类型**

```c
struct ftl_dump_l2p_record {
    u64 lba;
    u64 ppn_raw;
    u64 sequence;
    u32 flags;
    u32 reserved;
};
```

- [ ] **步骤2: 添加重建上下文**

```c
struct ftl_dump_rebuild_entry {
    u64 ppn_raw;
    u64 sequence;
    bool valid;
};

struct ftl_dump_rebuild_ctx {
    struct ftl_dump_rebuild_entry *l2p;
    u64 total_lbas;
    u64 applied_records;
    u64 skipped_records;
    bool initialized;
};
```

- [ ] **步骤3: 实现平面重建api**

```c
int ftl_dump_rebuild_init(struct ftl_dump_rebuild_ctx *ctx, u64 total_lbas);
void ftl_dump_rebuild_cleanup(struct ftl_dump_rebuild_ctx *ctx);
int ftl_dump_rebuild_apply_l2p_records(struct ftl_dump_rebuild_ctx *ctx,
                                       const struct ftl_dump_l2p_record *records,
                                       u64 count);
int ftl_dump_rebuild_lookup(struct ftl_dump_rebuild_ctx *ctx,
                            u64 lba, union ppn *ppn_out);
```

规则：

```text
record with highest sequence wins for each LBA
invalid LBA >= total_lbas is skipped and counted
ppn_raw == 0 is treated as unmapped
```

- [ ] **步骤4: 运行重建测试**

运行：

```bash
build/bin/test_ftl_dump_rebuild
```

预期：通过。

- [ ] **步骤5：提交**

```bash
git add include/ftl/ftl_dump.h src/ftl/ftl_dump.c tests/test_ftl_dump_rebuild.c Makefile
git commit -m "feat(ftl): add flat L2P dump rebuild records"
```

### 任务6: 添加多级映射页面转储

**文件：**
- 修改：`include/ftl/ftl_dump.h`
- 修改：`src/ftl/ftl_dump.c`
- 修改：`tests/test_ftl_dump_rebuild.c`

- [ ] **步骤1: 添加失败的多级地图页面测试**

测试将创建一个目录记录和一个页面记录:

```c
static void test_multilevel_map_page_rebuild(void)
{
    struct ftl_dump_map_dir_record dir;
    struct ftl_dump_map_page_record page;
    struct ftl_dump_rebuild_ctx rebuild;
    union ppn ppn;

    memset(&dir, 0, sizeof(dir));
    memset(&page, 0, sizeof(page));
    dir.map_page_id = 2;
    dir.base_lba = 1024;
    dir.entry_count = 512;
    dir.sequence = 50;

    page.map_page_id = 2;
    page.base_lba = 1024;
    page.entry_count = 512;
    page.entries[3].lba_delta = 3;
    page.entries[3].ppn_raw = 0x345;
    page.entries[3].sequence = 51;
    page.entries[3].state = FTL_DUMP_MAP_PRESENT;

    assert(ftl_dump_rebuild_init(&rebuild, 4096) == HFSSS_OK);
    assert(ftl_dump_rebuild_apply_map_pages(&rebuild, &dir, 1, &page, 1) == HFSSS_OK);
    assert(ftl_dump_rebuild_lookup(&rebuild, 1027, &ppn) == HFSSS_OK);
    assert(ppn.raw == 0x345);
    ftl_dump_rebuild_cleanup(&rebuild);
}
```

- [ ] **步骤2: 添加多级结构**

```c
#define FTL_DUMP_MAP_PAGE_ENTRIES 512

enum ftl_dump_map_entry_state {
    FTL_DUMP_MAP_EMPTY = 0,
    FTL_DUMP_MAP_PRESENT = 1,
    FTL_DUMP_MAP_TRIMMED = 2,
};

struct ftl_dump_map_dir_record {
    u64 map_page_id;
    u64 base_lba;
    u32 entry_count;
    u64 sequence;
};

struct ftl_dump_map_page_entry {
    u16 lba_delta;
    u8 state;
    u8 reserved;
    u64 ppn_raw;
    u64 sequence;
};

struct ftl_dump_map_page_record {
    u64 map_page_id;
    u64 base_lba;
    u32 entry_count;
    struct ftl_dump_map_page_entry entries[FTL_DUMP_MAP_PAGE_ENTRIES];
};
```

序列化规则:

```text
Version 1 writes the full fixed-size ftl_dump_map_page_record, including all 512 entries.
entry_count is the number of meaningful entries starting at entries[0].
entries at indexes entry_count..511 must be zero-filled and ignored by readers.
writers reject entry_count > FTL_DUMP_MAP_PAGE_ENTRIES; readers treat it as corrupt input.
```

- [ ] **步骤3: 实现页面应用程序**

添加：

```c
int ftl_dump_rebuild_apply_map_pages(
    struct ftl_dump_rebuild_ctx *ctx,
    const struct ftl_dump_map_dir_record *dirs,
    u64 dir_count,
    const struct ftl_dump_map_page_record *pages,
    u64 page_count);
```

规则：

```text
directory record must match page.map_page_id
entry LBA is page.base_lba + entry.lba_delta
PRESENT installs mapping
TRIMMED clears mapping
EMPTY does nothing
highest sequence wins
```

- [ ] **步骤4：运行测试**

运行：

```bash
build/bin/test_ftl_dump_rebuild
```

预期：通过。

- [ ] **步骤5：提交**

```bash
git add include/ftl/ftl_dump.h src/ftl/ftl_dump.c tests/test_ftl_dump_rebuild.c
git commit -m "feat(ftl): rebuild from multi-level mapping page dumps"
```

---

## 阶段3: 突变日志和日志回放

### 任务7: 添加映射日志重建语义

**文件：**
- 修改：`include/ftl/ftl_dump.h`
- 修改：`src/ftl/ftl_dump.c`
- 修改：`tests/test_ftl_dump_rebuild.c`

- [ ] **步骤1: 添加失败日志排序测试**

```c
static void test_write_trim_log_sequence_ordering(void)
{
    struct ftl_dump_rebuild_ctx rebuild;
    struct ftl_dump_map_log_record log[3];
    union ppn ppn;

    memset(log, 0, sizeof(log));
    log[0].op = FTL_DUMP_LOG_WRITE; log[0].lba = 9; log[0].new_ppn_raw = 0x900; log[0].sequence = 10;
    log[1].op = FTL_DUMP_LOG_TRIM;  log[1].lba = 9; log[1].sequence = 11;
    log[2].op = FTL_DUMP_LOG_WRITE; log[2].lba = 9; log[2].new_ppn_raw = 0x901; log[2].sequence = 12;

    assert(ftl_dump_rebuild_init(&rebuild, 128) == HFSSS_OK);
    assert(ftl_dump_rebuild_apply_map_log(&rebuild, log, 3) == HFSSS_OK);
    assert(ftl_dump_rebuild_lookup(&rebuild, 9, &ppn) == HFSSS_OK);
    assert(ppn.raw == 0x901);
    ftl_dump_rebuild_cleanup(&rebuild);
}
```

- [ ] **步骤2: 添加日志记录类型**

```c
enum ftl_dump_log_op {
    FTL_DUMP_LOG_WRITE = 1,
    FTL_DUMP_LOG_TRIM = 2,
    FTL_DUMP_LOG_GC_REMAP = 3,
    FTL_DUMP_LOG_GC_SKIP = 4,
};

struct ftl_dump_map_log_record {
    u64 sequence;
    enum ftl_dump_log_op op;
    u64 lba;
    u32 count;
    u64 old_ppn_raw;
    u64 new_ppn_raw;
    u32 stream_id;
    u64 dsb_id;
};
```

- [ ] **步骤3: 实现重播**

添加：

```c
int ftl_dump_rebuild_apply_map_log(struct ftl_dump_rebuild_ctx *ctx,
                                   const struct ftl_dump_map_log_record *records,
                                   u64 count);
```

规则：

```text
WRITE installs new_ppn_raw for lba
TRIM clears [lba, lba + count)
GC_REMAP installs new_ppn_raw only if current mapping equals old_ppn_raw or current mapping is absent and force flag is set
GC_SKIP does not install mapping and increments skipped_records
record with sequence <= current entry sequence is ignored
```

- [ ] **步骤4：运行测试**

运行：

```bash
build/bin/test_ftl_dump_rebuild
```

预期：通过。

- [ ] **步骤5：提交**

```bash
git add include/ftl/ftl_dump.h src/ftl/ftl_dump.c tests/test_ftl_dump_rebuild.c
git commit -m "feat(ftl): replay mapping mutation logs during dump rebuild"
```

### 任务8: 添加持久性日志镜像重放

**文件：**
- 修改：`include/ftl/ftl_dump.h`
- 修改：`src/ftl/ftl_dump.c`
- 修改：`src/ftl/superblock.c`如果需要只读导出帮助程序。
- 修改：`tests/test_ftl_dump_rebuild.c`

- [ ] **步骤1: 添加失败的日志镜像测试**

创建镜像现有超级块日志语义的记录:

```c
static void test_sb_journal_mirror_replay(void)
{
    struct ftl_dump_rebuild_ctx rebuild;
    struct ftl_dump_sb_journal_record jrnl[2];
    union ppn ppn;

    memset(jrnl, 0, sizeof(jrnl));
    jrnl[0].sequence = 20;
    jrnl[0].op = FTL_DUMP_JOURNAL_WRITE;
    jrnl[0].lba = 33;
    jrnl[0].ppn_raw = 0x3300;
    jrnl[0].length = 1;

    jrnl[1].sequence = 21;
    jrnl[1].op = FTL_DUMP_JOURNAL_TRIM_RANGE;
    jrnl[1].lba = 33;
    jrnl[1].length = 1;

    assert(ftl_dump_rebuild_init(&rebuild, 128) == HFSSS_OK);
    assert(ftl_dump_rebuild_apply_sb_journal(&rebuild, jrnl, 2) == HFSSS_OK);
    assert(ftl_dump_rebuild_lookup(&rebuild, 33, &ppn) == HFSSS_ERR_NOENT);
    ftl_dump_rebuild_cleanup(&rebuild);
}
```

- [ ] **步骤2: 添加日记帐镜像记录类型**

```c
enum ftl_dump_journal_op {
    FTL_DUMP_JOURNAL_WRITE = 1,
    FTL_DUMP_JOURNAL_TRIM = 2,
    FTL_DUMP_JOURNAL_TRIM_RANGE = 3,
};

struct ftl_dump_sb_journal_record {
    u64 sequence;
    enum ftl_dump_journal_op op;
    u64 lba;
    u64 ppn_raw;
    u32 length;
    u32 flags;
};
```

- [ ] **步骤3: 实施日记帐重放**

添加：

```c
int ftl_dump_rebuild_apply_sb_journal(
    struct ftl_dump_rebuild_ctx *ctx,
    const struct ftl_dump_sb_journal_record *records,
    u64 count);
```

映射现有日记帐操作:

```text
JRNL_OP_WRITE      -> FTL_DUMP_JOURNAL_WRITE
JRNL_OP_TRIM       -> FTL_DUMP_JOURNAL_TRIM
JRNL_OP_TRIM_RANGE -> FTL_DUMP_JOURNAL_TRIM_RANGE
```

- [ ] **步骤4: 仅在需要时添加只读导出器**

如果日记帐内部无法从`superblock_ctx`,添加：

```c
int sb_journal_export_records(struct superblock_ctx *sb,
                              struct ftl_dump_sb_journal_record *out,
                              u64 max_records,
                              u64 *count_out);
```

此函数是只读的，不能变异`sb->current_page`, `sb->buf_used`或日记重播状态。

- [ ] **步骤5：运行测试**

运行：

```bash
build/bin/test_ftl_dump_rebuild
build/bin/test_superblock
```

预期：通过。

- [ ] **步骤6：提交**

```bash
git add include/ftl/ftl_dump.h src/ftl/ftl_dump.c src/ftl/superblock.c tests/test_ftl_dump_rebuild.c
git commit -m "feat(ftl): replay journal mirror records during dump rebuild"
```

---

## 阶段4: 修剪位图和范围转储

### 任务9: 添加修剪位图测试

**文件：**
- 创建：`tests/test_ftl_dump_trim.c`
- 修改：`Makefile`

- [ ] **步骤1: 写入失败的trim位图epoch测试**

```c
#include "ftl/ftl_dump.h"
#include <assert.h>
#include <string.h>

static void test_trim_bitmap_clears_only_at_bitmap_epoch(void)
{
    struct ftl_dump_rebuild_ctx rebuild;
    struct ftl_dump_trim_bitmap_header hdr;
    u64 bitmap_words[1];
    union ppn ppn;

    memset(&hdr, 0, sizeof(hdr));
    bitmap_words[0] = 1ULL << 4;
    hdr.base_lba = 0;
    hdr.lba_count = 64;
    hdr.bitmap_epoch = 100;
    hdr.bit_count = 64;
    hdr.word_bits = 64;

    assert(ftl_dump_rebuild_init(&rebuild, 128) == HFSSS_OK);
    assert(ftl_dump_rebuild_force_set(&rebuild, 4, 0x444, 90) == HFSSS_OK);
    assert(ftl_dump_rebuild_apply_trim_bitmap(&rebuild, &hdr, bitmap_words, 1) == HFSSS_OK);
    assert(ftl_dump_rebuild_lookup(&rebuild, 4, &ppn) == HFSSS_ERR_NOENT);

    assert(ftl_dump_rebuild_force_set(&rebuild, 4, 0x445, 101) == HFSSS_OK);
    assert(ftl_dump_rebuild_apply_trim_bitmap(&rebuild, &hdr, bitmap_words, 1) == HFSSS_OK);
    assert(ftl_dump_rebuild_lookup(&rebuild, 4, &ppn) == HFSSS_OK);
    assert(ppn.raw == 0x445);
    ftl_dump_rebuild_cleanup(&rebuild);
}

int main(void)
{
    test_trim_bitmap_clears_only_at_bitmap_epoch();
    return 0;
}
```

- [ ] **步骤2：运行并验证失败情况**

运行：

```bash
make build/bin/test_ftl_dump_trim
```

预期: 缺少修剪位图api。

### 任务10: 实现trim位图重播

**文件：**
- 修改：`include/ftl/ftl_dump.h`
- 修改：`src/ftl/ftl_dump.c`
- 修改：`tests/test_ftl_dump_trim.c`

- [ ] **步骤1: 添加trim位图结构**

```c
struct ftl_dump_trim_bitmap_header {
    u64 base_lba;
    u64 lba_count;
    u64 bitmap_epoch;
    u64 bit_count;
    u32 word_bits;
    u32 flags;
};
```

- [ ] **步骤2: 添加帮助程序api**

```c
int ftl_dump_rebuild_force_set(struct ftl_dump_rebuild_ctx *ctx,
                               u64 lba, u64 ppn_raw, u64 sequence);

int ftl_dump_rebuild_apply_trim_bitmap(
    struct ftl_dump_rebuild_ctx *ctx,
    const struct ftl_dump_trim_bitmap_header *hdr,
    const u64 *bitmap_words,
    u64 word_count);
```

- [ ] **步骤3: 实现序列感知清除**

规则：

```text
if bit is set and entry.sequence <= bitmap_epoch, clear entry
if bit is set and entry.sequence > bitmap_epoch, keep entry
if bit is clear, do nothing
out-of-range bits are ignored and counted as skipped
```

- [ ] **步骤4：运行测试**

运行：

```bash
build/bin/test_ftl_dump_trim
build/bin/test_ftl_dump_rebuild
```

预期：通过。

- [ ] **步骤5：提交**

```bash
git add include/ftl/ftl_dump.h src/ftl/ftl_dump.c tests/test_ftl_dump_trim.c Makefile
git commit -m "feat(ftl): rebuild mapping state from trim bitmap dumps"
```

### 任务11: 添加修剪范围转储重放

**文件：**
- 修改：`include/ftl/ftl_dump.h`
- 修改：`src/ftl/ftl_dump.c`
- 修改：`tests/test_ftl_dump_trim.c`

- [ ] **步骤1: 添加不合格的修整范围订购测试**

```c
static void test_trim_range_sequence_vs_write_sequence(void)
{
    struct ftl_dump_rebuild_ctx rebuild;
    struct ftl_dump_trim_range_record tr;
    union ppn ppn;

    memset(&tr, 0, sizeof(tr));
    tr.start_lba = 10;
    tr.length = 8;
    tr.sequence = 200;

    assert(ftl_dump_rebuild_init(&rebuild, 128) == HFSSS_OK);
    assert(ftl_dump_rebuild_force_set(&rebuild, 12, 0x1200, 199) == HFSSS_OK);
    assert(ftl_dump_rebuild_force_set(&rebuild, 13, 0x1300, 201) == HFSSS_OK);
    assert(ftl_dump_rebuild_apply_trim_ranges(&rebuild, &tr, 1) == HFSSS_OK);
    assert(ftl_dump_rebuild_lookup(&rebuild, 12, &ppn) == HFSSS_ERR_NOENT);
    assert(ftl_dump_rebuild_lookup(&rebuild, 13, &ppn) == HFSSS_OK);
    assert(ppn.raw == 0x1300);
    ftl_dump_rebuild_cleanup(&rebuild);
}
```

- [ ] **步骤2: 添加记录和重放API**

```c
struct ftl_dump_trim_range_record {
    u64 sequence;
    u64 start_lba;
    u64 length;
    u32 flags;
};

int ftl_dump_rebuild_apply_trim_ranges(
    struct ftl_dump_rebuild_ctx *ctx,
    const struct ftl_dump_trim_range_record *records,
    u64 count);
```

- [ ] **步骤3：实现回放**

对于每个LBA`[start_lba, start_lba + length)`:

```text
clear only if record.sequence >= current entry.sequence
```

- [ ] **步骤4：运行测试**

运行：

```bash
build/bin/test_ftl_dump_trim
```

预期：通过。

- [ ] **步骤5：提交**

```bash
git add include/ftl/ftl_dump.h src/ftl/ftl_dump.c tests/test_ftl_dump_trim.c
git commit -m "feat(ftl): replay trim range dumps during rebuild"
```

---

## 阶段5: P2L派生的重建和调试比较

### 任务12: 从重建的L2P重建P2L

**文件：**
- 修改：`include/ftl/ftl_dump.h`
- 修改：`src/ftl/ftl_dump.c`
- 修改：`tests/test_ftl_dump_rebuild.c`

- [ ] **步骤1: 添加失败的P2L重建测试**

```c
static void test_p2l_rebuild_from_final_l2p(void)
{
    struct ftl_dump_rebuild_ctx rebuild;
    u64 lba;

    assert(ftl_dump_rebuild_init(&rebuild, 128) == HFSSS_OK);
    assert(ftl_dump_rebuild_force_set(&rebuild, 40, 0x4000, 10) == HFSSS_OK);
    assert(ftl_dump_rebuild_force_set(&rebuild, 41, 0x4001, 11) == HFSSS_OK);
    assert(ftl_dump_rebuild_p2l(&rebuild) == HFSSS_OK);
    assert(ftl_dump_rebuild_p2l_lookup(&rebuild, 0x4000, &lba) == HFSSS_OK);
    assert(lba == 40);
    assert(ftl_dump_rebuild_p2l_lookup(&rebuild, 0x4001, &lba) == HFSSS_OK);
    assert(lba == 41);
    ftl_dump_rebuild_cleanup(&rebuild);
}
```

- [ ] **步骤2: 添加P2L派生结构**

```c
struct ftl_dump_p2l_entry {
    u64 ppn_raw;
    u64 lba;
    bool valid;
};
```

添加到重建上下文:

```c
struct ftl_dump_p2l_entry *p2l;
u64 p2l_capacity;
u64 p2l_collisions;
```

- [ ] **步骤3: 实现派生P2L**

使用开放式寻址。重建扫描所有有效的L2P条目和插入`ppn_raw -> lba`.

- [ ] **步骤4：运行测试**

运行：

```bash
build/bin/test_ftl_dump_rebuild
```

预期：通过。

- [ ] **步骤5：提交**

```bash
git add include/ftl/ftl_dump.h src/ftl/ftl_dump.c tests/test_ftl_dump_rebuild.c
git commit -m "feat(ftl): rebuild derived P2L from dump L2P state"
```

### 任务13: 添加可选P2L调试转储比较

**文件：**
- 修改：`include/ftl/ftl_dump.h`
- 修改：`src/ftl/ftl_dump.c`
- 修改：`tests/test_ftl_dump_rebuild.c`

- [ ] **步骤1: 添加失败的不匹配报告测试**

```c
static void test_p2l_debug_mismatch_is_reported_not_applied(void)
{
    struct ftl_dump_rebuild_ctx rebuild;
    struct ftl_dump_p2l_debug_record dbg;
    struct ftl_dump_rebuild_report report;
    u64 lba;

    memset(&dbg, 0, sizeof(dbg));
    dbg.ppn_raw = 0x5000;
    dbg.lba = 99;

    assert(ftl_dump_rebuild_init(&rebuild, 128) == HFSSS_OK);
    assert(ftl_dump_rebuild_force_set(&rebuild, 50, 0x5000, 10) == HFSSS_OK);
    assert(ftl_dump_rebuild_p2l(&rebuild) == HFSSS_OK);
    assert(ftl_dump_rebuild_compare_p2l_debug(&rebuild, &dbg, 1, &report) == HFSSS_OK);
    assert(report.p2l_debug_mismatches == 1);
    assert(ftl_dump_rebuild_p2l_lookup(&rebuild, 0x5000, &lba) == HFSSS_OK);
    assert(lba == 50);
    ftl_dump_rebuild_cleanup(&rebuild);
}
```

- [ ] **步骤2: 添加调试记录和报告**

```c
struct ftl_dump_p2l_debug_record {
    u64 ppn_raw;
    u64 lba;
    u64 sequence;
    u32 flags;
};

struct ftl_dump_rebuild_report {
    u64 l2p_valid;
    u64 l2p_trimmed;
    u64 applied_records;
    u64 skipped_records;
    u64 p2l_entries;
    u64 p2l_collisions;
    u64 p2l_debug_mismatches;
    u64 crc_failures;
};
```

- [ ] **步骤3: 实施比较**

P2L调试转储从不更改重建的L2P/P2L。它只增加不匹配计数器。

- [ ] **步骤4：运行测试**

运行：

```bash
build/bin/test_ftl_dump_rebuild
```

预期：通过。

- [ ] **步骤5：提交**

```bash
git add include/ftl/ftl_dump.h src/ftl/ftl_dump.c tests/test_ftl_dump_rebuild.c
git commit -m "feat(ftl): compare optional P2L debug dump during rebuild"
```

---

## 阶段6: 实时FTL快照导出

### 任务14: 添加实时快照API

**文件：**
- 修改：`include/ftl/ftl_dump.h`
- 修改：`include/ftl/ftl.h`
- 修改：`src/ftl/ftl.c`
- 修改：`src/ftl/ftl_dump.c`
- 创建：`tests/test_ftl_dump_live.c`
- 修改：`Makefile`

- [ ] **步骤1: 写入失败的实时转储冒烟测试**

该测试初始化一个小的FTL，写入两页，trims一页，并转储到内存:

```c
static void test_live_dump_contains_write_and_trim_state(void)
{
    struct ftl_dump_buffer buf;
    struct ftl_dump_rebuild_ctx rebuild;
    union ppn ppn;

    setup_small_ftl_with_one_live_and_one_trimmed_lba();
    assert(ftl_dump_live_to_buffer(&ftl, FTL_DUMP_INCLUDE_L2P |
                                        FTL_DUMP_INCLUDE_TRIM,
                                   &buf) == HFSSS_OK);
    assert(ftl_dump_rebuild_from_buffer(&rebuild, buf.data, buf.len) == HFSSS_OK);
    assert(ftl_dump_rebuild_lookup(&rebuild, live_lba, &ppn) == HFSSS_OK);
    assert(ftl_dump_rebuild_lookup(&rebuild, trimmed_lba, &ppn) == HFSSS_ERR_NOENT);
}
```

- [ ] **步骤2: 添加请求和缓冲区类型**

```c
enum ftl_dump_flags {
    FTL_DUMP_INCLUDE_L2P = 1u << 0,
    FTL_DUMP_INCLUDE_MULTI_LEVEL_MAP = 1u << 1,
    FTL_DUMP_INCLUDE_MAP_LOG = 1u << 2,
    FTL_DUMP_INCLUDE_SB_JOURNAL = 1u << 3,
    FTL_DUMP_INCLUDE_TRIM = 1u << 4,
    FTL_DUMP_INCLUDE_P2L_DEBUG = 1u << 5,
};

struct ftl_dump_buffer {
    u8 *data;
    u64 len;
    u64 cap;
};

int ftl_dump_live_to_buffer(struct ftl_ctx *ctx, u32 flags,
                            struct ftl_dump_buffer *out);
void ftl_dump_buffer_cleanup(struct ftl_dump_buffer *buf);
```

- [ ] **步骤3: 快照一致性规则**

对于当前遗留映射:

```text
take ctx->lock
copy bounded snapshot metadata and stable map/journal references
release ctx->lock
```

锁是元数据冻结，而不是序列化窗口。实施不能成立`ctx->lock`同时写入大型L2P映像的最终转储缓冲区。在锁下，仅复制有界元数据或捕获一致快照所需的稳定页面指针/引用; 在CRC计算、节编码、文件IO或任何O (total_lbas) 序列化。如果平面遗留映射需要锁定下的深层副本，则将该路径选通到调试或小容量配置并返回`HFSSS_ERR_NOTSUP`对于大型配置，直到页面/指针快照路径存在。

对于未来的单变更工作线程地图:

```text
enqueue dump barrier to mutation worker
worker returns snapshot descriptor at one sequence
```

突变工作者屏障遵循相同的规则: 它返回从有界临界区组装的快照描述符或有界缓冲区。重序列化在突变工作线程之外运行。

此阶段实现了遗留锁路径，并将突变-工作者屏障路径留在了返回`HFSSS_ERR_NOTSUP`直到单个FTL重构着陆。

- [ ] **步骤4：运行测试**

运行：

```bash
build/bin/test_ftl_dump_live
build/bin/test_ftl_dump_rebuild
make test
```

预期：通过。

- [ ] **步骤5：提交**

```bash
git add include/ftl/ftl_dump.h include/ftl/ftl.h src/ftl/ftl.c src/ftl/ftl_dump.c tests/test_ftl_dump_live.c Makefile
git commit -m "feat(ftl): add live FTL dump snapshot API"
```

---

## 阶段7: 离线工具和报告

### 任务15: 添加本地转储工具

**文件：**
- 创建：`tools/ftl_dump_tool.c`
- 修改：`Makefile`
- 创建：`tests/test_ftl_dump_tool.sh`

- [ ] **步骤1: 添加CLI合同**

CLI形状:

```bash
build/bin/ftl-dump-tool inspect dump.ftld
build/bin/ftl-dump-tool rebuild dump.ftld --summary /tmp/rebuild.json
build/bin/ftl-dump-tool compare-p2l dump.ftld --summary /tmp/p2l.json
```

- [ ] **步骤2: 编写shell烟雾测试**

`tests/test_ftl_dump_tool.sh`:

```bash
#!/usr/bin/env bash
set -euo pipefail

BIN="${1:-build/bin/ftl-dump-tool}"
TMP="${TMPDIR:-/tmp}/hfsss-ftl-dump-tool.$$"
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT

"$BIN" --help > "$TMP/help.txt"
grep -q "inspect" "$TMP/help.txt"
grep -q "rebuild" "$TMP/help.txt"
```

- [ ] **步骤3: 实现CLI框架**

`--help`打印命令并退出0。未知命令退出非零。

- [ ] **步骤4: 电线检查/重建**

`inspect`验证截面并打印:

```text
magic
version
section_count
snapshot_sequence
section type/count/crc status
```

`rebuild`运行重建引擎并写入JSON摘要。

- [ ] **步骤5：运行测试**

运行：

```bash
make build/bin/ftl-dump-tool
bash tests/test_ftl_dump_tool.sh build/bin/ftl-dump-tool
```

预期：通过。

- [ ] **步骤6：提交**

```bash
git add tools/ftl_dump_tool.c tests/test_ftl_dump_tool.sh Makefile
git commit -m "feat(tools): add FTL dump inspect and rebuild tool"
```

### 任务16: 添加重新生成报表JSON

**文件：**
- 修改：`include/ftl/ftl_dump.h`
- 修改：`src/ftl/ftl_dump.c`
- 修改：`tools/ftl_dump_tool.c`
- 修改：`tests/test_ftl_dump_tool.sh`

- [ ] **步骤1: 添加报告JSON测试**

shell测试通过测试帮助器或捆绑生成的文件创建一个小的转储fixture并运行:

```bash
"$BIN" rebuild "$TMP/sample.ftld" --summary "$TMP/rebuild.json"
grep -q '"l2p_valid"' "$TMP/rebuild.json"
grep -q '"p2l_debug_mismatches"' "$TMP/rebuild.json"
grep -q '"crc_failures"' "$TMP/rebuild.json"
```

- [ ] **步骤2: 实现JSON writer**

添加：

```c
int ftl_dump_rebuild_report_json(const struct ftl_dump_rebuild_report *report,
                                 char *buf, u64 buf_len);
```

输出键:

```json
{
  "l2p_valid": 0,
  "l2p_trimmed": 0,
  "applied_records": 0,
  "skipped_records": 0,
  "p2l_entries": 0,
  "p2l_collisions": 0,
  "p2l_debug_mismatches": 0,
  "crc_failures": 0
}
```

- [ ] **步骤3：运行测试**

运行：

```bash
bash tests/test_ftl_dump_tool.sh build/bin/ftl-dump-tool
build/bin/test_ftl_dump_rebuild
```

预期：通过。

- [ ] **步骤4：提交**

```bash
git add include/ftl/ftl_dump.h src/ftl/ftl_dump.c tools/ftl_dump_tool.c tests/test_ftl_dump_tool.sh
git commit -m "feat(ftl): emit JSON rebuild reports for dump analysis"
```

---

## 阶段8: 最终验证

### 任务17: 全面验证和覆盖

**文件：**
- 除非验证找到间隙，否则不需要更改源。

- [ ] **步骤1: 运行目标转储测试**

运行：

```bash
build/bin/test_ftl_dump_format
build/bin/test_ftl_dump_rebuild
build/bin/test_ftl_dump_trim
build/bin/test_ftl_dump_live
bash tests/test_ftl_dump_tool.sh build/bin/ftl-dump-tool
```

预期：所有退出状态均为0。

- [ ] **步骤2: 运行FTL回归测试**

运行：

```bash
build/bin/test_ftl
build/bin/test_superblock
build/bin/test_data_superblock
build/bin/test_gc_mt
build/bin/test_power_cycle
```

预期：所有退出状态均为0。`test_power_cycle`不得要求任何新的转储行为。

- [ ] **步骤3: 运行完整套件**

运行：

```bash
make test
```

预期：退出代码为0。

- [ ] **步骤4: 运行覆盖率**

运行：

```bash
make coverage-clean
make coverage-ut
bash scripts/coverage/ratchet_check.sh
```

期望的：

```text
coverage-ut exits 0
ratchet check passes
new ftl_dump module has targeted line/branch coverage
overall line coverage stays at or above current project target
```

- [ ] **步骤5: 运行空白检查**

运行：

```bash
git diff --check
```

预期：无输出，退出码为0。

- [ ] **步骤6: 提交仅验证测试更改 (如果有)**

```bash
git add tests Makefile
git commit -m "test(ftl): cover dump rebuild diagnostics"
```

---

## 完成条件

该功能在以下情况下完成:

1. 转储文件格式进行版本控制和CRC验证。
2. 实现了平面L2P转储和重建。
3. 实现了多级映射目录/页面转储和重建。
4. 实现了有序映射突变日志回放。
5. 实现了持久性日志镜像重放。
6. 修剪位图重播是序列感知的。
7. TRIM range replay解决了trim后写入和trim后写入顺序。
8. P2L是从最终的L2P状态重建的。
9. 可选P2L调试转储进行比较和报告，从不作为权威应用。
10. 实时转储API可以捕获遗留映射状态，而无需更改正常检查点/恢复。
11. 本地CLI可以检查和重建转储文件。
12. Rebuild报告显示有效映射、跳过的记录、CRC失败、P2L冲突和P2L调试不匹配。
13. 正常电源路径保持不变。
14. `make test`和覆盖棘轮通过。

---

## 开放式后续设计

这些故意不属于该计划的一部分:

1. 从FTL转储文件启动时恢复。
2. UPLP紧急冲洗集成。
3. NAND OOB全介质扫描重建。
4. 用于转储/重建时间线的可视化仪表板。
5. 跨版本转储迁移超出版本1。
