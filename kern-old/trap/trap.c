#include "libs/defs.h"
#include "libs/x86.h"
#include "kern/mm/mmu.h"
#include "kern/mm/memlayout.h"
#include "kern/trap/trap.h"

// 中断描述符表
static struct gate_desc idt[256] = {{0}};

static struct pseudo_desc idt_pd = {
    sizeof(idt) - 1, (uintptr_t)idt
};

// 初始化中断描述符表
void idt_init(void) {
    // 设置中断向量对应的描述符
    extern uintptr_t __vectors[];
    for (int i = 0; i < sizeof(idt) / sizeof(struct gate_desc); i++) {
        // false代表中断门，GD_KTEXT表示使用内核代码段，段偏移地址为__vectors[i]，DPL_KERNEL表示中断特权级
        SET_GATE(idt[i], false, GD_KTEXT, __vectors[i], DPL_KERNEL);
    }

    // 将中断向量表地址记录到IDTR寄存器
    lidt(&idt_pd);
}

/* trap_dispatch - dispatch based on what type of trap occurred */
static void trap_dispatch(struct trapframe *tf) {
    switch (tf->tf_trapno) {
    case IRQ_OFFSET + IRQ_TIMER:
        break;
    case IRQ_OFFSET + IRQ_COM1:
        break;
    case IRQ_OFFSET + IRQ_KBD:
        break;
    case IRQ_OFFSET + IRQ_IDE1:
    case IRQ_OFFSET + IRQ_IDE2:
        /* do nothing */
        break;
    default:
    }
}

/* *
 * trap - handles or dispatches an exception/interrupt. if and when trap() returns,
 * the code in kern/trap/trapentry.S restores the old CPU state saved in the
 * trapframe and then uses the iret instruction to return from the exception.
 * */
void trap(struct trapframe *tf) {
    // dispatch based on what type of trap occurred
    trap_dispatch(tf);
}
