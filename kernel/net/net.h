#ifndef __INCLUDE_NET_H__
#define __INCLUDE_NET_H__

/* Packet num */
#define PKT_NUM 32

/* Net syscall handlers */
void net_handle_irq(void);
int do_net_recv(void *rxbuffer, int pkt_num, int *pkt_lens);
int do_net_send(void *txpacket, int length);

#endif  /* __INCLUDE_NET_H__ */
