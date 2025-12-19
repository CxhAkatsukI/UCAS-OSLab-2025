# Development Note

### Task 1: NIC Initialization and Polled Transmission

To meet the requirement of task 1 in the guidebook, we need to enable the kernel to communicate with the E1000 NIC hardware. Since the system is running in Sv39 virtual memory mode (from Project 4), we must first map the NIC's physical register space into the kernel's virtual address space. Then, we initialize the transmit (TX) descriptor ring and implement a polling-based transmission function.

#### 1.1 I/O Remapping (`ioremap`)
The NIC registers are located at a physical address provided by the Device Tree. To access them, we implement `ioremap`. We use 2MB large pages to map the I/O range starting from `IO_ADDR_START` (`0xffffffe000000000`). This ensures efficient access to memory-mapped I/O (MMIO).

```c
// kernel/mm/ioremap.c
void *ioremap(unsigned long phys_addr, unsigned long size)
{
    uintptr_t va_start = io_base;

    /* Map loop: Map 2MB pages until size is covered */
    while (size > 0) {
        kernel_map_page_helper(io_base, phys_addr, pa2kva(PGDIR_PA));

        /* Use 2MB steps */
        io_base   += LARGE_PAGE_SIZE;
        phys_addr += LARGE_PAGE_SIZE;

        if (size < LARGE_PAGE_SIZE)
            size = 0;
        else
            size -= LARGE_PAGE_SIZE;
    }

    local_flush_tlb_all();
    return (void *)va_start;
}
```

#### 1.2 Kernel Access to User Memory (`SUM` bit)
During network transmission, the kernel needs to read data directly from user-space buffers. In RISC-V, Supervisor mode is normally restricted from accessing User pages. We enable the `SUM` (allow Supervisor User Memory access) bit in the `sstatus` register during boot.

```assembly
/* arch/riscv/kernel/head.S */
ENTRY(_start)
  /* ... */
  /* Enable SUM bit to allow kernel access user memory */
  li t0, SR_SUM
  csrs CSR_SSTATUS, t0
  /* ... */
```

#### 1.3 Transmit Configuration (`e1000_configure_tx`)
We set up a circular array of 64 descriptors. Each descriptor points to a dedicated DMA buffer. We use `kva2pa` to provide the hardware with physical addresses. 

```c
// drivers/e1000.c
static void e1000_configure_tx(void)
{
    for (int i = 0; i < TXDESCS; i++) {
        /* Link descriptor to pre-allocated physical DMA buffer */
        tx_desc_array[i].addr = kva2pa((uintptr_t)tx_pkt_buffer[i]);
        tx_desc_array[i].cmd = E1000_TXD_CMD_RS;     /* Report Status */
        tx_desc_array[i].status = E1000_TXD_STAT_DD; /* Mark as initially Done */
    }

    /* Program Base Address and Length registers */
    uint64_t tx_base = kva2pa((uintptr_t)tx_desc_array);
    e1000_write_reg(e1000, E1000_TDBAL, (uint32_t)(tx_base & 0xffffffff));
    e1000_write_reg(e1000, E1000_TDBAH, (uint32_t)(tx_base >> 32));
    e1000_write_reg(e1000, E1000_TDLEN, TXDESCS * sizeof(struct e1000_tx_desc));

    /* Initialize Head and Tail pointers to 0 */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

    /* Enable Transmit and set collision parameters */
    e1000_write_reg(e1000, E1000_TCTL,
        E1000_TCTL_EN | E1000_TCTL_PSP | (E1000_TCTL_CT & 0x100) | (E1000_TCTL_COLD & 0x40000));

    local_flush_dcache();
}
```

#### 1.4 Polled Transmission (`e1000_transmit`)
The transmission logic follows these steps:
1.  Read the current `TDT` (Tail) from the NIC.
2.  Check the `DD` (Descriptor Done) bit in the status field. If it's 0, the hardware is still busy, and the queue is full.
3.  Copy user data to the DMA buffer and set the `EOP` (End of Packet) and `RS` (Report Status) bits.
4.  Increment `TDT` to notify the hardware that a new packet is ready.
5.  Use `local_flush_dcache()` to ensure the data is written from CPU cache to RAM where the DMA engine can see it.

```c
// drivers/e1000.c
int e1000_transmit(void *txpacket, int length)
{
    local_flush_dcache(); 

    uint32_t tail = e1000_read_reg(e1000, E1000_TDT);

    /* Check if hardware is done with this descriptor */
    if ((tx_desc_array[tail].status & E1000_TXD_STAT_DD) == 0) {
        return 0; /* Queue full, return 0 for calling logic to wait */
    }

    tx_desc_array[tail].status = 0; /* Reset status */
    uint16_t len = (length > TX_PKT_SIZE) ? TX_PKT_SIZE : length;
    tx_desc_array[tail].length = len;

    memcpy((uint8_t *)tx_pkt_buffer[tail], txpacket, len);
    tx_desc_array[tail].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;

    /* Advance tail */
    e1000_write_reg(e1000, E1000_TDT, (tail + 1) % TXDESCS);

    local_flush_dcache();
    return len;
}
```

This completes the basic polled transmission mechanism. In the `do_net_send` syscall, if `e1000_transmit` returns 0, the process will either busy-wait (Task 1) or block (Task 3).

### Task 2: NIC Initialization and Polled Reception

To meet the requirement of task 2 in the guidebook, we implement the reception logic for the E1000 NIC. Unlike transmission, which is active, reception is passive: the hardware identifies incoming packets, filters them by MAC address, and places them into memory. Our driver must prepare a ring of "empty" buffers for the NIC to fill and then poll them to retrieve data.

#### 2.1 Receive Configuration (`e1000_configure_rx`)
Initialization involves several critical steps to prepare the hardware for incoming traffic:
1.  **MAC Filtering**: We program the `RAL0` and `RAH0` registers with the hardcoded MAC address. This tells the NIC to accept packets specifically addressed to our board. We also enable Broadcast Accept Mode (`BAM`) to receive ARP requests and other broadcast traffic.
2.  **Descriptor Setup**: Similar to TX, we initialize a circular array of 64 descriptors. Each points to a `rx_pkt_buffer` in physical memory.
3.  **Head/Tail Logic**: For reception, the hardware owns descriptors in the range `[Head, Tail]`. We initialize `RDH` to 0 and `RDT` to `RXDESCS - 1`, giving the NIC ownership of all available descriptors at startup.
4.  **Control Register**: We enable the receiver (`EN`), set the buffer size to 2048 bytes, and enable Unicast Promiscuous Mode (`UPE`) for testing flexibility.

```c
// drivers/e1000.c
static void e1000_configure_rx(void)
{
    /* Set MAC Address to RAL0/RAH0 */
    uint32_t ral0 = (enetaddr[3] << 24) | (enetaddr[2] << 16) | (enetaddr[1] << 8) | enetaddr[0];
    uint32_t rah0 = E1000_RAH_AV | (enetaddr[5] << 8) | enetaddr[4]; 
    e1000_write_reg_array(e1000, E1000_RA, 0, ral0);
    e1000_write_reg_array(e1000, E1000_RA, 1, rah0);

    /* Initialize rx descriptors */
    for (int i = 0; i < RXDESCS; i++) {
        rx_desc_array[i].addr = kva2pa((uintptr_t)rx_pkt_buffer[i]);
        rx_desc_array[i].status = 0; 
    }

    uint64_t rx_base = kva2pa((uintptr_t)rx_desc_array);
    e1000_write_reg(e1000, E1000_RDBAL, (uint32_t)(rx_base & 0xffffffff));
    e1000_write_reg(e1000, E1000_RDBAH, (uint32_t)(rx_base >> 32));
    e1000_write_reg(e1000, E1000_RDLEN, RXDESCS * sizeof(struct e1000_rx_desc));

    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, RXDESCS - 1); // Hardware owns all

    /* Enable RX, BAM (Broadcast), and UPE (Promiscuous) */
    e1000_write_reg(e1000, E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_UPE);

    local_flush_dcache();
}
```

#### 2.2 Polled Reception (`e1000_poll`)
The `e1000_poll` function checks if a packet has arrived. The "next" packet to be processed by software is always at `(Tail + 1) % RXDESCS`. 
1.  We check the `DD` bit. If it is 1, the NIC has filled the buffer and updated the descriptor.
2.  We copy the data to the user-provided buffer.
3.  **Returning the Descriptor**: Crucially, we clear the status and advance the `RDT` register to this index. This "moves the boundary" and gives the buffer back to the hardware for future packets.

```c
// drivers/e1000.c
int e1000_poll(void *rxbuffer)
{
    local_flush_dcache();

    /* Software processes the descriptor AFTER the current Tail */
    uint32_t tail = (e1000_read_reg(e1000, E1000_RDT) + 1) % RXDESCS;

    /* Check if the NIC has finished writing this packet */
    if ((rx_desc_array[tail].status & E1000_RXD_STAT_DD) == 0) {
        return 0; /* No packet yet */
    }

    uint16_t length = rx_desc_array[tail].length;
    memcpy(rxbuffer, (uint8_t *)rx_pkt_buffer[tail], length);

    rx_desc_array[tail].status = 0; /* Clear status for next use */
    e1000_write_reg(e1000, E1000_RDT, tail); /* Give back to HW */

    local_flush_dcache();
    return length;
}
```

#### 2.3 Network Receive Subsystem (`do_net_recv`)
The high-level kernel function `do_net_recv` manages requests for multiple packets. For Task 2, if no packet is found, the system would typically busy-wait. However, to stay consistent with the Project 5 architecture, we implement the polling loop. (Note: The blocking logic for Task 3 is also present here, using `do_block`).

```c
// kernel/net/net.c
int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    int total_bytes = 0;
    for (int i = 0; i < pkt_num; i++) {
        int len = e1000_poll(rxbuffer);

        if (len > 0) {
            pkt_lens[i] = len;
            rxbuffer += len;
            total_bytes += len;
        } else {
            /* Task 2 Logic: Busy wait or loop back */
            /* Task 3 Logic: Block and wait for interrupt */
            e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0 | E1000_IMS_RXT0); 
            local_flush_dcache();
            do_block(&CURRENT_RUNNING->list, &recv_block_queue);
            i--; // Retry this packet index upon wake-up
        }
    }
    return total_bytes;
}
```

By completing this task, the OS is capable of basic two-way communication using the E1000 NIC via system calls.

### Task 3: Interrupt-based Packet Transmission and Reception

To meet the requirement of task 3 in the guidebook, we transition from CPU-intensive polling to an efficient interrupt-driven model. Instead of spinning while waiting for the NIC to finish sending or for new data to arrive, the current process will block and yield the CPU. The NIC will then trigger an external interrupt through the **PLIC** to wake the process once the hardware condition is met.

#### 3.1 External Interrupt Routing (PLIC)
The E1000 NIC is connected to the Platform-Level Interrupt Controller (PLIC). We must handle the multi-stage interrupt flow:
1.  **PLIC Claim**: When an external interrupt occurs, the kernel reads the `claim` register to identify the source (ID 33 for QEMU, ID 3 for PYNQ).
2.  **NIC Dispatch**: If the ID matches the NIC, we call `net_handle_irq`.
3.  **PLIC Complete**: After processing, we write the ID back to the `complete` register to allow the PLIC to signal future interrupts.

```c
// kernel/irq/irq.c
void handle_irq_ext(regs_context_t *regs, uint64_t stval, uint64_t scause)
{
    /* 1. Claim the interrupt from PLIC */
    uint32_t irq = plic_claim();

    /* 2. Check if the interrupt is from our NIC */
    if (irq == PLIC_E1000_PYNQ_IRQ || irq == PLIC_E1000_QEMU_IRQ) {
        net_handle_irq();
    }

    /* 3. Signal completion to PLIC */
    if (irq) {
        plic_complete(irq);
    }
}
```

#### 3.2 Transmit Blocking Logic (`do_net_send`)
In the interrupt-driven version, if `e1000_transmit` returns 0 (queue full), we enable the **TXQE** (Transmit Queue Empty) interrupt mask and call `do_block`. The process enters the `send_block_queue` and stays there until the hardware signals that it has emptied its descriptors.

```c
// kernel/net/net.c
int do_net_send(void *txpacket, int length)
{
    int trans_len = 0;
    while (1) {
        trans_len = e1000_transmit(txpacket, length);
        if (trans_len > 0) break; 

        /* Enable TXQE interrupt to wake us up when space opens */
        e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE);
        local_flush_dcache();

        /* Block the current process until hardware is ready */
        do_block(&CURRENT_RUNNING->list, &send_block_queue);
    }
    return trans_len;
}
```

#### 3.3 Receive Blocking Logic (`do_net_recv`)
Similarly, if `e1000_poll` finds no packets, we enable the **RXDMT0** (Receive Descriptor Minimum Threshold) or **RXT0** (Receive Timer) interrupts. This ensures that the process is woken up as soon as a packet arrives or the descriptor threshold is reached.

```c
// kernel/net/net.c
int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{
    for (int i = 0; i < pkt_num; i++) {
        int len = e1000_poll(rxbuffer);
        if (len > 0) {
            pkt_lens[i] = len;
            rxbuffer += len;
        } else {
            /* No data: Enable RX interrupts and sleep */
            e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0 | E1000_IMS_RXT0); 
            local_flush_dcache();
            do_block(&CURRENT_RUNNING->list, &recv_block_queue);
            i--; // Retry the same index after waking
        }
    }
    return 0;
}
```

#### 3.4 NIC Interrupt Handler (`net_handle_irq`)
The main NIC handler reads the `ICR` (Interrupt Cause Register) to see why the interrupt fired. **Note**: Reading `ICR` automatically clears the status bits. We then unblock all processes in the relevant queues.

```c
// kernel/net/net.c
void net_handle_irq(void)
{
    local_flush_dcache();
    uint32_t icr = e1000_read_reg(e1000, E1000_ICR);

    if (icr & E1000_ICR_TXQE) {
        /* Hardware finished sending, wake up senders */
        check_and_unblock(&send_block_queue);
        /* Clear mask to avoid interrupt storm */
        e1000_write_reg(e1000, E1000_IMC, E1000_IMC_TXQE);
    }
    
    if ((icr & E1000_ICR_RXDMT0) || (icr & E1000_ICR_RXT0)) {
        /* New packets arrived, wake up receivers */
        check_and_unblock(&recv_block_queue);
        e1000_write_reg(e1000, E1000_IMC, E1000_IMC_RXDMT0 | E1000_IMC_RXT0);
    }
    local_flush_dcache();
}
```

By using this approach, the OS maximizes CPU utilization, allowing other user programs (like `fly`) to run smoothly even during heavy network I/O.

### Task 4: Reliable Network Data Transfer

To meet the requirement of task 4 in the guidebook, we implement a simplified transport layer protocol to ensure reliable, ordered data delivery over the potentially unreliable network. This involves handling packet loss, duplication, and out-of-order arrival using sequence numbers (`seq`), Acknowledgments (`ACK`), and Resend requests (`RSD`).

#### 4.1 Reliable Protocol Header
We define a custom 8-byte header starting at the 55th byte of the Ethernet frame. We use `__attribute__((packed))` to ensure hardware-level alignment and helper macros like `ntohl`/`htonl` to handle the conversion between Network Byte Order (Big Endian) and RISC-V Byte Order (Little Endian).

```c
// include/os/net.h
struct reliable_hdr {
    uint8_t magic;       /* 0x45 */
    uint8_t flags;       /* DAT, ACK, RSD */
    uint16_t len;        /* Payload length */
    uint32_t seq;        /* Sequence Number (Byte offset) */
} __attribute__((packed));
```

#### 4.2 Sending Control Packets (`send_control`)
Since our kernel lacks a full TCP/IP stack, we manually construct control packets. We spoof the Ethernet and IPv4 headers to satisfy the `pktRxTx` tool on the host. This function allows the receiver to notify the sender of received data (`ACK`) or missing data (`RSD`).

```c
// kernel/net/net.c
static void send_control(uint8_t flags, uint32_t seq)
{
    memset(control_packet, 0, sizeof(control_packet));
    /* ... Manually fill Ethernet (14B) and IP (20B) and TCP (20B) ... */

    /* Fill Reliable Header at Offset 54 */
    struct reliable_hdr *rh = (struct reliable_hdr *)(control_packet + RELIABLE_HDR_OFFSET);
    rh->magic = NET_MAGIC;
    rh->flags = flags;
    rh->seq   = htonl(seq);

    /* Transmit via driver */
    while (e1000_transmit(control_packet, 64) == 0) ;
}
```

#### 4.3 Reorder Buffer and Stream Reception (`do_net_recv_stream`)
The core logic of the reliable layer resides in `do_net_recv_stream`. It maintains a global `current_seq` to track the next expected byte.
1.  **Check Reorder Buffer**: Before polling the NIC, we check if the expected packet was previously received out-of-order and stored in the `reorder_buf`.
2.  **In-Order Arrival**: If the received `seq == current_seq`, we copy the data to the user buffer and advance `current_seq`.
3.  **Out-of-Order Arrival**: If `seq > current_seq`, we store the packet in a free slot in the `reorder_buf` and send an `RSD` for the missing data.
4.  **Timeouts**: If no valid packet arrives for a certain period, we trigger an `RSD` to prompt the sender.

```c
// kernel/net/net.c
int do_net_recv_stream(void *buffer, int *nbytes)
{
    /* ... Initialization ... */
    while (received == 0) {
        /* 1. Check Reorder Buffer for current_seq */
        if (found_in_reorder_buffer) {
            /* Copy data, update current_seq, send ACK, break */
        }

        /* 2. Poll NIC for new packets */
        int len = e1000_poll(rx_raw);
        if (len <= 0) {
            /* Block and check for Timeout to send RSD */
            do_block(...);
            continue;
        }

        /* 3. Parse Header */
        struct reliable_hdr *rh = (struct reliable_hdr *)(rx_raw + RELIABLE_HDR_OFFSET);
        uint32_t seq = ntohl(rh->seq);

        if (seq == current_seq) {
            /* In-order: Copy and ACK */
            current_seq += dlen;
            send_control(NET_OP_ACK, current_seq);
        } else if (seq > current_seq) {
            /* Out-of-order: Store in buffer and RSD */
            store_in_reorder_buf(rx_raw);
            send_control(NET_OP_RSD, current_seq);
        }
    }
    return 0;
}
```

#### 4.4 Reliable Transfer Verification
In user space, we implemented `recv_file.c` to test this system. It receives a file, parses the file size from the first 4 bytes of the stream, and calculates a **Fletcher-16 checksum** to verify data integrity.

```c
// test/test_project5/recv_file.c
int main() {
    /* ... */
    while (total_received_data < file_size) {
        sys_net_recv_stream(buffer, &nbytes);
        /* Calculate Fletcher-16 on incoming stream */
        checksum = fletcher16_step(checksum, buffer, nbytes);
        total_received_data += nbytes;
    }
    printf("Final Checksum: 0x%04x\n", checksum);
}
```

This completes all tasks for Project 5. The system now supports a robust networking stack capable of handling real-world network conditions.

---

### Debug Note

#### Bug 1

**Phenomenon:** `e1000` does not trigger any interrupt.

**Solution:** Modify `plic_init` function to allow `e1000` trigger interrupts in S mode:

```C
// In drivers/plic.c
handler->hart_base   = plic_regs + CONTEXT_BASE + CONTEXT_PER_HART;
handler->enable_base = plic_regs + ENABLE_BASE + ENABLE_PER_HART;
```

#### Bug 2

**Phenomenon & AI Prompt:**

I'm currently working on Operating System Lab Project 5 task 4. I want you to read my code carefully and help me debug:

1. The issue is as follows: I executed `exec recv_stream` in the shell, since there's no packets received yet, the program's status will turn to `BLOCKED`. Then, I executed `sudo ./pktRxTx -m 5` from external, the expected behavior is, when enough numbers of packet arrives, `e1000` will trigger an interrupt. However, in my case, the interrupt never fires, and the user program stay blocked.
2. In the previous experiment, I confirmed that e1000 CAN trigger an interrupt when I execute the user program `recv`, and execute `./pktRxTx -m 1` from external, and type in `send 60`. This time the `recv` program will be woken up and start receiving packets. I am confused why this case will fire the interrupt and the problematic case won't.
3. I hope you to read my code carefully and tell me: a. when will the e1000 netcard fire an interrupt? How to fix my problem for task 4?? Pleas DO NOT modify my code directly, ONLY tell me how to fix it and give me example code. I will apply the fix MYSELF.
4. Here is the content of the guidebook, you can use as a reference: ...

**Solution:** The problem occurs because the `send_control` function is not working. Its sent control packets are filtered out by `pktRxTx`. In order to avoid this, our control packets shall have valid `IPv4` and `TCP` headers, as shown in the floowing code:

```C
// In kernel/net/net.c
    /* --- 2. IPv4 Header (20 bytes) --- */
    /* Offset 14 */
    uint8_t *ip = (uint8_t *)(control_packet + 14);
    ip[0] = 0x45; /* Ver=4, IHL=5 */
    ip[1] = 0x00; /* TOS */
    /* Total Len = 62 (EtherHeader(14) not included in IP Len) */
    /* IP(20) + TCP(20) + Reliable(8) = 48 bytes */
    ip[2] = 0x00; ip[3] = 0x30; /* 48 bytes */
    ip[4] = 0x00; ip[5] = 0x00; /* ID */
    ip[6] = 0x40; ip[7] = 0x00; /* Flags=DF, Frag=0 */
    ip[8] = 0x40; /* TTL=64 */
    ip[9] = 0x06; /* Proto=TCP (6) - CRITICAL for pktRxTx */
    /* Checksum (calc later) */
    ip[10] = 0; ip[11] = 0; 
    /* Src IP: 10.0.0.2 (Board) */
    ip[12] = 10; ip[13] = 0; ip[14] = 0; ip[15] = 2;
    /* Dst IP: 10.0.0.67 (Host - pktRxTx default) */
    ip[16] = 10; ip[17] = 0; ip[18] = 0; ip[19] = 67;
    
    /* Calc IP Checksum */
    uint16_t csum = calc_checksum(ip, 20);
    ip[10] = csum & 0xff;
    ip[11] = csum >> 8;

    /* --- 3. TCP Header (20 bytes) --- */
    /* Offset 14 + 20 = 34 */
    uint8_t *tcp = (uint8_t *)(control_packet + 34);
    /* Ports: 1234 -> 5678 */
    tcp[0] = 0x04; tcp[1] = 0xd2;
    tcp[2] = 0x16; tcp[3] = 0x2e;
    /* Seq/Ack = 0 */
    /* Offset=5 (20 bytes), Flags=ACK(0x10) */
    tcp[12] = 0x50; tcp[13] = 0x10;
    /* Window, Checksum, UrgPtr = 0 */
```

#### Bug 3

**Phenomenon**: QEMU passed, on-board stuck.

**Solution:** `RECV_TIMEOUT` value too large. make it smaller (I decreased it from `100000000` to `1000000`), although QEMU will encounter an error under this value (the error is subtle, only occurs when user program `recv_stream` transfers more than 100 packets), the on-board test is successful (As far as I can concern, it can stably transfer more than 500+ packets).
