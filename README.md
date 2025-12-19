# Project 5: Network Device Driver and Reliable Transport

**University of Chinese Academy of Sciences - Operating System (RISC-V)**

## 1. Overview
This project implements a high-performance network subsystem for the UCAS-OS kernel. We developed a device driver for the Intel **82540EM (e1000)** NIC, integrated it with the **PLIC** for interrupt-driven I/O, and built a custom **Reliable Transport Protocol** to handle packet loss and out-of-order delivery in a virtualized network environment.

## 2. Implemented Features

### Task 1 & 2: NIC Driver & Polled I/O
*   **MMIO via `ioremap`:** Implemented a kernel-space mapping utility using 2MB large pages to access NIC hardware registers at `0xffffffe0...`.
*   **DMA Descriptor Rings:** Configured circular transmit (TX) and receive (RX) rings using the Legacy descriptor format.
*   **Polled Transmission:** `e1000_transmit` handles buffer copying and tail pointer management.
*   **Polled Reception:** `e1000_poll` retrieves data from hardware-owned buffers and recycles descriptors.

### Task 3: Interrupt-Driven Networking
*   **PLIC Integration:** Registered a top-level handler for `IRQ_S_EXT`. Implemented the Claim/Complete handshake to route interrupts to the NIC.
*   **TXQE & RXDMT0:** Enabled hardware interrupts for "Transmit Queue Empty" and "Receive Descriptor Minimum Threshold" (triggered at 50% capacity).
*   **Process Blocking:** Processes now call `do_block` when the TX queue is full or the RX queue is empty, significantly reducing CPU usage compared to polling.

### Task 4: Reliable Transport Protocol (Stream API)
*   **Custom Protocol (Magic 0x45):** Implemented a transport layer supporting sequence numbers (`seq`), Acknowledgments (`ACK`), and Resend requests (`RSD`).
*   **Reorder Buffer:** Created a 1024-slot buffer in the kernel to cache out-of-order packets, ensuring the user-space stream remains contiguous.
*   **Stream Syscall:** `sys_net_recv_stream` provides a high-level API that abstracts away packet boundaries and network reliability issues.
*   **Integrity Verification:** Developed `recv_file` using the **Fletcher-16** checksum algorithm to verify 100% data accuracy over lossy links.

## 3. How to Build and Run

### Prerequisites
*   RISC-V Toolchain (gcc, gdb, qemu)
*   `tap0` network interface configured on the host machine.

### Compilation
```bash
make clean
make
```

### Running with Networking
**Multicore (SMP) Mode:**
```bash
make run-net
```

**Debugging Mode:**
```bash
make debug-net
```

## 4. Test Guide

Once the OS boots and you enter the shell:

### 1. Basic Transmit Test
```bash
exec send
```
*Observation:* Check the host terminal running `tcpdump -i tap0`. You should see 4 packets of 226 bytes each being transmitted.

### 2. Basic Receive Test
```bash
exec recv
```
*On Host:* Run `./pktRxTx -m 1` and type `send 32`.
*Observation:* The shell should print the hex content of the 32 packets received.

### 3. Reliable Stream Test
```bash
exec recv_stream
```
*On Host:* Run `./pktRxTx -m 5`. This mode simulates a lossy/unordered network.
*Observation:* The OS will log "Sending TCP RSD" when gaps are detected and successfully reconstruct the data stream.

### 4. File Transfer & Checksum
```bash
exec recv_file
```
*On Host:* Run `./pktRxTx -m 5 -l 10 -s 50 -f <filename>`.
*Observation:* The system will report the file size, show a progress bar, and finally print the Fletcher-16 checksum.

## 5. Key Design Decisions

### 5.1 Supervisor User Memory Access (`SUM`)
To allow the kernel to transmit data directly from user-space buffers via DMA, we explicitly set the `SR_SUM` bit in the `sstatus` register. This bypasses the Supervisor-mode protection that normally prevents accessing user-mapped pages, avoiding unnecessary `memcpy` operations.

### 5.2 PLIC Context Alignment
A critical fix was made in `plic_init`. The register offsets for `hart_base` and `enable_base` were shifted to include `CONTEXT_PER_HART`. Without this, the kernel was attempting to claim interrupts from the wrong PLIC context, resulting in silent failures during task 3.

### 5.3 Reliable Layer Timeout
The `sys_net_recv_stream` syscall includes a timeout mechanism. If no packets arrive within a specific window, it proactively sends an `RSD` (Resend) packet for the `current_seq`. This prevents the system from hanging if the final packet of a transmission is lost.
