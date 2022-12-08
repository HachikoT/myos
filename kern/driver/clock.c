#include "libs/defs.h"
#include "libs/x86.h"
#include "kern/driver/stdio.h"
#include "kern/driver/picirq.h"

// 8253可编程定时器，对应IRQ0中断
#define IO_TIMER1 0x040

// TIMER_FREQ/freq的值等于产生频率为freq的定时器的初始值
#define TIMER_FREQ 1193182
#define TIMER_DIV(x) ((TIMER_FREQ + (x) / 2) / (x))

// 8253定时器控制字结构
//
// +-2--+-2--+--3---+--1--+
// | SC | RL | Mode | BCD |
// +----+----+------+-----+
//
// SC：使用内部的哪个计数器（有0，1，2三个内部计数器）
// RL：读写模式，00计数器锁存；01只读写高8位；10只读写低8位；11先读写高8位，再读写低8位。
// Mode：操作模式，一般采用模式2，计数到0之后自动装载初始值重新开始计数
// BCD：使用二进制，还是BCD码
//
#define TIMER_MODE (IO_TIMER1 + 3) // timer mode port
#define TIMER_SEL0 0x00            // select counter 0
#define TIMER_RATEGEN 0x04         // mode 2, rate generator
#define TIMER_16BIT 0x30           // r/w counter 16 bits, LSB first

volatile size_t g_ticks;

// 初始化定时器，频率为100hz
void clock_init(void)
{
    // set 8253 timer-chip
    outb(TIMER_MODE, TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
    outb(IO_TIMER1, TIMER_DIV(100) % 256);
    outb(IO_TIMER1, TIMER_DIV(100) / 256);

    // initialize time counter 'ticks' to zero
    g_ticks = 0;

    // 允许定时器中断
    pic_enable(IRQ_TIMER);

    cprintf("++ setup timer interrupts\n");
}

// 获取当前的系统时间
long system_read_timer(void)
{
    return g_ticks;
}
