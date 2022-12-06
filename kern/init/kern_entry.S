#include "libs/descriptor.h"
#include "kern/mm/mem_layout.h"

#define REALLOC(x) (x - KERN_BASE)

// 内核入口函数，将内核重定向到高1GB的内存空间，在没开启页机制之前先临时用段机制来做重定向
// 入口地址为0xC0100000，但实际加载到0x100000处，所以需要用段机制加上偏移-KERN_BASE
.text
.globl kern_entry
kern_entry:
    // 重新加载临时的gdt
    lgdt REALLOC(__gdt_desc)
    movw $KERNEL_DS, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %ss

    ljmp $KERNEL_CS, $relocated

relocated:
    // 设置好基址指针和栈指针寄存器的值，让其指向内核栈空间（8KB），不再用之前bootloader的栈了
    movl $0x0, %ebp
    movl $kern_stack_top, %esp

    // 新的内核栈准备好了，可以调用kern_init了
    call kern_init

// kern_init不应该退出，进入死循环
spin:
    jmp spin

// 定义内核的栈空间（8KB）
.data
.align PG_SIZE                  // 4KB对齐
    .globl kern_stack
kern_stack:
    .space KSTACK_SIZE
    .globl kern_stack_top
kern_stack_top:

// 全局描述符表
.align 4                                                    // 四字节对齐
__gdt:
    SEG_NULL_ASM                                            // 第0项，null段描述符
    SEG_DESC_ASM(STA_X | STA_R, -KERN_BASE, 0xFFFFFFFF)     // 第1项（可执行/可读代码段，基址为-KERN_BASE，范围4GB）
    SEG_DESC_ASM(STA_W, -KERN_BASE, 0xFFFFFFFF)             // 第2项（可读写数据段，基址为-KERN_BASE，范围4GB）

__gdt_desc:
    .word 0x17                                              // sizeof(__gdt) - 1
    .long REALLOC(__gdt)                                    // __gdt的地址
