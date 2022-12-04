#include "libs/stdarg.h"
#include "user/libs/stdio.h"
#include "libs/error.h"
#include "user/libs/ulib.h"

void __panic(const char *file, int line, const char *fmt, ...)
{
    // print the 'message'
    va_list ap;
    va_start(ap, fmt);
    cprintf("user panic at %s:%d:\n    ", file, line);
    vcprintf(fmt, ap);
    cprintf("\n");
    va_end(ap);
    exit(-E_PANIC);
}

void __warn(const char *file, int line, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    cprintf("user warning at %s:%d:\n    ", file, line);
    vcprintf(fmt, ap);
    cprintf("\n");
    va_end(ap);
}
