#ifndef __LIBS_DESCRIPTOR_H__
#define __LIBS_DESCRIPTOR_H__

// 普通段类型相关位
#define STA_X 0x8 // Executable segment
#define STA_E 0x4 // Expand down (non-executable only)
#define STA_C 0x4 // Conforming code segment (executable only)
#define STA_W 0x2 // Writeable (non-executable only)
#define STA_R 0x2 // Readable (executable only)
#define STA_A 0x1 // Accessed

// null segment
#define SEG_NULL_ASM \
    .word 0, 0;      \
    .byte 0, 0, 0, 0

// 段描述符结构
//
// 高32位：
// +-----8------+-1-+--1--+-1-+--1--+------4------+-1-+--2--+-1-+--4---+-----8------+
// | base 31:24 | G | D/B | L | AVL | limit 19:16 | P | DPL | S | type | base 23:16 |
// +------------+---+-----+---+-----+-------------+---+-----+---+------+------------+
// 低32位：
// +-----------------------16---------------------+--------------16-----------------+
// |                   base 15:0                  |           limit 15:0            |
// +----------------------------------------------+---------------------------------+
//
// limit：段限长
// base：段基址
// type：和S搭配，表示系统段的类型，或者普通段的属性
// S：为0表示为系统段，为1表示为普通段
// DPL：段特权级
// P：存在位，如果这一位为0，则此描述符是非法的
// AVL：系统软件可用位（留给操作系统自己定义）
// L：是否为64位代码段
// D/B：0表示16位模式，1为32位模式。代码段为0则使用寄存器%ip，为1使用%eip。栈段为0使用寄存器%sp，为1使用%esp
// G：段限长的颗粒度，为0表示1B，段限长最大1MB，1表示4KB，段限长最大4GB

#define SEG_DESC_ASM(type, base, lim)               \
    .word(((lim) >> 12) & 0xffff), ((base)&0xffff); \
    .byte(((base) >> 16) & 0xff), (0x90 | (type)),  \
        (0xC0 | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)

#ifndef __ASSEMBLER__

struct seg_desc
{
    unsigned sd_lim_15_0 : 16;  // low bits of segment limit
    unsigned sd_base_15_0 : 16; // low bits of segment base address
    unsigned sd_base_23_16 : 8; // middle bits of segment base address
    unsigned sd_type : 4;       // segment type (see STS_ constants)
    unsigned sd_s : 1;          // 0 = system, 1 = application
    unsigned sd_dpl : 2;        // descriptor Privilege Level
    unsigned sd_p : 1;          // present
    unsigned sd_lim_19_16 : 4;  // high bits of segment limit
    unsigned sd_avl : 1;        // unused (available for software use)
    unsigned sd_rsv1 : 1;       // reserved
    unsigned sd_db : 1;         // 0 = 16-bit segment, 1 = 32-bit segment
    unsigned sd_g : 1;          // granularity: limit scaled by 4K when set
    unsigned sd_base_31_24 : 8; // high bits of segment base address
};

#define SEG_NULL \
    (struct seg_desc) { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }

#define SEG_DESC(type, base, lim, dpl)              \
    (struct seg_desc)                               \
    {                                               \
        ((lim) >> 12) & 0xffff, (base)&0xffff,      \
            ((base) >> 16) & 0xff, type, 1, dpl, 1, \
            (unsigned)(lim) >> 28, 0, 0, 1, 1,      \
            (unsigned)(base) >> 24                  \
    }

#define SEG_1M_DESC(type, base, lim, dpl)           \
    (struct seg_desc)                               \
    {                                               \
        (lim) & 0xffff, (base)&0xffff,              \
            ((base) >> 16) & 0xff, type, 1, dpl, 1, \
            (unsigned)(lim) >> 16, 0, 0, 1, 0,      \
            (unsigned)(base) >> 24                  \
    }

#endif // __ASSEMBLER__

#endif // __LIBS_DESCRIPTOR_H__
