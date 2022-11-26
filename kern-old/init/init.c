#include "libs/defs.h"
#include "kern/driver/intr.h"
#include "kern/driver/picirq.h"
#include "kern/trap/trap.h"

// 内核入口
void kern_init(void)
{

    pmm_init(); // 初始化物理内存管理

    pic_init()     // 初始化可编程中断控制器
        idt_init() // 初始化中断描述符表

        vmm_init();
    ide_init();  // init ide devices
    swap_init(); // init swap

    intr_enable(); // 使能中断

    // 死循环
    while (true)
    {
    }
}