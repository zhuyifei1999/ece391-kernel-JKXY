#include "ata.h"

#define ATA_IRQ_PRIM 14
#define ATA_IRQ_SEC 15


static struct ata_data driver_info;

static int32_t ata_read(struct file *file, char *buf, uint32_t nbytes){
    int32_t block_count = nbytes%512 ? nbytes/512 + 1 : nbytes/512;
    uint32_t off = file->pos;
	int32_t reg_offset = driver_info.io_base_reg;
    int32_t byte_count = 0;

    if(block_count<0){
        software_reset(&driver_info)
    }

    if(block_count > 0x3FFFFF)
        return -1;

    uint8_t status = inb(reg_offset + STATUS_OFF);
	if(status | STAT_BSY_BIT || status & STAT_DRQ_BIT){
		soft_reset(&driver_info);
	}

    int32_t lba = driver_info.stLBA + off/512;

    for(int i = 0; i < block_count; i++){
        if(0 != ata_read_28( (lba+i) , buf))
            return -1;
        buf+=512;
        byte_count+=512;
    }

    return byte_count;
}

static int32_t initrd_read(struct file *file, char *buf, uint32_t nbytes) {
    struct initrd_entry *metadata = file->vendor;
    int32_t max_nbytes = metadata->size - file->pos;
    // check whether specified bytes is greater than maximum
    if (nbytes > max_nbytes)
        nbytes = max_nbytes;
    // copy nbytes of file into buffer
    memcpy(buf, metadata->start_addr + file->pos, nbytes);
    return nbytes;
}

static int ata_read_28(uint32_t lba, uint8_t* buf, struct ata_data* dev){

	int32_t reg_offset = driver_info.io_base_reg;
	int32_t slavebit = driver_info.slave_bit;

	outb(0xE0 | (slavebit << 4) | ((lba >> 24) & 0x0F), reg_offset + DRIVE_HEAD_OFF);
	outb(0x00, reg_offset + ERROR_FEAT_OFF);
	// write sector count to port
	outb(1, reg_offset + SEC_COUNT_OFF);
	// write lba to port
	outb((uint8_t)lba, reg_offset + LBA_LO_OFF);
	outb((uint8_t)(lba>>8), reg_offset + LBA_MID_OFF);
	outb((uint8_t)(lba>>16), reg_offset + LBA_HI_OFF);
	// send read command
	outb(CMD_READ_SEC, reg_offset + STATUS_CMD_OFF);

    while (!ata_should_read(ata))
        schedule();
    current->state = TASK_RUNNING;
}


static void ATA_handler( uint8_t* buf){
	asm volatile (
			"								\n\
			movl	%1, %%edx				\n\
			movl	$256, %%ecx				\n\
			movl	%0, %%edi				\n\
			rep insw						\n\
			"
			:
			: "r"(buffer), "r"(reg_offset)
			: "%edx", "%ecx", "%edi"
	);
}

static void set_driver_data(struct ata_data* dev_info){
	// initialize driver data
	driver_info.slave_bit = dev_info->slave_bit;
	driver_info.io_base_reg =dev_info->io_base_reg;
	driver_info.prt_size = dev_info->prt_size;
	driver_info.stLBA = dev_info->stLBA;
}

static void init_ATA() {
    set_irq_handler(ATA_IRQ_PRIM, &ATA_handler);
    set_irq_handler(ATA_IRQ_SEC, &ATA_handler);
    driver_info.slave_bit = 0;
	driver_info.io_base_reg = 0x1F0;
	driver_info.prt_size = 8388608;
	driver_info.stLBA = 0;

}
DEFINE_INITCALL(init_ATA, drivers);
