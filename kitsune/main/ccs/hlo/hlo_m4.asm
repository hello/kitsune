
	.cdecls "hlo_m4.h"

	.thumb

	.def hlo_asm_accumulate

	.text

;---------------------------------------------------------
hlo_asm_accumulate:
		push	{r4, r5, r6, r7, lr}
		mov	r6, r0
		movs	r0, #0
l8020:		cmp	r6, #15
		ble.n	l8070
		ldr	r4, [r1, #0]
		ldr	r3, [r1, #4]
		ldmia.w	r2, {r5, r7}
		smlad	r3, r3, r7, r0
		smlad	r0, r4, r5, r3
		ldr	r4, [r1, #8]
		ldr	r3, [r1, #12]
		ldr	r5, [r2, #8]
		ldr	r7, [r2, #12]
		smlad	r3, r3, r7, r0
		smlad	r0, r4, r5, r3
		ldr	r4, [r1, #16]
		ldr	r3, [r1, #20]
		ldr	r5, [r2, #16]
		ldr	r7, [r2, #20]
		smlad	r3, r3, r7, r0
		smlad	r0, r4, r5, r3
		adds	r1, #32
		adds	r2, #32
		ldr.w	r4, [r1, #-8]
		ldr.w	r3, [r1, #-4]
		ldmdb	r2, {r5, r7}
		smlad	r3, r3, r7, r0
		smlad	r0, r4, r5, r3
		subs	r6, #16
		b.n	l8020
l8070:		cmp	r6, #7
		ble.n	l80a0
		ldr	r4, [r1, #0]
		ldr	r3, [r1, #4]
		ldmia.w	r2, {r5, r7}
		smlad	r3, r3, r7, r0
		smlad	r0, r4, r5, r3
		adds	r1, #16
		adds	r2, #16
		ldr.w	r4, [r1, #-8]
		ldr.w	r3, [r1, #-4]
		ldmdb	r2, {r5, r7}
		smlad	r3, r3, r7, r0
		smlad	r0, r4, r5, r3
		subs	r6, #8
		b.n	l8070
l80a0:		cmp	r6, #3
		ble.n	l80c0
		ldr.w	r4, [r1], #8
		ldr.w	r5, [r2], #8
		ldr.w	r3, [r1, #-4]
		ldr.w	r7, [r2, #-4]
		smlad	r3, r3, r7, r0
		smlad	r0, r4, r5, r3
		subs	r6, #4
		b.n	l80a0
l80c0:		cmp	r6, #1
		ble.n	l80d4
		ldr.w	r3, [r1], #4
		ldr.w	r4, [r2], #4
		smlad	r0, r3, r4, r0
		subs	r6, #2
		b.n	l80c0
l80d4:		pop	{r4, r5, r6, r7, pc}
