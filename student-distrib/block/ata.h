#ifndef ATA_H
#define ATA_H


#include "../irq.h"
#include "../lib/stdint.h"
#include "../lib/string.h"
#include "../vfs/file.h"
#include "../vfs/device.h"
#include "../multiboot.h"
#include "../errno.h"
#include "../initcall.h"
#include "../task/task.h"
#include "../task/sched.h"
#include "../task/session.h"
#include "../structure/list.h"
#include "../lib/io.h"
#include "../err.h"
#include "../errno.h"

#define ATA_PRIM_IRQ	14	// primary bus irq num
#define ATA_SEC_IRQ		15	// secondary bus irq num

#define PRIM_DATA_REG	0x1F0	///< primary IO port: 0x1F0 - 0x1F7
#define SEC_DATA_REG	0x170	///< primary IO port: 0x1F0 - 0x1F7
#define ERROR_FEAT_OFF	0x1	///< feature/error register
#define SEC_COUNT_OFF	0x2	///< sector count
#define SECTOR_NUM_OFF	0x3	///< sector number
#define LBA_LO_OFF		0x3	///< lba_lo
#define CYLIND_LOW_OFF	0x4	///< cylinder low
#define LBA_MID_OFF		0x4	///< lba_mid
#define CYLIND_HI_OFF	0x5	///< cylinder high
#define LBA_HI_OFF		0x5	///< lba_hi
#define DRIVE_HEAD_OFF	0x6	///< drive/head
#define STATUS_OFF		0x7

#define STAT_ERR	0x01	///< status bit err
#define STAT_DRQ	0x08	///< status bit drq
#define STAT_SRV	0x10	///< status bit srv
#define STAT_DF		0x20	///< status bit df
#define STAT_RDY	0x40	///< status bit rdy
#define STAT_BSY	0x80	///< status bit bsy

#define CMD_RESET			0x4		///< reset command
#define CMD_READ_SEC		0x20	///< sector read command
#define CMD_READ_SEC_EXT	0x24	///< sector read ext command
#define CMD_WRITE_SEC		0x30	///< sector write command
#define CMD_WRITE_SEC_EXT	0x34	///< sector write ext command
#define CMD_CACHE_FLUSH		0xE7	///< cache flush command
#define CMD_MASTER_ID		0xA0	///< master identify command
#define CMD_SLAVE_ID		0xA0	///< slave identify command
#define CMD_ID				0xEC	///< identify command

#define EIGHT_MB			8388608

struct ata_data {
	int32_t slave_bit;		///< master/slave
	struct task_struct *task;
	int32_t ata_base_reg;	///< primary/secondary
	int32_t stLBA;			///< lba offset
	int32_t prt_size;		///< partition size
};

static void ata_init();
static int32_t ata_close(struct file *file, struct inode *inode);
static int32_t ata_open(struct file *file, struct inode *inode);
static void ATA_handler();
static int32_t ata_should_read(struct ata_data* dev);
static int ata_read_28(uint32_t lba, char* buf, struct ata_data* dev);
static int32_t ata_read(struct file *file, char *buf, uint32_t nbytes);
static int ata_identify(int32_t ata_num);
static void soft_reset(struct ata_data* dev);
void io_delay(struct ata_data* dev);



#endif
