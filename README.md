# OpenHBX

OpenHBX 是面向 OCP High Bandwidth Flash（HBF）的确定性模拟器。当前仓库实现 `OCP_HBF_0_7` 产品路径。

设计说明见 [`docs/README.md`](docs/README.md)，正式实验规则见 [`docs/实验与测试执行规范.md`](docs/实验与测试执行规范.md)。

## 1. 准备 Docker 环境

所有命令均从仓库根目录执行。首次使用时构建开发镜像：

```bash
docker compose up -d --build dev
```

以后只需启动已有容器：

```bash
docker compose up -d dev
```

构建产物统一放在 `build/tests/`，不会写入测试源码目录 `tests/`。

## 2. 方法一：直接使用 Docker Compose

这种方法不进入容器，适合日常开发和自动化。

### 2.1 配置和编译

```bash
docker compose exec -T dev cmake -S . -B build/tests \
  -DOPENHBF_BUILD_TESTS=ON \
  -DOPENHBF_WITH_RAMULATOR2=OFF \
  -DCMAKE_BUILD_TYPE=Debug

docker compose exec -T dev cmake --build build/tests -j2
```

### 2.2 运行全部 HBF 读带宽实验

```bash
docker compose exec -T dev ctest \
  --test-dir build/tests \
  -R '^openhbx_hbf_max_read_bandwidth_test$' \
  --verbose
```

`--verbose`用于显示实验成功时的吞吐、延迟和检查结果；不加该参数时，CTest通常只显示通过或失败。

### 2.3 运行全部测试

```bash
docker compose exec -T dev ctest \
  --test-dir build/tests \
  --output-on-failure
```

## 3. 方法二：进入 Docker 后使用 CMake 和 Make

这种方法适合需要连续编译、运行和调试的开发过程。

### 3.1 进入容器

```bash
docker compose exec dev bash
```

### 3.2 配置和编译

```bash
mkdir -p build
cd build

cmake .. -B tests \
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

### 3.3 运行全部 HBF 读带宽实验

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
  requests:                   256 reads
  transferred:                1.000 MiB
  window:                     334 cycles (334.000 ns)
  throughput:                 3139.449 GB/s (3.139 TB/s)
  raw_link_ceiling:           4096.000 GB/s (4.096 TB/s)
  raw_link_utilization:       76.647 %
  ocp_user_target:            3072.000 GB/s (3.072 TB/s)
  ocp_target_achievement:     102.196 %

checks:
  completed_requests:         PASS (256/256)
  completed_bytes:            PASS (1048576 B)
  payload_integrity:          PASS
  outstanding_after_drain:    PASS (0)
  failures:                   PASS (0)
  observed_le_raw_ceiling:    PASS

result: PASS
```

结果说明：256个4 KiB读请求全部完成，共传输1 MiB；测量窗口为334个模拟周期，得到3.139 TB/s。该结果低于模型的4.096 TB/s raw link上限，并达到3.072 TB/s OCP用户带宽参考值的102.196%。

这里的3.139 TB/s是当前配置下的 **transaction-level模拟上限**。模型将OCP目标作为报告参考，尚未扣除所有真实协议和芯片开销，因此不能把它解释为硅片保证值。

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
