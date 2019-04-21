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

#define ATA_PRIM_IRQ 14 // primary bus irq num
#define ATA_SEC_IRQ  15 // secondary bus irq num

#define PRIM_DATA_REG  0x1F0 // primary IO port: 0x1F0 - 0x1F7
#define SEC_DATA_REG   0x170 // primary IO port: 0x170 - 0x177

#define ERROR_FEAT_OFF 0x1   // feature/error register
#define SEC_COUNT_OFF  0x2   // sector count
#define SECTOR_NUM_OFF 0x3   // sector number
#define LBA_LO_OFF     0x3   // lba_lo
#define CYLIND_LOW_OFF 0x4   // cylinder low
#define LBA_MID_OFF    0x4   // lba_mid
#define CYLIND_HI_OFF  0x5   // cylinder high
#define LBA_HI_OFF     0x5   // lba_hi
#define DRIVE_HEAD_OFF 0x6   // drive/head
#define STATUS_OFF     0x7
#define COMMAND_OFF    0x7
#define ALTERNATE_STAT 0x206

#define STAT_ERR 0x01 // status bit err
#define STAT_DRQ 0x08 // status bit drq
#define STAT_SRV 0x10 // status bit srv
#define STAT_DF  0x20 // status bit df
#define STAT_RDY 0x40 // status bit rdy
#define STAT_BSY 0x80 // status bit bsy

#define CMD_RESET         0x4  // reset command
#define CMD_READ_SEC      0x20 // sector read command
#define CMD_READ_SEC_EXT  0x24 // sector read ext command
#define CMD_WRITE_SEC     0x30 // sector write command
#define CMD_WRITE_SEC_EXT 0x34 // sector write ext command
#define CMD_CACHE_FLUSH   0xE7 // cache flush command
#define CMD_MASTER_ID     0xA0 // master identify command
#define CMD_SLAVE_ID      0xB0 // slave identify command
#define CMD_ID            0xEC // identify command

#define SECTOR_SIZE      512

//credit: https://github.com/ilufang/saenaios/blob/master/student-distrib/atadriver/ata.c

struct ata_data {
    int32_t slave_bit;    // master/slave
    int32_t ata_base_reg; // primary/secondary
    int32_t prt_size;     // partition size
};

#define ATA_IRQ_PRIM 14
#define ATA_IRQ_SEC 15

// initialize the driver info struct
static struct ata_data primary_master = {
    .slave_bit    = 0,
    .ata_base_reg = PRIM_DATA_REG,
    .prt_size     = 0,
};
static struct ata_data primary_slave = {
    .slave_bit    = 1,
    .ata_base_reg = PRIM_DATA_REG,
    .prt_size     = 0,
};
static struct ata_data secondary_master = {
    .slave_bit    = 0,
    .ata_base_reg = SEC_DATA_REG,
    .prt_size     = 0,
};
static struct ata_data secondary_slave = {
    .slave_bit    = 1,
    .ata_base_reg = SEC_DATA_REG,
    .prt_size     = 0,
};

static struct mutex ata_mutex;
MUTEX_STATIC_INIT(ata_mutex);

static struct task_struct *in_service;

/*
The suggestion is to read the Status register FIVE TIMES,
and only pay attention to the value returned by the last one
*/
/*
 *   io_delay
 *   DESCRIPTION: read status reg for four times to gain a 400ns's delay
 *   INPUTS: struct ata_data *dev
 */
static void io_delay(struct ata_data *dev) {
    int32_t reg_offset = dev->ata_base_reg;
    // 4 consecutive read to implement 400ns delay
    inb(reg_offset + ALTERNATE_STAT);
    inb(reg_offset + ALTERNATE_STAT);
    inb(reg_offset + ALTERNATE_STAT);
    inb(reg_offset + ALTERNATE_STAT);
}

// FIXME: Non-existent drives don't show 0 as expected
/*
 *   ata_identify
 *   DESCRIPTION: identitfy the ata hard disk. check if it is exist and get the size of it
 *   INPUTS: struct ata_data *dev
 *   OUTPUT: the size of disk
 */
static int ata_identify(struct ata_data *ata) {
    if (ata->prt_size) // Already identified
        return ata->prt_size;

    outb(0xA0, ata->ata_base_reg + DRIVE_HEAD_OFF);
    outb(0, ata->ata_base_reg + SEC_COUNT_OFF);
    outb(0, ata->ata_base_reg + SECTOR_NUM_OFF);
    outb(0, ata->ata_base_reg + LBA_MID_OFF);
    outb(0, ata->ata_base_reg + LBA_HI_OFF);
    outb(CMD_ID, ata->ata_base_reg + COMMAND_OFF);
    uint8_t stat = inb(ata->ata_base_reg + ALTERNATE_STAT);

    // check if the feedback of ata is valid
    if (!stat)
        return -ENXIO;
    while (stat & STAT_BSY) {
        stat = inb(ata->ata_base_reg + ALTERNATE_STAT);
    }
    while (!(stat & STAT_DRQ)) {
        if (stat & STAT_ERR)
            return -EIO;
        stat = inb(ata->ata_base_reg + ALTERNATE_STAT);
    }

    // copy the content from io port for 256 times
    uint16_t id_buf[256];
    asm volatile (
        "               \n\
        movl %1,%%edx   \n\
        movl $256,%%ecx \n\
        movl %0,%%edi   \n\
        rep insw        \n\
        "
        :
        : "r"(&id_buf), "r"(ata->ata_base_reg)
        : "edx", "ecx", "edi"
    );

    // the 60,61 byte contain the data of disk size
    int size = (id_buf[61]<<16) | id_buf[60];
    ata->prt_size = size * SECTOR_SIZE;
    return ata->prt_size;
}

/*
 *   ata_should_read
 *   DESCRIPTION: check if the status reg of ata to see if it is ready to read/write
 *   INPUTS: struct ata_data *dev
 *   OUTPUT: check code
 */
static int32_t ata_should_read(struct ata_data *dev) {
    uint8_t status = inb(dev->ata_base_reg + STATUS_OFF);
    if (status & STAT_DF || status & STAT_ERR)
        return -EIO;
    return (!(status & STAT_BSY) && (status & STAT_DRQ));
}

/*
 *   ata_read_28
 *   DESCRIPTION: read data from ata in 28bit mode
 *   INPUTS: uint32_t lba, char *buf, struct ata_data *dev
 *   OUTPUT: check code
 */
static int ata_read_28(uint32_t lba, char *buf, struct ata_data *dev) {
    uint8_t ret_state;
    int32_t reg_offset = dev->ata_base_reg;
    int32_t slavebit = dev->slave_bit;

    outb(0xE0 | (slavebit << 4) | ((lba >> 24) & 0x0F), reg_offset + DRIVE_HEAD_OFF);
    outb(0x00, reg_offset + ERROR_FEAT_OFF);
    // write sector count to port
    outb(1, reg_offset + SEC_COUNT_OFF);
    // write lba to port
    outb((uint8_t)lba, reg_offset + LBA_LO_OFF);
    outb((uint8_t)(lba>>8), reg_offset + LBA_MID_OFF);
    outb((uint8_t)(lba>>16), reg_offset + LBA_HI_OFF);
    // send read command
    outb(CMD_READ_SEC, reg_offset + COMMAND_OFF);

    io_delay(dev);
    
    // wait ata interrupt and then handle the packet
    while (true) {
        int32_t irq_ret = ata_should_read(dev);
        if (irq_ret < 0)
            return irq_ret;
        else if (irq_ret)
            break;

        current->state = TASK_UNINTERRUPTIBLE;
        schedule();
        current->state = TASK_RUNNING;
    }

    io_delay(dev);

    // read sector to buffer
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
    ret_state = inb(reg_offset + ALTERNATE_STAT);
    if (ret_state & STAT_DF || ret_state & STAT_ERR)
        return -EIO;

    return 0;
}

/*
 *   ata_read_28
 *   DESCRIPTION: read data from ata
 *   INPUTS: uint32_t lba, char *buf, struct ata_data *dev
 *   OUTPUT: the size of data it read
 */
static int32_t ata_read(struct file *file, char *buf, uint32_t nbytes) {
    struct ata_data *ata = file->vendor;
    uint32_t pos = file->pos;

    if (nbytes > ata->prt_size - pos)
        nbytes = ata->prt_size - pos;
    if (!nbytes)
        return 0;

    // malloc the data buf
    char *read_head_buf = kmalloc(SECTOR_SIZE);
    if (!read_head_buf)
        return -ENOMEM;

    // use mutex to resist other processes
    mutex_lock_uninterruptable(&ata_mutex);
    in_service = current;

    int32_t byte_count = 0;
    int32_t ret;
    while (nbytes) {
        uint32_t sector_num = pos / SECTOR_SIZE;
        uint32_t sector_off = pos % SECTOR_SIZE;
        // call ata_read_28 to read data
        ret = ata_read_28(sector_num, read_head_buf, ata);

        // if it read all
        if (ret)
            goto out;

        uint32_t inner_nbytes = SECTOR_SIZE - sector_off;
        if (inner_nbytes > nbytes)
            inner_nbytes = nbytes;

        memcpy(buf, read_head_buf + sector_off, inner_nbytes);

        // record the reading info
        nbytes -= inner_nbytes;
        byte_count += inner_nbytes;
        buf += inner_nbytes;
        pos += inner_nbytes;
    }

    ret = byte_count;

out:
    // free the malloced buf
    kfree(read_head_buf);

    file->pos = pos;
    in_service = NULL;

    // unlock mutex
    mutex_unlock(&ata_mutex);

    return ret;
}

/*
 *   ata_open
 *   DESCRIPTION: open the system, record the driver struct into vendor
 *   INPUTS: struct file *file, struct inode *inode
 *   OUTPUT: check code
 */
static int32_t ata_open(struct file *file, struct inode *inode) {
    switch (MINOR(inode->rdev)) {
    // open according to the inode's dev num
    case 0:
        file->vendor = &primary_master;
        break;
    case 1:
        file->vendor = &primary_slave;
        break;
    case 2:
        file->vendor = &secondary_master;
        break;
    case 3:
        file->vendor = &secondary_slave;
        break;
    default:
        return -ENXIO;
    }

    // if the hardware is error
	if (!ata_identify(file->vendor))
        return -ENXIO;

    return 0;
}

/*
 *   ata_seek
 *   DESCRIPTION: locate the offset
 *   INPUTS: struct file *file, int32_t offset, int32_t whence
 *   OUTPUT: check code
 */
static int32_t ata_seek(struct file *file, int32_t offset, int32_t whence) {
    int32_t new_pos;
    struct ata_data *ata;
    ata = file->vendor;

    uint32_t size = ata->prt_size;

    mutex_lock_uninterruptable(&ata_mutex);

    int32_t ret;

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
        ret = -EINVAL;
        goto out;
    }

    // check validity of new_pos
    if (new_pos >= size || new_pos < 0) {
        ret = -EINVAL;
        goto out;
    }

    file->pos = new_pos;
    ret = new_pos;

out:
    mutex_unlock(&ata_mutex);

    return ret;
}

static struct file_operations ata_dev_op = {
    .read    = &ata_read,
    //.write   = &ata_write,
    .open    = &ata_open,
    .seek    = &ata_seek,
};

/*
 *   ata_handler
 *   DESCRIPTION: wake up the read_28
 */
static void ata_handler() {
    if (in_service)
        wake_up_process(in_service);
}

/*
 *   ata_init
 *   DESCRIPTION: initialize the ata
 */
static void ata_init() {
    set_irq_handler(ATA_IRQ_PRIM, &ata_handler);
    set_irq_handler(ATA_IRQ_SEC, &ata_handler);

    // ATA has major device number 8
    register_dev(S_IFBLK, MKDEV(8, MINORMASK), &ata_dev_op);
}

DEFINE_INITCALL(ata_init, drivers);
