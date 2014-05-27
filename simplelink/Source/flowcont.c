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

#include "datatypes.h"
#include "simplelink.h"
#include "protocol.h"
#include "driver.h"
#include "flowcont.h"



//*****************************************************************************
// _SlDrvFlowContInit
//*****************************************************************************
void _SlDrvFlowContInit(void)
{
    g_pCB->FlowContCB.TxPoolCnt = FLOW_CONT_MIN;

    OSI_RET_OK_CHECK(sl_LockObjCreate(&g_pCB->FlowContCB.TxLockObj, "TxLockObj"));

    OSI_RET_OK_CHECK(sl_SyncObjCreate(&g_pCB->FlowContCB.TxSyncObj, "TxSyncObj"));

}

//*****************************************************************************
// _SlDrvFlowContDeinit
//*****************************************************************************
void _SlDrvFlowContDeinit(void)
{
    g_pCB->FlowContCB.TxPoolCnt = 0;

    OSI_RET_OK_CHECK(sl_LockObjDelete(&g_pCB->FlowContCB.TxLockObj));

    OSI_RET_OK_CHECK(sl_SyncObjDelete(&g_pCB->FlowContCB.TxSyncObj));


}

