/*
 * Platform API table linker section
 *
 * Places the struct platform_api at fixed address 0x7FF00
 * (last 256 bytes of the 512KB platform partition).
 *
 * Included by the platform build via zephyr_linker_sources(SECTIONS ...)
 */

SECTION_PROLOGUE(.platform_api, 0x7FF00, SUBALIGN(4))
{
    KEEP(*(.platform_api))
} GROUP_LINK_IN(ROMABLE_REGION)
