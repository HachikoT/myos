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

// eflags寄存器
//
// +-10-+--1-+--1--+--1--+--1-+--1-+--1-+-1-+--1-+---2--+--1-+--1-+--1-+--1-+--1-+--1-+-1-+--1-+-1-+--1-+-1-+--1-+
// | 0  | ID | VIP | VIF | AC | VM | RF | 0 | NT | IOPL | OF | DF | IF | TF | SF | ZF | 0 | AF | 0 | PF | 0 | CF |
// +----+----+-----+-----+----+----+----+---+----+------+----+----+----+----+----+----+---+----+---+----+---+----+
//
// CF：进位标志位。无符号数运算时，运算结果的最高有效位向前有借位或进位。
// PF：奇偶标志位。运算结果所有的bit中1的个数为偶数则为1，为奇数则为0。
// AF：辅助进位标志位。运算过程中看最后四位，不论长度为多少，最后四位向前有借位或者进位。
// ZF：零标志位。执行结果是否为0。
// SF：符号标志位。执行结果的符号位（最高位）。
// TF：调试标志位。当TF=1时，处理器每次只执行一条指令，即单步执行。
// IF：中断允许标志位。是否响应外部中断。可以通过sti和cli指令来开启和关闭。
// DF：方向标志位。在串处理指令中，每次操作后，如果DF=0，si、di递增；如果DF=1，si、di递减。可以通过std和cld指令来设置。
// OF：溢出标志位。记录了有符号运算的结果是否发生了溢出。
// IOPL：如果当前特权级小于或等于IOPL，则可以执行I/O操作，否则将出现一个保护性异常。IOPL只能由特权级为0的程序或任务来修改。
// NT：表示嵌套任务，当NT=0时，用堆栈中保存的值恢复EFLAGS、CS和EIP，从而实现返回；若NT=1，则通过任务切换实现中断返回。
// VM：表示虚拟8086模式，如果VM被置位且80386已处于保护模式下，则CPU切换到虚拟8086模式，此时，对段的任何操作又回到了实模式。
// RF：表示恢复标志(也叫重启标志)，与调试寄存器一起用于断点和单步操作。
// AC：表示对齐检查。当AC=1且CR0中的AM=1时，允许存储器进行地址对齐检查，若发现地址未对齐，将产生异常中断。
// VIF：表示虚拟中断标志。当VIF=1时，可以使用虚拟中断，当VIF=0时不能使用虚拟中断。该标志要和下面的VIP和CR4中的VME配合使用。
// VIP：表示虚拟中断挂起标志。当VIP=1时，VIF有效，VIP=0时VIF无效。
// ID：表示鉴别标志。该标志用来指示Pentium CPU是否支持CPUID的指令。

#define FL_CF 0x00000001     // Carry Flag
#define FL_PF 0x00000004     // Parity Flag
#define FL_AF 0x00000010     // Auxiliary carry Flag
#define FL_ZF 0x00000040     // Zero Flag
#define FL_SF 0x00000080     // Sign Flag
#define FL_TF 0x00000100     // Trap Flag
#define FL_IF 0x00000200     // Interrupt Flag
#define FL_DF 0x00000400     // Direction Flag
#define FL_OF 0x00000800     // Overflow Flag
#define FL_IOPL_0 0x00000000 // IOPL == 0
#define FL_IOPL_1 0x00001000 // IOPL == 1
#define FL_IOPL_2 0x00002000 // IOPL == 2
#define FL_IOPL_3 0x00003000 // IOPL == 3
#define FL_NT 0x00004000     // Nested Task
#define FL_RF 0x00010000     // Resume Flag
#define FL_VM 0x00020000     // Virtual 8086 mode
#define FL_AC 0x00040000     // Alignment Check
#define FL_VIF 0x00080000    // Virtual Interrupt Flag
#define FL_VIP 0x00100000    // Virtual Interrupt Pending
#define FL_ID 0x00200000     // ID flag

// 读取eflags寄存器的值
static inline uint32_t read_eflags(void)
{
    uint32_t eflags;
    __asm__ __volatile__("pushfl; popl %0"
                         : "=r"(eflags));
    return eflags;
}

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
