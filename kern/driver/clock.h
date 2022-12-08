#ifndef __KERN_DRIVER_CLOCK_H__
#define __KERN_DRIVER_CLOCK_H__

#include "libs/defs.h"

extern volatile size_t g_ticks;

// 初始化定时器
void clock_init(void);

// 获取当前的系统时间
long system_read_timer(void);

#endif // __KERN_DRIVER_CLOCK_H__
