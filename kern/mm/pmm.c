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
#include "kern/mm/kmalloc.h"
#include "libs/descriptor.h"
#include "kern/trap/trap.h"

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

// 初始化物理页
static void page_init(void)
{
    // boot中探测到的物理内存布局
    struct e820map *mem_map = (struct e820map *)(0x8000 + KERN_BASE);
    assert(12345 != mem_map->n_map);

    // 获取最大可用的物理内存地址（不超过KMEM_SIZE）
    uint64_t max_pa = 0;

    cprintf("e820map:\n");
    for (int i = 0; i < mem_map->n_map; i++)
    {
        uint64_t begin = mem_map->map[i].addr, end = begin + mem_map->map[i].size;
        cprintf("memory: %08llx, [%08llx, %08llx], type = %d.\n",
                mem_map->map[i].size, begin, end - 1, mem_map->map[i].type);
        if (mem_map->map[i].type == E820_MEM)
        {
            if (max_pa < end)
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

    // 将连续可用的内存页记录到内存管理器中
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

// 为内核页目录表申请一页空间
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

// 获取线性地址la对应物理页
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

// 设定页表，将线性地址[la，la + size)映射到物理地址[pa, pa + size)上
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
    // cr3寄存器保存页目录表的物理地址
    lcr3(g_boot_cr3);

    // cr0寄存器，控制了分页机制的开启
    uint32_t cr0 = rcr0();
    cr0 |= CR0_PE | CR0_PG | CR0_AM | CR0_WP | CR0_NE | CR0_TS | CR0_EM | CR0_MP;
    cr0 &= ~(CR0_TS | CR0_EM);
    lcr0(cr0);
}

// 全局描述符表
static struct seg_desc g_gdt[] = {
    SEG_NULL,
    [SEG_KTEXT] = SEG_DESC(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_KERNEL),
    [SEG_KDATA] = SEG_DESC(STA_W, 0x0, 0xFFFFFFFF, DPL_KERNEL),
    [SEG_UTEXT] = SEG_DESC(STA_X | STA_R, 0x0, 0xFFFFFFFF, DPL_USER),
    [SEG_UDATA] = SEG_DESC(STA_W, 0x0, 0xFFFFFFFF, DPL_USER),
    [SEG_TSS] = SEG_NULL};
static struct dt_desc g_gdt_desc = {sizeof(g_gdt) - 1, (uint32_t)g_gdt};

// 任务状态段
static struct task_state g_ts;

// 加载全局描述符表
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

// 初始化全局描述符表
static void gdt_init(void)
{
    // 当中断需要提权时，会切换堆栈，比如提升到ring0，那么就会切换到TSS中的esp0和ss0
    // 实际上每个用户进程会单独分配一个内核栈，这里初始化就先用内核本身的栈了
    g_ts.ts_esp0 = kern_stack_top;
    g_ts.ts_ss0 = KERNEL_DS;

    // initialize the TSS field of the gdt
    g_gdt[SEG_TSS] = SEG_1M_DESC(STS_T32A, (uint32_t)&g_ts, sizeof(g_ts), DPL_KERNEL);
    g_gdt[SEG_TSS].sd_s = 0;

    // 重新加载全局描述符表
    lgdt(&g_gdt_desc);

    // load the TSS
    ltr(GD_TSS);
}

// 初始化物理内存管理
void pmm_init(void)
{
    // 初始化物理内存管理器
    pmm_manager_init();

    // 初始化物理页
    page_init();

    // 初始化内核的页目录表
    g_boot_pgdir = boot_alloc_page();
    memset(g_boot_pgdir, 0, PG_SIZE);
    g_boot_cr3 = PADDR(g_boot_pgdir);

    // recursively insert boot_pgdir in itself
    // to form a virtual page table at virtual address VPT
    g_boot_pgdir[PDX(VPT)] = PADDR(g_boot_pgdir) | PDE_P | PDE_W;

    // 设定页表，将线性地址[KERN_BASE，KERN_BASE + KMEM_SIZE)映射到物理地址[0, 0 + KMEM_SIZE)上
    boot_map_segment(g_boot_pgdir, KERN_BASE, KMEM_SIZE, 0, PDE_W);

    // 临时设置线性地址[0, 4M)映射到物理地址[0, 4M)，确保内核能正常工作
    // 因为这时段机制还是生效的，如果这时开启页机制，那么没法对线性地址[0, 4M)做映射
    g_boot_pgdir[0] = g_boot_pgdir[PDX(KERN_BASE)];

    // 开启页机制
    enable_paging();

    // 最后再加载gdt，变为平坦模式，也就是逻辑地址等于线性地址，后续就靠页机制做重定位了
    gdt_init();

    // 这时可以取消临时的映射了
    g_boot_pgdir[0] = 0;

    kmalloc_init();
}

// 分配连续的n个页
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

        // if (page != NULL || n > 1 || swap_init_ok == 0)
        //     break;

        // extern struct mm_struct *check_mm_struct;
        // // cprintf("page %x, call swap_out in alloc_pages %d\n",page, n);
        // swap_out(check_mm_struct, n, 0);
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
            if (check_mm_struct != NULL)
            {
                swap_map_swappable(check_mm_struct, la, page, 0);
                page->pra_vaddr = la;
                assert(page->ref == 1);
                // cprintf("get No. %d  page: pra_vaddr %x, pra_link.prev %x, pra_link_next %x in pgdir_alloc_page\n", (page-pages), page->pra_vaddr,page->pra_page_link.prev, page->pra_page_link.next);
            }
            else
            { // now current is existed, should fix it in the future
              // swap_map_swappable(current->mm, la, page, 0);
              // page->pra_vaddr=la;
              // assert(page_ref(page) == 1);
              // panic("pgdir_alloc_page: no pages. now current is existed, should fix it in the future\n");
            }
        }
    }

    return page;
}

void unmap_range(pde_t *pgdir, uintptr_t start, uintptr_t end)
{
    assert(start % PG_SIZE == 0 && end % PG_SIZE == 0);
    assert(USER_ACCESS(start, end));

    do
    {
        pte_t *ptep = get_pte(pgdir, start, 0);
        if (ptep == NULL)
        {
            start = ROUND_DOWN(start + PT_SIZE, PT_SIZE);
            continue;
        }
        if (*ptep != 0)
        {
            page_remove_pte(pgdir, start, ptep);
        }
        start += PG_SIZE;
    } while (start != 0 && start < end);
}

void exit_range(pde_t *pgdir, uintptr_t start, uintptr_t end)
{
    assert(start % PG_SIZE == 0 && end % PG_SIZE == 0);
    assert(USER_ACCESS(start, end));

    start = ROUND_DOWN(start, PT_SIZE);
    do
    {
        int pde_idx = PDX(start);
        if (pgdir[pde_idx] & PTE_P)
        {
            free_page(pde2page(pgdir[pde_idx]));
            pgdir[pde_idx] = 0;
        }
        start += PT_SIZE;
    } while (start != 0 && start < end);
}

/* copy_range - copy content of memory (start, end) of one process A to another process B
 * @to:    the addr of process B's Page Directory
 * @from:  the addr of process A's Page Directory
 * @share: flags to indicate to dup OR share. We just use dup method, so it didn't be used.
 *
 * CALL GRAPH: copy_mm-->dup_mmap-->copy_range
 */
int copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end, bool share)
{
    assert(start % PG_SIZE == 0 && end % PG_SIZE == 0);
    assert(USER_ACCESS(start, end));
    // copy content by page unit.
    do
    {
        // call get_pte to find process A's pte according to the addr start
        pte_t *ptep = get_pte(from, start, 0), *nptep;
        if (ptep == NULL)
        {
            start = ROUND_DOWN(start + PT_SIZE, PT_SIZE);
            continue;
        }
        // call get_pte to find process B's pte according to the addr start. If pte is NULL, just alloc a PT
        if (*ptep & PTE_P)
        {
            if ((nptep = get_pte(to, start, 1)) == NULL)
            {
                return -E_NO_MEM;
            }
            uint32_t perm = (*ptep & PTE_USER);
            // get page from ptep
            struct page_desc *page = pte2page(*ptep);
            // alloc a page for process B
            struct page_desc *npage = alloc_page();
            assert(page != NULL);
            assert(npage != NULL);
            int ret = 0;
            /* LAB5:EXERCISE2 YOUR CODE
             * replicate content of page to npage, build the map of phy addr of nage with the linear addr start
             *
             * Some Useful MACROs and DEFINEs, you can use them in below implementation.
             * MACROs or Functions:
             *    page2kva(struct Page *page): return the kernel vritual addr of memory which page managed (SEE pmm.h)
             *    page_insert: build the map of phy addr of an Page with the linear addr la
             *    memcpy: typical memory copy function
             *
             * (1) find src_kvaddr: the kernel virtual address of page
             * (2) find dst_kvaddr: the kernel virtual address of npage
             * (3) memory copy from src_kvaddr to dst_kvaddr, size is PGSIZE
             * (4) build the map of phy addr of  nage with the linear addr start
             */
            void *kva_src = page2kva(page);
            void *kva_dst = page2kva(npage);

            memcpy(kva_dst, kva_src, PG_SIZE);

            ret = page_insert(to, npage, start, perm);
            assert(ret == 0);
        }
        start += PG_SIZE;
    } while (start != 0 && start < end);
    return 0;
}

/* *
 * The page directory entry corresponding to the virtual address range
 * [VPT, VPT + PTSIZE) points to the page directory itself. Thus, the page
 * directory is treated as a page table as well as a page directory.
 *
 * One result of treating the page directory as a page table is that all PTEs
 * can be accessed though a "virtual page table" at virtual address VPT. And the
 * PTE for number n is stored in vpt[n].
 *
 * A second consequence is that the contents of the current page directory will
 * always available at virtual address PGADDR(PDX(VPT), PDX(VPT), 0), to which
 * vpd is set bellow.
 * */
pte_t *const vpt = (pte_t *)VPT;
pde_t *const vpd = (pde_t *)PGADDR(PDX(VPT), PDX(VPT), 0);

// perm2str - use string 'u,r,w,-' to present the permission
static const char *
perm2str(int perm)
{
    static char str[4];
    str[0] = (perm & PTE_U) ? 'u' : '-';
    str[1] = 'r';
    str[2] = (perm & PTE_W) ? 'w' : '-';
    str[3] = '\0';
    return str;
}

// get_pgtable_items - In [left, right] range of PDT or PT, find a continuous linear addr space
//                   - (left_store*X_SIZE~right_store*X_SIZE) for PDT or PT
//                   - X_SIZE=PTSIZE=4M, if PDT; X_SIZE=PGSIZE=4K, if PT
//  paramemters:
//   left:        no use ???
//   right:       the high side of table's range
//   start:       the low side of table's range
//   table:       the beginning addr of table
//   left_store:  the pointer of the high side of table's next range
//   right_store: the pointer of the low side of table's next range
//  return value: 0 - not a invalid item range, perm - a valid item range with perm permission
static int
get_pgtable_items(size_t left, size_t right, size_t start, uintptr_t *table, size_t *left_store, size_t *right_store)
{
    if (start >= right)
    {
        return 0;
    }
    while (start < right && !(table[start] & PTE_P))
    {
        start++;
    }
    if (start < right)
    {
        if (left_store != NULL)
        {
            *left_store = start;
        }
        int perm = (table[start++] & PTE_USER);
        while (start < right && (table[start] & PTE_USER) == perm)
        {
            start++;
        }
        if (right_store != NULL)
        {
            *right_store = start;
        }
        return perm;
    }
    return 0;
}

void print_pgdir(void)
{
    cprintf("-------------------- BEGIN --------------------\n");
    size_t left, right = 0, perm;
    while ((perm = get_pgtable_items(0, N_PDE_ENTRY, right, vpd, &left, &right)) != 0)
    {
        cprintf("PDE(%03x) %08x-%08x %08x %s\n", right - left,
                left * PT_SIZE, right * PT_SIZE, (right - left) * PT_SIZE, perm2str(perm));
        size_t l, r = left * N_PTE_ENTRY;
        while ((perm = get_pgtable_items(left * N_PTE_ENTRY, right * N_PTE_ENTRY, r, vpt, &l, &r)) != 0)
        {
            cprintf("  |-- PTE(%05x) %08x-%08x %08x %s\n", r - l,
                    l * PG_SIZE, r * PG_SIZE, (r - l) * PG_SIZE, perm2str(perm));
        }
    }
    cprintf("--------------------- END ---------------------\n");
}