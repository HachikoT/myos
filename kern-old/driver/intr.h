#ifndef __KERN_DRIVER_INTR_H__
#define __KERN_DRIVER_INTR_H__

// 使能中断
void intr_enable(void);

// 禁止中断
void intr_disable(void);

#endif /* !__KERN_DRIVER_INTR_H__ */
