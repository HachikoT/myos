#ifndef __KERN_SYSCALL_SYSCALL_H__
#define __KERN_SYSCALL_SYSCALL_H__

#include "kern/trap/trap.h"

void syscall(struct trap_frame *tf);

#endif /* !__KERN_SYSCALL_SYSCALL_H__ */
