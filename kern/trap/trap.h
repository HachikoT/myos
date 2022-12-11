#ifndef __KERN_TRAP_TRAP_H__
#define __KERN_TRAP_TRAP_H__

#include "libs/defs.h"

/* Trap Numbers */

/* Processor-defined: */
#define T_DIVIDE 0 // divide error
#define T_DEBUG 1  // debug exception
#define T_NMI 2    // non-maskable interrupt
#define T_BRKPT 3  // breakpoint
#define T_OFLOW 4  // overflow
#define T_BOUND 5  // bounds check
#define T_ILLOP 6  // illegal opcode
#define T_DEVICE 7 // device not available
#define T_DBLFLT 8 // double fault
// #define T_COPROC                9    // reserved (not used since 486)
#define T_TSS 10   // invalid task switch segment
#define T_SEGNP 11 // segment not present
#define T_STACK 12 // stack exception
#define T_GPFLT 13 // general protection fault
#define T_PGFLT 14 // page fault
// #define T_RES                15    // reserved
#define T_FPERR 16   // floating point error
#define T_ALIGN 17   // aligment check
#define T_MCHK 18    // machine check
#define T_SIMDERR 19 // SIMD floating point error

#define T_SYSCALL 0x80 // SYSCALL, ONLY FOR THIS PROJ

/* *
 * These are arbitrarily chosen, but with care not to overlap
 * processor defined exceptions or interrupt vectors.
 * */
#define T_SWITCH_TOU 120 // user/kernel switch
#define T_SWITCH_TOK 121 // user/kernel switch

// pushal指令压入的寄存器数据
struct pushal_regs
{
    uint32_t reg_edi;
    uint32_t reg_esi;
    uint32_t reg_ebp;
    uint32_t reg_esp;
    uint32_t reg_ebx;
    uint32_t reg_edx;
    uint32_t reg_ecx;
    uint32_t reg_eax;
};

struct trap_frame
{
    struct pushal_regs tf_regs;
    uint16_t tf_gs;
    uint16_t tf_padding0;
    uint16_t tf_fs;
    uint16_t tf_padding1;
    uint16_t tf_es;
    uint16_t tf_padding2;
    uint16_t tf_ds;
    uint16_t tf_padding3;
    uint32_t tf_trapno;
    // 触发中断时自动保存的数据（对于异常8-14以及异常17都是硬件压入的，其他的是vector中手动压入的0，以便统一处理）
    uint32_t tf_err;
    uintptr_t tf_eip;
    uint16_t tf_cs;
    uint16_t tf_padding4;
    uint32_t tf_eflags;
    // 特权级提升的时候切换栈，保存的旧栈的地址
    uintptr_t tf_esp;
    uint16_t tf_ss;
    uint16_t tf_padding5;
} __attribute__((packed));

// 门描述符类型
#define STS_T16A 0x1 // Available 16-bit TSS
#define STS_LDT 0x2  // Local Descriptor Table
#define STS_T16B 0x3 // Busy 16-bit TSS
#define STS_CG16 0x4 // 16-bit Call Gate
#define STS_TG 0x5   // Task Gate / Coum Transmitions
#define STS_IG16 0x6 // 16-bit Interrupt Gate
#define STS_TG16 0x7 // 16-bit Trap Gate
#define STS_T32A 0x9 // Available 32-bit TSS
#define STS_T32B 0xB // Busy 32-bit TSS
#define STS_CG32 0xC // 32-bit Call Gate
#define STS_IG32 0xE // 32-bit Interrupt Gate
#define STS_TG32 0xF // 32-bit Trap Gate

// 门描述符类型
//
// 高32位：
// +----------------16---------------+-1-+--2--+-1-+--4---+--3--+---5--+
// |          offset 31:16           | P | DPL | S | type | rsv | args |
// +---------------------------------+---+-----+---+------+-----+------+
// 低32位：
// +----------------16---------------+----------------16---------------+
// |           seg selector          |           offset 15:0           |
// +---------------------------------+---------------------------------+
//
// offset：中断入口地址在段中的偏移地址
// segment selector：段选择子
// args：参数，对调用门有效
// rsv：保留位
// type：门类型
// S：必须为0，表示为系统段
// DPL：段特权级
// P：存在位
//
struct gate_desc
{
    unsigned gd_off_15_0 : 16;  // low 16 bits of offset in segment
    unsigned gd_ss : 16;        // segment selector
    unsigned gd_args : 5;       // # args, 0 for interrupt/trap gates
    unsigned gd_rsv1 : 3;       // reserved(should be zero I guess)
    unsigned gd_type : 4;       // type(STS_{TG,IG32,TG32})
    unsigned gd_s : 1;          // must be 0 (system)
    unsigned gd_dpl : 2;        // descriptor(meaning new) privilege level
    unsigned gd_p : 1;          // Present
    unsigned gd_off_31_16 : 16; // high bits of offset in segment
};

#define SET_GATE(gate, off, ss, args, type, dpl)     \
    {                                                \
        (gate).gd_off_15_0 = (uint32_t)(off)&0xffff; \
        (gate).gd_ss = (ss);                         \
        (gate).gd_args = (args);                     \
        (gate).gd_rsv1 = 0;                          \
        (gate).gd_type = (type);                     \
        (gate).gd_s = 0;                             \
        (gate).gd_dpl = (dpl);                       \
        (gate).gd_p = 1;                             \
        (gate).gd_off_31_16 = (uint32_t)(off) >> 16; \
    }

// 中断门和陷阱门的唯一区别就是中断门在触发中断的时候会自动将eflags的IF位置0屏蔽中断，iret返回时自动恢复
#define SET_IGATE(gate, off, ss, dpl) SET_GATE(gate, off, ss, 0, STS_IG32, dpl)          // 中断门
#define SET_TGATE(gate, off, ss, dpl) SET_GATE(gate, off, ss, 0, STS_TG32, dpl)          // 陷阱门
#define SET_CGATE(gate, off, ss, args, dpl) SET_GATE(gate, off, ss, args, STS_CG32, dpl) // 调用门

void idt_init(void); // 初始化中断描述符表

#endif // __KERN_TRAP_TRAP_H__
