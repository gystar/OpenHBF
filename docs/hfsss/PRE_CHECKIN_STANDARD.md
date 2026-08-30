# HFSSS提交前检查规范

**状态**：必填—没有例外，没有跳过。

此存储库的每次代码更改都必须在之前通过提交前检查的入口
提交或推送评审。这适用于人类贡献者
以及在此代码库上运行的所有人工智能代理。

## 功能说明

运行完整QEMU黑盒捆绑包的单个命令— nvme-cli
冒烟测试， fio read/write/trim与CRC32C数据验证，以及
校准的混合读/写应力情况—针对仪器化的HFSSS
通过真正的Linux客户机访问NBD 服务器。

```
make pre-checkin
```

默认捆绑包（按顺序运行） ：

| # |用例|适用范围：|
|---|------|----------------|
| 1 |`001_nvme_cli_smoke.sh`|nvme list/id-ctrl/smart-log表面发现|
| 2 |`002_nvme_namespace_info.sh`|命名空间枚举， id-ns nsze/ncap字段|
| 3 |`003_nvme_flush_smoke.sh`|SYNC-NVM FLUSH命令往返|
| 4 |`010_fio_randwrite_verify.sh`|4K随机写入， 512 MiB ， CRC32C验证|
| 5 |`011_fio_randrw_verify.sh`|4K 70/30混合， 256 MiB ， CRC32C验证|
| 6 |`012_fio_seqwrite_verify.sh`|128K顺序写入， 256 MiB ， CRC32C验证|
| 7 |`013_fio_trim_verify.sh`|DSM解除分配+读零语义|
| 8 |`014_fio_pre_checkin_stress.sh`|** 4K 70/30混合， 8 GiB累积I/O ， iodepth = 64 ， numjobs = 1 ， CRC32C验证** —核心门禁用例|
| 9 |`900_spdk_nvme_identify.sh`|SPDK端标识（如果SPDK不存在，则优雅地跳过）|

总时钟： Mac Studio （ Apple Silicon、HVF、
macOS 14 + ）。案例014是其中的大部分—它是典型的
混合-rw应力+验证信号，校准以搅拌工作
在并发队列压力下设置~ 8倍（ 1 GiB工作集× 8 GiB
累积I/O ）。并发来自单个作业的`iodepth=64`
而不是多个writers — `numjobs=2`和`verify=1`触发器
fio的“多个写入程序可能会覆盖属于其他
jobs "警告并可以中止验证阶段，因此单个作业
选择是深思熟虑的。

## 及格条件

- `make pre-checkin`退出，代码为0。
- 任何FIO输出中均无`verify:`错误线。
- 在任何情况下都没有`ASSERT FAIL`线。
- 底层运行器打印`pre-checkin PASS — ready to commit`。

其他任何事情都是失败的。请勿在失败时提交。

## 如何运行

### 先决条件（一次性）

- 支持HVF的QEMU ： `brew install qemu` （ Apple Silicon ）。
- GitHub CLI ： `brew install gh` （适用于`make setup-guest` ；也适用于PR 工作）。
- `guest/`下的客户机资产：
  - `alpine-hfsss.qcow2` -启用HFSSS的Alpine 客户机镜像
  - `cidata.iso` — cloud-init seed （为您的SSH密钥本地构建）
  - `ovmf_vars-saved.fd` -预初始化的UEFI VARS

引导这三个文件的最快方法是：

```
make setup-guest
```

该目标下载最新发布的`guest-bundle-*` GitHub
释放，验证SHA-256 ，提取`alpine-hfsss.qcow2`和
`ovmf_vars-saved.fd`进入`guest/` ，然后构建一个新鲜的`cidata.iso`
授权您的本地SSH密钥
（ `~/.ssh/hfsss_qemu_key` ，如果缺失，则在第一次运行时生成）。

如果需要，固定到特定版本：

```
make setup-guest SETUP_GUEST_TAG=guest-bundle-2026-04-26-v0.001
```

SSH密钥对位于`~/.ssh/hfsss_qemu_key{,.pub}` （持久
跨重启— runner用于默认为`/tmp/` ，其中macOS
擦除，因此cidata.iso授权不断漂移）。覆盖
如果环境需要，则使用`HFSSS_SSH_KEY=/path/to/key`定位
不同的路径。

如果您自己构建捆绑包而不是下载，
`scripts/scrub-guest-image.sh`从
运行来宾和`scripts/build-guest-bundle.sh <version>`包
内置PII最终扫描的结果。

### 每次提交

```
make pre-checkin
```

如果需要，请覆盖客户机目录：

```
make pre-checkin GUEST_DIR=/path/to/guest
```

将额外参数转发给Runner ：

```
make pre-checkin BLACKBOX_ARGS="--skip-build --nbd-mode mt"
```

### 下一工作日服务器模式

默认情况下，闸门运行`--mode async`。NBD服务器的三种模式是：

|Mode|描述|门角色|
|---|---|---|
|`sync`|单线程NBD ，单线程FTL|最简单；不执行并发|
|`mt`|多线程FTL工作线程，同步NBD回复|zXQ0QXZ生产路径使用|
|`async`|多线程FTL +异步NBD回复管道|最高并发；最接近目标工作负载|

`async`是默认值，因为（ 1 ）它是生产目标—实
工作负载执行异步管道，因此此处显示回归
首先； （ 2 ）在预入住期间一直保持经验稳定
自PR # 44登陆MT模式TRIM/TAA修复以来的验证运行； (3) a
zXQ0QXZ上的门将无声地错过
发货的路径。想要显式mt/sync覆盖的代理可以运行
`BLACKBOX_ARGS="--mode mt"`作为后续；强制入口本身
以生产路径为目标。

如果异步开始在闸门上拍打，则将默认值切换为`mt` ，
打开薄片上的跟踪问题—不要默默地削弱大门。

### 公关描述中的证据

粘贴预告片横幅（ `pre-checkin PASS — ready to commit` ）并
将计时线插入PR主体。没有该证据的公关不是
有资格接受审核。

## 如果大门无法运行会发生什么

**没什么。**没有跳过路径。如果QEMU无法启动，如果客户机
图像丢失，如果NBD 服务器无法启动—那就是拦截器，
并且必须在合并提交之前修复它。报告失败
上游并阻止您自己的公关。

土地法的唯一可接受原因尚未通过
`make pre-checkin`是真正的基础设施中断（主机电源
故障、虚拟机监控程序错误）和明确的书面所有者签核记录
在公关线程中。“我没有安装QEMU”是不可接受的
原因—安装QEMU。

## 这道门为何存在

HFSSS固件是一个I/O路径。不可转让的合同是
并发下的数据完整性。单元测试涵盖个人
组件； `make pre-checkin`涵盖合同端到端：

- Real guest Linux → NVMe驱动程序→PCIe队列→NBD线→HFSSS
FTL → GC→介质→和备份。
- 每个区块的CRC32C验证意味着无声损坏
致命，而不是“片状测试”。
- `numjobs=1 iodepth=64`的8 GiB应力案例保留了写入
路径、GC和写后读取交错足够长的时间来显示
在较轻的负荷下通过的比赛。

此大门本来会遇到的先前事件：
- Discard =取消映射直通回归（ TRIM返回陈旧数据）。
- 下一工作日写入响应标头字段排序错误。
- 在写入繁重的混合工作负载下进行GC竞赛。

## 飞轮与进化

当新的用户可见的I/O功能出现时（新的管理命令，新的
命名空间类型，新数据路径优化） ，该更改的所有者
负责将专用案例添加到捆绑包中，而不是
绕过大门。

校准014应力轮廓参数。请勿更改
让大门更快。如果代码更改回退了014运行时，
调查回归—不要稀释信号。

## 关联

- `CONTRIBUTING.md` —顶级贡献政策（此处积分）。
- `docs/CI_RUN_ISOLATION.md` —并发CI运行方式保持隔离。
- `scripts/qemu_blackbox/` —黑盒跑步机框架（ PR # 39 ）。
- `docs/superpowers/specs/2026-04-05-ci-test-framework-roadmap-design.md` - 5支柱CI路线图背景。
