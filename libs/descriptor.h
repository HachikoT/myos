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

#endif /* !__LIBS_DESCRIPTOR_H__ */
