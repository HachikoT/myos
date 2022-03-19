#ifndef __KERN_DRIVER_PICIRQ_H__
#define __KERN_DRIVER_PICIRQ_H__

// 初始化可编程中断控制器（Programmable Interrupt Controller）8259A
void pic_init(void);

// 使能特定irq端口，0-15
void pic_enable(unsigned int irq);

#endif /* !__KERN_DRIVER_PICIRQ_H__ */
