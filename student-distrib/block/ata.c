#include "ata.h"

#define ATA_IRQ_PRIM 14
#define ATA_IRQ_SEC 15


static struct ata_data driver_info;

/*
The suggestion is to read the Status register FIVE TIMES, 
and only pay attention to the value returned by the last one
*/
void io_delay(){
	int32_t reg_offset = driver_info.ata_base_reg;
	// 4 consecutive read to implement 400ns delay
	inb(reg_offset + STATUS_OFF);
	inb(reg_offset + STATUS_OFF);
	inb(reg_offset + STATUS_OFF);
	inb(reg_offset + STATUS_OFF);
}

static void soft_reset(struct ata_data* dev){
	int32_t reg_offset = dev->ata_base_reg;
	outb(CMD_RESET, reg_offset + STATUS_OFF);
	outb(0, reg_offset + STATUS_OFF);
	io_delay(dev);
	char stat = inb(reg_offset + STATUS_OFF);
	// check if BSY clear and RDY set
	while(!(stat & STAT_RDY) || (stat & STAT_BSY)){
		stat = inb(reg_offset + STATUS_OFF);
	}
}

static int32_t ata_read(struct file *file, char *buf, uint32_t nbytes){
    int32_t block_count = nbytes%512 ? nbytes/512 + 1 : nbytes/512;
    uint32_t off = file->pos;
	int32_t reg_offset = driver_info.ata_base_reg;
    int32_t byte_count = 0;

    if(block_count<0){
        software_reset(&driver_info);
    }

    if(block_count > 0x3FFFFF)
        return -1;

    uint8_t status = inb(reg_offset + STATUS_OFF);
	if(status | STAT_BSY || status & STAT_DRQ){
		soft_reset(&driver_info);
	}

    int32_t lba = driver_info.stLBA + off/512;

    for(int i = 0; i < block_count; i++){
        if(0 != ata_read_28( (lba+i) , buf ))
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
    uint8_t ret_state;
    int32_t irq_ret;
	int32_t reg_offset = driver_info.ata_base_reg;
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
	outb(CMD_READ_SEC, reg_offset + STATUS_OFF);

    io_delay();
    while(irq_ret = ata_should_read()){
        if(irq_ret == -1)
            return -1;
        else if (irq_ret == 1)
            break;

        schedule();
    }
    current->state = TASK_RUNNING;


    int16_t* buffer = (int16_t*)buf;
	// read 512 bytes to buffer
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

    io_delay();
    ret_state = inb(reg_offset + STATUS_OFF);
	if(ret_state & STAT_DF || ret_state & STAT_ERR)
		return -1;

	return 0;

}

static int32_t ata_should_read(){
    uint8_t status = inb(driver_info.ata_base_reg + STATUS_OFF);
    if(status & STAT_DF || status & STAT_ERR)
        return -1;
    return ((!status & STAT_BSY) & (status & STAT_DRQ));
}

static void ATA_handler( uint8_t* buf){
    wake_up_process()
}

static void set_driver_data(struct ata_data* dev_info){
	// initialize driver data
	driver_info.slave_bit = dev_info->slave_bit;
	driver_info.ata_base_reg =dev_info->ata_base_reg;
	driver_info.prt_size = dev_info->prt_size;
	driver_info.stLBA = dev_info->stLBA;
}

static void init_ATA() {
    set_irq_handler(ATA_IRQ_PRIM, &ATA_handler);
   // set_irq_handler(ATA_IRQ_SEC, &ATA_handler);
    driver_info.slave_bit = 0;
	driver_info.ata_base_reg = PRIM_DATA_REG;
	driver_info.prt_size = EIGHT_MB;
	driver_info.stLBA = 0;

}
DEFINE_INITCALL(init_ATA, drivers);
