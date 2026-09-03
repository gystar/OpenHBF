# OpenHBX

OpenHBX 是面向 OCP High Bandwidth Flash（HBF）的确定性模拟器。当前仓库实现 `OCP_HBF_0_7` 产品路径。

设计说明见 [`docs/README.md`](docs/README.md)，正式实验规则见 [`docs/实验与测试执行规范.md`](docs/实验与测试执行规范.md)。

## 1. 使用 Conda 在宿主机构建和执行

所有命令均从仓库根目录执行。LLMCompass 工作区已提供 Conda 环境：

```bash
conda activate /home/xuzihan/LLMCompass_3D_NMP/.conda-env
```

该环境需要提供 CMake、C++17 编译器、Make 和 Python。可用下列命令检查：

```bash
cmake --version
c++ --version
make --version
python --version
```

宿主机和 Docker 共用 `build/tests/` 作为构建目录。首次从 Docker 切换到 Conda，或反向切换时，必须使用 `--fresh`覆盖 CMake cache 中记录的源码绝对路径：

```bash
cmake --fresh -S . -B build/tests \
  -DOPENHBF_BUILD_TESTS=ON \
  -DOPENHBF_WITH_RAMULATOR2=OFF \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build/tests -j2
```

`--fresh` 只重建 `build/tests` 的 CMake cache，不会删除 `build/artifacts/` 中的实验证据。之后只要环境和源码路径未变，可去掉 `--fresh` 进行增量构建。

运行全部测试：

```bash
ctest --test-dir build/tests --output-on-failure
```

运行 HBF 读带宽实验：

```bash
ctest --test-dir build/tests \
  -R '^openhbx_hbf_max_read_bandwidth_test$' \
  --verbose
```

该测试覆盖16个Host Channel，每Channel拥有256个可并发Bank，共向4096个Bank各发出一个4 KiB读请求。运行时每2000个simulation cycles自动输出一次区间读写字节数和GB/s，无需从最终结果手工换算瞬时带宽。Conda是本项目推荐的默认构建与执行方式，也可按[`docs/实验与测试执行规范.md`](docs/实验与测试执行规范.md)形成正式E3/E4证据。

构建产物统一放在 `build/tests/`，不会写入测试源码目录 `tests/`。`build-host/` 不再作为常规构建目录。

## 2. 使用 Docker 隔离构建（可选复现环境）

需要固定容器工具链时，首次构建开发镜像：

```bash
docker compose up -d --build dev
```

以后只需启动已有容器：

```bash
docker compose up -d dev
```

### 2.1 直接使用 Docker Compose

这种方法不进入容器，适合日常开发和自动化。

#### 2.1.1 配置和编译

```bash
docker compose exec -T dev cmake --fresh -S . -B build/tests \
  -DOPENHBF_BUILD_TESTS=ON \
  -DOPENHBF_WITH_RAMULATOR2=OFF \
  -DCMAKE_BUILD_TYPE=Debug

docker compose exec -T dev cmake --build build/tests -j2
```

#### 2.1.2 运行全部 HBF 读带宽实验

```bash
docker compose exec -T dev ctest \
  --test-dir build/tests \
  -R '^openhbx_hbf_max_read_bandwidth_test$' \
  --verbose
```

`--verbose`用于显示实验成功时的吞吐、延迟和检查结果；不加该参数时，CTest通常只显示通过或失败。

#### 2.1.3 运行全部测试

```bash
docker compose exec -T dev ctest \
  --test-dir build/tests \
  --output-on-failure
```

### 2.2 进入 Docker 后使用 CMake 和 Make

这种方法适合需要连续编译、运行和调试的开发过程。

#### 2.2.1 进入容器

```bash
docker compose exec dev bash
```

#### 2.2.2 配置和编译

```bash
mkdir -p build
cd build

cmake --fresh .. -B tests \
  -DOPENHBF_BUILD_TESTS=ON \
  -DOPENHBF_WITH_RAMULATOR2=OFF \
  -DCMAKE_BUILD_TYPE=Debug

cd tests
make -j2
```

`cmake .. -B tests`表示：

- `..`：源码位于当前 `build/` 的上一级；
- `-B tests`：生成物写入 `build/tests/`；
- `make -j2`：使用两个并行任务编译。

#### 2.2.3 运行全部 HBF 读带宽实验

此时位于 `build/tests/`：

```bash
ctest -R '^openhbx_hbf_max_read_bandwidth_test$' --verbose
```

也可以直接运行实验程序：

```bash
./tests/performance/openhbx_hbf_max_read_bandwidth_test
```

正式验证推荐使用 CTest；直接运行二进制主要用于调试。

## 4. 如何理解读带宽实验结果

当前 `openhbx_hbf_max_read_bandwidth_test` 的核心输出如下：

```text
experiment:
  id:                         EXP-PERF-HBF-MAX-READ
  evidence:                   E4-model-resource candidate

measurement:
  requests:                   4096 reads
  transferred:                16.000 MiB
  window:                     8884 cycles (8884.000 ns)
  throughput:                 1888.475 GB/s (1.888 TB/s)
  raw_link_ceiling:           4096.000 GB/s (4.096 TB/s)
  raw_link_utilization:       46.105 %
  ocp_user_target:            3072.000 GB/s (3.072 TB/s)
  ocp_target_achievement:     61.474 %

checks:
  completed_requests:         PASS (4096/4096)
  completed_bytes:            PASS (16777216 B)
  payload_integrity:          PASS
  outstanding_after_drain:    PASS (0)
  failures:                   PASS (0)
  observed_le_raw_ceiling:    PASS

result: PASS
```

结果说明：当前synthetic拓扑为16 Core Die、每Core Die 16 Die、每Die 16 Bank。16个Host Channel均匀拥有全部4096个物理Bank，即每Channel 256 Bank。实验向每个Bank并发发出一个4 KiB读请求，共完成4096个请求并传输16 MiB；2026-09-03测量窗口为8884个模拟周期，得到1.888 TB/s。该结果低于模型的4.096 TB/s raw link上限，达到3.072 TB/s OCP用户带宽参考值的61.474%。

测量期间还会自动输出周期日志，例如：

```text
[OpenHBX][bandwidth] cycles=105357-107357 interval_cycles=2000 read_bytes=6819840 read_GBps=3409.920 ...
[OpenHBX][bandwidth] cycles=107357-109357 interval_cycles=2000 read_bytes=6901760 read_GBps=3450.880 ...
```

周期日志表示相邻2000-cycle窗口内实际完成的字节和带宽；最终`measurement`表示从开始注入到全部完成的批次平均带宽，两者口径不同。

这里的1.888 TB/s是当前配置和有限批次下的 **transaction-level模拟结果**。模型将OCP目标作为报告参考，尚未覆盖所有真实协议和芯片开销，因此不能把它解释为硅片保证值。

CTest还会把机器可读结果保存到 `build/artifacts/performance/EXP-PERF-HBF-MAX-READ/ctest/bandwidth.json`。

## 5. 使用短命令运行测试

根目录 `Makefile` 对 Docker、CMake 和 CTest 做了简单封装。它没有建立另一套测试系统。

### 5.1 查看所有测试

```bash
make list-tests
```

当前会列出21项测试及其精确名称。

### 5.2 运行一个测试

```bash
make test-one TEST=openhbx_hbf_max_read_bandwidth_test
```

将 `TEST` 替换为 `make list-tests` 显示的任意名称即可。单项测试默认使用verbose输出，因此能看到成功实验的具体结果。

### 5.3 常用测试组

```bash
make test                  # 全部测试
make test-system           # 系统测试
make test-conformance      # OCP和证据一致性测试
make test-performance      # 所有性能测试
```

主要单项测试：

| 测试目的 | 测试名称 |
|---|---|
| EventQueue、completion和日志 | `openhbx_system_event_kernel_test` |
| 完整系统生命周期 | `openhbx_s8_system_test` |
| OCP transaction vectors | `openhbx_s9_ocp_transaction_vectors` |
| 单endpoint Flash读带宽 | `openhbx_flash_read_bandwidth_test` |
| 全部HBF读带宽 | `openhbx_hbf_max_read_bandwidth_test` |

## 6. 运行 Trace 模拟器

从仓库根目录执行：

```bash
make run
```

等价的直接命令是在 `build/tests/` 中执行：

```bash
./openhbx_trace_runner \
  ../../configs/products/ocp_hbf_0_7.yaml \
  ../../tests/traces/write_read.trace
```

一次完整运行的摘要示例：

```text
config_hash=openhbx-fnv1a64-v1:74b02c1c73517cc2 accepted=65 completed=65 cycles=99
```

`accepted=completed=65`说明所有已接受请求都产生了终态，没有遗留请求；`cycles=99`是本次工作负载使用的模拟周期数；`config_hash`与最大带宽实验一致，说明两者读取了同一个产品配置。

## 7. 日志与输出目录

OpenHBX使用结构化`ObservedEvent`。文本和JSONL日志用于诊断，不作为正确性判据。详细说明见 [`docs/OPENHBX_LOGGING_AND_OBSERVABILITY.md`](docs/OPENHBX_LOGGING_AND_OBSERVABILITY.md)。

| 内容 | 目录 |
|---|---|
| CMake构建文件 | `build/tests/` |
| 正式实验结果 | `build/artifacts/<suite>/<experiment-id>/<run-id>/` |
| 测试源码 | `tests/` |

不要在仓库根目录、`src/`、`include/`、`tests/`、`docs/`或`configs/`中生成可执行文件、目标文件、日志或实验数据。
