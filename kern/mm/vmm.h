#ifndef __KERN_MM_VMM_H__
#define __KERN_MM_VMM_H__

#include "libs/defs.h"
#include "libs/list.h"
#include "kern/mm/mem_layout.h"
#include "kern/sync/sync.h"
#include "kern/sync/sem.h"
#include "kern/mm/mmu.h"

#define le2vma(le, member) \
    to_struct((le), struct vma_struct, member)

#define VM_READ 0x00000001
#define VM_WRITE 0x00000002
#define VM_EXEC 0x00000004
#define VM_STACK 0x00000008

struct mm_struct;
// 某一块连续的虚拟空间
struct vma_struct
{
    struct mm_struct *vm_mm; // the set of vma using the same PDT
    uintptr_t vm_start;      // start addr of vma
    uintptr_t vm_end;        // end addr of vma
    uint32_t vm_flags;       // flags of vma
    list_entry_t list_link;  // linear list link which sorted by start addr of vma
};

// 管理一个进程整个虚拟空间
struct mm_struct
{
    list_entry_t mmap_list;        // linear list link which sorted by start addr of vma
    struct vma_struct *mmap_cache; // g_cur_proc accessed vma, used for speed purpose
    pde_t *pgdir;                  // the PDT of these vma
    int map_count;                 // the count of these vma
    void *sm_priv;                 // the private data for swap manager
    int mm_count;                  // the number ofprocess which shared the mm
    semaphore_t mm_sem;            // mutex for using dup_mmap fun to duplicat the mm
    int locked_by;                 // the lock owner process's pid
};

static inline void lock_mm(struct mm_struct *mm)
{
    if (mm != NULL)
    {
        down(&(mm->mm_sem));
        // if (g_cur_proc != NULL)
        // {
        //     mm->locked_by = g_cur_proc->pid;
        // }
    }
}

static inline void unlock_mm(struct mm_struct *mm)
{
    if (mm != NULL)
    {
        up(&(mm->mm_sem));
        mm->locked_by = 0;
    }
}

void vmm_init(void); // 初始化虚拟内存管理

struct vma_struct *vma_create(uintptr_t vm_start, uintptr_t vm_end, uint32_t vm_flags); // 创建vma对象

struct mm_struct *mm_create(void);                                    // 创建mm对象
void mm_destroy(struct mm_struct *mm);                                // 销毁mm对象
void insert_vma_struct(struct mm_struct *mm, struct vma_struct *vma); // 在mm中插入vma，必须保证新的vma不会和已有的相交
struct vma_struct *find_vma(struct mm_struct *mm, uintptr_t addr);    // 在mm中查找包含addr地址的vma

int mm_map(struct mm_struct *mm, uintptr_t addr, size_t len, uint32_t vm_flags,
           struct vma_struct **vma_store);
int dup_mmap(struct mm_struct *to, struct mm_struct *from);
void exit_mmap(struct mm_struct *mm);

bool user_mem_check(struct mm_struct *mm, uintptr_t start, size_t len, bool write);
bool copy_from_user(struct mm_struct *mm, void *dst, const void *src, size_t len, bool writable);
bool copy_to_user(struct mm_struct *mm, void *dst, const void *src, size_t len);
bool copy_string(struct mm_struct *mm, char *dst, const char *src, size_t maxn);

int do_pgfault(struct mm_struct *mm, uint32_t error_code, uintptr_t addr);

extern volatile unsigned int pgfault_num;
extern struct mm_struct *check_mm_struct;

#endif // __KERN_MM_VMM_H__
