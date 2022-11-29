OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)

SECTIONS {
    . = 0x7C00;

    .startup : {
        *boot_asm.o(.text)
    }

    .text : { *(.text) }
    .data : { *(.data .rodata) }

    /DISCARD/ : { *(.eh_*) }
}
