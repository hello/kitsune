;-----------------------------------------------------------*/
; FreeRTOS TI LM4F port FPU register contenxt save tests.
;-----------------------------------------------------------*/

	.cdecls "FreeRTOSConfig.h"

	.thumb

	.def vRegTestClearFlopRegistersToParameterValue
	.def ulRegTestCheckFlopRegistersContainParameterValue
	
	.text
	
;-----------------------------------------------------------

vRegTestClearFlopRegistersToParameterValue

	; Clobber the auto saved registers.
	vmov d0, r0, r0
	vmov d1, r0, r0
	vmov d2, r0, r0
	vmov d3, r0, r0
	vmov d4, r0, r0
	vmov d5, r0, r0
	vmov d6, r0, r0
	vmov d7, r0, r0
	bx lr

;-----------------------------------------------------------

ulRegTestCheckFlopRegistersContainParameterValue

	vmov r1, s0
	cmp r0, r1
	bne return_error
	vmov r1, s1
	cmp r0, r1
	bne return_error
	vmov r1, s2
	cmp r0, r1
	bne return_error
	vmov r1, s3
	cmp r0, r1
	bne return_error
	vmov r1, s4
	cmp r0, r1
	bne return_error
	vmov r1, s5
	cmp r0, r1
	bne return_error
	vmov r1, s6
	cmp r0, r1
	bne return_error
	vmov r1, s7
	cmp r0, r1
	bne return_error
	vmov r1, s8
	cmp r0, r1
	bne return_error
	vmov r1, s9
	cmp r0, r1
	bne return_error
	vmov r1, s10
	cmp r0, r1
	bne return_error
	vmov r1, s11
	cmp r0, r1
	bne return_error
	vmov r1, s12
	cmp r0, r1
	bne return_error
	vmov r1, s13
	cmp r0, r1
	bne return_error
	vmov r1, s14
	cmp r0, r1
	bne return_error
	vmov r1, s15
	cmp r0, r1
	bne return_error
	
return_pass
	mov r0, #1
	bx lr

return_error
	mov r0, #0
	bx lr

	.end
	
