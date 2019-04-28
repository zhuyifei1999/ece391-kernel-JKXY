#ifndef UDP_H
#define UDP_H
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

typedef struct udp_packet {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;
    uint16_t checksum;
    uint8_t data[];
} __attribute__((packed)) udp_packet_t;

uint16_t udp_calculate_checksum(udp_packet_t * packet);

void udp_send_packet(uint8_t * dst_ip, uint16_t src_port, uint16_t dst_port, void * data, int len);


#endif
