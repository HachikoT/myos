#include "kern/mm/mem_layout.h"

// 每一个中断向量在保存好中断现场和中断向量号后跳转到这里，继续构建trap_frame
.text
.globl __alltraps
__alltraps:
    // 继续压入寄存器数据构建好trap_frame
    pushl %ds
    pushl %es
    pushl %fs
    pushl %gs
    pushal

    // 设置好ds和es
    movl $GD_KDATA, %eax
    movw %ax, %ds
    movw %ax, %es

    // %esp指向trap_frame的地址，压入栈作为trap函数的参数
    pushl %esp

    // 可以调用trap(struct trap_frame* tf)了
    call trap

    // 参数出栈
    popl %esp

// 退出中断，逆序出栈数据
.globl __trapret
__trapret:
    popal
    popl %gs
    popl %fs
    popl %es
    popl %ds

    // 出栈中断号和错误码
    addl $0x8, %esp
    iret

.globl forkrets
forkrets:
    # set stack to this new process's trapframe
    movl 4(%esp), %esp
    jmp __trapret
