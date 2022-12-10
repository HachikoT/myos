#ifndef __KERN_PROCESS_PROC_H__
#define __KERN_PROCESS_PROC_H__

#include "libs/defs.h"
#include "libs/list.h"
#include "kern/trap/trap.h"
#include "kern/mm/vmm.h"
#include "libs/skew_heap.h"
#include "kern/schedule/sched.h"
#include "kern/fs/fs.h"

/* fork flags used in do_fork*/
#define CLONE_VM 0x00000100     // set if VM shared between processes
#define CLONE_THREAD 0x00000200 // thread group

// process's state in his life cycle
enum proc_state
{
    PROC_UNINIT = 0, // uninitialized
    PROC_SLEEPING,   // sleeping
    PROC_RUNNABLE,   // runnable(maybe running)
    PROC_ZOMBIE,     // almost dead, and wait parent proc to reclaim his resource
};

// 一个进程执行的上下文
// 不包含eax和eflags因为进程都要调用switch_to才切换，相当于进程要先陷入switch_to才会切换
// 而switch_to上下文保证了会自动维护好eax和eflags的值
struct context
{
    uint32_t eip;
    uint32_t esp;
    uint32_t ebx;
    uint32_t ecx;
    uint32_t edx;
    uint32_t esi;
    uint32_t edi;
    uint32_t ebp;
};

#define PROC_NAME_LEN 15
#define MAX_PROCESS 4096
#define MAX_PID (MAX_PROCESS * 2)

#define PF_EXITING 0x00000001 // getting shutdown

#define WT_INTERRUPTED 0x80000000              // the wait state could be interrupted
#define WT_CHILD (0x00000001 | WT_INTERRUPTED) // wait child process
#define WT_KSEM 0x00000100                     // wait kernel semaphore
#define WT_TIMER (0x00000002 | WT_INTERRUPTED) // wait timer
#define WT_KBD (0x00000004 | WT_INTERRUPTED)   // wait the input of keyboard

extern list_entry_t g_proc_list;

struct proc_struct
{
    enum proc_state state;        // 进程的状态
    int pid;                      // 进程ID
    int runs;                     // the running times of Proces
    uintptr_t kstack;             // 用户进程的内核栈（栈底），中断的时候会用到
    volatile bool need_resched;   // 是否需要调度
    struct proc_struct *parent;   // 父进程
    struct mm_struct *mm;         // mm对象，管理进程的虚拟空间
    struct context context;       // 进程切换上下文
    struct trap_frame *tf;        // 中断的相关数据
    uintptr_t cr3;                // 当前进程的页目录表地址
    uint32_t flags;               // Process flag
    char name[PROC_NAME_LEN + 1]; // 进程名
    list_entry_t list_link;       // Process link list
    list_entry_t hash_link;       // Process hash list
    int exit_code;                // 退出的时候的代码
    uint32_t wait_state;          // waiting state
    struct proc_struct *cptr;     // child 子进程
    struct proc_struct *yptr;     // younger sibling 左边的兄弟
    struct proc_struct *optr;     // older sibling 右边的兄弟
    struct run_queue *rq;         // running queue contains Process
    list_entry_t run_link;        // the entry linked in run queue
    int time_slice;               // time slice for occupying the CPU
    skew_heap_entry_t run_pool;   // the entry in the run pool
    uint32_t stride;              // 进程的步进值，越小的越先被调度，每次调度就加上进程的优先级（stride调度算法）
    uint32_t priority;            // 进程的优先级
    struct files_struct *filesp;  // 进程的打开文件信息
};

#define le2proc(le, member) \
    to_struct((le), struct proc_struct, member)

extern struct proc_struct *g_idle_proc, *g_init_proc, *g_cur_proc;

void proc_init(void);
void proc_run(struct proc_struct *proc);
int kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags);

char *set_proc_name(struct proc_struct *proc, const char *name);
char *get_proc_name(struct proc_struct *proc);
void cpu_idle(void);

struct proc_struct *find_proc(int pid);
int do_fork(uint32_t clone_flags, uintptr_t stack, struct trap_frame *tf);
int do_exit(int error_code);

// FOR LAB6, set the process's priority (bigger value will get more CPU time)
void lab6_set_priority(uint32_t priority);

#endif /* !__KERN_PROCESS_PROC_H__ */
