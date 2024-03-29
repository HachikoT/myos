#include "libs/defs.h"
#include "kern/syscall/syscall.h"
#include "kern/process/proc.h"
#include "kern/driver/stdio.h"
#include "kern/mm/pmm.h"
#include "kern/debug/assert.h"
#include "kern/trap/trap.h"
#include "libs/unistd.h"
#include "kern/driver/clock.h"

static int
sys_exit(uint32_t arg[])
{
    int error_code = (int)arg[0];
    return do_exit(error_code);
}

static int
sys_fork(uint32_t arg[])
{
    struct trap_frame *tf = g_cur_proc->tf;
    uintptr_t stack = tf->tf_esp;
    return do_fork(0, stack, tf);
}

static int
sys_wait(uint32_t arg[])
{
    int pid = (int)arg[0];
    int *store = (int *)arg[1];
    return do_wait(pid, store);
}

static int sys_exec(uint32_t arg[])
{
    const char *name = (const char *)arg[0];
    int argc = (int)arg[1];
    const char **argv = (const char **)arg[2];
    return do_execve(name, argc, argv);
}

static int
sys_yield(uint32_t arg[])
{
    return do_yield();
}

static int
sys_kill(uint32_t arg[])
{
    int pid = (int)arg[0];
    return do_kill(pid);
}

static int
sys_getpid(uint32_t arg[])
{
    return g_cur_proc->pid;
}

static int
sys_putc(uint32_t arg[])
{
    int c = (int)arg[0];
    cputchar(c);
    return 0;
}

static int
sys_pgdir(uint32_t arg[])
{
    print_pgdir();
    return 0;
}

static uint32_t
sys_gettime(uint32_t arg[])
{
    return (int)g_ticks;
}

static int (*syscalls[])(uint32_t arg[]) = {
    [SYS_exit] = sys_exit,
    [SYS_fork] = sys_fork,
    [SYS_wait] = sys_wait,
    [SYS_exec] = sys_exec,
    [SYS_yield] = sys_yield,
    [SYS_kill] = sys_kill,
    [SYS_getpid] = sys_getpid,
    [SYS_putc] = sys_putc,
    [SYS_pgdir] = sys_pgdir,
    [SYS_gettime] = sys_gettime,
};

#define NUM_SYSCALLS ((sizeof(syscalls)) / (sizeof(syscalls[0])))

void syscall(struct trap_frame *tf)
{
    uint32_t arg[5];
    int num = tf->tf_regs.reg_eax;
    if (num >= 0 && num < NUM_SYSCALLS)
    {
        if (syscalls[num] != NULL)
        {
            arg[0] = tf->tf_regs.reg_edx;
            arg[1] = tf->tf_regs.reg_ecx;
            arg[2] = tf->tf_regs.reg_ebx;
            arg[3] = tf->tf_regs.reg_edi;
            arg[4] = tf->tf_regs.reg_esi;
            tf->tf_regs.reg_eax = syscalls[num](arg);
            return;
        }
    }
    print_trap_frame(tf);
    panic("undefined syscall %d, pid = %d, name = %s.\n",
          num, g_cur_proc->pid, g_cur_proc->name);
}
