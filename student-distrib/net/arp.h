#ifndef _ARP_H
#define _ARP_H

#include "../lib/stdbool.h"
#include "../lib/stdint.h"
#include "ethernet.h"
#include "ip.h"

#define ARP_REQUEST 1
#define ARP_REPLY 2

struct arp_packet {
    uint16_t hardware_type;
    uint16_t protocol;
    uint8_t hardware_addr_len;
    uint8_t protocol_addr_len;
    uint16_t opcode;
    mac_addr_t src_hardware_addr;
    ip_addr_t src_protocol_addr;
    mac_addr_t dst_hardware_addr;
    ip_addr_t dst_protocol_addr;
} __attribute__((packed));

struct arp_table_entry {
    ip_addr_t ip_addr;
    mac_addr_t mac_addr;
};

void arp_handle_packet(struct arp_packet *arp_packet, uint32_t len);

void arp_send_packet(mac_addr_t *dst_hardware_addr, ip_addr_t *dst_protocol_addr);

bool arp_lookup(mac_addr_t *ret_hardware_addr, ip_addr_t *ip_addr);

void arp_lookup_add(mac_addr_t *ret_hardware_addr, ip_addr_t *ip_addr);


#endif
