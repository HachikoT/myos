#ifndef __KERN_DRIVER_PICIRQ_H__
#define __KERN_DRIVER_PICIRQ_H__

// IRQ端口号和其对应的向量号的差值
#define IRQ_OFFSET 32

// IRQ端口宏定义
#define IRQ_TIMER 0
#define IRQ_KBD 1
#define IRQ_COM1 4
#define IRQ_IDE1 14
#define IRQ_IDE2 15

void pic_init(void);           // 初始化可编程中断控制器（PIC Programmable Interrupt Controller）8259A
void pic_enable(unsigned irq); // 使能特定的中断请求端口（IRQ Interrupt Request），允许IRQ 0~15

#endif // __KERN_DRIVER_PICIRQ_H__
