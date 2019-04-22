#include "pci.h"

#define RTL_8139_VENDOR_ID 0x10EC 
#define RTL_8139_DEVICE_ID 0x8139 
#define RTL_PORT_MAC     0x00
#define RTL_PORT_MAR     0x08
#define RTL_PORT_TXSTAT  0x10
#define RTL_PORT_TXBUF   0x20
#define RTL_PORT_RBSTART 0x30
#define RTL_PORT_CMD     0x37
#define RTL_PORT_RXPTR   0x38
#define RTL_PORT_RXADDR  0x3A
#define RTL_PORT_IMR     0x3C
#define RTL_PORT_ISR     0x3E
#define RTL_PORT_TCR     0x40
#define RTL_PORT_RCR     0x44
#define RTL_PORT_RXMISS  0x4C
#define RTL_PORT_CONFIG  0x52

#define RTL_IRQ          0x0A       // found in rtl pci config

#define RST (inb(ioaddr + 0x37) & 0x10)
static uint16_t ioaddr;
static uint8_t rx_buffer[8192 + 16];


static void rtl8139_handler(struct intr_info *info) {

}

 
static void rtl8139_init(){
    uint32_t rtl_pci_addr = pci_scan_device(RTL_8139_DEVICE_ID, RTL_8139_VENDOR_ID);
    if(rtl_pci_addr == 0){
        printk("rtl8139 not found! \n");
        return;
    }
    ioaddr = (uint16_t)pci_readl(rtl_pci_addr+0x10) & 0xFFFC;
    printk("rtl_init %x \n", rtl_pci_addr);
    printk("rtl_BAR0 %x \n", ioaddr );
    int i;
    for(i = 0; i<16; ++i){
        printk("rtl_config # %x: %8x \n", i, pci_readl(rtl_pci_addr+i*4) );
    }
    for(i = 0; i<6; ++i){
        printk("mac %2x \n", inb(ioaddr+i) );
    }
    //Turning on the RTL8139
    outb( 0x0, ioaddr + 0x52);
    //Software Reset!
    outb( 0x10, ioaddr + 0x37);
    while(RST);
    outl(rx_buffer, ioaddr + 0x30);
    //Set IMR + ISR
    outw(0x0005, ioaddr + 0x3C); 
    outl(0xf | (1 << 7), ioaddr + 0x44); // (1 << 7) is the WRAP bit, 0xf is AB+AM+APM+AAP
    outb(0x0C, ioaddr + 0x37); // Sets the RE and TE bits high



    printk("rtl8139  found! \n");
    while(1);
    set_irq_handler(RTL_IRQ, &rtl8139_handler);
}

DEFINE_INITCALL(rtl8139_init, drivers);
