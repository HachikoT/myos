#ifndef __KERN_DRIVE_STDIO_H__
#define __KERN_DRIVE_STDIO_H__

#include "libs/stdarg.h"

/* kern/libs/stdio.c */
int cprintf(const char *fmt, ...);
int vcprintf(const char *fmt, va_list ap);
void cputchar(int c);
int cputs(const char *str);
int getchar(void);

#endif /* !__KERN_DRIVE_STDIO_H__z */
