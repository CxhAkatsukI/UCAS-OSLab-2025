#include <e1000.h>
#include <type.h>
#include <os/string.h>
#include <os/time.h>
#include <assert.h>
#include <pgtable.h>

// E1000 Registers Base Pointer
volatile uint8_t *e1000;  // use virtual memory address

// E1000 Tx & Rx Descriptors
static struct e1000_tx_desc tx_desc_array[TXDESCS] __attribute__((aligned(16)));
static struct e1000_rx_desc rx_desc_array[RXDESCS] __attribute__((aligned(16)));

// E1000 Tx & Rx packet buffer
static char tx_pkt_buffer[TXDESCS][TX_PKT_SIZE];
static char rx_pkt_buffer[RXDESCS][RX_PKT_SIZE];

// Fixed Ethernet MAC Address of E1000
static const uint8_t enetaddr[6] = {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53};

/**
 * e1000_reset - Reset Tx and Rx Units; mask and clear all interrupts.
 **/
static void e1000_reset(void)
{
	/* Turn off the ethernet interface */
    e1000_write_reg(e1000, E1000_RCTL, 0);
    e1000_write_reg(e1000, E1000_TCTL, 0);

	/* Clear the transmit ring */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

	/* Clear the receive ring */
    e1000_write_reg(e1000, E1000_RDH, 0);
    e1000_write_reg(e1000, E1000_RDT, 0);

	/**
     * Delay to allow any outstanding PCI transactions to complete before
	 * resetting the device
	 */
    latency(1);

	/* Clear interrupt mask to stop board from generating interrupts */
    e1000_write_reg(e1000, E1000_IMC, 0xffffffff);

    /* Clear any pending interrupt events. */
    while (0 != e1000_read_reg(e1000, E1000_ICR)) ;
}

/**
 * e1000_configure_tx - Configure 8254x Transmit Unit after Reset
 **/
static void e1000_configure_tx(void)
{
    /* TODO: [p5-task1] Initialize tx descriptors */
    for (int i = 0; i < TXDESCS; i++) {
        /* Point the descriptor to our pre-allocated packet buffer */
        tx_desc_array[i].addr = kva2pa((uintptr_t)tx_pkt_buffer[i]);
        tx_desc_array[i].length = 0;
        tx_desc_array[i].cso = 0;
        tx_desc_array[i].cmd = E1000_TXD_CMD_RS;     /* Report Status (so we can check DD bit) */
        tx_desc_array[i].status = E1000_TXD_STAT_DD; /* Mark as "Done" initially so we can use it */
        tx_desc_array[i].css = 0;
    }

    /* TODO: [p5-task1] Set up the Tx descriptor base address and length */
    uint64_t tx_base = kva2pa((uintptr_t)tx_desc_array);
    e1000_write_reg(e1000, E1000_TDBAL, (uint32_t)(tx_base & 0xffffffff));
    e1000_write_reg(e1000, E1000_TDBAH, (uint32_t)(tx_base >> 32));
    e1000_write_reg(e1000, E1000_TDLEN, TXDESCS * sizeof(struct e1000_tx_desc));

	/* TODO: [p5-task1] Set up the HW Tx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_TDH, 0);
    e1000_write_reg(e1000, E1000_TDT, 0);

    /* TODO: [p5-task1] Program the Transmit Control Register */
    /* EN: Enable, PSP: Pad Short Packets, CT: Collision Threshold, COLD: Collision Distance */
    e1000_write_reg(e1000, E1000_TCTL,
        E1000_TCTL_EN | E1000_TCTL_PSP | (E1000_TCTL_CT & 0x100) | (E1000_TCTL_COLD & 0x40000));

    /* Ensure memory writes hit RAM */
    local_flush_dcache();
}

/**
 * e1000_configure_rx - Configure 8254x Receive Unit after Reset
 **/
static void e1000_configure_rx(void)
{
    /* TODO: [p5-task2] Set e1000 MAC Address to RAR[0] */
    /*
     * The card uses this to filter incoming packets.
     * enetaddr is {0x00, 0x0a, 0x35, 0x00, 0x1e, 0x53}
     */
    uint32_t ral0 = (enetaddr[3] << 24) | (enetaddr[2] << 16) | (enetaddr[1] << 8) | enetaddr[0];
    uint32_t rah0 = E1000_RAH_AV | (enetaddr[5] << 8) | enetaddr[4]; /* AV = Address Valid */

    e1000_write_reg_array(e1000, E1000_RA, 0, ral0);
    e1000_write_reg_array(e1000, E1000_RA, 1, rah0);

    /* TODO: [p5-task2] Initialize rx descriptors */
    for (int i = 0; i < RXDESCS; i++) {
        rx_desc_array[i].addr = kva2pa((uintptr_t)rx_pkt_buffer[i]);
        rx_desc_array[i].length = 0;
        rx_desc_array[i].status = 0; /* Hardware will set this when packet arrives */
        rx_desc_array[i].errors = 0;
    }

    /* TODO: [p5-task2] Set up the Rx descriptor base address and length */
    uint64_t rx_base = kva2pa((uintptr_t)rx_desc_array);
    e1000_write_reg(e1000, E1000_RDBAL, (uint32_t)(rx_base & 0xffffffff));
    e1000_write_reg(e1000, E1000_RDBAH, (uint32_t)(rx_base >> 32));
    e1000_write_reg(e1000, E1000_RDLEN, RXDESCS * sizeof(struct e1000_rx_desc));

    /* TODO: [p5-task2] Set up the HW Rx Head and Tail descriptor pointers */
    e1000_write_reg(e1000, E1000_RDH, 0);
    /*
     * Point Tail to the end. Hardware owns [Head, Tail]. 
     * Initially Hardware owns everything (0 to 63).
     */
    e1000_write_reg(e1000, E1000_RDT, RXDESCS - 1);

    /* TODO: [p5-task2] Program the Receive Control Register */
    /* EN: Enable, BAM: Broadcast Accept Mode, BSEX/BSIZE = 0 (2048 buffer size) */
    /* UPE: Unicast Promiscuous Mode (Accept all unicast packets) - needed for pktRxTx mismatch */
    e1000_write_reg(e1000, E1000_RCTL, E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_UPE);

    /* Configure RXT0 related registers */
    e1000_write_reg(e1000, E1000_RDTR, 1);
    e1000_write_reg(e1000, E1000_RADV, 1);

    /* TODO: [p5-task4] Enable RXDMT0 Interrupt */
    e1000_write_reg(e1000, E1000_IMS, E1000_IMS_RXDMT0 | E1000_IMS_RXT0);

    local_flush_dcache();
}

/**
 * e1000_init - Initialize e1000 device and descriptors
 **/
void e1000_init(void)
{
    /* Reset E1000 Tx & Rx Units; mask & clear all interrupts */
    e1000_reset();

    /* Configure E1000 Tx Unit */
    e1000_configure_tx();

    /* Configure E1000 Rx Unit */
    e1000_configure_rx();
}

/**
 * e1000_transmit - Transmit packet through e1000 net device
 * @param txpacket - The buffer address of packet to be transmitted
 * @param length - Length of this packet
 * @return - Number of bytes that are transmitted successfully
 **/
int e1000_transmit(void *txpacket, int length)
{
    /* TODO: [p5-task1] Transmit one packet from txpacket */

    /* Sync cache */
    local_flush_dcache(); 

    /* 1. Get current Tail index */
    uint32_t tail = e1000_read_reg(e1000, E1000_TDT);

    /* * 2. Check if the descriptor is available (DD bit set).
     * If DD is 0, hardware is still processing this slot.
     */
    if ((tx_desc_array[tail].status & E1000_TXD_STAT_DD) == 0) {
        return 0; /* Queue full */
    }

    /* 3. Prepare the descriptor */
    tx_desc_array[tail].status = 0; /* Clear DD bit (mark as in-use) */

    /* Clamp length to max packet size */
    uint16_t len = (length > TX_PKT_SIZE) ? TX_PKT_SIZE : length;
    tx_desc_array[tail].length = len;

    /* Copy data from the argument buffer to the DMA buffer */
    memcpy((uint8_t *)tx_pkt_buffer[tail], txpacket, len);

    /* Set command flags: EOP (End of Packet) + RS (Report Status) */
    tx_desc_array[tail].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;

    /* 4. Advance Tail pointer (circular buffer) */
    uint32_t next_tail = (tail + 1) % TXDESCS;
    e1000_write_reg(e1000, E1000_TDT, next_tail);

    /* Sync cache again */
    local_flush_dcache();
    return len;
}

/**
 * e1000_poll - Receive packet through e1000 net device
 * @param rxbuffer - The address of buffer to store received packet
 * @return - Length of received packet
 **/
int e1000_poll(void *rxbuffer)
{
    /* TODO: [p5-task2] Receive one packet and put it into rxbuffer */

    local_flush_dcache();

    /* 1. Determine the descriptor index to check */
    /* * The "Tail" register points to the last descriptor available to hardware.
     * So (Tail + 1) is the one software should look at.
     */
    uint32_t tail = (e1000_read_reg(e1000, E1000_RDT) + 1) % RXDESCS;

    /* 2. Check status */
    if ((rx_desc_array[tail].status & E1000_RXD_STAT_DD) == 0) {
        return 0; /* No packet received */
    }

    /* 3. Copy packet data */
    uint16_t length = rx_desc_array[tail].length;
    memcpy(rxbuffer, (uint8_t *)rx_pkt_buffer[tail], length);

    /* 4. Reset status */
    rx_desc_array[tail].status = 0;

    /* 5. Advance Tail to give this descriptor back to hardware */
    e1000_write_reg(e1000, E1000_RDT, tail);

    local_flush_dcache();
    return length;
}
