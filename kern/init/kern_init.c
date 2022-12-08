#include "libs/string.h"
#include "kern/driver/console.h"
#include "kern/driver/stdio.h"
#include "kern/mm/pmm.h"
#include "kern/driver/picirq.h"
#include "kern/trap/trap.h"
#include "kern/driver/clock.h"
#include "kern/driver/intr.h"
#include "kern/mm/vmm.h"
#include "kern/driver/ide.h"
#include "kern/process/proc.h"
#include "kern/schedule/sched.h"

// 内核入口
void kern_init(void)
{
    // bss段数据初始化清0
    extern char __bss_start[], __bss_end[];
    memset(__bss_start, 0, __bss_end - __bss_start);

    cons_init(); // 初始化控制台（键盘，显示）

    cprintf("myos is loading ...\n");

    pmm_init(); // 初始化物理内存管理

    pic_init(); // 初始化中断控制器
    idt_init(); // 初始化中断描述符表

    pic_enable(IRQ_KBD);

    clock_init();  // 初始化定时器
    intr_enable(); // 允许外部中断
    while (1)
    {
    }

    // vmm_init();   // init virtual memory management
    // sched_init(); // init scheduler
    // proc_init();  // init process table

    // ide_init();  // init ide devices
    // swap_init(); // init swap

    // cpu_idle(); // run idle process
}
