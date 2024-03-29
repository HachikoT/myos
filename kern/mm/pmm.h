#ifndef __KERN_MM_PMM_H__
#define __KERN_MM_PMM_H__

#include "libs/defs.h"
#include "kern/mm/mem_layout.h"
#include "kern/mm/mmu.h"
#include "kern/debug/assert.h"
#include "kern/mm/vmm.h"

// 物理内存管理框架，主要用来管理物理页
struct pmm_manager
{
    const char *name;                                       // 物理内存管理器名字
    void (*init)(void);                                     // 初始化物理内存管理器
    void (*mem_map_init)(struct page_desc *base, size_t n); // 将n个连续的物理页初始化到物理内存管理器
    struct page_desc *(*alloc_pages)(size_t n);             // 分配连续的n个物理页
    void (*free_pages)(struct page_desc *base, size_t n);   // 释放连续的n个物理页
    size_t (*n_free_pages)(void);                           // 获取当前还有多少个空闲的物理页（不一定连续）
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

struct page_desc *alloc_pages(size_t n);           // 分配连续的n个页
void free_pages(struct page_desc *base, size_t n); // 释放n个连续的页
size_t n_free_pages(void);                         // 获取内存管理器中总的空闲页数量

#define alloc_page() alloc_pages(1)
#define free_page(page) free_pages(page, 1)

int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share);

struct page_desc *pgdir_alloc_page(struct mm_struct *mm, pde_t *pgdir, uintptr_t la, uint32_t perm);
void unmap_range(pde_t *pgdir, uintptr_t start, uintptr_t end);
void exit_range(pde_t *pgdir, uintptr_t start, uintptr_t end);

// 根据线性地址la获取对应的页表项，如果create为true那么页表缺失的话自动创建
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create);

void pmm_init(void); // 初始化物理内存管理

void load_esp0(uintptr_t esp0); // 更新tss的esp0，指定ring0的栈地址

void print_pgdir(void);

#endif // __KERN_MM_PMM_H__
