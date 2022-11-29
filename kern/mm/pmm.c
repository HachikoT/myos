#include "kern/mm/pmm.h"
#include "kern/mm/mmu.h"
#include "kern/mm/memlayout.h"
#include "kern/mm/default_pmm_manager.h"
#include "libs/x86.h"
#include "libs/defs.h"
#include "kern/driver/stdio.h"
#include "kern/sync/sync.h"
#include "libs/string.h"
#include "libs/error.h"

// 当前物理内存管理器
static struct pmm_manager *pmm_mgr;

struct Page *pages; // 物理页数组首地址
size_t npage = 0;   // 物理页数量

// virtual address of boot-time page directory
pde_t *boot_pgdir = NULL;
// physical address of boot-time page directory
uintptr_t boot_cr3;

// 初始化物理内存管理器实例
static void pmm_manager_init(void)
{
    pmm_mgr = &default_pmm_mgr;
    cprintf("memory management: %s\n", pmm_mgr->name);
    pmm_mgr->init();
}

// init_memmap - call pmm->mem_map_init to build Page struct for free memory
static void mem_map_init(struct Page *base, size_t n)
{
    pmm_mgr->mem_map_init(base, n);
}

/* pmm_init - initialize the physical memory management */
static void page_init(void)
{
    // bootloader中探测到的物理内存布局
    struct e820map *mem_map = (struct e820map *)(0x8000 + KERN_BASE);

    // 记录最高内存
    uint64_t max_pa = 0;

    cprintf("e820map:\n");
    for (int i = 0; i < mem_map->n_map; i++)
    {
        uint64_t begin = mem_map->map[i].addr, end = begin + mem_map->map[i].size;
        cprintf("memory: %08llx, [%08llx, %08llx], type = %d.\n",
                mem_map->map[i].size, begin, end - 1, mem_map->map[i].type);
        if (mem_map->map[i].type == E820_MEM)
        {
            if (max_pa < end && begin < KMEM_SIZE)
            {
                max_pa = end;
            }
        }
    }
    if (max_pa > KMEM_SIZE)
    {
        max_pa = KMEM_SIZE;
    }

    extern char end[];

    // 计算出物理页的首地址和页数
    npage = max_pa / PG_SIZE;
    pages = (struct Page *)ROUND_UP((void *)end, PG_SIZE);

    for (int i = 0; i < npage; i++)
    {
        SET_PAGE_RESERVED(pages + i);
    }

    uintptr_t free_mem = PADDR((uintptr_t)pages + sizeof(struct Page) * npage);

    for (int i = 0; i < mem_map->n_map; i++)
    {
        uint64_t begin = mem_map->map[i].addr, end = begin + mem_map->map[i].size;
        if (mem_map->map[i].type == E820_MEM)
        {
            if (begin < free_mem)
            {
                begin = free_mem;
            }
            if (end > KMEM_SIZE)
            {
                end = KMEM_SIZE;
            }
            if (begin < end)
            {
                begin = ROUND_UP(begin, PG_SIZE);
                end = ROUND_DOWN(end, PG_SIZE);
                if (begin < end)
                {
                    mem_map_init(pa2page(begin), (end - begin) / PG_SIZE);
                }
            }
        }
    }
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE memory
struct Page *alloc_pages(size_t n)
{
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_mgr->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_mgr->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

// invalidate a TLB entry, but only if the page tables being
// edited are the ones currently in use by the processor.
void tlb_invalidate(pde_t *pgdir, uintptr_t la)
{
    if (rcr3() == PADDR(pgdir))
    {
        invlpg((void *)la);
    }
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t n_free_pages(void)
{
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_mgr->n_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

// boot_alloc_page - allocate one page using pmm->alloc_pages(1)
//  return value: the kernel virtual address of this allocated page
// note: this function is used to get the memory for PDT(Page Directory Table)&PT(Page Table)
static void *boot_alloc_page(void)
{
    struct Page *p = alloc_page();
    return page2kva(p);
}

// get_pte - get pte and return the kernel virtual address of this pte for la
//         - if the PT contians this pte didn't exist, alloc a page for PT
//  parameter:
//   pgdir:  the kernel virtual base address of PDT
//   la:     the linear address need to map
//   create: a logical value to decide if alloc a page for PT
//  return vaule: the kernel virtual address of this pte
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    pde_t *pdep = &pgdir[PDX(la)];
    if (!(*pdep & PTE_P))
    {
        struct Page *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        set_page_ref(page, 1);
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PG_SIZE);
        *pdep = pa | PTE_U | PTE_W | PTE_P;
    }
    return &((pte_t *)KADDR(PDE_ADDR(*pdep)))[PTX(la)];
}

// get_page - get related Page struct for linear address la using PDT pgdir
struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep_store != NULL)
    {
        *ptep_store = ptep;
    }
    if (ptep != NULL && *ptep & PTE_P)
    {
        return pte2page(*ptep);
    }
    return NULL;
}

// page_remove_pte - free an Page sturct which is related linear address la
//                 - and clean(invalidate) pte which is related linear address la
// note: PT is changed, so the TLB need to be invalidate
static inline void page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep)
{
    if (*ptep & PTE_P)
    {
        struct Page *page = pte2page(*ptep);
        if (page_ref_dec(page) == 0)
        {
            free_page(page);
        }
        *ptep = 0;
        tlb_invalidate(pgdir, la);
    }
}

// page_remove - free an Page which is related linear address la and has an validated pte
void page_remove(pde_t *pgdir, uintptr_t la)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep != NULL)
    {
        page_remove_pte(pgdir, la, ptep);
    }
}

// page_insert - build the map of phy addr of an Page with the linear addr la
//  paramemters:
//   pgdir: the kernel virtual base address of PDT
//   page:  the Page which need to map
//   la:    the linear address need to map
//   perm:  the permission of this Page which is setted in related pte
//  return value: always 0
// note: PT is changed, so the TLB need to be invalidate
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)
{
    pte_t *ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL)
    {
        return -E_NO_MEM;
    }
    page_ref_inc(page);
    if (*ptep & PTE_P)
    {
        struct Page *p = pte2page(*ptep);
        if (p == page)
        {
            page_ref_dec(page);
        }
        else
        {
            page_remove_pte(pgdir, la, ptep);
        }
    }
    *ptep = page2pa(page) | PTE_P | perm;
    tlb_invalidate(pgdir, la);
    return 0;
}

// boot_map_segment - setup&enable the paging mechanism
//  parameters
//   la:   linear address of this memory need to map (after x86 segment map)
//   size: memory size
//   pa:   physical address of this memory
//   perm: permission of this memory
static void boot_map_segment(pde_t *pgdir, uintptr_t la, size_t size, uintptr_t pa, uint32_t perm)
{
    size_t n = ROUND_UP(size + PG_OFF(la), PG_SIZE) / PG_SIZE;
    la = ROUND_DOWN(la, PG_SIZE);
    pa = ROUND_DOWN(pa, PG_SIZE);
    for (; n > 0; n--, la += PG_SIZE, pa += PG_SIZE)
    {
        pte_t *ptep = get_pte(pgdir, la, 1);
        *ptep = pa | PTE_P | perm;
    }
}

static void enable_paging(void)
{
    lcr3(boot_cr3);

    // turn on paging
    uint32_t cr0 = rcr0();
    cr0 |= CR0_PE | CR0_PG | CR0_AM | CR0_WP | CR0_NE | CR0_TS | CR0_EM | CR0_MP;
    cr0 &= ~(CR0_TS | CR0_EM);
    lcr0(cr0);
}

/* *
 * Task State Segment:
 *
 * The TSS may reside anywhere in memory. A special segment register called
 * the Task Register (TR) holds a segment selector that points a valid TSS
 * segment descriptor which resides in the GDT. Therefore, to use a TSS
 * the following must be done in function gdt_init:
 *   - create a TSS descriptor entry in GDT
 *   - add enough information to the TSS in memory as needed
 *   - load the TR register with a segment selector for that segment
 *
 * There are several fileds in TSS for specifying the new stack pointer when a
 * privilege level change happens. But only the fields SS0 and ESP0 are useful
 * in our os kernel.
 *
 * The field SS0 contains the stack segment selector for CPL = 0, and the ESP0
 * contains the new ESP value for CPL = 0. When an interrupt happens in protected
 * mode, the x86 CPU will look in the TSS for SS0 and ESP0 and load their value
 * into SS and ESP respectively.
 * */
static struct task_state ts;

/* *
 * Global Descriptor Table:
 *
 * The kernel and user segments are identical (except for the DPL). To load
 * the %ss register, the CPL must equal the DPL. Thus, we must duplicate the
 * segments for the user and the kernel. Defined as follows:
 *   - 0x0 :  unused (always faults -- for trapping NULL far pointers)
 *   - 0x8 :  kernel code segment
 *   - 0x10:  kernel data segment
 *   - 0x18:  user code segment
 *   - 0x20:  user data segment
 *   - 0x28:  defined for tss, initialized in gdt_init
 * */
static struct seg_desc gdt[] = {
    SEG_NULL,
    [SEG_KTEXT] = SEG_DESC(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_KERNEL),
    [SEG_KDATA] = SEG_DESC(STA_W, 0x0, 0xFFFFFFFF, DPL_KERNEL),
    [SEG_UTEXT] = SEG_DESC(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_USER),
    [SEG_UDATA] = SEG_DESC(STA_W, 0x0, 0xFFFFFFFF, DPL_USER),
    [SEG_TSS] = SEG_NULL};

static struct dt_desc gdt_dt = {sizeof(gdt) - 1, (uint32_t)gdt};

/* *
 * lgdt - load the global descriptor table register and reset the
 * data/code segement registers for kernel.
 * */
static inline void lgdt(struct dt_desc *dt)
{
    __asm__ __volatile__("lgdt (%0)" ::"r"(dt));
    __asm__ __volatile__("movw %%ax, %%gs" ::"a"(USER_DS));
    __asm__ __volatile__("movw %%ax, %%fs" ::"a"(USER_DS));
    __asm__ __volatile__("movw %%ax, %%es" ::"a"(KERNEL_DS));
    __asm__ __volatile__("movw %%ax, %%ds" ::"a"(KERNEL_DS));
    __asm__ __volatile__("movw %%ax, %%ss" ::"a"(KERNEL_DS));
    // reload cs
    __asm__ __volatile__("ljmp %0, $1f\n 1:\n" ::"i"(KERNEL_CS));
}

// 临时内核栈空间
uint8_t stack0[1024];

// 初始化全局描述符表GDT和任务状态段TSS
static void gdt_init(void)
{
    // 当中断需要提权时，会切换堆栈，比如提升到ring0，那么就会切换到TSS中的esp0和ss0
    ts.ts_esp0 = (uint32_t)&stack0 + sizeof(stack0);
    ts.ts_ss0 = KERNEL_DS;

    // initialize the TSS field of the gdt
    gdt[SEG_TSS] = SEG_1M_DESC(STS_T32A, (uint32_t)&ts, sizeof(ts), DPL_KERNEL);
    gdt[SEG_TSS].sd_s = 0;

    // reload all segment registers
    lgdt(&gdt_dt);

    // load the TSS
    ltr(GD_TSS);
}

// 初始化物理内存管理
void pmm_init(void)
{
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    pmm_manager_init();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // create boot_pgdir, an initial page directory(Page Directory Table, PDT)
    boot_pgdir = boot_alloc_page();
    memset(boot_pgdir, 0, PG_SIZE);
    boot_cr3 = PADDR(boot_pgdir);

    // recursively insert boot_pgdir in itself
    // to form a virtual page table at virtual address VPT
    boot_pgdir[PDX(VPT)] = PADDR(boot_pgdir) | PTE_P | PTE_W;

    // map all physical memory to linear memory with base linear addr KERNBASE
    // linear_addr KERNBASE~KERNBASE+KMEMSIZE = phy_addr 0~KMEMSIZE
    // But shouldn't use this map until enable_paging() & gdt_init() finished.
    boot_map_segment(boot_pgdir, KERN_BASE, KMEM_SIZE, 0, PTE_W);

    // temporary map:
    // virtual_addr 3G~3G+4M = linear_addr 0~4M = linear_addr 3G~3G+4M = phy_addr 0~4M
    boot_pgdir[0] = boot_pgdir[PDX(KERN_BASE)];

    enable_paging();

    // reload gdt(third time,the last time) to map all physical memory
    // virtual_addr 0~4G=liear_addr 0~4G
    // then set kernel stack(ss:esp) in TSS, setup TSS in gdt, load TSS
    gdt_init();

    // disable the map of virtual_addr 0~4M
    boot_pgdir[0] = 0;
}
