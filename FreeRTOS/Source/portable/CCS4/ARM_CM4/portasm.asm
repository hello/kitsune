
	.cdecls "FreeRTOSConfig.h"

	.thumb

	.def vPortSetInterruptMask
	.def vPortClearInterruptMask
	.def vPortSVCHandler
	.def vPortStartFirstTask
	.def xPortPendSVHandler
	.def vPortEnableVFP

	.ref pxCurrentTCB          
    .ref vTaskSwitchContext
    .ref memset
    .ref __stack

	.text

NVIC_VTABLE_R: .word  	0xE000ED08
CPACR_R:       .word    0xE000ED88

;---------------------------------------------------------

vPortSetInterruptMask:
	
	push { r0 }
	mov r0, #configMAX_SYSCALL_INTERRUPT_PRIORITY                     
	msr basepri, r0     
	pop { r0 }
	bx r14
	
;---------------------------------------------------------

vPortClearInterruptMask:

	push { r0 }
	mov r0, #0
	msr basepri, r0
	pop { r0 }
	bx r14
	

;---------------------------------------------------------

xPortPendSVHandler:

   mrs r0, psp                      

   ldr r3, pxCurrentTCBConst        ; Get the location of the current TCB.
   ldr r2, [r3]                     

   tst r14, #0x10					; Is the task using the FPU context?  If so, push high vfp registers.
   it EQ
   vstmdbeq r0!, {s16-s31}
   	
   stmdb r0!, {r4-r11, r14}         ; Save the remaining registers. 
   str r0, [r2]                     ; Save the new top of stack into the first member of the TCB. 

   stmdb sp!, {r3, r14}             
   mov r0, #configMAX_SYSCALL_INTERRUPT_PRIORITY                       
   msr basepri, r0                  
   bl vTaskSwitchContext           
   mov r0, #0                       
   msr basepri, r0                  
   ldmia sp!, {r3, r14}             														    
                                    ; Restore the context, including the critical nesting count.
   ldr r1, [r3]                     
   ldr r0, [r1]                     ; The first item in pxCurrentTCB is the task top of stack. 
   ldmia r0!, {r4-r11, r14}         ; Pop the registers.
   
   tst r14, #0x10					; Is the task using the FPU context?  If so, pop the high vfp registers too.
   it EQ
   vldmiaeq r0!, {s16-s31}
    
   msr psp, r0 

   bx r14                              

   .align 2                            
pxCurrentTCBConst: .word pxCurrentTCB 

;---------------------------------------------------------

vPortSVCHandler:
   	                                
   	ldr r3, pxCurrentTCBConst2      ; Restore the context.  
   	ldr r1, [r3]                    ; Use pxCurrentTCBConst to get the pxCurrentTCB address.  
   	ldr r0, [r1]                    ; The first item in pxCurrentTCB is the task top of stack. 
   	ldmia r0!, {r4-r11, r14}        ; Pop the registers that are not automatically saved on exception entry and the critical nesting count.  
   	msr psp, r0                     ; Restore the task stack pointer. 
   	mov r0, #0                      
   	msr basepri, r0                 ; Anything can interrupt the application thread                 
   	orr r14, #0xd                   ; Ensure lower bit of register for BX is set                  
   	bx r14                          ; Return from exception (to the 1st task)           
   	                                
   	.align 2                        
pxCurrentTCBConst2: .word pxCurrentTCB


;---------------------------------------------------------

vPortStartFirstTask:
                                       
 	ldr r1, NVIC_VTABLE_R			; Use the NVIC offset register to locate the stack. 
 	ldr r1, [r1]         
 	ldr r1, [r1]         
 	msr msp, r1          			; Set the msp back to the start of the stack.
 	ldr r0, __stack2				; Perpare to fill kernel stack with 0xA5
 	sub r2, r1, r0					; Calculate size of stack
 	sub r2, #20						; Don't fill our memset return stack
 	mov r1, #0xa5
 	bl memset 						; Fill with 0xA5 so we can caluclate stack usage
 	svc #0               			; System call to start first task.
 	
 	.align
__stack2:   .word __stack 

;-----------------------------------------------------------

vPortEnableVFP:
	ldr r0, CPACR_R					; The FPU enable bits are in the CPACR.
	ldr	r1, [r0]
	
	; Enable CP10 and CP11 coprocessors, then save back.
	orr	r1, #0xf00000
	str r1, [r0]
	dsb
	isb
	bx	r14	
	
	.end
		