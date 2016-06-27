/*****************************************************************************
* cc3200.ld
*
* GCC Linker script for get_time application (ti-rtos OS based)
*
* Copyright (C) 2014 Texas Instruments Incorporated - http://www.ti.com/
*
*
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions
*  are met:
*
*    Redistributions of source code must retain the above copyright
*    notice, this list of conditions and the following disclaimer.
*
*    Redistributions in binary form must reproduce the above copyright
*    notice, this list of conditions and the following disclaimer in the
*    documentation and/or other materials provided with the
*    distribution.
*
*    Neither the name of Texas Instruments Incorporated nor the names of
*    its contributors may be used to endorse or promote products derived
*    from this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
*  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
*  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
*  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
*  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
*  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
*  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
*  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
*  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
*  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
*  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
******************************************************************************/

MEMORY
{
    FLASH_HDR  (r)     : ORIGIN = 0x01000000, LENGTH = 0x00000800
    FLASH_CODE (r)     : ORIGIN = 0x01000800, LENGTH = 0x000ff800
    SRAM_DATA  (rwx)   : ORIGIN = 0x20000000, LENGTH = 0x00040000
}

REGION_ALIAS("REGION_TEXT", FLASH_CODE);
REGION_ALIAS("REGION_ARM_EXIDX", FLASH_CODE);
REGION_ALIAS("REGION_ARM_EXTAB", FLASH_CODE);
REGION_ALIAS("REGION_BSS", SRAM_DATA);
REGION_ALIAS("REGION_DATA", SRAM_DATA);
REGION_ALIAS("REGION_HEAP", SRAM_DATA);
REGION_ALIAS("REGION_STACK", SRAM_DATA);

SECTIONS {

    .intvecs : {
        KEEP (*(.intvecs))
    } > REGION_TEXT

	.dgbhdr : {

		KEEP(*(.dgbhdr));
	}

    .text : {
        CREATE_OBJECT_SYMBOLS
        KEEP (*(.text))
        *(.text.*)
        . = ALIGN(0x4);
        KEEP (*(.ctors))
        . = ALIGN(0x4);
        KEEP (*(.dtors))
        . = ALIGN(0x4);
        __init_array_start = .;
        KEEP (*(.init_array*))
        __init_array_end = .;
        *(.init)
        *(.fini*)
    } > REGION_TEXT

    PROVIDE (__etext = .);
    PROVIDE (_etext = .);
    PROVIDE (etext = .);

	.vtable(NOLOAD) : {
        KEEP (*(.vtable))
    } > REGION_DATA

	.data : ALIGN(4) {
        __data_load__ = LOADADDR (.data);
		. = ALIGN (4);
        __data_start__ = .;
        KEEP (*(.data))
        KEEP (*(.data*))
        . = ALIGN (4);
        __data_end__ = .;
    } > REGION_DATA AT> REGION_TEXT

	.ARM.exidx : {
        __exidx_start = .;
        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
        __exidx_end = .;
    } > REGION_ARM_EXIDX

    .ARM.extab : {
        *(.ARM.extab* .gnu.linkonce.armextab.*)
    } > REGION_ARM_EXTAB

    .rodata : {
        *(.rodata)
        *(.rodata*)
    } > REGION_TEXT

    .bss (NOLOAD): {
        __bss_start__ = .;
        *(.shbss)
        KEEP (*(.bss))
        *(.bss.*)
        *(COMMON)
        . = ALIGN (4);
        __bss_end__ = .;
    } > REGION_BSS

    .heap (NOLOAD) : ALIGN(0x8) {
        __heap_start__ = .;
        end = __heap_start__;
        _end = end;
        __end = end;
        . = . + HEAP_SIZE;
        KEEP(*(.heap))
        __heap_end__ = .;
        __HeapLimit = __heap_end__;
    } > REGION_HEAP

    .stack (NOLOAD): ALIGN(0x8) {
        _stack = .;
        __stack = .;
        KEEP(*(.stack))
    } > REGION_STACK
}
