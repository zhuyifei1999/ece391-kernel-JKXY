#ifndef _PCI_H
#define _PCI_H
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
// https://wiki.osdev.org/PCI
// register	offset	bits 31-24	bits 23-16	bits 15-8	bits 7-0
// 00	00	Device ID	Vendor ID
// 01	04	Status	Command
// 02	08	Class code	Subclass	Prog IF	Revision ID
// 03	0C	BIST	Header type	Latency Timer	Cache Line Size
// 04	10	Base address #0 (BAR0)
// 05	14	Base address #1 (BAR1)
// 06	18	Base address #2 (BAR2)
// 07	1C	Base address #3 (BAR3)
// 08	20	Base address #4 (BAR4)
// 09	24	Base address #5 (BAR5)
// 0A	28	Cardbus CIS Pointer
// 0B	2C	Subsystem ID	Subsystem Vendor ID
// 0C	30	Expansion ROM base address
// 0D	34	Reserved	Capabilities Pointer
// 0E	38	Reserved
// 0F	3C	Max latency	Min Grant	Interrupt PIN	Interrupt Line

uint32_t pci_scan_device(uint16_t deviceID, uint16_t vendorID);
uint32_t pci_readl(uint32_t address);


#endif

