#include "../irq.h"
#include "../lib/stdint.h"
#include "../lib/string.h"
#include "../vfs/file.h"
#include "../vfs/device.h"
#include "../initcall.h"
#include "../task/task.h"
#include "../task/sched.h"
#include "../structure/list.h"
#include "../lib/io.h"
#include "../panic.h"
#include "../err.h"
#include "../errno.h"

#define ATA_PRIM_IRQ 14 // primary bus irq num
#define ATA_SEC_IRQ  15 // secondary bus irq num

#define PRIM_DATA_REG  0x1F0 ///< primary IO port: 0x1F0 - 0x1F7
#define SEC_DATA_REG   0x170 ///< primary IO port: 0x1F0 - 0x1F7
#define ERROR_FEAT_OFF 0x1   ///< feature/error register
#define SEC_COUNT_OFF  0x2   ///< sector count
#define SECTOR_NUM_OFF 0x3   ///< sector number
#define LBA_LO_OFF     0x3   ///< lba_lo
#define CYLIND_LOW_OFF 0x4   ///< cylinder low
#define LBA_MID_OFF    0x4   ///< lba_mid
#define CYLIND_HI_OFF  0x5   ///< cylinder high
#define LBA_HI_OFF     0x5   ///< lba_hi
#define DRIVE_HEAD_OFF 0x6   ///< drive/head
#define STATUS_OFF     0x7

#define STAT_ERR 0x01 ///< status bit err
#define STAT_DRQ 0x08 ///< status bit drq
#define STAT_SRV 0x10 ///< status bit srv
#define STAT_DF  0x20 ///< status bit df
#define STAT_RDY 0x40 ///< status bit rdy
#define STAT_BSY 0x80 ///< status bit bsy

#define CMD_RESET         0x4  ///< reset command
#define CMD_READ_SEC      0x20 ///< sector read command
#define CMD_READ_SEC_EXT  0x24 ///< sector read ext command
#define CMD_WRITE_SEC     0x30 ///< sector write command
#define CMD_WRITE_SEC_EXT 0x34 ///< sector write ext command
#define CMD_CACHE_FLUSH   0xE7 ///< cache flush command
#define CMD_MASTER_ID     0xA0 ///< master identify command
#define CMD_SLAVE_ID      0xB0 ///< slave identify command
#define CMD_ID            0xEC ///< identify command

#define EIGHT_MB            8388608

struct ata_data {
    int32_t slave_bit;    ///< master/slave
    int32_t ata_base_reg; ///< primary/secondary
    int32_t stLBA;        ///< lba offset
    int32_t prt_size;     ///< partition size
};

#define ATA_IRQ_PRIM 14
#define ATA_IRQ_SEC 15

static struct ata_data primary_driver_info = {
    .slave_bit = 0,
    .ata_base_reg = PRIM_DATA_REG,
    .prt_size = EIGHT_MB,
    .stLBA = 0,
};
static struct ata_data secondary_driver_info = {
    .slave_bit = 0,
    .ata_base_reg = SEC_DATA_REG,
    .prt_size = EIGHT_MB,
    .stLBA = 0,
};

static struct list ata_queue;
LIST_STATIC_INIT(ata_queue);

/*
The suggestion is to read the Status register FIVE TIMES,
and only pay attention to the value returned by the last one
*/
static void io_delay(struct ata_data *dev) {
    int32_t reg_offset = dev->ata_base_reg;
    // 4 consecutive read to implement 400ns delay
    inb(reg_offset + STATUS_OFF);
    inb(reg_offset + STATUS_OFF);
    inb(reg_offset + STATUS_OFF);
    inb(reg_offset + STATUS_OFF);
}

static void soft_reset(struct ata_data *dev) {
    int32_t reg_offset = dev->ata_base_reg;
    outb(CMD_RESET, reg_offset + STATUS_OFF);
    outb(0, reg_offset + STATUS_OFF);
    io_delay(dev);
    char stat = inb(reg_offset + STATUS_OFF);
    // check if BSY clear and RDY set
    while (!(stat & STAT_RDY) || (stat & STAT_BSY)) {
        stat = inb(reg_offset + STATUS_OFF);
    }
}

//__attribute__((unused))
static int ata_identify(struct ata_data *ata) {
    outb(0xA0, ata->ata_base_reg + DRIVE_HEAD_OFF);
    outb(0, ata->ata_base_reg + SEC_COUNT_OFF);
    outb(0, ata->ata_base_reg + SECTOR_NUM_OFF);
    outb(0, ata->ata_base_reg + LBA_MID_OFF);
    outb(0, ata->ata_base_reg + LBA_HI_OFF);
    outb(CMD_ID, ata->ata_base_reg + STATUS_OFF);
    uint8_t stat = inb(ata->ata_base_reg + STATUS_OFF);

    if (!stat)
        return -ENXIO;
    while (stat & STAT_BSY) {
        stat = inb(ata->ata_base_reg + STATUS_OFF);
    }
    while (!(stat & STAT_DRQ)) {
        if (stat &STAT_ERR)
            return -EIO;
        stat = inb(ata->ata_base_reg + STATUS_OFF);
    }

    uint16_t id_buf[256];
    asm volatile (
        "               \n\
        movl %1,%%edx   \n\
        movl $256,%%ecx \n\
        movl %0,%%edi   \n\
        rep insw        \n\
        "
        :
        : "r"(&id_buf), "r"(0x1f0)
        : "edx", "ecx", "edi"
    );
    int size = (id_buf[61]<<16) | id_buf[60];
    return size * 512;
}

static int32_t ata_should_read(struct ata_data *dev) {
    uint8_t status = inb(dev->ata_base_reg + STATUS_OFF);
    if (status & STAT_DF || status & STAT_ERR)
        return -EIO;
    return ((~status & STAT_BSY) && (status & STAT_DRQ));
}

static int ata_read_28(uint32_t lba, char *buf, struct ata_data *dev) {
    uint8_t ret_state;
    int32_t irq_ret;
    int32_t reg_offset = dev->ata_base_reg;
    int32_t slavebit = dev->slave_bit;

    list_insert_back(&ata_queue, current);
    current->state = TASK_UNINTERRUPTIBLE;
    while (list_peek_front(&ata_queue) != current) {
        schedule();
    }
    current->state = TASK_RUNNING;

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

    io_delay(dev);

    int32_t ret = 0;

    while (irq_ret == ata_should_read(dev)) {
        if (irq_ret < 0) {
            ret = irq_ret;
            goto out;
        } else if (irq_ret)
            break;
        current->state = TASK_UNINTERRUPTIBLE;
        schedule();
        current->state = TASK_RUNNING;
    }

    // read 512 bytes to buffer
    asm volatile (
        "               \n\
        movl %1,%%edx   \n\
        movl $256,%%ecx \n\
        movl %0,%%edi   \n\
        rep insw        \n\
        "
        :
        : "r"(buf), "r"(reg_offset)
        : "edx", "ecx", "edi"
    );

    io_delay(dev);
    ret_state = inb(reg_offset + STATUS_OFF);
    if (ret_state & STAT_DF || ret_state & STAT_ERR) {
        ret = -EIO;
        goto out;
    }

out:
    cli();
    if (list_peek_front(&ata_queue) != current)
        BUG();
    list_pop_front(&ata_queue);
    sti();

    return ret;
}

static char read_head_buf[512];
static int32_t ata_read(struct file *file, char *buf, uint32_t nbytes) {
    struct ata_data *ata = file->vendor;
    uint32_t off = file->pos;
    int32_t reg_offset = ata->ata_base_reg;
    int32_t byte_count = 0;

    uint8_t status = inb(reg_offset + STATUS_OFF);
    if (status | STAT_BSY || status & STAT_DRQ) {
        soft_reset(ata);
    }

    uint32_t sector_start = off/512;
    uint32_t sector_end = (nbytes + off)/512;

    if(sector_start > sector_end || sector_end > 0xFFFFFF )
        return -1;
    if (0 != ata_read_28( sector_start , read_head_buf, ata))
        return -1;
    if(sector_start == sector_end){
        memcpy(buf, read_head_buf + off % 512, nbytes);
        return nbytes;
    }
    memcpy(buf, read_head_buf + off % 512, 512 - off % 512);
    buf += 512 - off % 512;
    byte_count += 512 - off % 512;
    for(; sector_start < sector_end ; ++sector_start ){
        if (0 != ata_read_28( sector_start , buf, ata))
           return -1;
        buf += 512;
        byte_count += 512;   
    }

    if (0 != ata_read_28( sector_end , read_head_buf, ata))
        return -1;
    memcpy(buf, read_head_buf, (nbytes + off) % 512 );
    byte_count += (nbytes + off) % 512;

    return byte_count;
}

static int32_t ata_open(struct file *file, struct inode *inode) {
	
    if (MINOR(inode->rdev) == 0)
        file->vendor = &primary_driver_info;
    else if (MINOR(inode->rdev) == 1)
        file->vendor = &secondary_driver_info;
    else
        return -ENXIO;
	if(0==ata_identify(file->vendor))
		return ENXIO;
    return 0;
}
static int32_t ata_seek(struct file *file, int32_t offset, int32_t whence) {
    int32_t new_pos;
    uint32_t size = EIGHT_MB;

    // cases for whence
    switch (whence) {
    case SEEK_SET:
        new_pos = offset;
        break;
    case SEEK_CUR:
        new_pos = file->pos + offset;
        break;
    case SEEK_END:
        new_pos = size + offset;
        break;
    default:
        return -EINVAL;
    }

    // check validity of new_pos
    if (new_pos >= size || new_pos < 0)
        return -EINVAL;
    file->pos = new_pos;
    return new_pos;
}
static struct file_operations ata_dev_op = {
    .read    = &ata_read,
    //.write   = &ata_write,
    .open    = &ata_open,
    .seek    = &ata_seek,
};

static void ata_handler() {
    wake_up_process(list_peek_front(&ata_queue));
}

static void ata_init() {
    set_irq_handler(ATA_IRQ_PRIM, &ata_handler);
    set_irq_handler(ATA_IRQ_SEC, &ata_handler);

    // ATA has major device number 8
    register_dev(S_IFBLK, MKDEV(8, MINORMASK), &ata_dev_op);
}

DEFINE_INITCALL(ata_init, drivers);
