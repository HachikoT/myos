#include "kern/process/proc.h"
#include "libs/list.h"
#include "kern/mm/kmalloc.h"
#include "libs/string.h"
#include "kern/mm/mem_layout.h"
#include "kern/debug/assert.h"
#include "kern/driver/stdio.h"
#include "kern/mm/pmm.h"
#include "libs/stdlib.h"
#include "kern/sync/sync.h"
#include "kern/trap/trap.h"
#include "kern/schedule/sched.h"
#include "libs/error.h"
#include "libs/unistd.h"
#include "libs/elf.h"
#include "kern/fs/vfs/vfs.h"
#include "kern/fs/fs.h"
#include "kern/fs/file.h"

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

list_entry_t g_proc_list;                        // 进程列表
static list_entry_t g_hash_list[HASH_LIST_SIZE]; // 根据pid哈希之后的进程列表

struct proc_struct *g_idle_proc = NULL; // 内核idle进程
struct proc_struct *g_init_proc = NULL; // 内核init进程
struct proc_struct *g_cur_proc = NULL;  // 当前运行的进程

static int n_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trap_frame *tf);
void switch_to(struct context *from, struct context *to);

// set_links - set the relation links of process
static void
set_links(struct proc_struct *proc)
{
    list_add(&g_proc_list, &(proc->list_link));
    proc->yptr = NULL;
    if ((proc->optr = proc->parent->cptr) != NULL)
    {
        proc->optr->yptr = proc;
    }
    proc->parent->cptr = proc;
    n_process++;
}

// remove_links - clean the relation links of process
static void
remove_links(struct proc_struct *proc)
{
    list_del(&(proc->list_link));
    if (proc->optr != NULL)
    {
        proc->optr->yptr = proc->yptr;
    }
    if (proc->yptr != NULL)
    {
        proc->yptr->optr = proc->optr;
    }
    else
    {
        proc->parent->cptr = proc->optr;
    }
    n_process--;
}

// 创建一个proc对象
static struct proc_struct *alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL)
    {
        proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->runs = 0;
        proc->kstack = 0;
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL;
        memset(&(proc->context), 0, sizeof(struct context));
        proc->tf = NULL;
        proc->cr3 = g_boot_cr3;
        proc->flags = 0;
        memset(proc->name, 0, PROC_NAME_LEN);
        list_init(&(proc->list_link));
        list_init(&(proc->hash_link));
        proc->exit_code = 0;
        proc->wait_state = 0;
        proc->cptr = NULL;
        proc->optr = NULL;
        proc->yptr = NULL;
        proc->rq = NULL;
        list_init(&(proc->run_link));
        proc->time_slice = 0;
        skew_heap_init(&(proc->run_pool));
        proc->stride = 0;
        proc->priority = 0;
        proc->filesp = NULL;
    }
    return proc;
}

// set_proc_name - set the name of proc
char *set_proc_name(struct proc_struct *proc, const char *name)
{
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
char *
get_proc_name(struct proc_struct *proc)
{
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// get_pid - alloc a unique pid for process
static int get_pid(void)
{
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &g_proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++last_pid >= MAX_PID)
    {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe)
    {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list)
        {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid)
            {
                if (++last_pid >= next_safe)
                {
                    if (last_pid >= MAX_PID)
                    {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid)
            {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}

// 切换运行进程
void proc_run(struct proc_struct *proc)
{
    if (proc != g_cur_proc)
    {
        bool intr_flag;
        struct proc_struct *prev = g_cur_proc, *next = proc;
        local_intr_save(intr_flag);
        {
            g_cur_proc = proc;
            load_esp0(next->kstack + KSTACK_SIZE);
            lcr3(next->cr3);
            switch_to(&(prev->context), &(next->context));
        }
        local_intr_restore(intr_flag);
    }
}

// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the g_cur_proc proc will execute here.
static void forkret(void)
{
    forkrets(g_cur_proc->tf);
}

// hash_proc - add proc into proc g_hash_list
static void
hash_proc(struct proc_struct *proc)
{
    list_add(g_hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// unhash_proc - delete proc from proc g_hash_list
static void
unhash_proc(struct proc_struct *proc)
{
    list_del(&(proc->hash_link));
}

// find_proc - find proc frome proc g_hash_list according to pid
struct proc_struct *find_proc(int pid)
{
    if (0 < pid && pid < MAX_PID)
    {
        list_entry_t *list = g_hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list)
        {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid)
            {
                return proc;
            }
        }
    }
    return NULL;
}

// kernel_thread - create a kernel thread using "fn" function
// NOTE: the contents of temp trapframe tf will be copied to
//       proc->tf in do_fork-->copy_thread function
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
    struct trap_frame tf;
    memset(&tf, 0, sizeof(struct trap_frame));
    tf.tf_cs = KERNEL_CS;
    tf.tf_ds = tf.tf_es = tf.tf_ss = KERNEL_DS;
    tf.tf_regs.reg_ebx = (uint32_t)fn;
    tf.tf_regs.reg_edx = (uint32_t)arg;
    tf.tf_eip = (uint32_t)kernel_thread_entry;
    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

// 为进程分配内核栈8KB，用于用户进程中断用
static int setup_kstack(struct proc_struct *proc)
{
    struct page_desc *page = alloc_pages(KSTACK_PAGE);
    if (page != NULL)
    {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

// put_kstack - free the memory space of process kernel stack
static void
put_kstack(struct proc_struct *proc)
{
    free_pages(kva2page((void *)(proc->kstack)), KSTACK_PAGE);
}

// 初始化虚拟空间，包含内核空间
static int setup_pgdir(struct mm_struct *mm)
{
    struct page_desc *page;
    if ((page = alloc_page()) == NULL)
    {
        return -E_NO_MEM;
    }
    pde_t *pgdir = page2kva(page);
    memcpy(pgdir, g_boot_pgdir, PG_SIZE);
    pgdir[PDX(VPT)] = PADDR(pgdir) | PTE_P | PTE_W;
    mm->pgdir = pgdir;
    return 0;
}

// put_pgdir - free the memory space of PDT
static void
put_pgdir(struct mm_struct *mm)
{
    free_page(kva2page(mm->pgdir));
}

// 拷贝父进程打开的文件
static int copy_fs(uint32_t clone_flags, struct proc_struct *proc)
{
    struct files_struct *filesp, *old_filesp = g_cur_proc->filesp;
    assert(old_filesp != NULL);

    if (clone_flags & CLONE_FS)
    {
        filesp = old_filesp;
        goto good_files_struct;
    }

    int ret = -E_NO_MEM;
    if ((filesp = files_create()) == NULL)
    {
        goto bad_files_struct;
    }

    if ((ret = dup_fs(filesp, old_filesp)) != 0)
    {
        goto bad_dup_cleanup_fs;
    }

good_files_struct:
    files_count_inc(filesp);
    proc->filesp = filesp;
    return 0;

bad_dup_cleanup_fs:
    files_destroy(filesp);
bad_files_struct:
    return ret;
}

static void
put_fs(struct proc_struct *proc)
{
    struct files_struct *filesp = proc->filesp;
    if (filesp != NULL)
    {
        if (files_count_dec(filesp) == 0)
        {
            files_destroy(filesp);
        }
    }
}

// 拷贝父进程的虚拟空间
static int copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
    struct mm_struct *mm, *oldmm = g_cur_proc->mm;

    /* g_cur_proc is a kernel thread */
    if (oldmm == NULL)
    {
        return 0;
    }
    if (clone_flags & CLONE_VM)
    {
        mm = oldmm;
        goto good_mm;
    }

    int ret = -E_NO_MEM;
    if ((mm = mm_create()) == NULL)
    {
        goto bad_mm;
    }

    // 初始化虚拟空间，包含内核空间
    if (setup_pgdir(mm) != 0)
    {
        goto bad_pgdir_cleanup_mm;
    }

    lock_mm(oldmm);
    {
        ret = dup_mmap(mm, oldmm);
    }
    unlock_mm(oldmm);

    if (ret != 0)
    {
        goto bad_dup_cleanup_mmap;
    }

good_mm:
    mm->mm_count++;
    proc->mm = mm;
    proc->cr3 = PADDR(mm->pgdir);
    return 0;
bad_dup_cleanup_mmap:
    exit_mmap(mm);
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    return ret;
}

// copy_thread - setup the trapframe on the  process's kernel stack top and
//             - setup the kernel entry point and stack of process
static void copy_thread(struct proc_struct *proc, uintptr_t esp, struct trap_frame *tf)
{

    proc->tf = (struct trap_frame *)(proc->kstack + KSTACK_SIZE) - 1;
    *(proc->tf) = *tf;
    proc->tf->tf_regs.reg_eax = 0;
    proc->tf->tf_esp = esp;
    proc->tf->tf_eflags |= FL_IF;

    proc->context.eip = (uintptr_t)forkret;
    proc->context.esp = (uintptr_t)(proc->tf);
}

/* do_fork -     parent process for a new child process
 * @clone_flags: used to guide how to clone the child process
 * @stack:       the parent's user stack pointer. if stack==0, It means to fork a kernel thread.
 * @tf:          the trapframe info, which will be copied to child process's proc->tf
 */
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trap_frame *tf)
{
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (n_process >= MAX_PROCESS)
    {
        goto fork_out;
    }
    ret = -E_NO_MEM;

    if ((proc = alloc_proc()) == NULL)
    {
        goto fork_out;
    }

    proc->parent = g_cur_proc;

    if (setup_kstack(proc) != 0)
    {
        goto bad_fork_cleanup_proc;
    }

    // 拷贝父进程打开的文件
    if (copy_fs(clone_flags, proc) != 0)
    {
        goto bad_fork_cleanup_kstack;
    }

    // 拷贝父进程的虚拟空间
    if (copy_mm(clone_flags, proc) != 0)
    {
        goto bad_fork_cleanup_fs;
    }

    copy_thread(proc, stack, tf);

    bool intr_flag;
    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        set_links(proc);
    }
    local_intr_restore(intr_flag);

    wakeup_proc(proc);

    ret = proc->pid;
fork_out:
    return ret;

bad_fork_cleanup_fs:
    put_fs(proc);
bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}

// do_exit - called by sys_exit
//   1. call exit_mmap & put_pgdir & mm_destroy to free the almost all memory space of process
//   2. set process' state as PROC_ZOMBIE, then call wakeup_proc(parent) to ask parent reclaim itself.
//   3. call scheduler to switch to other process
int do_exit(int error_code)
{
    if (g_cur_proc == g_idle_proc)
    {
        panic("g_idle_proc exit.\n");
    }
    if (g_cur_proc == g_init_proc)
    {
        panic("g_init_proc exit.\n");
    }

    struct mm_struct *mm = g_cur_proc->mm;
    if (mm != NULL)
    {
        lcr3(g_boot_cr3);
        mm->map_count--;
        if (mm->map_count == 0)
        {
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);
        }
        g_cur_proc->mm = NULL;
    }
    g_cur_proc->state = PROC_ZOMBIE;
    g_cur_proc->exit_code = error_code;

    bool intr_flag;
    struct proc_struct *proc;
    local_intr_save(intr_flag);
    {
        proc = g_cur_proc->parent;
        if (proc->wait_state == WT_CHILD)
        {
            wakeup_proc(proc);
        }
        while (g_cur_proc->cptr != NULL)
        {
            proc = g_cur_proc->cptr;
            g_cur_proc->cptr = proc->optr;

            proc->yptr = NULL;
            if ((proc->optr = g_init_proc->cptr) != NULL)
            {
                g_init_proc->cptr->yptr = proc;
            }
            proc->parent = g_init_proc;
            g_init_proc->cptr = proc;
            if (proc->state == PROC_ZOMBIE)
            {
                if (g_init_proc->wait_state == WT_CHILD)
                {
                    wakeup_proc(g_init_proc);
                }
            }
        }
    }
    local_intr_restore(intr_flag);

    schedule();
    panic("do_exit will not return!! %d.\n", g_cur_proc->pid);
}

// do_wait - wait one OR any children with PROC_ZOMBIE state, and free memory space of kernel stack
//         - proc struct of this child.
// NOTE: only after do_wait function, all resources of the child proces are free.
int do_wait(int pid, int *code_store)
{
    struct mm_struct *mm = g_cur_proc->mm;
    if (code_store != NULL)
    {
        if (!user_mem_check(mm, (uintptr_t)code_store, sizeof(int), 1))
        {
            return -E_INVAL;
        }
    }

    struct proc_struct *proc;
    bool intr_flag, haskid;
repeat:
    haskid = 0;
    if (pid != 0)
    {
        proc = find_proc(pid);
        if (proc != NULL && proc->parent == g_cur_proc)
        {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE)
            {
                goto found;
            }
        }
    }
    else
    {
        proc = g_cur_proc->cptr;
        for (; proc != NULL; proc = proc->optr)
        {
            haskid = 1;
            if (proc->state == PROC_ZOMBIE)
            {
                goto found;
            }
        }
    }
    if (haskid)
    {
        g_cur_proc->state = PROC_SLEEPING;
        g_cur_proc->wait_state = WT_CHILD;
        schedule();
        if (g_cur_proc->flags & PF_EXITING)
        {
            do_exit(-E_KILLED);
        }
        goto repeat;
    }
    return -E_BAD_PROC;

found:
    if (proc == g_idle_proc || proc == g_init_proc)
    {
        panic("wait g_idle_proc or g_init_proc.\n");
    }
    if (code_store != NULL)
    {
        *code_store = proc->exit_code;
    }
    local_intr_save(intr_flag);
    {
        unhash_proc(proc);
        remove_links(proc);
    }
    local_intr_restore(intr_flag);
    put_kstack(proc);
    kfree(proc);
    return 0;
}

static int load_icode_read(int fd, void *buf, size_t len, off_t offset)
{
    int ret;
    if ((ret = sysfile_seek(fd, offset, LSEEK_SET)) != 0)
    {
        return ret;
    }
    if ((ret = sysfile_read(fd, buf, len)) != len)
    {
        return (ret < 0) ? ret : -1;
    }
    return 0;
}

/* load_icode - load the content of binary program(ELF format) as the new content of g_cur_proc process
 * @binary:  the memory addr of the content of binary program
 * @size:  the size of the content of binary program
 */
static int
load_icode(int fd, int argc, char **kargv)
{
    /* (1) create a new mm for current process
     * (2) create a new PDT, and mm->pgdir= kernel virtual addr of PDT
     * (3) copy TEXT/DATA/BSS parts in binary to memory space of process
     *    (3.1) read raw data content in file and resolve elfhdr
     *    (3.2) read raw data content in file and resolve proghdr based on info in elfhdr
     *    (3.3) call mm_map to build vma related to TEXT/DATA
     *    (3.4) callpgdir_alloc_page to allocate page for TEXT/DATA, read contents in file
     *          and copy them into the new allocated pages
     *    (3.5) callpgdir_alloc_page to allocate pages for BSS, memset zero in these pages
     * (4) call mm_map to setup user stack, and put parameters into user stack
     * (5) setup current process's mm, cr3, reset pgidr (using lcr3 MARCO)
     * (6) setup uargc and uargv in user stacks
     * (7) setup trapframe for user environment
     * (8) if up steps failed, you should cleanup the env.
     */
    assert(argc >= 0 && argc <= EXEC_MAX_ARG_NUM);

    if (g_cur_proc->mm != NULL)
    {
        panic("load_icode: g_cur_proc->mm must be empty.\n");
    }

    int ret = -E_NO_MEM;
    struct mm_struct *mm;
    if ((mm = mm_create()) == NULL)
    {
        goto bad_mm;
    }
    if (setup_pgdir(mm) != 0)
    {
        goto bad_pgdir_cleanup_mm;
    }

    struct page_desc *page;

    struct elf32_header __elf, *elf = &__elf;
    if ((ret = load_icode_read(fd, elf, sizeof(struct elf32_header), 0)) != 0)
    {
        goto bad_elf_cleanup_pgdir;
    }

    if (elf->e_magic != ELF_MAGIC)
    {
        ret = -E_INVAL_ELF;
        goto bad_elf_cleanup_pgdir;
    }

    struct elf32_phdr __ph, *ph = &__ph;
    uint32_t vm_flags, perm, phnum;
    for (phnum = 0; phnum < elf->e_phnum; phnum++)
    {
        off_t phoff = elf->e_phoff + sizeof(struct elf32_phdr) * phnum;
        if ((ret = load_icode_read(fd, ph, sizeof(struct elf32_phdr), phoff)) != 0)
        {
            goto bad_cleanup_mmap;
        }
        if (ph->p_type != ELF_PT_LOAD)
        {
            continue;
        }
        if (ph->p_filesz > ph->p_memsz)
        {
            ret = -E_INVAL_ELF;
            goto bad_cleanup_mmap;
        }
        if (ph->p_filesz == 0)
        {
            continue;
        }
        vm_flags = 0, perm = PTE_U;
        if (ph->p_flags & ELF_PF_X)
            vm_flags |= VM_EXEC;
        if (ph->p_flags & ELF_PF_W)
            vm_flags |= VM_WRITE;
        if (ph->p_flags & ELF_PF_R)
            vm_flags |= VM_READ;
        if (vm_flags & VM_WRITE)
            perm |= PTE_W;
        if ((ret = mm_map(mm, ph->p_va, ph->p_memsz, vm_flags, NULL)) != 0)
        {
            goto bad_cleanup_mmap;
        }
        off_t offset = ph->p_offset;
        size_t off, size;
        uintptr_t start = ph->p_va, end, la = ROUND_DOWN(start, PG_SIZE);

        ret = -E_NO_MEM;

        end = ph->p_va + ph->p_filesz;
        while (start < end)
        {
            if ((page = pgdir_alloc_page(mm, mm->pgdir, la, perm)) == NULL)
            {
                ret = -E_NO_MEM;
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PG_SIZE - off, la += PG_SIZE;
            if (end < la)
            {
                size -= la - end;
            }
            if ((ret = load_icode_read(fd, page2kva(page) + off, size, offset)) != 0)
            {
                goto bad_cleanup_mmap;
            }
            start += size, offset += size;
        }
        end = ph->p_va + ph->p_memsz;

        if (start < la)
        {
            /* ph->p_memsz == ph->p_filesz */
            if (start == end)
            {
                continue;
            }
            off = start + PG_SIZE - la, size = PG_SIZE - off;
            if (end < la)
            {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
            assert((end < la && start == end) || (end >= la && start == la));
        }
        while (start < end)
        {
            if ((page = pgdir_alloc_page(mm, mm->pgdir, la, perm)) == NULL)
            {
                ret = -E_NO_MEM;
                goto bad_cleanup_mmap;
            }
            off = start - la, size = PG_SIZE - off, la += PG_SIZE;
            if (end < la)
            {
                size -= la - end;
            }
            memset(page2kva(page) + off, 0, size);
            start += size;
        }
    }
    sysfile_close(fd);

    vm_flags = VM_READ | VM_WRITE | VM_STACK;
    if ((ret = mm_map(mm, USTACK_TOP - USTACK_SIZE, USTACK_SIZE, vm_flags, NULL)) != 0)
    {
        goto bad_cleanup_mmap;
    }
    assert(pgdir_alloc_page(mm, mm->pgdir, USTACK_TOP - PG_SIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm, mm->pgdir, USTACK_TOP - 2 * PG_SIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm, mm->pgdir, USTACK_TOP - 3 * PG_SIZE, PTE_USER) != NULL);
    assert(pgdir_alloc_page(mm, mm->pgdir, USTACK_TOP - 4 * PG_SIZE, PTE_USER) != NULL);

    mm->mm_count++;
    g_cur_proc->mm = mm;
    g_cur_proc->cr3 = PADDR(mm->pgdir);
    lcr3(PADDR(mm->pgdir));

    // setup argc, argv
    uint32_t argv_size = 0, i;
    for (i = 0; i < argc; i++)
    {
        argv_size += strnlen(kargv[i], EXEC_MAX_ARG_LEN + 1) + 1;
    }

    uintptr_t stacktop = USTACK_TOP - (argv_size / sizeof(long) + 1) * sizeof(long);
    char **uargv = (char **)(stacktop - argc * sizeof(char *));

    argv_size = 0;
    for (i = 0; i < argc; i++)
    {
        uargv[i] = strcpy((char *)(stacktop + argv_size), kargv[i]);
        argv_size += strnlen(kargv[i], EXEC_MAX_ARG_LEN + 1) + 1;
    }

    stacktop = (uintptr_t)uargv - sizeof(int);
    *(int *)stacktop = argc;

    struct trap_frame *tf = g_cur_proc->tf;
    memset(tf, 0, sizeof(struct trap_frame));
    tf->tf_cs = USER_CS;
    tf->tf_ds = tf->tf_es = tf->tf_ss = USER_DS;
    tf->tf_esp = stacktop;
    tf->tf_eip = elf->e_entry;
    tf->tf_eflags = FL_IF;
    ret = 0;

out:
    return ret;
bad_cleanup_mmap:
    exit_mmap(mm);
bad_elf_cleanup_pgdir:
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    goto out;
}

// this function isn't very correct in LAB8
static void
put_kargv(int argc, char **kargv)
{
    while (argc > 0)
    {
        kfree(kargv[--argc]);
    }
}

static int
copy_kargv(struct mm_struct *mm, int argc, char **kargv, const char **argv)
{
    int i, ret = -E_INVAL;
    if (!user_mem_check(mm, (uintptr_t)argv, sizeof(const char *) * argc, 0))
    {
        return ret;
    }
    for (i = 0; i < argc; i++)
    {
        char *buffer;
        if ((buffer = kmalloc(EXEC_MAX_ARG_LEN + 1)) == NULL)
        {
            goto failed_nomem;
        }
        if (!copy_string(mm, buffer, argv[i], EXEC_MAX_ARG_LEN + 1))
        {
            kfree(buffer);
            goto failed_cleanup;
        }
        kargv[i] = buffer;
    }
    return 0;

failed_nomem:
    ret = -E_NO_MEM;
failed_cleanup:
    put_kargv(i, kargv);
    return ret;
}

// do_execve - call exit_mmap(mm)&put_pgdir(mm) to reclaim memory space of current process
//           - call load_icode to setup new memory space accroding binary prog.
int do_execve(const char *name, int argc, const char **argv)
{
    static_assert(EXEC_MAX_ARG_LEN >= FS_MAX_FPATH_LEN);
    struct mm_struct *mm = g_cur_proc->mm;
    if (!(argc >= 1 && argc <= EXEC_MAX_ARG_NUM))
    {
        return -E_INVAL;
    }

    char local_name[PROC_NAME_LEN + 1];
    memset(local_name, 0, sizeof(local_name));

    char *kargv[EXEC_MAX_ARG_NUM];
    const char *path;

    int ret = -E_INVAL;

    lock_mm(mm);
    if (name == NULL)
    {
        snprintf(local_name, sizeof(local_name), "<null> %d", g_cur_proc->pid);
    }
    else
    {
        if (!copy_string(mm, local_name, name, sizeof(local_name)))
        {
            unlock_mm(mm);
            return ret;
        }
    }
    if ((ret = copy_kargv(mm, argc, kargv, argv)) != 0)
    {
        unlock_mm(mm);
        return ret;
    }
    path = argv[0];
    unlock_mm(mm);
    files_closeall(g_cur_proc->filesp);

    /* sysfile_open will check the first argument path, thus we have to use a user-space pointer, and argv[0] may be incorrect */
    int fd;
    if ((ret = fd = sysfile_open(path, O_RDONLY)) < 0)
    {
        goto execve_exit;
    }
    if (mm != NULL)
    {
        lcr3(g_boot_cr3);
        mm->mm_count--;
        if (mm->mm_count == 0)
        {
            exit_mmap(mm);
            put_pgdir(mm);
            mm_destroy(mm);
        }
        g_cur_proc->mm = NULL;
    }
    ret = -E_NO_MEM;
    ;
    if ((ret = load_icode(fd, argc, kargv)) != 0)
    {
        goto execve_exit;
    }
    put_kargv(argc, kargv);
    set_proc_name(g_cur_proc, local_name);
    return 0;

execve_exit:
    put_kargv(argc, kargv);
    do_exit(ret);
    panic("already exit: %e.\n", ret);
}

// do_yield - ask the scheduler to reschedule
int do_yield(void)
{
    g_cur_proc->need_resched = 1;
    return 0;
}

// do_kill - kill process with pid by set this process's flags with PF_EXITING
int do_kill(int pid)
{
    struct proc_struct *proc;
    if ((proc = find_proc(pid)) != NULL)
    {
        if (!(proc->flags & PF_EXITING))
        {
            proc->flags |= PF_EXITING;
            if (proc->wait_state & WT_INTERRUPTED)
            {
                wakeup_proc(proc);
            }
            return 0;
        }
        return -E_KILLED;
    }
    return -E_INVAL;
}

// kernel_execve - do SYS_exec syscall to exec a user program called by user_main kernel_thread
static int kernel_execve(const char *name, const char **argv)
{
    int argc = 0, ret;
    while (argv[argc] != NULL)
    {
        argc++;
    }
    __asm__ __volatile__(
        "int %1;"
        : "=a"(ret)
        : "i"(T_SYSCALL), "0"(SYS_exec), "d"(name), "c"(argc), "b"(argv)
        : "memory");
    return ret;
}

#define __KERNEL_EXECVE(name, path, ...) ({              \
    const char *argv[] = {path, ##__VA_ARGS__, NULL};    \
    cprintf("kernel_execve: pid = %d, name = \"%s\".\n", \
            g_cur_proc->pid, name);                      \
    kernel_execve(name, argv);                           \
})

#define KERNEL_EXECVE(x, ...) __KERNEL_EXECVE(#x, #x, ##__VA_ARGS__)

#define KERNEL_EXECVE2(x, ...) KERNEL_EXECVE(x, ##__VA_ARGS__)

#define __KERNEL_EXECVE3(x, s, ...) KERNEL_EXECVE(x, #s, ##__VA_ARGS__)

#define KERNEL_EXECVE3(x, s, ...) __KERNEL_EXECVE3(x, s, ##__VA_ARGS__)

// user_main - kernel thread used to exec a user program
static int user_main(void *arg)
{
    KERNEL_EXECVE(user);
    panic("user_main execve failed.\n");
}

// init_main - the second kernel thread used to create user_main kernel threads
int init_main(void *arg)
{
    int ret;
    if ((ret = vfs_set_bootfs("disk0:")) != 0)
    {
        panic("set boot fs failed: %e.\n", ret);
    }

    size_t n_free_pages_store = n_free_pages();
    size_t kernel_allocated_store = kallocated();

    int pid = kernel_thread(user_main, NULL, 0);
    if (pid <= 0)
    {
        panic("create user_main failed.\n");
    }

    while (do_wait(0, NULL) == 0)
    {
        schedule();
    }

    fs_cleanup();

    cprintf("all user-mode processes have quit.\n");
    assert(g_init_proc->cptr == NULL && g_init_proc->yptr == NULL && g_init_proc->optr == NULL);
    assert(n_process == 2);
    assert(list_next(&g_proc_list) == &(g_init_proc->list_link));
    assert(list_prev(&g_proc_list) == &(g_init_proc->list_link));
    assert(n_free_pages_store == n_free_pages());
    assert(kernel_allocated_store == kallocated());
    return 0;
}

// proc_init - set up the first kernel thread g_idle_proc "idle" by itself and
//           - create the second kernel thread init_main
void proc_init(void)
{
    list_init(&g_proc_list);
    for (int i = 0; i < HASH_LIST_SIZE; i++)
    {
        list_init(g_hash_list + i);
    }

    if ((g_idle_proc = alloc_proc()) == NULL)
    {
        panic("cannot alloc g_idle_proc.\n");
    }

    // g_idle_proc成为第0个进程
    g_idle_proc->pid = 0;
    g_idle_proc->state = PROC_RUNNABLE;
    g_idle_proc->kstack = (uintptr_t)kern_stack;
    g_idle_proc->need_resched = 1;
    set_proc_name(g_idle_proc, "idle");
    if ((g_idle_proc->filesp = files_create()) == NULL)
    {
        panic("create filesp (idleproc) failed.\n");
    }
    files_count_inc(g_idle_proc->filesp);

    n_process++;

    g_cur_proc = g_idle_proc;

    int pid = kernel_thread(init_main, NULL, 0);
    if (pid <= 0)
    {
        panic("create init_main failed.\n");
    }

    g_init_proc = find_proc(pid);
    set_proc_name(g_init_proc, "init");
}

// cpu_idle - at the end of kern_init, the first kernel thread g_idle_proc will do below works
void cpu_idle(void)
{
    while (1)
    {
        // if (g_cur_proc->need_resched)
        // {
        cprintf("idle proc\n");
        schedule();
        // }
    }
}
