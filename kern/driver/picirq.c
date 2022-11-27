#include "kern/driver/picirq.h"
#include "libs/defs.h"
#include "libs/x86.h"

// 操作主从PIC的端口
#define PIC1_PORT0 0x20
#define PIC1_PORT1 0x21
#define PIC2_PORT0 0xA0
#define PIC2_PORT1 0xA1

// 从片连接到主片的IRQ2端口上
#define IRQ_SLAVE 2

// irq端口掩码，1表示关闭，0表示开启，初始值表示连接从片的IRQ2是开启的
static uint16_t irq_mask = 0xFFFF & ~(1 << IRQ_SLAVE);
static bool did_init = false;

// 使能特定的中断请求端口（IRQ Interrupt Request），允许IRQ 0~15
void pic_enable(unsigned irq)
{
    irq_mask = irq_mask & ~(1 << irq);
    if (did_init)
    {
        // 写入OCW1，指定要屏蔽的IRQ端口
        outb(PIC1_PORT1, irq_mask);
        outb(PIC2_PORT1, irq_mask >> 8);
    }
}

// 初始化可编程中断控制器（PIC Programmable Interrupt Controller）8259A
void pic_init(void)
{
    did_init = true;

    // 先屏蔽主从8259A的所有中断，写入OCW1
    outb(PIC1_PORT1, 0xFF);
    outb(PIC2_PORT1, 0xFF);

    // 设置主片 (8259A-1)

    // ICW1:  0001g0hi
    //    g:  0 = edge triggering, 1 = level triggering
    //    h:  0 = cascaded PICs, 1 = master only
    //    i:  0 = no ICW4, 1 = ICW4 required
    // 选择（g）外部中断请求信号为上升沿触发，（h）多片8259A级联，（i）要发送ICW4配置
    outb(PIC1_PORT0, 0x11);

    // ICW2: 中断起始偏移量
    // x86中，0-31的中断号Intel保留，不分配给IRQ，为异常。所以需要设置起始偏移量为32。
    outb(PIC1_PORT1, IRQ_OFFSET);

    // ICW3: 设置主片的哪一个IRQ端口用来连接从片
    outb(PIC1_PORT1, 1 << IRQ_SLAVE);

    // ICW4:  000nbmap
    //    n:  1 = special fully nested mode
    //    b:  1 = buffered mode
    //    m:  0 = slave PIC, 1 = master PIC
    //        (ignored when b is 0, as the master/slave role
    //         can be hardwired).
    //    a:  1 = Automatic EOI mode
    //    p:  0 = MCS-80/85 mode, 1 = intel x86 mode
    // 采用一般全嵌套模式：即当某个中断正在服务时，本级中断和低优先级的中断都被屏蔽，只有更高优先级的中断才会响应
    // 自动EOI模式：即在中断响应时，在8259A发送出中断矢量之后，自动将相应ISR复位
    outb(PIC1_PORT1, 0x3);

    // 设置从片 (8259A-2)
    outb(PIC2_PORT0, 0x11);           // ICW1
    outb(PIC2_PORT1, IRQ_OFFSET + 8); // ICW2，这里偏移值再加8
    outb(PIC2_PORT1, IRQ_SLAVE);      // ICW3，表示从片连接到主片的IRQ2端口
    // NB Automatic EOI mode doesn't tend to work on the slave.
    // Linux source code says it's "to be investigated".
    outb(PIC2_PORT1, 0x3); // ICW4

    // OCW3:  0ef01prs
    //   ef:  0x = NOP, 10 = clear specific mask, 11 = set specific mask
    //    p:  0 = no polling, 1 = polling mode
    //   rs:  0x = NOP, 10 = read IRR, 11 = read ISR
    outb(PIC1_PORT0, 0x68); // clear specific mask
    outb(PIC1_PORT0, 0x0a); // read IRR by default

    outb(PIC2_PORT0, 0x68); // OCW3
    outb(PIC2_PORT0, 0x0a); // OCW3
}
