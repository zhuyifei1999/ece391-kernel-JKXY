#include "udp.h"
#include "inet.h"
#include "../char/tty.h"
#include "../lib/string.h"
#include "../mm/kmalloc.h"

// adapted from: https://github.com/szhou42/osdev/tree/master/src/kernel

uint16_t udp_calculate_checksum(struct udp_packet *packet) {
    // UDP checksum is optional in IPv4
    return 0;
}

void udp_send_packet(ip_addr_t *dst_ip, uint16_t src_port, uint16_t dst_port, void *data, uint32_t len) {
    uint32_t length = sizeof(struct udp_packet) + len;
    struct udp_packet *packet = kmalloc(length);
    *packet = (struct udp_packet) {
        .src_port = htons(src_port),
        .dst_port = htons(dst_port),
        .length = htons(length),
        .checksum = udp_calculate_checksum(packet),
    };

    // Copy data over
    memcpy(packet->data, data, len);
    ip_send_packet(dst_ip, packet, length);

    kfree(packet);
}

void udp_handle_packet(struct udp_packet *packet, uint32_t len) {
    __attribute__((unused)) uint16_t src_port = ntohs(packet->src_port);
    __attribute__((unused)) uint16_t dst_port = ntohs(packet->dst_port);

    __attribute__((unused)) void *data_ptr = packet->data;
    uint32_t data_len = ntohs(packet->length);
    // assert data_len == len
    data_len -= sizeof(*packet);

    char *data = strndup(data_ptr, data_len);
    tty_foreground_puts(data);
    kfree(data);

    return;
}
