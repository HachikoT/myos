#ifndef __KERN_MM_PMM_H__
#define __KERN_MM_PMM_H__

#include "libs/defs.h"
#include "kern/mm/memlayout.h"

// pmm_manager is a physical memory management class. A special pmm manager - XXX_pmm_manager
// only needs to implement the methods in pmm_manager class, then XXX_pmm_manager can be used
// by ucore to manage the total physical memory space.
struct pmm_manager
{
    const char *name;                                  // XXX_pmm_manager's name
    void (*init)(void);                                // initialize internal description&management data structure
                                                       // (free block list, number of free block) of XXX_pmm_manager
    void (*mem_map_init)(struct Page *base, size_t n); // setup description&management data structcure according to
                                                       // the initial free physical memory space
    struct Page *(*alloc_pages)(size_t n);             // allocate >=n pages, depend on the allocation algorithm
    void (*free_pages)(struct Page *base, size_t n);   // free >=n pages with "base" addr of Page descriptor structures(memlayout.h)
    size_t (*n_free_pages)(void);                      // return the number of free pages
    void (*check)(void);                               // check the correctness of XXX_pmm_manager
};

/* *
 * PADDR - takes a kernel virtual address (an address that points above KERNBASE),
 * where the machine's maximum 256MB of physical memory is mapped and returns the
 * corresponding physical address.  It panics if you pass it a non-kernel virtual address.
 * */
#define PADDR(kva) ({                     \
    uintptr_t __m_kva = (uintptr_t)(kva); \
    __m_kva - KERN_BASE;                  \
})

/* *
 * KADDR - takes a physical address and returns the corresponding kernel virtual
 * address. It panics if you pass an invalid physical address.
 * */
#define KADDR(pa) ({              \
    uintptr_t __m_pa = (pa);      \
    (void *)(__m_pa + KERN_BASE); \
})

extern struct Page *pages;
extern size_t npage;

static inline struct Page *pa2page(uintptr_t pa)
{
    return &pages[PPN(pa)];
}

static inline ppn_t page2ppn(struct Page *page)
{
    return page - pages;
}

static inline uintptr_t page2pa(struct Page *page)
{
    return page2ppn(page) << PG_SHIFT;
}

static inline void *page2kva(struct Page *page)
{
    return KADDR(page2pa(page));
}

static inline struct Page *pte2page(pte_t pte)
{
    return pa2page(PTE_ADDR(pte));
}

void pmm_init(void); // 初始化物理内存管理

struct Page *alloc_pages(size_t n);
void free_pages(struct Page *base, size_t n);
size_t n_free_pages(void);

#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)

#endif /* !__KERN_MM_PMM_H__ */
