#ifndef __INCLUDE_NET_H__
#define __INCLUDE_NET_H__

#include <os/list.h>
#include <type.h>

#define PKT_NUM 32

#define ETH_ALEN 6u                 // Length of MAC address
#define ETH_P_IP 0x0800u            // IP protocol
// Ethernet header
struct ethhdr {
    uint8_t ether_dmac[ETH_ALEN];   // destination mac address
    uint8_t ether_smac[ETH_ALEN];   // source mac address
    uint16_t ether_type;            // protocol format
};

static LIST_HEAD(send_block_queue);
static LIST_HEAD(recv_block_queue);

void net_handle_irq(void);
int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens);
int do_net_send(void *txpacket, int length);

/* Task 4: Reliable Transport Protocol */
#define NET_MAGIC       0x45
#define NET_OP_DAT      0x01 /* Data Packet */
#define NET_OP_RSD      0x02 /* Resend Request */
#define NET_OP_ACK      0x04 /* Acknowledge */

/**
 * struct reliable_hdr - Header for the custom reliable transport protocol.
 * @magic: Magic number (0x45) to identify valid packets.
 * @flags: Packet type flags (DAT, ACK, RSD).
 * @len: Length of the data payload (Big Endian).
 * @seq: Sequence number for ordering and reliability (Big Endian).
 *
 * This structure is packed to ensure it occupies exactly 8 bytes and aligns
 * correctly with the network byte stream.
 */
struct reliable_hdr {
    uint8_t magic;       /* 0x45 */
    uint8_t flags;       /* DAT, ACK, RSD */
    uint16_t len;        /* Length of data (Big Endian) */
    uint32_t seq;        /* Sequence Number (Big Endian) */
} __attribute__((packed));

/* Helper macros for Endian conversion (RISC-V is Little Endian) */
#define ntohs(x) ((((x) & 0xff) << 8) | (((x) >> 8) & 0xff))
#define htons(x) ntohs(x)

#define ntohl(x) ((((x) & 0xff) << 24) | \
                  (((x) & 0xff00) << 8) | \
                  (((x) & 0xff0000) >> 8) | \
                  (((x) >> 24) & 0xff))
#define htonl(x) ntohl(x)

/* Protocol Constants */
#define REORDER_BUF_SIZE 1024
#define MAX_DATA_PER_PKT 1500
#define RELIABLE_HDR_OFFSET 54 /* Ethernet Header (14) + IP/UDP/Padding (55) */

/**
 * struct reorder_slot_t - Entry in the reorder buffer.
 * @valid: Flag indicating if this slot is occupied.
 * @seq: The sequence number of the stored packet.
 * @len: The length of the data payload.
 * @data: Buffer storing the out-of-order packet data.
 */
typedef struct {
    int valid;
    uint32_t seq;
    uint16_t len;
    uint8_t data[MAX_DATA_PER_PKT];
} reorder_slot_t;

/* Declare the new syscall handler */
int do_net_recv_stream(void *buffer, int *nbytes);

/* Initialize the reliable layer */
void init_reliable_layer(void);

#endif  // __INCLUDE_NET_H__
