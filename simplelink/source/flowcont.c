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


/*****************************************************************************/
/* Include files                                                             */
/*****************************************************************************/
#include "simplelink.h"
#include "protocol.h"
#include "driver.h"
#include "flowcont.h"


/*****************************************************************************/
/* _SlDrvFlowContInit */
/*****************************************************************************/
void _SlFlowContSet(void *pVoidBuf)
{
    SlDeviceFlowCtrlAsyncEvent_t *pFlowCtrlAsyncEvent   = (SlDeviceFlowCtrlAsyncEvent_t *)_SL_RESP_ARGS_START(pVoidBuf);

    if (pFlowCtrlAsyncEvent->MinTxPayloadSize != 0)
    {
       g_pCB->FlowContCB.MinTxPayloadSize = pFlowCtrlAsyncEvent->MinTxPayloadSize; 
    }
    
}


