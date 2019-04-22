#include "pci.h"

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
            if( data == ((uint32_t)deviceID << 16 | (uint32_t)vendorID) ){
                return address;
            }
        }
    }
    return 0;
}

// static void pci_init(){
//     uint32_t rtl_addr = pci_scan_device(0x8139, 0x10EC);
//     printk("rtl_init %x \n", rtl_addr);
//     printk("0x80000000 data %x \n", pci_readl(0x80000000));
//     while(1);
// }


// DEFINE_INITCALL(pci_init, drivers);



