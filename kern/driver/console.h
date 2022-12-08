#ifndef __KERN_DRIVER_CONSOLE_H__
#define __KERN_DRIVER_CONSOLE_H__

void cons_init(void);  // 初始化控制台console
void cons_putc(int c); // 向显示器写一个字符
int cons_getc(void);
void serial_intr(void);
void kbd_intr(void);

#endif // __KERN_DRIVER_CONSOLE_H__
