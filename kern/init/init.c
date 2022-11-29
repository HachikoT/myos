#include "libs/string.h"
#include "kern/mm/pmm.h"
#include "kern/driver/stdio.h"
#include "kern/driver/console.h"
#include "kern/driver/picirq.h"
#include "kern/trap/trap.h"
#include "kern/driver/clock.h"
#include "kern/driver/intr.h"

// 内核入口
void kern_init(void)
{
    // bss段初始化清0
    extern char bss_start[], bss_end[];
    memset(bss_start, 0, bss_end - bss_start);

    cons_init(); // init the console

    const char *message = "myos is loading ...";
    cprintf("%s\n\n", message);

    pmm_init(); // init physical memory management

    pic_init(); // 初始化中断控制器
    idt_init(); // init interrupt descriptor table

    clock_init();  // init clock interrupt
    intr_enable(); // enable irq interrupt

    /* do nothing */
    while (1)
    {
    }
}
