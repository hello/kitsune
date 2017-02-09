#ifndef _CODEC_RUNTIME_UPDATE_H_
#define _CODEC_RUNTIME_UPDATE_H_

#include "stdint.h"

/* MUX PROPERTIES */
#define	MUX_SELECT_MIC      1
#define	MUX_SELECT_LOOPBACK 2

/********************************************************************************
 *                        IMPORTANT: PLEASE READ
 *
 * Note: If the PurePath Process flow changes, the following enum needs to be
 * updated for all components that can be updated runtime
 ********************************************************************************
 */
typedef enum{
	MUX_LOOPBACK_SELECTOR,
	MAX_CONTROL_BLOCKS

}control_blocks_t;

/*
 * NOTE: Codec properties can be updated runtime only if MCLK, WCLK and BCLK
 * are enabled.
 */
int32_t codec_update_minidsp_mux(control_blocks_t type, uint32_t data);

int32_t codec_test_runtime_prop_update(void);
#endif

