#include "kern/trap/trap.h"
#include "libs/defs.h"
#include "libs/x86.h"
#include "kern/mm/mem_layout.h"
#include "kern/driver/stdio.h"
#include "kern/driver/clock.h"
#include "libs/string.h"
#include "kern/debug/assert.h"
#include "libs/error.h"
#include "kern/driver/picirq.h"
#include "kern/mm/vmm.h"
#include "kern/process/proc.h"
#include "kern/syscall/syscall.h"

#define TICK_NUM 100

static void print_ticks()
{
    cprintf("%d ticks\n", TICK_NUM);
}

// 中断向量表
static struct gate_desc g_idt[256];
static struct dt_desc g_idt_desc = {sizeof(g_idt) - 1, (uintptr_t)g_idt};

// 初始化中断描述符表
void idt_init(void)
{
    extern uintptr_t __vectors[];
    for (int i = 0; i < sizeof(g_idt) / sizeof(struct gate_desc); i++)
    {
        SET_IGATE(g_idt[i], __vectors[i], GD_KTEXT, DPL_KERNEL);
    }
    // 一般而言不检查门描述符里面的DPL，除了用软中断int n和单步中断int3，以及into引发的中断和异常
    // 这时候需要CPL必须小于等于DPL，也就是防止用户态程序随便调用内核的一些中断
    SET_TGATE(g_idt[T_SYSCALL], __vectors[T_SYSCALL], GD_KTEXT, DPL_USER);

    // 设置idt寄存器
    lidt(&g_idt_desc);
}

void print_regs(struct pushal_regs *regs)
{
    cprintf("  edi  0x%08x\n", regs->reg_edi);
    cprintf("  esi  0x%08x\n", regs->reg_esi);
    cprintf("  ebp  0x%08x\n", regs->reg_ebp);
    cprintf("  esp  0x%08x\n", regs->reg_esp);
    cprintf("  ebx  0x%08x\n", regs->reg_ebx);
    cprintf("  edx  0x%08x\n", regs->reg_edx);
    cprintf("  ecx  0x%08x\n", regs->reg_ecx);
    cprintf("  eax  0x%08x\n", regs->reg_eax);
}

static const char *trap_name(int trapno)
{
    static const char *const excnames[] = {
        "Divide error",
        "Debug",
        "Non-Maskable Interrupt",
        "Breakpoint",
        "Overflow",
        "BOUND Range Exceeded",
        "Invalid Opcode",
        "Device Not Available",
        "Double Fault",
        "Coprocessor Segment Overrun",
        "Invalid TSS",
        "Segment Not Present",
        "Stack Fault",
        "General Protection",
        "Page Fault",
        "(unknown trap)",
        "x87 FPU Floating-Point Error",
        "Alignment Check",
        "Machine-Check",
        "SIMD Floating-Point Exception"};

    if (trapno < sizeof(excnames) / sizeof(const char *const))
    {
        return excnames[trapno];
    }
    if (trapno >= IRQ_OFFSET && trapno < IRQ_OFFSET + 16)
    {
        return "Hardware Interrupt";
    }
    return "(unknown trap)";
}

static const char *IA32flags[] = {
    "CF",
    NULL,
    "PF",
    NULL,
    "AF",
    NULL,
    "ZF",
    "SF",
    "TF",
    "IF",
    "DF",
    "OF",
    NULL,
    NULL,
    "NT",
    NULL,
    "RF",
    "VM",
    "AC",
    "VIF",
    "VIP",
    "ID",
    NULL,
    NULL,
};

void print_trap_frame(struct trap_frame *tf)
{
    cprintf("trapframe at %p\n", tf);
    print_regs(&tf->tf_regs);
    cprintf("  ds   0x----%04x\n", tf->tf_ds);
    cprintf("  es   0x----%04x\n", tf->tf_es);
    cprintf("  fs   0x----%04x\n", tf->tf_fs);
    cprintf("  gs   0x----%04x\n", tf->tf_gs);
    cprintf("  trap 0x%08x %s\n", tf->tf_trapno, trap_name(tf->tf_trapno));
    cprintf("  err  0x%08x\n", tf->tf_err);
    cprintf("  eip  0x%08x\n", tf->tf_eip);
    cprintf("  cs   0x----%04x\n", tf->tf_cs);
    cprintf("  flag 0x%08x ", tf->tf_eflags);

    int i, j;
    for (i = 0, j = 1; i < sizeof(IA32flags) / sizeof(IA32flags[0]); i++, j <<= 1)
    {
        if ((tf->tf_eflags & j) && IA32flags[i] != NULL)
        {
            cprintf("%s,", IA32flags[i]);
        }
    }
    cprintf("IOPL=%d\n", (tf->tf_eflags & FL_IOPL_MASK) >> 12);

    if (!(tf->tf_cs == (uint16_t)KERNEL_CS))
    {
        cprintf("  esp  0x%08x\n", tf->tf_esp);
        cprintf("  ss   0x----%04x\n", tf->tf_ss);
    }
}

static inline void
print_pgfault(struct trap_frame *tf)
{
    /* error_code:
     * bit 0 == 0 means no page found, 1 means protection fault
     * bit 1 == 0 means read, 1 means write
     * bit 2 == 0 means kernel, 1 means user
     * */
    cprintf("page fault at 0x%08x: %c/%c [%s].\n", rcr2(),
            (tf->tf_err & 4) ? 'U' : 'K',
            (tf->tf_err & 2) ? 'W' : 'R',
            (tf->tf_err & 1) ? "protection fault" : "no page found");
}

static int pgfault_handler(struct trap_frame *tf)
{
    print_pgfault(tf);
    struct mm_struct *mm;
    if (g_cur_proc == NULL)
    {
        mm = check_mm_struct;
        // print_trap_frame(tf);
        // print_pgfault(tf);
        // panic("unhandled page fault.\n");
    }
    else
    {

        mm = g_cur_proc->mm;
    }
    // 缺页异常发生后
    // cr2寄存器会存储引起缺页异常的线性地址
    // 中断硬件压入的错误码
    //     - bit0（P）表示是页不存在（0），还是访问权限错误（1）
    //     - bit1（W/R）表示引起缺页异常的是读操作（0）还是写操作（1）
    //     - bit2（U/S）表示操作者的权限，内核态supervisor（0），用户态user（1）
    return do_pgfault(mm, tf->tf_err, rcr2());
}

// 按类型处理中断
static void trap_dispatch(struct trap_frame *tf)
{
    char c;
    int ret;

    switch (tf->tf_trapno)
    {
    case T_PGFLT:
        if ((ret = pgfault_handler(tf)) != 0)
        {
            panic("handle pgfault failed. ret=%d\n", ret);
            print_trap_frame(tf);
            if (g_cur_proc == NULL)
            {
                panic("handle pgfault failed. ret=%d\n", ret);
            }
            else
            {
                if (tf->tf_cs == (uint16_t)KERNEL_CS)
                {
                    panic("handle pgfault failed in kernel mode. ret=%d\n", ret);
                }
                cprintf("killed by kernel.\n");
                panic("handle user mode pgfault failed. ret=%d\n", ret);
                do_exit(-E_KILLED);
            }
        }
        break;
    case T_SYSCALL:
        syscall(tf);
        break;
    case IRQ_OFFSET + IRQ_TIMER:
        g_ticks++;
        if (g_ticks % 100 == 0)
        {
            print_ticks();
        }

        break;
    // case IRQ_OFFSET + IRQ_COM1:
    //     c = cons_getc();
    //     cprintf("serial [%03d] %c\n", c, c);
    //     break;
    // case IRQ_OFFSET + IRQ_KBD:
    //     c = cons_getc();
    //     cprintf("kbd [%03d] %c\n", c, c);
    //     break;
    case IRQ_OFFSET + IRQ_IDE1:
    case IRQ_OFFSET + IRQ_IDE2:
        /* do nothing */
        break;
    default:
        // in kernel, it must be a mistake
        if ((tf->tf_cs & 3) == 0)
        {
            print_trap_frame(tf);
        }
    }
}

/* trap_in_kernel - test if trap happened in kernel */
bool trap_in_kernel(struct trap_frame *tf)
{
    return (tf->tf_cs == (uint16_t)KERNEL_CS);
}

// 处理中断
void trap(struct trap_frame *tf)
{
    // dispatch based on what type of trap occurred
    // used for previous projects
    if (g_cur_proc == NULL)
    {
        trap_dispatch(tf);
    }
    else
    {
        // keep a trapframe chain in stack
        struct trap_frame *otf = g_cur_proc->tf;
        g_cur_proc->tf = tf;

        bool in_kernel = trap_in_kernel(tf);

        trap_dispatch(tf);

        g_cur_proc->tf = otf;
        if (!in_kernel)
        {
            if (g_cur_proc->flags & PF_EXITING)
            {
                do_exit(-E_KILLED);
            }
            if (g_cur_proc->need_resched)
            {
                schedule();
            }
        }
    }
}
