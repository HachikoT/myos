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

static inline void outsl(uint32_t port, const void *addr, int cnt)
{
    __asm__ __volatile__(
        "cld;"
        "repne; outsl;"
        : "=S"(addr), "=c"(cnt)
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

// 读取eflags寄存器的值
static inline uint32_t read_eflags(void)
{
    uint32_t eflags;
    __asm__ __volatile__("pushfl; popl %0"
                         : "=r"(eflags));
    return eflags;
}

/* Eflags register */
#define FL_CF 0x00000001        // Carry Flag
#define FL_PF 0x00000004        // Parity Flag
#define FL_AF 0x00000010        // Auxiliary carry Flag
#define FL_ZF 0x00000040        // Zero Flag
#define FL_SF 0x00000080        // Sign Flag
#define FL_TF 0x00000100        // Trap Flag
#define FL_IF 0x00000200        // Interrupt Flag
#define FL_DF 0x00000400        // Direction Flag
#define FL_OF 0x00000800        // Overflow Flag
#define FL_IOPL_MASK 0x00003000 // I/O Privilege Level bitmask
#define FL_IOPL_0 0x00000000    //   IOPL == 0
#define FL_IOPL_1 0x00001000    //   IOPL == 1
#define FL_IOPL_2 0x00002000    //   IOPL == 2
#define FL_IOPL_3 0x00003000    //   IOPL == 3
#define FL_NT 0x00004000        // Nested Task
#define FL_RF 0x00010000        // Resume Flag
#define FL_VM 0x00020000        // Virtual 8086 mode
#define FL_AC 0x00040000        // Alignment Check
#define FL_VIF 0x00080000       // Virtual Interrupt Flag
#define FL_VIP 0x00100000       // Virtual Interrupt Pending
#define FL_ID 0x00200000        // ID flag

static inline uint32_t read_ebp(void)
{
    uint32_t ebp;
    __asm__ __volatile__("movl %%ebp, %0"
                         : "=r"(ebp));
    return ebp;
}

static inline void
lcr0(uintptr_t cr0)
{
    __asm__ __volatile__("mov %0, %%cr0" ::"r"(cr0)
                         : "memory");
}

static inline void lcr3(uintptr_t cr3)
{
    __asm__ __volatile__("mov %0, %%cr3" ::"r"(cr3)
                         : "memory");
}

static inline uintptr_t
rcr0(void)
{
    uintptr_t cr0;
    __asm__ __volatile__("mov %%cr0, %0"
                         : "=r"(cr0)::"memory");
    return cr0;
}

static inline uintptr_t
rcr1(void)
{
    uintptr_t cr1;
    __asm__ __volatile__("mov %%cr1, %0"
                         : "=r"(cr1)::"memory");
    return cr1;
}

static inline uintptr_t
rcr2(void)
{
    uintptr_t cr2;
    __asm__ __volatile__("mov %%cr2, %0"
                         : "=r"(cr2)::"memory");
    return cr2;
}

static inline uintptr_t rcr3(void)
{
    uintptr_t cr3;
    __asm__ __volatile__("mov %%cr3, %0"
                         : "=r"(cr3)::"memory");
    return cr3;
}

static inline void invlpg(void *addr)
{
    __asm__ __volatile__("invlpg (%0)" ::"r"(addr)
                         : "memory");
}

#endif /* !__LIBS_X86_H__ */
