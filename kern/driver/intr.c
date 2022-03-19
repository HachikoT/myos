#include "libs/x86.h"
#include "kern/driver/intr.h"

// 使能中断
void intr_enable(void) {
    sti();
}

// 禁止中断
void intr_disable(void) {
    cli();
}
