#include "boot/boot/boot_asm.h"

.set PROT_MODE_CSEG,        0x8                     // 代码段选择子
.set PROT_MODE_DSEG,        0x10                    // 数据段选择子
.set CR0_PE_ON,             0x1                     // 保护模式使能位（CR0寄存器的第0位）

// BIOS会加载第一个扇区（512Byte）的数据作为bootloader，将其加载到0x7c00处，CS:IP设置为0:0x7c00，跳转到该处执行
// 需要在链接的时候确保start的地址为0x7c00，使其作为bootloader的入口函数
.globl start
start:
// 生成16位汇编代码，实模式运行
.code16

    cli                                             // 禁止中断

    // 设置好重要的段寄存器
    xorw %ax, %ax                                   // 0
    movw %ax, %ds                                   // Data Segment
    movw %ax, %es                                   // Extra Segment
    movw %ax, %ss                                   // Stack Segment

    // 开启第20位地址总线
    // 在8086中只有20位地址总线，编号0-19，到了后面80286，80386时代，地址总线超过20位了就遇到了兼容问题
    // 因为实模式中段基址+地址偏移可能超过20位，总共需要21位地址总线，这时为了兼容，就默认关闭第20位地址总线
    // 这也会影响保护模式中的寻址范围（第20位地址总线总为0），所以需要手动开启A20地址总线
enable_a20.1:
    // Wait for not busy(8042 input buffer empty).
    inb $0x64, %al
    testb $0x2, %al
    jnz enable_a20.1

    movb $0xd1, %al                                 // 0xd1 -> port 0x64
    outb %al, $0x64                                 // 0xd1 means: write data to 8042's P2 port

enable_a20.2:
    // Wait for not busy(8042 input buffer empty).
    inb $0x64, %al
    testb $0x2, %al
    jnz enable_a20.2

    movb $0xdf, %al                                 // 0xdf -> port 0x60
    outb %al, $0x60                                 // 0xdf = 11011111, means set P2's A20 bit(the 1 bit) to 1

    // 将全局描述符表的地址存储到GDTR寄存器
    // 使能保护模式
    lgdt gdt_desc
    movl %cr0, %eax
    orl $CR0_PE_ON, %eax
    movl %eax, %cr0

    // 进行far jump，同时加载CS和EIP，跳转到prot_cseg执行，执行完这句才进入32位保护模式
    ljmp $PROT_MODE_CSEG, $prot_cseg

prot_cseg:
// 生成32位汇编代码，保护模式运行
.code32

    // 重新设置好重要的段寄存器
    movw $PROT_MODE_DSEG, %ax                       // 数据段选择子
    movw %ax, %ds                                   // Data Segment
    movw %ax, %es                                   // Extra Segment
    movw %ax, %fs                                   // FS
    movw %ax, %gs                                   // GS
    movw %ax, %ss                                   // Stack Segment

    // 设置好基址和栈顶指针寄存器，范围0-start（0x7c00）。然后调用bootmain
    // 栈是从高地址向低地址增长的，%ebp记录栈底，用于根据偏移地址计算出栈对象的地址
    // 按理说%ebp是要大于%esp的，但是由于现在处于初始状态，设置为0也没什么问题等调用了bootmain之后就正常了
    movl $0x0, %ebp
    movl $start, %esp
    call boot_main

spin:
    // 如果bootmain退出后（不应该发生），进入死循环
    jmp spin

// 全局描述符表
.p2align 2                                          // 强制4字节对齐
gdt:
    SEG_NULL_ASM                                    // 第0项，null段描述符
    SEG_DESC_ASM(STA_X | STA_R, 0x0, 0xffffffff)    // 第1项（可执行/可读代码段，基址为0，范围4GB）
    SEG_DESC_ASM(STA_W, 0x0, 0xffffffff)            // 第2项（可读写数据段，基址为0，范围4GB）

gdt_desc:
    .word 0x17                                      // sizeof(gdt) - 1
    .long gdt                                       // gdt的地址
