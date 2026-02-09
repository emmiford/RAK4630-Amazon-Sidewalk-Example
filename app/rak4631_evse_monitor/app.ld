/*
 * App Image Linker Script
 *
 * Places the app callback table at 0x90000 (start of app partition)
 * followed by code and data sections.
 *
 * RAM: uses last 8KB of nRF52840 SRAM (0x2003E000 - 0x20040000)
 * Flash: 0x90000 - 0xF4FFF (404KB partition, app uses ~20-30KB)
 */

MEMORY
{
    FLASH (rx)  : ORIGIN = 0x90000, LENGTH = 0x65000   /* 404KB app partition */
    RAM   (rwx) : ORIGIN = 0x2003E000, LENGTH = 0x2000 /* 8KB reserved for app */
}

ENTRY(app_cb)

SECTIONS
{
    /* App callback table — must be at exactly 0x80000 */
    .app_header : ALIGN(4)
    {
        KEEP(*(.app_header))
    } > FLASH

    /* Code */
    .text : ALIGN(4)
    {
        *(.text*)
        *(.gnu.linkonce.t.*)
    } > FLASH

    /* Read-only data */
    .rodata : ALIGN(4)
    {
        *(.rodata*)
        *(.gnu.linkonce.r.*)
    } > FLASH

    /* ARM exception/unwind tables */
    .ARM.extab : ALIGN(4)
    {
        *(.ARM.extab*)
    } > FLASH

    .ARM.exidx : ALIGN(4)
    {
        *(.ARM.exidx*)
    } > FLASH

    /* Data initialization values (stored in flash, copied to RAM at startup) */
    _data_flash = .;

    /* Initialized data (copied from flash to RAM) */
    .data : ALIGN(4)
    {
        _data_start = .;
        *(.data*)
        *(.gnu.linkonce.d.*)
        _data_end = .;
    } > RAM AT> FLASH

    /* Zero-initialized data */
    .bss (NOLOAD) : ALIGN(4)
    {
        _bss_start = .;
        *(.bss*)
        *(.gnu.linkonce.b.*)
        *(COMMON)
        _bss_end = .;
    } > RAM

    /* App stack (remaining RAM) */
    .stack (NOLOAD) : ALIGN(8)
    {
        _stack_start = .;
        . = ORIGIN(RAM) + LENGTH(RAM);
        _stack_end = .;
    } > RAM

    /* Discard unneeded sections */
    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.debug*)
        *(.eh_frame*)
    }
}
