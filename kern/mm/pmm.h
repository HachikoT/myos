#ifndef __KERN_MM_PMM_H__
#define __KERN_MM_PMM_H__

#include "libs/defs.h"
#include "kern/mm/mem_layout.h"
#include "kern/mm/mmu.h"
#include "kern/debug/assert.h"

// 物理内存管理框架，主要用来管理物理页
struct pmm_manager
{
    const char *name;                                       // XXX_pmm_manager's name
    void (*init)(void);                                     // initialize internal description&management data structure
                                                            // (free block list, number of free block) of XXX_pmm_manager
    void (*mem_map_init)(struct page_desc *base, size_t n); // setup description&management data structcure according to
                                                            // the initial free physical memory space
    struct page_desc *(*alloc_pages)(size_t n);             // allocate >=n pages, depend on the allocation algorithm
    void (*free_pages)(struct page_desc *base, size_t n);   // free >=n pages with "base" addr of Page descriptor structures(mem_layout.h)
    size_t (*n_free_pages)(void);                           // return the number of free pages
    void (*check)(void);                                    // check the correctness of XXX_pmm_manager
};

extern struct pmm_manager *g_pmm_mgr;
extern pde_t *g_boot_pgdir;
extern uintptr_t g_boot_cr3;
extern struct page_desc *g_pages;
extern size_t g_npage;

extern char kern_stack[], kern_stack_top[];

// 计算物理地址对应的页描述符
static inline struct page_desc *pa2page(uintptr_t pa)
{
    return &g_pages[PPN(pa)];
}

// 计算在页描述符表中的偏移值
static inline ppn_t page2ppn(struct page_desc *page)
{
    return page - g_pages;
}

// 计算页描述符对应的物理地址
static inline uintptr_t page2pa(struct page_desc *page)
{
    return page2ppn(page) << PG_SHIFT;
}

// 计算页描述符对应的内核虚拟地址
static inline void *page2kva(struct page_desc *page)
{
    return KADDR(page2pa(page));
}

static inline struct page_desc *kva2page(void *kva)
{
    return pa2page(PADDR(kva));
}

static inline struct page_desc *
pte2page(pte_t pte)
{
    if (!(pte & PTE_P))
    {
        panic("pte2page called with invalid pte");
    }
    return pa2page(PTE_ADDR(pte));
}

static inline struct page_desc *
pde2page(pde_t pde)
{
    return pa2page(PDE_ADDR(pde));
}

struct page_desc *alloc_pages(size_t n);           // 分配连续的n个页描述符
void free_pages(struct page_desc *base, size_t n); // 释放n个连续的页描述符
size_t n_free_pages(void);                         // 获取内存管理器中总的空闲页描述符数量

#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)

struct page_desc *pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm);

void pmm_init(void); // 初始化物理内存管理

void load_esp0(uintptr_t esp0);

#endif /* !__KERN_MM_PMM_H__ */
