#ifndef __LIBS_X86_H__
#define __LIBS_X86_H__

#include "libs/defs.h"

// 从指定IO端口读取一个字节数据
static inline uint8_t inb(uint16_t port) {
    uint8_t data;
    // 内联汇编格式 模板 : 输出参数 : 输入参数 : 被修改的寄存器
    // 模板里面的编号是从第一个输出参数开始算的，也就是0
    // "a"表示用eax寄存器（inb指令要求），"="表示赋值
    // "d"表示用edx寄存器（inb指令要求）
    asm volatile ("inb %1, %0" : "=a" (data) : "d" (port));
    return data;
}

// 写一个字节数据到指定IO端口
static inline void outb(uint16_t port, uint8_t data) {
    asm volatile ("outb %0, %1" :: "a" (data), "d" (port));
}

// 写两个字节数据到指定IO端口
static inline void outw(uint16_t port, uint16_t data) {
    asm volatile ("outw %0, %1" :: "a" (data), "d" (port));
}

// 从指定端口读取cnt次4字节的数据到指定的内存地址
static inline void insl(uint32_t port, void *addr, int cnt) {
    asm volatile (
        "cld;"
        "repne; insl;"
        : "=D" (addr), "=c" (cnt)
        : "d" (port), "0" (addr), "1" (cnt)
        : "memory", "cc"
    );
}

#endif /* !__LIBS_X86_H__ */
