#ifndef _UDP_H
#define _UDP_H

#include "ip.h"
#include "../lib/stdint.h"

struct udp_packet {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
    uint8_t data[];
} __attribute__((packed));

uint16_t udp_calculate_checksum(struct udp_packet *packet);

void udp_send_packet(ip_addr_t *dst_ip, uint16_t src_port, uint16_t dst_port, void *data, uint32_t len);

void udp_handle_packet(struct udp_packet *packet, uint32_t len);

#endif
