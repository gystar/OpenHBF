# HFSSS vhost-user-blk 集成指南

## 功能说明

`hfsss-vhost-blk` 可执行文件通过 Unix 域套接字，将 HFSSS NVMe 模拟器公开为一个 **vhost-user 块设备**。QEMU 使用 `vhost-user-blk-pci` 设备连接该后端，从而在不需要内核驱动或真实硬件的情况下使用模拟 NVMe 设备。

借助该功能，可以从 Linux 客户机内部测试 HFSSS 的完整固件栈，包括 FTL、介质模型和 PCIe/NVMe 模块。

---

## 架构

```text
 Guest OS (Linux)
   |  virtio-blk driver
   v
 QEMU (qemu-system-aarch64)
   |  vhost-user protocol over Unix socket
   v
 hfsss-vhost-blk  <-- this server
   |  nvme_uspace API
   v
 HFSSS NVMe simulator
   |
   +-- FTL / Flash Translation Layer
   +-- HAL / Media model
   +-- PCIe/NVMe command queue engine
```

数据路径如下：

1. 客户机发出 virtio-blk 读写请求。
2. QEMU 通过 vhost-user 套接字转发请求。
3. `hfsss-vhost-blk` 将请求转换为 NVMe I/O 命令。
4. 命令经过完整的 HFSSS 栈，包括队列、FTL 和闪存模型。
5. 完成状态和数据返回 QEMU，随后传递给客户机。

---

## 前置条件

```bash
# macOS
brew install qemu

# Linux
apt install qemu-system-aarch64   # Debian/Ubuntu
dnf install qemu-system-aarch64   # Fedora
```

还需要一个 AArch64 Linux 客户机镜像（内核和根文件系统）。可以使用精简的 Alpine Linux virt 镜像：

```bash
wget https://dl-cdn.alpinelinux.org/alpine/v3.19/releases/aarch64/alpine-virt-3.19.1-aarch64.iso
```

---

## 快速开始

```bash
# 1. 构建项目
make -j$(nproc)

# 2. 运行 vhost 测试（可选的基本检查）
./build/bin/test_vhost_proto

# 3. 使用 HFSSS 后端启动 QEMU
KERNEL=/path/to/vmlinuz ROOTFS=/path/to/rootfs.qcow2 \
    scripts/run_qemu_nvme.sh
```

该脚本会先启动 `hfsss-vhost-blk`，然后启动 QEMU。按 `Ctrl-A X` 退出 QEMU；脚本会在退出时清理套接字。

---

## 手动启动

单独启动服务器：

```bash
./build/bin/hfsss-vhost-blk -s /tmp/hfsss-vhost.sock
```

然后使用以下参数手动启动 QEMU：

```text
-chardev socket,id=char0,path=/tmp/hfsss-vhost.sock,reconnect=1
-device  vhost-user-blk-pci,chardev=char0,num-queues=1
-object  memory-backend-file,id=mem,size=4G,mem-path=/tmp/qemu-mem,share=on
-numa    node,memdev=mem
```

vhost-user 协议要求使用 `-object memory-backend-file ... share=on` 和 `-numa node,memdev=mem`，以便 QEMU 通过文件描述符与后端共享客户机内存。

---

## 故障排查

| 现象 | 可能原因 | 解决方法 |
|------|----------|----------|
| `bind: Address already in use` | 存在旧的套接字文件 | 执行 `rm /tmp/hfsss-vhost.sock` |
| QEMU 立即退出 | 服务器尚未运行 | 在启动 QEMU 前启动 `hfsss-vhost-blk` |
| `accel hvf not available` | 当前主机不支持 HVF | 将 `-accel hvf` 替换为 `-accel tcg` |
| 客户机中未出现块设备 | QEMU 版本不兼容 | 使用支持 `vhost-user-blk-pci` 的 QEMU 版本 |
| 写入套接字时权限不足 | 套接字目录不可写 | 使用 `/tmp` 或用户主目录下的路径 |

最低支持的 QEMU 版本为 **5.0**。
