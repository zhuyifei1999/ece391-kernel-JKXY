#ifndef _IP_H
#define _IP_H

#include "../lib/stdint.h"

#define IP_IPV4 4

#define IP_PACKET_NO_FRAGMENT 2
#define IP_IS_LAST_FRAGMENT 4

#define PROTOCOL_UDP 17
#define PROTOCOL_TCP 6

typedef uint8_t ip_addr_t[4];

struct ip_packet {
    char version_ihl_ptr[0];
    uint8_t version:4;
    uint8_t ihl:4;
    uint8_t tos;
    uint16_t length;
    uint16_t id;
    char flags_fragment_ptr[0];
    uint8_t flags:3;
    uint8_t fragment_offset_high:5;
    uint8_t fragment_offset_low;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t header_checksum;
    ip_addr_t src_ip;
    ip_addr_t dst_ip;
    uint8_t data[];
} __attribute__((packed));

void get_ip_str(char *ip_str, ip_addr_t *ip);

uint16_t ip_calculate_checksum(struct ip_packet *packet);

void ip_send_packet(ip_addr_t *dst_ip, void *data, uint32_t len);

void ip_handle_packet(struct ip_packet *packet, uint32_t len);

#endif
