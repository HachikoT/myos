#include "libs/defs.h"
#include "libs/x86.h"
#include "libs/elf.h"

const unsigned SECTOR_SIZE = 512;           // 扇区大小，512字节

// 等待磁盘ready
static void wait_disk(void) {
    while ((inb(0x1F7) & 0xC0) != 0x40) {}
}

// 读取扇区编号为secno的扇区的数据到目的内存
static void read_sector(void *dst, uint32_t secno) {
    // 等待磁盘ready
    wait_disk();

    // 传输控制信息，表明要读的扇区
    outb(0x1F2, 1);                             // count = 1
    outb(0x1F3, secno & 0xFF);
    outb(0x1F4, (secno >> 8) & 0xFF);
    outb(0x1F5, (secno >> 16) & 0xFF);
    outb(0x1F6, ((secno >> 24) & 0xF) | 0xE0);
    outb(0x1F7, 0x20);                          // cmd 0x20 - read sectors

    // 等待磁盘ready
    wait_disk();

    // 读取该扇区，insl指令每次读取4字节，所以一共需要读取SECTOR_SIZE / 4次
    insl(0x1F0, dst, SECTOR_SIZE / 4);
}

// 从内核数据的offset偏移处读取count个字节到虚拟内存地址va，由于是按照扇区读取的，所以总数可能大于count
static void read_seg(uintptr_t va, uint32_t count, uint32_t offset) {
    // 调整边界va，按扇区大小对齐
    uintptr_t end_va = va + count;
    va -= offset % SECTOR_SIZE;

    // 计算出offset对应所处的扇区，需要加1，因为内核代码是从扇区1开始存储的
    uint32_t secno = (offset / SECTOR_SIZE) + 1;

    // 加载到内存
    for (; va < end_va; va += SECTOR_SIZE, secno++) {
        read_sector((void *)va, secno);
    }
}

// bootmain入口函数，加载elf格式的内核代码
void boot_main(void) {
    // elf头存储在0x10000处
    struct elf_header *elf_hdr = ((struct elf_header *)0x10000);

    // 读取第1页（4K）内核数据
    read_seg((uintptr_t)elf_hdr, SECTOR_SIZE * 8, 0);

    // 判断是否位合法elf格式
    if (elf_hdr->e_magic != ELF_MAGIC) {
        goto bad;
    }

    // 加载每一个程序段（忽略ph flags）
    struct prog_header *ph, *end_ph;
    ph = (struct prog_header *)((uintptr_t)elf_hdr + elf_hdr->e_phoff);
    end_ph = ph + elf_hdr->e_phnum;
    for (; ph < end_ph; ph++) {
        read_seg(ph->p_va & 0xFFFFFF, ph->p_memsz, ph->p_offset);
    }

    // 执行内核入口函数
    ((void (*)(void))(elf_hdr->e_entry & 0xFFFFFF))();

bad:
    outw(0x8A00, 0x8A00);
    outw(0x8A00, 0x8E00);

    // 死循环
    while (true) {}
}
