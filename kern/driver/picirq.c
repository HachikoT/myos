#include "libs/defs.h"
#include "libs/x86.h"
#include "kern/driver/picirq.h"

// 两个pic的IO端口号
#define IO_PIC1_CMD         0x20    // master命令端口 (IRQs 0-7)
#define IO_PIC1_DATA        0x21    // master数据端口 (IRQs 0-7)
#define IO_PIC2_CMD         0xA0    // slave命令端口 (IRQs 8-15)
#define IO_PIC2_DATA        0xA1    // slave控制端口 (IRQs 8-15)

#define IRQ_SLAVE           2       // 从pic级联到主pic的IRQ端口号
#define IRQ_OFFSET          32      // 中断向量偏移

// Current IRQ mask.
// Initial IRQ mask has interrupt 2 enabled (for slave 8259A).
static uint16_t irq_mask = 0xFFFF & ~(1 << IRQ_SLAVE);
static bool     did_init = false;

static void pic_setmask(uint16_t mask) {
    irq_mask = mask;
    if (did_init) {
        outb(IO_PIC1_DATA, mask);
        outb(IO_PIC2_DATA, mask >> 8);
    }
}

// 使能特定irq端口，0-15
void pic_enable(unsigned int irq) {
    pic_setmask(irq_mask & ~(1 << irq));
}

// 初始化可编程中断控制器（Programmable Interrupt Controller）8259A
// 8259A芯片控制外部中断，有主从两个pic，从pic级联到主pic的IRQ2接口
void pic_init(void) {
    did_init = true;

    // 还未初始化，先屏蔽主从8259A的所有中断
    outb(IO_PIC1_DATA, 0xFF);
    outb(IO_PIC2_DATA, 0xFF);

    // 初始化主8259A（8259A-1）

    // ICW1:  0001g0hi
    //    g:  0 = edge triggering, 1 = level triggering
    //    h:  0 = cascaded PICs, 1 = master only
    //    i:  0 = no ICW4, 1 = ICW4 required
    // 初始化命令字，选择（g）外部中断请求信号为上升沿触发，（h）多片8259A级联，（i）要发送ICW4配置
    outb(IO_PIC1_CMD, 0x11);

    // ICW2:  Vector offset
    // 中断向量寄存器，在8259A中中断向量编号0-15，但是在CPU中给的是32-47，所以给每一个中断加上偏移量
    outb(IO_PIC1_DATA, IRQ_OFFSET);

    // ICW3:  (master PIC) bit mask of IR lines connected to slaves
    //        (slave PIC) 3-bit # of slave's connection to master
    // 级联命令字
    outb(IO_PIC1_DATA, 1 << IRQ_SLAVE);

    // ICW4:  000nbmap
    //    n:  1 = special fully nested mode
    //    b:  1 = buffered mode
    //    m:  0 = slave PIC, 1 = master PIC
    //        (ignored when b is 0, as the master/slave role
    //         can be hardwired).
    //    a:  1 = Automatic EOI mode
    //    p:  0 = MCS-80/85 mode, 1 = intel x86 mode
    // 自动EOI模式，即在中断响应时，在8259A发送出中断矢量之后，自动将相应ISR复位
    // 且采用一般嵌套模式，即当某个中断正在服务时，本级中断和低优先级的中断都被屏蔽，只有更高优先级的中断才会响应
    outb(IO_PIC1_DATA, 0x3);

    // 初始化从8259A（8259A-2）
    outb(IO_PIC2_CMD, 0x11);             // ICW1
    outb(IO_PIC2_DATA, IRQ_OFFSET + 8);  // ICW2
    outb(IO_PIC2_DATA, IRQ_SLAVE);       // ICW3
    // NB Automatic EOI mode doesn't tend to work on the slave.
    // Linux source code says it's "to be investigated".
    outb(IO_PIC2_DATA, 0x3);             // ICW4

    // OCW3:  0ef01prs
    //   ef:  0x = NOP, 10 = clear specific mask, 11 = set specific mask
    //    p:  0 = no polling, 1 = polling mode
    //   rs:  0x = NOP, 10 = read IRR, 11 = read ISR
    outb(IO_PIC1_CMD, 0x68);    // clear specific mask
    outb(IO_PIC1_CMD, 0x0a);    // read IRR by default

    outb(IO_PIC2_CMD, 0x68);    // OCW3
    outb(IO_PIC2_CMD, 0x0a);    // OCW3

    // 初始只打开irq2的端口（为了从8259A能正常工作）
    if (irq_mask != 0xFFFF) {
        pic_setmask(irq_mask);
    }
}
