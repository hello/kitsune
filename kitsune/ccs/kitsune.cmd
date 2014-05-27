/******************************************************************************
*
*   Copyright (C) 2013 Texas Instruments Incorporated
*
*   All rights reserved. Property of Texas Instruments Incorporated.
*   Restricted rights to use, duplicate or disclose this code are
*   granted through contract.
*
*   The program may not be used without the written permission of
*   Texas Instruments Incorporated or against the terms and conditions
*   stipulated in the agreement under which this program has been supplied,
*   and under no circumstances can it be used with non-TI connectivity device.
*
******************************************************************************/
/******************************************************************************
 *
 * freertos_demo.cmd - CCS linker configuration file.
 *
 *****************************************************************************/

--retain=g_pfnVectors

/* The following command line options are set as part of the CCS project.    */
/* If you are building using the command line, or for some reason want to    */
/* define them here, you can uncomment and modify these lines as needed.     */
/* If you are using CCS for building, it is probably better to make any such */
/* modifications in your CCS project and leave this file alone.              */
/*                                                                           */
 __HEAP_SIZE=0x000200;
 __STACK_SIZE=0x100;
/* --library=rtsv7M3_T_le_eabi.lib*/

/* The starting address of the application.  Normally the interrupt vectors  */
/* must be located at the beginning of the application.                      */
#define APP_BASE 0x01000000
#define RAM_BASE 0x20000000

/* System memory map */

MEMORY
{
    /* Application stored in and executes from internal flash */
    FLASH (RX) : origin = APP_BASE, length = 0x00010000
    /* Application uses internal RAM for data */
    SRAM (RWX) : origin = 0x20000000, length = 0x00030000
}

/* Section allocation in memory */

SECTIONS
{
    .intvecs:   > RAM_BASE
    .text   :   > SRAM
    .const  :   > SRAM
    .cinit  :   > SRAM
    .pinit  :   > SRAM
    .init_array : > SRAM

    .vtable :   > SRAM
    .data   :   > SRAM
    .bss    :   > SRAM
    .sysmem :   > SRAM
    .stack  :   > SRAM
}

__STACK_TOP = __stack + __STACK_SIZE;
