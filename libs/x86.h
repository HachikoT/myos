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

// 用来描述gdt和idt和ldt表信息，packed表示紧凑模式，不进行内存对齐
struct dt_desc
{
    uint16_t dt_lim;  // Limit
    uint32_t dt_base; // Base address
} __attribute__((packed));

// 将中断向量表地址记录到IDTR寄存器
static inline void lidt(struct dt_desc *pd)
{
    __asm__ __volatile__("lidt (%0)" ::"r"(pd));
}

// 设置任务状态段选择子
static inline void ltr(uint16_t sel)
{
    __asm__ __volatile__("ltr %0" ::"r"(sel));
}

#define do_div(n, base) ({                               \
    unsigned long __upper, __low, __high, __mod, __base; \
    __base = (base);                                     \
    __asm__(""                                           \
            : "=a"(__low), "=d"(__high)                  \
            : "A"(n));                                   \
    __upper = __high;                                    \
    if (__high != 0)                                     \
    {                                                    \
        __upper = __high % __base;                       \
        __high = __high / __base;                        \
    }                                                    \
    __asm__("divl %2"                                    \
            : "=a"(__low), "=d"(__mod)                   \
            : "rm"(__base), "0"(__low), "1"(__upper));   \
    __asm__(""                                           \
            : "=A"(n)                                    \
            : "a"(__low), "d"(__high));                  \
    __mod;                                               \
})

#endif /* !__LIBS_X86_H__ */
