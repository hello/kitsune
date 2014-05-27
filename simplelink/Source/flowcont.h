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

#ifndef __FLOWCONT_H__
#define __FLOWCONT_H__

//*****************************************************************************
//
// If building with a C++ compiler, make all of the definitions in this header
// have a C binding.
//
//*****************************************************************************
#ifdef	__cplusplus
extern "C" {
#endif

#define WLAN_STATUS_CONN 1
#define WLAN_STATUS_DISCONN 0
#define EVENT_Q_STATUS_NOT_EMPTY 1
#define EVENT_Q_STATUS_EMPTY 0
#define PENDING_RECV_STATUS_PRESENT 1
#define PENDING_RECV_STATUS_ABSENT 0
#define FW_BUSY_PKTS_STATUS_PRESENT 1
#define FW_BUSY_PKTS_STATUS_ABSENT 0
#define PENDING_CMD_STATUS_PRESENT 1
#define PENDING_CMD_STATUS_ABSENT 0

#define FLOW_CONT_WLAN_STATUS_BIT          0
#define FLOW_CONT_EVENT_Q_STATUS_BIT       1
#define FLOW_CONT_PENDING_RECV_STATUS_BIT  2
#define FLOW_CONT_FW_BUSY_PKTS_STATUS_BIT  3
#define FLOW_CONT_PENDING_CMD_STATUS_BIT   4

#define FLOW_CONT_WLAN_STATUS_DEFAULT          (WLAN_STATUS_DISCONN         << FLOW_CONT_WLAN_STATUS_BIT        )
#define FLOW_CONT_EVENT_Q_STATUS_DEFAULT       (EVENT_Q_STATUS_NOT_EMPTY    << FLOW_CONT_EVENT_Q_STATUS_BIT     )
#define FLOW_CONT_PENDING_RECV_STATUS_DEFAULT  (PENDING_RECV_STATUS_ABSENT  << FLOW_CONT_PENDING_RECV_STATUS_BIT)
#define FLOW_CONT_FW_BUSY_PKTS_STATUS_DEFAULT  (FW_BUSY_PKTS_STATUS_ABSENT  << FLOW_CONT_FW_BUSY_PKTS_STATUS_BIT)
#define FLOW_CONT_PENDING_CMD_STATUS_DEFAULT   (PENDING_CMD_STATUS_ABSENT   << FLOW_CONT_PENDING_CMD_STATUS_BIT )

#define FLOW_CONT_STATUS_DEFAULT (FLOW_CONT_WLAN_STATUS_DEFAULT        |\
                                  FLOW_CONT_EVENT_Q_STATUS_DEFAULT     |\
                                  FLOW_CONT_PENDING_RECV_STATUS_DEFAULT|\
                                  FLOW_CONT_FW_BUSY_PKTS_STATUS_DEFAULT|\
                                  FLOW_CONT_PENDING_CMD_STATUS_DEFAULT )

//*****************************************************************************
//
// Prototypes for the APIs.
//
//*****************************************************************************

#define FLOW_CONT_MIN 1

extern void _SlDrvFlowContInit(void);

extern void _SlDrvFlowContDeinit(void);

//*****************************************************************************
//
// Mark the end of the C bindings section for C++ compilers.
//
//*****************************************************************************
#ifdef  __cplusplus
}
#endif // __cplusplus

#endif // __FLOWCONT_H__

