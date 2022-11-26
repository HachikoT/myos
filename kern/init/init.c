#include <defs.h>
#include <stdio.h>
#include <string.h>
#include <console.h>
#include <kdebug.h>
#include <picirq.h>
#include <trap.h>
#include <clock.h>
#include <intr.h>
#include <kmonitor.h>

#include "libs/string.h"
#include "kern/debug/kdebug.h"
#include "kern/mm/pmm.h"

void grade_backtrace(void);
static void lab1_switch_test(void);

// 内核入口
void kern_init(void)
{
    extern char edata[], end[];
    memset(edata, 0, end - edata);

    cons_init(); // init the console

    const char *message = "(THU.CST) os is loading ...";
    cprintf("%s\n\n", message);

    print_kerninfo();

    pmm_init(); // init physical memory management

    pic_init(); // init interrupt controller
    idt_init(); // init interrupt descriptor table

    clock_init();  // init clock interrupt
    intr_enable(); // enable irq interrupt

    /* do nothing */
    while (1)
    {
    }
}
