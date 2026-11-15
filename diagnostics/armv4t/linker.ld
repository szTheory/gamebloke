OUTPUT_ARCH(arm)
ENTRY(_start)

SECTIONS
{
    . = 0x08000000;
    .gba_header :
    {
        KEEP(*(.gba_header))
    }

    .text :
    {
        *(.text.startup)
        *(.text.smoke)
        *(.text*)
    }

    .rodata : { *(.rodata*) }
    .data : { *(.data*) }
    .bss (NOLOAD) : { *(.bss*) *(COMMON) }
    /DISCARD/ : { *(.comment) *(.note*) }
}
