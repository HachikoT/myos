#include "kern/driver/intr.h"
#include "libs/x86.h"

// 允许外部中断
void intr_enable(void)
{
    sti();
}

// 禁止外部中断
void intr_disable(void)
{
    cli();
}
