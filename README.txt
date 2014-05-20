==============================================================================
README:  This is the readme file for the EK-LM4F232 FreeRTOS port.

         FreeRTOS is owned by Real Time Engineers Ltd. and licensed under
         a modified GPL Licenses (see www.freertos.org for details(.
         
         This Port for the EK-LM4F232 supplied by:
         	Ken Pettit
         	Fuel7, Inc.
         	www.fuel7.com
         	
==============================================================================

This port creates several tasks which demonstrate the operation of FreeRTOS
semaphores, signalling from an ISR, Cortex-M4F FPU register context switching,
and I/O with the UART and OLED.  It creates a CLI on the EK-LM4F232 ICDI UART
which allows executing commands. The commands supported are:

    color:    Controls the color mode on the OLED.
    cpu:      Shows CPU utilization.
    fputest:  Performs an FPU context switch test for the specified number
              of seconds.  Can be ran multiple times to create additinal tasks.
    free:     Reports free heap memory available.
    tasks:    Displays a list of currently running tasks.

The file Src/main.c contains the macro 'mainCREATE_FPU_CONTEXT_SAVE_TEST' to
enable / disable testing of lazy FPU register stacking via ISRs.  Setting
this value to 1 causes every SysTICK to issue a 3-deep nested ISR sequence
saving the FPU registers and reloading them with new values during each
interrupt.  This is for testing purposes only and a real application should
remove this logic as it causes abnormally long context switch times and
stack frame depth requirements.

NOTES:
		1.  This port includes StellarisWare library and utility files
		    directly vs. linking from a .lib file.  For better code usage,
		    a real application should link using the LIB version.
		    
		2.  The following Stellaris library / utility files have been modified
		    in this port from the originals provided by TI:
		    
		    cfal96x64x16.c:   - Modified to use 16-bit OLED color vs 8-bit
		    
		    uartstdio.c:      - Added command line history feature
		                      - Added support for FreeRTOS signalling global
		                        semaphore from the RX ISR
		                      - Added interrupt priority to support FreeRTOS
		                        signaling from the RX ISR
		    
		    ustdlib.c:        - Added left-justify format specifier ('-')
		                        to the uvsnprintf function (as a NOP).
		
		3.  The following FreeRTOS 7.1.1 files have been modified from the
		    original provided by Real Time Engineers Ltd:
		    
		    task.c    - Modified task name reporting in vTaskList to align
		                the names based on configMAX_TASK_NAME_LEN
		                
		4.  The port enables configUSE_TRACE_FACILITY to support the 'tasks'
		    command.  This is a debug function included for demonstration
		    purposes only and could be disabled for a production application
		    as it increases the generated code size considerably.

(end of file)
		           