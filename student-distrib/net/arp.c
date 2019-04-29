#include "arp.h"
#include "inet.h"
#include "../drivers/rtl8139.h"
#include "../net/ethernet.h"
#include "../lib/string.h"
#include "../mm/kmalloc.h"
#include "../printk.h"
#include "../initcall.h"

// adapted from: https://github.com/szhou42/osdev/tree/master/src/kernel

#define ARP_TABLE_SIZE 512

struct arp_table_entry arp_table[ARP_TABLE_SIZE];
int arp_table_size;
int arp_table_curr;

mac_addr_t broadcast_mac_address = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

void arp_handle_packet(struct arp_packet *arp_packet, uint32_t len) {
    mac_addr_t dst_hardware_addr;
    ip_addr_t dst_protocol_addr;
    // Save some packet field
    memcpy(&dst_hardware_addr, arp_packet->src_hardware_addr, sizeof(dst_hardware_addr));
    memcpy(&dst_protocol_addr, arp_packet->src_protocol_addr, sizeof(dst_protocol_addr));
    // Reply arp request, if the ip address matches(have to hard code the IP eveywhere, because I don't have dhcp yet)
    if (ntohs(arp_packet->opcode) == ARP_REQUEST) {
        uint32_t my_ip = 0x0e02000a;

        if (memcmp(arp_packet->dst_protocol_addr, &my_ip, sizeof(dst_protocol_addr))) {
            // Set source MAC address, IP address (hardcode the IP address as 10.2.2.3 until we really get one..)
            get_mac_addr(&arp_packet->src_hardware_addr);
            arp_packet->src_protocol_addr[0] = 10;
            arp_packet->src_protocol_addr[1] = 0;
            arp_packet->src_protocol_addr[2] = 2;
            arp_packet->src_protocol_addr[3] = 14;

            // Set destination MAC address, IP address
            memcpy(arp_packet->dst_hardware_addr, &dst_hardware_addr, sizeof(dst_hardware_addr));
            memcpy(arp_packet->dst_protocol_addr, &dst_protocol_addr, sizeof(dst_protocol_addr));

            // Set opcode
            arp_packet->opcode = htons(ARP_REPLY);

            // Set lengths
            arp_packet->hardware_addr_len = 6;
            arp_packet->protocol_addr_len = 4;

            // Set hardware type
            arp_packet->hardware_type = htons(HARDWARE_TYPE_ETHERNET);

            // Set protocol = IPv4
            arp_packet->protocol = htons(ETHERNET_TYPE_IP);

            // Now send it with ethernet
            ethernet_send_packet(&dst_hardware_addr, arp_packet, sizeof(struct arp_packet), ETHERNET_TYPE_ARP);
        }
    } else if (ntohs(arp_packet->opcode) == ARP_REPLY){
        // May be we can handle the case where we get a reply after sending a request, but i don't think my os will ever need to do so...
    } else {
        printk("Got unknown ARP, opcode = %d\n", arp_packet->opcode);
    }

    // Now, store the ip-mac address mapping relation
    memcpy(&arp_table[arp_table_curr].ip_addr, &dst_protocol_addr, sizeof(dst_protocol_addr));
    memcpy(&arp_table[arp_table_curr].mac_addr, &dst_hardware_addr, sizeof(dst_hardware_addr));

    if (arp_table_size < 512)
        arp_table_size++;
    // Wrap around
    if (arp_table_curr >= 512)
        arp_table_curr = 0;
}

void arp_send_packet(mac_addr_t *dst_hardware_addr, ip_addr_t *dst_protocol_addr) {
    struct arp_packet *arp_packet = kmalloc(sizeof(*arp_packet));

    // Set source MAC address, IP address (hardcode the IP address as 10.2.2.3 until we really get one..)
    get_mac_addr(&arp_packet->src_hardware_addr);
    arp_packet->src_protocol_addr[0] = 10;
    arp_packet->src_protocol_addr[1] = 0;
    arp_packet->src_protocol_addr[2] = 2;
    arp_packet->src_protocol_addr[3] = 14;

    // Set destination MAC address, IP address
    memcpy(arp_packet->dst_hardware_addr, dst_hardware_addr, sizeof(*dst_hardware_addr));
    memcpy(arp_packet->dst_protocol_addr, dst_protocol_addr, sizeof(*dst_protocol_addr));

    // Set opcode
    arp_packet->opcode = htons(ARP_REQUEST);

    // Set lengths
    arp_packet->hardware_addr_len = 6;
    arp_packet->protocol_addr_len = 4;

    // Set hardware type
    arp_packet->hardware_type = htons(HARDWARE_TYPE_ETHERNET);

    // Set protocol = IPv4
    arp_packet->protocol = htons(ETHERNET_TYPE_IP);

    // Now send it with ethernet
    ethernet_send_packet(&broadcast_mac_address, arp_packet, sizeof(struct arp_packet), ETHERNET_TYPE_ARP);

    kfree(arp_packet);
}

void arp_lookup_add(mac_addr_t *ret_hardware_addr, ip_addr_t *ip_addr) {
    memcpy(&arp_table[arp_table_curr].ip_addr, ip_addr, sizeof(*ip_addr));
    memcpy(&arp_table[arp_table_curr].mac_addr, ret_hardware_addr, sizeof(*ret_hardware_addr));

    if (arp_table_size < 512)
        arp_table_size++;
    // Wrap around
    if (arp_table_curr >= 512)
        arp_table_curr = 0;
}

bool arp_lookup(mac_addr_t *ret_hardware_addr, ip_addr_t *ip_addr) {
    int i;
    for (i = 0; i < ARP_TABLE_SIZE; i++) {
        if (!memcmp(arp_table[i].ip_addr, ip_addr, sizeof(*ip_addr))) {
            memcpy(ret_hardware_addr, &arp_table[i].mac_addr, sizeof(*ret_hardware_addr));
            return true;
        }
    }
    return false;
}

static void arp_init() {
    ip_addr_t broadcast_ip;
    mac_addr_t broadcast_mac;

    memset(&broadcast_ip, 0xff, sizeof(broadcast_ip));
    memset(&broadcast_mac, 0xff, sizeof(broadcast_mac));

    arp_lookup_add(&broadcast_mac, &broadcast_ip);
}
DEFINE_INITCALL(arp_init, drivers);
