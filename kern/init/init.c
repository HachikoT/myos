#include "libs/string.h"
#include "kern/mm/pmm.h"
#include "kern/driver/stdio.h"
#include "kern/driver/console.h"
#include "kern/driver/picirq.h"
#include "kern/trap/trap.h"
#include "kern/driver/clock.h"
#include "kern/driver/intr.h"
#include "kern/mm/vmm.h"
#include "kern/driver/ide.h"
#include "kern/process/proc.h"

// 内核入口
void kern_init(void)
{
    // bss段数据初始化清0
    extern char __bss_start[], __bss_end[];
    memset(__bss_start, 0, __bss_end - __bss_start);

    cons_init(); // init the console

    const char *message = "myos is loading ...";
    cprintf("%s\n\n", message);

    pmm_init(); // init physical memory management

    pic_init(); // 初始化中断控制器
    idt_init(); // init interrupt descriptor table

    vmm_init();  // init virtual memory management
    proc_init(); // init process table

    ide_init();  // init ide devices
    swap_init(); // init swap

    clock_init();  // init clock interrupt
    intr_enable(); // enable irq interrupt

    cpu_idle(); // run idle process
}
