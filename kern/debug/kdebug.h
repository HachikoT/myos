#ifndef __KERN_DEBUG_KDEBUG_H__
#define __KERN_DEBUG_KDEBUG_H__

#include "libs/defs.h"
#include "kern/trap/trap.h"

void print_kerninfo(void);
void print_stackframe(void);
void print_debuginfo(uintptr_t eip);

#endif /* !__KERN_DEBUG_KDEBUG_H__ */
