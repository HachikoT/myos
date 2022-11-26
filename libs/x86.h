#ifndef __LIBS_X86_H__
#define __LIBS_X86_H__

#include "libs/defs.h"

// 从指定IO端口读取一个字节数据
static inline uint8_t inb(uint16_t port)
{
    uint8_t data;
    __asm__ __volatile__("inb %1, %0"
                         : "=a"(data)
                         : "d"(port));
    return data;
}

// 写一个字节数据到指定IO端口
static inline void outb(uint16_t port, uint8_t data)
{
    __asm__ __volatile__("outb %0, %1" ::"a"(data), "d"(port));
}

// 写两个字节数据到指定IO端口
static inline void outw(uint16_t port, uint16_t data)
{
    __asm__ __volatile__("outw %0, %1" ::"a"(data), "d"(port));
}

// 从指定端口读取cnt次4字节的数据到指定的内存地址
static inline void insl(uint32_t port, void *addr, int cnt)
{
    __asm__ __volatile__(
        "cld;"
        "repne; insl;"
        : "=D"(addr), "=c"(cnt)
        : "d"(port), "0"(addr), "1"(cnt)
        : "memory", "cc");
}

// 使能中断
static inline void sti(void)
{
    __asm__ __volatile__("sti");
}

// 禁止中断
static inline void cli(void)
{
    __asm__ __volatile__("cli");
}

/* Pseudo-descriptors used for LGDT, LLDT(not used) and LIDT instructions. */
// lgdt lidt命令的参数格式，末尾的packed表示紧凑模式，不用内存对齐导致格式对不上
struct pseudo_desc
{
    uint16_t pd_lim;  // Limit
    uint32_t pd_base; // Base address
} __attribute__((packed));

// 将中断向量表地址记录到IDTR寄存器
static inline void lidt(struct pseudo_desc *pd)
{
    __asm__ __volatile__("lidt (%0)" ::"r"(pd));
}

#endif /* !__LIBS_X86_H__ */
