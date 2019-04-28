#include "udp.h"
#include "inet.h"
#include "../lib/string.h"
#include "../mm/kmalloc.h"
#include "../printk.h"

uint16_t udp_calculate_checksum(struct udp_packet *packet) {
    // UDP checksum is optional in IPv4
    return 0;
}

void udp_send_packet(ip_addr_t *dst_ip, uint16_t src_port, uint16_t dst_port, void *data, uint32_t len) {
    uint32_t length = sizeof(struct udp_packet) + len;
    struct udp_packet *packet = kmalloc(length);
    memset(packet, 0, sizeof(struct udp_packet));
    packet->src_port = htons(src_port);
    packet->dst_port = htons(dst_port);
    packet->length = htons(length);
    packet->checksum = udp_calculate_checksum(packet);

    // Copy data over
    memcpy((void *)packet + sizeof(struct udp_packet), data, len);
    printk("UDP Packet sent\n");
    ip_send_packet(dst_ip, packet, length);
}

void udp_handle_packet(struct udp_packet *packet, uint32_t len) {
    //uint16_t src_port = ntohs(packet->src_port);
    uint16_t dst_port = ntohs(packet->dst_port);
    uint16_t length = ntohs(packet->length);

    __attribute__((unused)) void *data_ptr = packet->data;
    __attribute__((unused)) uint32_t data_len = length;
    printk("Received UDP packet, dst_port %d, data dump:\n", dst_port);

    // TODO
    return;
}
