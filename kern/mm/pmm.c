#include "kern/mm/pmm.h"
#include "kern/mm/mmu.h"
#include "kern/mm/mem_layout.h"
#include "kern/mm/ff_pmm_manager.h"
#include "libs/x86.h"
#include "libs/defs.h"
#include "kern/driver/stdio.h"
#include "kern/sync/sync.h"
#include "libs/string.h"
#include "libs/error.h"
#include "kern/debug/assert.h"
#include "kern/mm/swap.h"

// 当前物理内存管理器
struct pmm_manager *g_pmm_mgr;

// 页描述符数组
struct page_desc *g_pages;
size_t g_npage;

pde_t *g_boot_pgdir;  // boot页目录表的内核虚拟地址
uintptr_t g_boot_cr3; // boot页目录表的物理地址

// 初始化物理内存管理器
static void pmm_manager_init(void)
{
    g_pmm_mgr = &g_ff_pmm_mgr;
    cprintf("memory management: %s\n", g_pmm_mgr->name);
    g_pmm_mgr->init();
}

// 初始化页描述符和页的映射关系
static void page_init(void)
{
    // bootloader中探测到的物理内存布局
    struct e820map *mem_map = (struct e820map *)(0x8000 + KERN_BASE);
    assert(12345 != mem_map->n_map);

    // 获取最大的物理内存地址（不超过KMEM_SIZE）
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

    // 内核结尾后4KB对齐的地址用来存放页描述符数组
    extern char __ekernel[];
    g_npage = max_pa / PG_SIZE;
    g_pages = (struct page_desc *)ROUND_UP((void *)__ekernel, PG_SIZE);

    // 初始化所有的物理页都被内核占用了
    for (int i = 0; i < g_npage; i++)
    {
        SET_PG_FLAG_BIT(g_pages + i, PG_RESERVED);
    }

    // 将可用的内存块按页记录到内存管理器中
    uintptr_t free_mem = PADDR(g_pages + g_npage);
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
                    g_pmm_mgr->mem_map_init(pa2page(begin), (end - begin) / PG_SIZE);
                }
            }
        }
    }
}

// 分配连续的n个页描述符
struct page_desc *alloc_pages(size_t n)
{
    struct page_desc *page = NULL;
    bool intr_flag;

    while (1)
    {
        local_intr_save(intr_flag);
        {
            page = g_pmm_mgr->alloc_pages(n);
        }
        local_intr_restore(intr_flag);

        if (page != NULL || n > 1 || swap_init_ok == 0)
            break;

        extern struct mm_struct *check_mm_struct;
        // cprintf("page %x, call swap_out in alloc_pages %d\n",page, n);
        swap_out(check_mm_struct, n, 0);
    }
    return page;
}

// 释放n个连续的页描述符
void free_pages(struct page_desc *base, size_t n)
{
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        g_pmm_mgr->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

// 获取内存管理器中总的空闲页描述符数量
size_t n_free_pages(void)
{
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = g_pmm_mgr->n_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
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

// 为boot页目录表申请一页空间
static void *boot_alloc_page(void)
{
    struct page_desc *p = alloc_page();
    assert(p != NULL);
    return page2kva(p);
}

// 根据线性地址la获取对应的页表项，如果create为true那么页表缺失的话自动创建
pte_t *get_pte(pde_t *pgdir, uintptr_t la, bool create)
{
    pde_t *pdep = &pgdir[PDX(la)];
    if (!(*pdep & PDE_P))
    {
        struct page_desc *page;
        if (!create || (page = alloc_page()) == NULL)
        {
            return NULL;
        }
        page->ref = 1;
        uintptr_t pa = page2pa(page);
        memset(KADDR(pa), 0, PG_SIZE);
        *pdep = pa | PTE_U | PTE_W | PTE_P;
    }
    return &((pte_t *)KADDR(PDE_ADDR(*pdep)))[PTX(la)];
}

// 获取线性地址la对应页描述符
struct page_desc *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store)
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

// 设定页表，将[la，la + size)映射到[pa, pa + size)上
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

// 开启页机制
static void enable_paging(void)
{
    lcr3(g_boot_cr3);

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
    // 初始化物理内存管理器
    pmm_manager_init();

    // 初始化页描述符和页的映射关系
    page_init();

    // 初始化boot页目录表
    g_boot_pgdir = boot_alloc_page();
    memset(g_boot_pgdir, 0, PG_SIZE);
    g_boot_cr3 = PADDR(g_boot_pgdir);

    // recursively insert boot_pgdir in itself
    // to form a virtual page table at virtual address VPT
    g_boot_pgdir[PDX(VPT)] = PADDR(g_boot_pgdir) | PDE_P | PDE_W;

    // 设定页表，将[KERN_BASE，KERN_BASE + KMEM_SIZE)映射到[0, 0 + KMEM_SIZE)上
    boot_map_segment(g_boot_pgdir, KERN_BASE, KMEM_SIZE, 0, PDE_W);

    // 临时设置线性地址[0, 4M)映射到物理地址[0, 4M)，确保内核能正常工作
    // 因为这时段机制还是生效的，会将[KERN_BASE, KERN_BASE+4M)映射[0, 4M)，如果这时开启页机制，那么没法对线性地址[0, 4M)做映射
    g_boot_pgdir[0] = g_boot_pgdir[PDX(KERN_BASE)];

    // 开启页机制
    enable_paging();

    // 最后再加载gdt，变为平坦模式，也就是逻辑地址等于线性地址，后续就靠页机制做重定位了
    gdt_init();

    // 这时可以取消临时的映射了
    g_boot_pgdir[0] = 0;
}

void *kmalloc(size_t n)
{
    void *ptr = NULL;
    struct page_desc *base = NULL;
    assert(n > 0 && n < 1024 * 0124);
    int num_pages = (n + PG_SIZE - 1) / PG_SIZE;
    base = alloc_pages(num_pages);
    assert(base != NULL);
    ptr = page2kva(base);
    return ptr;
}

void kfree(void *ptr, size_t n)
{
    assert(n > 0 && n < 1024 * 0124);
    assert(ptr != NULL);
    struct page_desc *base = NULL;
    int num_pages = (n + PG_SIZE - 1) / PG_SIZE;
    base = kva2page(ptr);
    free_pages(base, num_pages);
}

// page_remove_pte - free an Page sturct which is related linear address la
//                 - and clean(invalidate) pte which is related linear address la
// note: PT is changed, so the TLB need to be invalidate
static inline void page_remove_pte(pde_t *pgdir, uintptr_t la, pte_t *ptep)
{
    if (*ptep & PTE_P)
    {
        struct page_desc *page = pte2page(*ptep);
        page->ref--;
        if (page->ref == 0)
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
int page_insert(pde_t *pgdir, struct page_desc *page, uintptr_t la, uint32_t perm)
{
    pte_t *ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL)
    {
        return -E_NO_MEM;
    }
    page->ref++;
    if (*ptep & PTE_P)
    {
        struct page_desc *p = pte2page(*ptep);
        if (p == page)
        {
            page->ref--;
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

// pgdir_alloc_page - call alloc_page & page_insert functions to
//                  - allocate a page size memory & setup an addr map
//                  - pa<->la with linear address la and the PDT pgdir
struct page_desc *pgdir_alloc_page(pde_t *pgdir, uintptr_t la, uint32_t perm)
{
    struct page_desc *page = alloc_page();
    if (page != NULL)
    {
        if (page_insert(pgdir, page, la, perm) != 0)
        {
            free_page(page);
            return NULL;
        }
        if (swap_init_ok)
        {
            swap_map_swappable(check_mm_struct, la, page, 0);
            page->pra_vaddr = la;
            assert(page->ref == 1);
            // cprintf("get No. %d  page: pra_vaddr %x, pra_link.prev %x, pra_link_next %x in pgdir_alloc_page\n", (page-pages), page->pra_vaddr,page->pra_page_link.prev, page->pra_page_link.next);
        }
    }

    return page;
}
