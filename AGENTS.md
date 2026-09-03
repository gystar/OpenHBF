# OpenHBF Agent Instructions

本文件适用于整个仓库。任何 agent 在修改代码、运行构建、添加测试或开展性能实验前，必须先阅读：

1. `docs/README.md`
2. `docs/实验与测试执行规范.md`
3. 所修改模块对应的 HLD 和 LLD

实验与测试必须遵守以下硬约束：

- 构建和测试默认在仓库提供的 Conda 环境中通过 CMake/CTest 执行；Docker用于固定工具链复现，两种路径均可形成正式证据，但必须在artifact中记录实际环境。
- 所有生成文件只能位于 `/tmp`、容器临时目录或仓库内被 `.gitignore` 覆盖的 `build-*` 目录。
- 禁止在仓库根目录、`src/`、`include/`、`tests/` 或 `docs/` 中生成 `.o`、`.a`、`.so`、可执行文件、日志或实验数据。
- 手工编译探针必须显式指定输出，例如 `-o /tmp/openhbf_probe.o`；不得依赖编译器默认输出路径。
- 不得把手工 `g++`、独立 Makefile 或 fake pipeline 的结果写成正式验证通过。
- 更新实现状态前，必须按 `docs/实验与测试执行规范.md` 保存可复现证据并核对证据等级。

若任务要求与上述规范冲突，应先向用户说明冲突，不得静默降低验证标准。
