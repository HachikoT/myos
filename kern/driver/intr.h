#ifndef __KERN_DRIVER_INTR_H__
#define __KERN_DRIVER_INTR_H__

void intr_enable(void);  // 允许外部中断
void intr_disable(void); // 禁止外部中断

#endif // __KERN_DRIVER_INTR_H__
