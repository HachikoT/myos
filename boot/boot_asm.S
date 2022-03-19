#include "boot/asm.h"

.set PROT_MODE_CSEG,        0x8                     # 内核代码段选择子
.set PROT_MODE_DSEG,        0x10                    # 内核数据段选择子
.set CR0_PE_ON,             0x1                     # 保护模式使能位（CR0寄存器的第0位）

# BIOS会加载第一个扇区（512Byte）的数据作为bootloader，加载到0x7c00，并切换代码段和地址偏移为0:7c00
# 在链接的时候会指定start的地址为0x7c00，作为bootloader的入口
.globl start
.code16                                             # 16位汇编模式
start:
    cli                                             # 禁止中断

    # 开启第20位地址总线
    # 在8086中只有20位地址总线，编号0-19，访问超过1MB的地址等于对1MB地址进行取模，因为超过20位的地址信息被丢弃了（相当于等于0）
    # 到了后面80286，80386时代，地址总线超过20位了，实模式中段基址+地址偏移可能超过20位，总共需要21位地址总线
    # 为了兼容，会默认关闭第20位地址总线，其值固定为0，这样也会影响保护模式中寻址范围，所以需要开启A20地址总线
enable_a20.1:
    # Wait for not busy(8042 input buffer empty).
    inb $0x64, %al
    testb $0x2, %al
    jnz enable_a20.1

    movb $0xd1, %al                                 # 0xd1 -> port 0x64
    outb %al, $0x64                                 # 0xd1 means: write data to 8042's P2 port

enable_a20.2:
    # Wait for not busy(8042 input buffer empty).
    inb $0x64, %al
    testb $0x2, %al
    jnz enable_a20.2

    movb $0xdf, %al                                 # 0xdf -> port 0x60
    outb %al, $0x60                                 # 0xdf = 11011111, means set P2's A20 bit(the 1 bit) to 1

    # 将全局描述表的地址存储到GDTR寄存器
    # 使能保护模式
    lgdt gdtdesc
    movl %cr0, %eax
    orl $CR0_PE_ON, %eax
    movl %eax, %cr0

    # 切换段地址，传入内核代码段选择子，段偏移地址，然后才真正进入32位保护模式
    ljmp $PROT_MODE_CSEG, $protcseg

.code32                                             # 32位汇编模式
protcseg:
    # Set up the protected-mode data segment registers
    movw $PROT_MODE_DSEG, %ax                       # Our data segment selector
    movw %ax, %ds                                   # -> DS: Data Segment
    movw %ax, %es                                   # -> ES: Extra Segment
    movw %ax, %fs                                   # -> FS
    movw %ax, %gs                                   # -> GS
    movw %ax, %ss                                   # -> SS: Stack Segment

    # 设置好栈底和栈顶指针寄存器，范围0-start（0x7c00）。然后调用bootmain
    # 栈是从高地质向低地址增长的，ebp记录栈底，用于根据偏移地址计算出栈对象的地址
    # 按理说ebp是要大于esp的，但是由于现在处于初始状态，设置为0也没什么问题等调用了bootmain之后就正常运行了
    movl $0x0, %ebp
    movl $start, %esp
    call boot_main

spin:
    # 如果bootmain退出后（不应该发生），进入死循环
    jmp spin

# 全局描述符表
.p2align 2                                          # 强制4字节对齐
gdt:
    SEG_NULLASM                                     # 第0项，null段描述符（规定项）
    SEG_ASM(STA_X | STA_R, 0x0, 0xffffffff)         # 第1项，段描述符（可执行/可读，基址为0，范围4GB）
    SEG_ASM(STA_W, 0x0, 0xffffffff)                 # 第2项，段描述符（可写，基址为0，范围4GB）

gdtdesc:
    .word 0x17                                      # gdt的大小减1
    .long gdt                                       # gdt的地址
