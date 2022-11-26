#ifndef __BOOT_BOOT_ASM_H__
#define __BOOT_BOOT_ASM_H__

/* Assembler macros to create x86 segments */

/* Normal segment */
#define SEG_NULL_ASM \
    .word 0, 0;      \
    .byte 0, 0, 0, 0

#define SEG_DESC_ASM(type, base, lim)               \
    .word(((lim) >> 12) & 0xffff), ((base)&0xffff); \
    .byte(((base) >> 16) & 0xff), (0x90 | (type)),  \
        (0xC0 | (((lim) >> 28) & 0xf)), (((base) >> 24) & 0xff)

/* Application segment type bits */
#define STA_X 0x8 // Executable segment
#define STA_E 0x4 // Expand down (non-executable only)
#define STA_C 0x4 // Conforming code segment (executable only)
#define STA_W 0x2 // Writeable (non-executable only)
#define STA_R 0x2 // Readable (executable only)
#define STA_A 0x1 // Accessed

#endif /* !__BOOT_BOOT_ASM_H__ */
