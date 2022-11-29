#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

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
#define KSTACK_PAGE 2                       // 内核栈的页数
#define PG_SIZE 4096                        // 页大小 4KB
#define KSTACK_SIZE (KSTACK_PAGE * PG_SIZE) // 内核栈的大小

/* *
 * Virtual page table. Entry PDX[VPT] in the PD (Page Directory) contains
 * a pointer to the page directory itself, thereby turning the PD into a page
 * table, which maps all the PTEs (Page Table Entry) containing the page mappings
 * for the entire virtual address space into that 4 Meg region starting at VPT.
 * */
#define VPT 0xFAC00000

// 段描述符定义（汇编）
#define SEG_NULL_ASM \
    .word 0, 0;      \
    .byte 0, 0, 0, 0

#define SEG_DESC_ASM(type, base, lim)               \
    .word(((lim) >> 12) & 0xffff), ((base)&0xffff); \
    .byte(((base) >> 16) & 0xff), (0x90 | (type)),  \
        (0xC0 | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)

#ifndef __ASSEMBLER__

#include "libs/defs.h"
#include "libs/list.h"
#include "libs/atomic.h"

/* free_area_t - maintains a doubly linked list to record free (unused) pages */
typedef struct
{
    list_entry_t free_list; // the list header
    unsigned n_free;        // # of free pages in this free list
} free_area_t;

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
        uint64_t size; //内存大小
        uint32_t type; // 内存类型
    } __attribute__((packed)) map[E820_MAX];
};

/* *
 * struct Page - Page descriptor structures. Each Page describes one
 * physical page. In kern/mm/pmm.h, you can find lots of useful functions
 * that convert Page to other data types, such as phyical address.
 * */
struct Page
{
    int ref;                // page frame's reference counter
    uint32_t flags;         // array of flags that describe the status of the page frame
    unsigned property;      // the num of free block, used in first fit pm manager
    list_entry_t page_link; // free list link
};

typedef uintptr_t pte_t;
typedef uintptr_t pde_t;
typedef size_t ppn_t;

/* Flags describing the status of a page frame */
#define PG_RESERVED 0 // the page descriptor is reserved for kernel or unusable
#define PG_PROPERTY 1 // the member 'property' is valid

#define SET_PAGE_RESERVED(page) set_bit(PG_RESERVED, &((page)->flags))
#define ClearPageReserved(page) clear_bit(PG_reserved, &((page)->flags))
#define PageReserved(page) test_bit(PG_reserved, &((page)->flags))
#define SET_PAGE_PROPERTY(page) set_bit(PG_PROPERTY, &((page)->flags))
#define CLEAR_PAGE_PROPERTY(page) clear_bit(PG_PROPERTY, &((page)->flags))
#define PageProperty(page) test_bit(PG_property, &((page)->flags))

static inline int
page_ref(struct Page *page)
{
    return page->ref;
}

static inline void set_page_ref(struct Page *page, int val)
{
    page->ref = val;
}

static inline int page_ref_inc(struct Page *page)
{
    page->ref += 1;
    return page->ref;
}

static inline int
page_ref_dec(struct Page *page)
{
    page->ref -= 1;
    return page->ref;
}

#define PG_SIZE 4096 // 页大小 4KB
#define PG_SHIFT 12  // log2(PG_SIZE)

// A linear address 'la' has a three-part structure as follows:
//
// +--------10------+-------10-------+---------12----------+
// | Page Directory |   Page Table   | Offset within Page  |
// |      Index     |     Index      |                     |
// +----------------+----------------+---------------------+
//  \--- PDX(la) --/ \--- PTX(la) --/ \---- PG_OFF(la) ---/
//  \----------- PPN(la) -----------/
//
// The PDX, PTX, PG_OFF, and PPN macros decompose linear addresses as shown.
// To construct a linear address la from PDX(la), PTX(la), and PG_OFF(la),
// use PG_ADDR(PDX(la), PTX(la), PG_OFF(la)).

#define PTX_SHIFT 12 // offset of PTX in a linear address
#define PDX_SHIFT 22 // offset of PDX in a linear address

// page directory index
#define PDX(la) ((((uintptr_t)(la)) >> PDX_SHIFT) & 0x3FF)

// page table index
#define PTX(la) ((((uintptr_t)(la)) >> PTX_SHIFT) & 0x3FF)

// 线性地址对应的页号
#define PPN(la) (((uintptr_t)(la)) >> PTX_SHIFT)

// offset in page
#define PG_OFF(la) (((uintptr_t)(la)) & 0xFFF)

// construct linear address from indexes and offset
#define PGADDR(d, t, o) ((uintptr_t)((d) << PDX_SHIFT | (t) << PTX_SHIFT | (o)))

// address in page table or page directory entry
#define PTE_ADDR(pte) ((uintptr_t)(pte) & ~0xFFF)
#define PDE_ADDR(pde) PTE_ADDR(pde)

/* page directory and page table constants */
#define NPDEENTRY 1024 // page directory entries per page directory
#define NPTEENTRY 1024 // page table entries per page table

#define PTSIZE (PGSIZE * NPTEENTRY) // bytes mapped by a page directory entry
#define PTSHIFT 22                  // log2(PTSIZE)

/* page table/directory entry flags */
#define PTE_P 0x001     // Present
#define PTE_W 0x002     // Writeable
#define PTE_U 0x004     // User
#define PTE_PWT 0x008   // Write-Through
#define PTE_PCD 0x010   // Cache-Disable
#define PTE_A 0x020     // Accessed
#define PTE_D 0x040     // Dirty
#define PTE_PS 0x080    // Page Size
#define PTE_MBZ 0x180   // Bits must be zero
#define PTE_AVAIL 0xE00 // Available for software use
                        // The PTE_AVAIL bits aren't used by the kernel or interpreted by the
                        // hardware, so user processes are allowed to set them arbitrarily.

#define PTE_USER (PTE_U | PTE_W | PTE_P)

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

// convert list entry to page
#define le2page(le, member) \
    to_struct((le), struct Page, member)

#endif /* !__ASSEMBLER__ */

#endif /* !__KERN_MM_MEMLAYOUT_H__ */
