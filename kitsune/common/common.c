/*
 *   Copyright (C) 2015 Texas Instruments Incorporated
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
 */

//*****************************************************************************
// common.c
//
// Common interface functions for CC3220 device
//
//*****************************************************************************

#include <string.h>
#include <stdlib.h>

//Driverlib includes
#include "hw_types.h"
#include "hw_memmap.h"
#include "hw_hib3p3.h"

//Common interface includes
#include "uart_if.h"
#include "common.h"


//*****************************************************************************
//
//! NotifyReturnToFactoryImage
//!
//!    @brief  Notify if device return to factory image
//!
//!     @param  None
//!
//!     @return None
//!
//
//*****************************************************************************
void 
NotifyReturnToFactoryImage()
{

	if(((HWREG(HIB3P3_BASE + HIB3P3_O_MEM_HIB_REG0) & (1<<7)) !=0 ) &&
		((HWREG(0x4402F0C8) &0x01) != 0) )
	{
		UART_PRINT("Return To Factory Image successful, Do a power cycle(POR) of the device using switch SW1-Reset\n\r");
		while(1);
	}
}


//*****************************************************************************
//
// Close the Doxygen group.
//! @}
//
//*****************************************************************************


