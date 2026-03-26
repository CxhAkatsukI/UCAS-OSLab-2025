#include <e1000.h>
#include <type.h>
#include <os/debug.h>
#include <os/net.h>
#include <os/sched.h>
#include <os/string.h>
#include <os/time.h>
#include <os/list.h>
#include <os/smp.h>
#include <os/irq.h>

/**
 * do_net_send - Send a network packet, blocking if the transmit queue is full.
 * @txpacket: Pointer to the buffer containing the packet data.
 * @length: Length of the packet in bytes.
 *
 * This function attempts to transmit a packet via the e1000 driver.
 * If the hardware transmit queue is full, it enables the Transmit Queue Empty
 * (TXQE) interrupt and blocks the current process. The process is woken up
 * by the interrupt handler when space becomes available.
 *
 * Return: The number of bytes successfully transmitted.
 */
int do_net_send(void *txpacket, int length)
{
    int trans_len = 0;

    while (1) {

        // TODO: [p5-task1] Transmit one network packet via e1000 device
        trans_len = e1000_transmit(txpacket, length);

        if (trans_len > 0) {
            /* Success! */
            break; 
        }

        // TODO: [p5-task4] Enable TXQE interrupt if transmit queue is full
        /* * Queue is full. Enable TXQE interrupt to wake us up when space opens.
         * IMS: Interrupt Mask Set 
         */
        e1000_write_reg(e1000, E1000_IMS, E1000_IMS_TXQE);
        local_flush_dcache();

        // TODO: [p5-task3] Call do_block when e1000 transmit queue is full
        /* Block the current process */
        do_block(&CURRENT_RUNNING->list, &send_block_queue);

        /* When we wake up, loop back and try e1000_transmit again. */
    }

    return trans_len;

}

int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens)
{

    int total_bytes = 0;
    int received_count = 0;

    /* Attempt to receive 'pkt_num' packets */
    for (int i = 0; i < pkt_num; i++) {

        // TODO: [p5-task2] Receive one network packet via e1000 device
        int len = e1000_poll(rxbuffer);

        if (len > 0) {
            /* Success: record length and advance buffer */
            pkt_lens[i] = len;
            rxbuffer += len;
            total_bytes += len;
            received_count++;
        } else {
            /* No packet available right now. Block. */

            /* Enable RXDMT0 (or RXT0) to wake us up on packet arrival */
            e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0 | E1000_IMS_RXT0); 
            /* e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXT0); Optional: if RXDMT0 is not enough */

            local_flush_dcache();

            // TODO: [p5-task3] Call do_block when there is no packet on the way
            do_block(&CURRENT_RUNNING->list, &recv_block_queue);

            /* Woke up (maybe by interrupt), loop back and try polling again */
        }
    }

    return total_bytes;
}

/**
 * check_and_unblock - Helper to unblock all tasks in a specific queue.
 * @queue: The block queue to process (send or recv).
 *
 * This function wakes up all processes currently waiting in the specified
 * queue. It is the simplest approach to ensure any process waiting for an
 * event gets a chance to run.
 */
void check_and_unblock(list_head *queue)
{
    while (!list_is_empty(queue)) {
        list_node_t *node = queue->next;
        do_unblock(node);
    }
}

/**
 * handle_e1000_txqe - Handler for TXQE (Transmit Queue Empty) interrupt.
 *
 * This interrupt fires when the hardware has finished sending packets and
 * the queue is sufficiently empty (TDT == TDH).
 * * We wake up any processes blocked on sending and disable the interrupt
 * mask (IMC) to prevent storms until we block again.
 */
static void handle_e1000_txqe(void)
{
    /* Wake up processes waiting to send */
    check_and_unblock(&send_block_queue);

    /* * Disable the TXQE interrupt mask bit (IMC).
     * We only enable it when we actually block in the transmit function.
     */
    e1000_write_reg(e1000, E1000_IMC, E1000_IMC_TXQE);
    local_flush_dcache();
}

/**
 * handle_e1000_rxdmt0 - Handler for RXDMT0 (Receive Descriptor Minimum Threshold).
 *
 * This interrupt fires when free descriptors are running low, or effectively
 * when incoming packets have consumed enough descriptors.
 *
 * Note: In reality, RXDMT0 triggers when the ring is *almost* full of data.
 * For this lab, we use it (or RXT0) as a signal to detect incoming packets.
 */
static void handle_e1000_rxdmt0(void)
{
    /* Wake up processes waiting to receive */
    check_and_unblock(&recv_block_queue);

    /* Disable interrupt until we block again */
    e1000_write_reg(e1000, E1000_IMC, E1000_IMC_RXDMT0 | E1000_IMC_RXT0);
    local_flush_dcache();
}

/**
 * net_handle_irq - Main Network Interrupt Dispatcher.
 *
 * This function reads the Interrupt Cause Register (ICR) to identify the
 * source of the interrupt (TX or RX) and calls the appropriate handler.
 * Reading ICR automatically clears the interrupt bits.
 */
void net_handle_irq(void)
{
    // TODO: [p5-task4] Handle interrupts from network device

    local_flush_dcache();
    uint32_t icr = e1000_read_reg(e1000, E1000_ICR); /* Reading ICR clears it! */

    if (icr & E1000_ICR_TXQE) {
        handle_e1000_txqe();
    }
    if ((icr & E1000_ICR_RXDMT0) || (icr & E1000_ICR_RXT0)) {
        handle_e1000_rxdmt0();
    }
}

/* Global State for the Reliable Layer */
static uint32_t current_seq = 0;

/* Global Reorder Buffer */
static reorder_slot_t reorder_buf[REORDER_BUF_SIZE];

/**
 * send_control - Send a control packet (ACK or RSD).
 * @flags: The operation flag (NET_OP_ACK or NET_OP_RSD).
 * @seq: The sequence number to acknowledge or request.
 *
 * This function constructs and transmits a control packet. It manually
 * fills the Ethernet header and the custom reliable protocol header
 * at the specific offset required by the receiving application.
 */
/* Global Buffer */
static char control_packet[128]; 

/* Simple Checksum Function */
static uint16_t calc_checksum(void *data, int len)
{
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *)data;
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    if (len) sum += *(uint8_t *)ptr;
    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return ~sum;
}

static void send_control(uint8_t flags, uint32_t seq)
{
    memset(control_packet, 0, sizeof(control_packet));

    /* --- 1. Ethernet Header (14 bytes) --- */
    struct ethhdr *eth = (struct ethhdr *)control_packet;
    /* Dest MAC: Host (Unicast) */
    uint8_t dst_mac[6] = {0x80, 0xfa, 0x5b, 0x33, 0x56, 0xef}; 
    memcpy(eth->ether_dmac, dst_mac, 6); 
    /* Source MAC: Spoofed Board MAC */
    uint8_t src_mac[6] = {0x00, 0x10, 0x53, 0x00, 0x30, 0x83};
    memcpy(eth->ether_smac, src_mac, 6);
    eth->ether_type = htons(0x0800); /* IPv4 */

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

    /* --- 4. Reliable Header --- */
    /* Offset 14 + 20 + 20 = 54 (Matches RELIABLE_HDR_OFFSET) */
    struct reliable_hdr *rh = (struct reliable_hdr *)(control_packet + RELIABLE_HDR_OFFSET);
    rh->magic = NET_MAGIC;
    rh->flags = flags;
    rh->len   = 0;
    rh->seq   = htonl(seq);

    /* --- 5. Transmit --- */
    int pkt_len = 64; /* Min Ethernet Frame */

    klog("Sending TCP ACK: Magic=%x Seq=%d\n", rh->magic, seq);

    while (do_net_send(control_packet, pkt_len) == 0) ;
}

/**
 * do_net_recv_stream - Receive a reliable stream of data.
 * @buffer: User buffer to write the received data to.
 * @nbytes: Input: maximum bytes to read. Output: actual bytes read.
 *
 * This function implements a reliable transport receiver. It checks the
 * reorder buffer for expected packets, polls the network for new data,
 * handles out-of-order packets by buffering them, and requests retransmission
 * (RSD) for missing sequence numbers.
 *
 * Return: 0 on success (bytes read are returned via nbytes pointer).
 */
/* Global function to be called from timer interrupt */
void net_timer_check(void)
{
    check_and_unblock(&recv_block_queue);
}

int do_net_recv_stream(void *buffer, int *nbytes)
{
    int wanted = *nbytes;
    int received = 0;
    uint8_t *user_ptr = (uint8_t *)buffer;
    uint64_t last_recv_time = get_ticks();
    const uint64_t RECV_TIMEOUT = 1000000; 

    while (received == 0) { // Only loop until we get AT LEAST one packet
        /* --- Step 1: Check Reorder Buffer --- */
        for (int i = 0; i < REORDER_BUF_SIZE; i++) {
            if (reorder_buf[i].valid && reorder_buf[i].seq == current_seq) {
                int dlen = reorder_buf[i].len;
                
                // Ensure we don't overflow if user provided a tiny buffer
                // In your case, 1400 is plenty for one 992 packet.
                int copy_len = (dlen > wanted) ? wanted : dlen;
                
                memcpy(user_ptr, reorder_buf[i].data, copy_len);
                received = copy_len;
                current_seq += dlen; // Advancing by dlen is okay now because we return
                reorder_buf[i].valid = 0;
                
                send_control(NET_OP_ACK, current_seq);
                break; 
            }
        }
        if (received > 0) break;

        /* --- Step 2: Poll Hardware --- */
        char rx_raw[2048];
        int len = e1000_poll(rx_raw);

        if (len <= 0) {
            e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0 | E1000_IMS_RXT0);
            local_flush_dcache();
            do_block(&CURRENT_RUNNING->list, &recv_block_queue);

            if ((get_ticks() - last_recv_time) > RECV_TIMEOUT) {
                send_control(NET_OP_RSD, current_seq);
                last_recv_time = get_ticks(); 
            }
            continue;
        }
        
        last_recv_time = get_ticks();

        /* --- Step 3: Parse Packet --- */
        if (len < RELIABLE_HDR_OFFSET + sizeof(struct reliable_hdr)) continue;
        struct reliable_hdr *rh = (struct reliable_hdr *)(rx_raw + RELIABLE_HDR_OFFSET);
        if (rh->magic != NET_MAGIC || rh->flags != NET_OP_DAT) continue;

        uint32_t seq = ntohl(rh->seq);
        uint16_t dlen = ntohs(rh->len);
        uint8_t *data_ptr = (uint8_t *)(rh + 1);

        if (seq == current_seq) {
            int copy_len = (dlen > wanted) ? wanted : dlen;
            memcpy(user_ptr, data_ptr, copy_len);
            received = copy_len;
            current_seq += dlen;
            send_control(NET_OP_ACK, current_seq);
        } else if (seq > current_seq) {
            // Out of order: Store in buffer
            for (int i = 0; i < REORDER_BUF_SIZE; i++) {
                if (!reorder_buf[i].valid) {
                    reorder_buf[i].valid = 1;
                    reorder_buf[i].seq = seq;
                    reorder_buf[i].len = dlen;
                    memcpy(reorder_buf[i].data, data_ptr, (dlen > MAX_DATA_PER_PKT) ? MAX_DATA_PER_PKT : dlen);
                    break;
                }
            }
            send_control(NET_OP_RSD, current_seq);
        } else {
            // Old packet: Just re-ACK
            send_control(NET_OP_ACK, current_seq);
        }
    }

    *nbytes = received;
    return 0;
}

/**
 * init_reliable_layer - Initialize the reliable transport layer.
 *
 * This function resets the reorder buffer valid bits and resets the
 * expected sequence number to 0.
 */
void init_reliable_layer(void)
{
    for(int i=0; i<REORDER_BUF_SIZE; i++) 
        reorder_buf[i].valid = 0;
    current_seq = 0;
}
