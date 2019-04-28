#ifndef ETHERNET_H
#define ETHERNET_H
#include "rtl8139.h"
#include "../mm/paging.h"
#include "../irq.h"
#include "../char/tty.h"
#include "../vfs/device.h"
#include "../lib/stdint.h"
#include "../lib/stdbool.h"
#include "../lib/string.h"
#include "../lib/cli.h"
#include "../mm/kmalloc.h"
#include "../lib/io.h"
#include "../initcall.h"

#define ETHERNET_TYPE_ARP 0x0806
#define ETHERNET_TYPE_IP  0x0800

#define HARDWARE_TYPE_ETHERNET 0x01

typedef struct ethernet_frame {
  uint8_t dst_mac_addr[6];
  uint8_t src_mac_addr[6];
  uint16_t type;
  uint8_t data[];
} __attribute__((packed)) ethernet_frame_t;

int ethernet_send_packet(uint8_t * dst_mac_addr, uint8_t * data, int len, uint16_t protocol);

void ethernet_handle_packet(ethernet_frame_t * packet, int len);


#endif
