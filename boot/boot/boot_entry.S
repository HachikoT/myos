#include "libs/descriptor.h"

.set PROT_MODE_CSEG,        0x8                     // 代码段选择子
.set PROT_MODE_DSEG,        0x10                    // 数据段选择子
.set CR0_PE_ON,             0x1                     // 保护模式使能位（CR0寄存器的第0位）
.set SMAP,                  0x534d4150              // 4个ASCII字符“SMAP”

// BIOS会加载第一个扇区（512Byte）的数据作为bootloader，将其加载到0x7c00处，并将CS:IP设置为0:0x7c00，跳转到该处执行
// 需要在链接的时候确保boot_entry的地址为0x7c00，使其作为bootloader的入口函数
.globl boot_entry
boot_entry:
// 生成16位汇编代码，实模式运行
.code16

    cli                                             // 禁止外部中断

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

    // 探测物理内存布局，使用e820方式，结果保存在地址为0x8000的e820map对象中
    //
    // struct e820map
    // {
    //     uint32_t n_map; // 探测到的内存块数量
    //     struct
    //     {
    //         uint64_t addr; // 内存基址
    //         uint64_t size; // 内存大小
    //         uint32_t type; // 内存类型
    //     } __attribute__((packed)) map[E820_MAX];
    // };
    //
probe_memory:
    movl $0, 0x8000         // 操作内存地址为0x8000的e820map对象，第一个字段（n_map）初始为0，表示当前已探测到的内存块数量
    xorl %ebx, %ebx         // ebx初始为0，表示当前要探测的内存块序号，每次探测之后会自动加1
    movw $0x8004, %di       // es:di指向的地址用来存放探测到的内存块属性
start_probe:
    movl $0xE820, %eax      // 设置eax，表示以e820方式进行探测
    movl $20, %ecx          // 内存块属性对象的大小，这里为20字节
    movl $SMAP, %edx        // 设置edx为534D4150h (即4个ASCII字符“SMAP”)，这是一个约定
    int $0x15               // 进行中断
    // 若eflags的CF位为0，则表示探测成功
    jnc cont
    // 若eflags的CF位为1，则表示探测失败
    movl $12345, 0x8000
    jmp finish_probe
cont:
    addw $20, %di           // 给di加上20字节，为下次探测做准备
    incl 0x8000             // 给e820map对象的第一个计数字段加1
    // 若ebx为0，表示探测完了
    cmpl $0, %ebx
    jnz start_probe
finish_probe:

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

    // 设置好基址和栈顶指针寄存器，范围0-boot_entry（0x7c00）。然后调用boot_main
    // 栈是从高地址向低地址增长的，%ebp记录栈底，用于根据偏移地址计算出栈对象的地址
    // 按理说%ebp是要大于%esp的，但是由于现在处于初始状态，设置为0也没什么问题等调用了boot_main之后就正常了
    movl $0x0, %ebp
    movl $boot_entry, %esp
    call boot_main

spin:
    // 如果boot_main退出后（不应该发生），进入死循环
    jmp spin

// 全局描述符表
.p2align 2                                          // 4字节对齐
gdt:
    SEG_NULL_ASM                                    // 第0项，null段描述符
    SEG_DESC_ASM(STA_X | STA_R, 0x0, 0xffffffff)    // 第1项（可执行/可读代码段，基址为0，范围4GB）
    SEG_DESC_ASM(STA_W, 0x0, 0xffffffff)            // 第2项（可读写数据段，基址为0，范围4GB）

gdt_desc:
    .word 0x17                                      // sizeof(gdt) - 1
    .long gdt                                       // gdt的地址
