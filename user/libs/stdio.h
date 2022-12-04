#ifndef __KERN_DRIVE_STDIO_H__
#define __KERN_DRIVE_STDIO_H__

#include "libs/defs.h"
#include "libs/stdarg.h"

int cprintf(const char *fmt, ...);
int vcprintf(const char *fmt, va_list ap);
int cputs(const char *str);

void printfmt(void (*putch)(int, void *), void *putdat, const char *fmt, ...);
void vprintfmt(void (*putch)(int, void *), void *putdat, const char *fmt, va_list ap);
int snprintf(char *str, size_t size, const char *fmt, ...);
int vsnprintf(char *str, size_t size, const char *fmt, va_list ap);

#endif /* !__KERN_DRIVE_STDIO_H__z */
