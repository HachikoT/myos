#include "libs/defs.h"
#include "user/libs/syscall.h"
#include "user/libs/stdio.h"
#include "user/libs/ulib.h"

void exit(int error_code)
{
    sys_exit(error_code);
    cprintf("BUG: exit failed.\n");
    while (1)
        ;
}

int fork(void)
{
    return sys_fork();
}

int wait(void)
{
    return sys_wait(0, NULL);
}

int waitpid(int pid, int *store)
{
    return sys_wait(pid, store);
}

void yield(void)
{
    sys_yield();
}

int kill(int pid)
{
    return sys_kill(pid);
}

int getpid(void)
{
    return sys_getpid();
}

// print_pgdir - print the PDT&PT
void print_pgdir(void)
{
    sys_pgdir();
}
