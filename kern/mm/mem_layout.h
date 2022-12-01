#ifndef __KERN_MM_MEM_LAYOUT_H__
#define __KERN_MM_MEM_LAYOUT_H__

/* This file contains the definitions for memory management in our OS. */

/* global segment number */
#define SEG_KTEXT 1
#define SEG_KDATA 2
#define SEG_UTEXT 3
#define SEG_UDATA 4
#define SEG_TSS 5

/* global descriptor numbers */
#define GD_KTEXT ((SEG_KTEXT) << 3) // kernel text
#define GD_KDATA ((SEG_KDATA) << 3) // kernel data
#define GD_UTEXT ((SEG_UTEXT) << 3) // user text
#define GD_UDATA ((SEG_UDATA) << 3) // user data
#define GD_TSS ((SEG_TSS) << 3)     // task segment selector

#define DPL_KERNEL (0)
#define DPL_USER (3)

#define KERNEL_CS ((GD_KTEXT) | DPL_KERNEL)
#define KERNEL_DS ((GD_KDATA) | DPL_KERNEL)
#define USER_CS ((GD_UTEXT) | DPL_USER)
#define USER_DS ((GD_UDATA) | DPL_USER)

/* *
 * Virtual memory map:                                          Permissions
 *                                                              kernel/user
 *
 *     4G ------------------> +---------------------------------+
 *                            |                                 |
 *                            |         Empty Memory (*)        |
 *                            |                                 |
 *                            +---------------------------------+ 0xFB000000
 *                            |   Cur. Page Table (Kern, RW)    | RW/-- PT_SIZE
 *     VPT -----------------> +---------------------------------+ 0xFAC00000
 *                            |        Invalid Memory (*)       | --/--
 *     KERN_TOP ------------> +---------------------------------+ 0xF8000000
 *                            |                                 |
 *                            |    Remapped Physical Memory     | RW/-- KMEM_SIZE
 *                            |                                 |
 *     KERN_BASE -----------> +---------------------------------+ 0xC0000000 (3GB)
 *                            |                                 |
 *                            |                                 |
 *                            |                                 |
 *                            ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * (*) Note: The kernel ensures that "Invalid Memory" is *never* mapped.
 *     "Empty Memory" is normally unmapped, but user programs may map pages
 *     there if desired.
 * */

#define KERN_BASE 0xC0000000
#define KMEM_SIZE 0x38000000 // the maximum amount of physical memory
#define KERN_TOP (KERN_BASE + KMEM_SIZE)

/* *
 * Virtual page table. Entry PDX[VPT] in the PD (Page Directory) contains
 * a pointer to the page directory itself, thereby turning the PD into a page
 * table, which maps all the PTEs (Page Table Entry) containing the page mappings
 * for the entire virtual address space into that 4 Meg region starting at VPT.
 * */
#define VPT 0xFAC00000

#define N_PDE_ENTRY 1024 // 页目录表项数量
#define N_PTE_ENTRY 1024 // 页表项数量

#define PG_SIZE 4096                    // 页大小 4KB
#define PG_SHIFT 12                     // log2(PG_SIZE)
#define PT_SIZE (PG_SIZE * N_PTE_ENTRY) // 一项页目录表项指向的物理内存大小（4MB）

#define KSTACK_PAGE 2                       // 内核栈的页数
#define KSTACK_SIZE (KSTACK_PAGE * PG_SIZE) // 内核栈的大小

#ifndef __ASSEMBLER__

#include "libs/defs.h"
#include "libs/list.h"
#include "libs/atomic.h"

// 定义用e820方式探测出的内存范围描述符结构
#define E820_MAX 20     // number of entries in E820MAP
#define E820_MEM 1      // 可用内存
#define E820_RESERVED 2 // 保留不可用内存

struct e820map
{
    int n_map; // 探测到的内存块数量
    struct
    {
        uint64_t addr; // 内存基址
        uint64_t size; // 内存大小
        uint32_t type; // 内存类型
    } __attribute__((packed)) map[E820_MAX];
};

// 页描述符，内核用来管理物理页的数据结构
struct page_desc
{
    unsigned ref;               // 页帧被引用的数量
    uint32_t flags;             // 状态标志
    unsigned property;          // 对于first fit的pmm来说该属性值表示当前连续的页的数量
    list_entry_t page_link;     // 链表指针
    list_entry_t pra_page_link; // used for pra (page replace algorithm)
    uintptr_t pra_vaddr;        // used for pra (page replace algorithm)
};

// 页描述符的flag的位定义
#define PG_RESERVED 0 // 页是否保留或被内核使用
#define PG_PROPERTY 1 // property属性是否有效

#define SET_PG_FLAG_BIT(page, bit) set_bit(bit, &((page)->flags))
#define CLEAR_PG_FLAG_BIT(page, bit) clear_bit(bit, &((page)->flags))
#define TEST_PG_FLAG_BIT(page, bit) test_bit(bit, &((page)->flags))

// 空闲链表，用来记录空闲的页
typedef struct
{
    list_entry_t free_list; // the list header
    unsigned n_free;        // # of free pages in this free list
} free_area_t;

// 将内核虚拟地址转换为实际的物理地址
#define PADDR(kva) ({                     \
    uintptr_t __m_kva = (uintptr_t)(kva); \
    __m_kva - KERN_BASE;                  \
})

// 将实际物理地址转换为内核虚拟地址
#define KADDR(pa) ({              \
    uintptr_t __m_pa = (pa);      \
    (void *)(__m_pa + KERN_BASE); \
})

// 将list_entry对象转换为page对象，这里的list_entry必须要为page对象的page_link成员
#define le2page(le) \
    to_struct((le), struct page_desc, page_link)

// 将list_entry对象转换为page对象，这里的list_entry必须要为page对象的page_link成员
#define le2page_pra(le, member) \
    to_struct((le), struct page_desc, member)

/* Control Register flags */
#define CR0_PE 0x00000001 // Protection Enable
#define CR0_MP 0x00000002 // Monitor coProcessor
#define CR0_EM 0x00000004 // Emulation
#define CR0_TS 0x00000008 // Task Switched
#define CR0_ET 0x00000010 // Extension Type
#define CR0_NE 0x00000020 // Numeric Errror
#define CR0_WP 0x00010000 // Write Protect
#define CR0_AM 0x00040000 // Alignment Mask
#define CR0_NW 0x20000000 // Not Writethrough
#define CR0_CD 0x40000000 // Cache Disable
#define CR0_PG 0x80000000 // Paging

#define CR4_PCE 0x00000100 // Performance counter enable
#define CR4_MCE 0x00000040 // Machine Check Enable
#define CR4_PSE 0x00000010 // Page Size Extensions
#define CR4_DE 0x00000008  // Debugging Extensions
#define CR4_TSD 0x00000004 // Time Stamp Disable
#define CR4_PVI 0x00000002 // Protected-Mode Virtual Interrupts
#define CR4_VME 0x00000001 // V86 Mode Extensions

#endif /* !__ASSEMBLER__ */

#endif /* !__KERN_MM_MEM_LAYOUT_H__ */
