#ifndef _ETHERNET_H
#define _ETHERNET_H

#include "../lib/stdint.h"

#define ETHERNET_TYPE_ARP 0x0806
#define ETHERNET_TYPE_IP  0x0800

#define HARDWARE_TYPE_ETHERNET 0x01

typedef uint8_t mac_addr_t[6];

struct ethernet_frame {
    mac_addr_t dst_mac_addr;
    mac_addr_t src_mac_addr;
    uint16_t type;
    uint8_t data[];
} __attribute__((packed));

int ethernet_send_packet(mac_addr_t *dst_mac_addr, void *data, uint32_t len, uint16_t protocol);

void ethernet_handle_packet(struct ethernet_frame *frame, uint32_t len);

#endif
