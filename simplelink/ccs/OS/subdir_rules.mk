################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Each subdirectory must supply rules for building sources it contributes
cc_pal.obj: C:/Users/chrisjohnson/Desktop/kitsune/simplelink/cc_pal.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"c:/ti/ccsv6/tools/compiler/arm_5.1.5/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib --abi=eabi -me -O1 --opt_for_speed=0 --fp_mode=relaxed --include_path="c:/ti/ccsv6/tools/compiler/arm_5.1.5/include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Source" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/driverlib" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/inc" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/oslib/" -g --gcc --define=ccs --define=SL_FULL --define=SL_PLATFORM_MULTI_THREADED --display_error_number --diag_warning=225 --diag_wrap=off --gen_func_subsections=on --printf_support=full --ual --preproc_with_compile --preproc_dependency="cc_pal.pp" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

device.obj: C:/Users/chrisjohnson/Desktop/kitsune/simplelink/source/device.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"c:/ti/ccsv6/tools/compiler/arm_5.1.5/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib --abi=eabi -me -O1 --opt_for_speed=0 --fp_mode=relaxed --include_path="c:/ti/ccsv6/tools/compiler/arm_5.1.5/include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Source" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/driverlib" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/inc" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/oslib/" -g --gcc --define=ccs --define=SL_FULL --define=SL_PLATFORM_MULTI_THREADED --display_error_number --diag_warning=225 --diag_wrap=off --gen_func_subsections=on --printf_support=full --ual --preproc_with_compile --preproc_dependency="device.pp" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

driver.obj: C:/Users/chrisjohnson/Desktop/kitsune/simplelink/source/driver.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"c:/ti/ccsv6/tools/compiler/arm_5.1.5/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib --abi=eabi -me -O1 --opt_for_speed=0 --fp_mode=relaxed --include_path="c:/ti/ccsv6/tools/compiler/arm_5.1.5/include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Source" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/driverlib" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/inc" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/oslib/" -g --gcc --define=ccs --define=SL_FULL --define=SL_PLATFORM_MULTI_THREADED --display_error_number --diag_warning=225 --diag_wrap=off --gen_func_subsections=on --printf_support=full --ual --preproc_with_compile --preproc_dependency="driver.pp" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

flowcont.obj: C:/Users/chrisjohnson/Desktop/kitsune/simplelink/source/flowcont.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"c:/ti/ccsv6/tools/compiler/arm_5.1.5/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib --abi=eabi -me -O1 --opt_for_speed=0 --fp_mode=relaxed --include_path="c:/ti/ccsv6/tools/compiler/arm_5.1.5/include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Source" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/driverlib" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/inc" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/oslib/" -g --gcc --define=ccs --define=SL_FULL --define=SL_PLATFORM_MULTI_THREADED --display_error_number --diag_warning=225 --diag_wrap=off --gen_func_subsections=on --printf_support=full --ual --preproc_with_compile --preproc_dependency="flowcont.pp" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

fs.obj: C:/Users/chrisjohnson/Desktop/kitsune/simplelink/source/fs.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"c:/ti/ccsv6/tools/compiler/arm_5.1.5/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib --abi=eabi -me -O1 --opt_for_speed=0 --fp_mode=relaxed --include_path="c:/ti/ccsv6/tools/compiler/arm_5.1.5/include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Source" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/driverlib" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/inc" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/oslib/" -g --gcc --define=ccs --define=SL_FULL --define=SL_PLATFORM_MULTI_THREADED --display_error_number --diag_warning=225 --diag_wrap=off --gen_func_subsections=on --printf_support=full --ual --preproc_with_compile --preproc_dependency="fs.pp" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

netapp.obj: C:/Users/chrisjohnson/Desktop/kitsune/simplelink/source/netapp.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"c:/ti/ccsv6/tools/compiler/arm_5.1.5/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib --abi=eabi -me -O1 --opt_for_speed=0 --fp_mode=relaxed --include_path="c:/ti/ccsv6/tools/compiler/arm_5.1.5/include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Source" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/driverlib" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/inc" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/oslib/" -g --gcc --define=ccs --define=SL_FULL --define=SL_PLATFORM_MULTI_THREADED --display_error_number --diag_warning=225 --diag_wrap=off --gen_func_subsections=on --printf_support=full --ual --preproc_with_compile --preproc_dependency="netapp.pp" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

netcfg.obj: C:/Users/chrisjohnson/Desktop/kitsune/simplelink/source/netcfg.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"c:/ti/ccsv6/tools/compiler/arm_5.1.5/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib --abi=eabi -me -O1 --opt_for_speed=0 --fp_mode=relaxed --include_path="c:/ti/ccsv6/tools/compiler/arm_5.1.5/include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Source" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/driverlib" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/inc" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/oslib/" -g --gcc --define=ccs --define=SL_FULL --define=SL_PLATFORM_MULTI_THREADED --display_error_number --diag_warning=225 --diag_wrap=off --gen_func_subsections=on --printf_support=full --ual --preproc_with_compile --preproc_dependency="netcfg.pp" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

nonos.obj: C:/Users/chrisjohnson/Desktop/kitsune/simplelink/source/nonos.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"c:/ti/ccsv6/tools/compiler/arm_5.1.5/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib --abi=eabi -me -O1 --opt_for_speed=0 --fp_mode=relaxed --include_path="c:/ti/ccsv6/tools/compiler/arm_5.1.5/include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Source" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/driverlib" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/inc" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/oslib/" -g --gcc --define=ccs --define=SL_FULL --define=SL_PLATFORM_MULTI_THREADED --display_error_number --diag_warning=225 --diag_wrap=off --gen_func_subsections=on --printf_support=full --ual --preproc_with_compile --preproc_dependency="nonos.pp" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

socket.obj: C:/Users/chrisjohnson/Desktop/kitsune/simplelink/source/socket.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"c:/ti/ccsv6/tools/compiler/arm_5.1.5/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib --abi=eabi -me -O1 --opt_for_speed=0 --fp_mode=relaxed --include_path="c:/ti/ccsv6/tools/compiler/arm_5.1.5/include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Source" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/driverlib" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/inc" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/oslib/" -g --gcc --define=ccs --define=SL_FULL --define=SL_PLATFORM_MULTI_THREADED --display_error_number --diag_warning=225 --diag_wrap=off --gen_func_subsections=on --printf_support=full --ual --preproc_with_compile --preproc_dependency="socket.pp" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

spawn.obj: C:/Users/chrisjohnson/Desktop/kitsune/simplelink/source/spawn.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"c:/ti/ccsv6/tools/compiler/arm_5.1.5/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib --abi=eabi -me -O1 --opt_for_speed=0 --fp_mode=relaxed --include_path="c:/ti/ccsv6/tools/compiler/arm_5.1.5/include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Source" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/driverlib" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/inc" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/oslib/" -g --gcc --define=ccs --define=SL_FULL --define=SL_PLATFORM_MULTI_THREADED --display_error_number --diag_warning=225 --diag_wrap=off --gen_func_subsections=on --printf_support=full --ual --preproc_with_compile --preproc_dependency="spawn.pp" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '

wlan.obj: C:/Users/chrisjohnson/Desktop/kitsune/simplelink/source/wlan.c $(GEN_OPTS) $(GEN_HDRS)
	@echo 'Building file: $<'
	@echo 'Invoking: ARM Compiler'
	"c:/ti/ccsv6/tools/compiler/arm_5.1.5/bin/armcl" -mv7M4 --code_state=16 --float_support=vfplib --abi=eabi -me -O1 --opt_for_speed=0 --fp_mode=relaxed --include_path="c:/ti/ccsv6/tools/compiler/arm_5.1.5/include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Include" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/simplelink/Source" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/driverlib" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/inc" --include_path="C:/Users/chrisjohnson/Desktop/kitsune/oslib/" -g --gcc --define=ccs --define=SL_FULL --define=SL_PLATFORM_MULTI_THREADED --display_error_number --diag_warning=225 --diag_wrap=off --gen_func_subsections=on --printf_support=full --ual --preproc_with_compile --preproc_dependency="wlan.pp" $(GEN_OPTS__FLAG) "$<"
	@echo 'Finished building: $<'
	@echo ' '


