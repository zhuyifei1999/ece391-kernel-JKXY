#include "ethernet.h"
#include "inet.h"
#include "arp.h"
#include "ip.h"
#include "../drivers/rtl8139.h"
#include "../lib/string.h"
#include "../mm/paging.h"
#include "../mm/kmalloc.h"
#include "../lib/io.h"
#include "../irq.h"

// adapted from: https://github.com/szhou42/osdev/tree/master/src/kernel

int ethernet_send_packet(mac_addr_t *dst_mac_addr, void *data, uint32_t len, uint16_t protocol) {
    mac_addr_t src_mac_addr;
    struct ethernet_frame *frame = kmalloc(sizeof(*frame) + len);

    // Get source mac address from network card driver
    get_mac_addr(&src_mac_addr);

    // Fill in source and destination mac address
    memcpy(&frame->src_mac_addr, &src_mac_addr, 6);
    memcpy(&frame->dst_mac_addr, &dst_mac_addr, 6);

    // Fill in data
    memcpy(frame->data, data, len);

    // Fill in type
    frame->type = htons(protocol);

    // Send packet
    rtl8139_send_packet(frame, sizeof(*frame) + len);

    kfree(frame);

    return len;
}

void ethernet_handle_packet(struct ethernet_frame *frame, uint32_t len) {
    int data_len = len - sizeof(*frame);
    // ARP packet
    if (ntohs(frame->type) == ETHERNET_TYPE_ARP) {
        arp_handle_packet((void *)&frame->data, data_len);
    }
    // IP packets(could be TCP, UDP or others)
    if (ntohs(frame->type) == ETHERNET_TYPE_IP) {
        ip_handle_packet((void *)&frame->data, data_len);
    }
}
