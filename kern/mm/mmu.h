#ifndef __KERN_MM_MMU_H__
#define __KERN_MM_MMU_H__

#include "libs/defs.h"

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

/* Application segment type bits */
#define STA_X 0x8 // Executable segment
#define STA_E 0x4 // Expand down (non-executable segments)
#define STA_C 0x4 // Conforming code segment (executable only)
#define STA_W 0x2 // Writeable (non-executable segments)
#define STA_R 0x2 // Readable (executable segments)
#define STA_A 0x1 // Accessed

/* System segment type bits */
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

// 中断或陷阱描述符类型
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

/* *
 * Set up a normal interrupt/trap gate descriptor
 *   - istrap: 1 for a trap (= exception) gate, 0 for an interrupt gate
 *   - sel: Code segment selector for interrupt/trap handler
 *   - off: Offset in code segment for interrupt/trap handler
 *   - dpl: Descriptor Privilege Level - the privilege level required
 *          for software to invoke this interrupt/trap gate explicitly
 *          using an int instruction.
 * */
#define SET_GATE(gate, istrap, sel, off, dpl)            \
    {                                                    \
        (gate).gd_off_15_0 = off & 0xffff;               \
        (gate).gd_ss = (sel);                            \
        (gate).gd_args = 0;                              \
        (gate).gd_rsv1 = 0;                              \
        (gate).gd_type = (istrap) ? STS_TG32 : STS_IG32; \
        (gate).gd_s = 0;                                 \
        (gate).gd_dpl = (dpl);                           \
        (gate).gd_p = 1;                                 \
        (gate).gd_off_31_16 = off >> 16;                 \
    }

/* Set up a call gate descriptor */
#define SETCALLGATE(gate, ss, off, dpl)              \
    {                                                \
        (gate).gd_off_15_0 = (uint32_t)(off)&0xffff; \
        (gate).gd_ss = (ss);                         \
        (gate).gd_args = 0;                          \
        (gate).gd_rsv1 = 0;                          \
        (gate).gd_type = STS_CG32;                   \
        (gate).gd_s = 0;                             \
        (gate).gd_dpl = (dpl);                       \
        (gate).gd_p = 1;                             \
        (gate).gd_off_31_16 = (uint32_t)(off) >> 16; \
    }

/* segment descriptors */
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

/* task state segment format (as described by the Pentium architecture book) */
struct task_state
{
    uint32_t ts_link;  // old ts selector
    uintptr_t ts_esp0; // stack pointers and segment selectors
    uint16_t ts_ss0;   // after an increase in privilege level
    uint16_t ts_padding1;
    uintptr_t ts_esp1;
    uint16_t ts_ss1;
    uint16_t ts_padding2;
    uintptr_t ts_esp2;
    uint16_t ts_ss2;
    uint16_t ts_padding3;
    uintptr_t ts_cr3; // page directory base
    uintptr_t ts_eip; // saved state from last task switch
    uint32_t ts_eflags;
    uint32_t ts_eax; // more saved state (registers)
    uint32_t ts_ecx;
    uint32_t ts_edx;
    uint32_t ts_ebx;
    uintptr_t ts_esp;
    uintptr_t ts_ebp;
    uint32_t ts_esi;
    uint32_t ts_edi;
    uint16_t ts_es; // even more saved state (segment selectors)
    uint16_t ts_padding4;
    uint16_t ts_cs;
    uint16_t ts_padding5;
    uint16_t ts_ss;
    uint16_t ts_padding6;
    uint16_t ts_ds;
    uint16_t ts_padding7;
    uint16_t ts_fs;
    uint16_t ts_padding8;
    uint16_t ts_gs;
    uint16_t ts_padding9;
    uint16_t ts_ldt;
    uint16_t ts_padding10;
    uint16_t ts_t;    // trap on task switch
    uint16_t ts_iomb; // i/o map base address
};

// 在页机制中一个线性地址（linear address）可以分为以下三部分
//
// +--------10------+-------10-------+---------12----------+
// | Page Directory |   Page Table   | Offset within Page  |
// |      Index     |     Index      |                     |
// +----------------+----------------+---------------------+
//  \--- PDX(la) --/ \--- PTX(la) --/ \---- PG_OFF(la) ---/
//  \----------- PPN(la) -----------/
//
// PDX：在页目录表中的序号
// PTX：在页表中的序号
// PG_OFF：在物理页中的偏移量

// 计算PDX和PTX的偏移值
#define PDX_SHIFT 22
#define PTX_SHIFT 12

#define PDX(la) ((((uintptr_t)(la)) >> PDX_SHIFT) & 0x3FF) // 页目录表序号
#define PTX(la) ((((uintptr_t)(la)) >> PTX_SHIFT) & 0x3FF) // 页表号
#define PPN(la) (((uintptr_t)(la)) >> PTX_SHIFT)           // 物理页号
#define PG_OFF(la) (((uintptr_t)(la)) & 0xFFF)             // 页内偏移

// 计算表项中包含的基址
#define PTE_ADDR(pte) ((uintptr_t)(pte) & ~0xFFF)
#define PDE_ADDR(pde) PTE_ADDR(pde)

// construct linear address from indexes and offset
#define PGADDR(d, t, o) ((uintptr_t)((d) << PDX_SHIFT | (t) << PTX_SHIFT | (o)))

// 页目录表项结构
//
// +------------20-----------+--3--+-1-+-1--+-1-+-1-+--1--+--1--+--1--+--1--+-1-+
// | Page Table Base Address | AVL | G | PS | 0 | A | PCD | PWT | U/S | R/W | P |
// +-------------------------+-----+---+----+---+---+-----+-----+-----+-----+---+
//
// Base Address：对应的页表或页的基地址
// AVL：系统软件可用位（留给操作系统自己定义）
// G：是否为全局页，需要%cr4.PGE位为1有效。在更新%cr3寄存器后，TLB也不会清除全局页的缓存
// PS：页大小，0表示4KB
// A：访问位。由硬件处理，表示此表项指向的页是否读写过
// PCD：为1不允许TLB缓存。当%cr0.CD=1时此位被忽略
// PWT：为1使用Write-through的缓存类型，为0使用Write-back的缓存类型。当%cr0.CD=1时此位被忽略
// U/S：为1表示用户级，所有特权级都可以访问
// R/W：读写标志，0表示只读，1表示可读写。在0、1、2特权级时，此位不起作用
// P：存在位，为1表示位于内存中

#define PDE_P 0x001   // Present
#define PDE_W 0x002   // Writeable
#define PDE_U 0x004   // User
#define PDE_PWT 0x008 // Write-Through
#define PDE_PCD 0x010 // Cache-Disable
#define PDE_A 0x020   // Accessed
#define PDE_PS 0x080  // Page Size
#define PDE_G 0x040   // Global
#define PDE_AVL 0xE00 // Available for software use

// 页表项结构
//
// +---------20 -------+--3--+-1-+-1-+-1-+-1-+--1--+--1--+--1--+--1--+-1-+
// | Page Base Address | AVL | G | 0 | D | A | PCD | PWT | U/S | R/W | P |
// +-------------------+-----+---+---+---+---+-----+-----+-----+-----+---+
//
// Base Address：对应的页的基地址
// AVL：系统软件可用位（留给操作系统自己定义）
// G：是否为全局页，需要%cr4.PGE位为1有效。在更新%cr3寄存器后，TLB也不会清除全局页的缓存
// D：脏位。由硬件处理，表示此表项指向的页是否写过数据
// A：访问位。由硬件处理，表示此表项指向的页是否读写过
// PCD：为1不允许TLB缓存。当%cr0.CD=1时此位被忽略
// PWT：为1使用Write-through的缓存类型，为0使用Write-back的缓存类型。当%cr0.CD=1时此位被忽略
// U/S：为1表示用户级，所有特权级都可以访问
// R/W：读写标志，0表示只读，1表示可读写。在0、1、2特权级时，此位不起作用
// P：存在位，为1表示位于内存中

#define PTE_P 0x001   // Present
#define PTE_W 0x002   // Writeable
#define PTE_U 0x004   // User
#define PTE_PWT 0x008 // Write-Through
#define PTE_PCD 0x010 // Cache-Disable
#define PTE_A 0x020   // Accessed
#define PTE_D 0x040   // Dirty
#define PTE_MBZ 0x180 // Bits must be zero
#define PTE_AVL 0xE00 // Available for software use

#define PTE_USER (PTE_U | PTE_W | PTE_P)

typedef uintptr_t pte_t;
typedef uintptr_t pde_t;
typedef size_t ppn_t;

#endif /* !__KERN_MM_MMU_H__ */
