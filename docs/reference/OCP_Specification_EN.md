# OCP HBF Architecture Specification: Product Description Summary

## Document Scope

- **Source**: *OCP HBF Architecture Specification v0.7.0 FINAL*
- **Coverage**: Section 4, Product Description, PDF pages 15--24
- **Specification date**: 2026-08-03

This document summarizes the HBF product positioning, performance targets, system organization, Base Die responsibilities, channel signals, and address mapping. It is an explanatory summary and does not replace the source specification.

## 1. Product Positioning and Architecture

High Bandwidth Flash (HBF) is a non-coherent, high-capacity flash device tightly coupled to an xPU Host Compute die. It uses a distributed, wide-interface, multi-channel architecture to deliver read bandwidth approaching HBM while providing substantially higher capacity.

The architecture follows these principles:

- One HBF stack supports up to 16 mutually independent host channels.
- Each host channel uses an independent UCIe link and accesses only its assigned set of NAND dies; cross-channel data access is prohibited.
- Channels are independently clocked and operated and need not be synchronous with one another; operation within a channel is synchronous.
- A channel may carry one or more virtual AXI interfaces.
- The Core Die is the NAND die stack. The Base Die is the bottom die containing control logic beneath the Core Die stack.
- One HBF stack can provide 512 GiB or more, with the design optimized for high-bandwidth reads.

## 2. Major Features

### 2.1 Host Interface

- Based on UCIe 3.0 guidelines and supports the UCIe Host Streaming Protocol.
- Each channel provides a 64-bit full-duplex mainband interface, with each lane operating at up to 32 Gbps.
- Sixteen host channels target up to 3.072 TiB/s of effective user bandwidth.
- A 1-bit sideband interface operates at 800 MHz for management and user-defined applications.
- Writes use a 4 KiB burst on a 4 KiB-aligned address.
- Reads support bursts from 64 B through 4 KiB in 64 B increments. Addresses are 64 B aligned, and a transfer must not cross a 4 KiB NAND page boundary.
- A temperature sensor provides a 2-bit encoded range output.

### 2.2 NAND and Device-Side Capabilities

- Supports a 16-die NAND stack and a 4 KiB NAND page.
- Internally manages NAND bad blocks and maintains a read-refresh counter.
- Supports TSV failure detection and redundancy mapping between the Base and Core dies.
- Supports IEEE 1500 and DA access ports.
- Optionally provides scratchpad memory on all UCIe channels for high-priority, ultra-low-latency access on the order of L3 cache read/write latency.
- The approximate package footprint is `10.975 mm x 16 mm x 775 um`.

## 3. Performance and Capacity

### 3.1 Bandwidth Derivation

| Item | Value |
|------|-------|
| UCIe line rate | 32 GT/s |
| Data width per host channel | 64 bit (8 Byte) |
| Raw bandwidth per channel | 256 GB/s |
| Number of host channels | 16 |
| Total raw bandwidth per UCIe link | 4096 GB/s |
| AXI Link Layer protocol efficiency | 75% |
| Effective bandwidth per HBF cube | 3072 GB/s (3.072 TB/s) |

### 3.2 Capacity and Speed Grades

The reference capacity is 512 GiB per cube, based on 16 dies, 16 banks per channel, and a 4096-byte page.

| Speed Grade | Maximum User Bandwidth | UCIe Configuration | UCIe Rate | AXI Interfaces per UCIe Channel | Maximum Stack Height |
|-------------|------------------------|--------------------|-----------|---------------------------------|----------------------|
| 1 | 0.384 TB/s | x64 | 8 GT/s | 8 | 1, 2, or 4 |
| 2 | 1.536 TB/s | x64 | 16 GT/s | 16 | 1, 2, or 4 |
| 3 | 3.072 TB/s | x64 | 32 GT/s | 16 | 1, 2, or 4 |

AXI interfaces within a UCIe channel are virtual. The host may interleave addresses across them according to application needs. Raw and usable channel capacities are fixed and divided evenly among the virtual AXI channels. Interleaving may use any granularity that is a multiple of 64 B.

## 4. System Organization and Responsibilities

HBF consists of three major entities:

1. **Base Die**: handles host traffic, manages the UCIe protocol, and communicates with the Core Die.
2. **Core Die**: a stack containing 16 NAND dies.
3. **TSV Channels**: route signals between the Base Die and NAND dies.

The system-level example uses four HBF chipsets connected to one host. Each chipset supplies 16 host channels and 512 GiB. Addressing, clocks, and operation are isolated between channels.

### 4.1 Base Die Subsystems

| Subsystem | Primary Responsibilities |
|-----------|--------------------------|
| UCIe Controller | Manages the UCIe protocol and Host xPU communication; includes D2D UCIe Adapter and UCIe Protocol Layer functions |
| UCIe PHY | Implements the host-side UCIe physical interface |
| Base Die Controller | Handles host and NAND commands and responses, moves and schedules data between UCIe and the Core Die, performs ECC encode/decode and error handling, processes sideband commands, tracks read/write/erase status, initializes NAND and handles POR, manages TSV redundancy, and maintains a per-block page-read counter |
| Admin/Debug Management | Implements IEEE 1500, DA Port, and TSV repair control; the Debug Controller is shared by all 16 channels |

## 5. Host Channels and Signals

Each host channel contains an independent UCIe link with a 64-bit mainband and a 1-bit sideband. A channel accesses only its discrete memory pool. `Reset_SB`, the IEEE 1500 test port, DA Port, and power-supply signals are shared globally.

### 5.1 Per-Channel Signals

- Incoming and outgoing mainband paths each include 64-bit data, differential clock, valid, and track signals.
- Incoming and outgoing sideband paths each include 1-bit data and clock signals.
- Several critical signals include redundant micro-bumps to tolerate damaged interconnects.

### 5.2 Global Signals

| Signal | Purpose |
|--------|---------|
| `RESETSB_n` | Active-low sideband reset |
| `RESETBD_n` | Active-low Base Die/SoC reset |
| `REFCLK+/-` | 100 MHz differential reference clock |
| `READY` | Active-high indication that HBF can accept UCIe commands |
| `DA[39:0]` | Debug Access interface |
| `Vmon[3:0]` | Core Die voltage monitoring, limited to 12 V |
| `CATTRIP` | Active-high catastrophic-temperature alert with a programmable threshold |
| `STRAP[3:0]` | Vendor-specific boot-mode configuration |
| `CHIPID[2:0]` | JTAG IDCODE Arc-Num most-significant bits |

Some width-one global signals use two uBumps carrying the same signal so operation can survive one damaged uBump. `CATTRIP` may be asserted when a Base or Core Die temperature sensor crosses a programmable threshold. Firmware may also periodically read Core Die sensors and trigger the signal.

### 5.3 IEEE 1500 and DA Port

The IEEE 1500 serial test port provides reset, clock, instruction-select, shift, capture, update, input-data, and 16-bit output-data signals. Redundant WSOs can be remapped over damaged WSO connections. `DA[12] = 0` selects the IEEE 1500 ports, while `DA[12] = 1` selects the DA Port.

## 6. Address Mapping and Data Interleaving

Host software maps the global address space into local addresses for individual HBF UCIe channels. Each channel exposes a continuous logical space, so the host normally does not need Bank, Die, or Block details. The Base Die maps local logical addresses onto physical NAND resources to maximize utilization and performance.

If physical organization must be derived, the host can read the mapping configuration from device configuration registers. In the specified mapping, the first four relevant local-address bits identify the Bank and the next two identify the Die.

- Device Logical Unit Size is 4 KiB, the minimum Base Die read/write granularity to the Core Die.
- The reference organization has 16 banks per die and four dies.
- `Device Logical Unit = HBF UCIe local channel address >> 6`.
- Device Logical Units may be striped across Core Die channels, dies, and banks to increase parallelism and performance.

With four AXI ports per UCIe module, the host treats each AXI port as a local channel and selects an address-interleaving option according to workload requirements. Maximum HBF read bandwidth is obtained when all four AXI ports respond at full bandwidth using 4 KiB interleaving.

## 7. Key Takeaway

The central HBF design connects a high-capacity NAND stack closely to an xPU through 16 isolated x64 UCIe channels. The Base Die hides NAND management, ECC, TSV repair, and physical address mapping, allowing the host to operate on continuous channel-local address spaces and exploit interleaved virtual AXI interfaces. The architecture prioritizes read bandwidth, reaching approximately 3.072 TB/s at the highest 32 GT/s speed grade while retaining 512 GiB-class capacity, debug access, thermal protection, and link redundancy.
