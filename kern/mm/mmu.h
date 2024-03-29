#ifndef __KERN_MM_MMU_H__
#define __KERN_MM_MMU_H__

#include "libs/defs.h"

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
