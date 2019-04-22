#include "../irq.h"
#include "../lib/stdint.h"
#include "../lib/stdbool.h"
#include "../lib/string.h"
#include "../vfs/file.h"
#include "../vfs/device.h"
#include "../initcall.h"
#include "../task/task.h"
#include "../task/sched.h"
#include "../structure/list.h"
#include "../mm/kmalloc.h"
#include "../lib/io.h"
#include "../mutex.h"
#include "../panic.h"
#include "../err.h"
#include "../errno.h"

#define PCI_ADDRESS_PORT    0xCF8      // 32 bits register
#define PCI_DATA_PORT       0xCFC       // 32 bits register
#define PCI_BUS_MAX         256
#define PCI_DEVICE_MAX      32

// 31	        30 - 24	    23 - 16	    15 - 11	        10 - 8	        7 - 0
// Enable Bit	Reserved	Bus Number	Device Number	Function Number	Register Offset¹

#define PCI_ADDRESS(bus, device, func, offset)      \
    ( ((uint32_t)(bus)<< 16) | ((uint32_t)(device) << 11) \
    | ((uint32_t)(func) << 8) | ((uint32_t)(offset) & 0xfc) | 0x80000000  )

uint32_t pci_readl(uint32_t address){
    outl(address, PCI_ADDRESS_PORT);
    return inl(PCI_DATA_PORT);
}

uint32_t pci_scan_device(uint16_t deviceID, uint16_t vendorID){
    uint32_t bus, device, address, data;
    for(bus = 0; bus < PCI_BUS_MAX; ++bus){
        for(device = 0; device < PCI_DEVICE_MAX; ++device){
            address = PCI_ADDRESS(bus, device, 0, 0);
            data = pci_readl(address);
            printk("address %8x   data %8x\n", address, data);
            if( data != 0xffffffff){//}== ((uint32_t)deviceID << 16 | (uint32_t)vendorID) ){
                return address;
            }
        }
    }
    return ~0;
}

static void pci_init(){
    uint32_t rtl_addr = pci_scan_device(0x8139, 0x10EC);
    printk("rtl_init %x \n", rtl_addr);
    printk("0x80000000 data %x \n", pci_readl(0x80000000));
    while(1);
}




uint16_t pciConfigReadWord (uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;
    uint16_t tmp = 0;
 
    /* create configuration address as per Figure 1 */
    address = (uint32_t)((lbus << 16) | (lslot << 11) |
              (lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
 
    /* write out the address */
    outl(address, 0xCF8);
    /* read in the data */
    /* (offset & 2) * 8) = 0 will choose the first word of the 32 bits register */
    tmp = (uint16_t)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xffff);
    return (tmp);
}

uint16_t pciCheckVendor(uint8_t bus, uint8_t slot) {
    uint16_t vendor, device;
    /* try and read the first configuration register. Since there are no */
    /* vendors that == 0xFFFF, it must be a non-existent device. */
    if ((vendor = pciConfigReadWord(bus,slot,0,0)) != 0xFFFF) {
       device = pciConfigReadWord(bus,slot,0,2);
       printk("vendor %x  device %x \n", vendor, device );
    } 
    return (vendor);
}

 void checkAllBuses(void) {
     uint16_t bus;
     uint8_t device;
 
     for(bus = 0; bus < 256; bus++) {
         for(device = 0; device < 32; device++) {
            pciCheckVendor(bus, device);
            //printk("check vendor %x\n",pciConfigReadWord(bus,device,0,0));
         }
     }
    printk("0x80000000 data %x \n", pci_readl(0x80000000));
    while(1);
 }
DEFINE_INITCALL(checkAllBuses, drivers);



