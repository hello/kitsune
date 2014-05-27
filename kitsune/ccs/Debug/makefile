################################################################################
# Automatically-generated file. Do not edit!
################################################################################

SHELL = cmd.exe

CG_TOOL_ROOT := C:/ti/ccsv5/tools/compiler/arm_5.1.1

ORDERED_OBJS += \
$(GEN_CMDS__FLAG) \
"./nanopb/pb_encode.obj" \
"./nanopb/pb_decode.obj" \
"./protobuf/simple.pb.obj" \
"./utils/ustdlib.obj" \
"./utils/cmdline.obj" \
"./uartstdio.obj" \
"./startup_ccs.obj" \
"./pinmux.obj" \
"./main.obj" \
"./fault.obj" \
"./commands.obj" \
"../kitsune.cmd" \
-l"C:/Users/chrisjohnson/Desktop/kitsune/kitsune/ccs\..\..\third_party\FreeRTOS\ccs\exe\FreeRTOS.a" \
-l"C:/Users/chrisjohnson/Desktop/kitsune/kitsune/ccs\..\..\driverlib\ccs\exe\driverlib.a" \
-l"libc.a" \

-include ../makefile.init

RM := DEL /F
RMDIR := RMDIR /S/Q

# All of the sources participating in the build are defined here
-include sources.mk
-include subdir_vars.mk
-include utils/subdir_vars.mk
-include protobuf/subdir_vars.mk
-include nanopb/subdir_vars.mk
-include subdir_rules.mk
-include utils/subdir_rules.mk
-include protobuf/subdir_rules.mk
-include nanopb/subdir_rules.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(S_DEPS)),)
-include $(S_DEPS)
endif
ifneq ($(strip $(S_UPPER_DEPS)),)
-include $(S_UPPER_DEPS)
endif
ifneq ($(strip $(S62_DEPS)),)
-include $(S62_DEPS)
endif
ifneq ($(strip $(C64_DEPS)),)
-include $(C64_DEPS)
endif
ifneq ($(strip $(ASM_DEPS)),)
-include $(ASM_DEPS)
endif
ifneq ($(strip $(CC_DEPS)),)
-include $(CC_DEPS)
endif
ifneq ($(strip $(S55_DEPS)),)
-include $(S55_DEPS)
endif
ifneq ($(strip $(C67_DEPS)),)
-include $(C67_DEPS)
endif
ifneq ($(strip $(C??_DEPS)),)
-include $(C??_DEPS)
endif
ifneq ($(strip $(CLA_DEPS)),)
-include $(CLA_DEPS)
endif
ifneq ($(strip $(CPP_DEPS)),)
-include $(CPP_DEPS)
endif
ifneq ($(strip $(S??_DEPS)),)
-include $(S??_DEPS)
endif
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
ifneq ($(strip $(C62_DEPS)),)
-include $(C62_DEPS)
endif
ifneq ($(strip $(CXX_DEPS)),)
-include $(CXX_DEPS)
endif
ifneq ($(strip $(C++_DEPS)),)
-include $(C++_DEPS)
endif
ifneq ($(strip $(ASM_UPPER_DEPS)),)
-include $(ASM_UPPER_DEPS)
endif
ifneq ($(strip $(K_DEPS)),)
-include $(K_DEPS)
endif
ifneq ($(strip $(C43_DEPS)),)
-include $(C43_DEPS)
endif
ifneq ($(strip $(S67_DEPS)),)
-include $(S67_DEPS)
endif
ifneq ($(strip $(SA_DEPS)),)
-include $(SA_DEPS)
endif
ifneq ($(strip $(S43_DEPS)),)
-include $(S43_DEPS)
endif
ifneq ($(strip $(OPT_DEPS)),)
-include $(OPT_DEPS)
endif
ifneq ($(strip $(S64_DEPS)),)
-include $(S64_DEPS)
endif
ifneq ($(strip $(C_UPPER_DEPS)),)
-include $(C_UPPER_DEPS)
endif
ifneq ($(strip $(C55_DEPS)),)
-include $(C55_DEPS)
endif
endif

-include ../makefile.defs

# Add inputs and outputs from these tool invocations to the build variables 

# All Target
all: C:/Users/chrisjohnson/Desktop/kitsune/kitsune/ccs/exe/kitsune.out

# Tool invocations
C:/Users/chrisjohnson/Desktop/kitsune/kitsune/ccs/exe/kitsune.out: $(OBJS) $(CMD_SRCS) $(GEN_CMDS)
	@echo 'Building target: $@'
	@echo 'Invoking: ARM Linker'
	"C:/ti/ccsv5/tools/compiler/arm_5.1.1/bin/armcl" -mv7M4 --float_support=fpalib --abi=eabi -me -g --no_intrinsics --gcc --define=ccs --define=USE_FREERTOS --diag_warning=225 --display_error_number --diag_wrap=off -z -m"kitsune.map" --heap_size=0x4000 --stack_size=0x100 -i"C:/ti/ccsv5/tools/compiler/arm_5.1.1/lib" -i"C:/ti/ccsv5/tools/compiler/arm_5.1.1/include" --reread_libs --warn_sections --display_error_number --diag_wrap=off --xml_link_info="kitsune_linkInfo.xml" --rom_model -o "C:/Users/chrisjohnson/Desktop/kitsune/kitsune/ccs/exe/kitsune.out" $(ORDERED_OBJS)
	@echo 'Finished building target: $@'
	@echo ' '
	@$(MAKE) --no-print-directory post-build

# Other Targets
clean:
	-$(RM) $(TMS470_EXECUTABLE_OUTPUTS__QUOTED) "C:\Users\chrisjohnson\Desktop\kitsune\kitsune\ccs\exe\kitsune.out"
	-$(RM) "commands.pp" "fault.pp" "main.pp" "pinmux.pp" "startup_ccs.pp" "uartstdio.pp" "utils\cmdline.pp" "utils\ustdlib.pp" "protobuf\simple.pb.pp" "nanopb\pb_decode.pp" "nanopb\pb_encode.pp" 
	-$(RM) "commands.obj" "fault.obj" "main.obj" "pinmux.obj" "startup_ccs.obj" "uartstdio.obj" "utils\cmdline.obj" "utils\ustdlib.obj" "protobuf\simple.pb.obj" "nanopb\pb_decode.obj" "nanopb\pb_encode.obj" 
	-@echo 'Finished clean'
	-@echo ' '

post-build:
	-"C:/ti/ccsv5/utils/tiobj2bin/tiobj2bin" "C:/Users/chrisjohnson/Desktop/kitsune/kitsune/ccs/exe/kitsune.out" "C:/Users/chrisjohnson/Desktop/kitsune/kitsune/ccs/exe/kitsune.bin" "C:/ti/ccsv5/tools/compiler/arm_5.1.1/bin/armofd" "C:/ti/ccsv5/tools/compiler/arm_5.1.1/bin/armhex" "C:/ti/ccsv5/utils/tiobj2bin/mkhex4bin"
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY: post-build

-include ../makefile.targets

