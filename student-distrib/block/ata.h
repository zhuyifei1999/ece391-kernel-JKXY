#ifndef ATA_H
#define ATA_H

#include "../lib/stdint.h"
#include "../lib/string.h"
#include "../vfs/file.h"
#include "../vfs/device.h"
#include "../multiboot.h"
#include "../errno.h"
#include "../initcall.h"


#define ATA_PRIM_IRQ	14	// primary bus irq num
#define ATA_SEC_IRQ		15	// secondary bus irq num

#define STAT_ERR_BIT	0x01	///< status bit err
#define STAT_DRQ_BIT	0x08	///< status bit drq
#define STAT_SRV_BIT	0x10	///< status bit srv
#define STAT_DF_BIT		0x20	///< status bit df
#define STAT_RDY_BIT	0x40	///< status bit rdy
#define STAT_BSY_BIT	0x80	///< status bit bsy



#define STATUS_OFF		0x7

struct ata_data {
	int32_t slave_bit;		///< master/slave
	int32_t io_base_reg;	///< primary/secondary
	int32_t stLBA;			///< lba offset
	int32_t prt_size;		///< partition size
};



#endif